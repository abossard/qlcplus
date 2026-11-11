import json
import subprocess
import threading
import time
import urllib.request

import numpy as np
import pytest

from edm_analysis import candidates, confidence, library, review
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
    doc.update({"analyzer": {"version": "1.new"}, "grid": {"bpm": 120.0, "period": 0.5, "first_downbeat": 0.0}, "sections": [{"label": "drop", "start_bar": 8, "end_bar": 16}],
                "events": [{"kind": "drop", "bar": 8}], "bars": {"start": [2.0 * k for k in range(17)], "levels": {n: [0.5] * 17 for n in ("loud", "kick", "hihat", "bass", "instruments", "vocal")}}})
    (root / "abababababababab.json").write_text(json.dumps(doc))
    old = {"analyzer_version": "1.old", "sections": [
        {"label": "drop", "start_bar": 8, "end_bar": 16, "verdict": "ok", "correct_label": None, "true_start_bar": None}]}
    (root / "ratings.json").write_text(json.dumps({"abababababababab": old}))

    with urllib.request.urlopen(base + "/api/ratings/abababababababab") as r:
        rating = json.load(r)

    assert rating["prefilled"] is True
    assert next(s for s in rating["sections"] if s["label"] == "drop")["verdict"] == "ok"   # the fusion also starts an intro at bar 0, which no Gold covers


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
           "bars": {"start": [2.0 * k for k in range(20)], "levels": {n: [0.2] * 8 + [0.9] * 12 for n in ("loud", "kick", "hihat", "bass", "instruments", "vocal")}},
           "peaks": {"rate": 20, "values": [round(0.1 + 0.8 * (i % 10) / 10, 2) for i in range(800)]},
           "sections": [{"label": "intro", "start_bar": 0, "end_bar": 8, "confidence": 77},
                        {"label": "drop", "start_bar": 8, "end_bar": 20, "confidence": 31}], "events": []}
    (tmp_path / f"{TID}.json").write_text(json.dumps(doc))
    (tmp_path / "index.json").write_text(json.dumps({TID: {"title": "song", "path": str(tone), "drops": 9, "sections": ["drop"] * 9}}))
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


@pytest.mark.parametrize("model", [False, True])
def test_the_report_comes_from_rounds_report_over_the_library(clip_site, monkeypatch, model):
    base, root = clip_site
    seen = {}
    if model:
        (root / confidence.MODEL_FILE).write_text(json.dumps({"groups": {}}))

    def fake(rounds_, answers, docs):
        seen.update(rounds=rounds_, answers=answers, docs=docs)
        return {"rounds": [], "sources": {"vdj": {"n": 3, "exact": 0.5}}}
    monkeypatch.setattr(review.rounds, "report", fake)
    assert json.loads(fetch(base + "/api/report")[2]) == {"rounds": [], "sources": {"vdj": {"n": 3, "exact": 0.5}}}
    assert [r["round"] for r in seen["rounds"]] == [1] and list(seen["docs"]) == [TID]
    doc = seen["docs"][TID]   # the Analysis QLC+ gets: fused, with the model's confidence, without the large arrays
    assert "peaks" not in doc and doc["analyzer"]["fusion"] == 1 and all("sources" in s for s in doc["sections"])
    assert [s.get("confidence") for s in doc["sections"] if s["start_bar"]] == [50 if model else None]


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


@pytest.fixture
def broken_music(monkeypatch):
    """Clip rendering fails (as when the music folder cannot be read) while state["broken"] is set."""
    state = {"broken": True, "calls": 0}
    real = review.render_clip

    def render_clip(*args):
        state["calls"] += 1
        if state["broken"]:
            raise subprocess.CalledProcessError(252, "ffmpeg")
        return real(*args)
    monkeypatch.setattr(review, "render_clip", render_clip)
    return state


@pytest.mark.parametrize("broken,missing", [(True, 4), (False, 0)])
def test_prerendering_says_how_many_clips_are_still_missing(clip_site, broken_music, broken, missing):
    _, root = clip_site
    broken_music["broken"] = broken
    assert Clips(root).prerender(CLIP_ROUND) == missing
    assert len(clip_files(root)) == 4 - missing


@pytest.mark.parametrize("streaks", [1, 2])
def test_a_failing_clip_is_reported_once_per_failure_streak(clip_site, broken_music, capsys, streaks):
    _, root = clip_site
    clips = Clips(root)
    for _ in range(streaks):
        broken_music["broken"] = True
        for _ in range(3):
            clips.prerender(CLIP_ROUND)
        broken_music["broken"] = False
        assert clips.prerender(CLIP_ROUND) == 0
        for f in (root / ".clips").glob("*.mp3"):
            f.unlink()
    lines = capsys.readouterr().err.splitlines()
    assert len(lines) == 4 * streaks and len(set(lines)) == 4 and all("r01-p00" in line for line in lines)


@pytest.mark.parametrize("fixed_after", [0.4, 1.0])
def test_the_background_thread_keeps_retrying_missing_clips_until_all_exist(clip_site, broken_music, capsys, fixed_after):
    _, root = clip_site
    stop = threading.Event()
    threading.Thread(target=review.warm, args=(root, Clips(root), LibraryReport(root), stop, 0.05), daemon=True).start()
    time.sleep(fixed_after)
    assert clip_files(root) == [] and broken_music["calls"] > 4      # it went on trying, nothing changed on disk
    broken_music["broken"] = False
    end = time.time() + 15
    while time.time() < end and len(clip_files(root)) < 4:
        time.sleep(0.1)
    stop.set()
    assert len(clip_files(root)) == 4
    assert len(capsys.readouterr().err.splitlines()) == 4              # one line per clip, not one per retry


@pytest.mark.parametrize("broken,code,said", [(False, 0, "all clips ready"), (True, 1, "4 clips missing")])
def test_review_prerender_makes_the_clips_of_the_current_round_and_exits(clip_site, broken_music, capsys, broken, code, said):
    from edm_analysis.cli import main

    _, root = clip_site
    broken_music["broken"] = broken
    assert main(["review", "--out", str(root), "--prerender"]) == code
    assert said in capsys.readouterr().out
    assert len(clip_files(root)) == (0 if broken else 4)


class Timed(Clips):
    """Clips that note when each pass began."""

    def __init__(self, out_dir):
        super().__init__(out_dir)
        self.times = []

    def prerender(self, rnd, answers=None):
        self.times.append(time.monotonic())
        return super().prerender(rnd, answers)


def wait_until(check, wait=15.0):
    end = time.time() + wait
    while time.time() < end and not check():
        time.sleep(0.05)
    return check()


def start_warm(root, clips, longest=0.4):
    stop = threading.Event()
    threading.Thread(target=review.warm, args=(root, clips, LibraryReport(root), stop, 0.05, longest), daemon=True).start()
    return stop


def test_the_wait_between_retries_doubles_up_to_the_longest(clip_site, broken_music):
    _, root = clip_site
    clips = Timed(root)
    stop = start_warm(root, clips)
    assert wait_until(lambda: len(clips.times) >= 7)
    stop.set()
    gaps = [b - a for a, b in zip(clips.times, clips.times[1:])]
    for gap, wanted in zip(gaps, [0.05, 0.1, 0.2, 0.4, 0.4, 0.4]):
        assert wanted * 0.9 <= gap <= wanted + 0.2, gaps       # grows, then stays at the longest


@pytest.mark.parametrize("how", ["round changed", "pass succeeded first"])
def test_the_wait_starts_over_when_the_round_changes_or_a_pass_leaves_nothing_missing(clip_site, broken_music, how):
    _, root = clip_site
    clips = Timed(root)
    stop = start_warm(root, clips)
    assert wait_until(lambda: len(clips.times) >= 6 and clips.times[-1] - clips.times[-2] > 0.3)     # backed off to the longest
    if how == "pass succeeded first":
        broken_music["broken"] = False
        assert wait_until(lambda: len(clip_files(root)) == 4)
        passes = len(clips.times)
        time.sleep(1.0)
        assert len(clips.times) == passes                                                           # nothing missing: no more passes
        broken_music["broken"] = True
        for f in (root / ".clips").glob("*.mp3"):
            f.unlink()
    trigger, changed = time.monotonic(), len(clips.times)
    (root / "rounds" / "round-01.json").write_text(json.dumps(CLIP_ROUND))
    assert wait_until(lambda: len(clips.times) >= changed + 3)
    stop.set()
    after = [trigger, *clips.times[changed:changed + 3]]
    gaps = [b - a for a, b in zip(after, after[1:])]
    assert gaps[0] < 0.45 and gaps[1] <= 0.3 and gaps[2] <= 0.3, gaps         # prompt, then 0.05 and 0.1 again instead of 0.4


# --- the Library scope: the site shows only the tracks inside it -----------------------------------------------

IN_A, IN_B, OUT_C = "a1a1a1a1a1a1a1a1", "b2b2b2b2b2b2b2b2", "c3c3c3c3c3c3c3c3"
PLACE = {IN_A: "/music/in/a.mp3", IN_B: "/music/in/sub/b.mp3", OUT_C: "/music/out/c.mp3"}


@pytest.fixture
def scoped_site(tmp_path):
    index = {}
    for tid, path in PLACE.items():
        doc = {"track": {"path": path, "title": tid, "duration": 40.0, "sha256": tid * 4}, "analyzer": {"version": "x"},
               "grid": {"bpm": 120.0, "period": 0.5, "first_downbeat": 0.0, "bar1_confidence": 90}, "beats": [], "bars": {"start": [0.0, 2.0], "levels": {n: [0.5, 0.5] for n in ("loud", "kick", "hihat", "bass", "instruments", "vocal")}},
               "peaks": {"rate": 20, "values": []}, "sections": [{"label": "drop", "start_bar": 1, "end_bar": 2, "confidence": 99}], "events": []}
        (tmp_path / f"{tid}.json").write_text(json.dumps(doc))
        index[tid] = {"title": tid, "path": path, "bpm": 120.0, "filename_bpm": 120.0, "drops": 1, "sections": ["drop"]}
    (tmp_path / "index.json").write_text(json.dumps(index))
    (tmp_path / "ratings.json").write_text(json.dumps({tid: {"overall": 4, "analyzer_version": "x"} for tid in (IN_A, OUT_C)}))
    (tmp_path / "rounds").mkdir()
    (tmp_path / "rounds" / "round-01.json").write_text(json.dumps({"round": 1, "created": "2026-11-05T10:00:00", "theories": [{"id": "t1", "text": "t"}], "probes": [
        {"id": "r01-p001", "track": OUT_C, "title": "c", "theory": "t1", "bucket": "unsure", "question": "yesno", "prompt": "Does it start here?",
         "claim": {"kind": "drop", "bar": 1, "time": 2.0, "confidence": 40}, "plays": [{"id": "main", "start": 0.0, "end": 4.0, "mark": 2.0}]}]}))
    httpd = ReviewServer(("127.0.0.1", 0), make_handler(tmp_path, tmp_path / "ratings.json"))
    threading.Thread(target=httpd.serve_forever, daemon=True).start()
    yield f"http://127.0.0.1:{httpd.server_address[1]}", tmp_path
    httpd.shutdown()


def api(base, path):
    with urllib.request.urlopen(base + path) as r:
        return json.load(r)


SCOPES = [(None, {IN_A, IN_B, OUT_C}), (["/music/in"], {IN_A, IN_B}), (["/music/out/*"], {OUT_C}), (["/music/in/a.mp3"], {IN_A})]


@pytest.mark.parametrize("include,listed", SCOPES)
def test_the_song_list_has_only_the_tracks_of_the_library(scoped_site, include, listed):
    base, root = scoped_site
    if include:
        library.set_scope(root, include)
    assert {t["id"] for t in api(base, "/api/tracks")} == listed


@pytest.mark.parametrize("include,listed", SCOPES)
def test_the_summary_counts_only_the_tracks_of_the_library(scoped_site, include, listed):
    base, root = scoped_site
    if include:
        library.set_scope(root, include)
    summary = api(base, "/api/summary")
    assert summary["tracks_analyzed"] == len(listed)
    assert summary["tracks_rated"] == len(listed & {IN_A, OUT_C})
    assert sum(summary["tempo"].values()) == len(listed)


@pytest.mark.parametrize("include,listed", SCOPES)
def test_the_report_measures_coverage_over_the_tracks_of_the_library_but_keeps_every_answer(scoped_site, monkeypatch, include, listed):
    base, root = scoped_site
    if include:
        library.set_scope(root, include)
    seen = {}
    monkeypatch.setattr(review.rounds, "report", lambda rounds_, answers, docs: seen.update(rounds=rounds_, docs=docs) or {})
    api(base, "/api/report")
    assert set(seen["docs"]) == listed and [r["round"] for r in seen["rounds"]] == [1]


@pytest.mark.parametrize("path", ["/api/track/{}", "/api/ratings/{}", "/api/reference/{}", "/api/gold/{}"])
def test_a_track_outside_the_library_that_is_in_the_round_still_opens(scoped_site, path):
    base, root = scoped_site
    library.set_scope(root, ["/music/in"])
    assert OUT_C not in {t["id"] for t in api(base, "/api/tracks")}
    api(base, path.format(OUT_C))
    rnd = api(base, "/api/round")
    assert [p["track"] for p in rnd["probes"]] == [OUT_C] and "peaks" in rnd["probes"][0]["plays"][0]


def test_a_change_of_scope_shows_at_once_without_restarting_the_site(scoped_site):
    base, root = scoped_site
    assert len(api(base, "/api/tracks")) == 3
    library.set_scope(root, ["/music/in"])
    assert len(api(base, "/api/tracks")) == 2
    library.clear_scope(root)
    assert len(api(base, "/api/tracks")) == 3


# --- the song view reads the Analysis QLC+ gets: the fusion, with the confidence model's numbers -------------------

FUSED_TID = "efefefefefefefef"
LEVEL = [0.2] * 12 + [0.9] * 12 + [0.2] * 8
FUSED_RAW = {"track": {"path": "", "title": "song", "duration": 64.0, "sha256": "ef" * 32}, "analyzer": {"version": "3.5"},
             "grid": {"bpm": 120.0, "period": 0.5, "confidence": 0.9, "first_downbeat": 0.0}, "beats": [0.5 * i for i in range(128)],
             "bars": {"start": [2.0 * k for k in range(32)], "levels": {n: LEVEL for n in ("loud", "kick", "bass", "hihat", "instruments", "vocal")}},
             "sections": [{"label": "intro", "start_bar": 0, "end_bar": 10}, {"label": "buildup", "start_bar": 10, "end_bar": 12},
                          {"label": "drop", "start_bar": 12, "end_bar": 24}, {"label": "outro", "start_bar": 24, "end_bar": 32}],
             "events": [{"kind": "buildup", "bar": 10}, {"kind": "drop", "bar": 12}]}


@pytest.fixture
def fused_site(tmp_path, tone):
    (tmp_path / f"{FUSED_TID}.json").write_text(json.dumps({**FUSED_RAW, "track": {**FUSED_RAW["track"], "path": str(tone)}}))
    (tmp_path / "index.json").write_text(json.dumps({FUSED_TID: {"title": "song", "path": str(tone)}}))
    (tmp_path / "candidates" / "dropdetector").mkdir(parents=True)
    (tmp_path / "candidates" / "dropdetector" / f"{FUSED_TID}.json").write_text(json.dumps({"drops": [{"time": 24.0, "confidence": 0.9}]}))
    httpd = ReviewServer(("127.0.0.1", 0), make_handler(tmp_path, tmp_path / "ratings.json"))
    threading.Thread(target=httpd.serve_forever, daemon=True).start()
    yield f"http://127.0.0.1:{httpd.server_address[1]}", tmp_path
    httpd.shutdown()


def served(base, what, tid=FUSED_TID):
    return json.loads(fetch(f"{base}/api/{what}/{tid}")[2])


def post(base, what, body, tid=FUSED_TID):
    req = urllib.request.Request(f"{base}/api/{what}/{tid}", data=json.dumps(body).encode(), method="POST", headers={"Content-Type": "application/json"})
    return json.load(urllib.request.urlopen(req))


@pytest.mark.parametrize("model", [False, True])
def test_the_track_is_the_fused_analysis_with_the_models_confidence_and_without_its_file_path(fused_site, model):
    base, root = fused_site
    raw = json.loads((root / f"{FUSED_TID}.json").read_text())
    if model:
        (root / confidence.MODEL_FILE).write_text(json.dumps({"groups": {}}))
    track = served(base, "track")
    expected = candidates.fused(root, raw)
    assert track["sections"] != raw["sections"] and [s["start_bar"] for s in track["sections"]] == [s["start_bar"] for s in expected["sections"]]
    assert track["rules"]["sections"] == raw["sections"] and track["analyzer"]["version"] == "3.5.1" and track["analyzer"]["fusion"] == 1
    assert all("sources" in s for s in track["sections"]) and "path" not in track["track"]
    assert all("confidence" in s for s in track["sections"]) == model


@pytest.mark.parametrize("stored,kept", [("1.5.1", True), ("3.5.1", True), ("3.5.2", False), ("3.6.1", False), ("3.5", False)])
def test_a_rating_of_the_fused_analysis_survives_only_a_features_update(fused_site, stored, kept):
    base, root = fused_site
    (root / "ratings.json").write_text(json.dumps({FUSED_TID: {"analyzer_version": stored, "overall": 5}}))
    assert (served(base, "ratings") != {}) == kept


def test_a_saved_rating_carries_the_fused_version_and_reading_gold_does_not_archive_it(fused_site):
    base, root = fused_site
    assert post(base, "ratings", {"overall": 4})["analyzer_version"] == "3.5.1"
    served(base, "gold")
    assert served(base, "ratings")["overall"] == 4 and not (root / "ratings_history.json").exists()


@pytest.mark.parametrize("what", ["track", "ratings", "gold"])
def test_a_candidate_file_being_rewritten_is_a_retry_later_and_archives_no_rating(fused_site, what):
    base, root = fused_site
    post(base, "ratings", {"overall": 4})
    (root / "candidates" / "dropdetector" / f"{FUSED_TID}.json").write_text("{not json")
    with pytest.raises(urllib.error.HTTPError) as e:
        fetch(f"{base}/api/{what}/{FUSED_TID}")
    assert e.value.code == 503
    assert json.loads((root / "ratings.json").read_text())[FUSED_TID]["analyzer_version"] == "3.5.1" and not (root / "ratings_history.json").exists()


# --- the report and the song list describe the fused Analysis, recomputed only for changed inputs ----------------

def report_docs(base, monkeypatch):
    """What rounds.report is given on each /api/report request."""
    seen = []
    monkeypatch.setattr(review.rounds, "report", lambda r, a, d: seen.append(dict(d)) or {})
    return seen


def change_analysis(root, tone):
    path = root / f"{TID}.json"
    path.write_text(path.read_text() + " ")


def change_candidate(root, tone):
    (root / "candidates" / "dropdetector").mkdir(parents=True, exist_ok=True)
    (root / "candidates" / "dropdetector" / f"{TID}.json").write_text(json.dumps({"drops": [{"time": 16.0, "confidence": 0.9}]}))


def change_model(root, tone):
    (root / confidence.MODEL_FILE).write_text(json.dumps({"groups": {}}))


def change_scope(root, tone):
    library.set_scope(root, [str(tone.parent)])


@pytest.mark.parametrize("change,recomputed", [(None, False), (change_analysis, True), (change_candidate, True), (change_model, True), (change_scope, True)])
def test_the_report_recomputes_a_document_only_when_an_input_of_the_analysis_changed(clip_site, tone, monkeypatch, change, recomputed):
    base, root = clip_site
    docs = report_docs(base, monkeypatch)
    fetch(base + "/api/report")
    if change:
        change(root, tone)
    fetch(base + "/api/report")
    assert (docs[1][TID] is not docs[0][TID]) == recomputed


def test_the_report_follows_a_candidate_that_changes_the_analysis(clip_site, tone, monkeypatch):
    base, root = clip_site
    docs = report_docs(base, monkeypatch)
    fetch(base + "/api/report")
    change_candidate(root, tone)
    fetch(base + "/api/report")
    expected = candidates.fused(root, json.loads((root / f"{TID}.json").read_text()))
    assert [(s["label"], s["start_bar"]) for s in docs[1][TID]["sections"]] == [(s["label"], s["start_bar"]) for s in expected["sections"]]
    assert docs[1][TID]["sections"] != docs[0][TID]["sections"]


def test_a_track_whose_analysis_cannot_be_fused_is_left_out_of_the_report(clip_site, monkeypatch):
    base, root = clip_site
    docs = report_docs(base, monkeypatch)
    (root / "candidates" / "dropdetector").mkdir(parents=True)
    (root / "candidates" / "dropdetector" / f"{TID}.json").write_text("{not json")
    fetch(base + "/api/report")
    assert docs == [{}]


@pytest.mark.parametrize("warm", [False, True])
def test_the_song_list_counts_come_from_the_fused_analysis_once_it_is_loaded(clip_site, monkeypatch, warm):
    base, root = clip_site
    change_candidate(root, None)
    if warm:
        report_docs(base, monkeypatch)
        fetch(base + "/api/report")
    row = json.loads(fetch(base + "/api/tracks")[2])[0]
    fused = candidates.fused(root, json.loads((root / f"{TID}.json").read_text()))
    expected = (sum(e["kind"] == "drop" for e in fused["events"]), [s["label"] for s in fused["sections"]]) if warm else (9, ["drop"] * 9)
    assert (row["drops"], row["sections"]) == expected
    assert warm == (expected != (9, ["drop"] * 9))
