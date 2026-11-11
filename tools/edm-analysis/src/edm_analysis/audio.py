"""Decoding and stems. All file and model access lives here."""

import json
import subprocess
from pathlib import Path

import numpy as np

STEMS = ("kick", "hihat", "bass", "instruments", "vocal")


def decode(path, sr, mono=True, stream=None, out=None):
    """Decode with ffmpeg. Returns float32 samples, or writes a WAV when `out` is given."""
    cmd = ["ffmpeg", "-v", "error", "-nostdin", "-i", str(path)]
    if stream is not None:
        cmd += ["-map", f"0:a:{stream}"]
    cmd += ["-ac", "1" if mono else "2", "-ar", str(sr)]
    if out is not None:
        subprocess.run(cmd + ["-y", str(out)], check=True)
        return None
    raw = subprocess.run(cmd + ["-f", "f32le", "-"], check=True, capture_output=True).stdout
    data = np.frombuffer(raw, np.float32)
    return data if mono else data.reshape(-1, 2).T


def vdj_stems_path(track):
    p = Path(str(track) + ".vdjstems")
    return p if p.exists() else None


def _stream_titles(path):
    probe = subprocess.run(
        ["ffprobe", "-v", "error", "-select_streams", "a", "-show_entries", "stream_tags=title", "-of", "json", str(path)],
        check=True, capture_output=True, text=True).stdout
    return [s.get("tags", {}).get("title", "") for s in json.loads(probe)["streams"]]


def load_vdj_stems(path, sr):
    titles = _stream_titles(path)
    return {t: decode(path, sr, stream=i) for i, t in enumerate(titles) if t in STEMS}


_demucs = None


def separate_demucs(wav_path, sr):
    """Fallback for tracks without VirtualDJ stems: htdemucs, drums split into kick and hihat bands."""
    global _demucs
    import soundfile as sf
    import torch
    from demucs.apply import apply_model
    from demucs.pretrained import get_model
    from scipy.signal import butter, resample_poly, sosfiltfilt

    if _demucs is None:
        _demucs = get_model("htdemucs").eval()
    device = "mps" if torch.backends.mps.is_available() else "cpu"
    x, in_sr = sf.read(wav_path, dtype="float32", always_2d=True)
    with torch.no_grad():
        out = apply_model(_demucs.to(device), torch.from_numpy(x.T)[None].to(device), device=device,
                          split=True, overlap=0.25, progress=False)[0].mean(dim=1).cpu().numpy()
    stems = {name: resample_poly(out[i], sr, in_sr).astype(np.float32) for i, name in enumerate(_demucs.sources)}
    drums = stems.pop("drums")
    stems["kick"] = sosfiltfilt(butter(4, 150, "low", fs=sr, output="sos"), drums).astype(np.float32)
    stems["hihat"] = sosfiltfilt(butter(4, 6000, "high", fs=sr, output="sos"), drums).astype(np.float32)
    stems["instruments"] = stems.pop("other")
    stems["vocal"] = stems.pop("vocals")
    return stems


_beat_model = None


def track_beats(wav_path):
    global _beat_model
    import torch
    from beat_this.inference import File2Beats

    if _beat_model is None:
        device = "mps" if torch.backends.mps.is_available() else "cpu"
        _beat_model = File2Beats(checkpoint_path="final0", device=device, dbn=False)
    beats, downbeats = _beat_model(str(wav_path))
    return np.asarray(beats), np.asarray(downbeats)
