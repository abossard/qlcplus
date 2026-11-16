"""Diff/import round: the section edits made in QLC+ asked back as A/B Probes against the Analysis.

QLC+ saves a song Show's section edits in the workspace as <SongDirector><Sections Show="id"><Pin Label Start End/>, in ms of
song time (the audio file's own time, whatever the clip's place in the Show). Only the reviewer's answer in the round is Gold;
the edits themselves never are. A difference is asked where the Analysis makes a Claim about it: a drop or breakdown start
(our start against the edited one) and the label of a section an edit covers whole. Buildup starts are nominal and intro and
outro have no start Claim, so their boundaries are not asked; a drop split in two is one drop with a lift.
"""

import json
import os
import random
import xml.etree.ElementTree as ET
from datetime import datetime

from .rounds import MAX_CLIP, MAX_TAP, THEORIES, _ab, _bar_time, _excerpt

ASKED_STARTS = ("drop", "breakdown")


def _tag(el):
    return el.tag.rsplit("}", 1)[-1]


def _pin(el):
    label, start, end = el.get("Label"), el.get("Start"), el.get("End")
    if not label or start is None or end is None:
        raise ValueError(f"a Pin needs Label, Start and End: {ET.tostring(el, encoding='unicode').strip()}")
    return {"label": label, "start": float(start) / 1000, "end": float(end) / 1000}


def workspace_edits(xml_text, folder=None):
    """[{"path": audio file, "pins": [{label, start, end} in seconds of song time]}] per song Show with edits, in workspace order.
    QLC+ saves a file below the workspace's folder relative to it: such a path is resolved against `folder`. ValueError on a broken Pin."""
    root = ET.fromstring(xml_text)
    functions = [el for el in root.iter() if _tag(el) == "Function"]
    audio = {el.get("ID"): next((s.text for s in el if _tag(s) == "Source"), None) for el in functions if el.get("Type") == "Audio"}
    shows = {el.get("ID"): next((audio[sf.get("ID")] for sf in el.iter() if _tag(sf) == "ShowFunction" and audio.get(sf.get("ID"))), None)
             for el in functions if el.get("Type") == "Show"}
    out = []
    for sections in (el for el in root.iter() if _tag(el) == "Sections"):
        path = shows.get(sections.get("Show"))
        pins = [_pin(p) for p in sections if _tag(p) == "Pin"]
        if path and folder and not os.path.isabs(path):
            path = os.path.normpath(os.path.join(folder, path))
        if path and pins:
            out.append({"path": path, "pins": pins})
    return out


def _bar_of(doc, t):
    """The nearest bar line, the end of the last bar included (a section ending at the song's end)."""
    return min(range(len(doc["bars"]["start"]) + 1), key=lambda b: abs(_bar_time(doc, b) - t))


def _start_probes(tid, doc, spans, rng):
    """Our drop and breakdown starts inside an edited span against the edited starts of the same kind, nearest first."""
    out = []
    for kind in ASKED_STARTS:
        ours = [s for s in doc["sections"] if s["label"] == kind and s["start_bar"] > 0
                and any(a <= s["start_bar"] < b for _, a, b in spans)]
        theirs = [a for label, a, _ in spans if label == kind and a > 0]
        same = {s["start_bar"] for s in ours} & set(theirs)
        ours = [s for s in ours if s["start_bar"] not in same]
        theirs = [a for a in theirs if a not in same]
        pairs = sorted(((abs(s["start_bar"] - a), s["start_bar"], a, s) for s in ours for a in theirs), key=lambda x: x[:3])
        used_ours, used_theirs = set(), set()
        for gap, bar, a, s in pairs:
            if gap > MAX_TAP or bar in used_ours or a in used_theirs:
                continue
            used_ours.add(bar)
            used_theirs.add(a)
            claim = {"kind": kind, "bar": bar, "time": _bar_time(doc, bar), "confidence": s["confidence"]}
            out.append(_ab(tid, doc, "qlc", f"Which marker is where the {kind} starts?", claim, {"bar": bar, "time": claim["time"]},
                           {"bar": a, "time": _bar_time(doc, a)}, "qlc", rng, lambda alt: _excerpt(doc, alt["time"])))
    return out


def _label_probes(tid, doc, spans, rng):
    """Our label of every section an edit covers whole, against the edit's label where they differ."""
    out = []
    for s in doc["sections"]:
        label = next((label for label, a, b in spans if a <= s["start_bar"] and s["end_bar"] <= b), s["label"])
        if label == s["label"]:
            continue
        t = _bar_time(doc, s["start_bar"])
        claim = {"kind": "label", "label": s["label"], "bar": s["start_bar"], "end_bar": s["end_bar"], "time": t,
                 "confidence": s.get("label_confidence", 50)}
        p = _ab(tid, doc, "qlc", "", claim, {"label": s["label"]}, {"label": label}, "qlc", rng,
                lambda alt: _excerpt(doc, t, 2, min(MAX_CLIP - 2, s["end_bar"] - s["start_bar"])))
        p["prompt"] = f"Which fits this part: A {p['alternatives']['a']['label']} or B {p['alternatives']['b']['label']}?"
        out.append(p)
    return out


def edit_probes(tid, doc, pins, rng):
    """The A/B Probes for one track's edits, by bar. Pure."""
    spans = [(p["label"], _bar_of(doc, p["start"]), _bar_of(doc, p["end"])) for p in pins]
    probes = _start_probes(tid, doc, spans, rng) + _label_probes(tid, doc, spans, rng)
    return [{**p, "fresh": False, "bucket": "qlc"} for p in sorted(probes, key=lambda p: p["claim"]["bar"])]


def _question(p):
    """What a Probe asks, whatever its A/B order and the Claim's confidence: the same question has the same key."""
    claim = {k: v for k, v in p["claim"].items() if k != "confidence"}
    return json.dumps([p["track"], claim, sorted(json.dumps(alt, sort_keys=True) for alt in p["alternatives"].values())], sort_keys=True)


def import_round(number, docs, edits, seed=None, created=None, previous=(), answers=None):
    """Round `number` from [(track id, pins)], one entry per song Show (two Shows may play the same song); tracks without an
    Analysis are left out, the same question from two Shows is asked once, and a question an earlier round asked and got an
    answer for is not asked again (a changed edit is a new question). Pure."""
    rng = random.Random(seed if seed is not None else number)
    probes = []
    asked = {_question(p) for r in previous for p in r["probes"] if p["id"] in (answers or {}) and "alternatives" in p}
    for tid, pins in edits:
        for p in edit_probes(tid, docs[tid], pins, rng) if tid in docs else []:
            key = _question(p)
            if key not in asked:
                asked.add(key)
                probes.append(p)
    return {"round": number, "created": created or datetime.now().isoformat(timespec="seconds"),
            "theories": [{"id": "qlc", "text": THEORIES["qlc"]}] if probes else [],
            "probes": [{"id": f"r{number:02d}-p{i + 1:03d}", **p} for i, p in enumerate(probes)]}
