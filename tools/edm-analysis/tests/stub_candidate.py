"""A stand-in for a Candidate's process: tests choose what it does through STUB_* environment variables."""

import os
import signal
import subprocess
import sys
import time
from pathlib import Path

raw, tracks = Path(sys.argv[1]), sys.argv[2:]
mode = os.environ.get("STUB_MODE", "ok")
if os.environ.get("STUB_LOG"):
    with open(os.environ["STUB_LOG"], "a") as f:
        f.write(f"{os.getpid()} {' '.join(Path(t).stem for t in tracks)}\n")
if mode == "alloc":
    held = b"x" * (int(os.environ["STUB_MB"]) * 10**6)
if mode == "grandchild":
    child = subprocess.Popen([sys.executable, "-c", "import time; time.sleep(60)"])
    Path(os.environ["STUB_GRANDCHILD"]).write_text(str(child.pid))
if mode in ("alloc", "sleep", "grandchild"):
    time.sleep(float(os.environ.get("STUB_SECONDS", "60")))
if mode == "selfkill":
    os.kill(os.getpid(), signal.SIGKILL)
if mode == "fail":
    sys.exit(int(os.environ.get("STUB_CODE", "1")))
for t in tracks:
    stem = Path(t).stem
    if stem in os.environ.get("STUB_FAIL", "").split(","):
        sys.exit(1)
    if mode != "nooutput":
        (raw / f"{stem}.json").write_text("{}")
