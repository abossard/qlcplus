"""Fit a constant-tempo beat grid to detected beats.

Beat trackers report times on a frame grid and miss beats in breakdowns, so
the raw beat list is not usable for bar and phrase arithmetic. EDM tracks hold
one tempo, so a straight line through the beats (time = t0 + k * period) is
both more precise and gap free. Tracks that change tempo get a low confidence.
"""

from dataclasses import dataclass

import numpy as np

BEATS_PER_BAR = 4
ON_GRID_SECONDS = 0.025


@dataclass(frozen=True)
class Grid:
    bpm: float
    period: float
    beats: np.ndarray        # grid beat times from the first to the last detected beat
    downbeats: np.ndarray    # grid beats that start a bar
    confidence: float        # share of detected beats that sit on the grid


def _initial_period(beats):
    diffs = np.diff(beats)
    diffs = diffs[(diffs > 0.25) & (diffs < 1.0)]
    med = np.median(diffs)
    near = diffs[np.abs(diffs - med) < 0.3 * med]
    return float(np.mean(near))


def _runs(beats, period, patience=8):
    """Number the beats by walking the gaps, so a small period error cannot drift.

    A beat that does not fit is skipped as a spurious detection; after `patience`
    misses in a row the tracker has changed phase (an off-grid intro, off-beats in
    a breakdown), and a new run starts at the first beat that did not fit.
    """
    runs, start = [], 0
    while start < len(beats):
        idx, anchor, misses, i = {start: 0}, start, 0, start + 1
        while i < len(beats) and misses < patience:
            steps = (beats[i] - beats[anchor]) / period
            whole = round(steps)
            if whole > 0 and abs(steps - whole) <= 0.25:
                idx[i], anchor, misses = idx[anchor] + whole, i, 0
            else:
                misses += 1
            i += 1
        runs.append(idx)
        start = anchor + 1 if misses >= patience else len(beats)
    return runs


def _assign_indices(beats, period):
    """Indices from the run whose line puts the most beats of the whole track on the grid."""
    def on_line(run):
        p, t0 = _line_fit(np.array(list(run.values())), beats[list(run)])
        pos = (beats - t0) / p
        idx = np.round(pos).astype(int)
        return np.abs(pos - idx) * p < ON_GRID_SECONDS, idx, pos
    runs = sorted(_runs(beats, period), key=len, reverse=True)
    candidates = [r for r in runs if len(r) >= 16] or runs[:1]
    _, idx, pos = max((on_line(r) for r in candidates), key=lambda c: c[0].sum())
    on = np.abs(pos - idx) <= 0.25
    return np.where(on, idx - idx[on].min(), -1)


def _line_fit(k, t):
    period, t0 = np.polyfit(k, t, 1)
    return float(period), float(t0)


def fit_grid(beats, downbeats):
    beats = np.sort(np.asarray(beats, float))
    downbeats = np.asarray(downbeats, float)
    period = _initial_period(beats)
    idx = _assign_indices(beats, period)
    used = idx >= 0
    period, t0 = _line_fit(idx[used], beats[used])
    for _ in range(3):
        resid = beats - (t0 + idx * period)
        used = (idx >= 0) & (np.abs(resid) < max(ON_GRID_SECONDS, 3 * np.std(resid[idx >= 0])))
        period, t0 = _line_fit(idx[used], beats[used])

    resid = np.abs(beats - (t0 + idx * period))
    on_grid = (idx >= 0) & (resid < ON_GRID_SECONDS)
    confidence = float(on_grid.sum() / len(beats))

    k_first, k_last = int(idx[on_grid].min()), int(idx[on_grid].max())
    ks = np.arange(k_first, k_last + 1)
    grid_beats = t0 + ks * period

    phase = _downbeat_phase(downbeats, t0, period)
    return Grid(
        bpm=60.0 / period,
        period=period,
        beats=grid_beats,
        downbeats=grid_beats[(ks - phase) % BEATS_PER_BAR == 0],
        confidence=confidence,
    )


def _downbeat_phase(downbeats, t0, period):
    if len(downbeats) == 0:
        return 0
    pos = (downbeats - t0) / period
    k = np.round(pos).astype(int)
    close = np.abs((pos - k) * period) < 2 * ON_GRID_SECONDS
    if not close.any():
        return 0
    votes = np.bincount(k[close] % BEATS_PER_BAR, minlength=BEATS_PER_BAR)
    return int(np.argmax(votes))
