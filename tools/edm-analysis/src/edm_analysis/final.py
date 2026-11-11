"""final/<id>.json: the automatic analysis in the form QLC+ reads.

Reviews never reach this file: they are training and scoring data only (GLOSSARY.md, Gold).
"""

import json
from pathlib import Path

from .candidates import fused
from .confidence import load_model, with_confidence

FINAL_SCHEMA = "qlcplus-edm-final/2"
SECTION_KEYS = ("label", "start_bar", "end_bar", "start", "end", "confidence", "label_confidence", "sources")
EVENT_KEYS = ("kind", "bar", "time", "confidence", "sources")


def final_document(doc):
    pick = lambda d, keys: {k: d[k] for k in keys if k in d}
    sections = [pick(s, SECTION_KEYS) for s in doc["sections"]]
    if sections:
        sections[0]["start"] = 0.0  # a beatless opening has no bars; it belongs to the first section
    grid = {"bpm": doc["grid"]["bpm"], "period": doc["grid"]["period"], "beats_per_bar": 4,
            "first_downbeat": doc["grid"]["first_downbeat"]}
    if "bar1_confidence" in doc["grid"]:
        grid["confidence"] = doc["grid"]["bar1_confidence"]
    return {"schema": FINAL_SCHEMA, "analyzer": doc.get("analyzer", {}).get("version"),
            "track": {k: doc["track"][k] for k in ("path", "title", "sha256")},
            "grid": grid, "sections": sections, "events": [pick(e, EVENT_KEYS) for e in doc["events"]]}


def write_final(out_dir, doc):
    """doc: the rule analysis or its fusion; the file always holds the fused structure."""
    out_dir = Path(out_dir)
    doc = doc if "rules" in doc else fused(out_dir, doc)
    model = load_model(out_dir)
    if model:
        doc = with_confidence(doc, model)
    path = out_dir / "final" / f"{doc['track']['sha256'][:16]}.json"
    path.parent.mkdir(exist_ok=True)
    tmp = path.with_name(path.name + ".tmp")
    tmp.write_text(json.dumps(final_document(doc), separators=(",", ":")))
    tmp.replace(path)
    return path
