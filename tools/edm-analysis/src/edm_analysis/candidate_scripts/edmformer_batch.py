"""Runs inside the EDMFormer environment: one model load, one JSON per track (named by track id).

candidates.run calls it with one track per process; the device comes from EDM_DEVICE (mps or cpu, default mps)."""

import json
import os
import sys
import time
from pathlib import Path

from edm98.inference.pipeline import InferencePipeline

out = Path(sys.argv[1])
pipeline = InferencePipeline(device=os.environ.get("EDM_DEVICE", "mps"), offline=False)
for i, audio in enumerate(sys.argv[2:], 1):
    target = out / f"{Path(audio).stem}.json"
    try:
        prediction = pipeline.predict_file(audio)
    except Exception as e:  # one broken file must not stop the run
        print(f"{audio}: {type(e).__name__}: {e}", flush=True)
        continue
    tmp = target.with_suffix(".tmp")
    tmp.write_text(json.dumps(prediction))
    tmp.replace(target)
    print(f"edmformer: {i}/{len(sys.argv) - 2}", flush=True)
    time.sleep(float(os.environ.get("EDM_PAUSE", "0")))
