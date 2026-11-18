import json

import pytest

from edm_analysis.analyze import filename_tags


@pytest.mark.parametrize("name,expected", [
    ("Biscits - Freak  (Extended Mix) 2024 [128 B Minor].mp3", (128.0, "B Minor")),
    ("Andy C - Ghost (Extended Mix) 2022 [89 A Minor].mp3", (89.0, "A Minor")),
    ("Riverside vs Lick It (VIP Edit) 2022 [128 1A].mp3", (128.0, "1A")),
    ("Better Off Alone (Bootleg) 2015 [128 12B].mp3", (128.0, "12B")),
    ("Track [174.5 F# Major].mp3", (174.5, "F# Major")),
    ("No tags here.mp3", (None, None)),
])
def test_filename_tags(name, expected):
    assert filename_tags(name) == expected


def test_renamed_track_reuses_features_and_updates_library_paths(tmp_path, monkeypatch):
    from edm_analysis.analyze import FEATURES_VERSION, STRUCTURE_VERSION, track_id
    from edm_analysis.cli import main
    from edm_analysis.final import write_final
    from test_candidates import _doc

    monkeypatch.chdir(tmp_path)
    song = tmp_path / "New name [120 1A].mp3"
    song.write_bytes(b"unchanged track contents")
    sha = track_id(song)
    out = tmp_path / "library"
    out.mkdir()
    doc = _doc(sha[:16])
    doc["track"]["sha256"] = sha
    doc["analyzer"].update(features=FEATURES_VERSION, structure=STRUCTURE_VERSION)
    raw = out / f"{sha[:16]}.json"
    raw.write_text(json.dumps(doc))
    final = write_final(out, doc)

    assert main(["analyze", song.name, "--out", str(out)]) == 0
    assert json.loads(raw.read_text())["bars"] == doc["bars"]
    for track in (json.loads(raw.read_text())["track"], json.loads(final.read_text())["track"],
                  json.loads((out / "index.json").read_text())[sha[:16]]):
        assert (track["path"], track["title"]) == (str(song.resolve()), song.stem)
    track = json.loads(raw.read_text())["track"]
    assert (track["filename_bpm"], track["filename_key"]) == (120.0, "1A")
