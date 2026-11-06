"""Calculated results are never lost: before a result file is replaced, its content is kept here.

<out>/archive/<name>/<tag>[-<n>].json.gz, one per distinct content. Tables and final files are
caches and are not archived; analyses, confidence models and Candidate outputs are.
"""

import gzip
import hashlib
from pathlib import Path


def archived(archive_dir, name):
    return sorted((Path(archive_dir) / name).glob("*.json.gz"), key=lambda p: p.stat().st_mtime_ns)


def preserve(path, archive_dir, tag):
    """Keep the current content of `path` (if any) under `tag`; returns the archive file."""
    path = Path(path)
    if not path.exists():
        return None
    data = path.read_bytes()
    digest = hashlib.sha256(data).digest()
    folder = Path(archive_dir) / path.stem
    for kept in archived(archive_dir, path.stem):
        if hashlib.sha256(gzip.decompress(kept.read_bytes())).digest() == digest:
            return kept
    folder.mkdir(parents=True, exist_ok=True)
    target, n = folder / f"{tag}.json.gz", 1
    while target.exists():
        n += 1
        target = folder / f"{tag}-{n}.json.gz"
    tmp = target.with_suffix(".tmp")
    tmp.write_bytes(gzip.compress(data))
    tmp.replace(target)
    return target


def preserve_analysis(path, out_dir):
    """Keep an analysis before it is replaced, tagged with the analyzer version that made it."""
    import json

    path = Path(path)
    if not path.exists():
        return None
    version = json.loads(path.read_text()).get("analyzer", {}).get("version", "unknown")
    return preserve(path, Path(out_dir) / "archive" / "analysis", f"v{version}")
