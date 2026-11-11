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


def cmd_relabel(args):
    """Recompute sections and events from stored features (no audio), e.g. after a rules change."""
    import re
    from .analyze import STRUCTURE_VERSION, with_structure, write_atomic
    from .archive import preserve_analysis
    from .final import write_final

    out_dir = Path(args.out)
    changed = 0
    for f in sorted(out_dir.glob("*.json")):
        if not re.fullmatch(r"[0-9a-f]{16}\.json", f.name):
            continue
        doc = json.loads(f.read_text())
        if doc["analyzer"].get("structure") != STRUCTURE_VERSION or args.force:
            doc = with_structure(doc)
            preserve_analysis(f, out_dir)
            write_atomic(f, json.dumps(doc, separators=(",", ":")))
            update_index(out_dir, doc)
            changed += 1
        write_final(out_dir, doc)
    print(f"relabelled {changed} tracks")
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


def cmd_confidence(args):
    """Fit the confidence model (Gold where there is enough, else Silver), report it, rewrite the final files."""
    from .archive import preserve
    from .confidence import MODEL_FILE, calibration, fit, gold_rows, load_model, silver_rows
    from .evaluate import library_docs, split_of
    from .final import write_final
    from .rounds import load_answers
    from .vdj import DEFAULT_DB, parse_database

    out_dir = Path(args.out)
    docs = library_docs(out_dir)
    vdj = parse_database(DEFAULT_DB) if DEFAULT_DB.exists() else {}
    silver = {tid: silver_rows(doc, vdj.get(doc["track"]["path"])) for tid, doc in docs.items()}
    answers = load_answers(out_dir)
    gold = {tid: gold_rows({k: a for k, a in answers.items() if a["track"] == tid}, docs) for tid in docs}
    part = lambda rows, split: [r for tid, rs in rows.items() if split in (None, split_of(tid)) for r in rs]
    held_out = fit(part(silver, "dev"), part(gold, "dev"))
    for group in ("grid", "start", "label"):
        src = held_out["groups"].get(group, {}).get("source", "none")
        test = [r for r in (part(gold, "test") if src == "gold" else part(silver, "test")) if r[0] == group]
        print(f"{group}: fitted on {src}, held-out calibration:", calibration(held_out, test))
    model = fit(part(silver, None), part(gold, None))
    previous = load_model(out_dir)
    if previous:
        preserve(out_dir / MODEL_FILE, out_dir / "archive" / "models", f"fitted-{previous['fitted']}")
    (out_dir / MODEL_FILE).write_text(json.dumps(model, indent=1))
    for doc in docs.values():
        write_final(out_dir, doc)
    print(f"model saved, {len(docs)} final files rewritten")
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


def _candidate_drops(out_dir, docs):
    """{track id: {Candidate: drop bars on our grid}} for the tracks a Candidate has output for."""
    from .candidates import NAMES, load
    from .transform import candidate_drop_bars

    out = {}
    for tid, doc in docs.items():
        loaded = {name: load(out_dir, name, tid) for name in NAMES}
        drops = {name: candidate_drop_bars(tid, doc, name, l) for name, l in loaded.items() if l is not None}
        if drops:
            out[tid] = drops
    return out


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
        rnd = make_round(len(rounds) + 1, docs, vdj, answers, rounds, size=args.size, candidate_drops=_candidate_drops(out_dir, docs))
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
    from .candidates import run

    run(Path(args.out), args.name, log=lambda m: print(m, flush=True), only_asked=args.only_asked, pause=args.pause)
    return 0


def cmd_export(args):
    from .export import export
    from .vdj import DEFAULT_DB, parse_database

    folder = export(Path(args.out), parse_database(DEFAULT_DB) if DEFAULT_DB.exists() else {})
    print(f"tables in {folder}")
    return 0


def cmd_review(args):
    from .review import serve

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
    cd.add_argument("--pause", type=float, default=0.0, help="seconds of rest per track, to keep the load down")
    cd.set_defaults(func=cmd_candidates)
    ex = sub.add_parser("export", help="rebuild comparison and learning tables (Parquet) from the raw outputs")
    ex.add_argument("--out", default=str(DEFAULT_OUT))
    ex.set_defaults(func=cmd_export)
    r = sub.add_parser("review", help="serve the rating site")
    r.add_argument("--out", default=str(DEFAULT_OUT))
    r.add_argument("--ratings", default=None, help="ratings file (default: <out>/ratings.json)")
    r.add_argument("--port", type=int, default=8765)
    r.add_argument("--lan", action="store_true", help="reachable from the local network, protected by a PIN")
    r.add_argument("--pin", default=None, help="PIN to use (default: a stored random 6-digit PIN)")
    r.set_defaults(func=cmd_review)
    args = ap.parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
