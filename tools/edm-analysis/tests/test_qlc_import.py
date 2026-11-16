import random

import pytest

from edm_analysis.confidence import _exact, gold_bar
from edm_analysis.qlc_import import edit_probes, import_round, workspace_edits
from edm_analysis.rounds import gold_answers, load_answers, record_answer, report, validate_answer

TID = "abababababababab"
SECTIONS = [("intro", 0, 8), ("buildup", 8, 16), ("drop", 16, 32), ("breakdown", 32, 40), ("drop", 40, 56), ("outro", 56, 64)]
DOC = {"track": {"title": "song", "path": "/music/song.mp3"},
       "grid": {"period": 0.5},
       "bars": {"start": [b * 2.0 for b in range(64)]},
       "sections": [{"label": label, "start_bar": a, "end_bar": b, "start": a * 2.0, "end": b * 2.0, "confidence": 60, "label_confidence": 70}
                    for label, a, b in SECTIONS]}


def pins(*rows):
    return [{"label": label, "start": start, "end": end} for label, start, end in rows]


def summary(p):
    """(claim kind, our bar, QLC+ bar) of a start Probe, (label, first bar, end bar, our label, QLC+ label) of a label Probe."""
    by = {alt["source"]: alt for alt in p["alternatives"].values()}
    if p["claim"]["kind"] == "label":
        return ("label", p["claim"]["bar"], p["claim"]["end_bar"], by["ours"]["label"], by["qlc"]["label"])
    return (p["claim"]["kind"], by["ours"]["bar"], by["qlc"]["bar"])


@pytest.mark.parametrize("edits,expected", [
    (pins(("drop", 32, 64)), []),
    (pins(("drop", 30, 64)), [("drop", 16, 15)]),
    (pins(("drop", 31.9, 64)), []),
    (pins(("buildup", 16, 50), ("drop", 50, 64)), [("drop", 16, 25)]),
    (pins(("drop", 32, 68), ("breakdown", 68, 80)), [("breakdown", 32, 34)]),
    (pins(("drop", 64, 80)), [("label", 32, 40, "breakdown", "drop")]),
    (pins(("buildup", 64, 80)), [("label", 32, 40, "breakdown", "buildup")]),
    (pins(("drop", 32, 80)), [("label", 32, 40, "breakdown", "drop")]),
    (pins(("drop", 0, 16)), [("label", 0, 8, "intro", "drop")]),
    (pins(("buildup", 12, 32)), []),
    (pins(("drop", 32, 48), ("drop", 48, 64)), []),
    (pins(("drop", 30, 48), ("drop", 48, 64)), [("drop", 16, 15)]),
    (pins(("intro", 0, 14)), []),
    (pins(("outro", 108, 128)), []),
    (pins(("drop", 30, 64), ("buildup", 64, 80)), [("drop", 16, 15), ("label", 32, 40, "breakdown", "buildup")]),
], ids=["unchanged", "drop a bar earlier", "drop snaps to its own bar", "drop 9 bars later", "breakdown later", "breakdown called drop", "relabel",
        "merge into drop", "intro called drop", "buildup start is nominal", "split drop is a lift", "split keeps the moved start",
        "intro end has no claim", "outro start has no claim", "two edits"])
def test_only_differences_become_probes(edits, expected):
    assert [summary(p) for p in edit_probes(TID, DOC, edits, random.Random(1))] == expected


@pytest.mark.parametrize("edits", [pins(("drop", 30, 64)), pins(("buildup", 64, 80))], ids=["start", "label"])
def test_a_difference_is_an_ab_probe_outside_fresh_accuracy(edits):
    (p,) = edit_probes(TID, DOC, edits, random.Random(1))
    assert (p["track"], p["title"], p["theory"], p["question"], p["fresh"]) == (TID, "song", "qlc", "ab", False)
    assert sorted(alt["source"] for alt in p["alternatives"].values()) == ["ours", "qlc"]
    assert [play["id"] for play in p["plays"]] == ["a", "b"]
    assert all(play["start"] < play["mark"] < play["end"] for play in p["plays"])
    assert [validate_answer(p, a) for a in ("a", "b", "neither", "skip")] == [None] * 4


@pytest.mark.parametrize("edits,pick,exact,bar", [
    (pins(("drop", 30, 64)), "ours", True, 16),
    (pins(("drop", 30, 64)), "qlc", False, 15),
    (pins(("buildup", 64, 80)), "ours", True, None),
    (pins(("buildup", 64, 80)), "qlc", False, None),
])
def test_answers_are_gold_the_tool_reads(tmp_path, edits, pick, exact, bar):
    from edm_analysis.rounds import save_round

    rnd = import_round(9, {TID: DOC}, {TID: edits}, seed=1, created="2026-11-16T12:00:00")
    save_round(tmp_path, rnd)
    (p,) = rnd["probes"]
    answer = next(k for k, alt in p["alternatives"].items() if alt["source"] == pick)
    assert record_answer(tmp_path, p["id"], answer) == (200, None)
    (record,) = gold_answers(load_answers(tmp_path)).values()
    assert (_exact(record), gold_bar(record)) == (exact, bar)
    assert report([rnd], load_answers(tmp_path), {TID: DOC})["rounds"][0]["fresh"] == 0


def test_import_round_numbers_its_probes():
    other = "cdcdcdcdcdcdcdcd"
    rnd = import_round(9, {TID: DOC, other: DOC}, {TID: pins(("drop", 30, 64)), other: pins(("buildup", 64, 80)), "gone": pins(("drop", 0, 4))},
                       seed=1, created="2026-11-16T12:00:00")
    assert (rnd["round"], rnd["created"]) == (9, "2026-11-16T12:00:00")
    assert [p["id"] for p in rnd["probes"]] == ["r09-p001", "r09-p002"]
    assert {p["track"] for p in rnd["probes"]} == {TID, other}
    assert [t["id"] for t in rnd["theories"]] == ["qlc"]


AUDIO = '<Function ID="{id}" Type="Audio" Name="a"><Source>{path}</Source></Function>'
SHOW = '<Function ID="{id}" Type="Show" Name="s"><Track ID="0" Name="Audio"><ShowFunction ID="{audio}" StartTime="{at}" Duration="9"/></Track>' \
       '<Track ID="1" Name="Song Director"><ShowFunction ID="7" StartTime="0" Duration="9"/></Track></Function>'
PINS = '<Sections Show="{show}"><Pin Label="buildup" Start="37512" End="48762.504000000001"/><Pin Label="drop" Start="48762.504" End="131262"/></Sections>'


def workspace(functions, sections):
    return f'<?xml version="1.0"?><Workspace xmlns="http://www.qlcplus.org/Workspace"><Engine>{functions}</Engine>' \
           f'<SongDirector>{sections}</SongDirector></Workspace>'


@pytest.mark.parametrize("xml,expected", [
    (workspace(AUDIO.format(id=0, path="/m/a.mp3") + SHOW.format(id=1, audio=0, at=0), PINS.format(show=1)),
     [{"path": "/m/a.mp3", "pins": pins(("buildup", 37.512, 48.762504000000001), ("drop", 48.762504, 131.262))}]),
    (workspace(AUDIO.format(id=0, path="/m/a.mp3") + SHOW.format(id=1, audio=0, at=5000), PINS.format(show=1)),
     [{"path": "/m/a.mp3", "pins": pins(("buildup", 37.512, 48.762504000000001), ("drop", 48.762504, 131.262))}]),
    (workspace(AUDIO.format(id=0, path="/m/a.mp3") + SHOW.format(id=1, audio=0, at=0), ""), []),
    (workspace(SHOW.format(id=1, audio=0, at=0), PINS.format(show=1)), []),
    (workspace(AUDIO.format(id=0, path="/m/a.mp3") + SHOW.format(id=1, audio=0, at=0), PINS.format(show=2)), []),
    (workspace(AUDIO.format(id=0, path="/m/a.mp3") + SHOW.format(id=1, audio=0, at=0) + AUDIO.format(id=3, path="/m/b.mp3")
               + SHOW.format(id=4, audio=3, at=0), PINS.format(show=4) + PINS.format(show=1)),
     [{"path": "/m/b.mp3", "pins": pins(("buildup", 37.512, 48.762504000000001), ("drop", 48.762504, 131.262))},
      {"path": "/m/a.mp3", "pins": pins(("buildup", 37.512, 48.762504000000001), ("drop", 48.762504, 131.262))}]),
], ids=["song time", "clip offset does not move song time", "no edits", "no audio", "unknown show", "two songs"])
def test_workspace_edits(xml, expected):
    assert workspace_edits(xml) == expected
