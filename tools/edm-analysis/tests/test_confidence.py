import json

import numpy as np
import pytest

from edm_analysis.confidence import (FEATURES, MIN_GOLD, MODEL_FILE, REVIEWER_ERROR, UNKNOWN, Evidence, _exact, bands, buildup_exists, claims_of, corrected_rate, fit,
                                     gold_bar, gold_rows, held_out, predict, silver_without_gold, with_confidence)
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


def test_the_labels_are_the_five_the_reviewer_uses():
    from edm_analysis.confidence import LABELS

    assert LABELS == ("intro", "buildup", "drop", "breakdown", "outro")
    assert [f for f in FEATURES["label"] if f.startswith("is_")] == [f"is_{l}" for l in LABELS]


@pytest.mark.parametrize("label,answer,rows", [
    ("drop", "is:groove", [False]),      # an old answer: not a drop, a steady beat part
    ("drop", "yes", [True]),
    ("groove", "yes", []),               # a Claim for a label the Analysis no longer makes trains nothing
    ("groove", "no", []),
])
def test_old_groove_answers_stay_gold_but_groove_claims_train_nothing(label, answer, rows):
    record = {"answer": answer, "round": 1, "track": "abababababababab", "claim": {"kind": "label", "label": label, "bar": 16, "end_bar": 32, "time": 32.0}}
    assert [e for _, _, e in gold_rows({"r01-p001": record}, {"abababababababab": DOC})] == rows


# --- a buildup is an existence Claim ---------------------------------------------------------------

BU = {"kind": "buildup", "bar": 14, "time": 28.0}
EXISTS = {"kind": "buildup_exists", "bar": 16, "time": 32.0}
NONE_ALT = {"none": {"source": "ours"}}


@pytest.mark.parametrize("claim,answer,alternatives,expected", [
    ({**EXISTS, "claimed": True}, "yes", None, True), ({**EXISTS, "claimed": True}, "none", None, False), ({**EXISTS, "claimed": False}, "yes", None, True),
    ({**EXISTS, "claimed": True}, "skip", None, None),
    (BU, "yes", None, True), (BU, "bar:8", None, True), (BU, "earlier", None, True), (BU, "none", {"ours": {"bar": 14}}, False), (BU, "no", None, None),
    (BU, "skip", None, None),
    ({"kind": "missed", "bar": 16, "time": 32.0}, "bar:8", NONE_ALT, True), ({"kind": "missed", "bar": 16, "time": 32.0}, "none", NONE_ALT, False),
    ({"kind": "missed", "bar": 16, "time": 32.0}, "yes", None, None),            # a new part at a cue says nothing about buildups
    ({"kind": "drop", "bar": 16, "time": 32.0}, "yes", None, None), ({"kind": "label", "label": "buildup", "bar": 8, "end_bar": 16, "time": 16.0}, "yes", None, None),
])
def test_whether_a_buildup_exists_before_a_drop_is_read_from_the_old_and_the_new_answers(claim, answer, alternatives, expected):
    assert buildup_exists({"claim": claim, "answer": answer, **({"alternatives": alternatives} if alternatives else {})}) is expected


@pytest.mark.parametrize("claimed,answer,exact", [(True, "yes", True), (True, "none", False), (False, "none", True), (False, "yes", False), (True, "skip", None)])
def test_an_existence_answer_is_exact_where_it_agrees_with_what_was_claimed(claimed, answer, exact):
    record = {"claim": {**EXISTS, "claimed": claimed}, "answer": answer}
    assert _exact(record) is exact and gold_bar(record) is None


@pytest.mark.parametrize("claim,answer,alternatives,rows", [
    ({**EXISTS, "claimed": True}, "yes", None, [True]),
    ({**EXISTS, "claimed": True}, "none", None, [False]),
    ({**EXISTS, "claimed": False}, "yes", None, [True]),               # the row says whether a buildup exists, whatever was claimed
    (BU, "bar:8", None, [True]),                                        # an old named start: a buildup exists before the drop at 16
    (BU, "none", {"ours": {"bar": 14}}, [False]),
    ({"kind": "missed", "bar": 16, "time": 32.0}, "bar:8", NONE_ALT, [True]),
    (BU, "no", None, []),                                               # "not at the marker" says nothing about existence, and no start row either
    (EXISTS, "skip", None, []),
])
def test_buildup_answers_train_the_existence_group_and_nothing_else(claim, answer, alternatives, rows):
    record = {"claim": claim, "answer": answer, "round": 1, "track": "abababababababab", **({"alternatives": alternatives} if alternatives else {})}
    out = gold_rows({"r01-p001": record}, {"abababababababab": DOC})
    assert [(g, len(x), e) for g, x, e in out] == [("exists", len(FEATURES["exists"]), e) for e in rows]


def test_an_answer_about_a_drop_that_is_gone_trains_nothing():
    record = {"claim": {**EXISTS, "bar": 40, "claimed": True}, "answer": "yes", "round": 1, "track": "abababababababab"}
    assert gold_rows({"r01-p001": record}, {"abababababababab": DOC}) == []


SUPPORT_EXISTS = {"buildup": {"rules": [[10, None]], "edmformer": [[6, None]]}, "drop": {"edmformer": [[16, None]], "dropdetector": [[16, 0.9]]}}


@pytest.mark.parametrize("support,expected", [
    (SUPPORT_EXISTS, {"rules_bu": 1.0, "em_bu": 1.0, "em_drop": 1.0}),
    ({"buildup": {"rules": [[10, None]]}, "drop": {}}, {"rules_bu": 1.0, "em_bu": 0.0, "em_drop": 0.0}),
    ({"buildup": {"rules": [[14, None]]}, "drop": {}}, {"rules_bu": 0.0, "em_bu": 0.0, "em_drop": 0.0}),        # less than 4 bars before the drop is no buildup
    ({}, {"rules_bu": 0.0, "em_bu": 0.0, "em_drop": 0.0}),
])
def test_the_existence_evidence_says_which_sources_have_a_buildup_before_the_drop(support, expected):
    got = dict(zip(FEATURES["exists"], Evidence({**DOC, "support": support}).exists(16)))
    assert {k: got[k] for k in expected} == expected


def test_the_existence_evidence_reads_the_levels_before_the_drop():
    doc = {**DOC, "bars": {**DOC["bars"], "descriptors": {"onset": [0.0] * 8 + [i / 8 for i in range(8)] + [0.5] * 16, "centroid": [0.5] * N}}}
    got = dict(zip(FEATURES["exists"], Evidence(doc).exists(16)))
    assert got["kick_min"] == 0.0 and got["bass_min"] == 0.0 and got["loud_step"] == pytest.approx(0.6) and got["rise_onset"] > 0.5 and got["rise_centroid"] == 0.0


def test_existence_has_no_silver_so_it_stays_unknown_until_there_are_thirty_gold_answers():
    rng = np.random.default_rng(12)
    assert "exists" not in fit([], rows("exists", MIN_GOLD - 1, rng))["groups"]
    model = fit([], rows("exists", MIN_GOLD, rng))
    assert model["groups"]["exists"]["source"] == "gold" and model["groups"]["exists"]["n"] == MIN_GOLD


@pytest.mark.parametrize("name", ["rules_bu", "em_bu", "em_drop"])
def test_more_sources_with_a_buildup_never_lowers_its_existence_confidence(name):
    rng = np.random.default_rng(13)
    i = FEATURES["exists"].index(name)
    data = [(g, [*x[:i], 1.0 - float(e), *x[i + 1:]], e) for g, x, e in rows("exists", 60, rng)]
    assert fit([], data)["groups"]["exists"]["coef"][i] >= 0


BUILDUP_DOC = {**DOC, "sections": [{"label": "intro", "start_bar": 0, "end_bar": 8, "start": 0.0, "end": 16.0},
                                   {"label": "buildup", "start_bar": 8, "end_bar": 16, "start": 16.0, "end": 32.0, "nominal": True},
                                   {"label": "drop", "start_bar": 16, "end_bar": 32, "start": 32.0, "end": 64.0}],
               "events": [{"kind": "buildup", "bar": 8, "time": 16.0}, {"kind": "drop", "bar": 16, "time": 32.0}]}


def test_a_buildup_gets_its_existence_confidence_and_its_start_is_no_exact_bar_claim():
    rng = np.random.default_rng(14)
    exists_only = fit([], rows("exists", 40, rng))
    got = with_confidence(BUILDUP_DOC, exists_only)
    buildup, drop = got["sections"][1], got["sections"][2]
    assert buildup["confidence"] == predict(exists_only, "exists", Evidence(BUILDUP_DOC).exists(16))
    assert drop["confidence"] == UNKNOWN and got["events"][0]["confidence"] == buildup["confidence"]
    start_only = fit(rows("start", 50, rng), [])
    assert with_confidence(BUILDUP_DOC, start_only)["sections"][1]["confidence"] == UNKNOWN      # the start model has nothing to say about a buildup


def test_the_claims_that_coverage_counts_are_grid_labels_drop_and_breakdown_starts_and_buildup_existence():
    model = {"fitted": "x", "groups": {}}
    claims = claims_of({"a": with_confidence(BUILDUP_DOC, model)})
    assert sorted(g for g, _ in claims) == ["exists", "grid", "label", "label", "label", "start"]
    assert all(c == UNKNOWN for _, c in claims)


@pytest.mark.parametrize("n_rows,fitted", [(MIN_GOLD, True), (MIN_GOLD - 1, False)])
def test_held_out_follows_how_the_full_model_is_fitted(n_rows, fitted):
    """A group with exactly MIN_GOLD rows is fitted on Gold; leaving a track out must not turn it into 'no model' and a column of 50s."""
    rng = np.random.default_rng(15)
    gold = {f"t{i}": [r] for i, r in enumerate(rows("label", n_rows, rng))}
    pairs = held_out({}, gold, "label")
    assert len(pairs) == n_rows
    assert (any(p != UNKNOWN for p, _ in pairs)) is fitted
