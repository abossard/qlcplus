"""Compare the sources (our rules, their fusion, the Candidates, VirtualDJ) with each other and with Gold.

Pure calculations over the rows of the exported tables (claims.parquet, answers.parquet); the command reads
them and prints. Bars are bars on our grid, as in transform.py. Shares of the reviewer's answers come with what they mean
if the reviewer errs at confidence.REVIEWER_ERROR.
"""

from itertools import zip_longest

from .confidence import REVIEWER_ERROR, corrected_rate
from .fusion import entry_drops
from .transform import DROP_SCORE

DROP_SOURCES = ("rules", "edmformer", "dropdetector")
OFFSET_WINDOW = 4          # bars around a break end where a source's drop counts as its guess for it
DETECTOR_WINDOW = 8        # bars around a sure detector drop where a source's drop counts as its guess for it (a phrase)
TOLERANCES = {"exact": 0, "within_1": 1}
REJECTED_TOLERANCES = {"exact": 0, "within_2": 2}   # the fusion starts a buildup 2 bars before the rules, so "on that bar" also reads within 2
GOLD_SOURCES = (*DROP_SOURCES, "vdj")
GOLD_KINDS = {"drop": ("rules", "fused", "edmformer", "dropdetector", "vdj"), "buildup": ("rules", "fused", "edmformer"),
              "breakdown": ("rules", "fused", "edmformer")}


def _share(hits, n):
    return round(hits / n, 4) if n else None


def _index(claims):
    """{track: {(source, kind): [claim]}}"""
    out = {}
    for c in claims:
        out.setdefault(c["track"], {}).setdefault((c["source"], c["kind"]), []).append(c)
    return out


def _bars(by, tid, source, kind):
    """Bars after the track start; the drop detector's drops only where it is at least DROP_SCORE sure."""
    sure = lambda c: source != "dropdetector" or (c["score"] or 0) >= DROP_SCORE
    return {c["bar"] for c in by.get(tid, {}).get((source, kind), []) if c["bar"] > 0 and (kind != "drop" or sure(c))}


def _scores(pairs):
    """Recall and precision of each track's predicted bars against its reference bars, exact and within 1 bar."""
    n_ref, n_pred = sum(len(ref) for _, ref in pairs), sum(len(pred) for pred, _ in pairs)
    out = {}
    for name, tol in TOLERANCES.items():
        found = sum(any(abs(r - p) <= tol for p in pred) for pred, ref in pairs for r in ref)
        right = sum(any(abs(p - r) <= tol for r in ref) for pred, ref in pairs for p in pred)
        out[name] = {"recall": _share(found, n_ref), "n_ref": n_ref, "precision": _share(right, n_pred), "n_pred": n_pred}
    return out


def _offsets(pairs, window=OFFSET_WINDOW):
    """{bar offset: count} of the source's nearest drop (earlier on a tie) within the window of each reference bar."""
    out = {}
    for drops, ends in pairs:
        for end in ends:
            near = min(drops, key=lambda d: (abs(d - end), d), default=None)
            if near is not None and abs(near - end) <= window:
                out[near - end] = out.get(near - end, 0) + 1
    return dict(sorted(out.items()))


def _gold(by, covered, answers):
    """Per kind, per source: Gold bars on the tracks it covers, and the share it has exactly or within 1 bar (observed, and read under the reviewer's error rate)."""
    out = {}
    for kind, sources in GOLD_KINDS.items():
        gold = [(a["track"], a["gold_bar"]) for a in answers if a.get("kind") == kind and a.get("gold_bar") is not None]
        if not gold:
            continue
        out[kind] = {}
        for source in sources:
            mine = [(_bars(by, t, source, "break_end" if source == "vdj" else kind), g) for t, g in gold if t in covered.get(source, ())]
            shares = {name: _share(sum(any(abs(g - b) <= tol for b in bars) for bars, g in mine), len(mine)) for name, tol in TOLERANCES.items()}
            out[kind][source] = {"n": len(mine), **shares, **{f"{k}_corrected": None if v is None else round(corrected_rate(v), 4) for k, v in shares.items()}}
    return out or None


def _sections(by, track, source):
    return sorted(by.get(track, {}).get((source, "section"), []), key=lambda c: c["bar"])


def _structure(by, covered):
    """Per source: its drops, first beat entries, drops straight after a drop, drops with a buildup right before them and those buildups' lengths,
    buildups and those no source supports (fused only), and lifts."""
    out = {}
    for source in ("rules", "fused"):
        lengths, drops, after_drop, buildups, unsupported, entries = {}, 0, 0, 0, 0, 0
        for t in covered.get(source, ()):
            secs = _sections(by, t, source)
            entries += len(entry_drops([{"label": c.get("label"), "start_bar": c["bar"]} for c in secs]))
            buildups += sum(c.get("label") == "buildup" for c in secs)
            unsupported += sum(c.get("label") == "buildup" and c.get("sources") == "" for c in secs)
            for prev, cur in zip(secs, secs[1:]):
                if cur.get("label") == "drop":
                    drops += 1
                    after_drop += prev.get("label") == "drop"
                    if prev.get("label") == "buildup":
                        lengths[cur["bar"] - prev["bar"]] = lengths.get(cur["bar"] - prev["bar"], 0) + 1
        if drops:
            out[source] = {"drops": drops, "with_buildup": sum(lengths.values()), "buildup_lengths": dict(sorted(lengths.items())), "after_drop": after_drop,
                           "buildups": buildups, "unsupported": unsupported, "entries": entries,
                           "entry_events": sum(len(by.get(t, {}).get((source, "entry"), [])) for t in covered.get(source, ())),
                           "lifts": sum(len(by.get(t, {}).get((source, "lift"), [])) for t in covered.get(source, ()))}
    return out


EXISTENCE_REACH = 24     # bars before a drop within which a source's buildup start is its buildup for that drop


def _existence(by, covered, answers):
    """Per source: of the answers that say whether a buildup exists before a drop, those where the source claims one and it exists (confirmed) or not
    (rejected), where it claims none and one exists (missed) or not (agreed_none), and the share right. Claimed is read from the source's own
    buildup starts in the bars before that drop, so every source is judged on the same questions."""
    rows = [(a["track"], a["drop_bar"], a["exists"]) for a in answers if a.get("exists") is not None and a.get("drop_bar") is not None]
    out = {}
    for source in ("rules", "fused", "edmformer"):
        counts = {"confirmed": 0, "rejected": 0, "missed": 0, "agreed_none": 0}
        mine = [(t, d, e) for t, d, e in rows if t in covered.get(source, ())]
        for t, d, exists in mine:
            claimed = any(d - EXISTENCE_REACH <= c["bar"] < d for c in by.get(t, {}).get((source, "buildup"), []))
            counts["confirmed" if claimed and exists else "rejected" if claimed else "missed" if exists else "agreed_none"] += 1
        if mine:
            out[source] = {"n": len(mine), **counts, "right": _share(counts["confirmed"] + counts["agreed_none"], len(mine))}
    return out or None


def _energy(by, covered):
    """Mean energy level of the fused sections per label, or None where the tables carry none: drops should sit above breakdowns."""
    levels = {}
    for t in covered.get("fused", ()):
        for c in _sections(by, t, "fused"):
            if c.get("energy") is not None:
                levels.setdefault(c.get("label"), []).append(c["energy"])
    return {label: {"n": len(v), "mean": round(sum(v) / len(v), 2)} for label, v in sorted(levels.items())} or None


def _rejected(by, covered, answers):
    """Per kind, per source: of the start Claims the reviewer rejected ("no", "Not in this clip", "No buildup"), the share the source still makes on that bar."""
    out = {}
    for kind, sources in GOLD_KINDS.items():
        rejected = [(a["track"], a["bar"]) for a in answers if a.get("kind") == kind and a.get("answer") in ("no", "none") and a.get("exact") is False]
        if rejected:
            out[kind] = {}
            for source in (s for s in sources if s != "vdj"):
                mine = [(_bars(by, t, source, kind), bar) for t, bar in rejected if t in covered.get(source, ())]
                out[kind][source] = {"n": len(mine), **{name: _share(sum(any(abs(bar - b) <= tol for b in bars) for bars, bar in mine), len(mine))
                                                      for name, tol in REJECTED_TOLERANCES.items()}}
    return out or None


def _label_changes(by, covered):
    """The rules' sections whose label differs from the fusion's at their start bar, as {"from>to": count}."""
    sections, from_to = 0, {}
    for t in covered.get("rules", set()) & covered.get("fused", set()):
        fused = _sections(by, t, "fused")
        for c in _sections(by, t, "rules"):
            sections += 1
            now = next((f.get("label") for f in reversed(fused) if f["bar"] <= c["bar"]), None)
            if now != c.get("label"):
                key = f"{c.get('label')}>{now}"
                from_to[key] = from_to.get(key, 0) + 1
    return {"sections": sections, "changed": sum(from_to.values()), "from_to": dict(sorted(from_to.items()))}


def compare(claims, answers=()):
    by = _index(claims)
    covered = {s: {t for t, kinds in by.items() if any(k[0] == s for k in kinds)} for s in sorted({c["source"] for c in claims})}
    covered = {**{s: set() for s in GOLD_SOURCES}, **covered}
    ends = {t: _bars(by, t, "vdj", "break_end") for t in set.intersection(*(covered[s] for s in GOLD_SOURCES))}
    tracks = sorted(t for t, e in ends.items() if e)   # without a break end every guess would count as wrong
    guesses = {s: {t: _bars(by, t, s, "drop") for t in tracks} for s in (*DROP_SOURCES, *(["fused"] if covered.get("fused") else []))}
    guesses["edmformer+dropdetector"] = {t: guesses["edmformer"][t] & guesses["dropdetector"][t] for t in tracks}
    guesses["two of three"] = {t: {b for b in set().union(*(guesses[s][t] for s in DROP_SOURCES))
                                   if sum(b in guesses[s][t] for s in DROP_SOURCES) >= 2} for t in tracks}
    sections = {s: [c["bar"] for t in covered[s] for c in by[t].get((s, "section"), []) if c["bar"] > 0] for s in covered}
    return {"coverage": {s: len(t) for s, t in covered.items() if t},
            "drops": {"tracks": len(tracks),
                      "scores": {name: _scores([(g[t], ends[t]) for t in tracks]) for name, g in guesses.items()},
                      "offsets": {s: _offsets([(guesses[s][t], ends[t]) for t in tracks]) for s in guesses if s in (*DROP_SOURCES, "fused")}},
            "detector_offsets": {s: _offsets([(_bars(by, t, s, "drop"), _bars(by, t, "dropdetector", "drop")) for t in covered[s] & covered["dropdetector"]], DETECTOR_WINDOW)
                                 for s in ("rules", "fused", "edmformer") if covered.get(s)},
            "structure": _structure(by, covered), "label_changes": _label_changes(by, covered),
            "section_starts": {s: {"n": len(bars), "on_8": _share(sum(b % 8 == 0 for b in bars), len(bars))}
                               for s, bars in sections.items() if bars},
            "gold": _gold(by, covered, answers), "rejected": _rejected(by, covered, answers), "energy": _energy(by, covered), "existence": _existence(by, covered, answers)}


def format_table(result):
    """The comparison as plain text."""
    cell = lambda v: "-" if v is None else f"{v:.2f}"
    lines = ["tracks covered"] + [f"  {s:<24}{n:>5}" for s, n in result["coverage"].items()]
    drops = result["drops"]
    lines += ["", f"drops against VirtualDJ break ends ({drops['tracks']} tracks that every drop source covers and that have a break end)",
              f"  {'':<24}{'guesses':>8}{'exact R':>9}{'exact P':>9}{'+-1 R':>9}{'+-1 P':>9}"]
    for name, s in drops["scores"].items():
        e, w = s["exact"], s["within_1"]
        lines.append(f"  {name:<24}{e['n_pred']:>8}{cell(e['recall']):>9}{cell(e['precision']):>9}{cell(w['recall']):>9}{cell(w['precision']):>9}")
    first = next(iter(drops["scores"].values()))["exact"]
    lines.append(f"  ({first['n_ref']} break ends; R is recall, P precision)")
    lines += ["", f"offset of the nearest drop from a break end, within {OFFSET_WINDOW} bars (drop bar minus break end bar)"]
    for s, counts in drops["offsets"].items():
        lines.append(f"  {s:<24}" + "  ".join(f"{o:+d}: {n}" for o, n in counts.items()))
    lines += ["", "section starts on a multiple of 8 bars", f"  {'':<24}{'starts':>8}{'share':>9}"]
    lines += [f"  {s:<24}{v['n']:>8}{cell(v['on_8']):>9}" for s, v in result["section_starts"].items()]
    lines += ["", "offset of the nearest drop from a sure drop detector drop, within " + str(DETECTOR_WINDOW) + " bars (drop bar minus detector bar)"]
    for s_, counts in result["detector_offsets"].items():
        lines.append(f"  {s_:<24}" + "  ".join(f"{o:+d}: {n}" for o, n in counts.items()))
    if result["structure"]:
        lines += ["", "drops with a buildup before them", f"  {'':<24}{'drops':>8}{'buildup':>9}{'share':>9}"]
        lines += [f"  {s_:<24}{v['drops']:>8}{v['with_buildup']:>9}{cell(_share(v['with_buildup'], v['drops'])):>9}" for s_, v in result["structure"].items()]
        lines += ["", "drops after a drop, buildups, buildups no source supports, lifts, first beat entries kept as drops, entry events", f"  {'':<24}{'after':>8}{'buildups':>9}{'no source':>10}{'lifts':>7}{'entries':>9}{'entry events':>14}"]
        lines += [f"  {s_:<24}{v['after_drop']:>8}{v['buildups']:>9}{v['unsupported']:>10}{v['lifts']:>7}{v['entries']:>9}{v['entry_events']:>14}" for s_, v in result["structure"].items()]
        lines += ["", "buildup length in bars: number of buildups"]
        lines += [f"  {s_:<24}" + "  ".join(f"{n}: {c}" for n, c in v["buildup_lengths"].items()) for s_, v in result["structure"].items()]
    if result["energy"]:
        lines += ["", "mean energy per label (fused sections, 1 to 10)", "  " + "  ".join(f"{label}: {v['mean']:.1f} (n {v['n']})" for label, v in result["energy"].items())]
    changes = result["label_changes"]
    if changes["sections"]:
        lines += ["", f"label changes: {changes['changed']} of {changes['sections']} rules sections have another label in the fusion",
                  "  " + "  ".join(f"{k}: {n}" for k, n in sorted(changes["from_to"].items(), key=lambda kv: -kv[1]))]
    if result["existence"]:
        lines += ["", "buildup existence against the reviewer's answers (a buildup exists before this drop, yes or no)",
                  f"  {'':<24}{'answers':>8}{'claimed and confirmed':>23}{'claimed and rejected':>22}{'not claimed, named':>20}{'agreed none':>13}{'right':>8}"]
        lines += [f"  {s_:<24}{v['n']:>8}{v['confirmed']:>23}{v['rejected']:>22}{v['missed']:>20}{v['agreed_none']:>13}{cell(v['right']):>8}" for s_, v in result["existence"].items()]
    for kind, table in (result["gold"] or {}).items():
        lines += ["", f"against Gold bars of {kind}s (observed; 'if' is the share if the reviewer errs {REVIEWER_ERROR:.0%} of the time)",
                  f"  {'':<24}{'answers':>8}{'exact':>9}{'if':>7}{'+-1':>9}{'if':>7}"]
        lines += [f"  {s_:<24}{v['n']:>8}{cell(v['exact']):>9}{cell(v['exact_corrected']):>7}{cell(v['within_1']):>9}{cell(v['within_1_corrected']):>7}"
                  for s_, v in table.items() if v["n"]]
    for kind, table in (result["rejected"] or {}).items():
        lines += ["", f"rejected {kind} Claims (the reviewer said no, not in this clip, no buildup): the share each source still makes on that bar",
                  f"  {'':<24}{'rejected':>9}{'exact':>9}{'+-2':>9}"]
        lines += [f"  {s_:<24}{v['n']:>9}{cell(v['exact']):>9}{cell(v['within_2']):>9}" for s_, v in table.items() if v["n"]]
    return "\n".join(lines)


def side_by_side(doc, answers):
    """One track as four columns: the rules' sections, the fused sections with their confidence and sources (~ marks a nominal start), the entry and
    lift events, and the answers on them."""
    left = [f"{s['label']} {s['start_bar']}-{s['end_bar']}" for s in doc["rules"]["sections"]]
    mid = [f"{s['label']} {'~' if s.get('nominal') else ''}{s['start_bar']}-{s['end_bar']}" + (f" {s['confidence']}%" if "confidence" in s else "")
           + (" " + "+".join(s["sources"]) if s.get("sources") else "") for s in doc["sections"]]
    events = [f"{e['kind']} {e['bar']}" for e in doc.get("events", []) if e["kind"] in ("entry", "lift")]
    right = [f"{a['kind']} {a['bar']}: {a['answer']}" + (f" (gold bar {a['gold_bar']})" if a.get("gold_bar") is not None else "")
             for a in sorted(answers, key=lambda a: a["bar"])]
    w = (max(map(len, [*left, "rules"]), default=5), max(map(len, [*mid, "fused"]), default=5), max(map(len, [*events, "events"]), default=6))
    rows = [f"{a:<{w[0]}} | {b:<{w[1]}} | {c:<{w[2]}} | {d}".rstrip() for a, b, c, d in zip_longest(left, mid, events, right, fillvalue="")]
    return "\n".join([doc["track"]["title"], f"{'rules':<{w[0]}}   {'fused':<{w[1]}}   {'events':<{w[2]}}   answers", *rows])
