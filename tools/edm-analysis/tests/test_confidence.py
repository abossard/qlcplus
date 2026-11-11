import json

import numpy as np
import pytest

from edm_analysis.confidence import FEATURES, MIN_GOLD, MODEL_FILE, UNKNOWN, fit, gold_bar, gold_rows, predict, with_confidence
from edm_analysis.final import write_final

N = 32
DOC = {"track": {"path": "/m/a.mp3", "title": "a", "sha256": "ab" * 32}, "analyzer": {"version": "2.5"},
       "grid": {"bpm": 120.0, "period": 0.5, "first_downbeat": 0.0, "confidence": 0.9},
       "bars": {"start": [2.0 * k for k in range(N)],
                "levels": {"kick": [0.0] * 16 + [1.0] * 16, "bass": [0.0] * 16 + [0.9] * 16, "hihat": [0.5] * N,
                           "instruments": [0.5] * N, "vocal": [0.0] * N, "loud": [0.3] * 16 + [0.9] * 16}},
       "raw": {"downbeats": [2.0 * k for k in range(N)]},
       "sections": [{"label": "intro", "start_bar": 0, "end_bar": 16, "start": 0.0, "end": 32.0},
                    {"label": "drop", "start_bar": 16, "end_bar": 32, "start": 32.0, "end": 64.0}],
       "events": [{"kind": "drop", "bar": 16, "time": 32.0}, {"kind": "vocal_in", "bar": 20, "time": 40.0}]}


def rows(group, n, rng):
    """Separable rows: the first feature decides whether a claim is exact."""
    width = len(FEATURES[group])
    out = []
    for i in range(n):
        exact = i % 2 == 0
        x = rng.normal(0, 0.1, width)
        x[0] += 1.0 if exact else -1.0
        out.append((group, x.tolist(), exact))
    return out


def test_confidence_follows_the_evidence():
    rng = np.random.default_rng(0)
    model = fit(rows("start", 200, rng), [])
    high = predict(model, "start", [1.0] + [0.0] * (len(FEATURES["start"]) - 1))
    low = predict(model, "start", [-1.0] + [0.0] * (len(FEATURES["start"]) - 1))
    assert 90 <= high <= 100 and 0 <= low <= 10
    assert model["groups"]["start"]["source"] == "silver"


@pytest.mark.parametrize("n_gold,source", [(MIN_GOLD - 1, "silver"), (MIN_GOLD, "gold")])
def test_gold_replaces_silver_once_there_is_enough(n_gold, source):
    rng = np.random.default_rng(1)
    model = fit(rows("start", 100, rng), rows("start", n_gold, rng))
    assert model["groups"]["start"]["source"] == source


@pytest.mark.parametrize("group", ["label", "grid"])
def test_a_group_without_judgements_is_unknown(group):
    model = fit(rows("start", 50, np.random.default_rng(2)), [])
    assert predict(model, group, [0.0] * len(FEATURES[group])) == UNKNOWN


def test_every_claim_gets_a_percentage():
    doc = with_confidence(DOC, fit(rows("start", 50, np.random.default_rng(3)), []))
    assert doc["sections"][0]["confidence"] == 100          # the track start is certain
    assert 0 <= doc["sections"][1]["confidence"] <= 100
    assert [s["label_confidence"] for s in doc["sections"]] == [UNKNOWN, UNKNOWN]
    assert doc["events"][0]["confidence"] == doc["sections"][1]["confidence"]
    assert "confidence" not in doc["events"][1]            # vocals are not a Claim
    assert doc["grid"]["bar1_confidence"] == UNKNOWN


def test_the_final_file_carries_the_confidence(tmp_path):
    (tmp_path / MODEL_FILE).write_text(json.dumps(fit(rows("start", 50, np.random.default_rng(4)), [])))
    final = json.loads(write_final(tmp_path, DOC).read_text())
    assert all(isinstance(s["confidence"], int) for s in final["sections"])
    assert final["events"][0]["confidence"] == final["sections"][1]["confidence"]
    assert final["grid"]["confidence"] == UNKNOWN


LOCATE = {"ours": {"bar": 16}, "edmformer": {"bar": 18}}


@pytest.mark.parametrize("record,group,exact", [
    ({"answer": "yes", "claim": {"kind": "drop", "bar": 16, "time": 32.0}}, "start", True),
    ({"answer": "later", "claim": {"kind": "drop", "bar": 16, "time": 32.0}}, "start", False),
    ({"answer": "no", "claim": {"kind": "grid", "bar": 0, "time": 0.0}}, "grid", False),
    ({"answer": "yes", "claim": {"kind": "label", "label": "drop", "bar": 16, "end_bar": 32, "time": 32.0}}, "label", True),
    ({"answer": "b", "claim": {"kind": "drop", "bar": 16, "time": 32.0},
      "alternatives": {"a": {"source": "ours"}, "b": {"source": "vdj"}}}, "start", False),
    ({"answer": "a", "claim": {"kind": "drop", "bar": 16, "time": 32.0},
      "alternatives": {"a": {"source": "ours"}, "b": {"source": "vdj"}}}, "start", True),
    ({"answer": "bar:18", "claim": {"kind": "drop", "bar": 16, "time": 32.0}}, "start", False),
    ({"answer": "is:groove", "claim": {"kind": "label", "label": "drop", "bar": 16, "end_bar": 32, "time": 32.0}}, "label", False),
    ({"answer": "bar:16", "claim": {"kind": "drop", "bar": 16, "time": 32.0}, "alternatives": LOCATE}, "start", True),
    ({"answer": "bar:18", "claim": {"kind": "drop", "bar": 16, "time": 32.0}, "alternatives": LOCATE}, "start", False),
    ({"answer": "none", "claim": {"kind": "drop", "bar": 16, "time": 32.0}, "alternatives": LOCATE}, "start", False),
])
def test_gold_rows_from_answers(record, group, exact):
    out = gold_rows({"r01-p001": {**record, "track": "abababababababab"}}, {"abababababababab": DOC})
    assert [(g, len(x), e) for g, x, e in out] == [(group, len(FEATURES[group]), exact)]


@pytest.mark.parametrize("record", [{"answer": "skip", "track": "abababababababab"}, {"answer": "yes", "track": "cdcdcdcdcdcdcdcd"}])
def test_answers_without_a_judgement_or_track_are_left_out(record):
    record = {**record, "claim": {"kind": "drop", "bar": 16, "time": 32.0}}
    assert gold_rows({"r01-p001": record}, {"abababababababab": DOC}) == []


DROP = {"kind": "drop", "bar": 16, "time": 32.0}
AB = {"a": {"source": "ours", "bar": 16}, "b": {"source": "vdj", "bar": 18}}


@pytest.mark.parametrize("answer,claim,alternatives,bar", [
    ("yes", DROP, None, 16),
    ("yes", {**DROP, "kind": "buildup", "bar": 12}, None, 12),
    ("yes", {**DROP, "kind": "missed", "bar": 20}, None, 20),
    ("yes", {**DROP, "bar": 20, "followup_of": "r01-p001"}, None, 20),
    ("bar:18", DROP, None, 18),
    ("bar:0", DROP, None, 0),
    ("bar:9", DROP, LOCATE, 9),
    ("a", DROP, AB, 16),
    ("b", DROP, AB, 18),
    ("neither", DROP, AB, None),
    ("b", {"kind": "grid", "bar": 0, "time": 32.0}, {"a": {"source": "ours", "shift_beats": 0}, "b": {"source": "vdj", "shift_beats": 2}}, None),
    ("none", DROP, LOCATE, None),
    ("skip", DROP, LOCATE, None),
    ("skip", DROP, None, None),
    ("no", DROP, None, None),
    ("earlier", DROP, None, None),
    ("later", DROP, None, None),
    ("yes", {"kind": "grid", "bar": 0, "time": 32.0}, None, None),
    ("yes", {"kind": "label", "label": "drop", "bar": 16, "end_bar": 32, "time": 32.0}, None, None),
    ("is:groove", {"kind": "label", "label": "drop", "bar": 16, "end_bar": 32, "time": 32.0}, None, None),
])
def test_the_gold_bar_is_known_only_where_the_reviewer_named_or_confirmed_one(answer, claim, alternatives, bar):
    record = {"answer": answer, "claim": claim, **({"alternatives": alternatives} if alternatives else {})}
    assert gold_bar(record) == bar
