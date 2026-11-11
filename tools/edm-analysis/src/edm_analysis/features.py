"""Per-bar levels and waveform peaks from decoded audio (pure calculations)."""

import numpy as np

STEM_RANGE_DB = 36.0   # stem level 0..1 spans this many dB below the mix's loud bars
MIX_RANGE_DB = 24.0


def _rms_db(x):
    return 10 * np.log10(np.mean(np.square(x, dtype=np.float64)) + 1e-12)


def bar_levels(mix, stems, bar_starts, sr):
    """0..1 level per bar for the mix ("loud") and each stem, relative to the mix's loud bars."""
    edges = np.round(np.asarray(bar_starts) * sr).astype(int)
    spans = [(a, b) for a, b in zip(edges[:-1], edges[1:]) if b > a]
    mix_db = np.array([_rms_db(mix[a:b]) for a, b in spans])
    ref = np.percentile(mix_db, 95)
    levels = {"loud": np.clip(1 + (mix_db - ref) / MIX_RANGE_DB, 0, 1)}
    for name, x in stems.items():
        db = np.array([_rms_db(x[a:b]) for a, b in spans])
        levels[name] = np.clip(1 + (db - ref) / STEM_RANGE_DB, 0, 1)
    return levels


def peaks(mix, sr, rate=20):
    hop = sr // rate
    n = len(mix) // hop
    frames = np.abs(mix[: n * hop]).reshape(n, hop)
    return np.round(frames.max(axis=1), 3)


HOP = 512


def _per_bar(frames, bar_starts, sr):
    """Mean of a frame-rate feature (last axis) within each bar."""
    edges = np.round(np.asarray(bar_starts) * sr / HOP).astype(int)
    n = frames.shape[-1]
    return np.stack([frames[..., min(a, n - 1):max(min(b, n), min(a, n - 1) + 1)].mean(axis=-1)
                     for a, b in zip(edges[:-1], edges[1:])], axis=-1)


def _scale(x):
    """0..1 within the track (5th to 95th percentile), so tracks with different mastering compare."""
    lo, hi = np.percentile(x, [5, 95])
    return np.clip((x - lo) / (hi - lo + 1e-9), 0, 1)


def bar_descriptors(mix, stems, bar_starts, sr):
    """Timbre (MFCC), brightness (spectral centroid) and onset density per bar.

    These carry what stem levels miss: risers and filter sweeps raise brightness,
    snare rolls raise onset density, and a change of sound changes the MFCCs.
    """
    import librosa

    mfcc = _per_bar(librosa.feature.mfcc(y=mix, sr=sr, n_mfcc=13, hop_length=HOP), bar_starts, sr).T
    centroid = _per_bar(np.log1p(librosa.feature.spectral_centroid(y=mix, sr=sr, hop_length=HOP)[0]), bar_starts, sr)
    inst = stems.get("instruments", mix)
    inst_centroid = _per_bar(np.log1p(librosa.feature.spectral_centroid(y=inst, sr=sr, hop_length=HOP)[0]), bar_starts, sr)
    onset = _per_bar(librosa.onset.onset_strength(y=mix, sr=sr, hop_length=HOP), bar_starts, sr)
    hihat_onset = _per_bar(librosa.onset.onset_strength(y=stems.get("hihat", mix), sr=sr, hop_length=HOP), bar_starts, sr)
    return {
        "mfcc": np.round(mfcc, 2).tolist(),
        "centroid": np.round(_scale(centroid), 3).tolist(),
        "inst_centroid": np.round(_scale(inst_centroid), 3).tolist(),
        "onset": np.round(_scale(onset), 3).tolist(),
        "hihat_onset": np.round(_scale(hihat_onset), 3).tolist(),
    }
