"""Turn the reviewer's ratings into ground truth and accuracy numbers (pure calculations)."""

from collections import Counter, defaultdict

GRID_MARKS = ("beat", "downbeat")  # reviewer taps that check the grid, not structure events
SECTION_EVENTS = ("drop", "buildup", "breakdown")  # these events are the start of their section


def _close(a, b):
    return abs(a - b) <= max(0.5, 0.005 * b)


def tempo_agreement(bpm, tag):
    if tag is None:
        return "untagged"
    if _close(bpm, tag):
        return "match"
    if _close(bpm, 2 * tag):
        return "double"
    if _close(bpm, tag / 2):
        return "half"
    return "mismatch"


def ground_truth_events(rating):
    """(kind, bar) the reviewer confirmed, corrected or added."""
    out = []
    for e in rating.get("events", []):
        if e.get("verdict") == "ok":
            out.append((e["kind"], e["bar"]))
        elif e.get("verdict") == "off":
            out.append((e["kind"], e["bar"] + int(e.get("offset_bars", 0))))
    out += [(m["kind"], m["bar"]) for m in rating.get("missed", []) if m["kind"] not in GRID_MARKS]
    return sorted(out, key=lambda x: (x[1], x[0]))


def _is_rated(r):
    return bool(r.get("grid", {}).get("beats") or r.get("overall")
                or any(e.get("verdict") for e in r.get("events", []))
                or any(s.get("verdict") for s in r.get("sections", [])))


def _share(values, good):
    values = [v for v in values if v]
    return sum(v == good for v in values) / len(values) if values else None


def summarize(ratings):
    rated = [r for r in ratings.values() if _is_rated(r)]
    counts = defaultdict(lambda: {"ok": 0, "off": 0, "wrong": 0, "missed": 0})
    confusion = Counter()
    label_ok = label_total = 0
    for r in rated:
        for e in r.get("events", []):
            if e.get("verdict") in ("ok", "off", "wrong"):
                counts[e["kind"]][e["verdict"]] += 1
        for m in r.get("missed", []):
            if m["kind"] not in GRID_MARKS:
                counts[m["kind"]]["missed"] += 1
        for s in r.get("sections", []):
            if s.get("verdict") == "ok":
                label_ok += 1
                label_total += 1
            elif s.get("verdict") == "wrong":
                label_total += 1
                confusion[f"{s['label']}->{s.get('correct_label', '?')}"] += 1
    events = {}
    for kind, c in counts.items():
        found = c["ok"] + c["off"] + c["wrong"]
        events[kind] = {**c,
                        "precision": c["ok"] / found if found else None,
                        "recall": c["ok"] / (c["ok"] + c["off"] + c["missed"]) if c["ok"] + c["off"] + c["missed"] else None}
    overall = [r["overall"] for r in rated if r.get("overall")]
    return {
        "tracks_rated": len(rated),
        "beats_ok": _share([r.get("grid", {}).get("beats") for r in rated], "ok"),
        "downbeat_ok": _share([r.get("grid", {}).get("downbeat") for r in rated], "ok"),
        "events": events,
        "section_label_accuracy": label_ok / label_total if label_total else None,
        "label_confusion": dict(confusion),
        "overall_mean": sum(overall) / len(overall) if overall else None,
    }


def _effective_sections(rating):
    rows = []
    for s in rating.get("sections", []):
        start = s.get("true_start_bar")
        rows.append({**s, "start": s["start_bar"] if start is None else start,
                     "judged": bool(s.get("verdict") or s.get("added") or (start is not None and start != s["start_bar"]))})
    rows.sort(key=lambda r: r["start"])
    for i, r in enumerate(rows):
        r["end"] = rows[i + 1]["start"] if i + 1 < len(rows) else r["end_bar"]
    return rows


def gold_from_rating(rating):
    """What the reviewer asserted, independent of the analyzer version that was rated.

    Structure events (drop, buildup, breakdown) are the starts of judged sections;
    a relabelled section also says its old label was not there.
    """
    events = {}

    def put(kind, bar, present, strong=False):
        if strong or (kind, bar) not in events:
            events[(kind, bar)] = present

    sections, boundaries = [], []
    for r in _effective_sections(rating):
        if not r["judged"]:
            continue
        label = r.get("correct_label") or r["label"]
        sections.append({"start_bar": r["start"], "end_bar": r["end"], "label": label})
        if r["start"] > 0:
            boundaries.append(r["start"])
        if label in SECTION_EVENTS:
            put(label, r["start"], True, strong=True)
        if r.get("correct_label") and r["label"] in SECTION_EVENTS and r["label"] != label:
            put(r["label"], r["start_bar"], False, strong=True)
    for e in rating.get("events", []):
        if e.get("verdict") == "ok":
            put(e["kind"], e["bar"], True)
        elif e.get("verdict") == "off":
            put(e["kind"], e["bar"] + int(e.get("offset_bars", 0)), True)
        elif e.get("verdict") == "wrong":
            put(e["kind"], e["bar"], False)
    for m in rating.get("missed", []):
        if m["kind"] not in GRID_MARKS:
            put(m["kind"], m["bar"], True)
    return {
        "sections": sections,
        "boundaries": sorted(set(boundaries)),
        "events": [{"kind": k, "bar": b, "present": p} for (k, b), p in sorted(events.items(), key=lambda x: (x[0][1], x[0][0]))],
        "grid": rating.get("grid", {}),
        "beat_marks": sorted(m["time"] for m in rating.get("missed", []) if m["kind"] in GRID_MARKS and "time" in m),
    }


def merge_gold(golds):
    """Combine gold from oldest to newest; a newer judgement replaces older ones it overlaps."""
    events, sections = {}, []
    for g in golds:
        for e in g.get("events", []):
            events[(e["kind"], e["bar"])] = e["present"]
        new = g.get("sections", [])
        keep = [s for s in sections if not any(s["start_bar"] < n["end_bar"] and n["start_bar"] < s["end_bar"] for n in new)]
        sections = sorted(keep + new, key=lambda s: s["start_bar"])
    return {"events": [{"kind": k, "bar": b, "present": p} for (k, b), p in sorted(events.items(), key=lambda x: (x[0][1], x[0][0]))],
            "sections": sections}


def prefill_rating(doc, gold):
    """A rating for new predictions that already holds what the reviewer said before."""
    starts = doc["bars"]["start"]
    bar_time = lambda k: starts[k] if k < len(starts) else starts[-1]
    gold_sections = gold.get("sections", [])
    pred_starts = {s["start_bar"] for s in doc["sections"]}

    def covering(bar):
        return next((g for g in gold_sections if g["start_bar"] <= bar < g["end_bar"]), None)

    sections = []
    for s in doc["sections"]:
        row = {"label": s["label"], "start_bar": s["start_bar"], "end_bar": s["end_bar"],
               "verdict": None, "correct_label": None, "true_start_bar": None}
        g = covering(s["start_bar"])
        if g is not None:
            same = g["label"] == s["label"]
            row["verdict"] = "ok" if same else "wrong"
            row["correct_label"] = None if same else g["label"]
        sections.append(row)
    for g in gold_sections:
        if g["start_bar"] not in pred_starts:
            sections.append({"label": g["label"], "start_bar": g["start_bar"], "end_bar": g["end_bar"], "verdict": "ok",
                             "correct_label": None, "true_start_bar": None, "added": True})
    sections.sort(key=lambda s: s["start_bar"])

    known = {(e["kind"], e["bar"]): e["present"] for e in gold.get("events", [])}
    events = []
    for e in doc["events"]:
        present = None if e["kind"] in SECTION_EVENTS else known.get((e["kind"], e["bar"]))
        events.append({"kind": e["kind"], "bar": e["bar"], "offset_bars": 0,
                       "verdict": None if present is None else "ok" if present else "wrong"})
    predicted = {(e["kind"], e["bar"]) for e in doc["events"]}
    missed = [{"kind": e["kind"], "bar": e["bar"], "time": bar_time(e["bar"])} for e in gold.get("events", [])
              if e["present"] and e["kind"] not in SECTION_EVENTS and (e["kind"], e["bar"]) not in predicted]
    return {"grid": dict(gold.get("grid", {})), "events": events, "sections": sections, "missed": missed,
            "overall": None, "notes": "", "prefilled": True}
