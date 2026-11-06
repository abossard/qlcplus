"""The review page in real browsers at phone, tablet and desktop size (Playwright: Chromium, and WebKit for iOS Safari)."""

import json
import shutil
import subprocess
import threading
import time

import pytest

from edm_analysis.review import Auth, ReviewServer, make_handler
from edm_analysis.rounds import record_answer

PIN = "135790"
BARS, PERIOD = 32, 0.5
DURATION = BARS * 4 * PERIOD
SECTIONS = [("intro", 0, 8), ("buildup", 8, 12), ("drop", 12, 24), ("outro", 24, 32)]
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
        "sections": [{"label": label, "start_bar": a, "end_bar": b} for label, a, b in SECTIONS],
        "events": [{"kind": "buildup", "bar": 8}, {"kind": "drop", "bar": 12}, {"kind": "vocal_in", "bar": 4}, {"kind": "vocal_out", "bar": 20}],
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


def make_round(ids):
    claim = {"kind": "drop", "bar": 12, "time": 24.0, "confidence": 40}
    probe = lambda n, tid, question, prompt, plays: {"id": f"r01-p00{n}", "track": tid, "title": f"Song {n}", "theory": "t1",
                                                      "bucket": "unsure", "question": question, "prompt": prompt, "claim": claim, "plays": plays}
    return {"round": 1, "created": "2026-11-05T10:00:00", "theories": [{"id": "t1", "text": "Drops are found one bar late"}], "probes": [
        probe(1, ids[0], "yesno", "Does the drop start at the marker?", [{"id": "main", "start": 20.0, "end": 28.0, "mark": 24.0}]),
        probe(2, ids[1], "ab", "Which one starts on the drop?", [{"id": "a", "start": 4.0, "end": 12.0, "mark": 8.0},
                                                                  {"id": "b", "start": 20.0, "end": 28.0, "mark": 24.0, "click_shift": 1}]),
        probe(3, ids[0], "label", "Is this a breakdown?", [{"id": "main", "start": 40.0, "end": 48.0, "mark": 44.0}]),
    ]}


@pytest.fixture
def round_library(library):
    base, root, ids = library
    (root / "rounds").mkdir()
    (root / "rounds" / "round-01.json").write_text(json.dumps(make_round(ids)))
    return base, root, ids


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


ANSWERS = {"r01-p001": "earlier", "r01-p002": "b", "r01-p003": "no"}


def answers(root):
    path = root / "rounds" / "answers.json"
    return {k: v["answer"] for k, v in json.loads(path.read_text()).items()} if path.exists() else {}


def open_round(page, base):
    page.goto(base + "/")
    page.locator("#pin").fill(PIN)
    page.locator("#loginForm button").click()
    page.locator(".quiz").wait_for()


def answer(page, value):
    """Taps an answer once the screen has been up long enough not to count as a double tap."""
    page.wait_for_function("() => performance.now() - quizShown > 400")
    page.locator(f".q-answers button[data-v='{value}']").tap()


def test_a_round_is_answered_end_to_end_on_a_phone(browser_, round_library):
    base, root, _ = round_library
    page = open_page(browser_, "phone")
    open_round(page, base)
    assert page.locator("#heading").inner_text() == "Round 1 · 0/3"
    assert page.locator(".q-theory").inner_text() == "Drops are found one bar late"
    for n, (probe_id, value) in enumerate(ANSWERS.items()):
        page.locator(f".quiz[data-probe='{probe_id}']").wait_for()
        assert page.evaluate("audio.paused")
        page.locator(".q-play").first.tap()
        page.wait_for_function("() => !audio.paused && audio.currentTime >= excerptStart")
        page.locator(".q-play", has_text="again").first.wait_for()
        answer(page, value)
        assert wait_for(lambda: answers(root).get(probe_id) == value)
    page.locator(".q-done").wait_for()
    assert page.locator("#heading").inner_text() == "Round 1 · 3/3"

    page.locator("#qReview").tap()
    page.locator(".quiz[data-probe='r01-p001'] .q-answers button.on[data-v='earlier']").wait_for()
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


def test_ab_plays_each_excerpt_with_its_own_audio(browser_, round_library):
    base, _, ids = round_library
    page = open_page(browser_, "phone")
    open_round(page, base)
    page.locator("#qSkip").tap()
    page.locator(".quiz[data-probe='r01-p002']").wait_for()
    assert page.locator(".q-play").all_inner_texts() == ["▶ Play A", "▶ Play B"]
    page.locator(".q-play").nth(0).tap()
    page.wait_for_function("() => !audio.paused && excerptStart === 4 && audio.currentTime >= 4 && audio.currentTime <= 12")
    assert page.evaluate("audio.src").endswith(f"/audio/{ids[1]}")
    page.locator(".q-play").nth(1).tap()
    assert page.locator(".q-play").nth(1).inner_text() == "Loading B…"  # the click mix is rendered on first use
    page.wait_for_function("() => !audio.paused && excerptStart === 20 && audio.currentTime >= 20 && audio.currentTime <= 28", timeout=30000)
    assert page.evaluate("audio.src").endswith(f"/audio/{ids[1]}?click=1&shift=1")
    page.locator(".q-play", has_text="Play B again").wait_for()
    assert page.locator(".q-play").all_inner_texts() == ["↻ Play A again", "↻ Play B again"]


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
