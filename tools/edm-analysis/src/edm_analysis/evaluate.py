"""Score analyses against VirtualDJ / Mixed In Key reference points and the reviewer's gold.

VirtualDJ and Mixed In Key are silver references (useful, not truth); the reviewer's
ratings are the only gold for labels and drops. Tracks split by id into a dev half,
used for tuning, and a test half, used only to report.
"""

import json
import re
from pathlib import Path

import numpy as np

from .library import in_library, load_scope
from .ratings import gold_from_rating, merge_gold
from .vdj import grid_agreement

MIK_MP3_OFFSET = 0.049   # Mixed In Key cues on MP3s sit this late against VirtualDJ's grid (measured on this library)
SECTION_EVENTS = ("drop", "buildup", "breakdown")


def split_of(track_id):
    return "dev" if int(track_id[0], 16) < 8 else "test"


def _nearest_bar(bars, t):
    i = int(np.searchsorted(bars, t))
    if i > 0 and (i == len(bars) or t - bars[i - 1] < bars[i] - t):
        i -= 1
    return i


def silver_boundaries(vdj, bar_starts, mik_offset=MIK_MP3_OFFSET):
    """Bar indices of VirtualDJ break starts and ends and of Mixed In Key energy cues."""
    bars = np.asarray(bar_starts)
    period = vdj.get("period") or 0.0
    return {
        "vdj_break_start": sorted({_nearest_bar(bars, a + period) for a, _ in vdj.get("breaks", [])}),
        "vdj_break_end": sorted({_nearest_bar(bars, b) for _, b in vdj.get("breaks", [])}),
        "mik_energy": sorted({_nearest_bar(bars, c["pos"] - mik_offset) for c in vdj.get("cues", [])
                              if re.match(r"Energy\s*\d+", c["name"])}),
    }


def boundary_scores(ours, ref, tol):
    """(precision, recall) with one-to-one matching within `tol` bars; precision is None without predictions."""
    ours, ref = sorted(ours), sorted(ref)
    used, hits = set(), 0
    for r in ref:
        candidates = [(abs(o - r), j) for j, o in enumerate(ours) if j not in used and abs(o - r) <= tol]
        if candidates:
            used.add(min(candidates)[1])
            hits += 1
    precision = hits / len(ours) if ours else None
    recall = hits / len(ref) if ref else None
    return precision, recall


def label_agreement(ours, gold):
    """Share of bars inside gold sections where our label matches."""
    def label_at(sections, bar):
        return next((s["label"] for s in sections if s["start_bar"] <= bar < s["end_bar"]), None)
    bars = [b for g in gold for b in range(g["start_bar"], g["end_bar"])]
    if not bars:
        return None
    return sum(label_at(ours, b) == label_at(gold, b) for b in bars) / len(bars)


def score_track(doc, vdj, golds):
    """All metrics for one analysed track."""
    bars = doc["bars"]["start"]
    starts = [s["start_bar"] for s in doc["sections"][1:]]
    drops = [e["bar"] for e in doc["events"] if e["kind"] == "drop"]
    out = {"id": doc["track"]["sha256"][:16], "title": doc["track"]["title"]}
    if vdj and vdj.get("period"):
        out["grid"] = grid_agreement(doc["beats"], bars, doc["grid"]["bpm"], vdj["period"], vdj["phase"])
        silver = silver_boundaries(vdj, bars)
        for name, ref in silver.items():
            if ref:
                out[name] = {"recall_1bar": boundary_scores(starts, ref, 1)[1], "recall_exact": boundary_scores(starts, ref, 0)[1], "n": len(ref)}
        if silver["vdj_break_end"]:
            out["drop_at_break_end"] = boundary_scores(drops, silver["vdj_break_end"], 1)[1]
    if golds:
        gold = merge_gold(golds)
        gold_starts = sorted({g["start_bar"] for g in gold["sections"] if g["start_bar"] > 0})
        if gold_starts:
            out["gold_boundaries"] = dict(zip(("precision_1bar", "recall_1bar"), boundary_scores(
                [b for b in starts if any(g["start_bar"] <= b < g["end_bar"] for g in gold["sections"])], gold_starts, 1)))
            out["gold_labels"] = label_agreement(doc["sections"], gold["sections"])
        for kind in SECTION_EVENTS:
            present = [e["bar"] for e in gold["events"] if e["kind"] == kind and e["present"]]
            absent = [e["bar"] for e in gold["events"] if e["kind"] == kind and not e["present"]]
            ours = [e["bar"] for e in doc["events"] if e["kind"] == kind]
            if present or absent:
                out[f"gold_{kind}"] = {"recall_1bar": boundary_scores(ours, present, 1)[1] if present else None,
                                       "false_kept": sum(any(abs(o - a) <= 0 for o in ours) for a in absent),
                                       "n": len(present)}
    return out


def _mean(rows, *keys):
    vals = []
    for r in rows:
        v = r
        for k in keys:
            v = v.get(k) if isinstance(v, dict) else None
        if isinstance(v, (int, float)):
            vals.append(float(v))
    return (round(float(np.mean(vals)), 3), len(vals)) if vals else (None, 0)


def summarize_scores(rows):
    out = {}
    for split in ("dev", "test"):
        part = [r for r in rows if split_of(r["id"]) == split]
        out[split] = {
            "tracks": len(part),
            "tempo_match": _mean([{"v": float(abs(r["grid"]["bpm_diff"]) < 0.05)} for r in part if "grid" in r], "v"),
            "bar1_match": _mean([{"v": float(r["grid"]["bar_phase"] == 0)} for r in part if "grid" in r], "v"),
            "vdj_break_end_recall_1bar": _mean(part, "vdj_break_end", "recall_1bar"),
            "vdj_break_start_recall_1bar": _mean(part, "vdj_break_start", "recall_1bar"),
            "mik_energy_recall_1bar": _mean(part, "mik_energy", "recall_1bar"),
            "mik_energy_recall_exact": _mean(part, "mik_energy", "recall_exact"),
            "drop_at_vdj_break_end": _mean(part, "drop_at_break_end"),
            "gold_boundary_recall_1bar": _mean(part, "gold_boundaries", "recall_1bar"),
            "gold_boundary_precision_1bar": _mean(part, "gold_boundaries", "precision_1bar"),
            "gold_label_agreement": _mean(part, "gold_labels"),
            "gold_drop_recall_1bar": _mean(part, "gold_drop", "recall_1bar"),
            "gold_buildup_recall_1bar": _mean(part, "gold_buildup", "recall_1bar"),
        }
    return out


def library_docs(out_dir, scoped=True):
    """{track id: analysis} for the analysed tracks in the Library (every analysed track with scoped=False)."""
    scope = load_scope(out_dir) if scoped else None
    docs = {}
    for f in sorted(Path(out_dir).glob("*.json")):
        if re.fullmatch(r"[0-9a-f]{16}\.json", f.name):
            doc = json.loads(f.read_text())
            if "sections" in doc and in_library(doc["track"]["path"], scope):
                docs[f.stem] = doc
    return docs


def evaluate_library(out_dir, vdj_songs, ratings_path=None, history_path=None):
    out_dir = Path(out_dir)
    ratings = json.loads(Path(ratings_path).read_text()) if ratings_path and Path(ratings_path).exists() else {}
    history = json.loads(Path(history_path).read_text()) if history_path and Path(history_path).exists() else {}
    rows = []
    for tid, doc in library_docs(out_dir).items():
        golds = [h["gold"] for h in history.get(tid, [])] + ([gold_from_rating(ratings[tid])] if tid in ratings else [])
        rows.append(score_track(doc, vdj_songs.get(doc["track"]["path"]), golds))
    return rows, summarize_scores(rows)
