"""Confidence per Claim, in percent: how likely the Claim is on the exact bar (or, for a label, right).

Each Claim gets a few numbers of evidence from the analysis itself. A logistic model per Claim
group turns them into a probability. It is fitted on Gold once there is enough of it, else on the
Silver references (VirtualDJ grid and breaks, Mixed In Key energy cues). Labels have no Silver, so
until there is Gold their confidence is 50: unknown.
"""

import json
from datetime import date
from pathlib import Path

import numpy as np

from .structure import DEFAULT, _matrix, _novelty

MIN_GOLD = 30              # answers a group needs before Gold replaces Silver
UNKNOWN = 50
RAW_DOWNBEAT_TOLERANCE = 0.07
START_KINDS = ("drop", "buildup", "breakdown")
LABELS = ("intro", "buildup", "drop", "breakdown", "groove", "outro")
FEATURES = {
    "grid": ("grid_share", "raw_downbeats_on_bar1"),
    "start": ("novelty", "phrase_line", "kick_change", "bass_change", "loud_change", "grid_share",
              "is_drop", "is_buildup", "is_breakdown"),
    "label": ("loudness", "drive", "kick_on", "length", "position", "grid_share", *(f"is_{l}" for l in LABELS)),
}
MODEL_FILE = "confidence-model.json"


def _levels(doc):
    return {k: np.asarray(v, float) for k, v in {**doc["bars"]["levels"], **doc["bars"].get("descriptors", {})}.items()}


def _change(x, bar, w=4):
    before, after = x[max(0, bar - w):bar], x[bar:bar + w]
    return float(abs(after.mean() - before.mean())) if len(before) and len(after) else 0.0


class Evidence:
    """Per-track inputs shared by every Claim of that track."""

    def __init__(self, doc):
        self.lv = _levels(doc)
        self.share = float(doc["grid"].get("confidence", 0.0))
        bars = np.asarray(doc["bars"]["start"], float)
        raw = np.asarray(doc.get("raw", {}).get("downbeats", []), float)
        on_bar1 = float(np.mean([np.min(np.abs(bars - t)) < RAW_DOWNBEAT_TOLERANCE for t in raw])) if len(raw) and len(bars) else 0.0
        self.grid = [self.share, on_bar1]
        self.nov = _novelty(_matrix(self.lv, DEFAULT), DEFAULT.window)
        self.top = self.nov.max() or 1.0
        loud = self.lv["loud"]
        self.mean_loud = loud[loud > 0.05].mean() if (loud > 0.05).any() else 0.0
        self.n = len(loud)

    def start(self, bar, kind):
        lv = self.lv
        return [float(self.nov[max(1, bar - 1):bar + 2].max() / self.top), float(bar % 8 == 0),
                _change(lv["kick"], bar), _change(lv["bass"], bar), _change(lv["loud"], bar), self.share,
                *(float(kind == k) for k in START_KINDS)]

    def label(self, a, b, label):
        lv, b = self.lv, min(b, self.n)
        return [float(lv["loud"][a:b].mean() - self.mean_loud), float(max(lv["kick"][a:b].mean(), lv["bass"][a:b].mean())),
                float(np.mean(lv["kick"][a:b] > 0.4)), (b - a) / self.n, a / self.n, self.share,
                *(float(label == l) for l in LABELS)]


def silver_rows(doc, vdj):
    """(group, features, exact) from VirtualDJ and Mixed In Key, for tracks that have them."""
    from .evaluate import silver_boundaries
    from .vdj import grid_agreement

    if not vdj or not vdj.get("period"):
        return []
    ev = Evidence(doc)
    g = grid_agreement(doc["beats"], doc["bars"]["start"], doc["grid"]["bpm"], vdj["period"], vdj["phase"])
    rows = [("grid", ev.grid, abs(g["bpm_diff"]) < 0.05 and g["bar_phase"] == 0)]
    silver = silver_boundaries(vdj, doc["bars"]["start"])
    marks = set(silver["vdj_break_start"]) | set(silver["vdj_break_end"]) | set(silver["mik_energy"])
    if marks:
        rows += [("start", ev.start(s["start_bar"], s["label"]), s["start_bar"] in marks)
                 for s in doc["sections"] if 0 < s["start_bar"] < ev.n]
    return rows


def _exact(record):
    """True, False or None (the reviewer couldn't tell) for one answer."""
    answer = record["answer"]
    if answer == "skip":
        return None
    if answer.startswith("bar:"):   # a named bar is exact only where it is the Claim's own
        return int(answer[4:]) == record["claim"]["bar"]
    if "alternatives" in record:
        return record["alternatives"].get(answer, {}).get("source") == "ours"
    return answer == "yes"


def gold_bar(record):
    """The bar the reviewer's answer puts a start on, or None where it names none."""
    answer, claim = record["answer"], record["claim"]
    if answer.startswith("bar:"):
        return int(answer[4:])
    if "alternatives" in record:
        return record["alternatives"].get(answer, {}).get("bar")
    return claim["bar"] if answer == "yes" and claim["kind"] in (*START_KINDS, "missed") else None


def gold_rows(answers, docs):
    """(group, features, exact) for every answered Probe whose track is still analysed."""
    cache, rows = {}, []
    for record in answers.values():
        exact, doc = _exact(record), docs.get(record["track"])
        if exact is None or doc is None or record["claim"]["kind"] == "missed":
            continue
        ev = cache.setdefault(record["track"], Evidence(doc))
        claim = record["claim"]
        bar = int(np.argmin(np.abs(np.asarray(doc["bars"]["start"]) - claim["time"])))
        if claim["kind"] == "grid":
            rows.append(("grid", ev.grid, exact))
        elif claim["kind"] == "label":
            rows.append(("label", ev.label(bar, bar + claim["end_bar"] - claim["bar"], claim["label"]), exact))
        else:
            rows.append(("start", ev.start(bar, claim["kind"]), exact))
    return rows


def fit_group(rows):
    from sklearn.linear_model import LogisticRegression

    X = np.asarray([x for x, _ in rows], float)
    y = np.asarray([bool(t) for _, t in rows])
    if len(set(y)) < 2:
        return None
    mean, std = X.mean(axis=0), X.std(axis=0) + 1e-9
    lr = LogisticRegression(C=1.0, max_iter=1000).fit((X - mean) / std, y)
    return {"mean": mean.tolist(), "std": std.tolist(), "coef": lr.coef_[0].tolist(), "intercept": float(lr.intercept_[0])}


def fit(silver, gold, today=None):
    """Model per group; Gold wins once a group has MIN_GOLD answers."""
    model = {"fitted": (today or date.today()).isoformat(), "groups": {}}
    for group, names in FEATURES.items():
        g = [(x, t) for k, x, t in gold if k == group]
        s = [(x, t) for k, x, t in silver if k == group]
        source, rows = ("gold", g) if len(g) >= MIN_GOLD else ("silver", s)
        params = fit_group(rows) if rows else None
        if params:
            model["groups"][group] = {"features": list(names), "source": source, "n": len(rows), **params}
    return model


def predict(model, group, x):
    m = (model or {}).get("groups", {}).get(group)
    if m is None:
        return UNKNOWN
    z = float(np.dot((np.asarray(x, float) - m["mean"]) / m["std"], m["coef"]) + m["intercept"])
    return int(round(100.0 / (1.0 + np.exp(-z))))


def with_confidence(doc, model):
    ev = Evidence(doc)
    sections = []
    for s in doc["sections"]:
        a, b = s["start_bar"], s["end_bar"]
        sections.append({**s, "confidence": predict(model, "start", ev.start(a, s["label"])) if 0 < a < ev.n else 100,
                         "label_confidence": predict(model, "label", ev.label(a, b, s["label"])) if min(b, ev.n) > a else UNKNOWN})
    by_start = {s["start_bar"]: s["confidence"] for s in sections}
    events = [{**e, "confidence": by_start.get(e["bar"], UNKNOWN)} if e["kind"] in START_KINDS else e for e in doc["events"]]
    return {**doc, "grid": {**doc["grid"], "bar1_confidence": predict(model, "grid", ev.grid)},
            "sections": sections, "events": events}


def calibration(model, rows, edges=(0, 50, 80, 95, 101)):
    """Per confidence band: claims, share exact. An honest model's bands match their numbers."""
    out = []
    for lo, hi in zip(edges[:-1], edges[1:]):
        hits = [t for g, x, t in rows if lo <= predict(model, g, x) < hi]
        out.append({"band": f"{lo}-{min(hi, 100)}", "n": len(hits), "exact": round(float(np.mean(hits)), 3) if hits else None})
    return out


def load_model(out_dir):
    path = Path(out_dir) / MODEL_FILE
    return json.loads(path.read_text()) if path.exists() else None
