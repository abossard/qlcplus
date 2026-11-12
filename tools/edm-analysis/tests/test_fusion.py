import copy

import pytest

from edm_analysis.fusion import FUSION_VERSION, fuse

N = 96
PLAIN = [("intro", 0, 32), ("buildup", 32, 40), ("drop", 40, 88), ("outro", 88, 96)]


def make_doc(sections=PLAIN, **levels):
    flat = {k: [0.5] * N for k in ("kick", "hihat", "bass", "instruments", "vocal", "loud")}
    flat["kick"] = [0.8] * N       # a running beat, as in most songs
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
    # an unsupported rules drop is a steady beat part: before any other part it is intro, as the last section the outro
    ([("intro", 0, 16), ("drop", 16, 40), ("breakdown", 40, 48), ("buildup", 48, 56), ("drop", 56, 96)], [(56, 0.9)],
     [("intro", 0, 40), ("breakdown", 40, 48), ("buildup", 48, 56), ("drop", 56, 96)]),
    ([("intro", 0, 16), ("buildup", 16, 24), ("drop", 24, 48), ("buildup", 48, 56), ("drop", 56, 96)], [(24, 0.9)],
     [("intro", 0, 16), ("buildup", 16, 24), ("drop", 24, 48), ("buildup", 48, 56), ("outro", 56, 96)]),
])
def test_other_loud_sections_are_what_the_evidence_supports(sections, found, expected):
    assert labels(fuse(make_doc(sections), {"dropdetector": detector(*found)})) == expected


@pytest.mark.parametrize("sections,found,expected", [
    ([("intro", 0, 40), ("drop", 40, 96)], [(40, 0.9)],                             # no rules buildup, no EDMFormer: a louder drop is no evidence
     [("intro", 0, 40), ("drop", 40, 96)]),
    ([("intro", 0, 18), ("breakdown", 18, 21), ("drop", 21, 96)], [(21, 0.9)],      # three bars of room: too short for a section
     [("intro", 0, 21), ("drop", 21, 96)]),
])
def test_no_buildup_without_evidence(sections, found, expected):
    assert labels(fuse(make_doc(sections, loud=stepped(found[0][0])), {"dropdetector": detector(*found)})) == expected


DROP_BUILDUP_DROP = [("intro", 0, 8), ("breakdown", 8, 16), ("drop", 16, 40), ("buildup", 40, 48), ("drop", 48, 96)]


def dip(a, b):
    """Kick and bass gone between bars a and b, as in a real break."""
    gone = [0.9] * a + [0.1] * (b - a) + [0.9] * (N - b)
    return {"kick": gone, "bass": gone}


@pytest.mark.parametrize("em,levels,expected", [
    (None, {}, [("intro", 0, 8), ("breakdown", 8, 16), ("drop", 16, 96)]),                          # a rules buildup between two drops, no dip, nothing behind it: one drop
    ([(44, "buildup"), (48, "drop")], {}, [("intro", 0, 8), ("breakdown", 8, 16), ("drop", 16, 96)]),         # EDMFormer has it but kick and bass play on
    ([(44, "buildup"), (48, "drop")], dip(44, 48),                                                  # EDMFormer has it and there is a real dip: a buildup
     [("intro", 0, 8), ("breakdown", 8, 16), ("drop", 16, 40), ("buildup", 40, 48), ("drop", 48, 96)]),
    (None, dip(44, 48), [("intro", 0, 8), ("breakdown", 8, 16), ("drop", 16, 40), ("breakdown", 40, 48), ("drop", 48, 96)]),   # a dip with only the rules behind it is a breakdown, the drop after it stays
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
    got = fuse(make_doc([("intro", 0, 8), ("breakdown", 8, 16), ("drop", 16, 40), ("breakdown", 40, 56), ("buildup", 56, 64), ("drop", 64, 96)]), {"dropdetector": detector((16, 0.9), (64, 0.9))})
    assert [e["kind"] for e in got["events"] if e["kind"] in ("drop", "lift")] == ["drop", "drop"]


@pytest.mark.parametrize("sections,entries", [
    ([("intro", 0, 40), ("drop", 40, 96)], [40]),                                                              # nothing before the first drop
    ([("intro", 0, 20), ("breakdown", 20, 40), ("drop", 40, 96)], []),
    ([("intro", 0, 20), ("buildup", 20, 40), ("drop", 40, 96)], []),
    ([("intro", 0, 20), ("drop", 20, 50), ("breakdown", 50, 60), ("drop", 60, 96)], [20]),                   # only the drops before the first break
])
def test_a_first_beat_entry_is_a_drop_with_no_breakdown_or_buildup_before_it(sections, entries):
    from edm_analysis.fusion import entry_drops

    assert entry_drops([{"label": l, "start_bar": a, "end_bar": b} for l, a, b in sections]) == entries


def test_a_buildup_names_the_sources_that_have_a_buildup_before_that_drop():
    got = fuse(make_doc(), {"dropdetector": detector((40, 0.9)), "edmformer": edmformer((0, "intro"), (31, "buildup"), (40, "drop"))})
    assert {(s["label"], s["start_bar"]): s["sources"] for s in got["sections"]}[("buildup", 32)] == ["edmformer", "rules"]


def test_edmformers_buildup_is_evidence_where_the_rules_have_none():
    got = fuse(make_doc([("intro", 0, 40), ("drop", 40, 96)]),
               {"dropdetector": detector((40, 0.9)), "edmformer": edmformer((0, "intro"), (34, "buildup"), (40, "drop"))})
    assert [(l, a, src) for (l, a, b), src in zip(labels(got), [s["sources"] for s in got["sections"]]) if l == "buildup"] == [("buildup", 32, ["edmformer"])]


def test_edmformers_buildup_of_another_drop_is_not_used():     # so there is no buildup before this one
    got = fuse(make_doc([("intro", 0, 40), ("drop", 40, 96)], loud=stepped(40)),
               {"dropdetector": detector((40, 0.9)), "edmformer": edmformer((0, "intro"), (24, "buildup"), (28, "drop"), (60, "breakdown"))})
    assert [(l, a) for l, a, b in labels(got) if l == "buildup"] == []


def test_a_rules_only_drop_that_starts_below_its_later_level_moves_to_the_step():
    levels = {k: [0.5] * 40 + [0.6] * 8 + [0.95] * (N - 48) for k in ("loud", "kick", "bass")}
    got = fuse(make_doc([("intro", 0, 32), ("buildup", 32, 40), ("drop", 40, 96)], **levels), {})
    assert drops(got) == [48] and [(l, a) for l, a, b in labels(got) if l == "buildup"] == [("buildup", 40)]


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
    assert {k for s in got["sections"] for k in s} == {"label", "start_bar", "end_bar", "start", "end", "sources", "energy", "nominal"}
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
    (0.0, [("intro", 0, 16), ("drop", 16, 60), ("breakdown", 60, 88), ("outro", 88, 96)]),       # sounds like the breakdown before it
    (0.9, [("intro", 0, 16), ("drop", 16, 60), ("breakdown", 60, 86), ("outro", 86, 96)]),       # sounds like the outro after it
])
def test_a_section_shorter_than_four_bars_joins_the_neighbour_it_sounds_like(fragment_level, expected):
    loud = [0.5] * 60 + [0.0] * 26 + [fragment_level] * 2 + [0.9] * 8
    sections = [("intro", 0, 16), ("drop", 16, 60), ("breakdown", 60, 86), ("buildup", 86, 88), ("outro", 88, 96)]
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


# --- five labels, energy ------------------------------------------------------------------

ALLOWED = {"intro", "buildup", "drop", "breakdown", "outro"}
RISING = [0.5] * 64 + [0.5 + 0.05 * i for i in range(8)] + [0.9] * 24      # loudness climbs through bars 64 to 72


@pytest.mark.parametrize("sections,found,levels,expected", [
    # a steady beat part before the first breakdown, buildup or drop is intro
    ([("intro", 0, 16), ("groove", 16, 32), ("drop", 32, 96)], [(32, 0.9)], {}, [("intro", 0, 32), ("drop", 32, 96)]),
    # after a drop with the beat still running it is part of that drop
    ([("intro", 0, 8), ("breakdown", 8, 16), ("drop", 16, 40), ("groove", 40, 56), ("breakdown", 56, 72), ("drop", 72, 96)], [(16, 0.9), (72, 0.9)], {},
     [("intro", 0, 8), ("breakdown", 8, 16), ("drop", 16, 56), ("breakdown", 56, 72), ("drop", 72, 96)]),
    # after a drop with the beat gone it is a breakdown
    ([("intro", 0, 8), ("breakdown", 8, 16), ("drop", 16, 40), ("groove", 40, 56), ("breakdown", 56, 72), ("drop", 72, 96)], [(16, 0.9), (72, 0.9)], {"kick": [0.5] * 40 + [0.1] * 16 + [0.5] * 40},
     [("intro", 0, 8), ("breakdown", 8, 16), ("drop", 16, 40), ("breakdown", 40, 72), ("drop", 72, 96)]),
    # between a breakdown and a drop: flat is breakdown, rising is the buildup
    ([("intro", 0, 8), ("breakdown", 8, 16), ("drop", 16, 40), ("breakdown", 40, 64), ("groove", 64, 72), ("drop", 72, 96)], [(16, 0.9), (72, 0.9)], {},
     [("intro", 0, 8), ("breakdown", 8, 16), ("drop", 16, 40), ("breakdown", 40, 72), ("drop", 72, 96)]),
    ([("intro", 0, 8), ("breakdown", 8, 16), ("drop", 16, 40), ("breakdown", 40, 64), ("groove", 64, 72), ("drop", 72, 96)], [(16, 0.9), (72, 0.9)], {"loud": RISING},
     [("intro", 0, 8), ("breakdown", 8, 16), ("drop", 16, 40), ("breakdown", 40, 64), ("buildup", 64, 72), ("drop", 72, 96)]),
    # between a buildup and its drop it belongs to the buildup, which starts 8 bars before the drop
    ([("intro", 0, 16), ("breakdown", 16, 32), ("buildup", 32, 40), ("groove", 40, 48), ("drop", 48, 96)], [(48, 0.9)], {},
     [("intro", 0, 16), ("breakdown", 16, 40), ("buildup", 40, 48), ("drop", 48, 96)]),
    # as the last section it is the outro
    ([("intro", 0, 16), ("breakdown", 16, 32), ("drop", 32, 72), ("breakdown", 72, 80), ("groove", 80, 96)], [(32, 0.9)], {},
     [("intro", 0, 16), ("breakdown", 16, 32), ("drop", 32, 72), ("breakdown", 72, 80), ("outro", 80, 96)]),
])
def test_a_steady_beat_part_is_named_by_where_it_sits(sections, found, levels, expected):
    assert labels(fuse(make_doc(sections, **levels), {"dropdetector": detector(*found)})) == expected


@pytest.mark.parametrize("sections,found,expected", [
    # an intro in the middle of the song, an outro before the end
    ([("intro", 0, 16), ("breakdown", 16, 32), ("intro", 32, 40), ("drop", 40, 64), ("outro", 64, 80), ("drop", 80, 96)], [(40, 0.9), (80, 0.9)],
     [("intro", 0, 16), ("breakdown", 16, 40), ("drop", 40, 96)]),
    ([("intro", 0, 16), ("breakdown", 16, 32), ("drop", 32, 64), ("outro", 64, 80), ("breakdown", 80, 96)], [(32, 0.9)],
     [("intro", 0, 16), ("breakdown", 16, 32), ("drop", 32, 80), ("breakdown", 80, 96)]),
])
def test_intro_only_opens_the_track_and_outro_only_closes_it(sections, found, expected):
    got = fuse(make_doc(sections), {"dropdetector": detector(*found)})
    assert labels(got) == expected


@pytest.mark.parametrize("candidates", [{}, {"dropdetector": detector((40, 0.9), (64, 0.8))}, {"dropdetector": detector()}])
def test_no_fused_section_has_another_label_than_the_five(candidates):
    sections = [("intro", 0, 16), ("groove", 16, 24), ("drop", 24, 40), ("groove", 40, 48), ("breakdown", 48, 56), ("intro", 56, 60),
                ("buildup", 60, 64), ("drop", 64, 80), ("outro", 80, 88), ("groove", 88, 96)]
    got = fuse(make_doc(sections), candidates)
    assert {s["label"] for s in got["sections"]} <= ALLOWED
    assert [s["label"] for s in got["sections"]].index("intro") == 0 and "intro" not in [s["label"] for s in got["sections"][1:]]
    assert "outro" not in [s["label"] for s in got["sections"][:-1]]


RUNNING = {"kick": [0.8] * N}      # the beat never stops: every drop looks like a first beat entry


@pytest.mark.parametrize("sections,found,expected,events", [
    # a track that never breaks down keeps its sure drop; the rules' buildup before it stands (My Head)
    ([("intro", 0, 32), ("buildup", 32, 40), ("drop", 40, 88), ("outro", 88, 96)], [(16, 0.68), (24, 0.59), (42, 0.99), (66, 0.6)],
     [("intro", 0, 34), ("buildup", 34, 42), ("drop", 42, 88), ("outro", 88, 96)], [42, 66]),
    # the rule applies once: the entry goes, the next sure hit stays a drop and later hits are its lifts
    ([("intro", 0, 16), ("drop", 16, 88), ("outro", 88, 96)], [(16, 0.92), (32, 0.9), (48, 0.97)],
     [("intro", 0, 32), ("drop", 32, 88), ("outro", 88, 96)], [32, 48]),
    # hits the detector is not sure of do not end the intro, the first sure one does
    ([("intro", 0, 16), ("drop", 16, 40), ("groove", 40, 52), ("drop", 52, 88), ("outro", 88, 96)], [(16, 0.57), (32, 0.55), (44, 0.93)],
     [("intro", 0, 44), ("drop", 44, 88), ("outro", 88, 96)], [44]),
    # but an intro never grows past half the track this way: then the entry stays the drop (One Night)
    ([("intro", 0, 16), ("drop", 16, 40), ("groove", 40, 52), ("drop", 52, 88), ("outro", 88, 96)], [(16, 0.57), (52, 0.55), (80, 0.93)],
     [("intro", 0, 16), ("drop", 16, 88), ("outro", 88, 96)], [16, 52, 80]),
    # the only drop of a track stays a drop
    ([("intro", 0, 40), ("drop", 40, 96)], [(40, 0.95)], [("intro", 0, 40), ("drop", 40, 96)], [40]),
    ([("intro", 0, 40), ("drop", 40, 96)], [(40, 0.6), (64, 0.7)], [("intro", 0, 40), ("drop", 40, 96)], [40, 64]),
    # a drop with a buildup before it is no entry, whatever the kick does
    ([("intro", 0, 32), ("buildup", 32, 40), ("drop", 40, 64), ("breakdown", 64, 72), ("drop", 72, 96)], [(40, 0.9), (72, 0.9)],
     [("intro", 0, 32), ("buildup", 32, 40), ("drop", 40, 64), ("breakdown", 64, 72), ("drop", 72, 96)], [40, 72]),
])
def test_the_entry_rule_applies_once_and_never_leaves_a_track_without_its_sure_drop(sections, found, expected, events):
    got = fuse(make_doc(sections, **RUNNING), {"dropdetector": detector(*found)})
    assert labels(got) == expected
    assert [e["bar"] for e in got["events"] if e["kind"] in ("drop", "lift")] == events


@pytest.mark.parametrize("found", [[(16, 0.95)], [(16, 0.6), (40, 0.91)], [(8, 0.9), (16, 0.9), (24, 0.9), (32, 0.9), (40, 0.9), (48, 0.9)], [(16, 0.5), (90, 0.99)], [(16, 0.5), (94, 0.99)]])
@pytest.mark.parametrize("sections", [[("intro", 0, 16), ("drop", 16, 96)], [("intro", 0, 96)], [("intro", 0, 16), ("drop", 16, 40), ("groove", 40, 56), ("drop", 56, 96)]])
def test_a_track_with_a_sure_detector_drop_always_has_a_drop(sections, found):
    got = fuse(make_doc(sections, **RUNNING), {"dropdetector": detector(*found)})
    assert any(s["label"] == "drop" for s in got["sections"]), labels(got)


def test_every_section_carries_an_energy_level_from_1_to_10_and_the_loudest_has_10():
    loud = [0.05] * 16 + [0.5] * 16 + [0.9] * 32 + [0.3] * 16 + [0.95] * 16
    got = fuse(make_doc([("intro", 0, 16), ("breakdown", 16, 32), ("drop", 32, 64), ("breakdown", 64, 80), ("drop", 80, 96)], loud=loud),
               {"dropdetector": detector((32, 0.9), (80, 0.9))})
    assert [(s["label"], s["energy"]) for s in got["sections"]] == [("intro", 1), ("breakdown", 5), ("drop", 9), ("breakdown", 3), ("drop", 10)]
    assert all(isinstance(s["energy"], int) for s in got["sections"])


@pytest.mark.parametrize("sections,em,start", [
    ([("intro", 0, 16), ("breakdown", 16, 32), ("buildup", 32, 48), ("drop", 48, 96)], None, 40),                       # 8 bars before the drop, not where the rules start
    ([("intro", 0, 16), ("breakdown", 16, 44), ("buildup", 44, 48), ("drop", 48, 96)], None, 40),
    ([("intro", 0, 16), ("breakdown", 16, 32), ("buildup", 32, 48), ("drop", 48, 96)], [(36, "buildup"), (48, "drop")], 40),   # EDMFormer at 12 bars: still 8
    ([("intro", 0, 16), ("breakdown", 16, 32), ("buildup", 32, 48), ("drop", 48, 96)], [(34, "buildup"), (48, "drop")], 40),   # at 14 bars or more: still nominal, the start is no exact bar
    ([("intro", 0, 16), ("breakdown", 16, 43), ("buildup", 43, 48), ("drop", 48, 96)], None, 40),
    ([("intro", 0, 38), ("breakdown", 38, 44), ("buildup", 44, 48), ("drop", 48, 96)], None, 42),                       # never leaves the breakdown under 4 bars
    ([("intro", 0, 40), ("breakdown", 40, 44), ("buildup", 44, 48), ("drop", 48, 96)], None, 44),                       # nor takes a 4-bar breakdown whole
])
def test_a_buildup_is_nominally_8_bars_before_its_drop_whatever_edmformers_start_is(sections, em, start):
    found = [(s[1], 0.9) for s in sections if s[0] == "drop"]
    kick = [0.5] * 96
    for a, b in [(s[1], s[2]) for s in sections if s[0] in ("breakdown", "buildup")]:
        kick[a:b] = [0.1] * (b - a)
    bass = list(kick)
    got = fuse(make_doc(sections, kick=kick, bass=bass), {"dropdetector": detector(*found), **({"edmformer": edmformer((0, "intro"), *em)} if em else {})})
    assert [(s["label"], s["start_bar"]) for s in got["sections"] if s["end_bar"] == 48] == [("buildup", start)]


def test_where_the_rules_have_no_buildup_the_section_before_the_drop_may_be_the_buildup_whole():
    got = fuse(make_doc([("intro", 0, 32), ("breakdown", 32, 40), ("drop", 40, 96)]),
               {"dropdetector": detector((40, 0.9)), "edmformer": edmformer((0, "intro"), (34, "buildup"), (40, "drop"))})
    assert labels(got) == [("intro", 0, 32), ("buildup", 32, 40), ("drop", 40, 96)]


# --- round 6: the entry is intro with an event, nominal buildups, outro by the beat ---------------

@pytest.mark.parametrize("kick_before", [0.1, 0.5, 0.9])
def test_a_first_beat_entry_is_intro_whatever_the_kick_was_and_says_where_the_beat_comes_in(kick_before):
    kick = [kick_before] * 40 + [0.9] * 24 + [0.1] * 8 + [0.9] * 24
    got = fuse(make_doc([("intro", 0, 40), ("drop", 40, 64), ("breakdown", 64, 72), ("drop", 72, 96)], kick=kick),
               {"dropdetector": detector((40, 0.9), (48, 0.9), (72, 0.9))})
    assert labels(got) == [("intro", 0, 48), ("drop", 48, 64), ("breakdown", 64, 72), ("drop", 72, 96)]
    entry = [e for e in got["events"] if e["kind"] == "entry"]
    assert entry == [{"kind": "entry", "bar": 40, "time": 80.0}]
    assert [e["bar"] for e in got["events"] if e["kind"] in ("drop", "lift")] == [48, 72]


@pytest.mark.parametrize("sections,found", [
    ([("intro", 0, 40), ("drop", 40, 96)], [(40, 0.95)]),                                                   # the only drop stays, with no entry event
    ([("intro", 0, 32), ("buildup", 32, 40), ("drop", 40, 96)], [(40, 0.95), (60, 0.95)]),                  # a buildup before it: no entry
    ([("intro", 0, 32), ("breakdown", 32, 40), ("drop", 40, 96)], [(40, 0.95)]),
])
def test_no_entry_event_where_the_drop_stays_or_something_comes_before_it(sections, found):
    got = fuse(make_doc(sections, **RUNNING), {"dropdetector": detector(*found)})
    assert [e for e in got["events"] if e["kind"] == "entry"] == [] and any(s["label"] == "drop" for s in got["sections"])


def test_at_most_one_entry_event_per_track():
    got = fuse(make_doc([("intro", 0, 16), ("drop", 16, 64), ("breakdown", 64, 72), ("drop", 72, 96)], **RUNNING),
               {"dropdetector": detector((16, 0.9), (32, 0.9), (48, 0.95), (72, 0.9))})
    assert [e["bar"] for e in got["events"] if e["kind"] == "entry"] == [16]


LOUD_END = {"kick": [0.9] * 40 + [0.8] * 56}


@pytest.mark.parametrize("tail,kick,loud,expected", [
    (20, 0.75, 0.74, "outro"),         # the beat is still running (r06-p008)
    (4, 0.0, 0.0, "outro"),            # a silent tail, energy 1 (r06-p024)
    (9, 0.27, 0.51, "breakdown"),      # the beat is mostly gone, the level is not (r06-p013)
    (21, 0.46, 0.51, "breakdown"),     # (r06-p020)
    (12, 0.6, 0.5, "outro"),           # the threshold itself is a running beat
    (12, 0.59, 0.5, "breakdown"),
])
def test_the_last_section_is_outro_only_where_the_beat_is_running_or_the_song_fades_out(tail, kick, loud, expected):
    start = N - tail
    levels = {"kick": [0.9] * start + [kick] * tail, "loud": [0.95] * start + [loud] * tail}
    got = fuse(make_doc([("intro", 0, 16), ("drop", 16, start), ("outro", start, N)], **levels), {"dropdetector": detector((16, 0.9))})
    assert labels(got)[-1][0] == expected


def test_an_outro_that_becomes_a_breakdown_merges_with_the_breakdown_before_it():
    levels = {"kick": [0.9] * 40 + [0.1] * 56, "loud": [0.95] * 40 + [0.5] * 56}
    got = fuse(make_doc([("intro", 0, 16), ("drop", 16, 40), ("breakdown", 40, 80), ("outro", 80, 96)], **levels), {"dropdetector": detector((16, 0.9), (24, 0.9))})
    assert labels(got)[-1] == ("breakdown", 40, 96)


def test_a_buildup_is_nominally_8_bars_and_says_so():
    got = fuse(make_doc([("intro", 0, 16), ("breakdown", 16, 32), ("buildup", 32, 48), ("drop", 48, 96)]), {"dropdetector": detector((48, 0.9)), "edmformer": edmformer((0, "intro"), (34, "buildup"), (48, "drop"))})
    assert [(s["label"], s["start_bar"], s.get("nominal")) for s in got["sections"] if s["label"] in ("buildup", "drop", "breakdown")] == [
        ("breakdown", 16, None), ("buildup", 40, True), ("drop", 48, None)]       # EDMFormer's earlier start is not followed either


# --- the ramp --------------------------------------------------------------------------------

SECTIONS = [{"label": "intro", "start_bar": 0, "end_bar": 8}, {"label": "buildup", "start_bar": 8, "end_bar": 16}, {"label": "drop", "start_bar": 16, "end_bar": 24},
            {"label": "breakdown", "start_bar": 24, "end_bar": 28}, {"label": "buildup", "start_bar": 28, "end_bar": 32}]


def test_without_a_measured_rise_the_ramp_is_a_straight_line_over_each_buildup_and_zero_elsewhere():
    from edm_analysis.fusion import ramp_levels

    out = ramp_levels({}, SECTIONS, 32)
    assert len(out) == 32 and out[:8] == [0.0] * 8 and out[16:28] == [0.0] * 12
    assert out[8:16] == pytest.approx([i / 8 for i in range(1, 9)]) and out[28:32] == pytest.approx([0.25, 0.5, 0.75, 1.0])


@pytest.mark.parametrize("key", ["onset", "centroid"])
def test_a_measured_rise_in_onset_density_or_brightness_shapes_the_ramp(key):
    from edm_analysis.fusion import ramp_levels

    series = [0.5] * 8 + [0.0, 0.0, 0.1, 0.2, 0.6, 0.7, 0.9, 1.0] + [0.5] * 16
    out = ramp_levels({key: series}, SECTIONS, 32)
    assert out[8:16] == pytest.approx([0.0, 0.0, 0.1, 0.2, 0.6, 0.7, 0.9, 1.0])
    assert out[15] == 1.0 and all(b >= a for a, b in zip(out[8:16], out[9:16]))


@pytest.mark.parametrize("series", [
    [0.5] * 32,                                                          # no change
    [0.5] * 8 + [1.0, 0.9, 0.8, 0.7, 0.5, 0.3, 0.1, 0.0] + [0.5] * 16,     # falling
    [0.5] * 8 + [0.50, 0.52, 0.50, 0.51, 0.50, 0.52, 0.51, 0.50] + [0.5] * 16,    # too small a range
    [0.5] * 8 + [0.0, 1.0, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0] + [0.5] * 16,     # noise, no trend
])
def test_a_rise_that_is_not_there_leaves_the_straight_line(series):
    from edm_analysis.fusion import ramp_levels

    assert ramp_levels({"onset": series, "centroid": series}, SECTIONS, 32)[8:16] == pytest.approx([i / 8 for i in range(1, 9)])


def test_a_dip_inside_a_measured_rise_never_lowers_the_ramp():
    from edm_analysis.fusion import ramp_levels

    series = [0.5] * 8 + [0.0, 0.3, 0.2, 0.5, 0.45, 0.8, 0.7, 1.0] + [0.5] * 16
    out = ramp_levels({"onset": series}, SECTIONS, 32)[8:16]
    assert all(b >= a for a, b in zip(out, out[1:])) and out[-1] == 1.0


def test_an_entry_that_would_leave_an_intro_over_half_the_track_stays_the_drop():
    """Whatever else turns into intro after the entry goes (demoted drops, an outro the rules put early), the intro never passes INTRO_SHARE."""
    sections = [("intro", 0, 8), ("drop", 8, 16), ("groove", 16, 24), ("drop", 24, 32), ("buildup", 32, 40), ("drop", 40, 72), ("outro", 72, 96)]
    got = fuse(make_doc(sections), {"dropdetector": detector((24, 0.92), (84, 0.78))})
    assert labels(got)[0][2] <= 0.55 * N
    assert [e for e in got["events"] if e["kind"] == "entry"] == [] and any(s["label"] == "drop" for s in got["sections"])
