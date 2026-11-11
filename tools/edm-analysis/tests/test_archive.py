import gzip
import json

import pytest

from edm_analysis.archive import archived, preserve


def test_nothing_to_keep_when_the_file_does_not_exist(tmp_path):
    assert preserve(tmp_path / "a.json", tmp_path / "archive", "v1") is None
    assert not (tmp_path / "archive").exists()


@pytest.mark.parametrize("tags,expected", [
    (["v1"], 1),             # one earlier version kept
    (["v1", "v1"], 1),       # the same content twice is kept once
    (["v1", "v2"], 2),       # each version kept
])
def test_earlier_results_are_kept_compressed(tmp_path, tags, expected):
    path = tmp_path / "abababababababab.json"
    for tag in tags:
        path.write_text(json.dumps({"version": tag}))
        preserve(path, tmp_path / "archive", tag)

    kept = archived(tmp_path / "archive", "abababababababab")
    assert len(kept) == expected
    assert json.loads(gzip.decompress(kept[-1].read_bytes()))["version"] == tags[-1]


def test_changed_content_under_the_same_tag_is_kept_too(tmp_path):
    path = tmp_path / "abababababababab.json"
    for content in ("first", "second"):
        path.write_text(json.dumps({"v": content}))
        preserve(path, tmp_path / "archive", "v1")
    contents = sorted(json.loads(gzip.decompress(p.read_bytes()))["v"] for p in archived(tmp_path / "archive", "abababababababab"))
    assert contents == ["first", "second"]


def test_relabel_keeps_the_analysis_it_replaces(tmp_path):
    from edm_analysis.analyze import STRUCTURE_VERSION
    from edm_analysis.cli import main
    from test_transform import DOC, TID

    old = {**DOC, "analyzer": {"features": 2, "structure": STRUCTURE_VERSION - 1, "version": f"2.{STRUCTURE_VERSION - 1}"}}
    (tmp_path / f"{TID}.json").write_text(json.dumps(old))

    assert main(["relabel", "--out", str(tmp_path)]) == 0

    kept = archived(tmp_path / "archive" / "analysis", TID)
    assert [p.name for p in kept] == [f"v2.{STRUCTURE_VERSION - 1}.json.gz"]
    assert json.loads(gzip.decompress(kept[0].read_bytes())) == old
    assert json.loads((tmp_path / f"{TID}.json").read_text())["analyzer"]["structure"] == STRUCTURE_VERSION


def test_a_model_output_that_would_be_replaced_is_kept(tmp_path):
    from edm_analysis.candidates import name_by_track

    (tmp_path / "index.json").write_text(json.dumps({"abababababababab": {"path": "/m/Song.mp3"}}))
    raw = tmp_path / "candidates" / "allin1"
    raw.mkdir(parents=True)
    (raw / "abababababababab.json").write_text(json.dumps({"path": "/m/Song.mp3", "run": 1}))
    (raw / "Song.json").write_text(json.dumps({"path": "/m/Song.mp3", "run": 2}))

    name_by_track(tmp_path, raw)

    kept = archived(tmp_path / "archive" / "candidates" / "allin1", "abababababababab")
    assert [json.loads(gzip.decompress(p.read_bytes()))["run"] for p in kept] == [1]
    assert json.loads((raw / "abababababababab.json").read_text())["run"] == 2
