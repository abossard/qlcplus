"""Running a Candidate safely: one process per EDMFormer track, memory and time limits, stop instead of retry, preflight."""

import json
import os
import sys
import time
from pathlib import Path
from types import SimpleNamespace

import pytest

from edm_analysis import candidates
from edm_analysis.candidates import EXIT_BUSY, EXIT_FAILED, Guard, command, group_pids, load_failed, parse_vm_stat, preflight, run
from edm_analysis.library import set_scope

STUB = Path(__file__).with_name("stub_candidate.py")
GB = 10**9
CALM = lambda: (40.0, 1.0)         # free memory in GB, 1-minute load average


def ids(n):
    return [f"{i:016x}" for i in range(n)]


@pytest.fixture
def library(tmp_path, monkeypatch):
    """Seven tracks in <tmp>/music, a stubbed Candidate process, a calm machine."""
    music = tmp_path / "music"
    music.mkdir()
    tracks = {tid: music / f"song{i}.mp3" for i, tid in enumerate(ids(7))}
    for path in tracks.values():
        path.write_bytes(b"")
    (tmp_path / "index.json").write_text(json.dumps({tid: {"path": str(path)} for tid, path in tracks.items()}))
    for name in candidates.NAMES:
        (tmp_path / "candidates" / name).mkdir(parents=True)
    monkeypatch.setattr(candidates, "CANDIDATES_HOME", tmp_path / "home")
    monkeypatch.setattr(candidates, "command", lambda name, raw, tracks, **kw: ([sys.executable, str(STUB), str(raw), *map(str, tracks)], None, {}))
    monkeypatch.setenv("STUB_LOG", str(tmp_path / "processes.log"))
    return tmp_path


def processes(root):
    log = root / "processes.log"
    return [line.split()[1:] for line in log.read_text().splitlines()] if log.exists() else []


def guard(**kw):
    return Guard(**{"measure": lambda pid: 10 * 10**6, "machine": CALM, "poll": 0.05, **kw})


def done(root, name="edmformer"):
    return sorted(p.stem for p in (root / "candidates" / name).glob("*.json") if p.stem != "failed")


# --- pure parts -------------------------------------------------------------------------

@pytest.mark.parametrize("free,load,reason", [
    (40.0, 1.0, None), (16.0, 6.0, None),                 # at the thresholds is calm
    (15.9, 1.0, "free memory"), (40.0, 6.1, "load"), (5.0, 9.0, "free memory"),
])
def test_preflight_refuses_a_busy_machine(free, load, reason):
    got = preflight(free, load, min_free_gb=16.0, max_load=6.0)
    assert (got is None) if reason is None else (reason in got)


def test_free_memory_counts_what_the_system_can_take_back():
    text = """Mach Virtual Memory Statistics: (page size of 16384 bytes)
Pages free:                                  1000.
Pages active:                                5000.
Pages inactive:                              2000.
Pages speculative:                            500.
Pages purgeable:                              100.
"Translation faults":                       26042396."""
    assert parse_vm_stat(text) == pytest.approx((1000 + 2000 + 500 + 100) * 16384 / GB)


@pytest.mark.parametrize("leader,expected", [(10, [10, 11, 12, 14]), (20, [20]), (99, [])])
def test_a_process_group_is_its_members_and_their_descendants(leader, expected):
    #       pid ppid pgid
    rows = [(10, 1, 10), (11, 10, 10), (12, 11, 10), (13, 1, 13), (14, 12, 77), (20, 1, 20)]    # 14 left the group but is still a descendant
    assert group_pids(rows, leader) == expected


@pytest.mark.skipif(sys.platform != "darwin", reason="reads the macOS physical footprint")
def test_the_footprint_of_a_process_counts_memory_it_holds():
    import subprocess

    child = subprocess.Popen([sys.executable, "-c", "import time; x = b'x' * 300_000_000; time.sleep(30)"], start_new_session=True)
    try:
        time.sleep(1.5)
        assert 250 * 10**6 < candidates.footprint(child.pid) < 2 * GB
    finally:
        child.kill()


@pytest.mark.parametrize("name,device,tracks,expect", [
    ("edmformer", "mps", ["a.mp3"], {"EDM_DEVICE": "mps", "PYTORCH_MPS_HIGH_WATERMARK_RATIO": "0.167", "PYTORCH_MPS_LOW_WATERMARK_RATIO": "0.083"}),
    ("edmformer", "cpu", ["a.mp3"], {"EDM_DEVICE": "cpu"}),
    ("allin1", "mps", ["a.mp3", "b.mp3"], {}),
    ("dropdetector", "mps", ["a.mp3"], {}),
])
def test_the_command_hands_the_device_and_a_metal_memory_cap_to_edmformer_only(name, device, tracks, expect, tmp_path):
    argv, cwd, env = command(name, tmp_path, tracks, device=device, max_memory_gb=8.0, ram_gb=64.0)
    assert {k: v for k, v in env.items() if k.startswith(("EDM_DEVICE", "PYTORCH_MPS"))} == expect
    assert all(t in argv for t in tracks) or name == "dropdetector"


def test_an_mps_cap_the_user_already_set_is_left_alone(tmp_path, monkeypatch):
    monkeypatch.setenv("PYTORCH_MPS_HIGH_WATERMARK_RATIO", "0.9")
    _, _, env = command("edmformer", tmp_path, ["a.mp3"], device="mps", max_memory_gb=8.0, ram_gb=64.0)
    assert "PYTORCH_MPS_HIGH_WATERMARK_RATIO" not in env


# --- running -------------------------------------------------------------------------------

def test_edmformer_runs_one_process_per_track_and_logs_peak_memory_and_seconds(library):
    log = []
    assert run(library, "edmformer", log=log.append, guard=guard(), max_tracks=3) == 0
    assert [len(p) for p in processes(library)] == [1, 1, 1] and len({tuple(p) for p in processes(library)}) == 3
    assert done(library) == ids(3)
    assert [m for m in log if "peak" in m and "mps" in m] and all(" s, peak " in m for m in log if m.endswith("mps)"))


def test_allin1_keeps_its_batches_of_five(library):
    assert run(library, "allin1", log=lambda m: None, guard=guard()) == 0
    assert [len(p) for p in processes(library)] == [5, 2]


def test_with_nothing_to_do_the_run_succeeds_and_starts_nothing(library):
    run(library, "edmformer", log=lambda m: None, guard=guard())
    assert run(library, "edmformer", log=lambda m: None, guard=guard()) == 0
    assert len(processes(library)) == 7


def test_the_memory_limit_kills_the_child_stops_the_run_and_records_why(library, monkeypatch):
    monkeypatch.setenv("STUB_MODE", "sleep")
    log = []
    code = run(library, "edmformer", log=log.append, guard=guard(max_memory_gb=8.0, measure=lambda pid: 9 * GB))
    first = ids(7)[0]
    assert code == EXIT_FAILED and len(processes(library)) <= 1                 # no second track (the first may die before it writes its line)
    assert any("stopped early, 7 of 7" in m for m in log)
    failed = load_failed(library / "candidates" / "edmformer")
    assert list(failed) == [first] and failed[first]["reason"] == "memory" and failed[first]["peak_gb"] == pytest.approx(9.0)
    assert any(first in m and "memory" in m and "9.0 GB" in m for m in log)


def test_a_real_allocation_over_the_limit_is_killed(library, monkeypatch):
    if sys.platform != "darwin":
        pytest.skip("reads the macOS physical footprint")
    monkeypatch.setenv("STUB_MODE", "alloc")
    monkeypatch.setenv("STUB_MB", "400")
    log = []
    code = run(library, "edmformer", log=log.append, guard=Guard(machine=CALM, poll=0.1, max_memory_gb=0.2, force=False))
    assert code == EXIT_FAILED and load_failed(library / "candidates" / "edmformer")[ids(7)[0]]["reason"] == "memory"


def test_the_time_limit_kills_the_whole_process_group(library, monkeypatch):
    monkeypatch.setenv("STUB_MODE", "grandchild")
    monkeypatch.setenv("STUB_GRANDCHILD", str(library / "grandchild.pid"))
    started = time.time()
    code = run(library, "edmformer", log=lambda m: None, guard=guard(max_seconds=1.0))
    assert code == EXIT_FAILED and time.time() - started < 15
    assert load_failed(library / "candidates" / "edmformer")[ids(7)[0]]["reason"] == "time"
    pid = int((library / "grandchild.pid").read_text())
    time.sleep(0.3)
    with pytest.raises(ProcessLookupError):
        os.kill(pid, 0)


@pytest.mark.parametrize("mode,code,reason", [("fail", "3", "exit code 3"), ("selfkill", None, "exit code -9"), ("nooutput", None, "no output")])
def test_a_failed_track_is_recorded_and_the_run_ends_without_a_retry(library, monkeypatch, mode, code, reason):
    monkeypatch.setenv("STUB_MODE", mode)
    if code:
        monkeypatch.setenv("STUB_CODE", code)
    log = []
    assert run(library, "edmformer", log=log.append, guard=guard()) == EXIT_FAILED
    assert len(processes(library)) == 1
    assert load_failed(library / "candidates" / "edmformer")[ids(7)[0]]["reason"] == reason
    assert any("stopped early" in m for m in log)


def test_a_failed_track_is_skipped_by_later_runs_until_retry_failed(library, monkeypatch):
    monkeypatch.setenv("STUB_FAIL", ids(7)[1])
    assert run(library, "edmformer", log=lambda m: None, guard=guard()) == EXIT_FAILED
    assert done(library) == ids(7)[:1]

    monkeypatch.delenv("STUB_FAIL")
    assert run(library, "edmformer", log=lambda m: None, guard=guard()) == 0                       # the failed one is skipped, the rest runs
    assert done(library) == [t for t in ids(7) if t != ids(7)[1]]
    assert list(load_failed(library / "candidates" / "edmformer")) == [ids(7)[1]]

    assert run(library, "edmformer", log=lambda m: None, guard=guard(), retry_failed=True) == 0   # now it is tried again, and the entry goes
    assert done(library) == ids(7) and load_failed(library / "candidates" / "edmformer") == {}


@pytest.mark.parametrize("machine,force,code,started", [
    (lambda: (40.0, 1.0), False, 0, 7), (lambda: (4.0, 1.0), False, 75, 0), (lambda: (40.0, 12.0), False, 75, 0), (lambda: (4.0, 12.0), True, 0, 7)])
def test_a_busy_machine_is_refused_before_anything_starts(library, machine, force, code, started):
    log = []
    assert run(library, "edmformer", log=log.append, guard=guard(machine=machine, force=force)) == code
    assert len(processes(library)) == started
    assert (code == 75) == any("busy" in m for m in log) and EXIT_BUSY == 75


def test_the_machine_is_checked_before_every_track(library):
    readings = iter([(40.0, 1.0), (40.0, 1.0), (3.0, 1.0)])
    assert run(library, "edmformer", log=lambda m: None, guard=guard(machine=lambda: next(readings))) == EXIT_BUSY
    assert done(library) == ids(7)[:2] and load_failed(library / "candidates" / "edmformer") == {}      # a busy stop is no failure


@pytest.mark.parametrize("limit,count", [(1, 1), (3, 3), (50, 7)])
def test_max_tracks_bounds_the_run(library, limit, count):
    assert run(library, "edmformer", log=lambda m: None, guard=guard(), max_tracks=limit) == 0
    assert len(done(library)) == count


def test_a_memory_that_cannot_be_measured_stops_the_run_before_it_starts(library):
    log = []
    assert run(library, "edmformer", log=log.append, guard=guard(measure=lambda pid: None)) == EXIT_FAILED
    assert any("cannot measure" in m for m in log) and processes(library) == [] and load_failed(library / "candidates" / "edmformer") == {}


@pytest.mark.parametrize("scope,library_only,only_asked,expected", [
    (["song[01].mp3"], False, False, ids(3)[:2] + ids(7)[2:]),     # the Library first, then the rest
    (["song[01].mp3"], True, False, ids(2)),                       # only the Library
    (["song2.mp3"], True, False, ids(7)[2:3]),
    (None, True, False, [ids(7)[i] for i in (0, 2, 1, 3, 4, 5, 6)]),   # no scope: every track, the asked ones first
    (["song[01].mp3"], False, True, [ids(7)[0], ids(7)[2]]),       # asked tracks only: 0 in the Library, 2 out
    (["song[01].mp3"], True, True, [ids(7)[0]]),
])
def test_library_runs_only_tracks_inside_the_library_in_the_guarded_run(library, scope, library_only, only_asked, expected):
    (library / "rounds").mkdir()
    (library / "rounds" / "round-01.json").write_text(json.dumps({"round": 1, "probes": [{"id": "r01-p001", "track": ids(7)[0]}, {"id": "r01-p002", "track": ids(7)[2]}]}))
    if scope:
        set_scope(library, [str(library / "music" / p) for p in scope])
    log = []
    assert run(library, "edmformer", log=log.append, guard=guard(), library=library_only, only_asked=only_asked) == 0
    assert [p[0] for p in processes(library)] == expected
    assert f"edmformer: {len(expected)} tracks to do" in log


def test_the_options_reach_the_run(library, monkeypatch, capsys):
    from edm_analysis.cli import main

    monkeypatch.setattr(candidates, "read_machine", CALM)
    monkeypatch.setattr(candidates, "footprint", lambda pid: 10 * 10**6)
    assert main(["candidates", "edmformer", "--out", str(library), "--max-tracks", "2", "--max-memory-gb", "4", "--max-minutes", "0.5", "--device", "cpu"]) == 0
    assert len(processes(library)) == 2
    monkeypatch.setattr(candidates, "read_machine", lambda: (1.0, 20.0))
    assert main(["candidates", "edmformer", "--out", str(library), "--max-tracks", "2"]) == 75
    assert main(["candidates", "edmformer", "--out", str(library), "--max-tracks", "1", "--force"]) == 0
