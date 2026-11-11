"""The fused structure of one track: drops from the best evidence, a buildup before each drop.

Pure: takes the rule analysis (analyze.with_structure) and the Candidates' raw outputs (candidates.load) and returns a
document of the same shape, so everything downstream reads sections and events as before. The rule output stays in
`rules`, each source's votes in `support` (confidence reads them), and every fused Claim says which sources
put it on its exact bar (`sources`). The choices below were made on the reviewer's Gold (numbers in .session-notes/edm-analysis/process.md):
- a drop detector drop at DROP_SCORE or above is a drop start; a rules drop within MOVE_WINDOW bars of one moves onto it;
- a rules drop no source supports is no drop where a drop detector has looked (Gold: 3 of 3 such drops rejected), else it stands,
  and is corrected as far as the rules' own levels allow (the drop is where loudness, kick and bass step up);
- a buildup needs evidence (the rules' buildup, or EDMFormer's) and starts where the rules' starts minus RULES_BUILDUP_EARLY (Gold:
  2 bars late in 3 of 4), or at EDMFormer's where that makes it EM_LONGER bars longer; between two drops it also needs EDMFormer
  and a real dip in kick and bass; at least MIN_BUILDUP bars, never before the section it eats into;
- a drop straight after a drop is the same drop: the later hit is a `lift` event, not a section start.
"""

import numpy as np

from .transform import DROP_SCORE, START_KINDS, snap

FUSION_VERSION = 1       # bump when these rules change: documents made from older ones are told apart by analyzer.version
MOVE_WINDOW = 8          # bars within which a rules drop is the same drop as another source's
MIN_BUILDUP = 4
MIN_SECTION = 4          # no fused section is shorter
LEVELS = ("kick", "hihat", "bass", "instruments", "vocal", "loud")
RULES_BUILDUP_EARLY = 2
PHRASE = 8
SURE_DROP = 0.9          # detector probability that sets a track's phrase offset
DETECTOR_FLOOR = 0.05    # weaker detector candidates are noise
EM_LONGER = 8            # EDMFormer's buildup start replaces the rules' where it is this many bars earlier (a long buildup the rules cut short)
DIP = 0.35               # kick and bass both fall below this inside a buildup that sits between two drops
OWN_KEYS = ("loud", "kick", "bass")
OWN_STEP = 0.15          # level rise between the start of a rules drop and a later stretch that moves its drop there
OWN_SPAN = (4, 12)       # bars after the rules drop in which that later stretch starts


def bar_time(doc):
    starts, bar_len = doc["bars"]["start"], 4 * doc["grid"]["period"]
    return lambda k: starts[k] if k < len(starts) else starts[-1] + bar_len * (k - len(starts) + 1)


def _starts(sections, kind):
    """Bars where the rules start a section of this kind; the track start counts only for a buildup."""
    return [s["start_bar"] for s in sections if s["label"] == kind and (s["start_bar"] > 0 or kind == "buildup")]


def support(doc, rules_sections, loaded):
    """{kind: {source: [[bar, probability or None]]}}: where each source that has output puts a start of each kind."""
    bar = lambda t: snap(doc, t)[0]
    out = {k: {"rules": [[b, None] for b in _starts(rules_sections, k)]} for k in START_KINDS}
    if "edmformer" in loaded:
        for k in START_KINDS:
            out[k]["edmformer"] = [[bar(s["start"]), None] for s in loaded["edmformer"]["sections"] if s["label"] == k and s["start"] > 0]
    if "dropdetector" in loaded:
        out["drop"]["dropdetector"] = [[bar(d["time"]), round(d["confidence"], 2)] for d in loaded["dropdetector"]["drops"]
                                       if d["confidence"] >= DETECTOR_FLOOR]
    return out


def phrase_offset(sup):
    """Bar offset 0 to 7 of the track's 8-bar phrase lines: where the surest drops sit, 0 unless clearly another."""
    sure = [b for b, p in sup["drop"].get("dropdetector", []) if p >= SURE_DROP] + [b for b, _ in sup["drop"].get("edmformer", [])]
    votes = np.bincount(np.asarray(sure or [b for b, _ in sup["drop"]["rules"]], int) % PHRASE, minlength=PHRASE)
    best = int(votes.argmax())
    return best if votes[best] > 1.5 * votes[0] else 0


def _near(bars, bar):
    return min((b for b in bars if abs(b - bar) <= MOVE_WINDOW), key=lambda b: (abs(b - bar), b), default=None)


def _own_bar(doc, section):
    """Where the rules' own levels put the drop of a rules drop section: the later stretch that is clearly louder than its start."""
    r, e = section["start_bar"], section["end_bar"]
    lv = {k: np.asarray(doc["bars"]["levels"][k], float) for k in OWN_KEYS}
    best, top = r, 0.0
    for c in range(r + OWN_SPAN[0], min(r + OWN_SPAN[1], e - MIN_BUILDUP, len(lv["loud"]) - MIN_BUILDUP) + 1):
        step = float(np.mean([lv[k][c:min(e, c + 4)].mean() - lv[k][r:c].mean() for k in OWN_KEYS]))
        if step > top:
            best, top = c, step
    return best if top >= OWN_STEP else r


def _fill(labels, a, b, label):
    labels[max(a, 0):min(b, len(labels))] = [label] * max(0, min(b, len(labels)) - max(a, 0))


def _run_start(labels, i):
    while i > 0 and labels[i - 1] == labels[i]:
        i -= 1
    return i


def _pre_drop(labels, d, sup, kick, bass, rules_buildups):
    """(label, start) of the stretch before the drop at d: a buildup, a breakdown, or None for neither. A buildup needs evidence: the rules' own
    buildup (start less two bars) or EDMFormer's (used where it makes the buildup EM_LONGER bars longer, or the rules have none). Between two
    drops it also needs EDMFormer and a real dip in kick and bass; with only the rules and a dip the stretch is a breakdown, and without a dip
    the second drop is a lift inside the first. At least MIN_BUILDUP bars, never before the section it eats into."""
    if d < MIN_BUILDUP:
        return None
    run = _run_start(labels, d - 1) if labels[d - 1] == "buildup" else d
    floor = _run_start(labels, run - 1) if run > 0 else 0
    em_drops = [b for b, _ in sup["drop"].get("edmformer", [])]
    em = [b for b, _ in sup["buildup"].get("edmformer", []) if b <= d - MIN_BUILDUP and _near([x for x in em_drops if x >= b][:1], d) is not None]
    start = run - (RULES_BUILDUP_EARLY if run in rules_buildups else 0) if run < d else None
    if em and (start is None or em[-1] <= start - EM_LONGER):
        start = em[-1]
    if start is None:
        return None
    start = max(start, floor)
    if 0 < start - floor < MIN_SECTION or (start == floor and run > 0 and labels[floor] == "drop"):   # no fragment left, no drop swallowed
        start = floor + MIN_SECTION
    if d - start < MIN_BUILDUP:
        return None
    if (labels[run - 1] if run else "intro") != "drop":
        return "buildup", start
    if not (kick[start:d].min() < DIP and bass[start:d].min() < DIP):
        return None
    return ("buildup", start) if em else ("breakdown", run)


def _cuts(labels, fused):
    return sorted({0, len(labels), *fused, *(i for i in range(1, len(labels)) if labels[i] != labels[i - 1])})


def _merge_fragments(labels, fused, levels):
    """Give every section shorter than MIN_SECTION the label of the neighbour whose levels it is closest to. A drop that short is
    no drop (or, merged into a drop, the earlier start stays); a fragment never takes the label of a drop that starts after it."""
    mean = lambda x, y: levels[x:y].mean(axis=0)
    while len(cuts := _cuts(labels, fused)) > 2:
        short = next(((i, a, b) for i, (a, b) in enumerate(zip(cuts, cuts[1:])) if b - a < MIN_SECTION), None)
        if short is None:
            break
        i, a, b = short
        options = [(cuts[i - 1], a)] if i else []
        if i + 2 < len(cuts) and (a in fused or b not in fused):
            options.append((b, cuts[i + 2]))
        options = options or [(b, cuts[i + 2])]
        x, y = min(options, key=lambda o: float(np.linalg.norm(mean(a, b) - mean(*o))))
        labels[a:b] = [labels[x]] * (b - a)
        fused = [d for d in fused if not a <= d < b and not (d == b and x == b and a in fused)]
        fused = sorted({*fused, *([a] if x == b and labels[x] == "drop" and (b in fused or a in fused) else [])})
    return labels, fused


def entry_drops(sections):
    """Bars of the drops with no breakdown and no buildup before them: the track enters on its first beat (not relabelled, the reviewer is asked)."""
    out, seen = [], False
    for s in sections:
        seen = seen or s["label"] in ("breakdown", "buildup")
        if s["label"] == "drop" and not seen:
            out.append(s["start_bar"])
    return out


def _sources(sup, rules_starts, bar, label):
    """Sources with a start of this kind on the exact bar; for a buildup within RULES_BUILDUP_EARLY bars, since the fusion starts it that much before the rules."""
    if label in START_KINDS:
        tol = RULES_BUILDUP_EARLY if label == "buildup" else 0
        return sorted(s for s, points in sup[label].items() if any(abs(b - bar) <= tol and (p is None or p >= DROP_SCORE) for b, p in points))
    return ["rules"] if bar in rules_starts else []


def fuse(doc, loaded):
    """doc: the rule analysis (or an earlier fusion of it); loaded: {Candidate name: candidates.load output} for those with output."""
    rules = doc.get("rules") or {"sections": doc["sections"], "events": doc["events"]}
    n, t = len(doc["bars"]["levels"]["loud"]), bar_time(doc)
    sup = support(doc, rules["sections"], loaded)
    strong = sorted({b for b, p in sup["drop"].get("dropdetector", []) if p >= DROP_SCORE and 0 < b < n})
    em_drops = [b for b, _ in sup["drop"].get("edmformer", []) if 0 < b < n]
    moves, demoted = {}, []
    for s in (s for s in rules["sections"] if s["label"] == "drop" and s["start_bar"] > 0):
        target = _near(strong, s["start_bar"])
        target = _near(em_drops, s["start_bar"]) if target is None else target
        if target is None and "dropdetector" in sup["drop"]:
            demoted.append(s)
        else:
            moves[s["start_bar"]] = _own_bar(doc, s) if target is None else target
    fused = sorted({*moves.values(), *strong})

    kick, bass = (np.asarray(doc["bars"]["levels"][k], float) for k in ("kick", "bass"))
    labels = ["intro"] * n
    for s in rules["sections"]:
        _fill(labels, s["start_bar"], s["end_bar"], s["label"])
    section_end = lambda bar: next((s["end_bar"] for s in rules["sections"] if s["start_bar"] <= bar < s["end_bar"]), n)
    for s in demoted:    # the rules' own context: before any fused drop an intro, after one a groove
        _fill(labels, s["start_bar"], s["end_bar"], "groove" if any(d < s["start_bar"] for d in fused) else "intro")
    for r, d in sorted(moves.items()):
        _fill(labels, r, d, "buildup") if d > r else _fill(labels, d, r, "drop")
    for d in fused:
        _fill(labels, d, section_end(d), "drop")
    rules_buildups = {b for b, _ in sup["buildup"]["rules"]}
    for d in fused:
        found = _pre_drop(labels, d, sup, kick, bass, rules_buildups)
        if found is None:
            run = _run_start(labels, d - 1) if d and labels[d - 1] == "buildup" else d
            _fill(labels, run, d, labels[run - 1] if run else "intro")     # no evidence for a buildup
        else:
            _fill(labels, found[1], d, found[0])

    labels, fused = _merge_fragments(labels, fused, np.stack([np.asarray(doc["bars"]["levels"][k], float) for k in LEVELS], axis=1))
    lifts = [d for d in fused if d > 0 and labels[d - 1] == "drop"]     # a drop straight after a drop is the same drop: a step inside it
    fused = [d for d in fused if d not in lifts]
    cuts = _cuts(labels, fused)
    rules_starts = {s["start_bar"] for s in rules["sections"]}
    early = {r for r, d in moves.items() if d > r}     # the rules called the stretch before the drop a drop: that is their buildup start
    sections = [{"label": labels[a], "start_bar": a, "end_bar": b, "start": round(t(a), 3) if a else 0.0, "end": round(t(b), 3),
                 "sources": sorted({*_sources(sup, rules_starts, a, labels[a]), *(["rules"] if labels[a] == "buildup" and a in early else [])})}
                for a, b in zip(cuts, cuts[1:])]
    events = [{"kind": s["label"], "bar": s["start_bar"], "time": round(t(s["start_bar"]), 3), "sources": s["sources"]}
              for prev, s in zip([None, *sections], sections) if s["label"] in START_KINDS and (prev is None or prev["label"] != s["label"] or s["start_bar"] in fused)]
    events += [{"kind": "lift", "bar": d, "time": round(t(d), 3), "sources": _sources(sup, rules_starts, d, "drop")} for d in lifts]
    events = sorted([*events, *(e for e in rules["events"] if e["kind"] not in START_KINDS)], key=lambda e: (e["bar"], e["kind"]))
    version = ".".join(str(doc["analyzer"]["version"]).split(".")[:2])
    return {**doc, "analyzer": {**doc["analyzer"], "fusion": FUSION_VERSION, "version": f"{version}.{FUSION_VERSION}"},
            "sections": sections, "events": events, "rules": rules, "support": {**sup, "phrase_offset": phrase_offset(sup)}}
