import numpy as np
import pytest

from edm_analysis.structure import find_events, label_sections

STEMS = ("kick", "hihat", "bass", "instruments", "vocal")

# stem levels per block kind, 0..1 relative to the track; "rise" ramps from the value to 0.95
BLOCKS = {
    "intro":     dict(kick=0.8, hihat=0.6, bass=0.3, instruments=0.3, vocal=0.0),
    "ambient":   dict(kick=0.0, hihat=0.1, bass=0.1, instruments=0.4, vocal=0.0),
    "breakdown": dict(kick=0.0, hihat=0.1, bass=0.15, instruments=0.6, vocal=0.7),
    "buildup":   dict(kick=0.0, hihat="rise", bass=0.1, instruments="rise", vocal=0.0),
    "plateau":   dict(kick=0.0, hihat=0.3, bass=0.1, instruments=0.5, vocal=0.0),
    "drop":      dict(kick=0.9, hihat=0.8, bass=0.9, instruments=0.8, vocal=0.0),
    "kickbreak": dict(kick=0.7, hihat=0.3, bass=0.0, instruments=0.6, vocal=0.6),
    "outro":     dict(kick=0.8, hihat=0.5, bass=0.3, instruments=0.25, vocal=0.0),
}


def make_bars(arrangement, seed=0, fills=False):
    rng = np.random.default_rng(seed)
    cols = {s: [] for s in STEMS}
    for kind, n in arrangement:
        for s in STEMS:
            v = BLOCKS[kind][s]
            seg = np.linspace(0.2, 0.95, n) if v == "rise" else np.full(n, v)
            cols[s].append(seg)
    bars = {s: np.clip(np.concatenate(c) + rng.normal(0, 0.03, sum(n for _, n in arrangement)), 0, 1)
            for s, c in cols.items()}
    if fills:  # one-bar kick and bass dropout at the end of every 8-bar phrase
        for k in range(7, len(bars["kick"]), 8):
            bars["kick"][k] = bars["bass"][k] = 0.05
    bars["loud"] = np.clip(0.35 * bars["kick"] + 0.35 * bars["bass"] + 0.3 * bars["instruments"], 0, 1)
    return bars


CASES = [
    pytest.param(
        [("intro", 16), ("buildup", 8), ("drop", 32), ("breakdown", 16), ("buildup", 8), ("drop", 32), ("outro", 16)],
        ["intro", "buildup", "drop", "breakdown", "buildup", "drop", "outro"],
        [24, 80],
        id="classic"),
    pytest.param(
        [("ambient", 8), ("intro", 16), ("buildup", 8), ("drop", 32), ("outro", 16)],
        ["intro", "buildup", "drop", "outro"],
        [32],
        id="ambient-intro"),
    pytest.param(
        [("intro", 16), ("drop", 32), ("plateau", 8), ("drop", 32), ("outro", 16)],
        ["intro", "drop", "buildup", "drop", "outro"],
        [16, 56],
        id="short-flat-break"),
    pytest.param(
        [("intro", 32), ("breakdown", 32), ("buildup", 16), ("drop", 64), ("outro", 32)],
        ["intro", "breakdown", "buildup", "drop", "outro"],
        [80],
        id="long-extended-mix"),
    pytest.param(
        [("intro", 16), ("buildup", 8), ("drop", 32), ("kickbreak", 16), ("buildup", 8), ("drop", 32), ("outro", 16)],
        ["intro", "buildup", "drop", "breakdown", "buildup", "drop", "outro"],
        [24, 80],
        id="breakdown-with-kick"),
    pytest.param(
        [("drop", 32), ("breakdown", 16), ("buildup", 8), ("drop", 32), ("outro", 16)],
        ["intro", "breakdown", "buildup", "drop", "outro"],
        [56],
        id="dj-intro-at-full-energy"),
]


@pytest.mark.parametrize("arrangement,labels,drop_bars", CASES)
def test_label_sections_on_synthetic_arrangements(arrangement, labels, drop_bars):
    bars = make_bars(arrangement)

    sections = label_sections(bars)

    assert [s.label for s in sections] == labels
    starts = np.cumsum([0] + [n for _, n in arrangement])
    assert set(s.start_bar for s in sections) <= set(starts.tolist())
    assert sections[-1].end_bar == starts[-1]
    events = find_events(sections, bars)
    assert [e.bar for e in events if e.kind == "drop"] == drop_bars


@pytest.mark.parametrize("arrangement,labels,drop_bars", CASES)
def test_label_sections_ignores_one_bar_phrase_fills(arrangement, labels, drop_bars):
    bars = make_bars(arrangement, fills=True)

    sections = label_sections(bars)

    assert [s.label for s in sections] == labels
    assert [e.bar for e in find_events(sections, bars) if e.kind == "drop"] == drop_bars


@pytest.mark.parametrize("gaps", [(), (60,), (60, 66)], ids=["clean", "one-bar-gap", "two-gaps"])
def test_find_events_marks_vocal_entries_and_exits(gaps):
    arrangement = [("intro", 16), ("buildup", 8), ("drop", 32), ("breakdown", 16), ("buildup", 8), ("drop", 32), ("outro", 16)]
    bars = make_bars(arrangement)
    for g in gaps:  # a breath between vocal phrases is not an exit
        bars["vocal"][g] = 0.0

    events = find_events(label_sections(bars), bars)

    assert [(e.kind, e.bar) for e in events if e.kind.startswith("vocal")] == [("vocal_in", 56), ("vocal_out", 72)]
