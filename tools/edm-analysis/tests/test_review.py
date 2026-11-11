import json
import subprocess
import threading
import time
import urllib.request

import numpy as np
import pytest

from edm_analysis import review
from edm_analysis.review import Clips, LibraryReport, ReviewServer, make_handler, parse_range


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


# --- probe clips, round media, report ------------------------------------------------------------

TID = "cdcdcdcdcdcdcdcd"
SR = 44100
CLIP_ROUND = {"round": 1, "created": "2026-11-05T10:00:00", "theories": [{"id": "t1", "text": "Drops are late"}], "probes": [
    {"id": "r01-p001", "track": TID, "title": "song", "theory": "t1", "bucket": "unsure", "question": "yesno",
     "prompt": "Does the drop start at the marker?", "claim": {"kind": "drop", "bar": 3, "time": 6.0, "confidence": 41},
     "plays": [{"id": "main", "start": 4.0, "end": 10.0, "mark": 6.0}]},
    {"id": "r01-p002", "track": TID, "title": "song", "theory": "t1", "bucket": "unsure", "question": "ab",
     "prompt": "Which one has the accent on bar 1?", "claim": {"kind": "grid", "bar": 0, "time": 6.0, "confidence": 41},
     "plays": [{"id": "a", "start": 4.0, "end": 10.0, "mark": 6.0, "click_shift": 0},
               {"id": "b", "start": 4.0, "end": 10.0, "mark": 6.0, "click_shift": 1}],
     "alternatives": {"a": {"source": "ours", "shift_beats": 0}, "b": {"source": "vdj", "shift_beats": 1}}},
]}


@pytest.fixture(scope="session")
def tone(tmp_path_factory):
    path = tmp_path_factory.mktemp("clip-audio") / "tone.mp3"
    subprocess.run(["ffmpeg", "-v", "error", "-f", "lavfi", "-i", "sine=frequency=220:duration=40", "-ac", "2", "-b:a", "128k", str(path)], check=True)
    return path


@pytest.fixture
def clip_site(tmp_path, tone):
    doc = {"track": {"path": str(tone), "title": "song", "duration": 40.0, "sha256": "cd" * 32}, "analyzer": {"version": "x"},
           "grid": {"bpm": 120.0, "period": 0.5, "first_downbeat": 0.0}, "beats": [0.5 * i for i in range(80)],
           "bars": {"start": [2.0 * k for k in range(20)], "levels": {}},
           "peaks": {"rate": 20, "values": [round(0.1 + 0.8 * (i % 10) / 10, 2) for i in range(800)]},
           "sections": [{"label": "intro", "start_bar": 0, "end_bar": 8, "confidence": 77},
                        {"label": "drop", "start_bar": 8, "end_bar": 20, "confidence": 31}], "events": []}
    (tmp_path / f"{TID}.json").write_text(json.dumps(doc))
    (tmp_path / "index.json").write_text(json.dumps({TID: {"title": "song", "path": str(tone)}}))
    (tmp_path / "rounds").mkdir()
    (tmp_path / "rounds" / "round-01.json").write_text(json.dumps(CLIP_ROUND))
    httpd = ReviewServer(("127.0.0.1", 0), make_handler(tmp_path, tmp_path / "ratings.json"))
    threading.Thread(target=httpd.serve_forever, daemon=True).start()
    yield f"http://127.0.0.1:{httpd.server_address[1]}", tmp_path
    httpd.shutdown()


def fetch(url, headers=None):
    with urllib.request.urlopen(urllib.request.Request(url, headers=headers or {})) as r:
        return r.status, dict(r.headers), r.read()


def pcm(mp3_bytes):
    out = subprocess.run(["ffmpeg", "-v", "error", "-i", "-", "-f", "f32le", "-ac", "1", "-ar", str(SR), "-"], input=mp3_bytes,
                         capture_output=True, check=True).stdout
    return np.frombuffer(out, np.float32)


def tone_energy(x, freq, t0, t1):
    seg = x[int(t0 * SR):int(t1 * SR)] * np.hanning(int((t1 - t0) * SR))
    spec = np.abs(np.fft.rfft(seg, SR)) ** 2   # 1 Hz bins
    return float(spec[int(freq) - 30:int(freq) + 30].sum())


@pytest.mark.parametrize("path,start,end", [("/clip/r01-p001/main", 4.0, 10.0), ("/clip/r01-p002/b", 4.0, 10.0)])
def test_a_clip_is_only_the_stretch_of_its_play(clip_site, path, start, end):
    base, _ = clip_site
    status, headers, body = fetch(base + path)
    assert status == 200 and headers["Content-Type"] == "audio/mpeg" and headers["Accept-Ranges"] == "bytes"
    assert len(pcm(body)) / SR == pytest.approx(end - start, abs=0.15)
    assert (float(headers["X-Clip-Start"]), float(headers["X-Clip-End"])) == (start, end)
    assert "render;dur=" in headers["Server-Timing"]


@pytest.mark.parametrize("path,present,absent", [
    ("/clip/r01-p002/a", 2.0, 2.5),    # accent on bar 1 at 6.0 s, clip time 2.0
    ("/clip/r01-p002/b", 2.5, 2.0),    # shifted one beat: the accent is at 6.5 s
])
def test_a_clip_with_click_shift_carries_the_accented_clicks_of_its_grid(clip_site, path, present, absent):
    base, _ = clip_site
    x = pcm(fetch(base + path)[2])
    loud = tone_energy(x, 1760, present, present + 0.06)
    assert loud > 20 * tone_energy(x, 1760, absent, absent + 0.06) and loud > 20 * tone_energy(x, 1760, 1.0, 1.06)


@pytest.mark.parametrize("query,cue", [("", False), ("?cue=0", False), ("?cue=1", True)])
def test_the_cue_blip_sits_exactly_on_the_mark_and_only_when_asked(clip_site, query, cue):
    base, _ = clip_site
    x = pcm(fetch(base + "/clip/r01-p001/main" + query)[2])
    at_mark, before = tone_energy(x, 3520, 2.0, 2.06), tone_energy(x, 3520, 1.5, 1.56)
    assert (at_mark > 1 and at_mark > 20 * before) == cue
    assert np.abs(x).max() < 0.2 or cue   # the tone alone is quiet: a play without click_shift has no beat clicks


def test_the_cue_does_not_apply_to_a_play_with_click_shift(clip_site):
    base, root = clip_site
    assert fetch(base + "/clip/r01-p002/a?cue=1")[2] == fetch(base + "/clip/r01-p002/a")[2]


@pytest.mark.parametrize("rng,status,content_range", [
    (None, 200, None), ("bytes=0-9", 206, "bytes 0-9/{size}"), ("bytes=-100", 206, "bytes {a}-{b}/{size}"),
    ("bytes=999999999-", 416, "bytes */{size}"), ("items=0-1", 416, "bytes */{size}"),
])
def test_clips_support_ranges(clip_site, rng, status, content_range):
    base, _ = clip_site
    size = len(fetch(base + "/clip/r01-p001/main")[2])
    try:
        code, headers, body = fetch(base + "/clip/r01-p001/main", {"Range": rng} if rng else None)
    except urllib.error.HTTPError as e:
        code, headers, body = e.code, dict(e.headers), b""
    assert code == status
    assert headers.get("Content-Range") == (content_range.format(size=size, a=size - 100, b=size - 1) if content_range else None)
    if code == 206:
        assert len(body) == int(headers["Content-Length"])


def clip_files(root):
    return sorted(p.name for p in (root / ".clips").glob("*.mp3")) if (root / ".clips").exists() else []


def test_prerendering_a_round_leaves_nothing_to_render_when_a_clip_is_asked_for(clip_site):
    base, root = clip_site
    clips = Clips(root)
    clips.prerender(CLIP_ROUND)
    # main (plain, cue), a (clicks), b (clicks): a play with click_shift has no cue variant
    assert len(clip_files(root)) == 4
    import re
    assert all(re.fullmatch(r"r01-p00[12]_(main|a|b)(_cue)?_[0-9a-f]{12}\.mp3", n) for n in clip_files(root)), clip_files(root)
    for path in ("/clip/r01-p001/main", "/clip/r01-p001/main?cue=1", "/clip/r01-p002/a", "/clip/r01-p002/b"):
        assert float(fetch(base + path)[1]["Server-Timing"].split("dur=")[1]) < 50
    assert len(clip_files(root)) == 4


def test_the_first_request_for_a_clip_renders_it_in_under_two_seconds(clip_site):
    base, root = clip_site
    t = time.time()
    fetch(base + "/clip/r01-p002/b")
    assert time.time() - t < 2 and len(clip_files(root)) == 1


def test_a_changed_play_gets_a_new_clip(clip_site):
    base, root = clip_site
    first = len(pcm(fetch(base + "/clip/r01-p001/main")[2])) / SR
    changed = json.loads(json.dumps(CLIP_ROUND))
    changed["probes"][0]["plays"][0]["end"] = 8.0
    (root / "rounds" / "round-01.json").write_text(json.dumps(changed))
    assert len(pcm(fetch(base + "/clip/r01-p001/main")[2])) / SR == pytest.approx(first - 2.0, abs=0.15)
    assert len(clip_files(root)) == 2


def test_the_background_thread_renders_the_clips_of_a_round_when_it_appears(clip_site):
    base, root = clip_site
    stop = threading.Event()
    (root / "rounds" / "round-01.json").rename(root / "round-01.json")
    t = threading.Thread(target=review.warm, args=(root, Clips(root), LibraryReport(root), stop, 0.05), daemon=True)
    t.start()
    time.sleep(0.4)
    assert clip_files(root) == []
    (root / "round-01.json").rename(root / "rounds" / "round-01.json")
    end = time.time() + 15
    while time.time() < end and len(clip_files(root)) < 4:
        time.sleep(0.1)
    stop.set()
    assert len(clip_files(root)) == 4


def test_the_round_carries_a_waveform_and_bar_lines_for_each_play_but_no_labels(clip_site):
    base, _ = clip_site
    rnd = json.loads(fetch(base + "/api/round")[2])
    main, a, b = rnd["probes"][0]["plays"][0], *rnd["probes"][1]["plays"]
    assert main["peaks"]["rate"] == 20 and main["peaks"]["t0"] == 4.0 and len(main["peaks"]["values"]) == 120
    # [time, accent, bar index]: bar starts every 2 s on this grid, so the accents are the bar lines
    assert [g for g in main["grid"] if g[1]] == [[4.0, 1, 2], [6.0, 1, 3], [8.0, 1, 4], [10.0, 1, 5]]
    assert [g[0] for g in main["grid"] if not g[1]][:3] == [4.5, 5.0, 5.5]
    assert [g[0] for g in b["grid"] if g[1]] == [4.5, 6.5, 8.5] and all(g[2] is None for g in b["grid"] if g[1])
    body = fetch(base + "/api/round")[2]
    for hidden in (b'"sections"', b'"intro"', b'"drop"', b'"confidence"', b'"bucket"', b'"source"', b'"path"'):
        assert hidden not in body


def test_reference_adds_what_each_candidate_found(clip_site):
    base, root = clip_site
    assert "candidates" not in json.loads(fetch(base + f"/api/reference/{TID}")[2])
    for name, raw in {"edmformer": [{"start": 0.0, "label": "intro"}, {"start": 8.0, "label": "drop"}],
                      "allin1": {"segments": [{"start": 0.0, "label": "start"}, {"start": 1.0, "label": "intro"}], "downbeats": [1.0] * 5000},
                      "dropdetector": {"drops": [{"time": 16.0, "confidence": 0.83}]}}.items():
        (root / "candidates" / name).mkdir(parents=True)
        (root / "candidates" / name / f"{TID}.json").write_text(json.dumps(raw))
    ref = json.loads(fetch(base + f"/api/reference/{TID}")[2])
    assert ref["candidates"] == {"edmformer": {"sections": [{"start": 0.0, "label": "intro"}, {"start": 8.0, "label": "drop"}], "drops": [{"time": 8.0, "confidence": None}]},
                                 "allin1": {"sections": [{"start": 1.0, "label": "intro"}], "drops": []},
                                 "dropdetector": {"sections": [], "drops": [{"time": 16.0, "confidence": 0.83}]}}


def test_the_report_comes_from_rounds_report_over_the_library(clip_site, monkeypatch):
    base, root = clip_site
    seen = {}

    def fake(rounds_, answers, docs):
        seen.update(rounds=rounds_, answers=answers, docs=docs)
        return {"rounds": [], "sources": {"vdj": {"n": 3, "exact": 0.5}}}
    monkeypatch.setattr(review.rounds, "report", fake)
    assert json.loads(fetch(base + "/api/report")[2]) == {"rounds": [], "sources": {"vdj": {"n": 3, "exact": 0.5}}}
    assert [r["round"] for r in seen["rounds"]] == [1] and list(seen["docs"]) == [TID]
    assert "peaks" not in seen["docs"][TID] and seen["docs"][TID]["sections"][1]["confidence"] == 31


def test_the_report_reads_a_document_again_only_when_its_file_changed(clip_site, monkeypatch):
    base, root = clip_site
    docs = []
    monkeypatch.setattr(review.rounds, "report", lambda r, a, d: docs.append(dict(d)) or {})
    fetch(base + "/api/report"), fetch(base + "/api/report")
    assert docs[0][TID] is docs[1][TID]
    path = root / f"{TID}.json"
    path.write_text(path.read_text() + " ")
    fetch(base + "/api/report")
    assert docs[2][TID] is not docs[1][TID]


def test_a_slow_report_never_holds_up_other_requests(clip_site, monkeypatch):
    base, _ = clip_site
    monkeypatch.setattr(review.rounds, "report", lambda r, a, d: time.sleep(1.5) or {})
    threads = [threading.Thread(target=fetch, args=(base + "/api/report",)) for _ in range(2)]
    [t.start() for t in threads]
    time.sleep(0.2)
    t0 = time.time()
    for path in ("/api/tracks", "/api/round", "/clip/r01-p001/main"):
        fetch(base + path)
    assert time.time() - t0 < 1.0
    [t.join() for t in threads]


@pytest.mark.parametrize("name,kept", [
    ("r01-p001_main_aaaaaaaaaaaa.mp3", True), ("r01-p002_a_bbbbbbbbbbbb.mp3", True), ("r01-p001_main_cue_cccccccccccc.mp3", True),
    ("r00-p001_main_dddddddddddd.mp3", False), ("r01-p099_main_eeeeeeeeeeee.mp3", False), ("r02-p001_a_ffffffffffff.mp3", False),
    ("r01-p001_main_aaaaaaaaaaaa.mp3.4242.tmp", True),   # a clip being written is not touched
])
def test_clips_of_no_probe_of_the_current_round_are_removed(clip_site, name, kept):
    base, root = clip_site
    (root / ".clips").mkdir()
    (root / ".clips" / name).write_bytes(b"x")
    Clips(root).prune(CLIP_ROUND)
    assert (root / ".clips" / name).exists() == kept


def test_the_background_thread_drops_the_clips_of_an_earlier_round_when_a_new_round_appears(clip_site):
    base, root = clip_site
    first = Clips(root)
    first.prerender(CLIP_ROUND)
    assert len(clip_files(root)) == 4
    newer = json.loads(json.dumps(CLIP_ROUND))
    newer["round"] = 2
    for probe in newer["probes"]:
        probe["id"] = probe["id"].replace("r01", "r02")
    stop = threading.Event()
    (root / "rounds" / "round-02.json").write_text(json.dumps(newer))
    threading.Thread(target=review.warm, args=(root, first, LibraryReport(root), stop, 0.05), daemon=True).start()
    end = time.time() + 15
    while time.time() < end and not (len(clip_files(root)) == 4 and all(n.startswith("r02-") for n in clip_files(root))):
        time.sleep(0.1)
    stop.set()
    assert len(clip_files(root)) == 4 and all(n.startswith("r02-") for n in clip_files(root)), clip_files(root)
