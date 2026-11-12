"""Confidence per Claim, in percent: how likely the Claim is on the exact bar (or, for a label, right; for a buildup, that one exists).

Each Claim gets a few numbers of evidence from the analysis itself (for a start also which sources agree on its
bar, from the fused document's `support`). A logistic model per Claim group turns them into a probability. It is
fitted on Gold once there is enough of it, else on the Silver references (VirtualDJ grid and breaks, Mixed In Key
energy cues). The grid group, whose Silver counts disagreement with VirtualDJ as wrong, takes the few Gold answers it
has in place of the Silver rows of their tracks (source "silver+gold"). Labels have no Silver, so until there is Gold their
confidence is 50: unknown.
"""

import json
from collections import Counter
from datetime import date
from pathlib import Path

import numpy as np

from .structure import DEFAULT, _matrix, _novelty
from .transform import DROP_SCORE

MIN_GOLD = 30              # answers a group needs before Gold replaces Silver
UNKNOWN = 50
RAW_DOWNBEAT_TOLERANCE = 0.07
START_KINDS = ("drop", "buildup", "breakdown")
LABELS = ("intro", "buildup", "drop", "breakdown", "outro")     # no groove: a steady beat part is named by where it sits (fusion.py)
FEATURES = {
    "grid": ("grid_share", "raw_downbeats_on_bar1"),
    "start": ("novelty", "phrase_line", "kick_change", "bass_change", "loud_change", "grid_share",
              "is_drop", "is_buildup", "is_breakdown", "det_prob", "rules_exact", "em_exact", "dd_exact", "near"),
    "label": ("loudness", "drive", "kick_on", "length", "position", "grid_share", *(f"is_{l}" for l in LABELS)),
    "exists": ("rules_bu", "em_bu", "em_drop", "kick_min", "bass_min", "loud_step", "rise_onset", "rise_centroid", "before_drop", "before_breakdown", "grid_share"),
}
AGREEMENT = ("det_prob", "rules_exact", "em_exact", "dd_exact", "near", "rules_bu", "em_bu", "em_drop")   # more agreement never lowers confidence (Gold is picked where sources disagree)
MODEL_FILE = "confidence-model.json"
REVIEWER_ERROR = 0.10      # share of the reviewer's answers assumed wrong; exact rates are also reported read under it
GOLD_FIRST = ("grid",)     # a group whose few Gold answers join the Silver rows (replacing those of their tracks) before it has MIN_GOLD


def corrected_rate(observed, error=REVIEWER_ERROR):
    """The true share behind an observed share of the reviewer's answers when they err at `error`: o = t(1 - e) + (1 - t)e."""
    return float(min(1.0, max(0.0, (observed - error) / (1 - 2 * error))))


def bands(pairs, edges=(0, 50, 80, 95, 101)):
    """Per confidence band of (percent, exact) pairs: Claims, share exact, and that share read under the reviewer's error rate."""
    out = []
    for lo, hi in zip(edges[:-1], edges[1:]):
        hits = [t for p, t in pairs if lo <= p < hi]
        exact = round(float(np.mean(hits)), 3) if hits else None
        out.append({"band": f"{lo}-{min(hi, 100)}", "n": len(hits), "exact": exact, "corrected": None if exact is None else round(corrected_rate(exact), 3)})
    return out


def silver_without_gold(silver, gold, groups=GOLD_FIRST):
    """Silver rows by track, without the rows of a group on tracks where Gold has judged that group."""
    return {tid: [r for r in rows if not (r[0] in groups and any(g[0] == r[0] for g in gold.get(tid, [])))] for tid, rows in silver.items()}


def held_out(silver, gold, group):
    """(percent, exact) for every Gold row of the group, each predicted by a model fitted without that track's Gold."""
    flat = lambda by: [r for rows in by.values() for r in rows]
    pairs, total = [], sum(r[0] == group for r in flat(gold))
    for tid, rows in gold.items():
        rest = {t: g for t, g in gold.items() if t != tid}
        mine = [r for r in rows if r[0] == group]
        if mine:
            # a group the full model fits on Gold is fitted on Gold with a track left out too, however few rows are left
            model = fit(flat(silver_without_gold(silver, rest)), flat(rest), min_gold=total - len(mine) if total >= MIN_GOLD else MIN_GOLD)
            pairs += [(predict(model, group, x), exact) for _, x, exact in mine]
    return pairs


def _kind(x):
    """The kind of a start Claim from its evidence: drop, buildup, breakdown, or other (intro, outro)."""
    return next((k for k in START_KINDS if x[FEATURES["start"].index(f"is_{k}")]), "other")


def _levels(doc):
    return {k: np.asarray(v, float) for k, v in {**doc["bars"]["levels"], **doc["bars"].get("descriptors", {})}.items()}


def _finite(x):
    """Evidence as plain floats; a track with missing levels (NaN) gets 0 where it has none."""
    return [float(v) for v in np.nan_to_num(np.asarray(x, float))]


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
        self.support = doc.get("support", {})
        self.sections = doc.get("sections", [])
        self.offset = self.support.get("phrase_offset", 0)

    def agreement(self, bar, kind):
        """(detector probability at the bar, sources on the exact bar as {rules, edmformer, dropdetector}, sources within one bar)."""
        votes = self.support.get(kind, {})
        prob = max((p for b, p in self.support.get("drop", {}).get("dropdetector", []) if b == bar), default=0.0)
        sure = lambda s, pts: [b for b, p in pts if s != "dropdetector" or p >= DROP_SCORE]
        exact = {s: bar in sure(s, pts) for s, pts in votes.items()}
        near = sum(any(abs(b - bar) <= 1 for b in sure(s, pts)) for s, pts in votes.items())
        return prob, exact, near

    def start(self, bar, kind):
        lv = self.lv
        prob, exact, near = self.agreement(bar, kind)
        return _finite([self.nov[max(1, bar - 1):bar + 2].max(initial=0.0) / self.top, float((bar - self.offset) % 8 == 0),
                        _change(lv["kick"], bar), _change(lv["bass"], bar), _change(lv["loud"], bar), self.share,
                        *(float(kind == k) for k in START_KINDS),
                        prob, *(float(exact.get(s, False)) for s in ("rules", "edmformer", "dropdetector")), near])

    def exists(self, drop):
        """Evidence for "a buildup exists before the drop at this bar": which sources have one within 24 bars before it, the levels over the
        8 bars before, and what came before them (the fused label there)."""
        lv, a = self.lv, max(0, drop - 8)
        near = lambda source, kind="buildup": float(any(drop - 24 <= b <= drop - 4 for b, _ in self.support.get(kind, {}).get(source, [])))
        rise = lambda key: float(np.polyfit(np.arange(drop - a), lv[key][a:drop], 1)[0] * (drop - a)) if key in lv and drop - a >= 3 else 0.0
        before = next((s["label"] for s in self.sections if s["start_bar"] <= max(0, drop - 9) < s["end_bar"]), "")
        em_drop = float(any(abs(b - drop) <= 8 for b, _ in self.support.get("drop", {}).get("edmformer", [])))
        return _finite([near("rules"), near("edmformer"), em_drop, lv["kick"][a:drop].min(initial=1.0), lv["bass"][a:drop].min(initial=1.0),
                        lv["loud"][drop:drop + 4].mean() - lv["loud"][max(0, drop - 4):drop].mean() if 0 < drop < self.n else 0.0,
                        rise("onset"), rise("centroid"), float(before == "drop"), float(before == "breakdown"), self.share])

    def label(self, a, b, label):
        lv, b = self.lv, min(b, self.n)
        return _finite([lv["loud"][a:b].mean() - self.mean_loud, max(lv["kick"][a:b].mean(), lv["bass"][a:b].mean()),
                        np.mean(lv["kick"][a:b] > 0.4), (b - a) / self.n, a / self.n, self.share,
                        *(float(label == l) for l in LABELS)])


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
                 for s in doc["sections"] if 0 < s["start_bar"] < ev.n and s["label"] != "buildup"]
    return rows


def _exact(record):
    """True, False or None (the reviewer couldn't tell) for one answer."""
    answer = record["answer"]
    if answer == "skip":
        return None
    if record["claim"]["kind"] == "buildup_exists":      # exact where the answer agrees with what was claimed
        return (answer == "yes") == record["claim"]["claimed"]
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


def buildup_exists(record):
    """True, False or None: whether a buildup exists before the drop an answer is about. An existence Probe says it with yes or none; of the
    older start Probes a named bar, "yes" (the marker is the start) or "earlier"/"later" mean one exists, "none" ("No buildup") that none does.
    "Not at the marker" and a skip say nothing."""
    claim, answer = record["claim"], record["answer"]
    if answer == "skip":
        return None
    if claim["kind"] == "buildup_exists":
        return answer == "yes"
    if claim["kind"] == "buildup" or (claim["kind"] == "missed" and "none" in record.get("alternatives", {})):
        if answer == "none":
            return False
        named = record.get("alternatives", {}).get(answer, {}).get("bar")
        return True if answer in ("yes", "earlier", "later") or answer.startswith("bar:") or named is not None else None
    return None


def drop_for(doc, record):
    """The start bar of the drop a buildup answer is about: the claim's own for an existence Probe or a missed one, else the first drop after the named or claimed bar."""
    claim = record["claim"]
    if claim["kind"] in ("buildup_exists", "missed"):
        bar = claim["bar"]
    else:
        bar = gold_bar(record) if gold_bar(record) is not None else claim["bar"]
    return next((s["start_bar"] for s in doc["sections"] if s["label"] == "drop" and s["start_bar"] > bar - (claim["kind"] in ("buildup_exists", "missed"))), None)


def gold_rows(answers, docs):
    """(group, features, exact) for every Gold answer (a Probe and its repeats count once) whose track is still analysed.
    Buildup answers train the existence group only (a row per answer whose drop is still there, whatever was claimed); they leave the exact-bar group
    alone, and so does a "missed" Claim."""
    from .rounds import gold_answers

    cache, rows = {}, []
    for record in gold_answers(answers).values():
        exact, doc, claim = _exact(record), docs.get(record["track"]), record["claim"]
        if doc is None:
            continue
        if claim["kind"] in ("buildup", "buildup_exists", "missed"):
            truth, drop = buildup_exists(record), drop_for(doc, record)
            if truth is not None and drop is not None:
                rows.append(("exists", cache.setdefault(record["track"], Evidence(doc)).exists(drop), truth))
            continue
        if exact is None:
            continue
        ev = cache.setdefault(record["track"], Evidence(doc))
        bar = int(np.argmin(np.abs(np.asarray(doc["bars"]["start"]) - claim["time"])))
        if claim["kind"] == "grid":
            rows.append(("grid", ev.grid, exact))
        elif claim["kind"] == "label" and claim["label"] not in LABELS:
            continue     # a Claim for a label the Analysis no longer makes (groove); "is:groove" on another label stays an answer
        elif claim["kind"] == "label":
            rows.append(("label", ev.label(bar, bar + claim["end_bar"] - claim["bar"], claim["label"]), exact))
        else:
            rows.append(("start", ev.start(bar, claim["kind"]), exact))
            named = gold_bar(record)
            if named is not None and named != bar and claim["kind"] == "drop":
                # the bar the reviewer named is exact by their word. Only for drops: other sources put drops there independently,
                # while a buildup or breakdown the reviewer names was found by no source, which would teach "unsupported is exact"
                rows.append(("start", ev.start(named, claim["kind"]), True))
    return rows


def _logistic(X, y, floor, C=1.0):
    """L2-penalised logistic regression (the objective of sklearn's LogisticRegression) with coefficients of the `floor` columns held at 0 or above."""
    from scipy.optimize import minimize

    k, sign = X.shape[1], np.where(y, 1.0, -1.0)

    def loss(p):
        z = X @ p[:k] + p[k]
        g = -C * sign / (1 + np.exp(sign * z))
        return C * np.sum(np.logaddexp(0, -sign * z)) + 0.5 * p[:k] @ p[:k], np.append(X.T @ g + p[:k], g.sum())

    bounds = [(0, None) if i in floor else (None, None) for i in range(k)] + [(None, None)]
    p = minimize(loss, np.zeros(k + 1), jac=True, bounds=bounds, method="L-BFGS-B").x
    return p[:k], float(p[k])


def fit_group(rows, names=()):
    X = np.asarray([x for x, _ in rows], float)
    y = np.asarray([bool(t) for _, t in rows])
    if len(set(y)) < 2:
        return None
    mean, std = X.mean(axis=0), X.std(axis=0) + 1e-9
    coef, intercept = _logistic((X - mean) / std, y, {i for i, n in enumerate(names) if n in AGREEMENT})
    return {"mean": mean.tolist(), "std": std.tolist(), "coef": coef.tolist(), "intercept": intercept}


def fit(silver, gold, today=None, min_gold=MIN_GOLD):
    """Model per group; Gold wins once a group has min_gold answers (MIN_GOLD)."""
    model = {"fitted": (today or date.today()).isoformat(), "groups": {}}
    for group, names in FEATURES.items():
        g = [(x, t) for k, x, t in gold if k == group]
        s = [(x, t) for k, x, t in silver if k == group]
        source, rows = ("gold", g) if len(g) >= min_gold else ("silver+gold", s + g) if g and group in GOLD_FIRST else ("silver", s)
        params = fit_group(rows, names) if rows else None
        if params:
            model["groups"][group] = {"features": list(names), "source": source, "n": len(rows), "gold": len(g), **params}
            if group == "start" and source == "gold":    # Gold alone says something only about the kinds of Claim it has enough rows for
                counts = Counter(_kind(x) for x, _ in rows)
                model["groups"][group]["kinds"] = sorted(k for k, n in counts.items() if n >= MIN_GOLD)
    return model


def predict(model, group, x):
    m = (model or {}).get("groups", {}).get(group)
    if m is None or m["features"] != list(FEATURES[group]):    # a model fitted on other evidence knows nothing of this
        return UNKNOWN
    if "kinds" in m and _kind(x) not in m["kinds"]:
        return UNKNOWN
    z = float(np.dot((np.asarray(x, float) - m["mean"]) / m["std"], m["coef"]) + m["intercept"])
    return int(round(100.0 / (1.0 + np.exp(-z))))


def with_confidence(doc, model):
    ev = Evidence(doc)
    sections = []
    for s in doc["sections"]:
        a, b = s["start_bar"], s["end_bar"]
        if s["label"] == "buildup":      # an existence Claim: a buildup exists before the drop it leads to (its start is nominal)
            conf = predict(model, "exists", ev.exists(b))
        else:
            conf = predict(model, "start", ev.start(a, s["label"])) if 0 < a < ev.n else 100
        sections.append({**s, "confidence": conf, "label_confidence": predict(model, "label", ev.label(a, b, s["label"])) if min(b, ev.n) > a else UNKNOWN})
    by_start = {s["start_bar"]: s["confidence"] for s in sections}
    events = [{**e, "confidence": by_start.get(e["bar"], UNKNOWN)} if e["kind"] in START_KINDS else e for e in doc["events"]]
    return {**doc, "grid": {**doc["grid"], "bar1_confidence": predict(model, "grid", ev.grid)},
            "sections": sections, "events": events}


def claims_of(docs):
    """(group, confidence) of every Claim that coverage counts: grid, labels, drop and breakdown starts, and buildup existence (not buildup starts)."""
    out = []
    for d in docs.values():
        out.append(("grid", d["grid"].get("bar1_confidence", UNKNOWN)))
        for s in d["sections"]:
            out.append(("label", s.get("label_confidence", UNKNOWN)))
            if s["label"] == "buildup":
                out.append(("exists", s.get("confidence", UNKNOWN)))
            elif s["start_bar"] > 0 and s["label"] in START_KINDS:
                out.append(("start", s.get("confidence", UNKNOWN)))
    return out


def calibration(model, rows, edges=(0, 50, 80, 95, 101)):
    """Per confidence band: claims, share exact. An honest model's bands match their numbers."""
    return bands([(predict(model, g, x), t) for g, x, t in rows], edges)


def load_model(out_dir):
    path = Path(out_dir) / MODEL_FILE
    return json.loads(path.read_text()) if path.exists() else None
