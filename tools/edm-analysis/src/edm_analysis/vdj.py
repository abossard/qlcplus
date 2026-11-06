"""VirtualDJ database.xml as a reference: beat grid, cue points, break regions."""

import xml.etree.ElementTree as ET
from pathlib import Path

import numpy as np

DEFAULT_DB = Path.home() / "Library" / "Application Support" / "VirtualDJ" / "database.xml"


def parse_database(path=DEFAULT_DB):
    songs = {}
    for song in ET.parse(path).getroot().iter("Song"):
        scan = song.find("Scan")
        period = float(scan.get("Bpm")) if scan is not None and scan.get("Bpm") else None
        phase = float(scan.get("Phase")) if scan is not None and scan.get("Phase") else None
        cues, starts, ends = [], [], []
        for poi in song.iter("Poi"):
            kind, name, pos = poi.get("Type"), poi.get("Name", ""), poi.get("Pos")
            if pos is None:
                continue
            if kind == "cue":
                cues.append({"name": name, "pos": float(pos)})
            elif kind == "remix" and name.startswith("Break"):
                starts.append(float(pos))
            elif kind == "remix" and name.startswith("End Break"):
                ends.append(float(pos))
        songs[song.get("FilePath")] = {"period": period, "phase": phase, "cues": cues,
                                       "breaks": list(zip(sorted(starts), sorted(ends)))}
    return songs


def grid_agreement(beats, bar_starts, bpm, period, phase):
    """How our grid sits on VirtualDJ's: tempo difference, beat offset, and which VDJ beat our bar 1 is on."""
    pos = (np.asarray(beats) - phase) / period
    offset = (pos - np.round(pos)) * period
    bar_pos = np.round((np.asarray(bar_starts) - phase) / period).astype(int) % 4
    return {
        "bpm_diff": bpm - 60.0 / period,
        "beat_offset_ms": float(np.median(offset) * 1000),
        "beats_within_20ms": float(np.mean(np.abs(offset) < 0.020)),
        "bar_phase": int(np.argmax(np.bincount(bar_pos, minlength=4))),
    }
