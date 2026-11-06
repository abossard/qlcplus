import numpy as np
import pytest

from edm_analysis.vdj import grid_agreement, parse_database

DB = """<?xml version="1.0" encoding="UTF-8"?>
<VirtualDJ_Database Version="8">
 <Song FilePath="/m/a.mp3" FileSize="1">
  <Scan Version="801" Bpm="0.468750" Phase="0.250000" Key="Am" />
  <Poi Name="Energy 5" Pos="15.25" Num="1" Type="cue" />
  <Poi Pos="0.25" Type="beatgrid" />
  <Poi Name="Break 1" Pos="59.5" Type="remix" />
  <Poi Name="End Break 1" Pos="90.0" Type="remix" />
 </Song>
 <Song FilePath="/m/b.mp3" FileSize="1">
  <Infos SongLength="100" />
 </Song>
</VirtualDJ_Database>
"""


def test_parse_database_reads_grid_cues_and_breaks(tmp_path):
    p = tmp_path / "database.xml"
    p.write_text(DB)

    songs = parse_database(p)

    a = songs["/m/a.mp3"]
    assert a["period"] == pytest.approx(0.46875) and a["phase"] == pytest.approx(0.25)
    assert a["cues"] == [{"name": "Energy 5", "pos": 15.25}]
    assert a["breaks"] == [(59.5, 90.0)]
    assert songs["/m/b.mp3"]["period"] is None


@pytest.mark.parametrize("our_offset,our_bar_shift,expect_offset_ms,expect_bar_phase", [
    (0.0, 0, 0.0, 0),
    (0.012, 0, 12.0, 0),
    (-0.020, 1, -20.0, 1),
    (0.0, 3, 0.0, 3),
])
def test_grid_agreement(our_offset, our_bar_shift, expect_offset_ms, expect_bar_phase):
    period, phase = 60 / 128, 0.25
    beats = phase + np.arange(400) * period + our_offset
    bar_starts = beats[our_bar_shift::4]

    g = grid_agreement(beats, bar_starts, 128.0, period, phase)

    assert g["bpm_diff"] == pytest.approx(0.0, abs=1e-6)
    assert g["beat_offset_ms"] == pytest.approx(expect_offset_ms, abs=0.5)
    assert g["bar_phase"] == expect_bar_phase
