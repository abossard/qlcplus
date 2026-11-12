import json

import pytest

from edm_analysis.cli import main

from test_candidates import _doc

TIDS = ["abababababababab", "cdcdcdcdcdcdcdcd"]


@pytest.fixture
def library(tmp_path, monkeypatch):
    monkeypatch.setattr("edm_analysis.vdj.DEFAULT_DB", tmp_path / "no-database.xml")
    for tid in TIDS:
        doc = _doc(tid)
        doc["analyzer"] = {"features": 2, "structure": 5, "version": "2.5"}
        (tmp_path / f"{tid}.json").write_text(json.dumps(doc))
    (tmp_path / "candidates" / "dropdetector").mkdir(parents=True)
    (tmp_path / "candidates" / "dropdetector" / f"{TIDS[0]}.json").write_text(json.dumps({"drops": [{"time": 36.0, "confidence": 0.95}]}))
    (tmp_path / "rounds").mkdir()
    (tmp_path / "rounds" / "answers.json").write_text(json.dumps({
        "r01-p001": {"round": 1, "track": TIDS[0], "answer": "bar:18", "claim": {"kind": "drop", "bar": 16, "time": 32.0, "confidence": 50}}}))
    return tmp_path


def snapshot(folder):
    return {p.relative_to(folder): p.read_bytes() for p in sorted(folder.rglob("*")) if p.is_file()}


def test_rebuild_writes_every_final_file_from_the_raw_outputs(library):
    assert main(["rebuild", "--out", str(library)]) == 0

    finals = {tid: json.loads((library / "final" / f"{tid}.json").read_text()) for tid in TIDS}
    drops = lambda final: [(s["start_bar"], s["sources"], s["confidence"]) for s in final["sections"] if s["label"] == "drop"]
    assert [d[:2] for d in drops(finals[TIDS[0]])] == [(18, ["dropdetector"])]       # moved onto the detector's drop
    assert [d[:2] for d in drops(finals[TIDS[1]])] == [(16, ["rules"])]              # no Candidate output: the rules stand
    assert all(isinstance(s["confidence"], int) for f in finals.values() for s in f["sections"])
    assert finals[TIDS[0]]["analyzer"] == "2.5.3"
    assert json.loads((library / "confidence-model.json").read_text())["analyzer"] == ["2.5.3"]     # the structure the model was fitted on


def test_rebuild_leaves_the_raw_outputs_as_they_were(library):
    raw = {p: b for p, b in snapshot(library).items() if p.parts[0] in ("candidates", "rounds") or p.name in [f"{t}.json" for t in TIDS]}

    main(["rebuild", "--out", str(library)])

    assert {p: b for p, b in snapshot(library).items() if p in raw} == raw


def test_rebuild_can_be_rerun(library):
    main(["rebuild", "--out", str(library)])
    first = snapshot(library)

    assert main(["rebuild", "--out", str(library)]) == 0

    assert snapshot(library) == first


def test_rebuild_recomputes_the_rules_where_their_version_is_old(library):
    from edm_analysis.analyze import STRUCTURE_VERSION

    doc = json.loads((library / f"{TIDS[1]}.json").read_text())
    (library / f"{TIDS[1]}.json").write_text(json.dumps({**doc, "analyzer": {"features": 2, "structure": STRUCTURE_VERSION - 1, "version": "2.0"}}))

    main(["rebuild", "--out", str(library)])

    assert json.loads((library / f"{TIDS[1]}.json").read_text())["analyzer"]["structure"] == STRUCTURE_VERSION


def test_confidence_reports_each_group_held_out_and_the_share_of_sure_claims(library, capsys):
    assert main(["confidence", "--out", str(library)]) == 0

    out = capsys.readouterr().out
    for part in ("grid:", "start:", "label:", "each track held out", "Claims at 95% or more", "grid ", "start ", "label "):
        assert part in out, part
    assert json.loads((library / "final" / f"{TIDS[0]}.json").read_text())["sections"][0]["confidence"] == 100


def test_compare_prints_tracks_side_by_side_by_id_prefix(library, capsys):
    assert main(["compare", "--out", str(library), "--tracks", TIDS[0][:6]]) == 0

    out = capsys.readouterr().out
    assert "rules" in out and "fused" in out and "drop 16-40" in out and "drop 18-40" in out and "drop 16: bar:18 (gold bar 18)" in out
    assert TIDS[1][:6] not in out


def test_compare_names_a_track_it_does_not_know(library, capsys):
    assert main(["compare", "--out", str(library), "--tracks", "ffffff"]) == 1
    assert "ffffff" in capsys.readouterr().err
