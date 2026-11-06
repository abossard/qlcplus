import numpy as np
import pytest

from edm_analysis.grid import fit_grid

FRAME = 0.02  # Beat This reports times on a 50 fps frame grid


def synth_beats(bpm, start, n, phase, drop=(), extra=(), seed=0):
    rng = np.random.default_rng(seed)
    period = 60.0 / bpm
    idx = np.arange(n)
    times = start + idx * period + rng.normal(0, 0.004, n)
    times = np.round(times / FRAME) * FRAME
    keep = np.ones(n, bool)
    for a, b in drop:
        keep[a:b] = False
    beats = np.sort(np.concatenate([times[keep], np.asarray(extra, float)]))
    downbeats = times[keep & (idx % 4 == phase)]
    true_grid = start + idx * period
    return beats, downbeats, true_grid


@pytest.mark.parametrize(
    "bpm,start,phase,drop,extra",
    [
        (128.0, 0.31, 0, (), ()),
        (122.0, 1.07, 2, (), ()),
        (140.0, 0.05, 1, ((200, 260),), ()),          # breakdown without detected beats
        (174.0, 0.40, 3, ((10, 30), (500, 540)), ()),
        (126.0, 0.55, 0, (), (33.333, 77.777, 120.01)),  # spurious detections
    ],
)
def test_fit_grid_recovers_tempo_phase_and_beat_times(bpm, start, phase, drop, extra):
    beats, downbeats, true_grid = synth_beats(bpm, start, 800, phase, drop, extra)

    grid = fit_grid(beats, downbeats)

    assert grid.bpm == pytest.approx(bpm, abs=0.05)
    first_downbeat = true_grid[phase]
    assert grid.downbeats[0] == pytest.approx(first_downbeat, abs=0.01)
    assert np.max(np.abs(grid.beats - true_grid[: len(grid.beats)])) < 0.01
    assert grid.confidence > 0.8


def test_fit_grid_reports_low_confidence_for_unsteady_tempo():
    t1 = np.arange(0, 60, 60 / 120)
    t2 = t1[-1] + np.arange(1, 120) * 60 / 140
    beats = np.concatenate([t1, t2])

    grid = fit_grid(beats, beats[::4])

    assert grid.confidence < 0.6


@pytest.mark.parametrize(
    "bpm,intro_beats,offbeat_run",
    [
        (123.0, 23, None),          # intro on a different phase, then a gap (ADAPT - Dawn)
        (128.0, 40, None),
        (126.0, 0, (300, 340)),     # tracker on the off-beats for a stretch, then back
        (124.0, 16, (200, 232)),
    ],
)
def test_fit_grid_follows_the_main_grid_across_a_phase_jump(bpm, intro_beats, offbeat_run):
    beats, downbeats, true_grid = synth_beats(bpm, 12.32, 520, 0)
    period = 60.0 / bpm
    if offbeat_run:
        a, b = offbeat_run
        beats = np.concatenate([beats[:a], beats[a:b] + period / 2, beats[b:]])
    intro = 0.38 + np.arange(intro_beats) * period  # half a beat off the main grid, ends before it
    beats = np.concatenate([intro, beats])

    grid = fit_grid(beats, downbeats)

    assert grid.bpm == pytest.approx(bpm, abs=0.05)
    assert grid.beats[-1] == pytest.approx(true_grid[-1], abs=0.01)
    assert np.max(np.abs(grid.beats - true_grid[np.round((grid.beats - 12.32) / period).astype(int)])) < 0.01
    assert grid.confidence > 0.8
