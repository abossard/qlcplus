import json

import pytest

from edm_analysis.candidates import load

TID = "abababababababab"
RAW = {
    "allin1": {"bpm": 122, "beats": [0.1, 0.6], "downbeats": [0.12, 2.08],
               "segments": [{"start": 0.0, "end": 0.12, "label": "start"}, {"start": 0.12, "end": 46.9, "label": "intro"},
                            {"start": 46.9, "end": 86.4, "label": "chorus"}, {"start": 86.4, "end": 90.0, "label": "end"}]},
    "edmformer": [{"label": "intro", "start": 0.0, "end": 15.7}, {"label": "intro", "start": 15.7, "end": 31.3},
                  {"label": "buildup", "start": 31.3, "end": 47.0}, {"label": "drop", "start": 47.0, "end": 62.6},
                  {"label": "drop", "start": 62.6, "end": 78.2}],
    "dropdetector": {"drops": [{"time": 93.78, "confidence": 0.99}]},
}


@pytest.mark.parametrize("name,sections,drops,downbeats", [
    ("allin1", [(0.12, "intro"), (46.9, "chorus")], [], [0.12, 2.08]),
    ("edmformer", [(0.0, "intro"), (31.3, "buildup"), (47.0, "drop")], [47.0], []),
    ("dropdetector", [], [93.78], []),
])
def test_candidates_load_into_one_shape(tmp_path, name, sections, drops, downbeats):
    (tmp_path / "candidates" / name).mkdir(parents=True)
    (tmp_path / "candidates" / name / f"{TID}.json").write_text(json.dumps(RAW[name]))

    out = load(tmp_path, name, TID)

    assert [(s["start"], s["label"]) for s in out["sections"]] == sections
    assert [d["time"] for d in out["drops"]] == drops
    assert out["downbeats"] == downbeats


def test_a_track_the_candidate_has_not_seen_is_none(tmp_path):
    assert load(tmp_path, "allin1", TID) is None


def test_allin1_output_is_renamed_to_the_track_id(tmp_path):
    from edm_analysis.candidates import name_by_track

    (tmp_path / "index.json").write_text(json.dumps({TID: {"path": "/m/Song (Extended Mix).mp3"}}))
    raw = tmp_path / "candidates" / "allin1"
    raw.mkdir(parents=True)
    (raw / "Song (Extended Mix).json").write_text(json.dumps({**RAW["allin1"], "path": "/m/Song (Extended Mix).mp3"}))
    (raw / "Unknown.json").write_text(json.dumps({**RAW["allin1"], "path": "/elsewhere.mp3"}))

    name_by_track(tmp_path, raw)

    assert {f.name for f in raw.glob("*.json")} == {f"{TID}.json", "Unknown.json"}
    assert load(tmp_path, "allin1", TID)["downbeats"] == [0.12, 2.08]
