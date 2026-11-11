"""Local rating site: serves analyses, the MP3s they came from, and stores ratings and Probe answers."""

import hashlib
import hmac
import ipaddress
import json
import math
import os
import re
import secrets
import socket
import subprocess
import sys
import tempfile
import threading
import time
from datetime import datetime, timezone
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

import numpy as np

from . import audio, candidates, rounds
from .ratings import gold_from_rating, merge_gold, prefill_rating, summarize, tempo_agreement

STATIC = Path(__file__).parent / "review_static"
TRACK_ID = re.compile(r"^[0-9a-f]{16}$")
PROBE_ID = re.compile(r"r\d{2}-p\d{3}")
PLAY_ID = re.compile(r"[a-z0-9]{1,8}")
MAX_BODY = 256 * 1024
COOKIE = "edm_review"
COOKIE_DAYS = 30
SECURITY_HEADERS = {
    "X-Content-Type-Options": "nosniff",
    "Referrer-Policy": "no-referrer",
    "X-Frame-Options": "DENY",
    "Content-Security-Policy": "default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; "
                               "img-src 'self' data:; media-src 'self'; frame-ancestors 'none'",
}


def parse_range(header, size):
    """(first, last) byte positions, None without a header, "invalid" when unsatisfiable."""
    if header is None:
        return None
    m = re.fullmatch(r"bytes=(\d*)-(\d*)", header.strip())
    if not m or (not m.group(1) and not m.group(2)):
        return "invalid"
    if not m.group(1):
        n = int(m.group(2))
        return (max(0, size - n), size - 1)
    first = int(m.group(1))
    last = min(int(m.group(2)) if m.group(2) else size - 1, size - 1)
    return (first, last) if first <= last else "invalid"


def _blip(out, sr, t, freq, gain):
    """A 30 ms sine blip starting at t seconds, added into `out`."""
    length = int(0.03 * sr)
    a = int(round(t * sr))
    if a < 0 or a >= len(out):
        return
    env = np.exp(-np.arange(length) / (0.006 * sr)).astype(np.float32)
    blip = gain * env * np.sin(2 * np.pi * freq * np.arange(length) / sr).astype(np.float32)
    out[a:a + length] += blip[: len(out) - a]


def click_track(n, sr, beats, downbeats):
    """Short sine blips on the beats, louder and higher on bar starts."""
    out = np.zeros(n, np.float32)
    for t, down in zip(beats, downbeats):
        _blip(out, sr, t, *((1760.0, 0.9) if down else (1100.0, 0.4)))
    return out


def corrected_grid(doc, shift_beats, offset_ms):
    """Beat times and bar-start flags after the reviewer's grid shift."""
    beats = np.asarray(doc["beats"]) + offset_ms / 1000.0
    first = int(np.argmin(np.abs(np.asarray(doc["beats"]) - doc["bars"]["start"][0])))
    downs = ((np.arange(len(beats)) - first - shift_beats) % 4) == 0
    return beats, downs


def render_click_mix(mp3, doc, shift_beats, offset_ms, out_path):
    sr = 44100
    x = audio.decode(mp3, sr, mono=False)
    beats, downs = corrected_grid(doc, shift_beats, offset_ms)
    clicks = click_track(x.shape[1], sr, beats, downs)
    mixed = np.clip(0.7 * x + clicks, -1, 1).astype(np.float32)
    subprocess.run(["ffmpeg", "-v", "error", "-y", "-f", "f32le", "-ar", str(sr), "-ac", "2", "-i", "-",
                    "-b:a", "192k", str(out_path)], input=mixed.T.tobytes(), check=True)


CLIP_SR = 44100
CUE_HZ = 3520.0


def read_doc(out_dir, track_id):
    p = Path(out_dir) / f"{track_id}.json"
    return json.loads(p.read_text()) if TRACK_ID.match(track_id) and p.exists() else None


def render_clip(mp3, doc, play, cue, out_path):
    """The audio of one play: only its stretch of the track, with the beat clicks when the play has click_shift,
    else a high blip on the mark when `cue`. Written to a temp file first, so a reader never sees half a clip."""
    start, end = play["start"], play["end"]
    raw = subprocess.run(["ffmpeg", "-v", "error", "-nostdin", "-ss", f"{start:.3f}", "-t", f"{end - start:.3f}", "-i", str(mp3),
                          "-ac", "2", "-ar", str(CLIP_SR), "-f", "f32le", "-"], check=True, capture_output=True).stdout
    x = np.frombuffer(raw, np.float32).reshape(-1, 2).T
    if not x.shape[1]:
        raise ValueError("the play lies past the end of the track")
    extra = np.zeros(x.shape[1], np.float32)
    if play.get("click_shift") is not None:
        beats, downs = corrected_grid(doc, play["click_shift"], 0)
        extra = click_track(len(extra), CLIP_SR, beats - start, downs)
    elif cue:
        _blip(extra, CLIP_SR, play["mark"] - start, CUE_HZ, 0.9)
    mixed = np.clip(0.7 * x + extra, -1, 1).astype(np.float32)
    tmp = out_path.with_name(f"{out_path.name}.{threading.get_ident()}.tmp")
    try:
        subprocess.run(["ffmpeg", "-v", "error", "-y", "-f", "f32le", "-ar", str(CLIP_SR), "-ac", "2", "-i", "-", "-b:a", "160k", "-f", "mp3", str(tmp)],
                       input=mixed.T.tobytes(), check=True)
        os.replace(tmp, out_path)
    finally:
        tmp.unlink(missing_ok=True)


def play_media(doc, play):
    """What the page draws for one play: the waveform and the grid as [time, accent, bar index]. Never the
    sections or their labels. The accents follow the play's click_shift, so the lines match what is heard."""
    start, end, out = play["start"], play["end"], {}
    pk = doc.get("peaks") or {}
    if pk.get("rate"):
        i0 = int(start * pk["rate"])
        out["peaks"] = {"t0": i0 / pk["rate"], "rate": pk["rate"], "values": [round(v, 2) for v in pk["values"][i0:math.ceil(end * pk["rate"])]]}
    bars = doc.get("bars", {}).get("start")
    if doc.get("beats") and bars:
        beats, downs = corrected_grid(doc, play.get("click_shift") or 0, 0)
        starts = np.asarray(bars)
        out["grid"] = [[round(float(t), 3), int(d), int(k) if d and abs(starts[k] - t) < 0.1 else None]
                       for t, d in zip(beats, downs) if start - 1e-3 <= t <= end + 1e-3
                       for k in [int(np.argmin(np.abs(starts - t)))]]
    return out


class Clips:
    """One short MP3 per play, cached in <out>/.clips/. A clip is named by probe, play and a hash of what it was
    made from, so a changed play or analysis gets a new clip and the name reveals nothing about the question."""

    def __init__(self, out_dir):
        self.out_dir, self.locks, self.guard = Path(out_dir), {}, threading.Lock()

    @staticmethod
    def cue_applies(play):
        return play.get("mark") is not None and play.get("click_shift") is None

    def path(self, doc, probe, play, cue):
        made_from = [play["start"], play["end"], play.get("mark") if cue else None, play.get("click_shift"), doc["analyzer"]["version"]]
        key = hashlib.sha1(json.dumps(made_from).encode()).hexdigest()[:12]
        return self.out_dir / ".clips" / f"{probe['id']}_{play['id']}{'_cue' if cue else ''}_{key}.mp3"

    def get(self, doc, probe, play, cue):
        """(path, milliseconds this call spent making it); a clip already on disk costs nothing."""
        cue = bool(cue) and self.cue_applies(play)
        path = self.path(doc, probe, play, cue)
        if path.exists():
            return path, 0
        began = time.monotonic()
        with self.guard:
            lock = self.locks.setdefault(path, threading.Lock())
        with lock:   # a request that comes while the background thread makes this clip waits for it
            if not path.exists():
                path.parent.mkdir(exist_ok=True)
                render_clip(doc["track"]["path"], doc, play, cue, path)
        return path, round((time.monotonic() - began) * 1000)

    def prune(self, rnd):
        """Removes the clips of probes that are not in this round; a clip still being written (.tmp) stays."""
        keep = tuple(f"{p['id']}_" for p in rnd["probes"])
        for f in (self.out_dir / ".clips").glob("*.mp3"):
            if not f.name.startswith(keep):
                f.unlink(missing_ok=True)

    def prerender(self, rnd, answers=None):
        """Every clip of a round, the open questions first."""
        for probe in sorted(rnd["probes"], key=lambda p: p["id"] in (answers or {})):
            doc = read_doc(self.out_dir, probe["track"])
            for play in probe["plays"] if doc else []:
                for cue in [False, True] if self.cue_applies(play) else [False]:
                    try:
                        self.get(doc, probe, play, cue)
                    except (subprocess.CalledProcessError, ValueError, OSError, KeyError) as e:
                        print(f"clip {probe['id']}/{play['id']}: {e}", file=sys.stderr, flush=True)


def _slim(doc):
    """An analysis without the large arrays rounds.report never reads."""
    return {**{k: v for k, v in doc.items() if k not in ("peaks", "raw", "beats")},
            "bars": {k: v for k, v in doc.get("bars", {}).items() if k != "descriptors"}}


class LibraryReport:
    """rounds.report over every analysis. Documents are cached by file stamp, so only a changed file is read
    again. Only report requests wait for the loading; every other request goes on."""

    def __init__(self, out_dir):
        self.out_dir, self.docs, self.lock = Path(out_dir), {}, threading.Lock()

    def refresh(self):
        with self.lock:
            files = {p.stem: p for p in self.out_dir.glob("*.json") if TRACK_ID.match(p.stem)}
            for tid, p in files.items():
                try:
                    st = p.stat()
                    stamp = (st.st_mtime_ns, st.st_size)
                    if tid not in self.docs or self.docs[tid][0] != stamp:
                        self.docs[tid] = (stamp, _slim(json.loads(p.read_text())))
                except (OSError, ValueError):
                    pass   # being rewritten right now: the cached copy, if any, serves
            for tid in set(self.docs) - set(files):
                del self.docs[tid]
            return {tid: doc for tid, (_, doc) in self.docs.items()}

    def get(self):
        docs = self.refresh()
        return rounds.report(rounds.all_rounds(self.out_dir), rounds.load_answers(self.out_dir), docs)


def warm(out_dir, clips, library, stop, every=5.0):
    """Background thread: renders the clips of the current round at start and whenever a round file appears or
    changes, so a tap never waits for a render; then loads the analyses the report needs."""
    seen = None
    while not stop.is_set():
        try:
            files = sorted((p.name, p.stat().st_mtime_ns) for p in (Path(out_dir) / "rounds").glob("round-*.json"))
            if files != seen:
                seen = files
                rnd = rounds.current_round(out_dir)
                if rnd:
                    clips.prune(rnd)
                    clips.prerender(rnd, rounds.load_answers(out_dir))
                library.refresh()
        except Exception as e:   # the thread must outlive one bad file
            print(f"warm-up: {e}", file=sys.stderr, flush=True)
        stop.wait(every)


def write_json_atomic(path, data):
    """Write to a unique temp file in the same folder, flush to disk, then rename over the target."""
    fd, tmp = tempfile.mkstemp(dir=path.parent, prefix=path.name + ".", suffix=".tmp")
    try:
        with os.fdopen(fd, "w") as f:
            json.dump(data, f, indent=1)
            f.flush()
            os.fsync(f.fileno())
        os.replace(tmp, path)
    except BaseException:
        Path(tmp).unlink(missing_ok=True)
        raise


VERDICTS = {None, "ok", "off", "wrong"}


def _is_int(v, lo=-10**6, hi=10**6):
    return isinstance(v, int) and not isinstance(v, bool) and lo <= v <= hi


def _is_num(v, lo, hi):
    return isinstance(v, (int, float)) and not isinstance(v, bool) and lo <= v <= hi


def _is_name(v):
    return isinstance(v, str) and re.fullmatch(r"[a-z_]{1,32}", v) is not None


def _check_items(items, rules, name, limit):
    if not isinstance(items, list) or len(items) > limit:
        return f"{name} must be a list of at most {limit} items"
    for item in items:
        if not isinstance(item, dict) or set(item) - set(rules):
            return f"{name}: unknown fields"
        for key, ok in rules.items():
            if key in item and not ok(item[key]):
                return f"{name}.{key} is invalid"
    return None


GRID_RULES = {"beats": lambda v: v in VERDICTS, "downbeat": lambda v: v in VERDICTS,
              "shift_beats": lambda v: _is_int(v, -64, 64), "offset_ms": lambda v: _is_num(v, -2000, 2000)}
EVENT_RULES = {"kind": _is_name, "bar": lambda v: _is_int(v, 0), "verdict": lambda v: v in VERDICTS,
               "offset_bars": _is_int}
SECTION_RULES = {"label": _is_name, "start_bar": lambda v: _is_int(v, 0), "end_bar": lambda v: _is_int(v, 0),
                 "verdict": lambda v: v in VERDICTS, "correct_label": lambda v: v is None or _is_name(v),
                 "true_start_bar": lambda v: v is None or _is_int(v, 0), "added": lambda v: isinstance(v, bool)}
MARK_RULES = {"kind": _is_name, "bar": lambda v: _is_int(v, 0), "time": lambda v: _is_num(v, 0, 24 * 3600)}


def validate_rating(rating):
    """None when the posted rating has the shape the review page writes, else the reason."""
    if not isinstance(rating, dict):
        return "rating must be an object"
    allowed = {"grid", "events", "sections", "missed", "overall", "notes", "prefilled", "analyzer_version", "updated"}
    if set(rating) - allowed:
        return "unknown fields: " + ", ".join(sorted(set(rating) - allowed))
    grid = rating.get("grid", {})
    if not isinstance(grid, dict) or set(grid) - set(GRID_RULES) or not all(GRID_RULES[k](v) for k, v in grid.items()):
        return "grid is invalid"
    for name, rules, limit in (("events", EVENT_RULES, 1000), ("sections", SECTION_RULES, 1000), ("missed", MARK_RULES, 2000)):
        error = _check_items(rating.get(name, []), rules, name, limit)
        if error:
            return error
    if rating.get("overall") is not None and not _is_int(rating["overall"], 1, 5):
        return "overall must be 1 to 5"
    if not isinstance(rating.get("notes", ""), str) or len(rating.get("notes", "")) > 10000:
        return "notes must be text up to 10000 characters"
    if not isinstance(rating.get("prefilled", False), bool):
        return "prefilled must be true or false"
    return None


def host_allowed(host_header):
    """IP literals and local names only, so a public domain rebound to this machine is refused."""
    host = (host_header or "").strip().lower()
    host = host[1:].split("]")[0] if host.startswith("[") else host.rsplit(":", 1)[0] if host.count(":") == 1 else host
    if not host:
        return False
    try:
        ipaddress.ip_address(host)
        return True
    except ValueError:
        return "." not in host or host.endswith((".local", ".localhost"))


class Auth:
    """A shared PIN for the home network. The phone types it once and keeps a session cookie."""

    MAX_FAILS, LOCK_SECONDS = 5, 60

    def __init__(self, pin, token):
        self.pin, self.token = pin, token
        self.fails, self.lock = {}, threading.Lock()

    @classmethod
    def load(cls, path, pin=None):
        """PIN and session token persist in `path` (mode 600), so a restart keeps the phone logged in."""
        path = Path(path)
        saved = json.loads(path.read_text()) if path.exists() else {}
        if pin and pin != saved.get("pin"):
            saved = {"pin": pin}
        saved.setdefault("pin", f"{secrets.randbelow(10**6):06d}")
        saved.setdefault("token", secrets.token_urlsafe(32))
        path.parent.mkdir(parents=True, exist_ok=True)
        write_json_atomic(path, saved)
        path.chmod(0o600)
        return cls(saved["pin"], saved["token"])

    def cookie_ok(self, header):
        jar = dict(p.strip().split("=", 1) for p in (header or "").split(";") if "=" in p)
        return hmac.compare_digest(jar.get(COOKIE, ""), self.token)

    def login(self, client, pin):
        """"ok", "wrong" or "locked"; five wrong PINs lock that client out for a minute."""
        with self.lock:
            count, since = self.fails.get(client, (0, 0.0))
            if count >= self.MAX_FAILS and time.monotonic() - since < self.LOCK_SECONDS:
                return "locked"
            if isinstance(pin, str) and hmac.compare_digest(pin.encode(), self.pin.encode()):
                self.fails.pop(client, None)
                return "ok"
            count = 0 if count >= self.MAX_FAILS else count
            self.fails[client] = (count + 1, time.monotonic())
            return "wrong"


def public_track(doc):
    """The track document without its location on disk."""
    return {**doc, "track": {k: v for k, v in doc["track"].items() if k != "path"}}


def _same_structure(a, b):
    """Versions are "<features>.<structure>"; new features alone (same beats) keep a rating valid."""
    return str(a).split(".")[-1] == str(b).split(".")[-1]


class RatingStore:
    """ratings.json holds the rating of the current analysis per track; a rating made
    against an older analysis moves to ratings_history.json, so no judgement is lost."""

    def __init__(self, path):
        self.path = Path(path)
        self.history_path = self.path.with_name("ratings_history.json")
        self.lock = threading.Lock()

    @staticmethod
    def _read(path):
        return json.loads(path.read_text()) if path.exists() else {}

    @staticmethod
    def _write(path, data):
        write_json_atomic(path, data)

    def load(self):
        return self._read(self.path)

    def history(self, track_id):
        return self._read(self.history_path).get(track_id, [])

    def current(self, track_id, version):
        """The rating for this analysis version; an older one is archived first."""
        with self.lock:
            data = self.load()
            rating = data.get(track_id)
            if rating is None or _same_structure(rating.get("analyzer_version"), version):
                return rating or {}
            history = self._read(self.history_path)
            history.setdefault(track_id, []).append(
                {"analyzer_version": rating.get("analyzer_version"), "gold": gold_from_rating(rating), "rating": rating})
            self._write(self.history_path, history)
            del data[track_id]
            self._write(self.path, data)
            return {}

    def gold(self, track_id, version):
        golds = [h["gold"] for h in self.history(track_id)]
        current = self.current(track_id, version)
        if current:
            golds.append(gold_from_rating(current))
        return {**merge_gold(golds), "versions": len(golds)}

    def save(self, track_id, rating):
        with self.lock:
            data = self.load()
            data[track_id] = rating
            self._write(self.path, data)


def make_handler(out_dir, ratings_path, reference=lambda: {}, auth=None, clips=None, library=None):
    """`reference` returns VirtualDJ data by file path (loaded lazily, it is a large file).
    With `auth`, everything except the page itself and the login needs the session cookie."""
    out_dir = Path(out_dir)
    store = RatingStore(ratings_path)
    clips, library = clips or Clips(out_dir), library or LibraryReport(out_dir)
    media = {}   # (track, start, end, click_shift) -> (analysis stamp, what the page draws)

    def load_doc(track_id):
        return read_doc(out_dir, track_id)

    def round_for_page():
        rnd = rounds.current_round(out_dir)
        if not rnd:
            return {"round": None}
        page = rounds.public_round(rnd, rounds.load_answers(out_dir))
        for probe, raw in zip(page["probes"], rnd["probes"]):
            path = out_dir / f"{raw['track']}.json"
            stamp = path.stat().st_mtime_ns if path.exists() else None
            for play, raw_play in zip(probe["plays"], raw["plays"]):
                key = (raw["track"], raw_play["start"], raw_play["end"], raw_play.get("click_shift"))
                if stamp and media.get(key, (None,))[0] != stamp:
                    media[key] = (stamp, play_media(load_doc(raw["track"]), raw_play))
                play.update(media[key][1] if stamp else {})
        return page

    def reference_for(track_id):
        doc = load_doc(track_id)
        if not doc:
            return None
        ref = dict(reference().get(doc["track"]["path"], {}))
        found = {}
        for name in candidates.NAMES:
            try:
                c = candidates.load(out_dir, name, track_id)
            except (OSError, ValueError, KeyError):
                continue
            if c and (c["sections"] or c["drops"]):
                found[name] = {"sections": c["sections"], "drops": c["drops"]}
        return {**ref, "candidates": found} if found else ref

    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *args):
            pass

        def end_headers(self):
            for k, v in SECURITY_HEADERS.items():
                self.send_header(k, v)
            super().end_headers()

        def _json(self, obj, status=200, headers=()):
            body = json.dumps(obj).encode()
            self.send_response(status)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Cache-Control", "no-store")
            for k, v in headers:
                self.send_header(k, v)
            self.end_headers()
            self.wfile.write(body)

        def _refused(self, public=False):
            """Sends the refusal and returns True when the request may not go on."""
            if not host_allowed(self.headers.get("Host")):
                self._json({"error": "unknown host"}, 403)
                return True
            if auth and not public and not auth.cookie_ok(self.headers.get("Cookie")):
                self._json({"error": "PIN required"}, 401)
                return True
            return False

        def _body(self):
            """(True, posted JSON), or (False, None) after sending the error."""
            origin = self.headers.get("Origin")
            if origin and origin.split("://", 1)[-1] != self.headers.get("Host"):
                return False, self._json({"error": "cross-site request"}, 403)
            if self.headers.get("Content-Type", "").split(";")[0].strip() != "application/json":
                return False, self._json({"error": "send application/json"}, 415)
            try:
                length = int(self.headers.get("Content-Length", 0))
            except ValueError:
                length = -1
            if not 0 < length <= MAX_BODY:
                return False, self._json({"error": f"body must be 1 to {MAX_BODY} bytes"}, 413 if length > MAX_BODY else 400)
            try:
                return True, json.loads(self.rfile.read(length))
            except ValueError:
                return False, self._json({"error": "invalid JSON"}, 400)

        def _file(self, path, ctype):
            data = path.read_bytes()
            self.send_response(200)
            self.send_header("Content-Type", ctype)
            self.send_header("Content-Length", str(len(data)))
            self.end_headers()
            self.wfile.write(data)

        def _audio(self, track_id, query):
            doc = load_doc(track_id)
            if not doc or not Path(doc["track"]["path"]).exists():
                return self.send_error(404)
            path = Path(doc["track"]["path"])
            if query.get("click") == "1":
                try:
                    shift, offset = int(query.get("shift", 0)), int(query.get("offset", 0))
                except ValueError:
                    return self.send_error(400)
                if not (GRID_RULES["shift_beats"](shift) and GRID_RULES["offset_ms"](offset)):
                    return self.send_error(400)
                cached = out_dir / ".clicks" / f"{track_id}_{shift}_{offset}.mp3"
                if not cached.exists():
                    cached.parent.mkdir(exist_ok=True)
                    render_click_mix(path, doc, shift, offset, cached)
                path = cached
            self._ranged(path)

        def _ranged(self, path, headers=()):
            size = path.stat().st_size
            rng = parse_range(self.headers.get("Range"), size)
            if rng == "invalid":
                self.send_response(416)
                self.send_header("Content-Range", f"bytes */{size}")
                return self.end_headers()
            first, last = rng or (0, size - 1)
            self.send_response(206 if rng else 200)
            self.send_header("Content-Type", "audio/mpeg")
            self.send_header("Accept-Ranges", "bytes")
            self.send_header("Content-Length", str(last - first + 1))
            if rng:
                self.send_header("Content-Range", f"bytes {first}-{last}/{size}")
            for k, v in headers:
                self.send_header(k, v)
            self.end_headers()
            with open(path, "rb") as f:
                f.seek(first)
                try:
                    self.wfile.write(f.read(last - first + 1))
                except (BrokenPipeError, ConnectionResetError):
                    pass   # the browser dropped this request, e.g. to start another clip

        def _clip(self, probe_id, play_id, query):
            if not (PROBE_ID.fullmatch(probe_id) and PLAY_ID.fullmatch(play_id)):
                return self.send_error(404)
            probe = next((p for r in rounds.all_rounds(out_dir) for p in r["probes"] if p["id"] == probe_id), None)
            play = next((pl for pl in probe["plays"] if pl["id"] == play_id), None) if probe else None
            doc = load_doc(probe["track"]) if play else None
            if not doc or not Path(doc["track"]["path"]).exists():
                return self.send_error(404)
            if query.get("cue", "0") not in ("0", "1"):
                return self.send_error(400)
            try:
                path, ms = clips.get(doc, probe, play, query.get("cue") == "1")
            except (subprocess.CalledProcessError, ValueError, OSError):
                return self.send_error(500)
            self._ranged(path, [("Server-Timing", f"render;dur={ms}"), ("X-Clip-Start", str(play["start"])), ("X-Clip-End", str(play["end"]))])

        def _tracks(self):
            index_path = out_dir / "index.json"
            index = json.loads(index_path.read_text()) if index_path.exists() else {}
            ratings = store.load()
            rows = []
            for tid, t in index.items():
                r = ratings.get(tid, {})
                rows.append({"id": tid, **{k: v for k, v in t.items() if k != "path"}, "rated": bool(r), "overall": r.get("overall"),
                             "tempo": tempo_agreement(t["bpm"], t.get("filename_bpm")) if "bpm" in t else None})
            return sorted(rows, key=lambda r: r["title"].lower())

        def do_GET(self):
            parts = self.path.split("?")[0].strip("/").split("/")
            query = dict(q.split("=", 1) for q in self.path.partition("?")[2].split("&") if "=" in q)
            if self._refused(public=parts == [""]):
                return
            if parts == [""]:
                return self._file(STATIC / "index.html", "text/html; charset=utf-8")
            if parts[0] == "audio" and len(parts) == 2:
                return self._audio(parts[1], query)
            if parts[0] == "clip" and len(parts) == 3:
                return self._clip(parts[1], parts[2], query)
            if parts[:2] == ["api", "tracks"]:
                return self._json(self._tracks())
            if parts == ["api", "round"]:
                return self._json(round_for_page())
            if parts == ["api", "report"]:
                return self._json(library.get())
            if parts[:2] == ["api", "summary"]:
                tracks = self._tracks()
                return self._json({**summarize(store.load()),
                                   "tempo": {k: sum(t["tempo"] == k for t in tracks)
                                             for k in ("match", "double", "half", "mismatch", "untagged")},
                                   "tracks_analyzed": len(tracks)})
            if parts[:2] == ["api", "track"] and len(parts) == 3:
                doc = load_doc(parts[2])
                return self._json(public_track(doc)) if doc else self.send_error(404)
            if parts[:2] in (["api", "ratings"], ["api", "gold"]) and len(parts) == 3:
                doc = load_doc(parts[2])
                if not doc:
                    return self.send_error(404)
                version = doc["analyzer"]["version"]
                if parts[1] == "gold":
                    return self._json(store.gold(parts[2], version))
                rating = store.current(parts[2], version)
                if not rating and "sections" in doc:
                    gold = store.gold(parts[2], version)
                    if gold["sections"] or gold["events"]:
                        rating = prefill_rating(doc, gold)
                return self._json(rating)
            if parts[:2] == ["api", "reference"] and len(parts) == 3:
                ref = reference_for(parts[2])
                return self._json(ref) if ref is not None else self.send_error(404)
            self.send_error(404)

        def do_POST(self):
            parts = self.path.split("?")[0].strip("/").split("/")
            login = parts == ["api", "login"]
            if self._refused(public=login):
                return
            if parts[:2] == ["api", "answers"] and len(parts) == 3:
                return self._answer(parts[2])
            if not login and (parts[:2] != ["api", "ratings"] or len(parts) != 3):
                return self.send_error(404)
            doc = None if login else load_doc(parts[2])
            if not login and not doc:
                return self.send_error(404)
            ok, body = self._body()
            if not ok:
                return
            if login:
                return self._login(body)
            error = validate_rating(body)
            if error:
                return self._json({"error": error}, 400)
            store.current(parts[2], doc["analyzer"]["version"])  # archive a stale rating before replacing it
            rating = {**body, "analyzer_version": doc["analyzer"]["version"],
                      "updated": datetime.now(timezone.utc).isoformat(timespec="seconds")}
            store.save(parts[2], rating)
            self._json(rating)

        def _answer(self, probe_id):
            if not PROBE_ID.fullmatch(probe_id):
                return self.send_error(404)
            ok, body = self._body()
            if not ok:
                return
            if not isinstance(body, dict) or set(body) != {"answer"}:
                return self._json({"error": 'send {"answer": ...}'}, 400)
            status, error = rounds.record_answer(out_dir, probe_id, body["answer"])
            self._json({"error": error} if error else {"ok": True}, status)

        def _login(self, body):
            if not auth:
                return self._json({"ok": True})
            result = auth.login(self.client_address[0], body.get("pin") if isinstance(body, dict) else None)
            if result == "locked":
                return self._json({"error": "too many wrong PINs, wait a minute"}, 429)
            if result == "wrong":
                return self._json({"error": "wrong PIN"}, 401)
            cookie = f"{COOKIE}={auth.token}; Path=/; HttpOnly; SameSite=Strict; Max-Age={COOKIE_DAYS * 86400}"
            self._json({"ok": True}, headers=[("Set-Cookie", cookie)])

    return Handler


def _vdj_reference():
    from functools import lru_cache
    from .vdj import DEFAULT_DB, parse_database

    @lru_cache(maxsize=1)
    def load():
        return parse_database(DEFAULT_DB) if DEFAULT_DB.exists() else {}
    return load


def lan_address():
    """This machine's address on the local network (no packet is sent)."""
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        try:
            s.connect(("10.255.255.255", 1))
            return s.getsockname()[0]
        except OSError:
            return "127.0.0.1"


class ReviewServer(ThreadingHTTPServer):
    """A browser opens several connections at once; the default listen backlog of 5 makes macOS reset some."""
    request_queue_size = 64


def serve(out_dir, port, ratings_path=None, lan=False, pin=None):
    """Local only by default; `lan` listens on every interface and requires the PIN."""
    ratings_path = Path(ratings_path) if ratings_path else Path(out_dir) / "ratings.json"
    auth = Auth.load(Path(out_dir) / ".review-auth.json", pin) if lan or pin else None
    host = "0.0.0.0" if lan else "127.0.0.1"
    clips, library = Clips(out_dir), LibraryReport(out_dir)
    threading.Thread(target=warm, args=(out_dir, clips, library, threading.Event()), daemon=True).start()
    httpd = ReviewServer((host, port), make_handler(out_dir, ratings_path, _vdj_reference(), auth, clips, library))
    url = f"http://{lan_address() if lan else '127.0.0.1'}:{port}/"
    print(f"Review site: {url}  (ratings: {ratings_path})", flush=True)
    if auth:
        print(f"PIN: {auth.pin}  (stored in {Path(out_dir) / '.review-auth.json'})", flush=True)
    httpd.serve_forever()
