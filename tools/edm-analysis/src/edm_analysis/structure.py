"""EDM section labels and events from per-bar features.

Input is one value per bar for each stem level (kick, hihat, bass, instruments,
vocal) and the mix ("loud"), scaled 0..1 within the track, plus optional
descriptors (brightness, onset density, MFCC timbre). The method follows the
sources in reports/EDM structure reliable baseline.md:
- boundaries are novelty peaks of the per-bar features on the 4-bar grid,
  favoured on 8-bar phrase lines (Foote novelty; phrase statistics of
  EDM-CUE, Raveform and the VirtualDJ / Mixed In Key markers of this library),
  plus every change of bass presence (Solberg: the drop is where bass and kick return);
- a section is a drop ("core") when it is louder than the track average and
  kick or bass are back (Vande Veire & De Bie 2018; Solberg 2014), and it comes
  after a break: loud sections that open a track are the (DJ) intro;
- the stretch right before a drop that lacks kick or bass, or rises, is the
  buildup; between drops the rest is breakdown (bass gone) or groove.
"""

from dataclasses import dataclass, field

import numpy as np
from scipy.ndimage import median_filter

UNIT = 4                 # bars; section boundaries snap to this grid
KICK_ON = 0.4
VOCAL_ON = 0.4
SPLIT_NOVELTY = 0.3      # feature distance that starts a new section inside a kick run
DROP_LOUDNESS = 0.8      # share of the loudest kick section a drop must reach
DROP_BASS = 0.5
TAIL_CONTRAST = 0.3      # head/tail distance that splits a breakdown from its buildup
FILL_BARS = 5            # median window that hides 1-2 bar kick fills at phrase ends
GROOVE_BASS = 0.3        # a kick section between drops needs this much bass to be groove, else breakdown
NOVELTY_FEATURES = ("hihat", "bass", "instruments", "vocal", "loud")


@dataclass(frozen=True)
class Section:
    start_bar: int
    end_bar: int
    label: str


@dataclass(frozen=True)
class Event:
    bar: int
    kind: str


def _runs(mask):
    """(start, end, value) for each run of equal values."""
    edges = np.flatnonzero(np.diff(mask.astype(int))) + 1
    bounds = [0, *edges.tolist(), len(mask)]
    return [(a, b, bool(mask[a])) for a, b in zip(bounds[:-1], bounds[1:])]


def _anchor(change_bars):
    """Phrase offset in bars. Stays 0 (the first downbeat) unless most changes agree on another offset,
    because a fill bar before a section makes its change land one bar early."""
    if not change_bars:
        return 0
    votes = np.bincount(np.asarray(change_bars) % UNIT, minlength=UNIT)
    best = int(np.argmax(votes))
    return best if votes[best] > 1.5 * votes[0] else 0


def _snap(bar, anchor, n):
    snapped = anchor + UNIT * round((bar - anchor) / UNIT)
    return int(min(max(snapped, 0), n))


def _kick_regions(kick, anchor):
    n = len(kick)
    on = kick > KICK_ON
    cuts = sorted({_snap(a, anchor, n) for a, _, _ in _runs(on)} | {0, n})
    regions = []
    for a, b in zip(cuts[:-1], cuts[1:]):
        if b > a:
            regions.append((a, b, bool(np.mean(on[a:b]) > 0.5)))
    return _merge_same(regions)


def _merge_same(regions):
    merged = []
    for a, b, v in regions:
        if merged and merged[-1][2] == v:
            merged[-1] = (merged[-1][0], b, v)
        else:
            merged.append((a, b, v))
    return merged


def _features(bars):
    return np.stack([bars[k] for k in NOVELTY_FEATURES], axis=1)


def _split_by_novelty(feat, a, b, anchor):
    """Cut a kick run where the stem mix changes, on 4-bar boundaries."""
    window = 2 * UNIT
    first = _snap(a + UNIT, anchor, b)
    candidates = [c for c in range(first, b - UNIT + 1, UNIT) if c - a >= UNIT and b - c >= UNIT]
    scores = []
    for c in candidates:
        left = feat[max(a, c - window):c].mean(axis=0)
        right = feat[c:min(b, c + window)].mean(axis=0)
        scores.append(float(np.linalg.norm(left - right)))
    cuts = [a]
    for i, c in enumerate(candidates):
        peak = all(scores[i] >= scores[j] for j in (i - 1, i + 1) if 0 <= j < len(scores))
        if peak and scores[i] > SPLIT_NOVELTY and c - cuts[-1] >= UNIT:
            cuts.append(c)
    cuts.append(b)
    return list(zip(cuts[:-1], cuts[1:]))


def _tail_split(feat, a, b):
    """Split a pre-drop stretch into (head, buildup tail); head may be empty."""
    if b - a <= 2 * UNIT:
        return None, (a, b)
    best, best_score, best_contrast = None, 0.0, 0.0
    for s in range(a + UNIT, b - UNIT + 1, UNIT):
        contrast = float(np.linalg.norm(feat[a:s].mean(axis=0) - feat[s:b].mean(axis=0)))
        score = (s - a) * (b - s) / (b - a) * contrast ** 2  # between-segment variance, favours clean halves
        if score > best_score:
            best, best_score, best_contrast = s, score, contrast
    if best is None or best_contrast < TAIL_CONTRAST:
        best = b - 2 * UNIT
    return (a, best), (best, b)


def _rising(bars, a, b):
    if b - a < UNIT:
        return False
    x = np.arange(b - a)
    rise = np.polyfit(x, bars["loud"][a:b], 1)[0] * (b - a)
    return rise > 0.15


@dataclass(frozen=True)
class Params:
    # defaults tuned on the dev half of the library against VirtualDJ breaks and
    # Mixed In Key cues, then checked on the held-out half (.session-notes/edm-analysis/process.md)
    window: int = 8             # bars on each side of a novelty comparison
    threshold: float = 0.6      # novelty peak needed, relative to the track's strongest
    phrase_bonus: float = 1.0   # novelty boost on 8-bar phrase lines
    drop_margin: float = 0.0    # a drop is at least this much louder than the track mean
    drop_drive: float = 0.35    # and carries this much kick or bass (in many tracks the drop bass sits in the kick stem)
    bass_on: float = 0.4        # bass presence that starts or ends a core
    mfcc_weight: float = 1.0
    descriptors: tuple = field(default=("centroid", "onset", "hihat_onset"))


DEFAULT = Params()
LEVELS = ("kick", "hihat", "bass", "instruments", "vocal", "loud")


def _matrix(bars, p):
    cols = [np.asarray(bars[k], float) for k in LEVELS + p.descriptors if k in bars]
    F = np.stack(cols, axis=1)
    if "mfcc" in bars and p.mfcc_weight > 0:
        M = np.asarray(bars["mfcc"], float)
        M = (M - M.mean(axis=0)) / (M.std(axis=0) + 1e-9)
        F = np.hstack([F, M * p.mfcc_weight / np.sqrt(M.shape[1])])
    return F


def _novelty(F, w):
    n = len(F)
    nov = np.zeros(n)
    for b in range(1, n):
        nov[b] = np.linalg.norm(F[max(0, b - w):b].mean(axis=0) - F[b:min(n, b + w)].mean(axis=0))
    return nov


def _state_changes(x, threshold, anchor, n):
    """Snapped bars where a smoothed on/off state flips, ignoring runs shorter than a unit."""
    runs = _runs(median_filter(np.asarray(x, float), FILL_BARS, mode="nearest") > threshold)
    return {_snap(r[0], anchor, n) for prev, r in zip(runs, runs[1:]) if min(prev[1] - prev[0], r[1] - r[0]) >= UNIT}


def _boundaries(bars, F, anchor, p):
    n = len(F)
    nov = _novelty(F, p.window)
    cands = [c for c in range(anchor % UNIT, n, UNIT) if UNIT <= c <= n - UNIT]
    score = {c: max(nov[max(1, c - 1):c + 2]) * (p.phrase_bonus if (c - anchor) % (2 * UNIT) == 0 else 1.0) for c in cands}
    top = max(score.values(), default=0.0)
    cuts = {c for i, c in enumerate(cands)
            if score[c] >= p.threshold * top
            and all(score[c] >= score[d] for d in cands[max(0, i - 1):i + 2])}
    cuts |= _state_changes(bars["bass"], p.bass_on, anchor, n)
    cuts |= _state_changes(bars["kick"], KICK_ON, anchor, n)
    ordered = []
    for c in sorted(c for c in cuts if 0 < c < n):
        if ordered and c - ordered[-1] < UNIT:
            if score.get(c, 0) > score.get(ordered[-1], 0):
                ordered[-1] = c
            continue
        ordered.append(c)
    return [0, *ordered, n]


def _rising(bars, a, b):
    if b - a < UNIT:
        return False
    keys = [k for k in ("loud", "centroid", "hihat_onset", "onset") if k in bars]
    x = np.arange(b - a)
    rise = np.mean([np.polyfit(x, np.asarray(bars[k], float)[a:b], 1)[0] * (b - a) for k in keys])
    return rise > 0.1


def label_sections(bars, p=DEFAULT):
    kick = np.asarray(bars["kick"], float)
    bass = np.asarray(bars["bass"], float)
    loud = np.asarray(bars["loud"], float)
    n = len(kick)
    F = _matrix(bars, p)
    runs = _runs(median_filter(kick, FILL_BARS, mode="nearest") > KICK_ON)
    anchor = _anchor([r[0] for prev, r in zip(runs, runs[1:]) if min(prev[1] - prev[0], r[1] - r[0]) >= UNIT])
    cuts = _boundaries(bars, F, anchor, p)
    pieces = list(zip(cuts[:-1], cuts[1:]))

    audible = loud[loud > 0.05]
    mean_loud = audible.mean() if len(audible) else 0.0
    drive = [max(kick[a:b].mean(), bass[a:b].mean()) for a, b in pieces]
    is_drop = [loud[a:b].mean() >= mean_loud + p.drop_margin and drive[i] >= p.drop_drive for i, (a, b) in enumerate(pieces)]
    if not any(is_drop) and pieces:
        best = max(range(len(pieces)), key=lambda i: loud[pieces[i][0]:pieces[i][1]].mean())
        is_drop[best] = drive[best] >= 0.3
    # a drop is the return after a break: a full-energy stretch that opens the track is the DJ intro
    for i in range(len(pieces)):
        if not is_drop[i]:
            break
        is_drop[i] = False
    drop_idx = [i for i, d in enumerate(is_drop) if d]

    labelled = []
    for i, (a, b) in enumerate(pieces):
        if is_drop[i]:
            labelled.append((a, b, "drop"))
            continue
        kick_on = np.mean(kick[a:b] > KICK_ON) > 0.5
        bass_on = bass[a:b].mean() >= GROOVE_BASS
        if not drop_idx or i < drop_idx[0]:
            context = "breakdown" if i > 0 and not kick_on else "intro"
        elif i > drop_idx[-1]:
            context = "outro"
        else:
            context = "groove" if kick_on and bass_on else "breakdown"
        next_is_drop = i + 1 < len(pieces) and is_drop[i + 1]
        if next_is_drop and (not kick_on or _rising(bars, a, b)):
            whole = b - a <= 4 * UNIT and _rising(bars, a, b)
            head, tail = (None, (a, b)) if whole else _tail_split(F, a, b)
            if head and head[1] > head[0]:
                labelled.append((*head, context))
            labelled.append((*tail, "buildup"))
        else:
            labelled.append((a, b, context))

    sections = []
    for a, b, label in labelled:
        if sections and sections[-1].label == label:
            sections[-1] = Section(sections[-1].start_bar, b, label)
        else:
            sections.append(Section(a, b, label))
    return sections


def find_events(sections, bars):
    events = []
    prev = None
    for s in sections:
        if s.label in ("drop", "buildup", "breakdown") and s.label != prev:
            events.append(Event(s.start_bar, s.label))
        prev = s.label
    vocal = median_filter(np.asarray(bars["vocal"], float), 3, mode="nearest") > VOCAL_ON
    for a, b, on in _runs(vocal):
        if on and b - a >= 2:
            events.append(Event(a, "vocal_in"))
            if b < len(vocal):
                events.append(Event(b, "vocal_out"))
    return sorted(events, key=lambda e: (e.bar, e.kind))
