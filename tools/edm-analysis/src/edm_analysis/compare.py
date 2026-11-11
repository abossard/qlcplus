"""Compare the sources (our rules, the Candidates, VirtualDJ) with each other and with Gold.

Pure calculations over the rows of the exported tables (claims.parquet, answers.parquet); the command reads
them and prints. Bars are bars on our grid, as in transform.py.
"""

from .transform import DROP_SCORE

DROP_SOURCES = ("rules", "edmformer", "dropdetector")
OFFSET_WINDOW = 4          # bars around a break end where a source's drop counts as its guess for it
TOLERANCES = {"exact": 0, "within_1": 1}
GOLD_SOURCES = (*DROP_SOURCES, "vdj")


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


def _offsets(pairs):
    """{bar offset: count} of the source's nearest drop (earlier on a tie) within the window of each break end."""
    out = {}
    for drops, ends in pairs:
        for end in ends:
            near = min(drops, key=lambda d: (abs(d - end), d), default=None)
            if near is not None and abs(near - end) <= OFFSET_WINDOW:
                out[near - end] = out.get(near - end, 0) + 1
    return dict(sorted(out.items()))


def _gold(by, covered, answers):
    """Per source: Gold bars of drops on the tracks it covers, and the share it has exactly or within 1 bar."""
    gold = [(a["track"], a["gold_bar"]) for a in answers if a.get("kind") == "drop" and a.get("gold_bar") is not None]
    if not gold:
        return None
    out = {}
    for source in GOLD_SOURCES:
        kind = "break_end" if source == "vdj" else "drop"
        mine = [(_bars(by, t, source, kind), g) for t, g in gold if t in covered[source]]
        out[source] = {"n": len(mine), **{name: _share(sum(any(abs(g - b) <= tol for b in bars) for bars, g in mine), len(mine))
                                          for name, tol in TOLERANCES.items()}}
    return out


def compare(claims, answers=()):
    by = _index(claims)
    covered = {s: {t for t, kinds in by.items() if any(k[0] == s for k in kinds)} for s in sorted({c["source"] for c in claims})}
    covered = {**{s: set() for s in GOLD_SOURCES}, **covered}
    ends = {t: _bars(by, t, "vdj", "break_end") for t in set.intersection(*(covered[s] for s in GOLD_SOURCES))}
    tracks = sorted(t for t, e in ends.items() if e)   # without a break end every guess would count as wrong
    guesses = {s: {t: _bars(by, t, s, "drop") for t in tracks} for s in DROP_SOURCES}
    guesses["edmformer+dropdetector"] = {t: guesses["edmformer"][t] & guesses["dropdetector"][t] for t in tracks}
    guesses["two of three"] = {t: {b for b in set().union(*(guesses[s][t] for s in DROP_SOURCES))
                                   if sum(b in guesses[s][t] for s in DROP_SOURCES) >= 2} for t in tracks}
    sections = {s: [c["bar"] for t in covered[s] for c in by[t].get((s, "section"), []) if c["bar"] > 0] for s in covered}
    return {"coverage": {s: len(t) for s, t in covered.items() if t},
            "drops": {"tracks": len(tracks),
                      "scores": {name: _scores([(g[t], ends[t]) for t in tracks]) for name, g in guesses.items()},
                      "offsets": {s: _offsets([(guesses[s][t], ends[t]) for t in tracks]) for s in DROP_SOURCES}},
            "section_starts": {s: {"n": len(bars), "on_8": _share(sum(b % 8 == 0 for b in bars), len(bars))}
                               for s, bars in sections.items() if bars},
            "gold": _gold(by, covered, answers)}


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
    if result["gold"]:
        lines += ["", "against Gold bars of drops", f"  {'':<24}{'answers':>8}{'exact':>9}{'+-1':>9}"]
        lines += [f"  {s:<24}{v['n']:>8}{cell(v['exact']):>9}{cell(v['within_1']):>9}" for s, v in result["gold"].items()]
    return "\n".join(lines)
