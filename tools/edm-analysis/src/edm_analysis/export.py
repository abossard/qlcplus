"""`edm-analysis export`: rebuild <out>/tables/*.parquet from the raw outputs (see transform.py).

The tables are derived data: delete them any time, run export again. Raw outputs are only read.
- claims.parquet: one row per source, track and claim, placed on our bar grid (sources: rules, fused, each Candidate, VirtualDJ, Mixed In Key);
- bars.parquet: one row per track and bar: our levels, each source's claims there, Gold;
- answers.parquet: one row per answered Probe;
- tracks.parquet: one row per track, with its style group and the sources that cover it.
"""

import os
import tempfile
from pathlib import Path

import pyarrow as pa
import pyarrow.parquet as pq

from . import transform
from .candidates import NAMES, load
from .confidence import load_model, with_confidence
from .evaluate import library_docs
from .rounds import load_answers, style_groups


def _table(rows):
    keys = list(dict.fromkeys(k for r in rows for k in r))
    return pa.Table.from_pylist([{k: r.get(k) for k in keys} for r in rows])


def _write(path, rows):
    fd, tmp = tempfile.mkstemp(dir=path.parent, prefix=path.name, suffix=".tmp")
    os.close(fd)
    pq.write_table(_table(rows), tmp)
    os.replace(tmp, path)


def export(out_dir, vdj_by_path, log=print):
    out_dir = Path(out_dir)
    model = load_model(out_dir)
    docs = {tid: with_confidence(d, model) if model else d for tid, d in library_docs(out_dir).items()}
    answers = transform.answer_rows(load_answers(out_dir), docs)
    groups = style_groups(docs) if len(docs) >= 8 else {}
    claims, bars, tracks = [], [], []
    for tid, doc in docs.items():
        mine = transform.claims_ours(tid, doc) + transform.claims_silver(tid, doc, vdj_by_path.get(doc["track"]["path"]))
        for name in NAMES:
            loaded = load(out_dir, name, tid)
            if loaded is not None:
                mine += transform.claims_candidate(tid, doc, name, loaded)
        claims += mine
        bars += transform.bar_rows(tid, doc, mine, answers)
        tracks.append(transform.track_row(tid, doc, {c["source"] for c in mine}, groups.get(tid)))
    folder = out_dir / "tables"
    folder.mkdir(exist_ok=True)
    for name, rows in (("claims", claims), ("bars", bars), ("answers", answers), ("tracks", tracks)):
        if rows:
            _write(folder / f"{name}.parquet", rows)
        log(f"{name}: {len(rows)} rows")
    return folder
