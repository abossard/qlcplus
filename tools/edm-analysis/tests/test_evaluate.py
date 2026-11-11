import pytest

from edm_analysis.evaluate import boundary_scores, label_agreement, silver_boundaries, split_of


@pytest.mark.parametrize("ours,ref,tol,expected", [
    ([16, 24, 56], [16, 24, 56], 0, (1.0, 1.0)),
    ([16, 25, 56], [16, 24, 56], 1, (1.0, 1.0)),
    ([16, 25, 56], [16, 24, 56], 0, (2 / 3, 2 / 3)),
    ([16, 24, 40, 56], [16, 56], 0, (0.5, 1.0)),
    ([], [16], 1, (None, 0.0)),
    ([16, 17], [16], 1, (0.5, 1.0)),   # one reference point matches at most one of ours
])
def test_boundary_scores(ours, ref, tol, expected):
    precision, recall = boundary_scores(ours, ref, tol)
    assert (precision, recall) == pytest.approx(expected) if expected[0] is not None else (precision, recall) == (None, expected[1])


def test_label_agreement_counts_bars_inside_judged_sections():
    ours = [{"label": "intro", "start_bar": 0, "end_bar": 16}, {"label": "drop", "start_bar": 16, "end_bar": 48}]
    gold = [{"label": "intro", "start_bar": 0, "end_bar": 8}, {"label": "buildup", "start_bar": 8, "end_bar": 16},
            {"label": "drop", "start_bar": 32, "end_bar": 40}]

    assert label_agreement(ours, gold) == pytest.approx((8 + 8) / 24)


def test_silver_boundaries_snap_to_bars_and_correct_offsets():
    bars = [i * 2.0 for i in range(40)]
    vdj = {"period": 0.5, "phase": 0.0, "breaks": [(9.5, 32.0)], "cues": [{"name": "Energy 5", "pos": 16.049}, {"name": "Cue 1", "pos": 8.0}]}

    s = silver_boundaries(vdj, bars, mik_offset=0.049)

    assert s["vdj_break_start"] == [5]   # Break sits one beat before the bar
    assert s["vdj_break_end"] == [16]
    assert s["mik_energy"] == [8]


@pytest.mark.parametrize("track_id,expected", [("0abc", "dev"), ("7fff", "dev"), ("8000", "test"), ("fabc", "test")])
def test_split_of(track_id, expected):
    assert split_of(track_id) == expected
