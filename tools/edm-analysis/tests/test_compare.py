import json

import numpy as np

import pytest

from edm_analysis.compare import compare, format_table


def c(track, source, kind, bar, score=None):
    return {"track": track, "source": source, "kind": kind, "bar": bar, "score": score}


CLAIMS = [
    # A and B are covered by every drop source; C has no drop detector output
    *[c("A", "vdj", "break_end", b) for b in (16, 48)], *[c("A", "rules", "drop", b) for b in (16, 49, 80)],
    *[c("A", "edmformer", "drop", b) for b in (16, 48)], c("A", "dropdetector", "drop", 16, 0.9), c("A", "dropdetector", "drop", 30, 0.49),
    c("B", "vdj", "break_end", 32), c("B", "rules", "drop", 33), c("B", "rules", "drop", 35), c("B", "edmformer", "drop", 32),
    c("B", "dropdetector", "drop", 33, 0.5),
    c("C", "vdj", "break_end", 8), c("C", "rules", "drop", 8), c("C", "edmformer", "drop", 8),
    # section starts; bar 0 is the track start, not a start
    *[c("A", "rules", "section", b) for b in (0, 8, 12, 16)], *[c("A", "allin1", "section", b) for b in (10, 16)],
    *[c("A", "edmformer", "section", b) for b in (16, 32)],
]
ANSWERS = [
    {"track": "A", "kind": "drop", "gold_bar": 48}, {"track": "B", "kind": "drop", "gold_bar": 32},
    {"track": "A", "kind": "drop", "gold_bar": None}, {"track": "A", "kind": "label", "gold_bar": None},
]


def test_every_source_says_how_many_tracks_it_covers():
    assert compare(CLAIMS)["coverage"] == {"allin1": 1, "dropdetector": 2, "edmformer": 3, "rules": 3, "vdj": 3}


@pytest.mark.parametrize("name,exact,within_1", [
    ("rules", (1 / 3, 1 / 5), (1.0, 3 / 5)),
    ("edmformer", (1.0, 1.0), (1.0, 1.0)),
    ("dropdetector", (1 / 3, 1 / 2), (2 / 3, 1.0)),                      # the 0.49 candidate is no drop
    ("edmformer+dropdetector", (1 / 3, 1.0), (1 / 3, 1.0)),               # both on the same bar
    ("two of three", (1 / 3, 1 / 2), (2 / 3, 1.0)),
])
def test_drops_are_scored_against_break_ends_on_tracks_every_source_covers(name, exact, within_1):
    out = compare(CLAIMS)["drops"]
    assert out["tracks"] == 2
    for key, (recall, precision) in (("exact", exact), ("within_1", within_1)):
        assert out["scores"][name][key]["recall"] == pytest.approx(recall, abs=1e-3)
        assert out["scores"][name][key]["precision"] == pytest.approx(precision, abs=1e-3)
    assert out["scores"][name]["exact"]["n_ref"] == 3


def test_scores_count_predictions_and_references():
    rules = compare(CLAIMS)["drops"]["scores"]["rules"]["exact"]
    assert (rules["n_ref"], rules["n_pred"]) == (3, 5)


@pytest.mark.parametrize("name,offsets", [
    ("rules", {0: 1, 1: 2}),
    ("edmformer", {0: 3}),
    ("dropdetector", {0: 1, 1: 1}),     # nothing within 4 bars of the break end at bar 48
])
def test_the_nearest_drop_of_each_source_is_counted_per_offset_from_the_break_end(name, offsets):
    assert compare(CLAIMS)["drops"]["offsets"][name] == offsets


@pytest.mark.parametrize("drops,offset", [
    ([15, 17], -1),      # a tie goes to the earlier drop
    ([20], 4),
    ([12], -4),
    ([21], None),
    ([10, 18], 2),
    ([0], None),         # bar 0 is the track start
])
def test_only_drops_within_four_bars_of_a_break_end_have_an_offset(drops, offset):
    claims = [c("A", s, "x", 1) for s in ("vdj", "edmformer", "dropdetector")] + [c("A", "vdj", "break_end", 16)]
    claims += [c("A", "rules", "drop", b) for b in drops]
    claims += [c("A", "edmformer", "drop", 16), c("A", "dropdetector", "drop", 16, 0.9)]
    assert compare(claims)["drops"]["offsets"]["rules"] == ({} if offset is None else {offset: 1})


def test_the_track_start_is_no_drop_and_no_break_end():
    claims = [c("A", "vdj", "break_end", 0), c("A", "vdj", "break_end", 2), c("A", "rules", "drop", 0), c("A", "rules", "drop", 3),
              c("A", "edmformer", "drop", 3), c("A", "dropdetector", "drop", 3, 0.9)]
    out = compare(claims)["drops"]
    assert out["scores"]["rules"]["exact"]["n_pred"] == 1 and out["scores"]["rules"]["exact"]["n_ref"] == 1
    assert out["offsets"]["rules"] == {1: 1}


@pytest.mark.parametrize("extra,tracks", [
    ([], 2),                                            # VirtualDJ has the track but no break end
    ([c("D", "vdj", "break_start", 5)], 2),             # a break start is no break end
    ([c("D", "vdj", "break_end", 0)], 2),               # nor is the track start
    ([c("D", "vdj", "break_end", 12)], 3),
])
def test_only_tracks_with_a_break_end_count_in_the_drop_table_and_its_offsets(extra, tracks):
    every_source = [c("D", s, "drop", 14, 0.9) for s in ("rules", "edmformer", "dropdetector")] + [c("D", "vdj", "bar1", 0)]
    out = compare([*CLAIMS, *every_source, *extra])["drops"]
    assert out["tracks"] == tracks
    assert out["scores"]["rules"]["exact"]["n_pred"] == 5 + (tracks == 3)
    assert sum(out["offsets"]["rules"].values()) == 3 + (tracks == 3)


def test_tracks_without_every_drop_source_are_left_out_of_the_drop_scores():
    out = compare([x for x in CLAIMS if x["track"] != "B"])["drops"]
    assert out["tracks"] == 1


@pytest.mark.parametrize("source,n,on_8", [("rules", 3, 2 / 3), ("allin1", 2, 1 / 2), ("edmformer", 2, 1.0)])
def test_section_starts_on_a_multiple_of_eight_bars(source, n, on_8):
    out = compare(CLAIMS)["section_starts"][source]
    assert out["n"] == n and out["on_8"] == pytest.approx(on_8, abs=1e-3)


@pytest.mark.parametrize("name,n,exact,within_1", [
    ("rules", 2, 0.0, 1.0), ("edmformer", 2, 1.0, 1.0), ("dropdetector", 2, 0.0, 0.5), ("vdj", 2, 1.0, 1.0)])
def test_the_gold_bars_score_each_source(name, n, exact, within_1):
    out = compare(CLAIMS, ANSWERS)["gold"]["drop"][name]
    assert (out["n"], out["exact"], out["within_1"]) == (n, exact, within_1)


@pytest.mark.parametrize("answers", [[], [{"track": "A", "kind": "drop", "gold_bar": None}], [{"track": "A", "kind": "drop"}]])
def test_without_gold_bars_there_is_no_gold_section(answers):
    assert compare(CLAIMS, answers)["gold"] is None


def test_a_gold_answer_counts_only_for_sources_that_cover_the_track():
    out = compare(CLAIMS, [{"track": "C", "kind": "drop", "gold_bar": 8}])["gold"]["drop"]
    assert out["dropdetector"]["n"] == 0 and out["dropdetector"]["exact"] is None and out["rules"]["n"] == 1


def test_the_table_names_every_section_and_source():
    text = format_table(compare(CLAIMS, ANSWERS))
    for part in ("tracks covered", "drops against VirtualDJ break ends", "offset", "section starts", "Gold",
                 "rules", "edmformer", "dropdetector", "allin1", "two of three"):
        assert part in text
    assert "None" not in text


def test_the_table_has_no_gold_part_without_gold():
    assert "Gold" not in format_table(compare(CLAIMS))


def test_the_command_asks_for_an_export_when_the_claims_table_is_missing(tmp_path, capsys):
    from edm_analysis.cli import main

    assert main(["compare", "--out", str(tmp_path)]) == 1
    assert "export" in capsys.readouterr().err


@pytest.mark.parametrize("as_json", [False, True])
def test_the_command_prints_a_table_or_json_from_the_exported_tables(tmp_path, capsys, as_json):
    import pyarrow as pa
    import pyarrow.parquet as pq

    from edm_analysis.cli import main

    (tmp_path / "tables").mkdir()
    pq.write_table(pa.Table.from_pylist(CLAIMS), tmp_path / "tables" / "claims.parquet")
    pq.write_table(pa.Table.from_pylist(ANSWERS), tmp_path / "tables" / "answers.parquet")

    assert main(["compare", "--out", str(tmp_path), *(["--json"] if as_json else [])]) == 0

    out = capsys.readouterr().out
    if as_json:
        assert json.loads(out)["drops"]["tracks"] == 2 and json.loads(out)["gold"]["drop"]["rules"]["n"] == 2
    else:
        assert "tracks covered" in out and "Gold" in out


# --- the fused structure ---

def sec(track, source, bar, label):
    return {**c(track, source, "section", bar), "label": label}


FUSED_CLAIMS = [
    # track F: the rules call a buildup at 8 and a drop at 16 (the drop is a phrase early), the fusion moves the drop to 24 and starts the buildup at 14
    *[sec("F", "rules", b, l) for b, l in ((0, "intro"), (8, "buildup"), (16, "drop"), (40, "outro"))],
    *[sec("F", "fused", b, l) for b, l in ((0, "intro"), (14, "buildup"), (24, "drop"), (40, "groove"))],
    c("F", "rules", "drop", 16), c("F", "fused", "drop", 24), c("F", "rules", "buildup", 8), c("F", "fused", "buildup", 14),
    c("F", "dropdetector", "drop", 24, 0.97), c("F", "dropdetector", "drop", 30, 0.2), c("F", "edmformer", "drop", 25),
    # track G: no buildup before the rules' drop, one before the fused drop
    *[sec("G", "rules", b, l) for b, l in ((0, "intro"), (16, "drop"))], *[sec("G", "fused", b, l) for b, l in ((0, "intro"), (8, "buildup"), (16, "drop"))],
    c("G", "rules", "drop", 16), c("G", "fused", "drop", 16), c("G", "fused", "buildup", 8), c("G", "dropdetector", "drop", 16, 0.9),
]
FUSED_ANSWERS = [{"track": "F", "kind": "drop", "gold_bar": 24}, {"track": "F", "kind": "buildup", "gold_bar": 14}, {"track": "G", "kind": "drop", "gold_bar": 17},
                 {"track": "F", "kind": "breakdown", "gold_bar": 30}]


def test_fused_claims_are_a_source_of_their_own():
    assert compare(FUSED_CLAIMS)["coverage"]["fused"] == 2


@pytest.mark.parametrize("source,offsets", [("rules", {-8: 1, 0: 1}), ("fused", {0: 2}), ("edmformer", {1: 1})])
def test_the_offset_of_each_sources_drops_from_the_detectors_sure_drops(source, offsets):
    # the detector's 0.2 drop at 30 is no drop; G has no EDMFormer output, so it counts for neither
    assert compare(FUSED_CLAIMS)["detector_offsets"][source] == offsets


@pytest.mark.parametrize("kind,source,n,exact,within_1", [
    ("drop", "rules", 2, 0.0, 0.5), ("drop", "fused", 2, 0.5, 1.0), ("drop", "dropdetector", 2, 0.5, 1.0),
    ("buildup", "rules", 1, 0.0, 0.0), ("buildup", "fused", 1, 1.0, 1.0),
    ("breakdown", "rules", 1, 0.0, 0.0),
])
def test_gold_bars_are_scored_per_kind(kind, source, n, exact, within_1):
    out = compare(FUSED_CLAIMS, FUSED_ANSWERS)["gold"][kind][source]
    assert (out["n"], out["exact"], out["within_1"]) == (n, exact, within_1)


@pytest.mark.parametrize("kind", ["drop", "buildup", "breakdown"])
def test_gold_shares_come_with_what_they_mean_under_the_reviewers_error_rate(kind):
    from edm_analysis.confidence import corrected_rate

    rows = [r for r in compare(FUSED_CLAIMS, FUSED_ANSWERS)["gold"][kind].values() if r["n"]]
    assert rows
    for r in rows:
        assert r["exact_corrected"] == pytest.approx(corrected_rate(r["exact"])) and r["within_1_corrected"] == pytest.approx(corrected_rate(r["within_1"]))


def test_the_structure_counts_drops_with_a_buildup_and_the_buildup_lengths():
    out = compare(FUSED_CLAIMS)["structure"]
    assert out["rules"] == {"drops": 2, "with_buildup": 1, "buildup_lengths": {8: 1}, "after_drop": 0, "buildups": 1, "unsupported": 0, "lifts": 0, "entries": 1, "entry_events": 0}     # G has no buildup before its drop
    assert out["fused"] == {"drops": 2, "with_buildup": 2, "buildup_lengths": {8: 1, 10: 1}, "after_drop": 0, "buildups": 2, "unsupported": 0, "lifts": 0, "entries": 0, "entry_events": 0}


def test_the_structure_counts_drops_after_drops_buildups_without_a_source_and_lifts():
    claims = [*[{**sec("H", "fused", b, l), "sources": s} for b, l, s in ((0, "intro", ""), (8, "drop", "rules"), (16, "drop", "rules"), (24, "buildup", ""), (30, "buildup", "rules+edmformer"), (36, "drop", "x"))],
              {**c("H", "fused", "lift", 20), "label": "lift", "sources": "x"}, {**c("H", "fused", "lift", 22), "label": "lift", "sources": ""},
              *[sec("H", "rules", b, l) for b, l in ((0, "intro"), (8, "drop"), (16, "drop"))]]
    out = compare(claims)["structure"]
    assert (out["fused"]["after_drop"], out["fused"]["buildups"], out["fused"]["unsupported"], out["fused"]["lifts"]) == (1, 2, 1, 2)
    assert (out["rules"]["after_drop"], out["rules"]["buildups"], out["rules"]["lifts"]) == (1, 0, 0)
    assert (out["fused"]["entries"], out["rules"]["entries"]) == (2, 2)     # both drops come before any breakdown or buildup


def test_label_changes_name_the_rules_sections_the_fusion_labels_differently():
    assert compare(FUSED_CLAIMS)["label_changes"] == {"sections": 6, "changed": 3, "from_to": {"buildup>intro": 1, "drop>buildup": 1, "outro>groove": 1}}


def test_the_table_shows_the_fusion_numbers():
    text = format_table(compare(FUSED_CLAIMS, FUSED_ANSWERS))
    for part in ("fused", "drops with a buildup before them", "buildup length", "label changes", "drop>buildup",
                 "Gold bars of buildups", "Gold bars of drops", "sure drop detector drop", "reviewer"):
        assert part in text, part
    assert "None" not in text


def test_a_track_is_printed_as_rules_fused_events_and_answers_side_by_side():
    from edm_analysis.compare import side_by_side

    doc = {"track": {"title": "Some Song"},
           "rules": {"sections": [{"label": "intro", "start_bar": 0, "end_bar": 16}, {"label": "drop", "start_bar": 16, "end_bar": 40}]},
           "sections": [{"label": "intro", "start_bar": 0, "end_bar": 14, "confidence": 100, "sources": ["rules"]},
                        {"label": "buildup", "start_bar": 14, "end_bar": 24, "confidence": 55, "sources": [], "nominal": True},
                        {"label": "drop", "start_bar": 24, "end_bar": 40, "confidence": 91, "sources": ["dropdetector", "edmformer"]}],
           "events": [{"kind": "entry", "bar": 8, "time": 16.0}, {"kind": "drop", "bar": 24, "time": 48.0}, {"kind": "lift", "bar": 32, "time": 64.0}, {"kind": "vocal_in", "bar": 3, "time": 6.0}]}
    answers = [{"kind": "drop", "bar": 16, "answer": "bar:24", "gold_bar": 24}, {"kind": "label", "bar": 14, "answer": "no", "gold_bar": None}]

    lines = side_by_side(doc, answers).splitlines()

    assert lines[0] == "Some Song" and lines[1].split() == ["rules", "fused", "events", "answers"]
    rows = [[col.strip() for col in line.split("|")] for line in lines[2:]]
    assert rows[0][:3] == ["intro 0-16", "intro 0-14 100% rules", "entry 8"]
    assert rows[1][:3] == ["drop 16-40", "buildup ~14-24 55%", "lift 32"]            # ~ marks a nominal start
    assert rows[2][1] == "drop 24-40 91% dropdetector+edmformer"
    assert rows[0][3] == "label 14: no" and rows[1][3] == "drop 16: bar:24 (gold bar 24)"


# --- rejected Claims ---

REJECTED_CLAIMS = [*[c("R", s, "drop", 16) for s in ("rules", "fused", "edmformer")], c("R", "dropdetector", "drop", 16, 0.9),
                   c("R", "rules", "drop", 40), c("R", "rules", "buildup", 30), c("R", "fused", "buildup", 32), c("R", "edmformer", "buildup", 31),
                   c("Q", "rules", "drop", 16), c("Q", "fused", "drop", 17)]
REJECTED_ANSWERS = [
    {"track": "R", "kind": "drop", "bar": 16, "answer": "no", "exact": False, "gold_bar": None},          # rejected, and every source still makes it
    {"track": "R", "kind": "drop", "bar": 40, "answer": "none", "exact": False, "gold_bar": None},        # only the rules do
    {"track": "R", "kind": "buildup", "bar": 32, "answer": "none", "exact": False, "gold_bar": None},     # no buildup: the sources have starts one or two bars off
    {"track": "Q", "kind": "drop", "bar": 16, "answer": "no", "exact": False, "gold_bar": None},
    {"track": "R", "kind": "buildup", "bar": 48, "answer": "none", "exact": True, "gold_bar": None},      # confirms there is none: not a rejected Claim
    {"track": "R", "kind": "drop", "bar": 24, "answer": "bar:20", "exact": False, "gold_bar": 20},        # a named bar is no rejection
    {"track": "R", "kind": "label", "bar": 16, "answer": "no", "exact": False, "gold_bar": None},         # labels are no start Claims
    {"track": "R", "kind": "drop", "bar": 56, "answer": "skip", "exact": None, "gold_bar": None},
]


@pytest.mark.parametrize("kind,source,n,exact,within_2", [
    ("drop", "rules", 3, 1.0, 1.0), ("drop", "fused", 3, 1 / 3, 2 / 3), ("drop", "edmformer", 2, 0.5, 0.5), ("drop", "dropdetector", 2, 0.5, 0.5),
    ("buildup", "rules", 1, 0.0, 1.0), ("buildup", "fused", 1, 1.0, 1.0), ("buildup", "edmformer", 1, 0.0, 1.0),
])
def test_rejected_claims_show_how_many_each_source_still_makes_on_that_bar(kind, source, n, exact, within_2):
    out = compare(REJECTED_CLAIMS, REJECTED_ANSWERS)["rejected"][kind][source]
    assert (out["n"], out["exact"], out["within_2"]) == (n, pytest.approx(exact, abs=1e-3), pytest.approx(within_2, abs=1e-3))


def test_there_is_no_rejected_table_without_rejections():
    assert compare(REJECTED_CLAIMS, REJECTED_ANSWERS[4:])["rejected"] is None


def test_the_table_lists_the_rejected_claims_and_the_structure_counts():
    text = format_table(compare([*FUSED_CLAIMS, *REJECTED_CLAIMS], REJECTED_ANSWERS))
    for part in ("rejected", "still make", "drops after a drop", "lifts"):
        assert part in text, part


def test_mean_energy_per_label_shows_drops_above_breakdowns():
    claims = [{**sec("E", "fused", b, l), "energy": e} for b, l, e in ((0, "intro", 3), (16, "drop", 10), (48, "breakdown", 4), (64, "drop", 9), (96, "outro", 2))]
    claims += [sec("E", "rules", 0, "intro")]
    out = compare(claims)["energy"]
    assert out == {"breakdown": {"n": 1, "mean": 4.0}, "drop": {"n": 2, "mean": 9.5}, "intro": {"n": 1, "mean": 3.0}, "outro": {"n": 1, "mean": 2.0}}
    text = format_table(compare(claims))
    assert "mean energy per label" in text and "9.5" in text


def test_no_energy_table_for_tables_exported_without_it():
    assert compare(FUSED_CLAIMS)["energy"] is None and "mean energy" not in format_table(compare(FUSED_CLAIMS))


# --- buildup existence ------------------------------------------------------------------------

EXIST_CLAIMS = [
    c("X", "rules", "buildup", 8), c("X", "fused", "buildup", 8), c("X", "edmformer", "buildup", 6), *[c("X", s, "drop", 16) for s in ("rules", "fused", "edmformer")],
    c("Y", "fused", "buildup", 24), *[c("Y", s, "drop", 32) for s in ("rules", "fused", "edmformer")],
    *[c("Z", s, "drop", 48) for s in ("rules", "fused", "edmformer")],
    c("W", "fused", "buildup", 2), c("W", "fused", "drop", 40),            # a buildup 38 bars before the drop is another one
]
EXIST_ANSWERS = [
    {"track": "X", "kind": "buildup_exists", "exists": True, "drop_bar": 16, "claimed": True},
    {"track": "Y", "kind": "buildup_exists", "exists": False, "drop_bar": 32, "claimed": True},
    {"track": "Z", "kind": "buildup", "exists": True, "drop_bar": 48, "claimed": None},
    {"track": "W", "kind": "buildup", "exists": False, "drop_bar": 40, "claimed": None},
    {"track": "X", "kind": "buildup", "exists": None, "drop_bar": None},      # no word about existence
]


@pytest.mark.parametrize("source,expected", [
    ("fused", {"n": 4, "confirmed": 1, "rejected": 1, "missed": 1, "agreed_none": 1}),
    ("rules", {"n": 3, "confirmed": 1, "rejected": 0, "missed": 1, "agreed_none": 1}),         # W has no rules claims at all
    ("edmformer", {"n": 3, "confirmed": 1, "rejected": 0, "missed": 1, "agreed_none": 1}),
])
def test_buildup_existence_counts_claimed_and_confirmed_rejected_missed_and_agreed_none(source, expected):
    got = compare(EXIST_CLAIMS, EXIST_ANSWERS)["existence"][source]
    assert {k: got[k] for k in expected} == expected
    assert got["right"] == pytest.approx((expected["confirmed"] + expected["agreed_none"]) / expected["n"], abs=1e-3)


def test_there_is_no_existence_table_without_existence_answers():
    assert compare(EXIST_CLAIMS, EXIST_ANSWERS[-1:])["existence"] is None


def test_the_table_prints_buildup_existence_and_entry_events():
    text = format_table(compare([*FUSED_CLAIMS, *EXIST_CLAIMS, {**c("F", "fused", "entry", 8), "label": "entry"}], EXIST_ANSWERS))
    for part in ("buildup existence", "claimed and confirmed", "entry events"):
        assert part in text, part


# --- what in the sound separates the reviewer's answers ---------------------------------------------

def bars_of(track, n=48, **series):
    """Per-bar rows of one track: every level 0.5 unless given."""
    keys = ("kick", "bass", "loud", "centroid", "inst_centroid", "onset", "hihat_onset")
    return [{"track": track, "bar": k, **{key: series.get(key, [0.5] * n)[k] for key in keys}} for k in range(n)]


def test_the_sound_before_a_drop_is_read_as_rises_dropouts_and_a_dip():
    from edm_analysis.compare import sound_before

    rising = [0.5] * 24 + [i / 8 for i in range(8)] + [0.5] * 16
    rows = bars_of("T", centroid=rising, kick=[0.9] * 28 + [0.0] * 4 + [0.9] * 16, loud=[0.8] * 30 + [0.2] * 2 + [0.9] * 16)
    got = sound_before({r["bar"]: r for r in rows}, 32)
    assert got["centroid rise 8"] == pytest.approx(1.0, abs=0.05) and got["centroid rise 4"] > 0.3 and got["onset rise 8"] == 0.0
    assert got["kick out 8"] == pytest.approx(1.0) and got["bass out 8"] == pytest.approx(0.5)
    assert got["loud dip 2"] == pytest.approx(0.6)


def test_missing_levels_read_as_nothing_rather_than_nan():
    from edm_analysis.compare import sound_before

    rows = bars_of("T", centroid=[None] * 48)
    assert all(v == v for v in sound_before({r["bar"]: r for r in rows}, 32).values())


@pytest.mark.parametrize("noise,at_least,at_most", [(0.0, 1.0, 1.0), (10.0, 0.0, 0.95)])
def test_each_feature_gets_a_held_out_accuracy_and_the_best_one_is_chosen_inside_each_fold(noise, at_least, at_most):
    from edm_analysis.compare import separation

    rng = np.random.default_rng(0)
    rows = [(f"t{i}", {"clear": float(i % 2) + noise * rng.normal(), "noise": rng.normal()}, bool(i % 2)) for i in range(30)]
    out = separation(rows)
    assert out["n"] == 30 and out["yes"] == 15
    assert at_least <= out["features"]["clear"]["held_out"] <= at_most
    assert set(out["rules"]) >= {"majority", "best feature chosen per fold", "logistic of all features"}
    assert out["features"]["clear"]["yes_mean"] > out["features"]["clear"]["no_mean"] or noise


def test_one_track_with_several_answers_is_held_out_as_a_whole():
    from edm_analysis.compare import separation

    rows = [("same", {"f": 1.0}, True), ("same", {"f": 1.0}, True), ("other", {"f": 0.0}, False), ("third", {"f": 0.0}, False)]
    assert separation(rows)["rules"]["majority"] == 0.0     # held out together, both "same" answers meet an all-"no" rest; split up, one would vote for the other


def test_compare_reads_the_sound_before_each_answered_drop_and_scores_the_current_rule():
    claims = [*EXIST_CLAIMS]
    bars = [*bars_of("X"), *bars_of("Y"), *bars_of("Z", n=60), *bars_of("W")]
    out = compare(claims, EXIST_ANSWERS, bars)["buildup_sound"]
    assert out["n"] == 4 and out["yes"] == 2
    assert out["rules"]["current rule (fused)"] == pytest.approx(0.5)
    assert "centroid rise 8" in out["features"]
    assert compare(claims, EXIST_ANSWERS)["buildup_sound"] is None
    assert "what in the sound" in format_table(compare(claims, EXIST_ANSWERS, bars))


def test_breakdown_starts_are_read_as_kick_and_bass_falling_away_at_the_claimed_bar():
    from edm_analysis.compare import fall_at

    rows = bars_of("T", kick=[0.9] * 16 + [0.1] * 32, bass=[0.8] * 16 + [0.8] * 32, loud=[0.9] * 16 + [0.5] * 32)
    got = fall_at({r["bar"]: r for r in rows}, 16)
    assert got["kick fall"] == pytest.approx(0.8) and got["bass fall"] == pytest.approx(0.0) and got["loud fall"] == pytest.approx(0.4)
    assert got["strongest fall within 8 bars"] == 0
