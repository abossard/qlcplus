import pytest

from edm_analysis.ratings import ground_truth_events, summarize, tempo_agreement


def rating(events=(), missed=(), sections=(), beats="ok", downbeat="ok", overall=None):
    return {"grid": {"beats": beats, "downbeat": downbeat},
            "events": list(events), "missed": list(missed), "sections": list(sections), "overall": overall}


@pytest.mark.parametrize("events,missed,expected", [
    ([{"kind": "drop", "bar": 24, "verdict": "ok"}], [], [("drop", 24)]),
    ([{"kind": "drop", "bar": 24, "verdict": "off", "offset_bars": -4}], [], [("drop", 20)]),
    ([{"kind": "drop", "bar": 24, "verdict": "wrong"}], [{"kind": "drop", "bar": 56}], [("drop", 56)]),
    ([{"kind": "buildup", "bar": 16, "verdict": None}], [], []),
    ([], [{"kind": "beat", "bar": 3, "time": 7.01}, {"kind": "scene", "bar": 40, "time": 80.2}], [("scene", 40)]),
])
def test_ground_truth_events_applies_verdicts(events, missed, expected):
    assert ground_truth_events(rating(events, missed)) == expected


def test_summarize_counts_precision_recall_and_label_accuracy():
    ratings = {
        "a": rating(
            events=[{"kind": "drop", "bar": 24, "verdict": "ok"}, {"kind": "drop", "bar": 80, "verdict": "wrong"},
                    {"kind": "buildup", "bar": 16, "verdict": "off", "offset_bars": 8}],
            missed=[{"kind": "drop", "bar": 120}, {"kind": "beat", "bar": 2, "time": 4.0}, {"kind": "downbeat", "bar": 0, "time": 0.5}],
            sections=[{"label": "drop", "verdict": "ok"}, {"label": "intro", "verdict": "wrong", "correct_label": "breakdown"}],
            overall=4),
        "b": rating(beats="drift", downbeat="wrong",
                    events=[{"kind": "drop", "bar": 32, "verdict": "ok"}], overall=2),
        "c": {"events": [], "missed": [], "sections": []},  # opened but nothing rated
    }

    s = summarize(ratings)

    assert s["tracks_rated"] == 2
    assert s["beats_ok"] == pytest.approx(0.5)
    assert s["downbeat_ok"] == pytest.approx(0.5)
    assert s["events"]["drop"] == {"ok": 2, "off": 0, "wrong": 1, "missed": 1, "precision": pytest.approx(2 / 3), "recall": pytest.approx(2 / 3)}
    assert s["events"]["buildup"]["off"] == 1
    assert "beat" not in s["events"] and "downbeat" not in s["events"]
    assert s["section_label_accuracy"] == pytest.approx(0.5)
    assert s["label_confusion"] == {"intro->breakdown": 1}
    assert s["overall_mean"] == pytest.approx(3.0)


@pytest.mark.parametrize("bpm,tag,expected", [
    (128.0, 128.0, "match"),
    (127.96, 128.0, "match"),
    (173.6, 87.0, "double"),
    (64.0, 128.0, "half"),
    (125.0, 128.0, "mismatch"),
    (128.0, None, "untagged"),
])
def test_tempo_agreement(bpm, tag, expected):
    assert tempo_agreement(bpm, tag) == expected


from edm_analysis.ratings import gold_from_rating, merge_gold

RATED = {
    "grid": {"beats": "ok", "downbeat": "wrong", "shift_beats": 1, "offset_ms": 0},
    "events": [{"kind": "vocal_in", "bar": 30, "verdict": "ok", "offset_bars": 0},
               {"kind": "vocal_out", "bar": 50, "verdict": "wrong", "offset_bars": 0},
               {"kind": "drop", "bar": 70, "verdict": "ok", "offset_bars": 0}],
    "missed": [{"kind": "beat", "bar": 2, "time": 4.01}, {"kind": "scene", "bar": 44, "time": 82.5}],
    "sections": [
        {"label": "intro", "start_bar": 0, "end_bar": 16, "verdict": "ok", "correct_label": None, "true_start_bar": None},
        {"label": "buildup", "start_bar": 16, "end_bar": 24, "verdict": None, "correct_label": None, "true_start_bar": 20},
        {"label": "drop", "start_bar": 24, "end_bar": 40, "verdict": "ok", "correct_label": None, "true_start_bar": None},
        {"label": "drop", "start_bar": 40, "end_bar": 56, "verdict": "wrong", "correct_label": "breakdown", "true_start_bar": None},
        {"label": "buildup", "start_bar": 48, "end_bar": 56, "verdict": "ok", "correct_label": None, "true_start_bar": None, "added": True},
        {"label": "outro", "start_bar": 56, "end_bar": 64, "verdict": None, "correct_label": None, "true_start_bar": None},
    ],
    "overall": 3,
}


def test_gold_from_rating_ties_structure_events_to_section_starts():
    g = gold_from_rating(RATED)

    # intro ok, buildup boundary moved to 20 (start confirmed by the drag), drop ok, drop->breakdown, added buildup split at 48
    assert g["sections"] == [{"start_bar": 0, "end_bar": 20, "label": "intro"},
                             {"start_bar": 20, "end_bar": 24, "label": "buildup"},  # dragging a start confirms it
                             {"start_bar": 24, "end_bar": 40, "label": "drop"},
                             {"start_bar": 40, "end_bar": 48, "label": "breakdown"},
                             {"start_bar": 48, "end_bar": 56, "label": "buildup"}]
    assert g["boundaries"] == [20, 24, 40, 48]
    assert g["events"] == [{"kind": "buildup", "bar": 20, "present": True},
                           {"kind": "drop", "bar": 24, "present": True},
                           {"kind": "vocal_in", "bar": 30, "present": True},
                           {"kind": "breakdown", "bar": 40, "present": True},
                           {"kind": "drop", "bar": 40, "present": False},
                           {"kind": "scene", "bar": 44, "present": True},
                           {"kind": "buildup", "bar": 48, "present": True},
                           {"kind": "vocal_out", "bar": 50, "present": False},
                           {"kind": "drop", "bar": 70, "present": True}]
    assert g["grid"] == {"beats": "ok", "downbeat": "wrong", "shift_beats": 1, "offset_ms": 0}
    assert g["beat_marks"] == [4.01]


@pytest.mark.parametrize("older,newer,expected_events,expected_sections", [
    pytest.param(
        {"events": [{"kind": "drop", "bar": 24, "present": True}], "sections": [{"start_bar": 0, "end_bar": 16, "label": "intro"}]},
        {"events": [{"kind": "drop", "bar": 24, "present": False}], "sections": [{"start_bar": 8, "end_bar": 24, "label": "buildup"}]},
        [{"kind": "drop", "bar": 24, "present": False}],
        [{"start_bar": 8, "end_bar": 24, "label": "buildup"}],
        id="newer-wins-on-overlap"),
    pytest.param(
        {"events": [{"kind": "drop", "bar": 24, "present": True}], "sections": [{"start_bar": 0, "end_bar": 16, "label": "intro"}]},
        {"events": [{"kind": "buildup", "bar": 16, "present": True}], "sections": [{"start_bar": 16, "end_bar": 24, "label": "buildup"}]},
        [{"kind": "buildup", "bar": 16, "present": True}, {"kind": "drop", "bar": 24, "present": True}],
        [{"start_bar": 0, "end_bar": 16, "label": "intro"}, {"start_bar": 16, "end_bar": 24, "label": "buildup"}],
        id="disjoint-judgements-add-up"),
])
def test_merge_gold(older, newer, expected_events, expected_sections):
    merged = merge_gold([older, newer])

    assert merged["events"] == expected_events
    assert merged["sections"] == expected_sections


def test_prefill_rating_carries_gold_onto_new_predictions():
    from edm_analysis.ratings import prefill_rating

    doc = {"sections": [{"label": "intro", "start_bar": 0, "end_bar": 16}, {"label": "buildup", "start_bar": 16, "end_bar": 24},
                        {"label": "drop", "start_bar": 24, "end_bar": 56}, {"label": "outro", "start_bar": 56, "end_bar": 64}],
           "events": [{"kind": "buildup", "bar": 16}, {"kind": "drop", "bar": 24}, {"kind": "vocal_in", "bar": 30}],
           "bars": {"start": [i * 2.0 for i in range(64)]}}
    gold = {"sections": [{"start_bar": 0, "end_bar": 20, "label": "intro"}, {"start_bar": 20, "end_bar": 24, "label": "buildup"},
                         {"start_bar": 24, "end_bar": 40, "label": "drop"}, {"start_bar": 40, "end_bar": 48, "label": "breakdown"}],
            "events": [{"kind": "vocal_in", "bar": 30, "present": False}, {"kind": "vocal_out", "bar": 44, "present": True}],
            "grid": {"beats": "ok"}}

    r = prefill_rating(doc, gold)

    by_start = {(s.get("true_start_bar") or s["start_bar"]): s for s in r["sections"]}
    assert by_start[0]["verdict"] == "ok"
    assert by_start[16]["correct_label"] == "intro"            # predicted start inside a gold intro
    assert by_start[20] == {"label": "buildup", "start_bar": 20, "end_bar": 24, "verdict": "ok", "correct_label": None,
                            "true_start_bar": None, "added": True}
    assert by_start[24]["verdict"] == "ok"
    assert by_start[40]["label"] == "breakdown" and by_start[40]["added"]
    assert by_start[56]["verdict"] is None                      # nothing known about the outro
    assert [e["verdict"] for e in r["events"]] == [None, None, "wrong"]
    assert r["missed"] == [{"kind": "vocal_out", "bar": 44, "time": 88.0}]
    assert r["grid"] == {"beats": "ok"} and r["prefilled"] is True
