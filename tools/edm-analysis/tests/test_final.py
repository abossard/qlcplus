import json

import pytest

from edm_analysis.final import write_final

DOC = {"track": {"path": "/m/a.mp3", "title": "a", "sha256": "ab" * 32}, "analyzer": {"version": "2.5"},
       "grid": {"bpm": 120.0, "period": 0.5, "first_downbeat": 0.0},
       "bars": {"start": [2.0 * k for k in range(20)],
                "levels": {k: [0.5] * 20 for k in ("kick", "hihat", "bass", "instruments", "vocal", "loud")}},
       "sections": [{"label": "intro", "start_bar": 0, "end_bar": 8, "start": 0.0, "end": 16.0},
                    {"label": "drop", "start_bar": 8, "end_bar": 20, "start": 16.0, "end": 40.0}],
       "events": [{"kind": "drop", "bar": 8, "time": 16.0}, {"kind": "vocal_in", "bar": 10, "time": 20.0}]}


def test_ratings_never_reach_the_final_file(tmp_path):
    (tmp_path / "ratings.json").write_text(json.dumps({"abababababababab": {"analyzer_version": "2.5", "grid": {"shift_beats": 1}, "sections": [
        {"label": "drop", "start_bar": 8, "end_bar": 20, "verdict": "wrong", "correct_label": "groove", "true_start_bar": None}]}}))
    (tmp_path / "ratings_history.json").write_text(json.dumps({"abababababababab": [{"analyzer_version": "1.3", "gold": {
        "sections": [{"start_bar": 4, "end_bar": 8, "label": "buildup"}], "events": []}}]}))

    path = write_final(tmp_path, DOC)

    final = json.loads(path.read_text())
    assert path == tmp_path / "final" / "abababababababab.json"
    assert [(s["label"], s["start_bar"], s["start"]) for s in final["sections"]] == [("intro", 0, 0.0), ("drop", 8, 16.0)]
    assert [(e["kind"], e["bar"], e["time"]) for e in final["events"]] == [("drop", 8, 16.0), ("vocal_in", 10, 20.0)]
    assert final["grid"]["first_downbeat"] == 0.0
    assert "reviewed" not in json.dumps(final)


@pytest.mark.parametrize("first_bar", [12.0, 0.4])
def test_the_first_section_starts_at_the_beginning_of_the_track(tmp_path, first_bar):
    doc = {**DOC, "grid": {**DOC["grid"], "first_downbeat": first_bar},
           "sections": [{**DOC["sections"][0], "start": first_bar}, DOC["sections"][1]]}

    final = json.loads(write_final(tmp_path, doc).read_text())

    assert final["sections"][0]["start"] == 0.0
    assert final["sections"][1]["start"] == 16.0


def test_the_final_file_is_the_fused_structure_with_its_sources(tmp_path):
    (tmp_path / "candidates" / "dropdetector").mkdir(parents=True)
    (tmp_path / "candidates" / "dropdetector" / "abababababababab.json").write_text(json.dumps({"drops": [{"time": 20.0, "confidence": 0.95}]}))

    final = json.loads(write_final(tmp_path, DOC).read_text())

    assert [(s["label"], s["start_bar"], s["sources"]) for s in final["sections"]] == [("intro", 0, ["rules"]), ("drop", 10, ["dropdetector"])]
    assert [(e["kind"], e["bar"], e["sources"]) for e in final["events"] if e["kind"] == "drop"] == [("drop", 10, ["dropdetector"])]
    assert final["analyzer"] == "2.5.1"


def test_a_fused_document_is_written_as_it_is(tmp_path):
    from edm_analysis.candidates import fused

    doc = fused(tmp_path, DOC)
    assert json.loads(write_final(tmp_path, doc).read_text()) == json.loads(write_final(tmp_path, DOC).read_text())
