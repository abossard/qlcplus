import json

import numpy as np
import pytest

from edm_analysis.confidence import (FEATURES, MIN_GOLD, MODEL_FILE, REVIEWER_ERROR, UNKNOWN, Evidence, bands, corrected_rate, fit, gold_bar, gold_rows,
                                     held_out, predict, silver_without_gold, with_confidence)
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


@pytest.mark.parametrize("record,group,exact", [   # exact: one entry per row; a named bar other than the Claim's adds a row that is exact
    ({"answer": "yes", "claim": {"kind": "drop", "bar": 16, "time": 32.0}}, "start", [True]),
    ({"answer": "later", "claim": {"kind": "drop", "bar": 16, "time": 32.0}}, "start", [False]),
    ({"answer": "no", "claim": {"kind": "grid", "bar": 0, "time": 0.0}}, "grid", [False]),
    ({"answer": "yes", "claim": {"kind": "label", "label": "drop", "bar": 16, "end_bar": 32, "time": 32.0}}, "label", [True]),
    ({"answer": "b", "claim": {"kind": "drop", "bar": 16, "time": 32.0},
      "alternatives": {"a": {"source": "ours"}, "b": {"source": "vdj"}}}, "start", [False]),
    ({"answer": "a", "claim": {"kind": "drop", "bar": 16, "time": 32.0},
      "alternatives": {"a": {"source": "ours"}, "b": {"source": "vdj"}}}, "start", [True]),
    ({"answer": "bar:18", "claim": {"kind": "drop", "bar": 16, "time": 32.0}}, "start", [False, True]),
    ({"answer": "is:groove", "claim": {"kind": "label", "label": "drop", "bar": 16, "end_bar": 32, "time": 32.0}}, "label", [False]),
    ({"answer": "bar:16", "claim": {"kind": "drop", "bar": 16, "time": 32.0}, "alternatives": LOCATE}, "start", [True]),
    ({"answer": "bar:18", "claim": {"kind": "drop", "bar": 16, "time": 32.0}, "alternatives": LOCATE}, "start", [False, True]),
    ({"answer": "none", "claim": {"kind": "drop", "bar": 16, "time": 32.0}, "alternatives": LOCATE}, "start", [False]),
])
def test_gold_rows_from_answers(record, group, exact):
    out = gold_rows({"r01-p001": {**record, "track": "abababababababab", "round": 1}}, {"abababababababab": DOC})
    assert [(g, len(x), e) for g, x, e in out] == [(group, len(FEATURES[group]), e) for e in exact]


@pytest.mark.parametrize("record", [{"answer": "skip", "track": "abababababababab"}, {"answer": "yes", "track": "cdcdcdcdcdcdcdcd"}])
def test_answers_without_a_judgement_or_track_are_left_out(record):
    record = {**record, "claim": {"kind": "drop", "bar": 16, "time": 32.0}}
    assert gold_rows({"r01-p001": {**record, "round": 1}}, {"abababababababab": DOC}) == []


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


SUPPORT = {"drop": {"rules": [[16, None]], "edmformer": [[17, None]], "dropdetector": [[16, 0.9], [40, 0.3]]},
           "buildup": {"rules": [[12, None]]}, "breakdown": {"rules": []}, "phrase_offset": 3}


def evidence(bar, kind, support=SUPPORT):
    return dict(zip(FEATURES["start"], Evidence({**DOC, "support": support}).start(bar, kind)))


@pytest.mark.parametrize("bar,kind,expected", [
    (16, "drop", {"det_prob": 0.9, "rules_exact": 1, "em_exact": 0, "dd_exact": 1, "near": 3}),
    (17, "drop", {"det_prob": 0.0, "rules_exact": 0, "em_exact": 1, "dd_exact": 0, "near": 3}),    # within one bar of the rules and the detector
    (40, "drop", {"det_prob": 0.3, "rules_exact": 0, "em_exact": 0, "dd_exact": 0, "near": 0}),    # the detector is not sure enough to count
    (12, "buildup", {"det_prob": 0.0, "rules_exact": 1, "em_exact": 0, "dd_exact": 0, "near": 1}),
    (20, "groove", {"det_prob": 0.0, "rules_exact": 0, "em_exact": 0, "dd_exact": 0, "near": 0}),
])
def test_the_start_evidence_says_which_sources_agree_on_the_bar(bar, kind, expected):
    got = evidence(bar, kind)
    assert {k: got[k] for k in expected} == pytest.approx(expected)


@pytest.mark.parametrize("bar,on_line", [(19, 1.0), (27, 1.0), (16, 0.0), (20, 0.0)])
def test_the_phrase_line_is_the_tracks_own(bar, on_line):
    assert evidence(bar, "groove")["phrase_line"] == on_line


def test_a_document_without_support_has_no_agreement():
    got = dict(zip(FEATURES["start"], Evidence(DOC).start(16, "drop")))
    assert (got["det_prob"], got["rules_exact"], got["near"], got["phrase_line"]) == (0.0, 0.0, 0.0, 1.0)


def test_a_model_fitted_on_other_evidence_says_unknown():
    model = fit(rows("start", 50, np.random.default_rng(5)), [])
    model["groups"]["start"]["features"] = ["something", "else"]
    assert predict(model, "start", [0.0] * len(FEATURES["start"])) == UNKNOWN


@pytest.mark.parametrize("group,n_gold,source,n", [
    ("grid", 3, "silver+gold", 103), ("grid", MIN_GOLD, "gold", MIN_GOLD), ("start", 3, "silver", 100)])
def test_a_few_grid_answers_join_the_silver_rows_until_there_are_enough_to_stand_alone(group, n_gold, source, n):
    rng = np.random.default_rng(6)
    model = fit(rows(group, 100, rng), rows(group, n_gold, rng))
    assert (model["groups"][group]["source"], model["groups"][group]["n"], model["groups"][group]["gold"]) == (source, n, n_gold)


def test_silver_rows_of_a_judged_grid_are_replaced_by_the_judgement():
    silver = {"a": [("grid", [1.0, 1.0], False), ("start", [0.0], True)], "b": [("grid", [0.0, 0.0], True)]}
    gold = {"a": [("grid", [1.0, 1.0], True)], "c": [("start", [0.0], True)]}
    assert silver_without_gold(silver, gold) == {"a": [("start", [0.0], True)], "b": [("grid", [0.0, 0.0], True)]}


@pytest.mark.parametrize("observed,corrected", [(0.9, 1.0), (0.82, 0.9), (0.5, 0.5), (0.1, 0.0), (1.0, 1.0), (0.0, 0.0), (0.05, 0.0)])
def test_an_observed_share_is_read_under_the_reviewers_error_rate(observed, corrected):
    assert REVIEWER_ERROR == 0.10
    assert corrected_rate(observed) == pytest.approx(corrected)


@pytest.mark.parametrize("error,observed,corrected", [(0.0, 0.7, 0.7), (0.25, 0.625, 0.75)])
def test_the_error_rate_can_be_given(error, observed, corrected):
    assert corrected_rate(observed, error) == pytest.approx(corrected)


def test_bands_report_each_confidence_band_with_its_observed_and_corrected_share():
    pairs = [(10, False), (20, True), (60, True), (60, True), (90, False), (96, True), (99, True)]
    out = bands(pairs)
    assert [(b["band"], b["n"], b["exact"]) for b in out] == [("0-50", 2, 0.5), ("50-80", 2, 1.0), ("80-95", 1, 0.0), ("95-100", 2, 1.0)]
    assert [b["corrected"] for b in out] == [pytest.approx(0.5), 1.0, 0.0, 1.0]
    assert bands([])[0] == {"band": "0-50", "n": 0, "exact": None, "corrected": None}


def test_held_out_confidence_is_judged_on_tracks_the_model_never_saw():
    rng = np.random.default_rng(7)
    gold = {f"t{i}": rows("start", 2, rng) for i in range(40)}
    pairs = held_out({}, gold, "start")
    assert len(pairs) == 80 and all(0 <= p <= 100 for p, _ in pairs)
    # separable rows: held-out predictions still follow the truth
    assert np.mean([(p >= 50) == t for p, t in pairs]) > 0.9


@pytest.mark.parametrize("claim", ["start", "label"])
def test_a_track_with_missing_levels_still_gets_finite_evidence(claim):
    nan = {**DOC, "bars": {**DOC["bars"], "levels": {**DOC["bars"]["levels"], "kick": [float("nan")] * N, "loud": [float("nan")] * 8 + [0.5] * (N - 8)}}}
    ev = Evidence(nan)
    x = ev.start(16, "drop") if claim == "start" else ev.label(16, 32, "drop")
    assert np.all(np.isfinite(x))


def test_a_bar_the_reviewer_names_for_a_buildup_is_no_example_of_an_exact_claim():
    record = {"answer": "bar:12", "track": "abababababababab", "claim": {"kind": "buildup", "bar": 14, "time": 28.0}}
    assert [e for _, _, e in gold_rows({"r01-p001": {**record, "round": 1}}, {"abababababababab": DOC})] == [False]


@pytest.mark.parametrize("name", ["det_prob", "rules_exact", "em_exact", "dd_exact", "near"])
def test_more_agreement_between_sources_never_lowers_confidence(name):
    """Gold is picked where sources disagree, so exact claims can look like the ones with less agreement: the model must not learn that."""
    rng = np.random.default_rng(8)
    data = rows("start", 80, rng)
    i = FEATURES["start"].index(name)
    data = [(g, [*x[:i], 1.0 - float(e), *x[i + 1:]], e) for g, x, e in data]     # agreement only where the claim is wrong
    model = fit(data, [])
    assert model["groups"]["start"]["coef"][i] >= 0


def test_the_constrained_fit_still_follows_the_other_evidence():
    model = fit(rows("start", 200, np.random.default_rng(9)), [])
    zero = [0.0] * len(FEATURES["start"])
    assert predict(model, "start", [1.0, *zero[1:]]) > 90 > 10 > predict(model, "start", [-1.0, *zero[1:]])


@pytest.mark.parametrize("answers,exacts", [
    (["bar:16"], [True]), (["bar:16", "bar:16"], [True]), (["bar:16", "bar:18"], []), (["bar:16", "bar:18", "bar:18"], [False, True])])
def test_gold_rows_count_a_probe_and_its_repeats_as_one_answer(answers, exacts):
    claim = {"kind": "drop", "bar": 16, "time": 32.0}
    records = {f"r0{i}-p001": {"answer": a, "round": i, "track": "abababababababab", "claim": claim, **({"repeat_of": "r01-p001"} if i > 1 else {})}
               for i, a in enumerate(answers, 1)}
    assert [e for _, _, e in gold_rows(records, {"abababababababab": DOC})] == exacts


def test_a_buildup_the_reviewer_locates_where_we_had_none_is_gold_for_the_tables_but_no_training_row():
    record = {"answer": "bar:8", "track": "abababababababab", "claim": {"kind": "missed", "bar": 16, "time": 32.0},
              "alternatives": {"none": {"source": "ours"}}}
    assert gold_rows({"r01-p001": {**record, "round": 1}}, {"abababababababab": DOC}) == []
    assert gold_bar(record) == 8


def _start_rows(kind_counts, rng):
    """Separable start rows of each kind ("other": none of the kind flags): the first feature decides whether a claim is exact."""
    flags = {k: FEATURES["start"].index(f"is_{k}") for k in ("drop", "buildup", "breakdown")}
    out = []
    for kind, n in kind_counts.items():
        for i in range(n):
            x = rng.normal(0, 0.1, len(FEATURES["start"]))
            x[0] += 1.0 if i % 2 == 0 else -1.0
            if kind != "other":
                x[flags[kind]] = 1.0
            out.append(("start", x.tolist(), i % 2 == 0))
    return out


@pytest.mark.parametrize("kind,sure", [("drop", True), ("buildup", False), ("breakdown", False), ("other", False)])
def test_a_claim_kind_with_too_little_gold_stays_unknown_when_gold_alone_fits_the_group(kind, sure):
    model = fit([], _start_rows({"drop": MIN_GOLD + 10, "buildup": MIN_GOLD - 1, "breakdown": 3}, np.random.default_rng(10)))
    flags = {k: FEATURES["start"].index(f"is_{k}") for k in ("drop", "buildup", "breakdown")}
    x = [0.0] * len(FEATURES["start"])
    x[0] = 1.0
    if kind != "other":
        x[flags[kind]] = 1.0
    assert model["groups"]["start"]["kinds"] == ["drop"]
    assert (predict(model, "start", x) > 80) is sure and (sure or predict(model, "start", x) == UNKNOWN)


def test_silver_rows_cover_every_kind_of_start():
    model = fit(_start_rows({"drop": 40, "buildup": 40, "other": 40}, np.random.default_rng(11)), [])
    assert "kinds" not in model["groups"]["start"]
