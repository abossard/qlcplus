"""Pretrained Candidates, each in its own environment under CANDIDATES_HOME (never bundled with QLC+).

Each Candidate runs over the Library and leaves its raw output in <out>/candidates/<name>/.
`load` turns that into one shape for scoring against Gold: sections (start, label), drops
(time, confidence) and downbeats where the Candidate has them.

Setup (licences and sources in .session-notes/edm-analysis/pretrained-candidates.md):
- allin1: all-in-one-mlx (MIT) in CANDIDATES_HOME/allin1/.venv;
- edmformer: github.com/25ohms/EDM-98 (CC BY 4.0, MuQ weights CC BY-NC 4.0) in CANDIDATES_HOME/edmformer;
- dropdetector: github.com/felixmeyer6/drop-detector (GPL-3.0) in CANDIDATES_HOME/drop-detector.
"""

import csv
import json
import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

from .archive import preserve
from .fusion import fuse
from .library import in_library, load_scope

CANDIDATES_HOME = Path.home() / ".cache" / "edm-analysis" / "candidates"
NAMES = ("allin1", "edmformer", "dropdetector")
TRACK_FILE = re.compile(r"[0-9a-f]{16}\.json")
BATCH = 5                # tracks per call: a stopped run loses at most one batch


def links(out_dir):
    """<home>/links/<track id>.mp3 for every analysed track, so outputs are named by track id."""
    out_dir, folder = Path(out_dir), CANDIDATES_HOME / "links"
    folder.mkdir(parents=True, exist_ok=True)
    index = json.loads((out_dir / "index.json").read_text())
    for tid, entry in index.items():
        link = folder / f"{tid}.mp3"
        if not link.is_symlink() and Path(entry["path"]).exists():
            try:
                link.symlink_to(entry["path"])
            except FileExistsError:  # another Candidate run made it meanwhile
                pass
    return folder


def name_by_track(out_dir, raw):
    """allin1 names its output after the real file, not the link: rename to <track id>.json."""
    by_path = {e["path"]: tid for tid, e in json.loads((Path(out_dir) / "index.json").read_text()).items()}
    for f in raw.glob("*.json"):
        if not TRACK_FILE.fullmatch(f.name):
            tid = by_path.get(json.loads(f.read_text()).get("path"))
            if tid:
                preserve(raw / f"{tid}.json", Path(out_dir) / "archive" / "candidates" / raw.name, "earlier")
                f.replace(raw / f"{tid}.json")


def raw_dir(out_dir, name):
    path = Path(out_dir) / "candidates" / name
    path.mkdir(parents=True, exist_ok=True)
    return path


def _call(cmd, name, log, **kwargs):
    """Run a Candidate's subprocess; a failure is logged with its exit code (negative: killed by that signal)."""
    code = subprocess.run(cmd, check=False, **kwargs).returncode
    if code:
        log(f"{name}: exit code {code}")


def run(out_dir, name, log=print, only_asked=False, pause=0.0, library=False):
    """pause: seconds of rest per track, so the Mac stays usable while a Candidate runs. library: only tracks inside the Library scope."""
    folder, raw = links(out_dir), raw_dir(out_dir, name)
    if name == "allin1":
        name_by_track(out_dir, raw)
    from .rounds import all_rounds

    asked = {p["track"] for r in all_rounds(out_dir) for p in r["probes"]}
    scope = load_scope(out_dir)
    index = json.loads((Path(out_dir) / "index.json").read_text())
    inside = {tid for tid, e in index.items() if in_library(e["path"], scope)}
    # tracks of the Library first, then the ones the reviewer is asked about, so Candidates can be scored on Gold early
    todo = sorted((p for p in folder.glob("*.mp3") if not (raw / f"{p.stem}.json").exists()),
                  key=lambda p: (p.stem not in inside, p.stem not in asked, p.stem))
    if only_asked:
        todo = [p for p in todo if p.stem in asked]
    if library:
        todo = [p for p in todo if p.stem in inside]
    log(f"{name}: {len(todo)} tracks to do")
    if not todo:
        return
    if name == "allin1":
        exe = CANDIDATES_HOME / "allin1" / ".venv" / "bin" / "allin1-mlx"
        for i in range(0, len(todo), BATCH):
            _call([str(exe), "--no-multiprocess", "-o", str(raw), *map(str, todo[i:i + BATCH])], name, log)
            name_by_track(out_dir, raw)
            time.sleep(pause * len(todo[i:i + BATCH]))
            log(f"{name}: {min(i + BATCH, len(todo))}/{len(todo)}")
    elif name == "edmformer":
        home = CANDIDATES_HOME / "edmformer"
        env = {**os.environ, "MUSICFMPATH": str(home / "src" / "third_party" / "musicfm"), "EDM_PAUSE": str(pause)}
        script = Path(__file__).parent / "candidate_scripts" / "edmformer_batch.py"
        _call([str(home / ".venv" / "bin" / "python"), str(script), str(raw), *map(str, todo)], name, log, cwd=home / "src", env=env)
    elif name == "dropdetector":
        home = CANDIDATES_HOME / "drop-detector"
        chunk_dir = CANDIDATES_HOME / "dropdetector-chunk"
        for i in range(0, len(todo), BATCH):
            shutil.rmtree(chunk_dir, ignore_errors=True)
            chunk_dir.mkdir(parents=True)
            for p in todo[i:i + BATCH]:
                (chunk_dir / p.name).symlink_to(p.resolve())
            (raw / "chunk.csv").unlink(missing_ok=True)
            # threshold 0 keeps every candidate with its probability; never pass --tag (it writes into the MP3s)
            _call([str(home / ".venv" / "bin" / "python"), "model_predict.py", "-f", str(chunk_dir), "-T", "0.0",
                   "-c", str(raw / "chunk.csv")], name, log, cwd=home / "src", stdout=subprocess.DEVNULL)
            rows = {}
            if not (raw / "chunk.csv").exists():  # the run failed: leave these tracks to do
                log(f"{name}: batch at {i} failed")
                continue
            if (raw / "chunk.csv").exists():
                with open(raw / "chunk.csv") as f:
                    for r in csv.DictReader(f):
                        rows.setdefault(Path(r["fpath"]).stem, []).append({"time": float(r["time"]), "confidence": float(r["confidence"])})
            for p in todo[i:i + BATCH]:
                (raw / f"{p.stem}.json").write_text(json.dumps({"drops": rows.get(p.stem, [])}))
            log(f"{name}: {min(i + BATCH, len(todo))}/{len(todo)}")
            time.sleep(pause * len(todo[i:i + BATCH]))
    else:
        sys.exit(f"unknown candidate {name}")
    left = [p for p in todo if not (raw / f"{p.stem}.json").exists()]
    if left:
        log(f"{name}: stopped early, {len(left)} of {len(todo)} tracks still to do")


def load(out_dir, name, tid):
    """{"sections": [{start, label}], "drops": [{time, confidence}], "downbeats": [...]} or None."""
    path = Path(out_dir) / "candidates" / name / f"{tid}.json"
    if not path.exists():
        return None
    raw = json.loads(path.read_text())
    if name == "allin1":
        return {"sections": [{"start": s["start"], "label": s["label"]} for s in raw["segments"] if s["label"] not in ("start", "end")],
                "drops": [], "downbeats": raw["downbeats"]}
    if name == "edmformer":
        merged = [s for i, s in enumerate(raw) if i == 0 or s["label"] != raw[i - 1]["label"]]
        return {"sections": [{"start": s["start"], "label": s["label"]} for s in merged],
                "drops": [{"time": s["start"], "confidence": None} for s in merged if s["label"] == "drop"], "downbeats": []}
    return {"sections": [], "drops": raw["drops"], "downbeats": []}


def fused(out_dir, doc):
    """The fused structure of a rule analysis, from whatever Candidate output exists for the track (rules only if none)."""
    tid = doc["track"]["sha256"][:16]
    return fuse(doc, {name: loaded for name in NAMES if (loaded := load(out_dir, name, tid)) is not None})
