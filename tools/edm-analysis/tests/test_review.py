import json
import threading
import urllib.request

import pytest

from edm_analysis.review import ReviewServer, make_handler, parse_range


@pytest.mark.parametrize("header,size,expected", [
    (None, 1000, None),
    ("bytes=0-", 1000, (0, 999)),
    ("bytes=100-199", 1000, (100, 199)),
    ("bytes=900-5000", 1000, (900, 999)),
    ("bytes=-100", 1000, (900, 999)),
    ("bytes=2000-", 1000, "invalid"),
    ("items=0-1", 1000, "invalid"),
])
def test_parse_range(header, size, expected):
    assert parse_range(header, size) == expected


@pytest.fixture
def server(tmp_path):
    mp3 = tmp_path / "song.mp3"
    mp3.write_bytes(bytes(range(256)) * 4)
    doc = {"track": {"path": str(mp3), "sha256": "ab" * 32, "title": "song"}, "analyzer": {"version": "x"}}
    (tmp_path / "abababababababab.json").write_text(json.dumps(doc))
    (tmp_path / "index.json").write_text(json.dumps({"abababababababab": {"title": "song"}}))
    reference = {str(mp3): {"period": 0.5, "phase": 0.1, "cues": [{"name": "Energy 6", "pos": 30.0}], "breaks": [(10.0, 20.0)]}}
    httpd = ReviewServer(("127.0.0.1", 0), make_handler(tmp_path, tmp_path / "ratings.json", lambda: reference))
    threading.Thread(target=httpd.serve_forever, daemon=True).start()
    yield f"http://127.0.0.1:{httpd.server_address[1]}", tmp_path
    httpd.shutdown()


def test_review_server_serves_audio_ranges_and_persists_ratings(server):
    base, root = server

    req = urllib.request.Request(base + "/audio/abababababababab", headers={"Range": "bytes=10-19"})
    with urllib.request.urlopen(req) as r:
        assert r.status == 206
        assert r.read() == bytes(range(10, 20))

    body = json.dumps({"overall": 4}).encode()
    urllib.request.urlopen(urllib.request.Request(base + "/api/ratings/abababababababab", data=body, method="POST",
                                                  headers={"Content-Type": "application/json"}))
    with urllib.request.urlopen(base + "/api/ratings/abababababababab") as r:
        saved = json.load(r)
    assert saved["overall"] == 4
    assert saved["analyzer_version"] == "x"
    assert json.loads((root / "ratings.json").read_text())["abababababababab"]["overall"] == 4

    with urllib.request.urlopen(base + "/api/tracks") as r:
        tracks = json.load(r)
    assert tracks[0]["id"] == "abababababababab" and tracks[0]["rated"] is True


def test_review_server_serves_vdj_reference(server):
    base, root = server
    with urllib.request.urlopen(base + "/api/reference/abababababababab") as r:
        ref = json.load(r)
    assert ref == {"breaks": [[10.0, 20.0]], "cues": [{"name": "Energy 6", "pos": 30.0}], "period": 0.5, "phase": 0.1}


def test_review_server_rejects_unknown_track_ids(server):
    base, _ = server
    with pytest.raises(urllib.error.HTTPError) as e:
        urllib.request.urlopen(base + "/audio/..%2f..%2fetc")
    assert e.value.code == 404


@pytest.mark.parametrize("beats,downs", [
    ([0.5, 1.0, 1.5, 2.0], [True, False, False, False]),
    ([0.25, 0.75], [False, True]),
])
def test_click_track_puts_louder_clicks_on_bar_starts(beats, downs):
    import numpy as np
    from edm_analysis.review import click_track

    sr = 22050
    x = click_track(int(3 * sr), sr, beats, downs)

    for t, d in zip(beats, downs):
        seg = np.abs(x[int(t * sr): int((t + 0.03) * sr)])
        before = np.abs(x[int((t - 0.05) * sr): int((t - 0.01) * sr)])
        assert seg.max() > 0.2 and before.max() < 1e-6
        assert (seg.max() > 0.5) == d


@pytest.mark.parametrize("stored,kept", [("1.x", True), ("2.x", True), ("1.2", False)])
def test_a_rating_survives_a_features_only_update(server, stored, kept):
    base, root = server
    doc = json.loads((root / "abababababababab.json").read_text())
    doc["analyzer"]["version"] = "3.x"
    (root / "abababababababab.json").write_text(json.dumps(doc))
    (root / "ratings.json").write_text(json.dumps({"abababababababab": {"analyzer_version": stored, "overall": 5}}))

    with urllib.request.urlopen(base + "/api/ratings/abababababababab") as r:
        assert (json.load(r) != {}) == kept


def test_ratings_of_an_older_analysis_move_to_history_and_stay_gold(server):
    base, root = server
    old = {"analyzer_version": "old", "sections": [
        {"label": "drop", "start_bar": 8, "end_bar": 16, "verdict": "ok", "correct_label": None, "true_start_bar": None}]}
    (root / "ratings.json").write_text(json.dumps({"abababababababab": old}))

    with urllib.request.urlopen(base + "/api/ratings/abababababababab") as r:
        assert json.load(r) == {}
    history = json.loads((root / "ratings_history.json").read_text())
    assert history["abababababababab"][0]["rating"]["analyzer_version"] == "old"
    assert "abababababababab" not in json.loads((root / "ratings.json").read_text())
    with urllib.request.urlopen(base + "/api/gold/abababababababab") as r:
        gold = json.load(r)
    assert gold["events"] == [{"kind": "drop", "bar": 8, "present": True}]
    assert gold["sections"] == [{"start_bar": 8, "end_bar": 16, "label": "drop"}]


def test_ratings_endpoint_prefills_from_earlier_gold(server):
    base, root = server
    doc = json.loads((root / "abababababababab.json").read_text())
    doc.update({"analyzer": {"version": "1.new"}, "sections": [{"label": "drop", "start_bar": 8, "end_bar": 16}],
                "events": [{"kind": "drop", "bar": 8}], "bars": {"start": [0.0, 2.0]}})
    (root / "abababababababab.json").write_text(json.dumps(doc))
    old = {"analyzer_version": "1.old", "sections": [
        {"label": "drop", "start_bar": 8, "end_bar": 16, "verdict": "ok", "correct_label": None, "true_start_bar": None}]}
    (root / "ratings.json").write_text(json.dumps({"abababababababab": old}))

    with urllib.request.urlopen(base + "/api/ratings/abababababababab") as r:
        rating = json.load(r)

    assert rating["prefilled"] is True
    assert rating["sections"][0]["verdict"] == "ok"
