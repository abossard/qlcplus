import copy

import pytest

from edm_analysis.fusion import FUSION_VERSION, fuse

N = 96
PLAIN = [("intro", 0, 32), ("buildup", 32, 40), ("drop", 40, 88), ("outro", 88, 96)]


def make_doc(sections=PLAIN, **levels):
    flat = {k: [0.5] * N for k in ("kick", "hihat", "bass", "instruments", "vocal", "loud")}
    flat.update({k: list(v) for k, v in levels.items()})
    return {"track": {"path": "/m/a.mp3", "title": "a", "sha256": "ab" * 32}, "analyzer": {"features": 2, "structure": 5, "version": "2.5"},
            "grid": {"bpm": 120.0, "period": 0.5, "first_downbeat": 0.0, "confidence": 0.9},
            "bars": {"start": [2.0 * k for k in range(N)], "levels": flat},
            "sections": [{"label": l, "start_bar": a, "end_bar": b, "start": 2.0 * a, "end": 2.0 * b} for l, a, b in sections],
            "events": [{"kind": s[0], "bar": s[1], "time": 2.0 * s[1]} for s in sections if s[0] in ("drop", "buildup", "breakdown")]
                      + [{"kind": "vocal_in", "bar": 20, "time": 40.0}]}


def stepped(bar):
    """Loudness that steps up at a bar: what makes a drop worth a buildup before it when no source says where it starts."""
    return [0.5] * bar + [0.9] * (N - bar)


def detector(*drops):
    return {"sections": [], "drops": [{"time": 2.0 * b, "confidence": p} for b, p in drops], "downbeats": []}


def edmformer(*parts):
    return {"sections": [{"start": 2.0 * b, "label": l} for b, l in parts], "drops": [{"time": 2.0 * b, "confidence": None} for b, l in parts if l == "drop"],
            "downbeats": []}


def labels(doc):
    return [(s["label"], s["start_bar"], s["end_bar"]) for s in doc["sections"]]


def drops(doc):
    return [s["start_bar"] for s in doc["sections"] if s["label"] == "drop"]


@pytest.mark.parametrize("found,expected", [
    ([(48, 0.9)], [48]),                    # the rules drop moves onto a drop detector drop after it
    ([(32, 0.9)], [32]),                    # or before it
    ([(40, 0.5)], [40]),                    # at the threshold
    ([(40, 0.9), (64, 0.8)], [40, 64]),     # a detector drop with no rules drop near is added
    ([(49, 0.9)], [49]),                    # nine bars away is another drop: the rules drop is unsupported and goes
    ([(48, 0.49)], []),                     # below the threshold the detector says nothing
    ([(40, 0.7), (48, 0.6)], [40, 48]),     # a second drop near the rules drop stays
])
def test_drops_are_the_detector_drops_the_rules_drops_move_onto(found, expected):
    got = fuse(make_doc(), {"dropdetector": detector(*found)})
    assert [e["bar"] for e in got["events"] if e["kind"] in ("drop", "lift")] == expected     # a drop straight after a drop is a lift


@pytest.mark.parametrize("candidates,expected", [
    ({}, [40]),                                                              # no Candidate output: the rules stand
    ({"edmformer": edmformer((0, "intro"), (44, "drop"))}, [44]),            # EDMFormer supports a rules drop and sets its bar
    ({"edmformer": edmformer((0, "intro"), (60, "drop"))}, [40]),            # too far: with no detector to say otherwise the rules stand
    ({"dropdetector": detector()}, []),                                      # the detector has output and finds no drop: unsupported
    ({"dropdetector": detector(), "edmformer": edmformer((0, "intro"), (44, "drop"))}, [44]),
    ({"allin1": {"sections": [], "drops": [], "downbeats": []}}, [40]),      # allin1 has no drops to say anything about
])
def test_rules_drops_need_support_only_where_a_detector_has_looked(candidates, expected):
    assert drops(fuse(make_doc(), candidates)) == expected


@pytest.mark.parametrize("sections,found,expected", [
    # an unsupported rules drop before any fused drop is an intro, after one a groove
    ([("intro", 0, 16), ("drop", 16, 40), ("breakdown", 40, 48), ("buildup", 48, 56), ("drop", 56, 96)], [(56, 0.9)],
     [("intro", 0, 40), ("breakdown", 40, 46), ("buildup", 46, 56), ("drop", 56, 96)]),
    ([("intro", 0, 16), ("buildup", 16, 24), ("drop", 24, 48), ("buildup", 48, 56), ("drop", 56, 96)], [(24, 0.9)],
     [("intro", 0, 14), ("buildup", 14, 24), ("drop", 24, 48), ("buildup", 48, 56), ("groove", 56, 96)]),
])
def test_other_loud_sections_are_what_the_evidence_supports(sections, found, expected):
    assert labels(fuse(make_doc(sections), {"dropdetector": detector(*found)})) == expected


@pytest.mark.parametrize("sections,found,start", [
    (PLAIN, [(40, 0.9)], 30),                                    # the rules' buildup start is two bars late
    (PLAIN, [(48, 0.9)], 30),                                    # a drop moved later keeps the rules' buildup start
    ([("intro", 0, 32), ("buildup", 32, 40), ("drop", 40, 96)], [(40, 0.9)], 30),
    ([("intro", 0, 16), ("breakdown", 16, 20), ("buildup", 20, 24), ("drop", 24, 96)], [(24, 0.9)], 20),   # never leaves the breakdown under 4 bars
    ([("intro", 0, 16), ("breakdown", 16, 22), ("buildup", 22, 26), ("drop", 26, 96)], [(26, 0.9)], 20),
])
def test_a_buildup_starts_two_bars_before_the_rules_start_theirs(sections, found, start):
    got = fuse(make_doc(sections), {"dropdetector": detector(*found)})
    drop = drops(got)[0]
    assert [(s["label"], s["start_bar"]) for s in got["sections"] if s["end_bar"] == drop] == [("buildup", start)]


@pytest.mark.parametrize("sections,found,expected", [
    ([("intro", 0, 40), ("drop", 40, 96)], [(40, 0.9)],                             # no rules buildup, no EDMFormer: a louder drop is no evidence
     [("intro", 0, 40), ("drop", 40, 96)]),
    ([("intro", 0, 18), ("breakdown", 18, 21), ("drop", 21, 96)], [(21, 0.9)],      # three bars of room: too short for a section
     [("intro", 0, 21), ("drop", 21, 96)]),
])
def test_no_buildup_without_evidence(sections, found, expected):
    assert labels(fuse(make_doc(sections, loud=stepped(found[0][0])), {"dropdetector": detector(*found)})) == expected


DROP_BUILDUP_DROP = [("intro", 0, 16), ("drop", 16, 40), ("buildup", 40, 48), ("drop", 48, 96)]


def dip(a, b):
    """Kick and bass gone between bars a and b, as in a real break."""
    gone = [0.9] * a + [0.1] * (b - a) + [0.9] * (N - b)
    return {"kick": gone, "bass": gone}


@pytest.mark.parametrize("em,levels,expected", [
    (None, {}, [("intro", 0, 16), ("drop", 16, 96)]),                                    # a rules buildup between two drops, no dip, nothing behind it: one drop
    ([(44, "buildup"), (48, "drop")], {}, [("intro", 0, 16), ("drop", 16, 96)]),         # EDMFormer has it but kick and bass play on
    ([(44, "buildup"), (48, "drop")], dip(44, 48),                                       # EDMFormer has it and there is a real dip: a buildup
     [("intro", 0, 16), ("drop", 16, 38), ("buildup", 38, 48), ("drop", 48, 96)]),
    (None, dip(44, 48), [("intro", 0, 16), ("drop", 16, 40), ("breakdown", 40, 48), ("drop", 48, 96)]),   # a dip with only the rules behind it is a breakdown, the drop after it stays
])
def test_between_two_drops_a_buildup_needs_edmformer_and_a_real_dip(em, levels, expected):
    candidates = {"dropdetector": detector((16, 0.9), (48, 0.9)), **({"edmformer": edmformer((0, "intro"), *em)} if em else {})}
    assert labels(fuse(make_doc(DROP_BUILDUP_DROP, **levels), candidates)) == expected


def test_a_stretch_the_rules_called_drop_before_a_later_detector_drop_is_a_buildup_from_the_rules_drop():
    got = fuse(make_doc([("intro", 0, 32), ("drop", 32, 96)]), {"dropdetector": detector((40, 0.9))})
    assert labels(got) == [("intro", 0, 32), ("buildup", 32, 40), ("drop", 40, 96)]
    assert {(s["label"], s["start_bar"]): s["sources"] for s in got["sections"]}[("buildup", 32)] == ["rules"]


def test_a_buildup_at_the_start_of_the_track_has_the_rules_behind_it():
    got = fuse(make_doc([("buildup", 0, 8), ("drop", 8, 96)]), {})
    assert [(s["label"], s["sources"]) for s in got["sections"]][:2] == [("buildup", ["rules"]), ("drop", ["rules"])]


@pytest.mark.parametrize("found,lifts", [([(40, 0.9), (64, 0.8)], [(64, ["dropdetector"])]), ([(40, 0.9), (60, 0.8), (80, 0.7)], [(60, ["dropdetector"]), (80, ["dropdetector"])]),
                                         ([(40, 0.9)], [])])
def test_a_drop_straight_after_a_drop_is_a_lift_inside_it(found, lifts):
    got = fuse(make_doc([("intro", 0, 40), ("drop", 40, 96)]), {"dropdetector": detector(*found)})
    assert labels(got) == [("intro", 0, 40), ("drop", 40, 96)]
    assert [(e["bar"], e["sources"]) for e in got["events"] if e["kind"] == "lift"] == lifts
    assert [e["bar"] for e in got["events"] if e["kind"] == "drop"] == [40]
    assert all(e["time"] == 2.0 * e["bar"] and set(e) == {"kind", "bar", "time", "sources"} for e in got["events"] if e["kind"] == "lift")


def test_a_drop_that_a_buildup_separates_from_the_one_before_is_no_lift():
    got = fuse(make_doc([("intro", 0, 16), ("drop", 16, 40), ("breakdown", 40, 56), ("buildup", 56, 64), ("drop", 64, 96)]), {"dropdetector": detector((16, 0.9), (64, 0.9))})
    assert [e["kind"] for e in got["events"] if e["kind"] in ("drop", "lift")] == ["drop", "drop"]


@pytest.mark.parametrize("em,expected", [
    ([(34, "buildup"), (44, "drop")], 38),     # EDMFormer's start is 4 bars before the rules': the rules' stands (EDMFormer was 1 exact in 5 on Gold)
    ([(30, "buildup"), (44, "drop")], 30),     # 8 bars or more before: a long buildup the rules' 4 bars cut short
])
def test_edmformers_start_wins_only_where_it_makes_the_buildup_much_longer(em, expected):
    sections = [("intro", 0, 20), ("breakdown", 20, 40), ("buildup", 40, 44), ("drop", 44, 96)]
    got = fuse(make_doc(sections), {"dropdetector": detector((44, 0.9)), "edmformer": edmformer((0, "intro"), *em)})
    assert [(l, a) for l, a, b in labels(got) if l == "buildup"] == [("buildup", expected)]


@pytest.mark.parametrize("sections,entries", [
    ([("intro", 0, 40), ("drop", 40, 96)], [40]),                                                              # nothing before the first drop
    ([("intro", 0, 20), ("breakdown", 20, 40), ("drop", 40, 96)], []),
    ([("intro", 0, 20), ("buildup", 20, 40), ("drop", 40, 96)], []),
    ([("intro", 0, 20), ("drop", 20, 50), ("breakdown", 50, 60), ("drop", 60, 96)], [20]),                   # only the drops before the first break
])
def test_a_first_beat_entry_is_a_drop_with_no_breakdown_or_buildup_before_it(sections, entries):
    from edm_analysis.fusion import entry_drops

    assert entry_drops([{"label": l, "start_bar": a, "end_bar": b} for l, a, b in sections]) == entries


def test_a_buildup_names_the_sources_that_put_it_within_the_rules_shift():
    got = fuse(make_doc(), {"dropdetector": detector((40, 0.9)), "edmformer": edmformer((0, "intro"), (31, "buildup"), (40, "drop"))})
    assert {(s["label"], s["start_bar"]): s["sources"] for s in got["sections"]}[("buildup", 30)] == ["edmformer", "rules"]


def test_edmformers_buildup_start_is_used_where_the_rules_have_none():
    got = fuse(make_doc([("intro", 0, 40), ("drop", 40, 96)]),
               {"dropdetector": detector((40, 0.9)), "edmformer": edmformer((0, "intro"), (34, "buildup"), (40, "drop"))})
    assert [(l, a) for l, a, b in labels(got) if l == "buildup"] == [("buildup", 34)]


def test_edmformers_buildup_of_another_drop_is_not_used():     # so there is no buildup before this one
    got = fuse(make_doc([("intro", 0, 40), ("drop", 40, 96)], loud=stepped(40)),
               {"dropdetector": detector((40, 0.9)), "edmformer": edmformer((0, "intro"), (24, "buildup"), (28, "drop"), (60, "breakdown"))})
    assert [(l, a) for l, a, b in labels(got) if l == "buildup"] == []


def test_a_rules_only_drop_that_starts_below_its_later_level_moves_to_the_step():
    levels = {k: [0.5] * 40 + [0.6] * 8 + [0.95] * (N - 48) for k in ("loud", "kick", "bass")}
    got = fuse(make_doc([("intro", 0, 32), ("buildup", 32, 40), ("drop", 40, 96)], **levels), {})
    assert drops(got) == [48] and [(l, a) for l, a, b in labels(got) if l == "buildup"] == [("buildup", 30)]


@pytest.mark.parametrize("step", [0.05, 0.1])
def test_a_small_step_inside_a_rules_drop_moves_nothing(step):
    levels = {k: [0.5] * 40 + [0.8] * 8 + [0.8 + step] * (N - 48) for k in ("loud", "kick", "bass")}
    assert drops(fuse(make_doc(**levels), {})) == [40]


def test_sources_tell_who_supports_a_claim_on_its_exact_bar():
    got = fuse(make_doc(), {"dropdetector": detector((40, 0.9)), "edmformer": edmformer((0, "intro"), (40, "drop"))})
    by = {(s["label"], s["start_bar"]): s["sources"] for s in got["sections"]}
    assert by[("drop", 40)] == ["dropdetector", "edmformer", "rules"]
    moved = fuse(make_doc(), {"dropdetector": detector((48, 0.9))})
    assert {(s["label"], s["start_bar"]): s["sources"] for s in moved["sections"]}[("drop", 48)] == ["dropdetector"]
    assert {e["kind"]: e["sources"] for e in moved["events"] if e["kind"] == "drop"} == {"drop": ["dropdetector"]}


def test_the_document_keeps_its_shape_and_the_rules_output():
    doc = make_doc()
    got = fuse(doc, {"dropdetector": detector((48, 0.9))})
    assert {k for s in got["sections"] for k in s} == {"label", "start_bar", "end_bar", "start", "end", "sources"}
    assert [s["start_bar"] for s in got["sections"]] == sorted(s["start_bar"] for s in got["sections"])
    assert got["sections"][0]["start_bar"] == 0 and got["sections"][-1]["end_bar"] == N
    assert all(a["end_bar"] == b["start_bar"] for a, b in zip(got["sections"], got["sections"][1:]))
    assert got["rules"]["sections"] == doc["sections"] and got["rules"]["events"] == doc["events"]
    assert got["bars"] == doc["bars"] and got["grid"] == doc["grid"]
    assert [e for e in got["events"] if e["kind"] == "vocal_in"] == [{"kind": "vocal_in", "bar": 20, "time": 40.0}]
    assert {e["kind"] for e in got["events"]} == {"drop", "buildup", "vocal_in"}


def test_events_sit_on_the_starts_of_the_fused_sections():
    got = fuse(make_doc(), {"dropdetector": detector((48, 0.9))})
    starts = {(s["label"], s["start_bar"]) for s in got["sections"]}
    assert {(e["kind"], e["bar"]) for e in got["events"] if e["kind"] != "vocal_in"} <= starts
    assert [e["time"] for e in got["events"] if e["kind"] == "drop"] == [96.0]


def test_the_version_tells_a_fused_file_from_a_rules_file():
    got = fuse(make_doc(), {})
    assert got["analyzer"]["fusion"] == FUSION_VERSION and got["analyzer"]["version"] == f"2.5.{FUSION_VERSION}"
    assert make_doc()["analyzer"]["version"] == "2.5"


@pytest.mark.parametrize("candidates", [
    {}, {"dropdetector": detector((48, 0.9), (64, 0.3))}, {"dropdetector": detector((48, 0.9)), "edmformer": edmformer((0, "intro"), (34, "buildup"), (48, "drop"))}])
def test_fusing_twice_changes_nothing(candidates):
    once = fuse(make_doc(), candidates)
    assert fuse(once, candidates) == once


def test_fusing_leaves_its_input_alone():
    doc = make_doc()
    before = copy.deepcopy(doc)
    fuse(doc, {"dropdetector": detector((48, 0.9))})
    assert doc == before


@pytest.mark.parametrize("found,offset", [([(40, 0.95)], 0), ([(41, 0.95), (57, 0.95)], 1), ([(43, 0.95), (45, 0.4)], 3), ([], 0)])
def test_the_phrase_offset_is_where_the_sure_drops_sit_on_an_eight_bar_line(found, offset):
    got = fuse(make_doc([("intro", 0, 40), ("drop", 40, 96)]), {"dropdetector": detector(*found)})
    assert got["support"]["phrase_offset"] == offset


def test_support_lists_every_source_that_has_output_per_kind():
    got = fuse(make_doc(), {"dropdetector": detector((48, 0.9), (50, 0.2), (52, 0.01)), "edmformer": edmformer((0, "intro"), (34, "buildup"), (48, "drop"))})
    sup = got["support"]
    assert sup["drop"] == {"rules": [[40, None]], "dropdetector": [[48, 0.9], [50, 0.2]], "edmformer": [[48, None]]}   # below 0.05 is noise
    assert sup["buildup"] == {"rules": [[32, None]], "edmformer": [[34, None]]}
    assert sup["breakdown"] == {"rules": [], "edmformer": []}
    assert fuse(make_doc(), {})["support"]["drop"] == {"rules": [[40, None]]}


def test_a_buildup_start_never_leaves_the_section_it_eats_into_shorter_than_four_bars():
    got = fuse(make_doc([("intro", 0, 32), ("breakdown", 32, 36), ("buildup", 36, 44), ("drop", 44, 96)]), {"dropdetector": detector((44, 0.9))})
    assert labels(got) == [("intro", 0, 32), ("breakdown", 32, 36), ("buildup", 36, 44), ("drop", 44, 96)]


@pytest.mark.parametrize("fragment_level,expected", [
    (0.0, [("intro", 0, 16), ("breakdown", 16, 40), ("groove", 40, 96)]),       # sounds like the breakdown before it
    (0.9, [("intro", 0, 16), ("breakdown", 16, 38), ("groove", 38, 96)]),       # sounds like the groove after it
])
def test_a_section_shorter_than_four_bars_joins_the_neighbour_it_sounds_like(fragment_level, expected):
    loud = [0.0] * 38 + [fragment_level] * 2 + [0.9] * 56
    sections = [("intro", 0, 16), ("breakdown", 16, 38), ("intro", 38, 40), ("groove", 40, 96)]
    assert labels(fuse(make_doc(sections, kick=loud, loud=loud), {})) == expected


@pytest.mark.parametrize("sections,found", [
    ([("intro", 0, 16), ("buildup", 16, 24), ("drop", 24, 94), ("outro", 94, 96)], [(24, 0.9)]),     # a short outro
    ([("intro", 0, 3), ("breakdown", 3, 32), ("drop", 32, 96)], [(32, 0.9)]),                          # a short intro
    ([("intro", 0, 32), ("drop", 32, 96)], [(32, 0.9), (95, 0.9)]),                                    # a detector drop in the last bar
    ([("intro", 0, 32), ("drop", 32, 96)], [(34, 0.9), (36, 0.9)]),
])
def test_no_fused_section_is_shorter_than_four_bars(sections, found):
    got = fuse(make_doc(sections, loud=stepped(sections[1][1])), {"dropdetector": detector(*found)})
    assert all(b - a >= 4 for l, a, b in labels(got)), labels(got)
    assert got["sections"][0]["start_bar"] == 0 and got["sections"][-1]["end_bar"] == N
    assert all(x["end_bar"] == y["start_bar"] for x, y in zip(got["sections"], got["sections"][1:]))
