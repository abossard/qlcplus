"""The fused structure of one track: drops from the best evidence, a buildup before each drop.

Pure: takes the rule analysis (analyze.with_structure) and the Candidates' raw outputs (candidates.load) and returns a
document of the same shape, so everything downstream reads sections and events as before. The rule output stays in
`rules`, each source's votes in `support` (confidence reads them), and every fused Claim says which sources
put it on its exact bar (`sources`). The choices below were made on the reviewer's Gold (numbers in .session-notes/edm-analysis/process.md):
- a drop detector drop at DROP_SCORE or above is a drop start; a rules drop within MOVE_WINDOW bars of one moves onto it;
- a rules drop no source supports is no drop where a drop detector has looked (Gold: 3 of 3 such drops rejected), else it stands,
  and is corrected as far as the rules' own levels allow (the drop is where loudness, kick and bass step up);
- a buildup starts where the rules' buildup starts minus RULES_BUILDUP_EARLY (Gold: 2 bars late in 3 of 4), else at EDMFormer's
  buildup start, else a phrase line before the drop where the drop is louder than what comes before it; at least MIN_BUILDUP bars, never before the section it eats into.
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
STEP_UP = 0.1            # loudness rise into a drop that follows a drop and makes the stretch before it a buildup
OWN_KEYS = ("loud", "kick", "bass")
OWN_STEP = 0.15          # level rise between the start of a rules drop and a later stretch that moves its drop there
OWN_SPAN = (4, 12)       # bars after the rules drop in which that later stretch starts


def bar_time(doc):
    starts, bar_len = doc["bars"]["start"], 4 * doc["grid"]["period"]
    return lambda k: starts[k] if k < len(starts) else starts[-1] + bar_len * (k - len(starts) + 1)


def _starts(sections, kind):
    return [s["start_bar"] for s in sections if s["label"] == kind and s["start_bar"] > 0]


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


def _buildup_start(labels, d, sup, loud):
    """Start of the buildup before the drop at d, or None where there is none or no room for one."""
    if d < MIN_BUILDUP:
        return None
    steps_up = loud[d:d + MIN_BUILDUP].mean() - loud[d - MIN_BUILDUP:d].mean() >= STEP_UP
    if labels[d - 1] == "drop":      # straight after a drop: a buildup only if the drop steps up from a dip
        return d - MIN_BUILDUP if steps_up and _run_start(labels, d - 1) <= d - MIN_BUILDUP else None
    run = _run_start(labels, d - 1) if labels[d - 1] == "buildup" else d
    floor = _run_start(labels, run - 1) if run > 0 else 0     # the buildup never starts before the section it eats into
    em_drops = [b for b, _ in sup["drop"].get("edmformer", [])]
    em = [b for b, _ in sup["buildup"].get("edmformer", []) if b <= d - MIN_BUILDUP
          and _near([x for x in em_drops if x >= b][:1], d) is not None]
    rules = [run - RULES_BUILDUP_EARLY] if run < d else []
    for c in (*rules, *em[-1:], *([d - PHRASE] if steps_up else [])):
        c = max(c, floor)
        if 0 < c - floor < MIN_SECTION or (c == floor and run > 0 and labels[floor] == "drop"):   # no fragment left, no drop swallowed
            c = floor + MIN_SECTION
        if d - c >= MIN_BUILDUP:
            return c
    return None


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


def _sources(sup, rules_starts, bar, label):
    if label in START_KINDS:
        return sorted(s for s, points in sup[label].items() if any(b == bar and (p is None or p >= DROP_SCORE) for b, p in points))
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

    loud = np.asarray(doc["bars"]["levels"]["loud"], float)
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
    for d in fused:
        start = _buildup_start(labels, d, sup, loud)
        if start is None:
            run = _run_start(labels, d - 1) if d and labels[d - 1] == "buildup" else d
            _fill(labels, run, d, labels[run - 1] if run else "intro")     # a run too short to be a buildup
        else:
            _fill(labels, start, d, "buildup")

    labels, fused = _merge_fragments(labels, fused, np.stack([np.asarray(doc["bars"]["levels"][k], float) for k in LEVELS], axis=1))
    cuts = _cuts(labels, fused)
    rules_starts = {s["start_bar"] for s in rules["sections"]}
    sections = [{"label": labels[a], "start_bar": a, "end_bar": b, "start": round(t(a), 3) if a else 0.0, "end": round(t(b), 3),
                 "sources": _sources(sup, rules_starts, a, labels[a])} for a, b in zip(cuts, cuts[1:])]
    events = [{"kind": s["label"], "bar": s["start_bar"], "time": round(t(s["start_bar"]), 3), "sources": s["sources"]}
              for prev, s in zip([None, *sections], sections) if s["label"] in START_KINDS and (prev is None or prev["label"] != s["label"] or s["start_bar"] in fused)]
    events = sorted([*events, *(e for e in rules["events"] if e["kind"] not in START_KINDS)], key=lambda e: (e["bar"], e["kind"]))
    version = ".".join(str(doc["analyzer"]["version"]).split(".")[:2])
    return {**doc, "analyzer": {**doc["analyzer"], "fusion": FUSION_VERSION, "version": f"{version}.{FUSION_VERSION}"},
            "sections": sections, "events": events, "rules": rules, "support": {**sup, "phrase_offset": phrase_offset(sup)}}
