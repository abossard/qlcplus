"""Stateless transformers: raw outputs to flat tables for comparing sources and learning from Gold.

Raw outputs stay as they were written: our analysis (<out>/<id>.json), each Candidate's own output
(<out>/candidates/<name>/<id>.json), the VirtualDJ database and the answers (<out>/rounds/answers.json).
Nothing here reads or writes files: every function takes raw documents and returns rows, so each
table can be deleted and rebuilt from the raw outputs at any time.

Every time is also placed on our bar grid: `bar` is the nearest bar start and `offset_beats` how far
the time sits from it, so sources can be compared bar by bar.
"""

import re

import numpy as np

START_KINDS = ("drop", "buildup", "breakdown")
LEVELS = ("kick", "hihat", "bass", "instruments", "vocal", "loud")
DESCRIPTORS = ("centroid", "inst_centroid", "onset", "hihat_onset")
MIK_MP3_OFFSET = 0.049   # same measured offset as evaluate.MIK_MP3_OFFSET
DROP_SCORE = 0.5         # a Candidate's drop below this probability is not a drop


def snap(doc, t):
    """(bar, offset in beats) of time t on our grid."""
    starts = np.asarray(doc["bars"]["start"], float)
    bar = int(np.argmin(np.abs(starts - t)))
    return bar, round(float((t - starts[bar]) / doc["grid"]["period"]), 3)


def _claim(tid, doc, source, kind, t, label=None, score=None, sources=None):
    bar, offset = snap(doc, t)
    return {"track": tid, "source": source, "kind": kind, "label": label, "time": round(float(t), 3),
            "bar": bar, "offset_beats": offset, "score": None if score is None else float(score),
            "sources": None if sources is None else "+".join(sources)}


def _section_claims(tid, doc, source, sections):
    starts, out = doc["bars"]["start"], []
    for s in sections:
        t = starts[s["start_bar"]] if s["start_bar"] < len(starts) else s.get("start", 0.0)
        out.append(_claim(tid, doc, source, "section", t, s["label"], s.get("confidence"), s.get("sources")))
        if s["label"] in START_KINDS and s["start_bar"] > 0:
            out.append(_claim(tid, doc, source, s["label"], t, s["label"], s.get("confidence"), s.get("sources")))
    return out


def claims_ours(tid, doc):
    """Our bar 1 and the rules' sections, and the fused sections where the document is a fusion (doc["rules"] keeps the rules')."""
    out = [_claim(tid, doc, "rules", "bar1", doc["grid"]["first_downbeat"], score=doc["grid"].get("bar1_confidence"))]
    out += _section_claims(tid, doc, "rules", doc.get("rules", doc)["sections"])
    if "rules" not in doc:
        return out
    lifts = [_claim(tid, doc, "fused", "lift", doc["bars"]["start"][e["bar"]], "lift", sources=e.get("sources", [])) for e in doc["events"] if e["kind"] == "lift"]
    return out + _section_claims(tid, doc, "fused", doc["sections"]) + lifts


def claims_candidate(tid, doc, name, loaded):
    """Rows from candidates.load(...) output: downbeats, sections (start, label), drops (time, confidence)."""
    out = []
    if loaded["downbeats"]:
        first = doc["grid"]["first_downbeat"]
        nearest = min(loaded["downbeats"], key=lambda t: abs(t - first))
        out.append(_claim(tid, doc, name, "bar1", nearest))
    for s in loaded["sections"]:
        out.append(_claim(tid, doc, name, "section", s["start"], s["label"]))
        if s["label"] in START_KINDS and s["start"] > 0:
            out.append(_claim(tid, doc, name, s["label"], s["start"], s["label"]))
    if name != "edmformer":   # EDMFormer's drops are its drop sections, already above
        out += [_claim(tid, doc, name, "drop", d["time"], "drop", d["confidence"]) for d in loaded["drops"]]
    return out


def candidate_drop_bars(tid, doc, name, loaded):
    """Sorted bars on our grid where a Candidate puts a drop it is sure enough of."""
    return sorted({c["bar"] for c in claims_candidate(tid, doc, name, loaded)
                   if c["kind"] == "drop" and (c["score"] is None or c["score"] >= DROP_SCORE)})


def claims_silver(tid, doc, vdj, mik_offset=MIK_MP3_OFFSET):
    """VirtualDJ bar 1 and break markers, Mixed In Key energy cues."""
    from .vdj import grid_agreement

    if not vdj or not vdj.get("period"):
        return []
    period = doc["grid"]["period"]
    g = grid_agreement(doc["beats"], doc["bars"]["start"], doc["grid"]["bpm"], vdj["period"], vdj["phase"])
    p = g["bar_phase"]          # our bar 1 sits on VirtualDJ's beat p: its bar 1 is the nearest of -p or 4-p beats away
    shift = -p if p <= 2 else 4 - p
    out = [_claim(tid, doc, "vdj", "bar1", doc["grid"]["first_downbeat"] + shift * period, score=-g["bpm_diff"])]
    for a, b in vdj.get("breaks", []):
        out.append(_claim(tid, doc, "vdj", "break_start", a + (vdj.get("period") or 0.0)))
        out.append(_claim(tid, doc, "vdj", "break_end", b))
    for c in vdj.get("cues", []):
        m = re.match(r"Energy\s*(\d+)", c["name"])
        if m:
            out.append(_claim(tid, doc, "mik", "energy", c["pos"] - mik_offset, f"E{m.group(1)}", int(m.group(1))))
    return out


def answer_rows(answers, docs):
    """One row per Gold answer (a Probe and its repeats count once, see rounds.gold_answers); `exact` is True, False or None (couldn't tell).
    A buildup the reviewer located where we had none is stored as a "missed" Claim with a "none" alternative: it is a buildup answer."""
    from .confidence import _exact, gold_bar
    from .rounds import gold_answers

    rows = []
    for pid, a in sorted(gold_answers(answers).items()):
        claim, doc = a["claim"], docs.get(a["track"])
        bar, offset = snap(doc, claim["time"]) if doc else (claim["bar"], None)
        rows.append({"probe": pid, "round": a["round"], "track": a["track"],
                     "kind": "buildup" if claim["kind"] == "missed" and "none" in a.get("alternatives", {}) else claim["kind"], "label": claim.get("label"), "time": claim["time"], "bar": bar, "offset_beats": offset,
                     "answer": a["answer"], "exact": _exact(a), "gold_bar": gold_bar(a), "confidence_asked": claim.get("confidence"),
                     "answered_at": a.get("at"), "analyzer": doc.get("analyzer", {}).get("version") if doc else None})
    return rows


def bar_rows(tid, doc, claims, answers=()):
    """One row per bar: our per-bar levels, what each source says at that bar, and the Gold for it."""
    starts = doc["bars"]["start"]
    lv, desc = doc["bars"]["levels"], doc["bars"].get("descriptors", {})
    n = len(lv["loud"])
    rows = [{"track": tid, "bar": k, "time": starts[k] if k < len(starts) else None,
             **{k2: lv[k2][k] for k2 in LEVELS if k2 in lv},
             **{d: desc[d][k] for d in DESCRIPTORS if d in desc and k < len(desc[d])},
             **({f"mfcc_{i}": v for i, v in enumerate(desc["mfcc"][k])} if "mfcc" in desc and k < len(desc["mfcc"]) else {})}
            for k in range(n)]
    sections = {}
    for c in claims:
        if c["kind"] == "section":
            sections.setdefault(c["source"], []).append(c)
        if 0 <= c["bar"] < n and c["kind"] != "section":
            # starts carry the source's own score where it has one (confidence, probability), else 1
            value = c["score"] if c["kind"] in START_KINDS and c["score"] is not None else 1.0
            key = f"{c['source']}_{c['kind']}"
            rows[c["bar"]][key] = max(rows[c["bar"]].get(key, 0.0), value)
    for source, secs in sections.items():
        secs = sorted(secs, key=lambda c: c["bar"])
        for i, c in enumerate(secs):
            end = secs[i + 1]["bar"] if i + 1 < len(secs) else n
            if 0 <= c["bar"] < n:
                rows[c["bar"]][f"{source}_start"] = True
            for k in range(max(0, c["bar"]), min(end, n)):
                rows[k][f"{source}_label"] = c["label"]
    for a in answers:
        if a["track"] == tid and a["exact"] is not None and 0 <= a["bar"] < n:
            rows[a["bar"]][f"gold_{a['kind']}"] = a["exact"]
    return rows


def track_row(tid, doc, sources, group=None):
    return {"track": tid, "title": doc["track"]["title"], "bpm": doc["grid"]["bpm"],
            "grid_share": doc["grid"].get("confidence"), "bar1_confidence": doc["grid"].get("bar1_confidence"),
            "bars": len(doc["bars"]["levels"]["loud"]), "duration": doc["track"].get("duration"),
            "analyzer": doc.get("analyzer", {}).get("version"), "style_group": group, "sources": sorted(sources)}
