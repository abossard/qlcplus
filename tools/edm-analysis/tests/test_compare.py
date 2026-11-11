import json

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
    out = compare(CLAIMS, ANSWERS)["gold"][name]
    assert (out["n"], out["exact"], out["within_1"]) == (n, exact, within_1)


@pytest.mark.parametrize("answers", [[], [{"track": "A", "kind": "drop", "gold_bar": None}], [{"track": "A", "kind": "drop"}]])
def test_without_gold_bars_there_is_no_gold_section(answers):
    assert compare(CLAIMS, answers)["gold"] is None


def test_a_gold_answer_counts_only_for_sources_that_cover_the_track():
    out = compare(CLAIMS, [{"track": "C", "kind": "drop", "gold_bar": 8}])["gold"]
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
        assert json.loads(out)["drops"]["tracks"] == 2 and json.loads(out)["gold"]["rules"]["n"] == 2
    else:
        assert "tracks covered" in out and "Gold" in out
