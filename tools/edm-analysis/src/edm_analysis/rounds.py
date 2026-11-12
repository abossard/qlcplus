"""Review rounds: Theories tested by short Probes, and the reviewer's answers to them (Gold).

Files under <out>/rounds/:
- round-NN.json: one round's theories and probes, fixed once made;
- answers.json: {probe id: {answer, at, round, track, claim}}, the newest answer per probe. Each answer
  keeps the claim it judged, so Gold stays valid when the analysis changes. A probe asked again in a later
  round (a repeat) has its own answer, with repeat_of: the first probe's id; gold_answers() settles them;
- answers-log.jsonl: every answer ever given, append only, so a changed answer is never lost.
"""

import json
import os
import re
import tempfile
import threading
from collections import Counter
from datetime import datetime
from pathlib import Path

from .confidence import LABELS, START_KINDS, UNKNOWN, _exact, gold_bar

ROUND_FILE = re.compile(r"round-(\d{2})\.json")
OPTIONS = {   # the buttons; bar taps and "is:<label>" come on top of them
    "yesno": [("yes", "Yes"), ("no", "No"), ("skip", "Can't tell")],
    "ab": [("a", "A"), ("b", "B"), ("neither", "Neither"), ("skip", "Can't tell")],
    "label": [("yes", "Yes"), ("no", "No"), ("skip", "Can't tell")],
    "locate": [("none", "Not in this clip"), ("skip", "Can't tell")],
}
START_OPTIONS = [("yes", "Yes"), ("none", "Not in this clip"), ("skip", "Can't tell")]   # yesno about a start: a bar tap names where it is
OLD_ANSWERS = {"yesno": ["no", "earlier", "later"]}   # no longer buttons; answers given before stay valid
TAP = ("yesno", "locate")                       # questions where the reviewer may name a bar: "bar:<n>"
MAX_TAP = 16                                    # bars from the Claim
BAR_ANSWER = re.compile(r"bar:(0|[1-9][0-9]*)")
PUBLIC_PROBE = ("id", "track", "title", "theory", "question", "prompt")
PUBLIC_PLAY = ("id", "start", "end", "mark", "click_shift")
_answers_lock = threading.Lock()


def _dir(out_dir):
    return Path(out_dir) / "rounds"


def _read(path, default):
    return json.loads(path.read_text()) if path.exists() else default


def _write_atomic(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, tmp = tempfile.mkstemp(dir=path.parent, prefix=path.name, suffix=".tmp")
    with os.fdopen(fd, "w") as f:
        json.dump(data, f, indent=1)
        f.flush()
        os.fsync(f.fileno())
    os.replace(tmp, path)


def all_rounds(out_dir):
    files = sorted(f for f in _dir(out_dir).glob("round-*.json") if ROUND_FILE.fullmatch(f.name))
    return [json.loads(f.read_text()) for f in files]


def current_round(out_dir):
    rounds = all_rounds(out_dir)
    return rounds[-1] if rounds else None


def load_answers(out_dir):
    return _read(_dir(out_dir) / "answers.json", {})


def options(probe):
    """The buttons of a probe as (answer, label); a probe may name what its 'none' means, and a yes/no with a 'none' is a start question."""
    base = START_OPTIONS if probe["question"] == "yesno" and (probe["claim"]["kind"] in START_KINDS or "none_label" in probe) else OPTIONS[probe["question"]]
    return [(v, probe.get("none_label", label) if v == "none" else label) for v, label in base]


def validate_answer(probe, answer):
    q = probe["question"]
    allowed = [v for v, _ in options(probe)] + OLD_ANSWERS.get(q, [])
    if isinstance(answer, str):
        if answer in allowed or (q == "label" and answer.startswith("is:") and answer[3:] in LABELS):
            return None
        bar = BAR_ANSWER.fullmatch(answer)
        if bar and q in TAP and probe.get("tap", True):
            if int(bar[1]) >= probe.get("before", int(bar[1]) + 1):
                return f"bar must be before bar {probe['before']}"
            near = probe.get("near", probe["claim"]["bar"])
            return None if abs(int(bar[1]) - near) <= MAX_TAP else f"bar must be within {MAX_TAP} bars of the marker"
    forms = ["is:<label>"] if q == "label" else ["bar:<n>"] if q in TAP and probe.get("tap", True) else []
    return f"answer must be one of {', '.join(allowed + forms)}"


def record_answer(out_dir, probe_id, answer, now=None):
    """(HTTP status, error). A probe may be answered again; the newest answer counts."""
    probe = next((p for r in all_rounds(out_dir) for p in r["probes"] if p["id"] == probe_id), None)
    if probe is None:
        return 404, "unknown probe"
    error = validate_answer(probe, answer)
    if error:
        return 400, error
    record = {"answer": answer, "at": now or datetime.now().isoformat(timespec="seconds"),
              "round": int(probe_id[1:3]), "track": probe["track"], "claim": probe["claim"]}
    for key in ("alternatives", "sources", "repeat_of"):
        if key in probe:
            record[key] = probe[key]
    with _answers_lock:
        answers = load_answers(out_dir)
        log = _dir(out_dir) / "answers-log.jsonl"
        with open(log, "a") as f:
            if f.tell() == 0:   # a log started after answers exist begins with them
                f.writelines(json.dumps({"probe": k, **v}) + "\n" for k, v in answers.items())
            f.write(json.dumps({"probe": probe_id, **record}) + "\n")
            f.flush()
            os.fsync(f.fileno())
        answers[probe_id] = record
        _write_atomic(_dir(out_dir) / "answers.json", answers)
    return 200, None


def public_round(rnd, answers):
    """What the question screen gets: no confidence, bucket or source that could bias the answer."""
    probes = [{**{k: p[k] for k in PUBLIC_PROBE},
               "plays": [{k: play[k] for k in PUBLIC_PLAY if k in play} for play in p["plays"]],
               "options": [{"value": v, "label": label} for v, label in options(p)],
               "tap": p.get("tap", p["question"] in TAP), **({"labels": list(LABELS)} if p["question"] == "label" else {}),
               "answer": answers.get(p["id"], {}).get("answer")} for p in rnd["probes"]]
    return {"round": rnd["round"], "theories": [{"id": t["id"], "text": t["text"]} for t in rnd["theories"]],
            "probes": probes, "progress": {"answered": sum(p["answer"] is not None for p in probes), "total": len(probes)}}


# --- making a round -------------------------------------------------------------------------------

ROUND_SIZE = 40
BUCKETS = {"unsure": 0.6, "style": 0.2, "random": 0.2}   # share of a round; unsure includes follow-ups
REPEAT_SHARE = 0.1   # of a round, asked again on top of the other shares: answers the reviewer gave earlier
STYLE_GROUPS = 8
MAX_CLIP = 16        # bars
THEORIES = {
    "followup": "Where you said earlier or later, the real start is 4 bars away",
    "grid": "Bar 1 is wrong where our grid disagrees with VirtualDJ",
    "drop": "Where our drop and VirtualDJ's break end disagree, VirtualDJ is right",
    "detectors": "Where the detectors disagree about a drop, one of them has the exact bar",
    "buildup": "Every drop has a buildup",
    "entry": "The first time the beat comes in is an intro",
    "lift": "Long drops start where we say",
    "intro": "Long loud intros hide a new part at Mixed In Key's energy cues",
    "labels": "Section labels are right",
    "calibration": "The confidence numbers are honest",
}
PROMPTS = {"drop": "Does the drop start at the marker?", "buildup": "Does the buildup start at the marker?",
           "breakdown": "Does the breakdown start at the marker?", "missed": "Does a new part start at the marker?",
           "grid": "Are the clicks on the beat, with the accent on bar 1?"}


def _bar_time(doc, bar):
    starts, bar_len = doc["bars"]["start"], 4 * doc["grid"]["period"]
    return round(starts[bar] if bar < len(starts) else starts[-1] + bar_len * (bar - len(starts) + 1), 3)


def _excerpt(doc, t, before=2, after=2, **extra):
    bar_len = 4 * doc["grid"]["period"]
    return {"start": round(max(0.0, t - before * bar_len), 3), "end": round(t + after * bar_len, 3), "mark": t, **extra}


def _probe(tid, doc, theory, question, prompt, claim, plays, alternatives=None, sources=None, **extra):
    p = {"track": tid, "title": doc["track"]["title"], "theory": theory, "question": question, "prompt": prompt,
         "claim": claim, "plays": plays, **extra}
    if alternatives:
        p["alternatives"] = alternatives
    if sources:
        p["sources"] = sources
    return p


def _start_claim(doc, s):
    return {"kind": s["label"], "bar": s["start_bar"], "time": _bar_time(doc, s["start_bar"]), "confidence": s["confidence"]}


def _span(bar, others):
    """(first, last) bar of a clip: 2 bars around bar, widened to hold each of the other bars with a bar to spare, nearest first, within MAX_CLIP."""
    first, last = bar - 2, bar + 2
    for b in sorted(others, key=lambda b: abs(b - bar)):
        wider = (min(first, b - 1), max(last, b + 2))
        if wider[1] - wider[0] <= MAX_CLIP:
            first, last = wider
    return max(0, first), last


def _yesno_start(tid, doc, s, theory, sources=None):
    claim, bar = _start_claim(doc, s), s["start_bar"]
    first, last = _span(bar, (sources or {}).values())
    return _probe(tid, doc, theory, "yesno", PROMPTS[s["label"]], claim, [{"id": "main", **_excerpt(doc, claim["time"], bar - first, last - bar)}],
                  sources=sources)


def _ab(tid, doc, theory, prompt, claim, ours, theirs, source, rng, play):
    """A/B in random order, so 'A' is not always ours."""
    sides = [("ours", ours), (source, theirs)]
    rng.shuffle(sides)
    alternatives = {k: {"source": src, **alt} for k, (src, alt) in zip("ab", sides)}
    plays = [{"id": k, **play(alt)} for k, (_, alt) in zip("ab", sides)]
    return _probe(tid, doc, theory, "ab", prompt, claim, plays, alternatives)


def _near(bar, sources):
    """{name: its drop bar nearest to bar} for the sources of {name: [drop bars]} that have one within 8 bars."""
    nearest = {name: min(bars, key=lambda b: abs(b - bar)) for name, bars in sources.items() if bars}
    return {name: b for name, b in nearest.items() if abs(b - bar) <= 8}


def _locate(tid, doc, s, sources):
    """Tap the bar of our drop s, where some source of {name: [drop bars]} puts a drop 1 to 8 bars away; else None."""
    near = _near(s["start_bar"], sources)
    if not any(b != s["start_bar"] for b in near.values()):
        return None
    bars = [s["start_bar"], *near.values()]
    first, last = max(0, min(bars) - 4), max(bars) + 2
    if last - first > 16:
        return None
    play = {"id": "main", "start": _bar_time(doc, first), "end": _bar_time(doc, last), "mark": None}
    return _probe(tid, doc, "detectors", "locate", "Tap the bar where the drop starts", _start_claim(doc, s), [play],
                  {"ours": {"bar": s["start_bar"]}, **{name: {"bar": b} for name, b in near.items()}})


def _buildup_exists(tid, doc, drop, theory):
    """Is there a buildup before our drop? The Claim says whether our Analysis has one ending at the drop; no bar is asked
    for, so the probe takes no bar taps and its 'none' reads "No"."""
    ours = next((s for s in doc["sections"] if s["label"] == "buildup" and s["end_bar"] == drop), None)
    t = _bar_time(doc, drop)
    claim = {"kind": "buildup_exists", "bar": drop, "time": t, "claimed": ours is not None, "confidence": ours["confidence"] if ours else UNKNOWN}
    return _probe(tid, doc, theory, "yesno", "Is there a buildup before this drop?", claim, [{"id": "main", **_excerpt(doc, t, 8, 2)}],
                  tap=False, none_label="No")


def _entry(tid, doc, bar):
    """Is the stretch after the beat comes in (the section that holds it) an intro?"""
    section = next((s for s in doc["sections"] if s["start_bar"] <= bar < s["end_bar"]), None)
    t = _bar_time(doc, bar)
    claim = {"kind": "label", "label": "intro", "bar": bar, "end_bar": section["end_bar"] if section else bar + 4, "time": t,
             "confidence": section.get("label_confidence", UNKNOWN) if section else UNKNOWN}
    return _probe(tid, doc, "entry", "label", "Is this part an intro?", claim, [{"id": "main", **_excerpt(doc, t, 4, 4)}])


def _lift(tid, doc, bar):
    """Tap the bar where the drop starts, in a clip that holds both the start of our merged drop and the lift bar inside it.
    None where the lift is in no drop or the clip would be longer than MAX_CLIP. (Round 5 asked a yes/no at the lift bar;
    its probes keep `near` and `none_label`.)"""
    s = next((s for s in reversed(doc["sections"]) if s["start_bar"] < bar), None)
    if s is None or s["label"] != "drop" or s["start_bar"] == 0:
        return None
    first, last = max(0, s["start_bar"] - 4), bar + 2
    if last - first > MAX_CLIP:
        return None
    play = {"id": "main", "start": _bar_time(doc, first), "end": _bar_time(doc, last), "mark": None}
    return _probe(tid, doc, "lift", "locate", "Tap the bar where the drop starts", _start_claim(doc, s), [play],
                  {"ours": {"bar": s["start_bar"]}, "lift": {"bar": bar}})


def theory_candidates(tid, doc, vdj, rng, drops=None):
    """{theory: [probe]} for one track whose analysis carries confidence. drops: {Candidate: drop bars} where
    Candidates have output for the track; its drops are then located by tapping, not compared with A/B."""
    from .evaluate import silver_boundaries
    from .vdj import grid_agreement

    out = {t: [] for t in THEORIES}
    grid_ok = doc["grid"].get("bar1_confidence", 0) >= 50
    starts = [s for s in doc["sections"] if s["start_bar"] > 0 and s["label"] in ("drop", "buildup", "breakdown")]
    ends = []
    first_drop = next((s for s in starts if s["label"] == "drop"), None)
    grid_at = _bar_time(doc, first_drop["start_bar"] if first_drop else len(doc["bars"]["start"]) // 2)
    grid_claim = {"kind": "grid", "bar": 0, "time": grid_at, "confidence": doc["grid"].get("bar1_confidence", 50)}
    if vdj and vdj.get("period"):
        g = grid_agreement(doc["beats"], doc["bars"]["start"], doc["grid"]["bpm"], vdj["period"], vdj["phase"])
        if abs(g["bpm_diff"]) < 0.05 and g["bar_phase"] != 0:
            shift = (4 - g["bar_phase"]) % 4
            out["grid"].append(_ab(tid, doc, "grid", "Which one has the accent on bar 1?", grid_claim, {"shift_beats": 0}, {"shift_beats": shift},
                                   "vdj", rng, lambda alt: _excerpt(doc, grid_at, 0, 4, click_shift=alt["shift_beats"])))
        silver = silver_boundaries(vdj, doc["bars"]["start"])
        ends = silver["vdj_break_end"]
        for s in starts:
            if s["label"] == "drop" and ends and grid_ok and drops is None:
                near = min(ends, key=lambda b: abs(b - s["start_bar"]))
                if 1 <= abs(near - s["start_bar"]) <= 8:
                    claim = _start_claim(doc, s)
                    out["drop"].append(_ab(tid, doc, "drop", "Which marker is where the drop starts?", claim,
                                           {"bar": s["start_bar"], "time": claim["time"]},
                                           {"bar": near, "time": _bar_time(doc, near)}, "vdj", rng,
                                           lambda alt: _excerpt(doc, alt["time"])))
        intro = doc["sections"][0] if doc["sections"] and doc["sections"][0]["label"] == "intro" else None
        if intro and intro["end_bar"] >= 32 and grid_ok:
            for cue in silver["mik_energy"]:
                if 8 <= cue < intro["end_bar"] - 4:
                    t = _bar_time(doc, cue)
                    out["intro"].append(_probe(tid, doc, "intro", "yesno", PROMPTS["missed"],
                                               {"kind": "missed", "bar": cue, "time": t, "confidence": 50},
                                               [{"id": "main", **_excerpt(doc, t)}]))
    if drops is not None and grid_ok:
        located = [_locate(tid, doc, s, {**drops, "vdj": ends}) for s in starts if s["label"] == "drop"]
        out["detectors"] = [p for p in located if p]
    out["grid"] = out["grid"] or [_probe(tid, doc, "calibration", "label", PROMPTS["grid"], grid_claim,
                                         [{"id": "main", **_excerpt(doc, grid_at, 0, 4, click_shift=0)}])]
    if grid_ok:
        for i, s in enumerate(doc["sections"]):
            if s["end_bar"] - s["start_bar"] >= 4 or i >= len(doc["sections"]) - 2:   # the last two sections however short
                t = _bar_time(doc, s["start_bar"])
                out["labels"].append(_probe(tid, doc, "labels", "label", f"Is this part {'an' if s['label'][0] in 'aeiou' else 'a'} {s['label']}?",
                                            {"kind": "label", "label": s["label"], "bar": s["start_bar"], "end_bar": s["end_bar"],
                                             "time": t, "confidence": s.get("label_confidence", 50)},
                                            [{"id": "main", **_excerpt(doc, t, 1, 4)}]))
        out["entry"] = [_entry(tid, doc, e["bar"]) for e in doc.get("events", []) if e["kind"] == "entry"]
        out["lift"] = [p for p in (_lift(tid, doc, e["bar"]) for e in doc.get("events", []) if e["kind"] == "lift" and e["bar"] > 0) if p]
        others = {**(drops or {}), "vdj": ends}
        out["calibration"] = [_yesno_start(tid, doc, s, "calibration", _near(s["start_bar"], others) if s["label"] == "drop" else None)
                              for s in starts if s["label"] != "buildup"]
        for s in starts:   # a buildup's start is not asked about: a draw on one asks whether its drop has a buildup
            if s["label"] == "buildup" and any(x["label"] == "drop" and x["start_bar"] == s["end_bar"] for x in doc["sections"]):
                out["calibration"].append(_buildup_exists(tid, doc, s["end_bar"], "calibration"))
        for s in starts:
            if s["label"] == "drop":
                for pool in ("buildup", "calibration"):
                    out[pool].append(_buildup_exists(tid, doc, s["start_bar"], pool))
    return out


def followups(answers, docs, previous):
    """A yes/no Probe 4 bars away for every 'earlier' or 'later' answer that no round followed up yet."""
    asked = {p["claim"].get("followup_of") for r in previous for p in r["probes"]}
    out = []
    for pid, a in sorted(answers.items()):
        doc = docs.get(a["track"])
        if a["answer"] not in ("earlier", "later") or doc is None or pid in asked or a["claim"]["kind"] == "buildup":
            continue
        bar = a["claim"]["bar"] + (-4 if a["answer"] == "earlier" else 4)
        if bar <= 0 or bar >= len(doc["bars"]["start"]):
            continue
        t = _bar_time(doc, bar)
        kind = a["claim"]["kind"]
        out.append(_probe(a["track"], doc, "followup", "yesno", PROMPTS.get(kind, PROMPTS["missed"]),
                          {"kind": kind, "bar": bar, "time": t, "confidence": 50, "followup_of": pid},
                          [{"id": "main", **_excerpt(doc, t)}]))
    return out


def style_groups(docs, k=STYLE_GROUPS, seed=0):
    """{track id: group} from how tracks sound: mean stem levels, timbre and tempo."""
    from sklearn.cluster import KMeans
    import numpy as np

    ids = sorted(docs)
    rows = []
    for tid in ids:
        d = docs[tid]
        lv = [float(np.mean(d["bars"]["levels"][k])) for k in ("kick", "hihat", "bass", "instruments", "vocal", "loud")]
        mfcc = np.asarray(d["bars"].get("descriptors", {}).get("mfcc", [[0.0] * 13]), float).mean(axis=0).tolist()
        rows.append(lv + mfcc + [d["grid"]["bpm"] / 150.0])
    X = np.nan_to_num(np.asarray(rows))   # a track without timbre data still gets a group
    X = (X - X.mean(axis=0)) / (X.std(axis=0) + 1e-9)
    labels = KMeans(n_clusters=min(k, len(ids)), n_init=10, random_state=seed).fit_predict(X)
    return dict(zip(ids, (int(x) for x in labels)))


def _rank(p):
    """Order within a pool: locate probes by how much the sources disagree, the others by confidence."""
    if p["question"] != "locate":
        return (p["claim"]["confidence"],)
    bars = {a["bar"] for a in p["alternatives"].values()}
    return (-len(bars), min(bars) - max(bars))


def _said(record):
    """What an answer means: 'yes' to a drop at bar 16 and 'bar:16' are the same, and on a start question so are 'no' and 'none'."""
    bar = gold_bar(record)
    return bar if bar is not None else "none" if record["answer"] == "no" and record["claim"]["kind"] in START_KINDS else record["answer"]


def _groups(answers):
    """{first probe id: [answered probe ids, oldest first]}: a probe and the repeats of it."""
    groups = {}
    for pid, a in answers.items():
        groups.setdefault(a.get("repeat_of", pid), []).append(pid)
    return {root: sorted(pids, key=lambda p: (answers[p]["round"], p)) for root, pids in groups.items()}


def _winner(answers, pids):
    """The first of pids whose answer more than half of the group's answers share, else None. 'Can't tell' does not vote."""
    voted = [p for p in pids if answers[p]["answer"] != "skip"]
    if not voted:
        return pids[0]
    best, n = Counter(_said(answers[p]) for p in voted).most_common(1)[0]
    return next(p for p in voted if _said(answers[p]) == best) if n * 2 > len(voted) else None


def gold_answers(answers):
    """The answers that count as Gold: one per probe and its repeats. Repeats that agree count once; where they
    disagree none counts until a third answer settles it."""
    kept = [_winner(answers, pids) for pids in _groups(answers).values()]
    return {pid: answers[pid] for pid in kept if pid is not None}


def _asks_buildup_start(probe):
    """A buildup's start is nominal, not a Claim: its yes/no and its old locate questions (also the none-case) are not asked again."""
    return probe["claim"]["kind"] == "buildup" or probe["prompt"] == "Tap the bar where the buildup starts"


def repeat_candidates(answers, docs, previous):
    """[(urgency, probe)]: earlier probes whose yes, no or bar answer is worth asking again unchanged. 0: the repeats
    disagree, 1: a drop the reviewer rejected although two sources agree on a bar, 2: two sources agree on a bar the answer
    did not name, 3: any other. Not those already settled or waiting."""
    probes = {p["id"]: p for r in previous for p in r["probes"]}
    waiting = {p["repeat_of"] for p in probes.values() if "repeat_of" in p and p["id"] not in answers}
    out = []
    for root, pids in _groups(answers).items():
        first, probe = answers[pids[0]], probes.get(root)
        named = first["answer"] in ("yes", "no") or first["answer"].startswith("bar:") or (first["answer"] == "none" and "alternatives" not in first and first["claim"]["kind"] in (*START_KINDS, "buildup_exists"))
        if probe is None or root in waiting or probe["track"] not in docs or not named or _asks_buildup_start(probe):
            continue
        voted = [p for p in pids if answers[p]["answer"] != "skip"]
        if len(voted) >= 2 and _winner(answers, pids) is not None:
            continue
        agreed = [bar for bar, n in Counter(source_bars(first).values()).items() if n >= 2]
        rejected = first["claim"]["kind"] == "drop" and first["answer"] in ("no", "none") and agreed
        out.append((0 if len(voted) >= 2 else 1 if rejected else 2 if any(bar != gold_bar(first) for bar in agreed) else 3, probe))
    return out


def make_round(number, docs, vdj_by_path, answers, previous, size=ROUND_SIZE, seed=None, created=None, candidate_drops=None):
    """A new round: unsure Claims across the Theories, Claims from the least-reviewed Styles, and random ones,
    and on top of them REPEAT_SHARE of the size in earlier answers asked again unchanged.

    Pure: docs must already carry confidence. One probe per track, and only tracks no earlier round got an
    answer on, so every round measures Fresh accuracy (follow-ups and repeats excepted). candidate_drops:
    {track id: {Candidate: drop bars on our grid}}."""
    import random

    rng = random.Random(seed if seed is not None else number)
    seen_tracks = {p["track"] for r in previous for p in r["probes"] if p["id"] in answers}
    fresh = sorted(t for t in docs if t not in seen_tracks)
    rng.shuffle(fresh)
    asked = Counter(a["claim"]["label"] for a in answers.values() if a["claim"]["kind"] == "label")   # label questions go to the labels asked least
    cands = {tid: theory_candidates(tid, docs[tid], vdj_by_path.get(docs[tid]["track"]["path"]), rng, (candidate_drops or {}).get(tid))
             for tid in fresh}
    repeats, repeated = [], set()
    for _, p in sorted(repeat_candidates(answers, docs, previous), key=lambda c: (c[0], rng.random())):
        if len(repeats) < round(size * REPEAT_SHARE) and p["track"] not in repeated:
            repeated.add(p["track"])
            repeats.append({**{k: v for k, v in p.items() if k != "id"}, "bucket": "repeat", "repeat_of": p.get("repeat_of", p["id"])})
    size -= len(repeats)
    used, probes = set(), []

    def take(p, bucket):
        if p["track"] in used and p["theory"] != "followup":
            return False
        used.add(p["track"])
        probes.append({**p, "bucket": bucket})
        asked.update([p["claim"]["label"]] if p["claim"]["kind"] == "label" else [])
        return True

    n_unsure = round(size * BUCKETS["unsure"])
    for p in followups(answers, docs, previous)[:n_unsure // 3]:
        take(p, "unsure")
    # disputed grids first, then the least confident grids; every other pool by confidence
    pools = {t: sorted((p for c in cands.values() for p in c[t]), key=lambda p: (p["theory"] != t, _rank(p), rng.random()))
             for t in ("grid", "detectors", "buildup", "entry", "lift", "drop", "intro", "labels")}
    while len(probes) < n_unsure and any(pools.values()):
        for t in (*pools, "detectors", "buildup", "entry", "labels"):   # the pools that tell most per answer get two turns
            pools["labels"].sort(key=lambda p: asked[p["claim"]["label"]])
            while pools[t] and not take(pools[t].pop(0), "unsure"):
                pass
            if len(probes) >= n_unsure:
                break

    groups = style_groups(docs) if len(docs) >= STYLE_GROUPS else {t: 0 for t in docs}
    reviewed = {}
    for a in answers.values():
        reviewed[groups.get(a["track"])] = reviewed.get(groups.get(a["track"]), 0) + 1
    for p in probes:
        reviewed[groups.get(p["track"])] = reviewed.get(groups.get(p["track"]), 0) + 1
    n_style = round(size * BUCKETS["style"])
    for _ in range(n_style):
        options = [tid for tid in fresh if tid not in used and cands[tid]["calibration"]]
        if not options:
            break
        tid = min(options, key=lambda t: (reviewed.get(groups.get(t), 0), rng.random()))
        take(rng.choice(cands[tid]["calibration"]), "style")
        reviewed[groups[tid]] = reviewed.get(groups[tid], 0) + 1

    rarest = lambda ps: sorted(ps, key=lambda p: (asked[p["claim"]["label"]], rng.random()))[:1]
    pool = [p for tid in fresh if tid not in used
            for p in cands[tid]["calibration"] + cands[tid]["grid"][:1] + cands[tid]["entry"] + rarest(cands[tid]["labels"])]
    rng.shuffle(pool)
    for p in pool:
        if len(probes) >= size:
            break
        take(p, "random")

    for p in repeats:   # anywhere in the round, so the order does not tell
        probes.insert(rng.randrange(len(probes) + 1), p)
    probes = [{"id": f"r{number:02d}-p{i + 1:03d}", **p} for i, p in enumerate(probes)]
    used_theories = {p["theory"] for p in probes}
    return {"round": number, "created": created or datetime.now().isoformat(timespec="seconds"),
            "theories": [{"id": t, "text": THEORIES[t]} for t in THEORIES if t in used_theories], "probes": probes}


def save_round(out_dir, rnd):
    _write_atomic(_dir(out_dir) / f"round-{rnd['round']:02d}.json", rnd)


# --- scoring --------------------------------------------------------------------------------------

TARGET_EXACT = 0.95
TARGET_COVERAGE = 0.70
TARGET_MIN_SURE = 20       # sure answers needed before the exact rate counts
SURE = 95
BANDS = (0, 50, 80, 95, 101)
CONFIRMS = {"detectors": lambda r: gold_bar(r) in [b for src, b in source_bars(r).items() if src != "ours"],
            "grid": lambda r: r.get("alternatives", {}).get(r["answer"], {}).get("source") == "vdj",
            "drop": lambda r: r.get("alternatives", {}).get(r["answer"], {}).get("source") == "vdj",
            "buildup": lambda r: r["answer"] == "yes" if r["claim"]["kind"] == "buildup_exists" else r["claim"]["kind"] == "buildup" and _exact(r),
            "entry": lambda r: (r["answer"] == "yes") == (r["claim"].get("label") == "intro"), "lift": _exact,
            "intro": lambda r: r["answer"] == "yes", "labels": lambda r: r["answer"] == "yes",
            "followup": lambda r: r["answer"] == "yes"}


def source_bars(record):
    """{source: bar} that each source put on the Claim of an answered Probe; ours only for our own start Claims."""
    if "alternatives" in record:
        return {a.get("source", k): a["bar"] for k, a in record["alternatives"].items() if "bar" in a}
    claim = record["claim"]
    return {"ours": claim["bar"], **record.get("sources", {})} if claim["kind"] in START_KINDS and "followup_of" not in claim else {}


def group_of(claim):
    return {"grid": "grid", "label": "label", "missed": None}.get(claim["kind"], "start")


def _share(xs):
    return round(sum(xs) / len(xs), 3) if xs else None


def library_claims(docs):
    """(group, confidence) of every Claim in the current analyses."""
    out = []
    for d in docs.values():
        out.append(("grid", d["grid"].get("bar1_confidence", 50)))
        for s in d["sections"]:
            out.append(("label", s.get("label_confidence", 50)))
            if s["start_bar"] > 0 and s["label"] in ("drop", "buildup", "breakdown"):
                out.append(("start", s.get("confidence", 50)))
    return out


def reviewer_agreement(answers):
    """{"repeats": comparable repeat answers, "same": share that match the first answer}; 'can't tell' is not comparable."""
    firsts = {pid: a for pid, a in answers.items() if "repeat_of" not in a}
    pairs = [(firsts[a["repeat_of"]], a) for a in answers.values() if a.get("repeat_of") in firsts]
    same = [_said(first) == _said(again) for first, again in pairs if "skip" not in (first["answer"], again["answer"])]
    return {"repeats": len(same), "same": _share(same)}


def report(rounds, answers, docs):
    """Fresh accuracy per round, Theory results, calibration of all Gold, coverage and the Target. Repeats are not
    fresh and not Theory results; they count as Gold once, and not at all while they disagree."""
    probes = {p["id"]: p for r in rounds for p in r["probes"]}
    judged = [(probes[pid], a, _exact(a)) for pid, a in gold_answers(answers).items() if pid in probes]
    judged = [(p, a, e) for p, a, e in judged if e is not None]
    per_round = []
    for r in rounds:
        mine = [(p, a, e) for p, a, e in judged if a["round"] == r["round"] and p["theory"] != "followup" and "repeat_of" not in p]
        per_round.append({"round": r["round"], "answered": sum(1 for p in r["probes"] if p["id"] in answers),
                          "total": len(r["probes"]),
                          "fresh_exact": {g: _share([e for p, a, e in mine if group_of(p["claim"]) == g])
                                          for g in ("grid", "start", "label")}})
    theories = {}
    for p, a, _ in judged:
        if p["theory"] in CONFIRMS and "repeat_of" not in p:
            theories.setdefault(p["theory"], []).append(CONFIRMS[p["theory"]](a))
    on_gold_bar = {}
    for _, a, _ in judged:
        gold = gold_bar(a)
        if gold is not None:
            for source, bar in source_bars(a).items():
                on_gold_bar.setdefault(source, []).append(bar == gold)
    claims = [(p, e) for p, a, e in judged if group_of(p["claim"])]
    bands = [{"band": f"{lo}-{min(hi, 100)}", "n": len(hits), "exact": _share(hits)}
             for lo, hi in zip(BANDS[:-1], BANDS[1:])
             for hits in [[e for p, e in claims if lo <= p["claim"]["confidence"] < hi]]]
    sure = [e for p, e in claims if p["claim"]["confidence"] >= SURE]
    lib = library_claims(docs)
    coverage = _share([c >= SURE for _, c in lib])
    sure_exact = _share(sure)
    return {"rounds": per_round,
            "theories": {t: {"n": len(v), "confirmed": _share(v), "text": THEORIES[t]} for t, v in theories.items()},
            "sources": {s: {"n": len(v), "exact": _share(v)} for s, v in on_gold_bar.items()},
            "reviewer": reviewer_agreement(answers),
            "calibration": bands,
            "coverage": coverage,
            "target": {"sure_exact": sure_exact, "sure_n": len(sure), "coverage": coverage,
                       "met": bool(len(sure) >= TARGET_MIN_SURE and sure_exact >= TARGET_EXACT
                                   and coverage is not None and coverage >= TARGET_COVERAGE)}}
