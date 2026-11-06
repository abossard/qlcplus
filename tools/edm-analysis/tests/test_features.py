import numpy as np
import pytest

from edm_analysis.features import bar_descriptors

SR = 22050


def tone(freq, seconds, amp=0.5):
    t = np.arange(int(seconds * SR)) / SR
    return (amp * np.sin(2 * np.pi * freq * t)).astype(np.float32)


@pytest.mark.parametrize("low,high", [(200.0, 4000.0), (100.0, 2000.0)])
def test_bar_descriptors_track_brightness_and_shape(low, high):
    bar = 2.0
    mix = np.concatenate([tone(low, 4 * bar), tone(high, 4 * bar)])
    stems = {"hihat": np.zeros_like(mix), "instruments": mix.copy()}
    starts = np.arange(9) * bar

    d = bar_descriptors(mix, stems, starts, SR)

    assert np.asarray(d["mfcc"]).shape == (8, 13)
    for key in ("centroid", "inst_centroid", "onset", "hihat_onset"):
        assert len(d[key]) == 8 and all(0.0 <= v <= 1.0 for v in d[key])
    assert np.mean(d["centroid"][4:]) > np.mean(d["centroid"][:4]) + 0.3


def test_bar_descriptors_onset_rises_with_a_snare_roll():
    bar = 2.0
    clicks = np.zeros(int(8 * bar * SR), np.float32)
    for k in range(8):  # notes per bar double every bar: 1, 2, 4, ... like a roll
        n = 2 ** min(k, 5)
        for j in range(n):
            a = int((k * bar + j * bar / n) * SR)
            clicks[a:a + 200] = np.random.default_rng(k * 100 + j).normal(0, 0.5, 200)
    starts = np.arange(9) * bar

    d = bar_descriptors(clicks, {"hihat": clicks, "instruments": np.zeros_like(clicks)}, starts, SR)

    assert d["hihat_onset"][6] > d["hihat_onset"][0]
