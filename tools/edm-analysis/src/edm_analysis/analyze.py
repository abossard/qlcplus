"""Analyze one track into a versioned JSON document."""

import hashlib
import json
import re
import tempfile
from pathlib import Path

import numpy as np

from . import audio
from .features import bar_descriptors, bar_levels, peaks
from .archive import preserve_analysis
from .final import write_final
from .grid import fit_grid
from .structure import find_events, label_sections

SCHEMA = "qlcplus-edm-analysis/1"
FEATURES_VERSION = 2   # bump when beats, grid, stem levels or peaks change: needs the audio again
STRUCTURE_VERSION = 5  # bump when section/event rules change: recomputed from stored levels
SR = 22050
FILENAME_TAG = re.compile(r"\[(\d+(?:\.\d+)?)\s+([A-G][#b]?\s+(?:Major|Minor)|\d{1,2}[AB])\]")


def track_id(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def filename_tags(path):
    m = FILENAME_TAG.search(Path(path).name)
    return (float(m.group(1)), m.group(2)) if m else (None, None)


def write_atomic(path, text):
    """The review site reads these files while a library run rewrites them."""
    tmp = path.with_name(path.name + ".tmp")
    tmp.write_text(text)
    tmp.replace(path)


def _r(x, nd=3):
    return [round(float(v), nd) for v in x]


def features_document(path, sha, duration, raw_beats, raw_downbeats, grid, levels, descriptors, wave, stem_source):
    return {
        "schema": SCHEMA,
        "analyzer": {"features": FEATURES_VERSION, "beats": "beat_this final0", "stems": stem_source},
        "track": {"path": str(path), "sha256": sha, "title": Path(path).stem, "duration": round(duration, 3)},
        "grid": {"bpm": round(grid.bpm, 3), "period": round(grid.period, 6), "beats_per_bar": 4,
                 "first_downbeat": round(float(grid.downbeats[0]), 4), "confidence": round(grid.confidence, 3)},
        "beats": _r(grid.beats),
        "raw": {"beats": _r(raw_beats), "downbeats": _r(raw_downbeats)},
        "bars": {"start": _r(grid.downbeats[: len(levels["loud"])]),
                 "levels": {k: _r(v, 3) for k, v in levels.items()},
                 "descriptors": descriptors},
        "peaks": {"rate": 20, "values": wave.tolist()},
    }


def with_structure(doc):
    """Add sections and events computed from the stored per-bar levels."""
    starts = doc["bars"]["start"]
    bar_len = 4 * doc["grid"]["period"]
    bar_time = lambda k: starts[k] if k < len(starts) else starts[-1] + bar_len * (k - len(starts) + 1)
    levels = {k: np.asarray(v, float) for k, v in {**doc["bars"]["levels"], **doc["bars"].get("descriptors", {})}.items()}
    sections = label_sections(levels)
    events = find_events(sections, levels)
    bpm_tag, key_tag = filename_tags(doc["track"]["path"])
    track = {**doc["track"], "filename_bpm": bpm_tag, "filename_key": key_tag}
    analyzer = {**doc["analyzer"], "structure": STRUCTURE_VERSION,
                "version": f"{doc['analyzer']['features']}.{STRUCTURE_VERSION}"}
    return {**doc, "analyzer": analyzer, "track": track,
            "sections": [{"label": s.label, "start_bar": s.start_bar, "end_bar": s.end_bar,
                          "start": round(bar_time(s.start_bar), 3) if s.start_bar else 0.0, "end": round(bar_time(s.end_bar), 3)}
                         for s in sections],
            "events": [{"kind": e.kind, "bar": e.bar, "time": round(bar_time(e.bar), 3)} for e in events]}


def _extract_features(path, sha, on_stage):
    with tempfile.TemporaryDirectory() as tmp:
        wav = Path(tmp) / "track.wav"
        on_stage("decode")
        audio.decode(path, 44100, mono=False, out=wav)
        on_stage("beats")
        beats, downbeats = audio.track_beats(wav)
        grid = fit_grid(beats, downbeats)
        on_stage("stems")
        mix = audio.decode(wav, SR)
        stems_file = audio.vdj_stems_path(path)
        stems = audio.load_vdj_stems(stems_file, SR) if stems_file else {}
        complete = len(stems) == len(audio.STEMS) and all(len(x) >= 0.98 * len(mix) for x in stems.values())
        if complete:
            stem_source = "vdjstems"
        else:
            stems, stem_source = audio.separate_demucs(wav, SR), "htdemucs"
    on_stage("levels")
    bar_starts = np.append(grid.downbeats, grid.downbeats[-1] + 4 * grid.period)
    bar_starts = bar_starts[bar_starts * SR <= len(mix)]
    levels = bar_levels(mix, stems, bar_starts, SR)
    descriptors = bar_descriptors(mix, stems, bar_starts, SR)
    return features_document(path, sha, len(mix) / SR, beats, downbeats, grid, levels, descriptors, peaks(mix, SR), stem_source)


def analyze_track(path, out_dir, force=False, on_stage=lambda stage: None):
    """Returns (document, what was reused: "all", "features" or None)."""
    path = Path(path)
    out_dir = Path(out_dir)
    on_stage("hash")
    sha = track_id(path)
    out = out_dir / f"{sha[:16]}.json"
    doc = json.loads(out.read_text()) if out.exists() and not force else None
    if doc and doc["analyzer"].get("features") == FEATURES_VERSION and doc["analyzer"].get("structure") == STRUCTURE_VERSION:
        if not (out_dir / "final" / out.name).exists():
            write_final(out_dir, doc)
        return doc, "all"
    reused = "features" if doc and doc["analyzer"].get("features") == FEATURES_VERSION else None
    if not reused:
        doc = _extract_features(path, sha, on_stage)
    on_stage("structure")
    doc = with_structure(doc)
    out_dir.mkdir(parents=True, exist_ok=True)
    preserve_analysis(out, out_dir)
    write_atomic(out, json.dumps(doc, separators=(",", ":")))
    write_final(out_dir, doc)
    return doc, reused
