"""The review page in real browsers at phone, tablet and desktop size (Playwright: Chromium, and WebKit for iOS Safari)."""

import json
import re
import shutil
import subprocess
import threading
import time

import pytest

from edm_analysis import rounds
from edm_analysis.review import Auth, Clips, ReviewServer, make_handler
from edm_analysis.rounds import record_answer

PIN = "135790"
BARS, PERIOD = 32, 0.5
DURATION = BARS * 4 * PERIOD
SECTIONS = [("intro", 0, 8), ("buildup", 8, 12), ("drop", 12, 24), ("outro", 24, 32)]   # what the page shows: the fusion starts a buildup 2 bars before the rules do
RULES = [("intro", 0, 10), ("buildup", 10, 12), ("drop", 12, 24), ("outro", 24, 32)]
VIEWPORTS = {
    "phone": {"viewport": {"width": 390, "height": 844}, "device_scale_factor": 3, "has_touch": True, "is_mobile": True},
    "tablet": {"viewport": {"width": 820, "height": 1180}, "device_scale_factor": 2, "has_touch": True, "is_mobile": True},
    "desktop": {"viewport": {"width": 1440, "height": 900}, "device_scale_factor": 1},
}
TOUCH = ["phone", "tablet"]
BROWSERS = ["chromium", "webkit"]


def track_doc(tid, title, mp3, intro=0.0):
    """intro: seconds of beatless audio before bar 0."""
    bars = [intro + 2.0 * k for k in range(BARS)]
    level = [0.2 if k < 12 or k >= 24 else 0.9 for k in range(BARS)]
    return {
        "track": {"path": str(mp3), "title": title, "sha256": tid * 4, "duration": intro + DURATION, "filename_bpm": 120.0},
        "analyzer": {"version": "2.5", "stems": "vdj"},
        "grid": {"bpm": 120.0, "period": PERIOD, "confidence": 0.9, "first_downbeat": intro},
        "beats": [intro + PERIOD * i for i in range(BARS * 4)],
        "bars": {"start": bars, "levels": {n: level for n in ("loud", "kick", "hihat", "bass", "instruments", "vocal")}},
        "peaks": {"rate": 20, "values": [round(0.3 + 0.6 * ((i // 10) % 2), 2) for i in range(int((intro + DURATION) * 20))]},
        "sections": [{"label": label, "start_bar": a, "end_bar": b} for label, a, b in RULES],
        "events": [{"kind": "buildup", "bar": 10}, {"kind": "drop", "bar": 12}, {"kind": "vocal_in", "bar": 4}, {"kind": "vocal_out", "bar": 20}],
    }


@pytest.fixture(scope="session")
def mp3(tmp_path_factory):
    path = tmp_path_factory.mktemp("audio") / "tone.mp3"
    subprocess.run(["ffmpeg", "-v", "error", "-f", "lavfi", "-i", f"sine=frequency=220:duration={DURATION}",
                    "-ac", "2", "-b:a", "64k", str(path)], check=True)
    return path


@pytest.fixture
def library(request, tmp_path, mp3):
    """Parametrize indirectly with the seconds before bar 0 to get a beatless intro."""
    index = {}
    for n, title in enumerate(["Alpha (Extended Mix) - Some Artist feat. Someone With A Long Name 2024 [120 8A]", "Beta - Other"]):
        tid = f"{n + 1:x}" * 16
        song = tmp_path / f"{tid}.mp3"
        shutil.copy(mp3, song)
        (tmp_path / f"{tid}.json").write_text(json.dumps(track_doc(tid, title, song, getattr(request, "param", 0.0))))
        index[tid] = {"title": title, "path": str(song), "bpm": 120.0, "filename_bpm": 120.0, "drops": 1,
                      "sections": [s[0] for s in SECTIONS]}
    (tmp_path / "index.json").write_text(json.dumps(index))
    auth = Auth.load(tmp_path / ".review-auth.json", PIN)
    httpd = ReviewServer(("127.0.0.1", 0), make_handler(tmp_path, tmp_path / "ratings.json", lambda: {}, auth))
    threading.Thread(target=httpd.serve_forever, daemon=True).start()
    yield f"http://127.0.0.1:{httpd.server_address[1]}", tmp_path, sorted(index)
    httpd.shutdown()


CLAIM = {"kind": "drop", "bar": 12, "time": 24.0, "confidence": 40}


def probe(n, tid, question, prompt, plays, **extra):
    return {"id": f"r01-p{n:03d}", "track": tid, "title": f"Song {n}", "theory": "t1", "bucket": "unsure", "question": question,
            "prompt": prompt, "claim": CLAIM, "plays": plays, **extra}


def make_round(ids, probes=None):
    return {"round": 1, "created": "2026-11-05T10:00:00", "theories": [{"id": "t1", "text": "Drops are found one bar late"}], "probes": probes or [
        probe(1, ids[0], "yesno", "Does the drop start at the marker?", [{"id": "main", "start": 20.0, "end": 28.0, "mark": 24.0}]),
        probe(2, ids[1], "ab", "Which one starts on the drop?", [{"id": "a", "start": 4.0, "end": 12.0, "mark": 8.0},
                                                                  {"id": "b", "start": 20.0, "end": 28.0, "mark": 24.0, "click_shift": 1}]),
        probe(3, ids[0], "label", "Is this a breakdown?", [{"id": "main", "start": 40.0, "end": 48.0, "mark": 44.0}]),
    ]}


def new_round(ids, labels_probe=True):
    """One of each kind of the next contract: tap, locate, label with and without its own labels, A/B."""
    main = lambda a, b, mark: [{"id": "main", "start": float(a), "end": float(b), "mark": mark}]
    probes = [
        probe(1, ids[0], "yesno", "Does the drop start at the marker?", main(20, 28, 24.0)),
        probe(2, ids[1], "locate", "Where does the new part start?", main(28, 36, None)),
        probe(3, ids[0], "label", "Is this part a drop?", main(40, 48, 44.0)),
        probe(4, ids[1], "yesno", "Does the buildup start at the marker?", main(8, 16, 12.0), tap=False),
        probe(5, ids[0], "ab", "Which one has the accent on bar 1?", [{"id": "a", "start": 20.0, "end": 28.0, "mark": 20.0, "click_shift": 0},
                                                                    {"id": "b", "start": 20.0, "end": 28.0, "mark": 20.0, "click_shift": 1}]),
        probe(6, ids[1], "label", "Is this part a drop?", main(44, 52, 48.0), labels=["drop", "groove", "outro"]),
    ]
    return make_round(ids, probes)


def many_round(ids, n=12):
    return make_round(ids, [probe(i, ids[i % 2], "yesno", "Does the drop start at the marker?", [{"id": "main", "start": 20.0, "end": 28.0, "mark": 24.0}])
                            for i in range(1, n + 1)])


def put_round(library, rnd):
    base, root, ids = library
    (root / "rounds").mkdir(exist_ok=True)
    (root / "rounds" / "round-01.json").write_text(json.dumps(rnd))
    return base, root, ids


@pytest.fixture
def round_library(library):
    return put_round(library, make_round(library[2]))


@pytest.fixture
def contract(monkeypatch):
    """A probe's own tap and labels fields reach the page, as a round may set them (public_round derives them from the question type)."""
    public = rounds.public_round

    def public_round(rnd, answers):
        out = public(rnd, answers)
        for shown, raw in zip(out["probes"], rnd["probes"]):
            shown.update({k: raw[k] for k in ("tap", "labels") if k in raw})
        return out
    monkeypatch.setattr(rounds, "public_round", public_round)


@pytest.fixture
def stored_shape(monkeypatch):
    """The round as the stored round 1 shows it: no tap, no labels, and the old yes/no buttons."""
    public = rounds.public_round

    def public_round(rnd, answers):
        out = public(rnd, answers)
        for shown in out["probes"]:
            shown.pop("tap", None), shown.pop("labels", None)
            if shown["question"] == "yesno":
                shown["options"] = [{"value": v, "label": v.title()} for v in ("yes", "no", "earlier", "later", "skip")]
        return out
    monkeypatch.setattr(rounds, "public_round", public_round)


@pytest.fixture
def new_library(library, contract):
    return put_round(library, new_round(library[2]))


@pytest.fixture(params=BROWSERS)
def browser_(request, playwright):
    browser = getattr(playwright, request.param).launch()
    yield browser
    browser.close()


def open_page(browser, size):
    options = dict(VIEWPORTS[size])
    if browser.browser_type.name == "firefox":
        options.pop("is_mobile", None)
    context = browser.new_context(**options)
    context.set_default_timeout(8000)
    return context.new_page()


def login(page, base):
    page.goto(base + "/")
    page.locator("#pin").fill(PIN)
    page.locator("#loginForm button").click()
    page.locator("#list .row").first.wait_for()


def open_track(page, base, size):
    login(page, base)
    page.locator("#list .row").first.click()
    page.locator("#overview").wait_for()
    page.wait_for_function("() => views.overview && views.zoom")


def tap(page, size, x, y):
    if size in TOUCH:
        page.touchscreen.tap(x, y)
    else:
        page.mouse.click(x, y)


def tap_section(page, size, canvas, label):
    """Taps the middle of a section's band in the overview or zoom canvas."""
    a, b = next((a, b) for name, a, b in SECTIONS if name == label)
    page.locator(f"#{canvas}").evaluate("el => el.scrollIntoView({block: 'center'})")
    box = page.locator(f"#{canvas}").bounding_box()
    t0, t1 = page.evaluate(f"[views.{canvas}.t0, views.{canvas}.t1]")
    band = page.evaluate(f"rows('{canvas}').sec[1]")
    mid = (max(a * 4 * PERIOD, t0) + min(b * 4 * PERIOD, t1)) / 2
    tap(page, size, box["x"] + (mid - t0) / (t1 - t0) * box["width"], box["y"] + band / 2)
    page.locator("#menu.open").wait_for()


def stored(root, tid, wait=5.0):
    """The track's saved rating once the page's debounced save has landed."""
    end = time.time() + wait
    while time.time() < end:
        path = root / "ratings.json"
        if path.exists() and tid in json.loads(path.read_text()):
            return json.loads(path.read_text())[tid]
        time.sleep(0.1)
    raise AssertionError("rating was not saved")


def wait_for(fn, wait=5.0):
    end = time.time() + wait
    while time.time() < end:
        value = fn()
        if value:
            return value
        time.sleep(0.1)
    return fn()


OVERFLOW = """() => {
  const W = document.documentElement.clientWidth, bad = [];
  for (const el of document.querySelectorAll("body *")) {
    const r = el.getBoundingClientRect();
    if (!r.width || !r.height || el.closest(".scroll-x")) continue;
    if (r.right > W + 1 || r.left < -1) bad.push(`${el.tagName}#${el.id}.${el.className} ${Math.round(r.left)}..${Math.round(r.right)}`);
  }
  return { scrollWidth: document.documentElement.scrollWidth, width: W, bad: bad.slice(0, 8) };
}"""

SMALL_TARGETS = """(min) => {
  const small = [];
  for (const el of document.querySelectorAll("button, input, textarea, summary, [role=button]")) {
    const r = el.getBoundingClientRect(), st = getComputedStyle(el);
    if (!r.width || !r.height || st.visibility === "hidden") continue;
    if (r.width < min - 0.5 || r.height < min - 0.5) small.push(`${el.tagName}#${el.id} "${el.textContent.trim().slice(0, 20)}" ${Math.round(r.width)}x${Math.round(r.height)}`);
  }
  return small;
}"""


def screens(page, base, size):
    """Each main screen, yielded after it is on screen."""
    login(page, base)
    yield "list"
    page.locator("#list .row").first.click()
    page.wait_for_function("() => views.overview && views.zoom")
    yield "track"
    tap_section(page, size, "overview", "drop")
    yield "section-menu"
    page.locator("#menu [data-close]").click()
    if size in TOUCH:
        page.locator("#btnRate").click()
        page.locator("#rate.open").wait_for()
    yield "rate"
    if size in TOUCH:
        page.locator("#rate [data-close]").click()
    page.locator("#btnSummary").click()
    page.locator(".tiles").wait_for()
    yield "summary"


@pytest.mark.parametrize("size", list(VIEWPORTS))
def test_no_screen_scrolls_sideways(browser_, library, size):
    base, _, _ = library
    page = open_page(browser_, size)
    for screen in screens(page, base, size):
        result = page.evaluate(OVERFLOW)
        assert result["scrollWidth"] <= result["width"] and not result["bad"], (screen, result)


@pytest.mark.parametrize("size", TOUCH)
def test_touch_targets_are_at_least_44px(browser_, library, size):
    base, _, _ = library
    page = open_page(browser_, size)
    for screen in screens(page, base, size):
        assert page.evaluate(SMALL_TARGETS, 44) == [], screen


@pytest.mark.parametrize("size", TOUCH)
def test_canvas_section_bands_are_tall_enough_to_tap(browser_, library, size):
    base, _, _ = library
    page = open_page(browser_, size)
    open_track(page, base, size)
    assert page.evaluate("[rows('overview').sec[1], rows('zoom').sec[1]]") == [44, 44]


def test_text_inputs_do_not_trigger_ios_zoom(browser_, library):
    base, _, _ = library
    page = open_page(browser_, "phone")
    page.goto(base + "/")
    assert page.evaluate("parseFloat(getComputedStyle(document.querySelector('#pin')).fontSize)") >= 16
    login(page, base)
    assert page.evaluate("parseFloat(getComputedStyle(document.querySelector('#search')).fontSize)") >= 16


@pytest.mark.parametrize("size", list(VIEWPORTS))
def test_transport_stays_in_the_thumb_zone_while_scrolling(browser_, library, size):
    base, _, _ = library
    page = open_page(browser_, size)
    open_track(page, base, size)
    for y in (0, 10000):
        page.evaluate(f"scrollTo(0, {y}); document.querySelector('main').scrollTop = {y}")
        box = page.locator("#play").bounding_box()
        height = page.viewport_size["height"]
        assert box["y"] + box["height"] <= height and box["y"] >= height - 100, (y, box)


TRANSPORT_CLEAR = """() => {
  const bar = document.querySelector("#transport").getBoundingClientRect(), main = document.querySelector("main").getBoundingClientRect();
  const hidden = [...document.querySelectorAll("#transport button")].filter(b => b.offsetWidth).filter(b => {
    const r = b.getBoundingClientRect();
    return !b.contains(document.elementFromPoint(r.x + r.width / 2, r.y + r.height / 2));
  }).map(b => b.id);
  const over = [...document.querySelectorAll("main .panel, #rate")].filter(p => {
    const r = p.getBoundingClientRect();
    return Math.min(r.bottom, main.bottom) > bar.top && Math.max(r.top, main.top) < bar.bottom && r.right > bar.left;
  }).map(p => p.id || p.className);
  return { hidden, over };
}"""


@pytest.mark.parametrize("width,height", [(1024, 768), (1280, 800), (1440, 900), (1920, 1080)])
@pytest.mark.parametrize("scroll", [0, 0.5, 1])
def test_desktop_panels_never_cover_the_transport(browser_, library, width, height, scroll):
    base, _, _ = library
    page = browser_.new_context(viewport={"width": width, "height": height}).new_page()
    open_track(page, base, "desktop")
    page.evaluate(f"""() => {{ const m = document.querySelector('main');
      m.scrollTop = {scroll} * (m.scrollHeight - m.clientHeight); scrollTo(0, {scroll} * document.documentElement.scrollHeight); }}""")
    assert page.evaluate(TRANSPORT_CLEAR) == {"hidden": [], "over": []}


@pytest.mark.parametrize("size", list(VIEWPORTS))
def test_the_pin_screen_guards_the_site(browser_, library, size):
    base, _, _ = library
    page = open_page(browser_, size)
    page.goto(base + "/")
    page.locator("#login.open").wait_for()
    page.locator("#pin").fill("000000")
    page.locator("#loginForm button").click()
    page.locator("#loginError", has_text="wrong PIN").wait_for()
    assert page.locator("#list .row").count() == 0
    page.locator("#pin").fill(PIN)
    page.locator("#loginForm button").click()
    page.locator("#list .row").first.wait_for()
    assert not page.locator("#login").is_visible()
    page.reload()
    page.locator("#list .row").first.wait_for()
    assert not page.locator("#login").is_visible()


@pytest.mark.parametrize("size", list(VIEWPORTS))
def test_rating_a_track_end_to_end(browser_, library, size):
    base, root, ids = library
    page = open_page(browser_, size)
    open_track(page, base, size)

    tap_section(page, size, "zoom", "intro")
    page.locator("#menu button", has_text="intro is right").click()
    assert wait_for(lambda: stored(root, ids[0])["sections"][0]["verdict"] == "ok")

    tap_section(page, size, "overview", "drop")
    page.locator("#menu button[data-l='groove']").click()
    drop = lambda: next(s for s in stored(root, ids[0])["sections"] if s["label"] == "drop")
    assert wait_for(lambda: drop()["correct_label"] == "groove" and drop()["verdict"] == "wrong")

    if size in TOUCH:
        page.locator("#btnRate").click()
    page.locator("#overall button[data-n='4']").click()
    page.locator("[data-grid='beats'] button[data-v='ok']").click()
    assert wait_for(lambda: stored(root, ids[0])["overall"] == 4 and stored(root, ids[0])["grid"]["beats"] == "ok")

    page.reload()
    page.wait_for_function("() => views.overview && views.zoom")
    assert page.locator("#overall button[data-n='4']").get_attribute("class") == "on"
    if size in TOUCH:
        assert page.locator("#btnRate").inner_text() == "Rate 4★"


@pytest.mark.parametrize("size", list(VIEWPORTS))
def test_a_marker_tool_tap_splits_the_section_there(browser_, library, size):
    base, root, ids = library
    page = open_page(browser_, size)
    open_track(page, base, size)
    page.locator("#tools button", has_text="drop").click()
    page.locator("#zoom").evaluate("el => el.scrollIntoView({block: 'center'})")
    box = page.locator("#zoom").bounding_box()
    t0, t1 = page.evaluate("[views.zoom.t0, views.zoom.t1]")
    wave = page.evaluate("rows('zoom').wave")
    t = 6 * 4 * PERIOD + 0.2  # just after bar 6
    tap(page, size, box["x"] + (t - t0) / (t1 - t0) * box["width"], box["y"] + wave[0] + wave[1] * 0.75)
    added = lambda: [s for s in stored(root, ids[0])["sections"] if s.get("added")]
    assert wait_for(lambda: [(s["label"], s["start_bar"]) for s in added()] == [("drop", 6)])


def test_dragging_a_section_edge_moves_it(browser_, library):
    base, root, ids = library
    page = open_page(browser_, "desktop")
    open_track(page, base, "desktop")
    box = page.locator("#overview").bounding_box()
    x = lambda bar: box["x"] + bar * 4 * PERIOD / DURATION * box["width"]
    y = box["y"] + page.evaluate("rows('overview').sec[1]") / 2
    page.mouse.move(x(12), y)
    page.mouse.down()
    page.mouse.move(x(13), y, steps=5)
    page.mouse.move(x(14), y, steps=5)
    page.mouse.up()
    drop = lambda: next(s for s in stored(root, ids[0])["sections"] if s["label"] == "drop")
    assert wait_for(lambda: drop()["true_start_bar"] == 14)


@pytest.mark.parametrize("library", [12.0], indirect=True)
@pytest.mark.parametrize("canvas", ["overview", "zoom"])
@pytest.mark.parametrize("size", list(VIEWPORTS))
def test_the_first_section_covers_the_beatless_intro(browser_, library, size, canvas):
    base, _, _ = library
    page = open_page(browser_, size)
    open_track(page, base, size)
    page.locator(f"#{canvas}").evaluate("el => el.scrollIntoView({block: 'center'})")
    band = page.evaluate(f"rows('{canvas}').sec")
    pixel = page.evaluate(f"""() => {{ const c = document.querySelector('#{canvas}'), k = devicePixelRatio;
      return [...c.getContext('2d').getImageData(3 * k, ({band[0]} + {band[1]} / 2) * k, 1, 1).data.slice(0, 3)]; }}""")
    assert pixel == page.evaluate("[1, 3, 5].map(i => parseInt(css('--sec-intro').slice(i, i + 2), 16))")
    box = page.locator(f"#{canvas}").bounding_box()
    t0, t1 = page.evaluate(f"[views.{canvas}.t0, views.{canvas}.t1]")
    tap(page, size, box["x"] + (5.0 - t0) / (t1 - t0) * box["width"], box["y"] + band[0] + band[1] / 2)
    page.locator("#menu.open").wait_for()
    assert "intro" in page.locator("#menu").inner_text() and "bars 0–8" in page.locator("#menu").inner_text()


@pytest.mark.parametrize("size", list(VIEWPORTS))
def test_play_from_a_section_starts_one_bar_before_it(browser_, library, size):
    base, _, _ = library
    page = open_page(browser_, size)
    open_track(page, base, size)
    tap_section(page, size, "overview", "drop")
    page.locator("#menu button[data-play]").click()
    page.wait_for_function("() => audio.readyState >= 1 && excerptStart !== null")
    start, end, now = page.evaluate("[excerptStart, excerptEnd, audio.currentTime]")
    assert start == 11 * 4 * PERIOD and end == 16 * 4 * PERIOD
    assert start - 0.05 <= now <= end


@pytest.mark.parametrize("size", ["phone", "desktop"])
def test_the_click_track_is_rendered_into_the_audio(browser_, library, size):
    base, root, ids = library
    page = open_page(browser_, size)
    open_track(page, base, size)
    page.locator("#click").click()
    assert "click=1" in page.evaluate("audio.src")
    page.wait_for_function("() => audio.readyState >= 1", timeout=30000)
    assert page.locator("#click").get_attribute("aria-pressed") == "true"
    assert (root / ".clicks" / f"{ids[0]}_0_0.mp3").exists()


def test_next_opens_the_next_unrated_track(browser_, library):
    base, _, ids = library
    page = open_page(browser_, "phone")
    open_track(page, base, "phone")
    page.locator("#btnNext").click()
    page.wait_for_function(f"() => currentId === '{ids[1]}' && doc && doc.track.title.startsWith('Beta')")
    assert page.locator("#heading").inner_text() == "Beta - Other"


ANSWERS = {"r01-p001": "none", "r01-p002": "b", "r01-p003": "skip"}


def answers(root):
    path = root / "rounds" / "answers.json"
    return {k: v["answer"] for k, v in json.loads(path.read_text()).items()} if path.exists() else {}


def open_round(page, base):
    page.goto(base + "/")
    page.locator("#pin").fill(PIN)
    page.locator("#loginForm button").click()
    page.locator(".quiz").wait_for()


def settle(page):
    """Waits until the screen has been up long enough for a tap not to count as a double tap."""
    page.wait_for_function("() => performance.now() - quizShown > 400")


def hit(page, selector, wait=True):
    if wait:
        settle(page)
    loc = page.locator(selector).first
    loc.tap() if page.evaluate("matchMedia('(pointer: coarse)').matches") else loc.click()


def answer(page, value):
    hit(page, f".q-answers button[data-v='{value}']")


def answer_and_wait(page, value):
    """Answers, then waits for the screen to change."""
    shown = page.evaluate("document.querySelector('.quiz').dataset.probe || 'x'")
    answer(page, value)
    page.wait_for_function("(b) => (document.querySelector('.quiz') && document.querySelector('.quiz').dataset.probe || 'x') !== b", arg=shown)


def go(page, n):
    """Shows question n (1 based) without answering the ones before it."""
    page.evaluate(f"goProbe({n - 1})")
    page.locator(f".quiz[data-probe='r01-p{n:03d}']").wait_for()


def playing(page):
    page.wait_for_function("() => !audio.paused && audio.currentTime > 0")


def test_a_round_is_answered_end_to_end_on_a_phone(browser_, round_library):
    base, root, _ = round_library
    page = open_page(browser_, "phone")
    open_round(page, base)
    assert page.locator("#heading").inner_text() == "Round 1 · 0/3"
    assert page.locator(".q-theory").inner_text() == ""    # the zone stays, empty: the Theory would tell which answer is expected
    for n, (probe_id, value) in enumerate(ANSWERS.items()):
        page.locator(f".quiz[data-probe='{probe_id}']").wait_for()
        if n == 0:
            hit(page, ".q-play")    # later questions start by themselves after the answer
        playing(page)
        answer(page, value)
        assert wait_for(lambda: answers(root).get(probe_id) == value)
    page.locator(".q-done").wait_for()
    assert page.locator("#heading").inner_text() == "Round 1 · 3/3"

    page.locator("#qReview").tap()
    page.locator(".quiz[data-probe='r01-p001'] .q-answers button.on[data-v='none']").wait_for()
    answer(page, "yes")
    assert wait_for(lambda: answers(root)["r01-p001"] == "yes")
    page.locator(".quiz[data-probe='r01-p002'] .q-answers button.on[data-v='b']").wait_for()
    page.locator("#qBack").tap()
    page.locator(".quiz[data-probe='r01-p001'] .q-answers button.on[data-v='yes']").wait_for()
    page.locator("#qSkip").tap()
    page.locator(".quiz[data-probe='r01-p002']").wait_for()
    assert answers(root) == {**ANSWERS, "r01-p001": "yes"}


def test_skipped_questions_come_back_before_the_round_is_done(browser_, round_library):
    base, root, _ = round_library
    page = open_page(browser_, "phone")
    open_round(page, base)
    page.locator("#qSkip").tap()
    answer(page, "a")
    page.locator(".quiz[data-probe='r01-p003']").wait_for()
    answer(page, "yes")
    page.locator(".quiz[data-probe='r01-p001']").wait_for()
    assert answers(root) == {"r01-p002": "a", "r01-p003": "yes"}


def test_ab_plays_each_play_as_its_own_clip_and_chains_a_into_b_only_by_itself(browser_, round_library):
    base, _, ids = round_library
    page = open_page(browser_, "phone")
    open_round(page, base)
    page.locator("#qSkip").tap()
    page.locator(".quiz[data-probe='r01-p002']").wait_for()
    assert page.locator(".q-play").all_inner_texts() == ["▶ Play A", "▶ Play B"]
    page.locator(".q-play").nth(0).tap()
    page.wait_for_function("() => !audio.paused && audio.currentTime > 0 && audio.currentTime < 8")
    assert page.evaluate("audio.src").endswith("/clip/r01-p002/a?cue=1")
    page.locator(".q-play").nth(1).tap()
    page.wait_for_function("() => !audio.paused && audio.src.endsWith('/clip/r01-p002/b')")
    page.wait_for_function("() => isFinite(audio.duration) && audio.currentTime > 0")
    page.evaluate("audio.currentTime = audio.duration - 0.3")   # a tapped play does not go on to the next one
    page.wait_for_function("() => audio.ended")
    assert page.evaluate("audio.src").endswith("/clip/r01-p002/b")


@pytest.mark.parametrize("answered,opens", [([], ".quiz"), (list(ANSWERS), "#list .row")])
def test_the_site_opens_on_the_questions_only_while_some_are_open(browser_, round_library, answered, opens):
    base, root, _ = round_library
    for probe_id in answered:
        record_answer(root, probe_id, ANSWERS[probe_id])
    page = open_page(browser_, "phone")
    page.goto(base + "/")
    page.locator("#pin").fill(PIN)
    page.locator("#loginForm button").click()
    page.locator(opens).first.wait_for()


def question_screens(page, base):
    open_round(page, base)
    for probe_id in ANSWERS:
        page.locator(f".quiz[data-probe='{probe_id}']").wait_for()
        yield probe_id
        answer(page, ANSWERS[probe_id])
    page.locator(".q-done").wait_for()
    yield "done"


ANSWER_BAR = """() => {
  const r = document.querySelector(".q-answers").getBoundingClientRect();
  const small = [...document.querySelectorAll(".q-answers button, .q-play")].map(b => b.getBoundingClientRect()).filter(b => b.height < 56 || b.width < 56);
  return { bottom: Math.round(r.bottom), top: Math.round(r.top), height: innerHeight, small: small.length };
}"""


@pytest.mark.parametrize("size", TOUCH)
def test_question_screens_fit_and_answers_sit_in_the_thumb_zone(browser_, round_library, size):
    base, _, _ = round_library
    page = open_page(browser_, size)
    for screen in question_screens(page, base):
        overflow = page.evaluate(OVERFLOW)
        assert overflow["scrollWidth"] <= overflow["width"] and not overflow["bad"], (screen, overflow)
        assert page.evaluate(SMALL_TARGETS, 44) == [], screen
        bar = page.evaluate(ANSWER_BAR)
        assert bar["small"] == 0 and bar["height"] - 200 <= bar["top"] and bar["bottom"] <= bar["height"], (screen, bar)


NEW = ["yesno", "locate", "label", "yesno without taps", "ab", "label with its own labels"]


def new_screens(page, base):
    """Each kind of question, and the label question after No."""
    open_round(page, base)
    for n, name in enumerate(NEW, 1):
        go(page, n)
        yield name
        if name.startswith("label"):
            settle(page)
            page.locator(".q-answers button[data-v='no']").click(force=True)
            page.locator(".q-what").wait_for()
            yield name + " after No"


@pytest.mark.parametrize("size", list(VIEWPORTS))
def test_every_kind_of_question_fits_with_its_answers_in_reach(browser_, new_library, size):
    base, _, _ = new_library
    page = open_page(browser_, size)
    for screen in new_screens(page, base):
        overflow = page.evaluate(OVERFLOW)
        assert overflow["scrollWidth"] <= overflow["width"] and not overflow["bad"], (screen, overflow)
        if size in TOUCH:
            assert page.evaluate(SMALL_TARGETS, 44) == [], screen
            bar = page.evaluate(ANSWER_BAR)
            assert bar["small"] == 0 and bar["height"] - 220 <= bar["top"] and bar["bottom"] <= bar["height"], (screen, bar)
            assert page.evaluate("document.documentElement.scrollHeight <= innerHeight"), screen   # nothing to scroll to reach an answer


def pixel_at(page, canvas, t, y):
    """RGB of the strip at track time t; y is "mid" or "bottom"."""
    return page.evaluate("""([sel, t, y]) => { const c = document.querySelector(sel), p = quiz.probes[quizIndex].plays[+c.dataset.k];
        const x = Math.min(c.width - 1, Math.round((t - p.start) / (p.end - p.start) * c.width));
        return [...c.getContext('2d').getImageData(x, y === 'mid' ? c.height >> 1 : c.height - 2, 1, 1).data].slice(0, 3); }""", [canvas, t, y])


def colour(page, name):
    return page.evaluate(f"[1, 3, 5].map(i => parseInt(css('{name}').slice(i, i + 2), 16))")


@pytest.mark.parametrize("size", list(VIEWPORTS))
def test_each_play_has_a_waveform_with_bar_lines_and_a_marker_only_where_there_is_one(browser_, new_library, size):
    base, _, _ = new_library
    page = open_page(browser_, size)
    open_round(page, base)
    assert page.locator(".q-wave").count() == 1
    assert pixel_at(page, ".q-wave", 24.0, "mid") == colour(page, "--mark")                       # the marker, full height
    plain = pixel_at(page, ".q-wave", 21.25, "bottom")
    assert pixel_at(page, ".q-wave", 22.0, "bottom") != plain and pixel_at(page, ".q-wave", 21.0, "bottom") != plain   # a bar line and a beat tick
    box = page.locator(".q-wave").bounding_box()
    assert box["width"] > 200 and box["height"] >= 56
    go(page, 2)    # locate: nothing to mark
    mark = colour(page, "--mark")
    row = page.evaluate("""() => { const c = document.querySelector('.q-wave'), d = c.getContext('2d').getImageData(0, c.height >> 1, c.width, 1).data, out = [];
        for (let i = 0; i < d.length; i += 4) out.push([d[i], d[i + 1], d[i + 2]]); return out; }""")
    assert mark not in row


@pytest.mark.parametrize("size", list(VIEWPORTS))
def test_the_strip_tints_what_was_played_and_the_playhead_moves(browser_, new_library, size):
    base, _, _ = new_library
    page = open_page(browser_, size)
    open_round(page, base)
    hit(page, ".q-play")
    playing(page)
    page.evaluate("audio.pause(); audio.currentTime = 4.0")
    page.wait_for_function("() => audio.currentTime === 4.0")
    page.wait_for_timeout(150)
    assert pixel_at(page, ".q-wave", 24.0, "mid") == colour(page, "--playhead")
    assert pixel_at(page, ".q-wave", 21.25, "bottom") != pixel_at(page, ".q-wave", 27.25, "bottom")   # played part is tinted
    page.evaluate("audio.currentTime = 6.0")
    page.wait_for_timeout(150)
    assert pixel_at(page, ".q-wave", 26.0, "mid") == colour(page, "--playhead")


@pytest.mark.parametrize("size", list(VIEWPORTS))
def test_tapping_the_strip_seeks_inside_the_clip(browser_, new_library, size):
    base, _, _ = new_library
    page = open_page(browser_, size)
    open_round(page, base)
    hit(page, ".q-play")
    playing(page)
    src = page.evaluate("audio.src")
    box = page.locator(".q-wave").bounding_box()
    tap(page, size, box["x"] + box["width"] * 0.75, box["y"] + box["height"] / 2)
    page.wait_for_function("() => audio.currentTime > 5.5 && audio.currentTime < 7.5")
    assert page.evaluate("audio.src") == src


@pytest.mark.parametrize("size", list(VIEWPORTS))
def test_bar_buttons_answer_with_the_bar_index(browser_, new_library, size):
    base, root, _ = new_library
    page = open_page(browser_, size)
    open_round(page, base)
    assert page.locator(".q-barbtn").all_inner_texts() == ["1", "2", "3", "4", "5"]
    assert page.locator(".q-barbtn").first.get_attribute("data-v") == "bar:10"   # bar 10 starts at 20 s
    hit(page, ".q-barbtn >> nth=2")
    assert wait_for(lambda: answers(root) == {"r01-p001": "bar:12"})


def test_a_round_in_the_stored_shape_still_shows_bar_buttons_and_the_old_answers(browser_, round_library, stored_shape):
    base, root, _ = round_library
    page = open_page(browser_, "phone")
    open_round(page, base)
    assert page.locator(".q-barbtn").count() == 5
    assert page.locator(".q-answers button").evaluate_all("els => els.map(e => e.dataset.v)") == ["yes", "no", "earlier", "later", "skip"]
    answer(page, "earlier")
    page.locator(".quiz[data-probe='r01-p002']").wait_for()
    assert page.locator(".q-barbtn").count() == 0                       # an A/B pair takes no bar taps
    go(page, 3)
    assert "breakdown" in page.locator(".q-define").inner_text().lower()   # the label comes from the prompt, the choices from the defaults
    answer(page, "no")
    assert page.locator(".q-what button[data-v^='is:']").evaluate_all("els => els.map(e => e.dataset.v)") == \
        [f"is:{l}" for l in ("intro", "buildup", "drop", "groove", "outro")]
    assert answers(root) == {"r01-p001": "earlier"}


@pytest.mark.parametrize("size", list(VIEWPORTS))
def test_a_locate_question_is_answered_on_the_bars(browser_, new_library, size):
    base, root, _ = new_library
    page = open_page(browser_, size)
    open_round(page, base)
    go(page, 2)
    assert page.locator(".q-answers button").evaluate_all("els => els.map(e => e.dataset.v)") == ["none", "skip"]
    assert page.locator(".q-barbtn").count() == 5
    hit(page, ".q-barbtn >> nth=1")
    assert wait_for(lambda: answers(root) == {"r01-p002": "bar:15"})


def test_a_question_that_takes_no_bar_taps_has_no_bar_buttons(browser_, new_library):
    base, _, _ = new_library
    page = open_page(browser_, "phone")
    open_round(page, base)
    for n in (4, 5, 3):
        go(page, n)
        assert page.locator(".q-barbtn").count() == 0


LABELS = ["intro", "buildup", "drop", "breakdown", "groove", "outro"]


@pytest.mark.parametrize("label", LABELS)
def test_a_label_question_says_in_one_line_what_the_label_means(browser_, library, contract, label):
    base, _, ids = library
    article = "an" if label[0] in "aeiou" else "a"
    put_round(library, make_round(ids, [probe(1, ids[0], "label", f"Is this part {article} {label}?", [{"id": "main", "start": 40.0, "end": 48.0, "mark": 44.0}])]))
    page = open_page(browser_, "phone")
    open_round(page, base)
    text = page.locator(".q-define").inner_text()
    assert text.lower().startswith(label) and 30 < len(text) < 150
    assert page.locator(".q-define").evaluate("el => el.getClientRects().length > 0 && el.getBoundingClientRect().height < 70")


def test_the_six_definitions_are_different(browser_, library, contract):
    base, _, ids = library
    put_round(library, make_round(ids, [probe(i, ids[0], "label", f"Is this part a {l}?", [{"id": "main", "start": 40.0, "end": 48.0, "mark": 44.0}]) for i, l in enumerate(LABELS, 1)]))
    page = open_page(browser_, "phone")
    open_round(page, base)
    texts = []
    for n in range(1, 7):
        go(page, n)
        texts.append(page.locator(".q-define").inner_text())
    assert len(set(texts)) == 6


@pytest.mark.parametrize("n,others,pick,expected", [
    (3, ["intro", "buildup", "breakdown", "groove", "outro"], "is:groove", "is:groove"),
    (3, ["intro", "buildup", "breakdown", "groove", "outro"], "no", "no"),
    (6, ["groove", "outro"], "is:outro", "is:outro"),
])
def test_no_on_a_label_question_asks_what_it_is(browser_, new_library, n, others, pick, expected):
    base, root, _ = new_library
    page = open_page(browser_, "phone")
    open_round(page, base)
    go(page, n)
    answer(page, "no")
    page.locator(".q-what").wait_for()
    assert answers(root) == {}     # nothing is stored until the reviewer says what it is, or just no
    assert page.locator(".q-what button[data-v^='is:']").evaluate_all("els => els.map(e => e.dataset.v)") == [f"is:{o}" for o in others]
    assert page.locator(".q-what button[data-v='no']").inner_text().startswith("Just no")
    hit(page, f".q-what button[data-v='{pick}']")
    assert wait_for(lambda: answers(root) == {f"r01-p{n:03d}": expected})


@pytest.mark.parametrize("stored", ["no", "is:groove"])
def test_a_label_question_answered_no_shows_no_when_it_comes_back(browser_, new_library, stored):
    base, root, _ = new_library
    record_answer(root, "r01-p003", stored)
    page = open_page(browser_, "phone")
    open_round(page, base)
    go(page, 3)
    assert page.locator(".q-answers button.on").get_attribute("data-v") == "no"


def test_autoplay_starts_the_next_question_after_an_answer_and_can_be_turned_off(browser_, new_library):
    base, root, _ = new_library
    page = open_page(browser_, "phone")
    open_round(page, base)
    assert page.evaluate("audio.paused")                       # no tap yet: nothing may start by itself
    assert page.locator("#optAutoplay").get_attribute("aria-pressed") == "true"
    answer(page, "yes")
    page.locator(".quiz[data-probe='r01-p002']").wait_for()
    playing(page)
    assert page.evaluate("audio.src").endswith("/clip/r01-p002/main")
    hit(page, "#optAutoplay")
    assert page.locator("#optAutoplay").get_attribute("aria-pressed") == "false"
    answer(page, "skip")
    page.locator(".quiz[data-probe='r01-p003']").wait_for()
    page.wait_for_timeout(900)
    assert page.evaluate("audio.paused")
    page.reload()
    page.locator(".quiz").wait_for()
    assert page.locator("#optAutoplay").get_attribute("aria-pressed") == "false"


def test_the_page_works_when_the_browser_refuses_local_storage(browser_, new_library):
    base, root, _ = new_library
    page = open_page(browser_, "phone")
    page.add_init_script("Object.defineProperty(window, 'localStorage', { get() { throw new Error('blocked') } })")
    open_round(page, base)
    hit(page, "#optLoop")
    hit(page, "#optAutoplay")
    answer(page, "yes")
    assert wait_for(lambda: answers(root) == {"r01-p001": "yes"})


def test_a_pair_plays_a_then_b_by_itself_after_an_answer(browser_, new_library):
    base, _, _ = new_library
    page = open_page(browser_, "phone")
    open_round(page, base)
    go(page, 4)
    answer(page, "yes")        # question 5 is the A/B pair
    page.locator(".quiz[data-probe='r01-p005']").wait_for()
    page.wait_for_function("() => !audio.paused && audio.src.endsWith('/clip/r01-p005/a') && isFinite(audio.duration) && audio.currentTime > 0")
    page.evaluate("audio.currentTime = audio.duration - 0.3")
    page.wait_for_function("() => !audio.paused && audio.src.endsWith('/clip/r01-p005/b')")


@pytest.mark.parametrize("loop", [True, False])
def test_loop_repeats_the_clip(browser_, new_library, loop):
    base, _, _ = new_library
    page = open_page(browser_, "phone")
    open_round(page, base)
    if loop:
        hit(page, "#optLoop")
    assert page.locator("#optLoop").get_attribute("aria-pressed") == str(loop).lower()
    hit(page, ".q-play")
    playing(page)
    page.evaluate("audio.currentTime = audio.duration - 0.4")
    page.wait_for_timeout(1300)
    assert page.evaluate("!audio.paused && audio.currentTime < 3") == loop
    assert page.evaluate("audio.ended") == (not loop)


def test_the_cue_toggle_switches_the_clip_and_is_remembered(browser_, new_library):
    base, _, _ = new_library
    page = open_page(browser_, "phone")
    open_round(page, base)
    assert page.locator("#optCue").get_attribute("aria-pressed") == "true"
    hit(page, ".q-play")
    playing(page)
    assert page.evaluate("audio.src").endswith("/clip/r01-p001/main?cue=1")
    hit(page, "#optCue")
    page.wait_for_function("() => !audio.src.includes('cue=1') && !audio.paused")
    page.reload()
    page.locator(".quiz").wait_for()
    assert page.locator("#optCue").get_attribute("aria-pressed") == "false"
    hit(page, ".q-play")
    playing(page)
    assert not page.evaluate("audio.src").endswith("cue=1")


@pytest.mark.parametrize("n,visible", [(1, True), (2, False), (5, False)])
def test_the_cue_toggle_shows_only_where_a_play_has_a_marker_without_clicks(browser_, new_library, n, visible):
    base, _, _ = new_library
    page = open_page(browser_, "phone")
    open_round(page, base)
    go(page, n)
    assert page.locator("#optCue").is_visible() and page.locator("#optCue").is_enabled() == visible


def press(page, key, wait=True):
    if wait:
        settle(page)
    page.keyboard.press(key)


def test_a_round_can_be_answered_from_the_keyboard_alone(browser_, new_library):
    base, root, _ = new_library
    page = open_page(browser_, "desktop")
    open_round(page, base)
    assert page.locator(".q-answers button[data-v='yes'] kbd").inner_text() == "Y"
    assert page.locator(".q-play kbd").inner_text() == "Space"
    assert page.locator("#optReplay kbd").inner_text() == "R"
    press(page, "Space")
    playing(page)
    page.wait_for_timeout(700)
    press(page, "Space")
    assert page.evaluate("audio.paused")
    paused_at = page.evaluate("audio.currentTime")
    press(page, "r")
    page.wait_for_function(f"() => !audio.paused && audio.currentTime < {paused_at}")
    press(page, "y")                                                   # 1 yesno
    page.locator(".quiz[data-probe='r01-p002']").wait_for()
    assert page.locator(".q-barbtn").first.inner_text() == "1"
    press(page, "2")                                                   # 2 locate: second bar line
    page.locator(".quiz[data-probe='r01-p003']").wait_for()
    press(page, "n")                                                   # 3 label: not a drop ...
    page.locator(".q-what").wait_for()
    press(page, "1")                                                   # ... it is the first of the other labels
    page.locator(".quiz[data-probe='r01-p004']").wait_for()
    press(page, "s")                                                   # 4 skipped
    page.locator(".quiz[data-probe='r01-p005']").wait_for()
    press(page, "ArrowLeft")
    page.locator(".quiz[data-probe='r01-p004']").wait_for()
    press(page, "ArrowRight")
    page.locator(".quiz[data-probe='r01-p005']").wait_for()
    press(page, "b")                                                   # 5 A/B
    page.locator(".quiz[data-probe='r01-p006']").wait_for()
    assert wait_for(lambda: len(answers(root)) == 4)
    assert answers(root) == {"r01-p001": "yes", "r01-p002": "bar:15", "r01-p003": "is:intro", "r01-p005": "b"}
    press(page, "Escape")
    page.wait_for_function("() => document.body.dataset.view === 'list'")


def test_two_quick_key_presses_answer_only_one_question(browser_, library, contract):
    base, root, ids = library
    put_round(library, many_round(ids))
    page = open_page(browser_, "desktop")
    open_round(page, base)
    settle(page)
    page.keyboard.press("y")
    page.keyboard.press("y")
    page.locator(".quiz[data-probe='r01-p002']").wait_for()
    page.wait_for_timeout(700)
    assert answers(root) == {"r01-p001": "yes"}


@pytest.mark.parametrize("size", list(VIEWPORTS))
def test_there_is_a_clear_way_out_to_the_songs_and_back_to_the_questions(browser_, new_library, size):
    base, _, _ = new_library
    page = open_page(browser_, size)
    open_round(page, base)
    assert page.locator("#btnSongs").is_visible() and page.locator("#btnSongs").inner_text().startswith("Songs")
    hit(page, "#btnSongs", wait=False)
    page.wait_for_function("() => document.body.dataset.view === 'list'")
    assert page.locator("#btnRound").is_visible() and not page.locator("#btnSongs").is_visible()
    hit(page, "#btnRound", wait=False)
    page.locator(".quiz").wait_for()


@pytest.mark.parametrize("width,height", [(1024, 768), (1440, 900), (1920, 1080)])
def test_on_desktop_the_question_is_centred_without_the_song_list_and_the_answers_follow_the_plays(browser_, new_library, width, height):
    base, _, _ = new_library
    page = browser_.new_context(viewport={"width": width, "height": height}).new_page()
    open_round(page, base)
    assert not page.locator("aside").is_visible() and not page.locator("#list").is_visible()
    quiz = page.locator(".quiz").bounding_box()
    assert abs(quiz["x"] + quiz["width"] / 2 - width / 2) <= 3 and quiz["width"] <= 800
    plays, answers_ = page.locator(".q-cards").bounding_box(), page.locator(".q-answers").bounding_box()
    assert 0 <= answers_["y"] - (plays["y"] + plays["height"]) < 350
    assert page.evaluate("document.documentElement.scrollHeight <= innerHeight")
    overflow = page.evaluate(OVERFLOW)
    assert overflow["scrollWidth"] <= overflow["width"] and not overflow["bad"]


@pytest.mark.parametrize("size,presses,answered,action", [
    ("desktop", 10, 0, "#qContinue"), ("phone", 3, 7, "#qContinue"), ("desktop", 3, 7, "#qStop")])
def test_a_calm_screen_follows_every_ten_answers(browser_, library, contract, size, presses, answered, action):
    base, root, ids = library
    put_round(library, many_round(ids))
    for n in range(1, answered + 1):
        record_answer(root, f"r01-p{n:03d}", "yes")
    page = open_page(browser_, size)
    open_round(page, base)
    for _ in range(presses):
        assert page.locator(".q-break").count() == 0
        answer_and_wait(page, "yes")
    page.locator(".q-break").wait_for()
    text = page.locator(".q-break").inner_text()
    assert "10 answered" in text and "2 left" in text
    overflow = page.evaluate(OVERFLOW)
    assert overflow["scrollWidth"] <= overflow["width"] and not overflow["bad"]
    if size in TOUCH:
        assert page.evaluate(SMALL_TARGETS, 44) == []
    hit(page, action)
    if action == "#qContinue":
        page.locator(".quiz[data-probe='r01-p011']").wait_for()
    else:
        page.wait_for_function("() => document.body.dataset.view === 'list'")
    assert len(answers(root)) == 10


def test_no_break_comes_after_the_last_answer_of_a_round(browser_, library, contract):
    base, root, ids = library
    put_round(library, many_round(ids, 10))
    for n in range(1, 10):
        record_answer(root, f"r01-p{n:03d}", "yes")
    page = open_page(browser_, "phone")
    open_round(page, base)
    answer(page, "yes")
    page.locator(".q-done").wait_for()
    assert page.locator(".q-break").count() == 0


def answered_round(library, values=("yes", "b", "yes")):
    base, root, ids = library
    for n, v in enumerate(values, 1):
        record_answer(root, f"r01-p{n:03d}", v)
    return base, root


@pytest.mark.parametrize("size", list(VIEWPORTS))
def test_the_finished_round_shows_the_report(browser_, round_library, size):
    base, root = answered_round(round_library, ("yes", "b", "no"))
    record_answer(root, "r01-p003", "yes", now="2026-11-06T10:00:00")
    page = open_page(browser_, size)
    page.goto(base + "/")
    page.locator("#pin").fill(PIN)
    page.locator("#loginForm button").click()
    page.locator("#list .row").first.wait_for()
    page.locator("#btnRound").click()
    page.locator(".q-done .report").wait_for()
    text = page.locator(".report").inner_text().lower()   # headings are set in capitals
    for part in ("Round 1", "3/3", "Theories", "Calibration", "0-50", "67%", "Target", "Coverage"):
        assert part.lower() in text, (part, text)
    overflow = page.evaluate(OVERFLOW)
    assert overflow["scrollWidth"] <= overflow["width"] and not overflow["bad"], overflow
    if size in TOUCH:
        assert page.evaluate(SMALL_TARGETS, 44) == []


@pytest.mark.parametrize("sources,shown", [({"vdj": {"n": 12, "exact": 0.5}, "ours": {"n": 30, "exact": None}}, True), (None, False)])
def test_the_report_lists_sources_only_when_the_round_logic_sends_them(browser_, round_library, monkeypatch, sources, shown):
    base, root = answered_round(round_library)
    real = rounds.report
    monkeypatch.setattr(rounds, "report", lambda *a: {**{k: v for k, v in real(*a).items() if k != "sources"}, **({"sources": sources} if sources else {})})
    page = open_page(browser_, "desktop")
    page.goto(base + "/")
    page.locator("#pin").fill(PIN)
    page.locator("#loginForm button").click()
    page.locator("#list .row").first.wait_for()
    page.locator("#btnSummary").click()
    page.locator(".report .panel").first.wait_for()
    text = page.locator(".report").inner_text().lower()
    assert ("sources" in text) == shown
    if shown:
        assert "vdj" in text and "50%" in text and "30" in text


@pytest.mark.parametrize("reviewer,said", [({"repeats": 6, "same": 0.5}, "3 of 6 questions"), ({"repeats": 10, "same": 0.9}, "9 of 10 questions"),
                                           ({"repeats": 1, "same": 1.0}, "1 of 1 question "), ({"repeats": 0, "same": None}, None), (None, None)])
def test_the_summary_shows_the_reviewers_own_consistency_when_questions_came_up_again(browser_, round_library, monkeypatch, reviewer, said):
    base, _ = answered_round(round_library)
    real = rounds.report
    monkeypatch.setattr(rounds, "report", lambda *a: {k: v for k, v in {**real(*a), "reviewer": reviewer}.items() if v is not None})
    page = open_page(browser_, "phone")
    page.goto(base + "/")
    page.locator("#pin").fill(PIN)
    page.locator("#loginForm button").click()
    page.locator("#list .row").first.wait_for()
    page.locator("#btnSummary").tap()
    page.locator(".report .panel").first.wait_for()
    panel = page.locator(".report .panel", has_text="Consistency")
    assert panel.count() == (1 if said else 0)
    if said:
        text = panel.inner_text()
        assert said in text and "the same way when they came up again" in text and f"{round(reviewer['same'] * 100)}%" in text
        assert "repeat" not in text.lower() and panel.bounding_box()["y"] > page.locator(".report .panel", has_text="Calibration").bounding_box()["y"]


@pytest.mark.parametrize("size", list(VIEWPORTS))
def test_the_summary_shows_the_report_above_the_old_ratings_summary(browser_, round_library, size):
    base, _ = answered_round(round_library)
    page = open_page(browser_, size)
    page.goto(base + "/")
    page.locator("#pin").fill(PIN)
    page.locator("#loginForm button").click()
    page.locator("#list .row").first.wait_for()
    page.locator("#btnSummary").click()
    page.locator(".report").wait_for()
    page.locator(".tiles").wait_for()
    assert page.locator(".report").bounding_box()["y"] < page.locator(".tiles").bounding_box()["y"]
    overflow = page.evaluate(OVERFLOW)
    assert overflow["scrollWidth"] <= overflow["width"] and not overflow["bad"], overflow
    if size in TOUCH:
        assert page.evaluate(SMALL_TARGETS, 44) == []


@pytest.mark.parametrize("answered,text", [(0, "Round 1 · 0/3"), (2, "Round 1 · 2/3")])
def test_the_header_shows_the_round_progress_in_the_song_list(browser_, round_library, answered, text):
    base, root, _ = round_library
    for n in range(1, answered + 1):
        record_answer(root, f"r01-p{n:03d}", ("yes", "a", "yes")[n - 1])
    page = open_page(browser_, "desktop")
    open_round(page, base)
    hit(page, "#btnSongs", wait=False)
    page.wait_for_function("() => document.body.dataset.view === 'list'")
    assert page.locator("#progress").inner_text() == text


@pytest.fixture
def candidate_library(library):
    base, root, ids = library
    for name, raw in {"edmformer": [{"start": 0.0, "label": "intro"}, {"start": 24.0, "label": "drop"}, {"start": 48.0, "label": "outro"}],
                      "allin1": {"segments": [{"start": 0.0, "label": "intro"}, {"start": 16.0, "label": "buildup"}], "downbeats": []},
                      "dropdetector": {"drops": [{"time": 32.0, "confidence": 0.83}]}}.items():
        (root / "candidates" / name).mkdir(parents=True)
        (root / "candidates" / name / f"{ids[0]}.json").write_text(json.dumps(raw))
    return library


@pytest.mark.parametrize("size", list(VIEWPORTS))
@pytest.mark.parametrize("canvas", ["overview", "zoom"])
def test_candidate_outputs_are_labelled_reference_rows_under_the_virtualdj_row(browser_, candidate_library, size, canvas):
    base, _, _ = candidate_library
    page = open_page(browser_, size)
    open_track(page, base, size)
    assert page.evaluate("candidateRows().map(r => r.label)") == ["EDMFormer", "allin1", "drop det.", "rules"]
    r = page.evaluate(f"rows('{canvas}')")
    assert [c[0] for c in r["cand"]] == sorted(c[0] for c in r["cand"]) and len(r["cand"]) == 4
    assert r["ref"][0] + r["ref"][1] <= r["cand"][0][0] and r["cand"][3][0] + r["cand"][3][1] <= r["wave"][0]
    assert all(h >= 10 for _, h in [c[:2] for c in r["cand"]])
    if canvas == "overview":
        page.locator("#overview").evaluate("el => el.scrollIntoView({block: 'center'})")
        page.wait_for_function("() => views.overview && views.overview.rows.cand")
        px = lambda t, row: page.evaluate("""([t, row]) => { const c = document.querySelector('#overview'), v = views.overview, k = devicePixelRatio, r = rows('overview').cand[row];
            return [...c.getContext('2d').getImageData(Math.round((t - v.t0) / (v.t1 - v.t0) * c.width), (r[0] + r[1] / 2) * k, 1, 1).data].slice(0, 3); }""", [t, row])
        assert px(36.0, 0) == colour(page, "--sec-drop") and px(20.0, 0) == colour(page, "--sec-intro")      # EDMFormer
        assert px(20.0, 1) == colour(page, "--sec-buildup")                                                  # allin1
        assert px(32.0, 2) != px(44.0, 2)                                                                    # a drop tick
        assert px(17.0, 3) == colour(page, "--sec-intro") and px(21.0, 3) == colour(page, "--sec-buildup")  # the rules: their buildup starts at bar 10


def test_a_track_without_candidates_has_no_candidate_rows(browser_, library):
    base, _, _ = library
    page = open_page(browser_, "desktop")
    open_track(page, base, "desktop")
    assert page.evaluate("candidateRows().length") == 0 and page.evaluate("rows('overview').cand") in ([], None)


@pytest.mark.parametrize("model,index,text", [(False, 0, "intro"), (False, 1, "buildup"), (False, 2, "drop"), (True, 0, "intro 100%"), (True, 1, "buildup 50%"), (True, 2, "drop 50%")])
def test_section_bands_show_the_claim_confidence_when_the_analysis_has_it(browser_, library, model, index, text):
    base, root, _ = library
    if model:   # the confidence model's numbers reach the Analysis QLC+ gets; this one knows nothing, so every start says 50
        (root / "confidence-model.json").write_text(json.dumps({"groups": {}}))
    page = open_page(browser_, "desktop")
    open_track(page, base, "desktop")
    assert page.evaluate(f"bandLabel({index})") == text


MOVED_DROP = [("intro", 0, 8), ("drop", 8, 24), ("outro", 24, 32)]   # the levels only rise at bar 12: the fusion moves the drop there, onto no source's bar


@pytest.mark.parametrize("size", list(VIEWPORTS))
@pytest.mark.parametrize("raw,label,said", [(RULES, "drop", "On this bar in: rules"), (RULES, "buildup", "On this bar in: rules"),
                                            (MOVED_DROP, "buildup", "On this bar in: rules"), (MOVED_DROP, "drop", "No source has it on this bar")])
def test_the_section_box_says_which_sources_put_the_section_on_its_bar(browser_, library, size, raw, label, said):
    base, root, ids = library
    path = root / f"{ids[0]}.json"
    doc = json.loads(path.read_text())
    doc["sections"] = [{"label": n, "start_bar": a, "end_bar": b} for n, a, b in raw]
    doc["events"] = [e for e in doc["events"] if e["kind"] != "drop"] + [{"kind": "drop", "bar": next(a for n, a, _ in raw if n == "drop")}]
    path.write_text(json.dumps(doc))
    page = open_page(browser_, size)
    open_track(page, base, size)
    assert page.evaluate("rating.sections.map(s => [s.label, s.start_bar, s.end_bar])") == [[n, a, b] for n, a, b in SECTIONS]
    tap_section(page, size, "overview", label)
    assert said in page.locator("#menu .title").inner_text()
    assert page.locator("#menu").bounding_box()["y"] + page.locator("#menu").bounding_box()["height"] <= page.viewport_size["height"] + 1


def test_the_song_view_shows_the_fused_sections_and_events(browser_, library):
    base, _, _ = library
    page = open_page(browser_, "desktop")
    open_track(page, base, "desktop")
    assert page.evaluate("rating.sections.map(s => [s.label, s.start_bar, s.end_bar])") == [[n, a, b] for n, a, b in SECTIONS]
    assert page.evaluate("rating.events.filter(e => e.kind === 'buildup').map(e => e.bar)") == [8]


@pytest.mark.parametrize("prerendered,probe_n,limit", [(True, 1, 1.0), (True, 2, 1.0), (False, 1, 2.0), (False, 2, 2.0)])
def test_the_first_play_of_a_probe_starts_fast(browser_, round_library, prerendered, probe_n, limit):
    base, root, _ = round_library
    if prerendered:
        Clips(root).prerender(json.loads((root / "rounds" / "round-01.json").read_text()))
    page = open_page(browser_, "phone")
    open_round(page, base)
    go(page, probe_n)
    page.evaluate("() => { window.heard = null; audio.addEventListener('playing', () => { window.heard ??= performance.now(); }); }")
    t0 = page.evaluate("performance.now()")
    page.locator(".q-play").last.tap() if probe_n == 2 else page.locator(".q-play").tap()
    page.wait_for_function("() => window.heard !== null", timeout=10000)
    ms = page.evaluate("window.heard") - t0
    print(f"first play of probe {probe_n}, {'prerendered' if prerendered else 'cold'}: {ms:.0f} ms")
    assert ms < limit * 1000


def pause_at(page, t):
    """Plays the current question's first clip, then stops it t seconds into the clip."""
    hit(page, ".q-play")
    playing(page)
    page.evaluate(f"audio.pause(); audio.currentTime = {t}")
    page.wait_for_function(f"() => audio.paused && Math.abs(audio.currentTime - {t}) < 0.05")
    page.wait_for_timeout(150)


@pytest.mark.parametrize("n,tap", [(1, True), (2, True), (4, False), (5, False)])
def test_it_starts_now_is_enabled_only_for_tap_questions_with_a_clip_in_play(browser_, new_library, n, tap):
    base, _, _ = new_library
    page = open_page(browser_, "phone")
    open_round(page, base)
    go(page, n)
    assert page.locator("#qNow").is_visible() and not page.locator("#qNow").is_enabled()      # always in its place, dimmed until it applies
    hit(page, ".q-play")
    playing(page)
    assert page.locator("#qNow").is_enabled() == tap
    page.evaluate("audio.pause()")                      # paused inside the clip
    page.wait_for_timeout(150)
    assert page.locator("#qNow").is_enabled() == tap
    if tap:
        page.evaluate("audio.currentTime = audio.duration - 0.2; audio.play()")
        page.wait_for_function("() => audio.ended")     # at the end of the clip it is gone again
        page.wait_for_timeout(150)
        assert not page.locator("#qNow").is_enabled()


@pytest.mark.parametrize("size,how", [("phone", "tap"), ("desktop", "tap"), ("desktop", "enter")])
@pytest.mark.parametrize("t,bar", [(0.7, 10), (1.4, 11), (5.2, 13), (7.9, 14)])
def test_it_starts_now_answers_the_bar_line_nearest_the_playhead_and_that_button_is_lit(browser_, new_library, size, how, t, bar):
    base, root, _ = new_library
    page = open_page(browser_, size)
    open_round(page, base)
    pause_at(page, t)
    page.locator(f".q-barbtn.now[data-v='bar:{bar}']").wait_for()
    assert page.locator(".q-barbtn.now").count() == 1
    assert page.locator("#qNow kbd").inner_text() == "Enter" if size == "desktop" else True
    hit(page, "#qNow") if how == "tap" else press(page, "Enter")
    assert wait_for(lambda: answers(root) == {"r01-p001": f"bar:{bar}"})


@pytest.mark.parametrize("size", TOUCH)
@pytest.mark.parametrize("n", [1, 2])
def test_it_starts_now_sits_in_the_thumb_zone_above_the_other_answers(browser_, new_library, size, n):
    base, _, _ = new_library
    page = open_page(browser_, size)
    open_round(page, base)
    go(page, n)
    pause_at(page, 2.0)
    now, rest, height = page.locator("#qNow").bounding_box(), page.locator(".q-answers").bounding_box(), page.viewport_size["height"]
    assert now["height"] >= 56 and now["y"] + now["height"] <= rest["y"] and now["y"] >= height - 340
    assert page.evaluate("document.documentElement.scrollHeight <= innerHeight")
    assert page.evaluate(SMALL_TARGETS, 44) == []
    overflow = page.evaluate(OVERFLOW)
    assert overflow["scrollWidth"] <= overflow["width"] and not overflow["bad"]


@pytest.mark.parametrize("how", ["button", "key", "now"])
def test_a_bar_answer_shows_the_pick_on_the_strip_before_moving_on(browser_, new_library, how):
    base, root, _ = new_library
    page = open_page(browser_, "desktop")
    open_round(page, base)
    pause_at(page, 5.9)                                  # the line nearest is at 26 s, the fourth, bar 13
    pick = {"button": lambda: hit(page, ".q-barbtn >> nth=3"), "key": lambda: press(page, "4"), "now": lambda: press(page, "Enter")}[how]
    t0 = time.time()
    pick()
    page.locator(".q-barbtn.on").wait_for()
    assert pixel_at(page, ".q-wave", 26.0, "mid") == colour(page, "--good")
    assert page.locator(".quiz").get_attribute("data-probe") == "r01-p001"
    page.locator(".quiz[data-probe='r01-p002']").wait_for()
    assert time.time() - t0 >= 0.4
    assert wait_for(lambda: answers(root) == {"r01-p001": "bar:13"})


def test_the_report_names_each_theory_by_the_text_it_carries_else_by_the_current_rounds_text_else_by_its_id(browser_, round_library, monkeypatch):
    base, root = answered_round(round_library)
    real = rounds.report
    monkeypatch.setattr(rounds, "report", lambda *a: {**real(*a), "theories": {"old": {"n": 2, "confirmed": 0.5, "text": "Text of an earlier round"},
                                                                               "t1": {"n": 3, "confirmed": 1.0}, "zzz": {"n": 1, "confirmed": 0.0}}})
    page = open_page(browser_, "desktop")
    page.goto(base + "/")
    page.locator("#pin").fill(PIN)
    page.locator("#loginForm button").click()
    page.locator("#list .row").first.wait_for()
    page.locator("#btnSummary").click()
    page.locator(".report .panel").first.wait_for()
    rows = page.locator(".report .panel", has_text="Theories").locator("tr td:first-child").all_inner_texts()
    assert rows == ["Text of an earlier round", "Drops are found one bar late", "zzz"]


@pytest.mark.parametrize("view", ["round", "list", "summary"])
def test_the_header_title_is_never_cut_at_360px(browser_, round_library, view):
    base, _, _ = round_library
    page = browser_.new_context(viewport={"width": 360, "height": 740}, device_scale_factor=3, has_touch=True, is_mobile=True).new_page()
    page.set_default_timeout(8000)
    open_round(page, base)
    if view != "round":
        hit(page, "#btnSongs", wait=False)
        page.wait_for_function("() => document.body.dataset.view === 'list'")
    if view == "summary":
        page.locator("#btnSummary").tap()
        page.locator(".report").wait_for()
    cut = page.evaluate("(() => { const h = document.querySelector('#heading'); return [h.textContent, h.scrollWidth, h.clientWidth]; })()")
    assert cut[1] <= cut[2], cut
    overflow = page.evaluate(OVERFLOW)
    assert overflow["scrollWidth"] <= overflow["width"] and not overflow["bad"]
    assert page.evaluate(SMALL_TARGETS, 44) == []


# --- a layout that does not move: fixed zones in a fixed order, whatever the question and its state ---------------

def stable_round(ids):
    main = lambda a, b, mark, **extra: [{"id": "main", "start": float(a), "end": float(b), "mark": mark, **extra}]
    return make_round(ids, [
        probe(1, ids[0], "yesno", "Does the drop start at the marker?", main(20, 28, 24.0)),                         # 5 bars
        probe(2, ids[1], "locate", "Where does the new part start?", main(8, 40, None)),                              # 17 bars
        probe(3, ids[0], "locate", "Where does the new part start?", main(20, 24, None)),                             # 3 bars
        probe(4, ids[1], "label", "Is this part a drop?", main(40, 48, 44.0)),
        probe(5, ids[0], "label", "Are the clicks on the beat, with the accent on bar 1?", main(20, 28, 20.0, click_shift=0)),
        probe(6, ids[1], "ab", "Which one has the accent on bar 1?", [{"id": "a", "start": 20.0, "end": 28.0, "mark": 20.0, "click_shift": 0},
                                                                    {"id": "b", "start": 20.0, "end": 28.0, "mark": 20.0, "click_shift": 1}]),
        probe(7, ids[0], "yesno", "Does the buildup start at the marker?", main(8, 16, 12.0), tap=False),
        probe(8, ids[1], "locate", "Tap the bar where the buildup starts", main(0, 28, None), none_label="No buildup", before=12,
              claim={"kind": "buildup", "bar": 8, "time": 16.0, "confidence": 40}),                                   # 14 bars, taps only before bar 12
        probe(9, ids[0], "yesno", "Does the drop start at the marker?", main(8, 40, 24.0)),                          # 16 bars
    ])


@pytest.fixture
def stable_library(library, contract):
    return put_round(library, stable_round(library[2]))


ZONES = ["header.top", ".q-theory", ".q-title", ".q-prompt", ".q-define", ".q-cards", ".q-barzone", "#qOpts", ".q-nav", "#qNow", "#qAnswers",
         "#optReplay", "#optAutoplay", "#optLoop", "#optCue", "#qBack", "#qSkip", "#qCount"]
LAYOUT = """(zones) => {
  const box = el => { const r = el.getBoundingClientRect(); return [r.x, r.y, r.width, r.height].map(v => Math.round(v * 10) / 10); };
  const all = s => [...document.querySelectorAll(s)].map(box);
  return { zones: Object.fromEntries(zones.map(s => [s, document.querySelector(s) ? box(document.querySelector(s)) : null])),
           plays: all(".q-play"), waves: all(".q-wave"), bars: all(".q-barbtn"),
           answers: Object.fromEntries([...document.querySelectorAll("#qAnswers button[data-v]")].map(b => [b.dataset.v, box(b)])),
           order: [...document.querySelector(".quiz").children].map(c => c.id || c.className.split(" ")[0]),
           bar_order: [...document.querySelector(".q-bar").children].map(c => c.id || c.className.split(" ")[0]),
           fits: document.documentElement.scrollHeight <= innerHeight };
}"""


def layout(page):
    return page.evaluate(LAYOUT, ZONES)


def stable_states(page, n, kind):
    """(state, layout) for question n through every state it can be in."""
    go(page, n)
    yield "before play", layout(page)
    hit(page, ".q-play")
    yield "loading or playing", layout(page)
    playing(page)
    for i in range(3):
        page.wait_for_timeout(150)
        yield f"playing {i}", layout(page)
    page.evaluate("audio.pause()")
    page.wait_for_timeout(100)
    yield "paused", layout(page)
    page.wait_for_function("() => isFinite(audio.duration)")
    page.evaluate("audio.currentTime = audio.duration - 0.2; audio.play()")
    page.wait_for_function("() => audio.ended")
    page.wait_for_timeout(100)
    yield "clip ended", layout(page)
    hit(page, "#optLoop", wait=False)
    hit(page, "#optAutoplay", wait=False)
    if page.locator("#optCue").is_enabled():
        hit(page, "#optCue", wait=False)
    page.wait_for_timeout(100)
    yield "toggles changed", layout(page)
    for sel in ("#optLoop", "#optAutoplay", "#optCue"):         # and back, so the next question starts as this one did
        if page.locator(sel).is_enabled():
            hit(page, sel, wait=False)
    if n == 4:
        answer(page, "no")
        page.locator(".q-what").wait_for()
        yield "what", layout(page)
    elif kind == "tap":
        hit(page, ".q-barbtn >> nth=1")
        page.wait_for_timeout(120)
        yield "bar picked", layout(page)


@pytest.mark.parametrize("size", list(VIEWPORTS))
def test_nothing_moves_between_question_types_or_states(browser_, stable_library, size):
    base, _, _ = stable_library
    page = open_page(browser_, size)
    open_round(page, base)
    kinds = {1: "tap", 2: "tap", 3: "tap", 4: "label", 5: "label", 6: "ab", 7: "plain", 8: "tap", 9: "tap"}
    samples = [(n, state, lay) for n in kinds for state, lay in stable_states(page, n, kinds[n])]
    first = samples[0][2]
    assert all(box for box in first["zones"].values()), first["zones"]
    for n, state, lay in samples:
        assert lay["zones"] == first["zones"], (n, state, lay["zones"], first["zones"])         # every zone, every control that is always there
        assert lay["order"] == first["order"] and lay["bar_order"] == first["bar_order"], (n, state)   # nothing inserted or removed
        if size in TOUCH:
            assert lay["fits"], (n, state)
    for n in kinds:
        mine = [lay for m, _, lay in samples if m == n]
        for lay in mine:
            assert lay["plays"] == mine[0]["plays"] and lay["waves"] == mine[0]["waves"] and lay["bars"] == mine[0]["bars"], n
            if state_has_answers := lay["answers"]:
                pass
        steady = [lay["answers"] for (m, state, lay) in samples if m == n and state != "what"]
        assert all(a == steady[0] for a in steady), n


@pytest.mark.parametrize("size", list(VIEWPORTS))
def test_the_same_answer_is_in_the_same_place_on_every_question_type_and_bars_do_not_move_with_their_count(browser_, stable_library, size):
    base, _, _ = stable_library
    page = open_page(browser_, size)
    open_round(page, base)
    seen = {}
    for n in range(1, 10):
        go(page, n)
        lay = layout(page)
        seen[n] = lay
    skip = {tuple(seen[n]["answers"]["skip"]) for n in seen}
    assert len(skip) == 1, skip                                                                    # Can't tell is always there
    assert seen[1]["answers"]["yes"] == seen[4]["answers"]["yes"] == seen[6]["answers"]["a"] == seen[9]["answers"]["yes"]   # first and second answer cells
    assert seen[4]["answers"]["no"] == seen[6]["answers"]["b"]
    assert seen[6]["answers"]["neither"] == seen[2]["answers"]["none"] == seen[1]["answers"]["none"] == seen[8]["answers"]["none"] == seen[9]["answers"]["none"]   # the third cell
    assert [len(seen[n]["bars"]) for n in (1, 2, 3, 7, 8, 9)] == [5, 17, 3, 0, 15, 17]
    for k in range(3):
        assert seen[1]["bars"][k] == seen[2]["bars"][k] == seen[3]["bars"][k] == seen[8]["bars"][k] == seen[9]["bars"][k]   # the first bars sit where they always do
    for n in (2, 9):
        zone = seen[n]["zones"][".q-barzone"]
        assert seen[n]["bars"][16][1] + seen[n]["bars"][16][3] <= zone[1] + zone[3] + 0.5, n         # 17 bars fit the zone


@pytest.mark.parametrize("n,now,cue", [(1, False, True), (2, False, False), (4, False, True), (6, False, False), (7, False, True)])
def test_controls_that_do_not_apply_are_dimmed_not_hidden(browser_, stable_library, n, now, cue):
    base, _, _ = stable_library
    page = open_page(browser_, "phone")
    open_round(page, base)
    go(page, n)
    for sel in ("#optReplay", "#optAutoplay", "#optLoop", "#optCue", "#qNow", "#qBack", "#qSkip"):
        assert page.locator(sel).is_visible(), sel
    assert page.locator("#qNow").is_enabled() == now and page.locator("#optCue").is_enabled() == cue
    for sel, enabled in (("#qNow", now), ("#optCue", cue)):
        assert (float(page.evaluate(f"getComputedStyle(document.querySelector('{sel}')).opacity")) < 1) == (not enabled)


@pytest.mark.parametrize("size", list(VIEWPORTS))
def test_the_audio_error_and_save_messages_take_no_room(browser_, stable_library, size):
    base, _, _ = stable_library
    page = open_page(browser_, size)
    open_round(page, base)
    before = layout(page)
    page.evaluate("note('Not saved: the connection was lost')")
    assert layout(page) == before
    assert "Not saved" in page.locator("#qCount").inner_text()


# --- a buildup question: only bars before the drop can be tapped, and "no buildup" is the third answer ---------------

BEFORE = 6   # the drop is at bar 6 of a clip that starts at bar 0


def buildup_round(ids):
    return make_round(ids, [probe(1, ids[0], "locate", "Tap the bar where the buildup starts", [{"id": "main", "start": 0.0, "end": 28.0, "mark": None}],
                                  none_label="No buildup", before=BEFORE, claim={"kind": "buildup", "bar": 4, "time": 8.0, "confidence": 40}),
                                probe(2, ids[1], "yesno", "Does the drop start at the marker?", [{"id": "main", "start": 8.0, "end": 40.0, "mark": 24.0}])])


@pytest.fixture
def buildup_library(library, contract):
    return put_round(library, buildup_round(library[2]))


@pytest.mark.parametrize("size", list(VIEWPORTS))
def test_bars_at_or_after_the_limit_are_dimmed_and_cannot_be_pressed(browser_, buildup_library, size):
    base, _, _ = buildup_library
    page = open_page(browser_, size)
    open_round(page, base)
    bars = page.evaluate("[...document.querySelectorAll('.q-barbtn')].map(b => [+b.dataset.v.slice(4), b.disabled, +getComputedStyle(b).opacity])")
    assert [b[0] for b in bars] == list(range(15))
    assert all(b[1] == (b[0] >= BEFORE) and (b[2] < 1) == (b[0] >= BEFORE) for b in bars), bars
    go(page, 2)                                                       # no limit on a question that has none
    assert page.locator(".q-barbtn:disabled").count() == 0


@pytest.mark.parametrize("how,bar,answered", [("button", 5, True), ("button", 6, False), ("button", 9, False),
                                              ("key", 6, True), ("key", 7, False), ("key", 9, False)])
def test_a_bar_is_picked_by_button_or_key_only_before_the_limit(browser_, buildup_library, how, bar, answered):
    base, root, _ = buildup_library
    page = open_page(browser_, "desktop")
    open_round(page, base)
    settle(page)
    if how == "button":
        page.locator(f".q-barbtn[data-v='bar:{bar}']").click(force=True)
    else:
        press(page, str(bar), wait=False)                             # the digit n is the nth bar button, so bar n - 1
    page.wait_for_timeout(900)
    assert (wait_for(lambda: answers(root)) if answered else answers(root)) == ({"r01-p001": f"bar:{bar if how == 'button' else bar - 1}"} if answered else {})


@pytest.mark.parametrize("t,bar", [(7.9, 4), (9.6, 5), (12.4, None), (13.9, None), (20.0, None)])
def test_it_starts_now_is_dimmed_when_the_nearest_bar_line_cannot_be_tapped(browser_, buildup_library, t, bar):
    base, root, _ = buildup_library
    page = open_page(browser_, "phone")
    open_round(page, base)
    pause_at(page, t)
    assert page.locator("#qNow").is_enabled() == (bar is not None)
    assert (page.locator(".q-barbtn.now").count() == 1) == (bar is not None)
    if bar is not None:
        hit(page, "#qNow")
        assert wait_for(lambda: answers(root) == {"r01-p001": f"bar:{bar}"})


def test_no_buildup_is_the_third_answer_and_is_recorded_as_none(browser_, buildup_library):
    base, root, _ = buildup_library
    page = open_page(browser_, "phone")
    open_round(page, base)
    button = page.locator(".q-answers button[data-v='none']")
    assert button.inner_text() == "No buildup" and button.evaluate("el => el.parentElement.className") == "a3"
    hit(page, ".q-answers button[data-v='none']")
    assert wait_for(lambda: answers(root) == {"r01-p001": "none"})


@pytest.mark.parametrize("n", [1, 2])
def test_the_strip_shades_the_bars_that_cannot_be_tapped(browser_, buildup_library, n):
    base, _, _ = buildup_library
    page = open_page(browser_, "desktop")
    open_round(page, base)
    go(page, n)
    page.wait_for_timeout(250)                                         # the strip is drawn after its first resize
    plain = pixel_at(page, ".q-wave", 8.75, "bottom")
    assert pixel_at(page, ".q-wave", 10.25, "bottom") == plain
    assert (pixel_at(page, ".q-wave", 14.25, "bottom") != plain) == (n == 1)                  # shaded from bar 6, which starts at 12 s


@pytest.mark.parametrize("n", [1, 2])
def test_a_buildup_question_and_a_16_bar_question_fit_at_360px(browser_, buildup_library, n):
    base, _, _ = buildup_library
    page = browser_.new_context(viewport={"width": 360, "height": 740}, device_scale_factor=3, has_touch=True, is_mobile=True).new_page()
    page.set_default_timeout(8000)
    open_round(page, base)
    go(page, n)
    overflow = page.evaluate(OVERFLOW)
    assert overflow["scrollWidth"] <= overflow["width"] and not overflow["bad"]
    assert page.evaluate(SMALL_TARGETS, 44) == []
    assert page.evaluate("[...document.querySelectorAll('#qAnswers button')].every(b => b.scrollWidth <= b.clientWidth)")     # no text cut
    bars = page.evaluate("[...document.querySelectorAll('.q-barbtn')].map(b => { const r = b.getBoundingClientRect(), z = document.querySelector('.q-barzone').getBoundingClientRect(); return r.bottom <= z.bottom + 0.5 })")
    assert len(bars) == [15, 17][n - 1] and all(bars)


# --- the Theory behind a question is never shown on the question screen -----------------------------------------------

@pytest.fixture
def theory_library(library, contract):
    rnd = stable_round(library[2])
    rnd["theories"] = [{"id": "zq-theory-id", "text": "ZQ theory text"}]
    for p in rnd["probes"]:
        p["theory"] = "zq-theory-id"
    return put_round(library, rnd)


@pytest.mark.parametrize("size", ["phone", "desktop"])
@pytest.mark.parametrize("n", range(1, 10))
def test_no_theory_text_or_id_is_in_the_question_view_in_any_state(browser_, theory_library, size, n):
    base, _, _ = theory_library
    page = open_page(browser_, size)
    open_round(page, base)
    html = lambda: page.evaluate("document.documentElement.outerHTML + document.body.innerText")
    go(page, n)
    states = [html()]
    hit(page, ".q-play")
    playing(page)
    states.append(html())
    if n == 4:
        answer(page, "no")
        page.locator(".q-what").wait_for()
        states.append(html())
    for state in states:
        assert "ZQ theory text" not in state and "zq-theory-id" not in state
    assert page.locator(".q-theory").inner_text() == "" and page.locator(".q-theory").is_visible()
