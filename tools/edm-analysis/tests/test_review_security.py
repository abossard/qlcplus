import http.client
import json
import stat
import subprocess
import threading
from concurrent.futures import ThreadPoolExecutor

import pytest

from edm_analysis.review import Auth, ReviewServer, host_allowed, make_handler, validate_rating

TID = "abababababababab"
PIN = "424242"
DOC = {"track": {"path": "", "title": "song", "sha256": "ab" * 32, "duration": 40.0}, "analyzer": {"version": "2.5"},
       "grid": {"bpm": 120.0, "period": 0.5, "first_downbeat": 0.0},
       "bars": {"start": [2.0 * k for k in range(20)]},
       "sections": [{"label": "intro", "start_bar": 0, "end_bar": 8}, {"label": "drop", "start_bar": 8, "end_bar": 20}],
       "events": [{"kind": "drop", "bar": 8}]}
GOOD = {"grid": {"beats": "ok", "downbeat": None, "shift_beats": 0, "offset_ms": 0},
        "events": [{"kind": "drop", "bar": 8, "verdict": "ok", "offset_bars": 0}],
        "sections": [{"label": "drop", "start_bar": 8, "end_bar": 20, "verdict": "wrong", "correct_label": "groove",
                      "true_start_bar": 9},
                     {"label": "buildup", "start_bar": 6, "end_bar": 8, "verdict": "ok", "correct_label": None,
                      "true_start_bar": None, "added": True}],
        "missed": [{"kind": "beat", "bar": 3, "time": 6.12}], "overall": 4, "notes": "kick late", "prefilled": False}
CLAIM = {"kind": "drop", "bar": 8, "time": 16.0, "confidence": 41}
ROUND = {"round": 1, "created": "2026-11-05T10:00:00", "theories": [{"id": "t1", "text": "Drops are late"}], "probes": [
    {"id": "r01-p001", "track": TID, "title": "song", "theory": "t1", "bucket": "unsure", "question": "yesno",
     "prompt": "Does the drop start at the marker?", "claim": CLAIM, "plays": [{"id": "main", "start": 12.0, "end": 20.0, "mark": 16.0}]},
    {"id": "r01-p002", "track": TID, "title": "song", "theory": "t1", "bucket": "sure", "question": "ab", "prompt": "Which is the drop?",
     "claim": CLAIM, "plays": [{"id": "a", "start": 12.0, "end": 20.0, "mark": 16.0}, {"id": "b", "start": 20.0, "end": 28.0, "mark": 24.0}],
     "alternatives": {"a": {"bar": 8, "time": 16.0, "source": "ours"}, "b": {"bar": 12, "time": 24.0, "source": "vdj"}}},
]}


@pytest.fixture
def site(tmp_path):
    mp3 = tmp_path / "song.mp3"
    mp3.write_bytes(bytes(range(256)) * 4)
    (tmp_path / f"{TID}.json").write_text(json.dumps({**DOC, "track": {**DOC["track"], "path": str(mp3)}}))
    (tmp_path / "index.json").write_text(json.dumps({TID: {"title": "song", "path": str(mp3), "bpm": 120.0}}))
    auth = Auth.load(tmp_path / ".review-auth.json", PIN)
    httpd = ReviewServer(("127.0.0.1", 0), make_handler(tmp_path, tmp_path / "ratings.json", lambda: {}, auth))
    threading.Thread(target=httpd.serve_forever, daemon=True).start()
    yield httpd.server_address[1], tmp_path, auth
    httpd.shutdown()


@pytest.fixture
def round_site(site):
    _, root, _ = site
    (root / "rounds").mkdir()
    (root / "rounds" / "round-01.json").write_text(json.dumps(ROUND))
    return site


def call(port, method, path, body=None, headers=None):
    conn = http.client.HTTPConnection("127.0.0.1", port, timeout=5)
    data = body if isinstance(body, bytes) or body is None else json.dumps(body).encode()
    conn.request(method, path, body=data, headers={"Host": f"127.0.0.1:{port}", **(headers or {})})
    r = conn.getresponse()
    payload = r.read()
    conn.close()
    return r.status, dict(r.getheaders()), payload


def login(port):
    status, headers, _ = call(port, "POST", "/api/login", {"pin": PIN}, {"Content-Type": "application/json"})
    assert status == 200
    return headers["Set-Cookie"].split(";")[0]


def post_rating(port, cookie, body, **extra):
    return call(port, "POST", f"/api/ratings/{TID}", body, {"Content-Type": "application/json", "Cookie": cookie, **extra})


def post_answer(port, cookie, probe_id, body, **extra):
    return call(port, "POST", f"/api/answers/{probe_id}", body, {"Content-Type": "application/json", "Cookie": cookie, **extra})


@pytest.mark.parametrize("path", ["/api/tracks", "/api/summary", f"/api/track/{TID}", f"/api/ratings/{TID}",
                                  f"/api/gold/{TID}", f"/api/reference/{TID}", f"/audio/{TID}", "/api/round", "/api/report"])
def test_everything_but_the_page_needs_the_pin(site, path):
    port, _, _ = site
    assert call(port, "GET", path)[0] == 401
    assert call(port, "GET", path, headers={"Cookie": "edm_review=guess"})[0] == 401
    assert call(port, "GET", path, headers={"Cookie": login(port)})[0] == 200


def test_the_page_loads_without_the_pin_and_sends_security_headers(site):
    port, _, _ = site
    status, headers, body = call(port, "GET", "/")
    assert status == 200 and b"<html" in body
    assert headers["X-Content-Type-Options"] == "nosniff" and "frame-ancestors 'none'" in headers["Content-Security-Policy"]


def test_login_sets_a_strict_http_only_cookie_and_refuses_a_wrong_pin(site):
    port, _, auth = site
    assert call(port, "POST", "/api/login", {"pin": "000000"}, {"Content-Type": "application/json"})[0] == 401
    status, headers, _ = call(port, "POST", "/api/login", {"pin": PIN}, {"Content-Type": "application/json"})
    cookie = headers["Set-Cookie"]
    assert status == 200 and f"edm_review={auth.token}" in cookie
    assert "HttpOnly" in cookie and "SameSite=Strict" in cookie and PIN not in cookie


def test_five_wrong_pins_lock_the_client_out(site):
    port, _, _ = site
    for _ in range(Auth.MAX_FAILS):
        assert call(port, "POST", "/api/login", {"pin": "111111"}, {"Content-Type": "application/json"})[0] == 401
    assert call(port, "POST", "/api/login", {"pin": PIN}, {"Content-Type": "application/json"})[0] == 429


@pytest.mark.parametrize("host,allowed", [
    ("192.168.1.20:8765", True), ("127.0.0.1:8765", True), ("localhost:8765", True), ("studio-mac.local:8765", True),
    ("studio-mac", True), ("[::1]:8765", True), ("fe80::1", True),
    ("evil.example.com:8765", False), ("192.168.1.20.nip.io", False), ("", False),
])
def test_host_header_must_be_an_address_or_a_local_name(host, allowed):
    assert host_allowed(host) == allowed


def test_a_rebound_public_host_name_is_refused(site):
    port, _, _ = site
    assert call(port, "GET", "/", headers={"Host": "attacker.example:8765"})[0] == 403


@pytest.mark.parametrize("change,error", [
    ({}, None),
    ({"overall": 7}, "overall"),
    ({"overall": "5"}, "overall"),
    ({"overall": True}, "overall"),
    ({"admin": True}, "unknown fields"),
    ({"notes": "x" * 10001}, "notes"),
    ({"grid": {"shift_beats": 1000}}, "grid"),
    ({"grid": {"beats": "maybe"}}, "grid"),
    ({"sections": "drop"}, "sections"),
    ({"sections": [{"label": "drop", "start_bar": 8, "path": "/etc/passwd"}]}, "sections"),
    ({"sections": [{"label": "../drop", "start_bar": 8}]}, "sections.label"),
    ({"events": [{"kind": "drop", "bar": -1}]}, "events.bar"),
    ({"missed": [{"kind": "beat", "bar": 1, "time": "soon"}]}, "missed.time"),
    ({"missed": [{"kind": "beat", "bar": 1, "time": 1.0}] * 2001}, "missed"),
])
def test_validate_rating(change, error):
    result = validate_rating({**GOOD, **change})
    assert (result is None) if error is None else (error in result)


@pytest.mark.parametrize("body", [None, [], "rating", 5])
def test_validate_rating_needs_an_object(body):
    assert validate_rating(body) == "rating must be an object"


def test_a_valid_rating_is_stored_but_never_reaches_the_final_file(site):
    port, root, _ = site
    status, _, body = post_rating(port, login(port), GOOD)
    assert status == 200
    stored = json.loads((root / "ratings.json").read_text())[TID]
    assert stored["overall"] == 4 and stored["analyzer_version"] == "2.5"
    assert not (root / "final").exists()


@pytest.mark.parametrize("body,headers,status", [
    ({**GOOD, "overall": 9}, {}, 400),
    (b"{not json", {}, 400),
    (b"null", {}, 400),
    (GOOD, {"Content-Type": "text/plain"}, 415),
    (GOOD, {"Origin": "http://evil.example"}, 403),
    (b"{" + b" " * (256 * 1024) + b"}", {}, 413),
])
def test_bad_writes_are_refused_and_change_nothing(site, body, headers, status):
    port, root, _ = site
    assert post_rating(port, login(port), body, **headers)[0] == status
    assert not (root / "ratings.json").exists() and not (root / "final").exists()


def test_a_same_origin_write_is_accepted(site):
    port, _, _ = site
    assert post_rating(port, login(port), GOOD, Origin=f"http://127.0.0.1:{port}")[0] == 200


def test_writes_need_the_pin(site):
    port, root, _ = site
    assert post_rating(port, "edm_review=nope", GOOD)[0] == 401
    assert not (root / "ratings.json").exists()


def test_concurrent_writes_leave_valid_files_and_no_temp_files(site):
    port, root, _ = site
    cookie = login(port)
    with ThreadPoolExecutor(8) as pool:
        statuses = list(pool.map(lambda n: post_rating(port, cookie, {**GOOD, "overall": n % 5 + 1})[0], range(40)))
    assert statuses == [200] * 40
    assert json.loads((root / "ratings.json").read_text())[TID]["overall"] in range(1, 6)
    assert not list(root.rglob("*.tmp"))


@pytest.mark.parametrize("path", ["/ratings.json", "/index.json", f"/{TID}.json", "/final/" + TID + ".json",
                                  "/review_static/index.html", "/../../etc/passwd", "/audio/..%2f..%2fetc%2fpasswd",
                                  f"/audio/{TID}/../../song.mp3", "/.review-auth.json"])
def test_nothing_else_on_disk_is_served(site, path):
    port, _, _ = site
    status, _, body = call(port, "GET", path, headers={"Cookie": login(port)})
    assert status == 404 and PIN.encode() not in body


@pytest.mark.parametrize("query", ["click=1&shift=abc", "click=1&shift=500", "click=1&offset=99999"])
def test_click_track_parameters_are_checked(site, query):
    port, root, _ = site
    assert call(port, "GET", f"/audio/{TID}?{query}", headers={"Cookie": login(port)})[0] == 400
    assert not (root / ".clicks").exists()


def test_api_does_not_reveal_file_locations(site):
    port, root, _ = site
    cookie = login(port)
    for path in ("/api/tracks", f"/api/track/{TID}"):
        body = call(port, "GET", path, headers={"Cookie": cookie})[2]
        assert str(root).encode() not in body and b'"path"' not in body


def test_auth_file_is_private_and_a_new_pin_rotates_the_session(tmp_path):
    path = tmp_path / ".review-auth.json"
    first = Auth.load(path)
    assert len(first.pin) == 6 and first.pin.isdigit()
    assert stat.S_IMODE(path.stat().st_mode) == 0o600
    again = Auth.load(path)
    assert (again.pin, again.token) == (first.pin, first.token)
    changed = Auth.load(path, "999999")
    assert changed.pin == "999999" and changed.token != first.token


def test_without_a_round_the_round_is_null(site):
    port, _, _ = site
    status, _, body = call(port, "GET", "/api/round", headers={"Cookie": login(port)})
    assert status == 200 and json.loads(body) == {"round": None}


def test_the_round_shows_probes_without_confidence_source_or_paths(round_site):
    port, root, _ = round_site
    cookie = login(port)
    assert post_answer(port, cookie, "r01-p002", {"answer": "b"})[0] == 200
    status, _, body = call(port, "GET", "/api/round", headers={"Cookie": cookie})
    rnd = json.loads(body)
    assert status == 200 and rnd["round"] == 1 and rnd["progress"] == {"answered": 1, "total": 2}
    assert [(p["id"], p["track"], p["answer"]) for p in rnd["probes"]] == [("r01-p001", TID, None), ("r01-p002", TID, "b")]
    for hidden in (b'"confidence"', b'"bucket"', b'"source"', b'"claim"', b'"alternatives"', b'"path"', str(root).encode()):
        assert hidden not in body


def test_an_answer_is_stored_and_can_be_changed(round_site):
    port, root, _ = round_site
    cookie = login(port)
    assert post_answer(port, cookie, "r01-p001", {"answer": "earlier"})[0] == 200
    assert post_answer(port, cookie, "r01-p001", {"answer": "yes"}, Origin=f"http://127.0.0.1:{port}")[0] == 200
    stored = json.loads((root / "rounds" / "answers.json").read_text())
    assert stored["r01-p001"]["answer"] == "yes" and stored["r01-p001"]["claim"] == CLAIM


@pytest.mark.parametrize("probe_id,body,headers,status", [
    ("r01-p001", {"answer": "maybe"}, {}, 400),
    ("r01-p002", {"answer": "yes"}, {}, 400),
    ("r01-p001", {"answer": None}, {}, 400),
    ("r01-p001", {"answer": "yes", "confidence": 99}, {}, 400),
    ("r01-p001", {}, {}, 400),
    ("r01-p001", ["yes"], {}, 400),
    ("r01-p001", b"{not json", {}, 400),
    ("r01-p001", {"answer": "yes"}, {"Content-Type": "text/plain"}, 415),
    ("r01-p001", {"answer": "yes"}, {"Origin": "http://evil.example"}, 403),
    ("r01-p001", b"{" + b" " * (256 * 1024) + b"}", {}, 413),
    ("r01-p001", {"answer": "yes"}, {"Cookie": "edm_review=nope"}, 401),
    ("r01-p001", {"answer": "yes"}, {"Host": "attacker.example"}, 403),
    ("r09-p001", {"answer": "yes"}, {}, 404),
    ("r1-p1", {"answer": "yes"}, {}, 404),
    ("..%2fanswers", {"answer": "yes"}, {}, 404),
    ("r01-p001/x", {"answer": "yes"}, {}, 404),
])
def test_bad_answers_are_refused_and_change_nothing(round_site, probe_id, body, headers, status):
    port, root, _ = round_site
    assert post_answer(port, login(port), probe_id, body, **headers)[0] == status
    assert not (root / "rounds" / "answers.json").exists()


@pytest.mark.parametrize("path", ["/clip/r01-p001/main", "/clip/r01-p002/a?cue=1"])
def test_clips_need_the_pin_and_the_host_rule(round_site, path):
    port, _, _ = round_site
    assert call(port, "GET", path)[0] == 401
    assert call(port, "GET", path, headers={"Cookie": "edm_review=guess"})[0] == 401
    assert call(port, "GET", path, headers={"Cookie": login(port), "Host": "attacker.example"})[0] == 403


@pytest.mark.parametrize("path,status", [
    ("/clip/r1-p1/main", 404), ("/clip/r01-p001/", 404), ("/clip/r01-p001/..", 404), ("/clip/r01-p001/main/x", 404),
    ("/clip/r01-p001/MAIN", 404), ("/clip/r01-p001/nope", 404), ("/clip/r09-p001/main", 404), ("/clip/..%2fanswers/main", 404),
    ("/clip/r01-p001/main?cue=2", 400), ("/clip/r01-p001/main?cue=", 400),
])
def test_clip_ids_and_cue_are_checked_before_anything_is_rendered(round_site, path, status):
    port, root, _ = round_site
    assert call(port, "GET", path, headers={"Cookie": login(port)})[0] == status
    assert not (root / ".clips").exists()


@pytest.mark.parametrize("path", ["/api/round", "/api/report"])
def test_round_and_report_leak_no_claim_bucket_source_or_file_path(round_site, path):
    port, root, _ = round_site
    cookie = login(port)
    assert post_answer(port, cookie, "r01-p001", {"answer": "yes"})[0] == 200
    status, _, body = call(port, "GET", path, headers={"Cookie": cookie})
    assert status == 200
    for hidden in (b'"confidence"', b'"bucket"', b'"source"', b'"claim"', b'"alternatives"', b'"path"', b'"sections"', b'"label_confidence"',
                   str(root).encode(), b"song.mp3"):
        assert hidden not in body, hidden


def test_clip_files_are_named_without_confidence_or_source(round_site):
    port, root, _ = round_site
    mp3 = root / "real.mp3"
    subprocess.run(["ffmpeg", "-v", "error", "-f", "lavfi", "-i", "sine=frequency=220:duration=40", "-ac", "2", str(mp3)], check=True)
    doc = json.loads((root / f"{TID}.json").read_text())
    (root / f"{TID}.json").write_text(json.dumps({**doc, "track": {**doc["track"], "path": str(mp3)}, "beats": [0.5 * i for i in range(80)],
                                                  "bars": {"start": [2.0 * k for k in range(20)]}}))
    cookie = login(port)
    for path in ("/clip/r01-p001/main", "/clip/r01-p002/b"):
        status, headers, _ = call(port, "GET", path, headers={"Cookie": cookie})
        assert status == 200 and "41" not in "".join(headers.values()) and "vdj" not in "".join(headers.values())
    names = " ".join(p.name for p in (root / ".clips").iterdir())
    assert "vdj" not in names and "ours" not in names and "41" not in names.replace("r01-p00", "")
