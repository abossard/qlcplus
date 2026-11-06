"""Review rounds: Theories tested by short Probes, and the reviewer's answers to them (Gold).

Files under <out>/rounds/:
- round-NN.json: one round's theories and probes, fixed once made;
- answers.json: {probe id: {answer, at, round, track, claim}}, the newest answer per probe. Each answer
  keeps the claim it judged, so Gold stays valid when the analysis changes;
- answers-log.jsonl: every answer ever given, append only, so a changed answer is never lost.
"""

import json
import os
import re
import tempfile
import threading
from datetime import datetime
from pathlib import Path

ROUND_FILE = re.compile(r"round-(\d{2})\.json")
OPTIONS = {
    "yesno": [("yes", "Yes"), ("no", "No"), ("earlier", "Earlier"), ("later", "Later"), ("skip", "Can't tell")],
    "ab": [("a", "A"), ("b", "B"), ("neither", "Neither"), ("skip", "Can't tell")],
    "label": [("yes", "Yes"), ("no", "No"), ("skip", "Can't tell")],
}
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


def validate_answer(probe, answer):
    allowed = [v for v, _ in OPTIONS[probe["question"]]]
    return None if isinstance(answer, str) and answer in allowed else f"answer must be one of {', '.join(allowed)}"


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
    if "alternatives" in probe:
        record["alternatives"] = probe["alternatives"]
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
               "options": [{"value": v, "label": label} for v, label in OPTIONS[p["question"]]],
               "answer": answers.get(p["id"], {}).get("answer")} for p in rnd["probes"]]
    return {"round": rnd["round"], "theories": [{"id": t["id"], "text": t["text"]} for t in rnd["theories"]],
            "probes": probes, "progress": {"answered": sum(p["answer"] is not None for p in probes), "total": len(probes)}}


# --- making a round -------------------------------------------------------------------------------

ROUND_SIZE = 40
BUCKETS = {"unsure": 0.6, "style": 0.2, "random": 0.2}   # share of a round; unsure includes follow-ups
STYLE_GROUPS = 8
THEORIES = {
    "followup": "Where you said earlier or later, the real start is 4 bars away",
    "grid": "Bar 1 is wrong where our grid disagrees with VirtualDJ",
    "drop": "Where our drop and VirtualDJ's break end disagree, VirtualDJ is right",
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


def _probe(tid, doc, theory, question, prompt, claim, plays, alternatives=None):
    p = {"track": tid, "title": doc["track"]["title"], "theory": theory, "question": question, "prompt": prompt,
         "claim": claim, "plays": plays}
    if alternatives:
        p["alternatives"] = alternatives
    return p


def _start_claim(doc, s):
    return {"kind": s["label"], "bar": s["start_bar"], "time": _bar_time(doc, s["start_bar"]), "confidence": s["confidence"]}


def _yesno_start(tid, doc, s, theory):
    claim = _start_claim(doc, s)
    return _probe(tid, doc, theory, "yesno", PROMPTS[s["label"]], claim, [{"id": "main", **_excerpt(doc, claim["time"])}])


def _ab(tid, doc, theory, prompt, claim, ours, theirs, source, rng, play):
    """A/B in random order, so 'A' is not always ours."""
    sides = [("ours", ours), (source, theirs)]
    rng.shuffle(sides)
    alternatives = {k: {"source": src, **alt} for k, (src, alt) in zip("ab", sides)}
    plays = [{"id": k, **play(alt)} for k, (_, alt) in zip("ab", sides)]
    return _probe(tid, doc, theory, "ab", prompt, claim, plays, alternatives)


def theory_candidates(tid, doc, vdj, rng):
    """{theory: [probe]} for one track whose analysis carries confidence."""
    from .evaluate import silver_boundaries
    from .vdj import grid_agreement

    out = {t: [] for t in THEORIES}
    grid_ok = doc["grid"].get("bar1_confidence", 0) >= 50
    starts = [s for s in doc["sections"] if s["start_bar"] > 0 and s["label"] in ("drop", "buildup", "breakdown")]
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
            if s["label"] == "drop" and ends and grid_ok:
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
    out["grid"] = out["grid"] or [_probe(tid, doc, "calibration", "label", PROMPTS["grid"], grid_claim,
                                         [{"id": "main", **_excerpt(doc, grid_at, 0, 4, click_shift=0)}])]
    if grid_ok:
        for s in doc["sections"]:
            if s["end_bar"] - s["start_bar"] >= 4:
                t = _bar_time(doc, s["start_bar"])
                out["labels"].append(_probe(tid, doc, "labels", "label", f"Is this part {'an' if s['label'][0] in 'aeiou' else 'a'} {s['label']}?",
                                            {"kind": "label", "label": s["label"], "bar": s["start_bar"], "end_bar": s["end_bar"],
                                             "time": t, "confidence": s.get("label_confidence", 50)},
                                            [{"id": "main", **_excerpt(doc, t, 1, 4)}]))
        out["calibration"] = [_yesno_start(tid, doc, s, "calibration") for s in starts]
    return out


def followups(answers, docs, previous):
    """A yes/no Probe 4 bars away for every 'earlier' or 'later' answer that no round followed up yet."""
    asked = {p["claim"].get("followup_of") for r in previous for p in r["probes"]}
    out = []
    for pid, a in sorted(answers.items()):
        doc = docs.get(a["track"])
        if a["answer"] not in ("earlier", "later") or doc is None or pid in asked:
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


def make_round(number, docs, vdj_by_path, answers, previous, size=ROUND_SIZE, seed=None, created=None):
    """A new round: unsure Claims across the Theories, Claims from the least-reviewed Styles, and random ones.

    Pure: docs must already carry confidence. One probe per track, and only tracks no earlier round used,
    so every round measures Fresh accuracy (follow-ups excepted)."""
    import random

    rng = random.Random(seed if seed is not None else number)
    seen_tracks = {p["track"] for r in previous for p in r["probes"]}
    fresh = sorted(t for t in docs if t not in seen_tracks)
    rng.shuffle(fresh)
    cands = {tid: theory_candidates(tid, docs[tid], vdj_by_path.get(docs[tid]["track"]["path"]), rng) for tid in fresh}
    used, probes = set(), []

    def take(p, bucket):
        if p["track"] in used and p["theory"] != "followup":
            return False
        used.add(p["track"])
        probes.append({**p, "bucket": bucket})
        return True

    n_unsure = round(size * BUCKETS["unsure"])
    for p in followups(answers, docs, previous)[:n_unsure // 3]:
        take(p, "unsure")
    # disputed grids first, then the least confident grids; every other pool by confidence
    pools = {t: sorted((p for c in cands.values() for p in c[t]), key=lambda p: (p["theory"] != t, p["claim"]["confidence"], rng.random()))
             for t in ("grid", "drop", "intro", "labels")}
    while len(probes) < n_unsure and any(pools.values()):
        for t in pools:
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

    pool = [p for tid in fresh if tid not in used for p in cands[tid]["calibration"] + cands[tid]["grid"][:1]]
    rng.shuffle(pool)
    for p in pool:
        if len(probes) >= size:
            break
        take(p, "random")

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
CONFIRMS = {"grid": lambda r: r.get("alternatives", {}).get(r["answer"], {}).get("source") == "vdj",
            "drop": lambda r: r.get("alternatives", {}).get(r["answer"], {}).get("source") == "vdj",
            "intro": lambda r: r["answer"] == "yes", "labels": lambda r: r["answer"] == "yes",
            "followup": lambda r: r["answer"] == "yes"}


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


def report(rounds, answers, docs):
    """Fresh accuracy per round, Theory results, calibration of all Gold, coverage and the Target."""
    from .confidence import _exact

    probes = {p["id"]: p for r in rounds for p in r["probes"]}
    judged = [(probes[pid], a, _exact(a)) for pid, a in answers.items() if pid in probes]
    judged = [(p, a, e) for p, a, e in judged if e is not None]
    per_round = []
    for r in rounds:
        mine = [(p, a, e) for p, a, e in judged if a["round"] == r["round"] and p["theory"] != "followup"]
        per_round.append({"round": r["round"], "answered": sum(1 for p in r["probes"] if p["id"] in answers),
                          "total": len(r["probes"]),
                          "fresh_exact": {g: _share([e for p, a, e in mine if group_of(p["claim"]) == g])
                                          for g in ("grid", "start", "label")}})
    theories = {}
    for p, a, _ in judged:
        if p["theory"] in CONFIRMS:
            theories.setdefault(p["theory"], []).append(CONFIRMS[p["theory"]](a))
    claims = [(p, e) for p, a, e in judged if group_of(p["claim"])]
    bands = [{"band": f"{lo}-{min(hi, 100)}", "n": len(hits), "exact": _share(hits)}
             for lo, hi in zip(BANDS[:-1], BANDS[1:])
             for hits in [[e for p, e in claims if lo <= p["claim"]["confidence"] < hi]]]
    sure = [e for p, e in claims if p["claim"]["confidence"] >= SURE]
    lib = library_claims(docs)
    coverage = _share([c >= SURE for _, c in lib])
    sure_exact = _share(sure)
    return {"rounds": per_round,
            "theories": {t: {"n": len(v), "confirmed": _share(v)} for t, v in theories.items()},
            "calibration": bands,
            "coverage": coverage,
            "target": {"sure_exact": sure_exact, "sure_n": len(sure), "coverage": coverage,
                       "met": bool(len(sure) >= TARGET_MIN_SURE and sure_exact >= TARGET_EXACT
                                   and coverage is not None and coverage >= TARGET_COVERAGE)}}
