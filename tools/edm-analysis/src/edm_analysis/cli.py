"""edm-analysis analyze <paths> --out DIR [--jsonl]   |   edm-analysis review --out DIR"""

import argparse
import json
import sys
import time
import traceback
from pathlib import Path

DEFAULT_OUT = Path.home() / "Library" / "Application Support" / "QLC+" / "edm-analysis"


def collect(paths):
    files = []
    for p in map(Path, paths):
        if p.is_dir():
            files += sorted(f for f in p.rglob("*") if f.suffix.lower() == ".mp3" and not f.name.startswith("._"))
        elif p.suffix.lower() == ".mp3":
            files.append(p)
    return files


def emit(jsonl, **msg):
    if jsonl:
        print(json.dumps(msg), flush=True)
    elif msg["event"] == "track_done":
        print(f"[{msg['index'] + 1}/{msg['total']}] {msg['bpm']:.2f} BPM  {msg['summary']}  {Path(msg['path']).name}", flush=True)
    elif msg["event"] == "track_error":
        print(f"[{msg['index'] + 1}/{msg['total']}] ERROR {msg['error']}  {Path(msg['path']).name}", file=sys.stderr, flush=True)


def update_index(out_dir, doc):
    from .analyze import write_atomic

    index_path = out_dir / "index.json"
    index = json.loads(index_path.read_text()) if index_path.exists() else {}
    t = doc["track"]
    index[t["sha256"][:16]] = {"title": t["title"], "path": t["path"], "bpm": doc["grid"]["bpm"],
                               "filename_bpm": t["filename_bpm"], "duration": t["duration"],
                               "sections": [s["label"] for s in doc["sections"]],
                               "drops": sum(e["kind"] == "drop" for e in doc["events"])}
    write_atomic(index_path, json.dumps(index, indent=1))


def cmd_analyze(args):
    from .analyze import analyze_track

    files = collect(args.paths)[: args.limit or None]
    out_dir = Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)
    emit(args.jsonl, event="start", total=len(files), out=str(out_dir))
    done = failed = 0
    for i, f in enumerate(files):
        t0 = time.time()
        emit(args.jsonl, event="track_start", index=i, total=len(files), path=str(f))
        try:
            doc, reused = analyze_track(
                f, out_dir, force=args.force,
                on_stage=lambda s, i=i, f=f: emit(args.jsonl, event="stage", index=i, total=len(files), path=str(f), stage=s))
            update_index(out_dir, doc)
            done += 1
            emit(args.jsonl, event="track_done", index=i, total=len(files), path=str(f), id=doc["track"]["sha256"][:16],
                 reused=reused, seconds=round(time.time() - t0, 1), bpm=doc["grid"]["bpm"],
                 summary=" ".join(s["label"] for s in doc["sections"]))
        except Exception as e:  # one broken file must not stop a library run
            failed += 1
            emit(args.jsonl, event="track_error", index=i, total=len(files), path=str(f), error=f"{type(e).__name__}: {e}",
                 trace=traceback.format_exc(limit=3))
    emit(args.jsonl, event="finished", total=len(files), done=done, failed=failed, out=str(out_dir))
    return 0 if failed == 0 else 1


def _raw_docs(out_dir):
    """(path, document) of every raw rule analysis."""
    import re

    for f in sorted(Path(out_dir).glob("*.json")):
        if re.fullmatch(r"[0-9a-f]{16}\.json", f.name):
            yield f, json.loads(f.read_text())


def _relabel(out_dir, force):
    """Recompute the rules' sections and events from stored features where their version is old (or force); returns how many."""
    from .analyze import STRUCTURE_VERSION, with_structure, write_atomic
    from .archive import preserve_analysis

    changed = 0
    for f, doc in _raw_docs(out_dir):
        if doc["analyzer"].get("structure") != STRUCTURE_VERSION or force:
            doc = with_structure(doc)
            preserve_analysis(f, out_dir)
            write_atomic(f, json.dumps(doc, separators=(",", ":")))
            update_index(out_dir, doc)
            changed += 1
    return changed


def _write_finals(out_dir):
    from .final import write_final

    n = 0
    for _, doc in _raw_docs(out_dir):
        write_final(out_dir, doc)
        n += 1
    return n


def cmd_relabel(args):
    """Recompute sections and events from stored features (no audio), e.g. after a rules change."""
    out_dir = Path(args.out)
    changed = _relabel(out_dir, args.force)
    _write_finals(out_dir)
    print(f"relabelled {changed} tracks")
    return 0


def cmd_rebuild(args):
    """Rebuild every final file from the raw outputs, without audio: rules where their version is old, fusion, confidence model, finals."""
    out_dir = Path(args.out)
    changed = _relabel(out_dir, args.force)
    _fit_confidence(out_dir, print)
    print(f"relabelled {changed} tracks, {_write_finals(out_dir)} final files written")
    return 0


def cmd_evaluate(args):
    from .evaluate import evaluate_library
    from .vdj import DEFAULT_DB, parse_database

    out_dir = Path(args.out)
    rows, summary = evaluate_library(out_dir, parse_database(DEFAULT_DB) if DEFAULT_DB.exists() else {},
                                     out_dir / "ratings.json", out_dir / "ratings_history.json")
    if args.save:
        Path(args.save).write_text(json.dumps({"summary": summary, "tracks": rows}, indent=1))
    width = max(len(k) for k in summary["dev"])
    print(f"{'metric':<{width}}  {'dev':>14}  {'test':>14}")
    for key in summary["dev"]:
        if key == "tracks":
            print(f"{key:<{width}}  {summary['dev'][key]:>14}  {summary['test'][key]:>14}")
            continue
        cell = lambda v: "-" if v[0] is None else f"{v[0]:.3f} (n={v[1]})"
        print(f"{key:<{width}}  {cell(summary['dev'][key]):>14}  {cell(summary['test'][key]):>14}")
    return 0


def _bands_text(rows):
    from .confidence import REVIEWER_ERROR

    return "  ".join(f"{b['band']}: {b['exact']:.2f} (n {b['n']}, {b['corrected']:.2f} if reviewer errs {REVIEWER_ERROR:.0%})" for b in rows if b["n"]) or "no judged Claims"


def _fit_confidence(out_dir, say):
    """Fit the confidence model (Gold where a group has enough, else Silver) and save it; say() reports each group's calibration. Returns the Library's analyses."""
    from .archive import preserve
    from .confidence import MIN_GOLD, MODEL_FILE, UNKNOWN, bands, calibration, fit, gold_rows, held_out, load_model, silver_rows, silver_without_gold
    from .evaluate import library_docs, split_of
    from .rounds import load_answers
    from .vdj import DEFAULT_DB, parse_database

    docs, analysed = library_docs(out_dir), library_docs(out_dir, scoped=False)
    vdj = parse_database(DEFAULT_DB) if DEFAULT_DB.exists() else {}
    silver = {tid: silver_rows(doc, vdj.get(doc["track"]["path"])) for tid, doc in docs.items()}
    by_track = {}
    for pid, a in load_answers(out_dir).items():
        by_track.setdefault(a["track"], {})[pid] = a
    gold = {tid: gold_rows(answers, analysed) for tid, answers in by_track.items() if tid in analysed}   # answers outside the Library stay Gold
    part = lambda rows, split: [r for tid, rs in rows.items() if split in (None, split_of(tid)) for r in rs]
    model = {**fit(part(silver_without_gold(silver, gold), None), part(gold, None)), "analyzer": sorted({d["analyzer"]["version"] for d in docs.values()})}
    dev = fit(part(silver_without_gold(silver, {t: g for t, g in gold.items() if split_of(t) == "dev"}), "dev"), part(gold, "dev"))
    for group in ("grid", "start", "label"):
        m = model["groups"].get(group)
        say(f"{group}: " + (f"fitted on {m['source']} ({m['n']} rows, {m['gold']} Gold answers; Gold alone from {MIN_GOLD})" if m
                     else f"no model ({sum(r[0] == group for rows in gold.values() for r in rows)} Gold answers, no Silver): confidence is {UNKNOWN}"))
        say("  on Gold, each track held out: " + _bands_text(bands(held_out(silver, gold, group))))
        if m and m["source"] != "gold":
            say("  on Silver, test half held out: " + _bands_text(calibration(dev, [r for r in part(silver, "test") if r[0] == group])))
    previous = load_model(out_dir)
    if previous and previous != model:
        preserve(out_dir / MODEL_FILE, out_dir / "archive" / "models", f"fitted-{previous['fitted']}")
    (out_dir / MODEL_FILE).write_text(json.dumps(model, indent=1))
    return docs


def cmd_confidence(args):
    """Fit the confidence model, report it, rewrite the Library's final files."""
    from .confidence import load_model, with_confidence
    from .final import write_final
    from .rounds import library_claims

    out_dir = Path(args.out)
    docs = _fit_confidence(out_dir, print)
    for doc in docs.values():
        write_final(out_dir, doc)
    model = load_model(out_dir)
    claims = library_claims({tid: with_confidence(doc, model) for tid, doc in docs.items()})
    share = lambda cs: f"{sum(c >= 95 for c in cs) / max(1, len(cs)):.0%}"
    print(f"model saved, {len(docs)} final files rewritten")
    print(f"Claims at 95% or more: {share([c for _, c in claims])} of {len(claims)}; " + ", ".join(f"{g} {share([c for k, c in claims if k == g])}" for g in ("grid", "start", "label")))
    return 0


def _round_inputs(out_dir):
    from .confidence import load_model, with_confidence
    from .evaluate import library_docs
    from .rounds import all_rounds, load_answers
    from .vdj import DEFAULT_DB, parse_database

    model = load_model(out_dir)
    docs = {tid: with_confidence(d, model) for tid, d in library_docs(out_dir).items()}
    vdj = parse_database(DEFAULT_DB) if DEFAULT_DB.exists() else {}
    return docs, vdj, load_answers(out_dir), all_rounds(out_dir)


def _candidate_bars(out_dir, docs, bars_of):
    """{track id: {Candidate: bars on our grid}} for the tracks a Candidate has output for."""
    from .candidates import NAMES, load

    out = {}
    for tid, doc in docs.items():
        loaded = {name: load(out_dir, name, tid) for name in NAMES}
        bars = {name: bars_of(tid, doc, name, l) for name, l in loaded.items() if l is not None}
        if bars:
            out[tid] = bars
    return out


def _candidate_drops(out_dir, docs):
    from .transform import candidate_drop_bars

    return _candidate_bars(out_dir, docs, candidate_drop_bars)


def _candidate_buildups(out_dir, docs):
    from .transform import claims_candidate

    return _candidate_bars(out_dir, docs, lambda tid, doc, name, loaded: sorted(
        {c["bar"] for c in claims_candidate(tid, doc, name, loaded) if c["kind"] == "buildup"}))


def cmd_round(args):
    """Make the next review round, or report how the rounds went."""
    from .rounds import make_round, report, save_round

    out_dir = Path(args.out)
    docs, vdj, answers, rounds = _round_inputs(out_dir)
    if args.action == "new":
        open_probes = [p for p in (rounds[-1]["probes"] if rounds else []) if p["id"] not in answers]
        if open_probes and not args.force:
            print(f"round {rounds[-1]['round']} still has {len(open_probes)} unanswered questions (use --force)")
            return 1
        rnd = make_round(len(rounds) + 1, docs, vdj, answers, rounds, size=args.size,
                         candidate_drops=_candidate_drops(out_dir, docs), candidate_buildups=_candidate_buildups(out_dir, docs))
        save_round(out_dir, rnd)
        counts = {}
        for p in rnd["probes"]:
            counts[(p["bucket"], p["theory"])] = counts.get((p["bucket"], p["theory"]), 0) + 1
        print(f"round {rnd['round']}: {len(rnd['probes'])} questions")
        for (bucket, theory), n in sorted(counts.items()):
            print(f"  {bucket:<7} {theory:<12} {n}")
        return 0
    print(json.dumps(report(rounds, answers, docs), indent=1))
    return 0


def cmd_candidates(args):
    from .candidates import Guard, run

    guard = Guard(max_memory_gb=args.max_memory_gb, max_seconds=args.max_minutes * 60, min_free_gb=args.min_free_gb, max_load=args.max_load, force=args.force)
    return run(Path(args.out), args.name, log=lambda m: print(m, flush=True), only_asked=args.only_asked, pause=args.pause, library=args.library,
               guard=guard, max_tracks=args.max_tracks, device=args.device, retry_failed=args.retry_failed)


def cmd_export(args):
    from .export import export
    from .vdj import DEFAULT_DB, parse_database

    folder = export(Path(args.out), parse_database(DEFAULT_DB) if DEFAULT_DB.exists() else {})
    print(f"tables in {folder}")
    return 0


def cmd_library(args):
    """Show, set or clear the scope of the Library."""
    from .library import clear_scope, in_library, load_scope, set_scope

    out_dir = Path(args.out)
    if args.action == "set" and not args.patterns:
        print("library set needs at least one pattern", file=sys.stderr)
        return 1
    if args.action == "set":
        set_scope(out_dir, args.patterns)
    elif args.action == "clear":
        clear_scope(out_dir)
    scope = load_scope(out_dir)
    print("every analysed track is in the Library" if scope is None else "include:\n" + "\n".join(f"  {p}" for p in scope["include"]))
    index = json.loads((out_dir / "index.json").read_text()) if (out_dir / "index.json").exists() else {}
    inside = sum(in_library(e["path"], scope) for e in index.values())
    print(f"{len(index)} analysed tracks: {inside} in the Library, {len(index) - inside} out")
    return 0


def _show_tracks(out_dir, prefixes):
    """Each named track as rules, fused sections and answers side by side."""
    from .compare import side_by_side
    from .confidence import load_model, with_confidence
    from .evaluate import library_docs
    from .rounds import load_answers
    from .transform import answer_rows

    model = load_model(out_dir)
    docs = {tid: with_confidence(doc, model) if model else doc for tid, doc in library_docs(out_dir, scoped=False).items()}
    rows = answer_rows(load_answers(out_dir), docs)
    for prefix in prefixes:
        found = [tid for tid in docs if tid.startswith(prefix)]
        if not found:
            print(f"no analysed track starts with {prefix}", file=sys.stderr)
            return 1
        print(side_by_side(docs[found[0]], [r for r in rows if r["track"] == found[0]]), end="\n\n")
    return 0


def cmd_compare(args):
    import pyarrow.parquet as pq

    from .compare import compare, format_table

    if args.tracks:
        return _show_tracks(Path(args.out), args.tracks)
    folder = Path(args.out) / "tables"
    if not (folder / "claims.parquet").exists():
        print("no claims table: run `edm-analysis export` first", file=sys.stderr)
        return 1
    read = lambda name: pq.read_table(folder / f"{name}.parquet").to_pylist() if (folder / f"{name}.parquet").exists() else []
    result = compare(read("claims"), read("answers"))
    print(json.dumps(result, indent=1) if args.json else format_table(result))
    return 0


def cmd_review(args):
    from .review import prerender, serve

    if args.prerender:
        missing = prerender(Path(args.out))
        print(f"{missing} clips missing" if missing else "all clips ready")
        return 1 if missing else 0
    serve(Path(args.out), args.port, args.ratings, lan=args.lan, pin=args.pin)
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(prog="edm-analysis", fromfile_prefix_chars="@")
    sub = ap.add_subparsers(dest="cmd", required=True)
    a = sub.add_parser("analyze", help="analyze MP3 files or folders")
    a.add_argument("paths", nargs="+")
    a.add_argument("--out", default=str(DEFAULT_OUT))
    a.add_argument("--limit", type=int, default=0)
    a.add_argument("--force", action="store_true")
    a.add_argument("--jsonl", action="store_true", help="machine-readable progress on stdout")
    a.set_defaults(func=cmd_analyze)
    rl = sub.add_parser("relabel", help="recompute sections and events from stored features")
    rl.add_argument("--out", default=str(DEFAULT_OUT))
    rl.add_argument("--force", action="store_true")
    rl.set_defaults(func=cmd_relabel)
    rb = sub.add_parser("rebuild", help="rebuild every final file from the raw outputs: rules if outdated, fusion, confidence, final (no audio)")
    rb.add_argument("--out", default=str(DEFAULT_OUT))
    rb.add_argument("--force", action="store_true", help="recompute the rules' sections even where their version is current")
    rb.set_defaults(func=cmd_rebuild)
    e = sub.add_parser("evaluate", help="score analyses against VirtualDJ / Mixed In Key and your ratings")
    e.add_argument("--out", default=str(DEFAULT_OUT))
    e.add_argument("--save", default=None, help="write per-track scores to this JSON file")
    e.set_defaults(func=cmd_evaluate)
    c = sub.add_parser("confidence", help="fit the per-claim confidence model and rewrite the final files")
    c.add_argument("--out", default=str(DEFAULT_OUT))
    c.set_defaults(func=cmd_confidence)
    rd = sub.add_parser("round", help="make the next review round, or report on the rounds")
    rd.add_argument("action", choices=("new", "report"))
    rd.add_argument("--out", default=str(DEFAULT_OUT))
    rd.add_argument("--size", type=int, default=40)
    rd.add_argument("--force", action="store_true", help="make a new round even if the current one is unfinished")
    rd.set_defaults(func=cmd_round)
    cd = sub.add_parser("candidates", help="run a pretrained Candidate over the library")
    cd.add_argument("name", choices=("allin1", "edmformer", "dropdetector"))
    cd.add_argument("--out", default=str(DEFAULT_OUT))
    cd.add_argument("--only-asked", action="store_true", help="only tracks that review rounds ask about")
    cd.add_argument("--library", action="store_true", help="only tracks inside the Library scope (every track where no scope is set)")
    cd.add_argument("--pause", type=float, default=0.0, help="seconds of rest per track, to keep the load down")
    from .candidates import Guard

    cd.add_argument("--max-tracks", type=int, default=None, help="do at most this many tracks in this call")
    cd.add_argument("--device", choices=("mps", "cpu"), default="mps", help="where EDMFormer runs")
    cd.add_argument("--max-memory-gb", type=float, default=Guard.max_memory_gb, help="kill the process group past this physical footprint")
    cd.add_argument("--max-minutes", type=float, default=Guard.max_seconds / 60, help="kill the process group of a track that runs longer")
    cd.add_argument("--min-free-gb", type=float, default=Guard.min_free_gb, help="refuse to start with less free memory (exit code 75)")
    cd.add_argument("--max-load", type=float, default=Guard.max_load, help="refuse to start above this 1-minute load average (exit code 75)")
    cd.add_argument("--force", action="store_true", help="skip the busy check")
    cd.add_argument("--retry-failed", action="store_true", help="also try tracks recorded in failed.json")
    cd.set_defaults(func=cmd_candidates)
    ex = sub.add_parser("export", help="rebuild comparison and learning tables (Parquet) from the raw outputs")
    ex.add_argument("--out", default=str(DEFAULT_OUT))
    ex.set_defaults(func=cmd_export)
    lb = sub.add_parser("library", help="show, set or clear which tracks are in the Library")
    lb.add_argument("action", choices=("show", "set", "clear"))
    lb.add_argument("patterns", nargs="*", help="globs of files or folders to include (set)")
    lb.add_argument("--out", default=str(DEFAULT_OUT))
    lb.set_defaults(func=cmd_library)
    cp = sub.add_parser("compare", help="compare the sources with each other and with Gold, from the exported tables")
    cp.add_argument("--out", default=str(DEFAULT_OUT))
    cp.add_argument("--json", action="store_true", help="machine-readable output")
    cp.add_argument("--tracks", nargs="+", help="instead of the tables: these tracks (id prefixes) as rules, fused sections and answers side by side")
    cp.set_defaults(func=cmd_compare)
    r = sub.add_parser("review", help="serve the rating site")
    r.add_argument("--out", default=str(DEFAULT_OUT))
    r.add_argument("--ratings", default=None, help="ratings file (default: <out>/ratings.json)")
    r.add_argument("--port", type=int, default=8765)
    r.add_argument("--lan", action="store_true", help="reachable from the local network, protected by a PIN")
    r.add_argument("--pin", default=None, help="PIN to use (default: a stored random 6-digit PIN)")
    r.add_argument("--prerender", action="store_true", help="make the clips of the current round and exit, without serving (exit 1 if some are missing)")
    r.set_defaults(func=cmd_review)
    args = ap.parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
