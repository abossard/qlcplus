"""Pretrained Candidates, each in its own environment under CANDIDATES_HOME (never bundled with QLC+).

Each Candidate runs over the Library and leaves its raw output in <out>/candidates/<name>/.
`load` turns that into one shape for scoring against Gold: sections (start, label), drops
(time, confidence) and downbeats where the Candidate has them.

Setup (licences and sources in .session-notes/edm-analysis/pretrained-candidates.md):
- allin1: all-in-one-mlx (MIT) in CANDIDATES_HOME/allin1/.venv;
- edmformer: github.com/25ohms/EDM-98 (CC BY 4.0, MuQ weights CC BY-NC 4.0) in CANDIDATES_HOME/edmformer;
- dropdetector: github.com/felixmeyer6/drop-detector (GPL-3.0) in CANDIDATES_HOME/drop-detector.

Running a Candidate safely (by hand, on an idle Mac, never from a job that restarts itself):
    edm-analysis candidates edmformer --library --max-tracks 1          # one track, then look at the log
    edm-analysis candidates edmformer --library --max-tracks 5 --device cpu
Every process runs in its own process group, one track per process for EDMFormer (its two large models live in unified memory and
used over 20 GB when one process ran every track), batches of 5 for allin1 and the drop detector. While it runs the parent reads the
group's memory (macOS physical footprint, see `footprint`) and its age, and kills the whole group past --max-memory-gb (default 8)
or --max-minutes (default 20 per track). Before each process it checks free memory and the 1-minute load average and refuses on a busy
Mac (exit code 75, nothing killed; --force skips the check). A track that fails, is killed or leaves no output is logged with its peak memory
and reason, recorded in <out>/candidates/<name>/failed.json and ends the run with exit code 1: there is no retry. Later runs skip recorded
tracks until --retry-failed. Exit codes: 0 done or nothing to do, 1 a track failed, 75 the Mac was busy.
"""

import csv
import ctypes
import json
import os
import re
import shutil
import signal
import subprocess
import sys
import time
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path

from .archive import preserve
from .fusion import fuse
from .library import in_library, load_scope

CANDIDATES_HOME = Path.home() / ".cache" / "edm-analysis" / "candidates"
NAMES = ("allin1", "edmformer", "dropdetector")
TRACK_FILE = re.compile(r"[0-9a-f]{16}\.json")
BATCH = 5                # tracks per process for allin1 and the drop detector: a stopped run loses at most one batch
GB = 10**9
EXIT_FAILED, EXIT_BUSY = 1, 75      # 75 is EX_TEMPFAIL: try again when the Mac is calm
FAILED_FILE = "failed.json"
MPS_WORKING_SET = 0.75   # share of RAM Metal recommends as a process's working set; PYTORCH_MPS_HIGH_WATERMARK_RATIO is a ratio of it


@dataclass
class Guard:
    """Limits and measurements of a guarded run. measure(pid) and machine() default to `footprint` and `read_machine` and exist for tests."""
    max_memory_gb: float = 8.0
    max_seconds: float = 1200.0
    min_free_gb: float = 16.0     # twice the memory limit: the Mac keeps room for its own work
    max_load: float = 6.0         # 1-minute load average; 10 cores under normal use sit well below
    force: bool = False           # skip the preflight check (the limits still hold)
    poll: float = 0.5
    measure: object = None
    machine: object = None


def preflight(free_gb, load1, min_free_gb, max_load):
    """Why the Mac is too busy to start a Candidate, or None."""
    if free_gb < min_free_gb:
        return f"free memory {free_gb:.1f} GB is under {min_free_gb:g} GB"
    if load1 > max_load:
        return f"load average {load1:.1f} is over {max_load:g}"
    return None


def parse_vm_stat(text):
    """GB the system can hand out at once: free, inactive, speculative and purgeable pages (vm_stat output)."""
    page = int(re.search(r"page size of (\d+) bytes", text).group(1))
    pages = {k: int(v) for k, v in re.findall(r"Pages (free|inactive|speculative|purgeable):\s+(\d+)\.", text)}
    return sum(pages.values()) * page / GB


def read_machine():
    """(free GB, 1-minute load average) of this Mac."""
    out = subprocess.run(["vm_stat"], capture_output=True, text=True, check=False).stdout
    return parse_vm_stat(out), os.getloadavg()[0]


def group_pids(rows, leader):
    """Pids of the process group `leader` and of everything descended from it, from (pid, ppid, pgid) rows."""
    members = {pid for pid, _, pgid in rows if pgid == leader or pid == leader}
    grew = True
    while grew:
        grew = False
        for pid, ppid, _ in rows:
            if pid not in members and ppid in members:
                members.add(pid)
                grew = True
    return sorted(members)


def _pid_footprint(pid):
    """ri_phys_footprint of one process (proc_pid_rusage, flavor 0): resident, compressed and Metal memory the process is charged for."""
    info = (ctypes.c_uint64 * 64)()
    if ctypes.CDLL("/usr/lib/libproc.dylib", use_errno=True).proc_pid_rusage(pid, 0, ctypes.byref(info)) != 0:
        return None
    return int(info[9])      # uuid (2 words), user, system, pkg idle, interrupt wakeups, pageins, wired, resident, then the footprint


def footprint(leader):
    """Bytes of physical footprint summed over the process group `leader` and its descendants, or None where it cannot be read (not macOS).
    Covers resident, compressed and Metal/IOKit memory that the kernel charges to those processes, which is what Activity Monitor's Memory
    column shows. Blind spots: memory the GPU driver holds for the process but charges elsewhere, shared memory counted once per process,
    and processes that left the group and lost their parent."""
    if sys.platform != "darwin":
        return None
    out = subprocess.run(["ps", "-axo", "pid=,ppid=,pgid="], capture_output=True, text=True, check=False).stdout
    rows = [tuple(map(int, line.split())) for line in out.splitlines() if line.strip()]
    sizes = [_pid_footprint(pid) for pid in group_pids(rows, leader)]
    sizes = [x for x in sizes if x is not None]
    return sum(sizes) if sizes else None


def load_failed(raw):
    path = Path(raw) / FAILED_FILE
    return json.loads(path.read_text()) if path.exists() else {}


def _save_failed(raw, failed):
    path = Path(raw) / FAILED_FILE
    tmp = path.with_name(path.name + ".tmp")
    tmp.write_text(json.dumps(failed, indent=1))
    tmp.replace(path)


def command(name, raw, tracks, device="mps", max_memory_gb=8.0, ram_gb=None):
    """(argv, cwd, extra environment) of one Candidate process over `tracks`."""
    if name == "allin1":
        return [str(CANDIDATES_HOME / "allin1" / ".venv" / "bin" / "allin1-mlx"), "--no-multiprocess", "-o", str(raw), *map(str, tracks)], None, {}
    if name == "edmformer":
        home = CANDIDATES_HOME / "edmformer"
        script = Path(__file__).parent / "candidate_scripts" / "edmformer_batch.py"
        env = {"MUSICFMPATH": str(home / "src" / "third_party" / "musicfm"), "EDM_DEVICE": device}
        if device == "mps" and "PYTORCH_MPS_HIGH_WATERMARK_RATIO" not in os.environ:
            # second line of defence: PyTorch's Metal allocator raises instead of growing past this share of the recommended working set
            ram = ram_gb or os.sysconf("SC_PHYS_PAGES") * os.sysconf("SC_PAGE_SIZE") / GB
            ratio = min(1.0, max_memory_gb / (MPS_WORKING_SET * ram))
            env |= {"PYTORCH_MPS_HIGH_WATERMARK_RATIO": f"{ratio:.3f}", "PYTORCH_MPS_LOW_WATERMARK_RATIO": f"{ratio / 2:.3f}"}
        return [str(home / ".venv" / "bin" / "python"), str(script), str(raw), *map(str, tracks)], home / "src", env
    home = CANDIDATES_HOME / "drop-detector"
    # threshold 0 keeps every candidate with its probability; never pass --tag (it writes into the MP3s)
    return [str(home / ".venv" / "bin" / "python"), "model_predict.py", "-f", str(CANDIDATES_HOME / "dropdetector-chunk"), "-T", "0.0",
            "-c", str(Path(raw) / "chunk.csv")], home / "src", {}


def _kill_group(proc):
    try:
        os.killpg(proc.pid, signal.SIGKILL)
    except (ProcessLookupError, PermissionError):
        pass
    proc.wait()


def _guarded(argv, cwd, env, guard, stdout=None):
    """Run one process in its own group under the limits: (reason or None, peak GB, seconds). reason: "memory", "time" or "exit code N"."""
    measure = guard.measure or footprint
    start, peak, reason = time.monotonic(), 0, None
    proc = subprocess.Popen(argv, cwd=cwd, env={**os.environ, **env}, start_new_session=True, stdout=stdout)
    try:
        while proc.poll() is None:
            used = measure(proc.pid) or 0
            peak = max(peak, used)
            if used > guard.max_memory_gb * GB:
                reason = "memory"
            elif time.monotonic() - start > guard.max_seconds:
                reason = "time"
            if reason:
                _kill_group(proc)
                break
            time.sleep(guard.poll)
    finally:
        _kill_group(proc)     # also takes stray descendants of a child that ended by itself, and the child if we are interrupted
    code = proc.returncode
    reason = reason or (f"exit code {code}" if code else None)
    return reason, peak / GB, time.monotonic() - start


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


def _finish(name, unit, raw, out_dir):
    """What a Candidate's process leaves besides its own files: allin1 names its output after the real file, the drop detector writes one csv."""
    if name == "allin1":
        name_by_track(out_dir, raw)
    elif name == "dropdetector" and (raw / "chunk.csv").exists():
        rows = {}
        with open(raw / "chunk.csv") as f:
            for r in csv.DictReader(f):
                rows.setdefault(Path(r["fpath"]).stem, []).append({"time": float(r["time"]), "confidence": float(r["confidence"])})
        for p in unit:
            (raw / f"{p.stem}.json").write_text(json.dumps({"drops": rows.get(p.stem, [])}))


def _record(raw, unit, reason, peak, seconds):
    failed = load_failed(raw)
    for p in unit:
        if not (raw / f"{p.stem}.json").exists():
            failed[p.stem] = {"reason": reason, "peak_gb": round(peak, 2), "seconds": round(seconds), "at": datetime.now().isoformat(timespec="seconds")}
    _save_failed(raw, failed)


def run(out_dir, name, log=print, only_asked=False, pause=0.0, library=False, guard=None, max_tracks=None, device="mps", retry_failed=False):
    """Run a Candidate over the tracks it has no output for, one guarded process per track (EDMFormer) or batch (see the module docstring).
    pause: seconds of rest per track. library: only tracks inside the Library scope. max_tracks: at most this many. Returns the exit code."""
    guard = guard or Guard()
    folder, raw = links(out_dir), raw_dir(out_dir, name)
    if name not in NAMES:
        sys.exit(f"unknown candidate {name}")
    if name == "allin1":
        name_by_track(out_dir, raw)
    from .rounds import all_rounds

    asked = {p["track"] for r in all_rounds(out_dir) for p in r["probes"]}
    scope = load_scope(out_dir)
    index = json.loads((Path(out_dir) / "index.json").read_text())
    inside = {tid for tid, e in index.items() if in_library(e["path"], scope)}
    failed = set() if retry_failed else set(load_failed(raw))
    # tracks of the Library first, then the ones the reviewer is asked about, so Candidates can be scored on Gold early
    todo = sorted((p for p in folder.glob("*.mp3") if not (raw / f"{p.stem}.json").exists() and p.stem not in failed),
                  key=lambda p: (p.stem not in inside, p.stem not in asked, p.stem))
    if only_asked:
        todo = [p for p in todo if p.stem in asked]
    if library:
        todo = [p for p in todo if p.stem in inside]
    todo = todo[:max_tracks]
    skipped = len(failed & {p.stem for p in folder.glob("*.mp3")})
    log(f"{name}: {len(todo)} tracks to do" + (f", {skipped} skipped as failed earlier (--retry-failed)" if skipped else ""))
    if not todo:
        return 0
    if (guard.measure or footprint)(os.getpid()) is None:
        log(f"{name}: cannot measure memory on this system, so no limit could hold: not starting")
        return EXIT_FAILED
    units = [[p] for p in todo] if name == "edmformer" else [todo[i:i + BATCH] for i in range(0, len(todo), BATCH)]
    for unit in units:
        if not guard.force and (busy := preflight(*(guard.machine or read_machine)(), guard.min_free_gb, guard.max_load)):
            log(f"{name}: the Mac is busy, {busy}: nothing started, nothing killed")
            return EXIT_BUSY
        if name == "dropdetector":
            chunk = CANDIDATES_HOME / "dropdetector-chunk"
            shutil.rmtree(chunk, ignore_errors=True)
            chunk.mkdir(parents=True)
            for p in unit:
                (chunk / p.name).symlink_to(p.resolve())
            (raw / "chunk.csv").unlink(missing_ok=True)
        argv, cwd, env = command(name, raw, unit, device=device, max_memory_gb=guard.max_memory_gb)
        reason, peak, seconds = _guarded(argv, cwd, env, guard, stdout=subprocess.DEVNULL if name == "dropdetector" else None)
        _finish(name, unit, raw, out_dir)
        if not reason and any(not (raw / f"{p.stem}.json").exists() for p in unit):
            reason = "no output"
        label = unit[0].stem if len(unit) == 1 else f"batch of {len(unit)}"
        if reason:
            _record(raw, unit, reason, peak, seconds)
            log(f"{name}: {label} failed: {reason}, peak {peak:.1f} GB after {seconds:.0f} s")
            left = [p for p in todo if not (raw / f"{p.stem}.json").exists()]
            log(f"{name}: stopped early, {len(left)} of {len(todo)} tracks still to do")
            return EXIT_FAILED
        again = load_failed(raw)
        if any(p.stem in again for p in unit):      # a retried track that worked
            _save_failed(raw, {k: v for k, v in again.items() if k not in {p.stem for p in unit}})
        log(f"{name}: {label} done in {seconds:.0f} s, peak {peak:.1f} GB" + (f" ({device})" if name == "edmformer" else ""))
        time.sleep(pause * len(unit))
    return 0


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
