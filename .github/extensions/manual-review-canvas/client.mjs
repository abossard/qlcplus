// Iframe renderer for the manual-review canvas.
// Returns a single self-contained HTML document (inline CSS + JS). The page
// talks to the extension's loopback API (/api/*) and subscribes to /events
// (SSE) so agent-driven changes appear live. Styling uses app canvas theme
// tokens with safe fallbacks.
//
// NOTE: the embedded <script> deliberately avoids backticks and ${...} so it
// can live inside this outer template literal without escaping headaches.

export function renderHtml() {
    return `<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8" />
<meta name="viewport" content="width=device-width, initial-scale=1.0" />
<title>Manual Review</title>
<style>
:root {
  --bg: var(--background-color-default, #0d1117);
  --fg: var(--text-color-default, #e6edf3);
  --muted: var(--text-color-muted, #8b949e);
  --border: var(--border-color-default, #30363d);
  --accent: var(--true-color-blue, #2f81f7);
  --pass: var(--true-color-green, #3fb950);
  --fail: var(--true-color-red, #f85149);
  --skip: var(--true-color-yellow, #d29922);
  --panel: var(--background-color-muted, rgba(255,255,255,0.03));
  --focus: var(--color-focus-outline, #2f81f7);
  --z-scrim: 40; --z-toc: 50; --z-help: 100;
}
* { box-sizing: border-box; }
html, body { height: 100%; margin: 0; }
body {
  background: var(--bg);
  color: var(--fg);
  font-family: var(--font-sans, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif);
  font-size: var(--text-body-medium, 14px);
  line-height: var(--leading-body-medium, 20px);
  display: flex;
  flex-direction: column;
}
code, kbd {
  font-family: var(--font-mono, "SFMono-Regular", Consolas, monospace);
  font-size: 12px;
}
kbd {
  background: var(--panel);
  border: 1px solid var(--border);
  border-radius: 4px;
  padding: 1px 5px;
}
/* Start screen */
#start { flex: 1; display: flex; align-items: center; justify-content: center; padding: 2rem; }
#start .card { text-align: center; max-width: 460px; }
#start h1 { font-size: 22px; font-weight: 600; }
#start p { color: var(--muted); }
.row { display: flex; gap: .5rem; align-items: center; justify-content: center; flex-wrap: wrap; }
input[type=text] {
  background: var(--panel); color: var(--fg);
  border: 1px solid var(--border); border-radius: 6px; padding: 6px 10px; font: inherit;
}
input[type=text]:focus { outline: 2px solid var(--focus); outline-offset: -1px; }
button {
  background: var(--panel); color: var(--fg);
  border: 1px solid var(--border); border-radius: 6px;
  padding: 6px 12px; font: inherit; cursor: pointer;
}
button:hover { border-color: var(--accent); }
button.primary { background: var(--accent); border-color: var(--accent); color: #fff; font-weight: 600; }
/* App shell — fills the viewport so progress/toolbar pin and content scrolls */
#app { flex: 1; min-height: 0; display: flex; flex-direction: column; }
/* Progress */
#progress { position: relative; height: 26px; border-bottom: 1px solid var(--border); background: var(--panel); flex: 0 0 auto; }
#bar { position: absolute; left: 0; top: 0; bottom: 0; width: 0; background: var(--pass); opacity: .25; transition: width .2s ease; }
#bar.fail { background: var(--fail); }
#ptext { position: relative; line-height: 26px; padding: 0 10px; font-size: 12px; color: var(--muted); white-space: nowrap; }
/* Layout — full-width content with a sticky TOC bar on top */
#layout { flex: 1; display: flex; flex-direction: column; min-height: 0; }
#content { flex: 1; min-height: 0; overflow-y: auto; padding: 12px 16px 24px; }

/* Sticky TOC bar: always visible, shows current section, toggles the drawer */
#tocbar {
  flex: 0 0 auto; position: relative; z-index: var(--z-toc);
  display: flex; align-items: center; gap: 8px;
  padding: 6px 10px; border-bottom: 1px solid var(--border); background: var(--panel);
}
#toc-toggle {
  flex: 0 0 auto; display: inline-flex; align-items: center; gap: 6px;
  padding: 4px 10px; font-size: 12px; font-weight: 600; white-space: nowrap;
}
#toc-toggle .chev { font-size: 9px; opacity: .8; transition: transform .15s ease; }
#tocbar.open #toc-toggle .chev { transform: rotate(180deg); }
#toc-current {
  flex: 1; min-width: 0; font-size: 12px; color: var(--muted); cursor: pointer;
  white-space: nowrap; overflow: hidden; text-overflow: ellipsis;
}
/* Scrim + drawer drop from directly under the bar */
#toc-scrim {
  position: absolute; left: 0; right: 0; top: 100%; height: 100vh;
  background: rgba(0,0,0,.35); opacity: 0; pointer-events: none;
  transition: opacity .15s ease; z-index: var(--z-scrim);
}
#tocbar.open #toc-scrim { opacity: 1; pointer-events: auto; }
#tocpanel {
  position: absolute; left: 0; right: 0; top: 100%;
  max-height: min(72vh, 480px); overflow-y: auto;
  background: var(--bg); border-bottom: 1px solid var(--border);
  box-shadow: 0 14px 30px rgba(0,0,0,.38); padding: 8px;
  transform: translateY(-6px); opacity: 0; pointer-events: none;
  transition: transform .18s cubic-bezier(.22,1,.36,1), opacity .15s ease; z-index: var(--z-toc);
}
#tocbar.open #tocpanel { transform: translateY(0); opacity: 1; pointer-events: auto; }
@media (prefers-reduced-motion: reduce) {
  #tocpanel, #toc-scrim, #toc-toggle .chev { transition: none; }
}
.sb-section { margin-bottom: 6px; }
.sb-head { font-weight: 600; font-size: 12px; color: var(--muted); padding: 6px 4px 2px; cursor: pointer; text-transform: uppercase; letter-spacing: .03em; }
.sb-head:hover { color: var(--fg); }
.sb-case { font-size: 13px; padding: 5px 8px; border-radius: 6px; cursor: pointer; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
.sb-case:hover { background: var(--panel); }
.sb-case.active { background: var(--panel); color: var(--fg); box-shadow: inset 0 0 0 1px var(--border); }
.tester { display: flex; flex-direction: column; gap: 4px; padding: 4px 4px 10px; border-bottom: 1px solid var(--border); margin-bottom: 8px; }
.tester label { font-size: 11px; color: var(--muted); }
/* Sections / cases */
h2.sec { font-size: 18px; border-bottom: 1px solid var(--border); padding-bottom: 4px; margin-top: 18px; }
blockquote { margin: 6px 0; padding: 6px 10px; border-left: 3px solid var(--border); color: var(--muted); font-size: 12px; }
h3.case { font-size: 14px; margin: 14px 0 4px; }
.step { font-size: 12px; color: var(--muted); margin: 2px 0; }
.step .label { font-weight: 600; color: var(--fg); margin-right: 4px; }
/* Check item */
.check { border: 1px solid var(--border); border-radius: 8px; padding: 8px 10px; margin: 6px 0; background: var(--panel); display: grid; grid-template-columns: auto 1fr; gap: 6px 10px; align-items: start; }
.check.focused { outline: 2px solid var(--focus); outline-offset: 1px; }
.check.s-pass { border-left: 4px solid var(--pass); }
.check.s-fail { border-left: 4px solid var(--fail); }
.check.s-skip { border-left: 4px solid var(--skip); }
.ctrls { display: flex; gap: 4px; }
.ctrls button { padding: 2px 6px; font-size: 14px; line-height: 1; }
.ctrls button.on.p { background: var(--pass); border-color: var(--pass); }
.ctrls button.on.f { background: var(--fail); border-color: var(--fail); }
.ctrls button.on.s { background: var(--skip); border-color: var(--skip); color: #1a1a1a; }
.ctext { align-self: center; }
.cmeta { grid-column: 2; display: flex; gap: 6px; align-items: center; }
.cmeta input[type=text] { flex: 1; font-size: 12px; padding: 3px 8px; }
.shotbtn { cursor: pointer; border: 1px solid var(--border); border-radius: 6px; padding: 3px 8px; font-size: 13px; }
.shotbtn input { display: none; }
.paste { font-size: 11px; color: var(--muted); cursor: pointer; border: 1px dashed var(--border); border-radius: 6px; padding: 3px 6px; }
.shots { grid-column: 2; display: flex; gap: 6px; flex-wrap: wrap; }
.shot { position: relative; display: inline-flex; }
.shot img { max-height: 60px; border: 1px solid var(--border); border-radius: 6px; display: block; }
.shot .del { position: absolute; top: -7px; right: -7px; width: 18px; height: 18px; padding: 0; line-height: 16px; text-align: center; border-radius: 50%; border: 1px solid var(--border); background: var(--panel); color: var(--fg); cursor: pointer; font-size: 13px; opacity: 0; transition: opacity .12s; }
.shot:hover .del, .shot .del:focus { opacity: 1; }
.shot .del:hover { background: var(--fail); border-color: var(--fail); color: #fff; }
/* Toolbar */
#toolbar { flex: 0 0 auto; display: flex; gap: 8px; padding: 8px 12px; border-top: 1px solid var(--border); background: var(--panel); flex-wrap: wrap; align-items: center; }
#toolbar .spacer { flex: 1; }
/* Help */
#help { position: fixed; inset: 0; z-index: var(--z-help); background: rgba(0,0,0,.6); display: none; align-items: center; justify-content: center; }
#help .box { background: var(--bg); border: 1px solid var(--border); border-radius: 10px; padding: 16px 20px; max-width: 420px; }
#help table { border-collapse: collapse; }
#help td { padding: 3px 10px 3px 0; font-size: 13px; }
.hidden { display: none !important; }
.empty { color: var(--muted); font-style: italic; }
</style>
</head>
<body>
  <div id="start" class="hidden">
    <div class="card">
      <h1>📋 Manual Review</h1>
      <p>Walk the <code>MANUAL_REVIEW.md</code> checklist. Progress, notes and screenshots are saved to <code>test-reports/</code> in the repo.</p>
      <div class="row">
        <input type="text" id="start-tester" placeholder="Your name (optional)" />
        <button class="primary" id="start-btn">🚀 Start review</button>
      </div>
      <p id="start-resume" class="hidden"></p>
    </div>
  </div>

  <div id="app" class="hidden">
    <div id="progress"><div id="bar"></div><div id="ptext">Loading…</div></div>
    <div id="layout">
      <div id="tocbar">
        <button id="toc-toggle" aria-expanded="false" aria-controls="tocpanel" title="Browse sections">
          <span aria-hidden="true">☰</span> Sections <span class="chev" aria-hidden="true">▾</span>
        </button>
        <span id="toc-current" title="Jump to a section">Jump to a section…</span>
        <div id="toc-scrim"></div>
        <nav id="tocpanel" aria-label="Table of contents"></nav>
      </div>
      <main id="content"></main>
    </div>
    <div id="toolbar">
      <button id="tb-export" title="Export report (Ctrl+E)">📄 Export</button>
      <button id="tb-conclude" title="Finalize this run">✅ Conclude</button>
      <button id="tb-reset" title="Abandon active run">🔄 New run</button>
      <span class="spacer"></span>
      <span id="tb-meta" style="font-size:12px;color:var(--muted)"></span>
      <button id="tb-help" title="Help (?)">❓</button>
    </div>
  </div>

  <div id="help">
    <div class="box">
      <h2>Keyboard shortcuts</h2>
      <table>
        <tr><td><kbd>j</kbd> / <kbd>↓</kbd></td><td>Next item</td></tr>
        <tr><td><kbd>k</kbd> / <kbd>↑</kbd></td><td>Previous item</td></tr>
        <tr><td><kbd>p</kbd></td><td>Pass</td></tr>
        <tr><td><kbd>f</kbd></td><td>Fail</td></tr>
        <tr><td><kbd>s</kbd></td><td>Skip</td></tr>
        <tr><td><kbd>n</kbd></td><td>Focus note</td></tr>
        <tr><td><kbd>Ctrl+V</kbd></td><td>Paste screenshot</td></tr>
        <tr><td><kbd>Ctrl+E</kbd></td><td>Export report</td></tr>
        <tr><td><kbd>?</kbd></td><td>Toggle help</td></tr>
      </table>
      <p class="empty">Click anywhere to close.</p>
    </div>
  </div>

<script>
"use strict";
(function () {
  var plan = null, session = null, runId = null, flat = [], idx = 0;

  function $(id) { return document.getElementById(id); }
  function esc(s) { return String(s == null ? "" : s).replace(/&/g,"&amp;").replace(/</g,"&lt;").replace(/>/g,"&gt;").replace(/"/g,"&quot;"); }
  function getJSON(u) { return fetch(u).then(function (r) { return r.json(); }); }
  function postJSON(u, b) { return fetch(u, { method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify(b || {}) }).then(function (r) { return r.json(); }); }

  function stepLabel(k) {
    var m = { do: "🔧 Do:", verify: "👁 Verify:", prerequisite: "⚡ Prerequisite:", expected: "🎯 Expected:", "why-manual": "❓ Why manual:", note: "📝 Note:", text: "" };
    return m[k] || "";
  }
  function itemState(id) { return (session.items && session.items[id]) || { status: "pending", note: "", screenshots: [] }; }
  function allChecks(tc) {
    var out = [];
    tc.steps.forEach(function (s) { out = out.concat(s.checks); });
    return out.concat(tc.tableChecks);
  }
  function buildFlat() {
    flat = [];
    plan.sections.forEach(function (sec) { sec.cases.forEach(function (tc) { allChecks(tc).forEach(function (c) { flat.push(c); }); }); });
  }

  // ── boot ──
  function boot() {
    getJSON("/api/state").then(function (st) {
      plan = st.plan; session = st.session; runId = st.runId;
      buildFlat();
      if (!runId) showStart(st); else showApp();
    });
  }

  function showStart(st) {
    $("app").classList.add("hidden");
    $("start").classList.remove("hidden");
    var resume = $("start-resume");
    if (st && st.lastRun) {
      resume.classList.remove("hidden");
      resume.textContent = "Most recent run: " + st.lastRun;
    } else { resume.classList.add("hidden"); }
  }

  function showApp() {
    $("start").classList.add("hidden");
    $("app").classList.remove("hidden");
    renderSidebar(); renderContent(); updateProgress(); highlight();
    updateCurrentSection();
    $("tb-meta").textContent = "run " + (runId || "");
  }

  // ── render ──
  function renderSidebar() {
    var sb = $("tocpanel"); sb.innerHTML = "";
    var t = document.createElement("div"); t.className = "tester";
    t.innerHTML = '<label>Tester</label><input type="text" id="tester" value="' + esc(session.tester || "") + '" placeholder="Your name" />';
    sb.appendChild(t);
    t.querySelector("input").addEventListener("change", function (e) {
      session.tester = e.target.value; postJSON("/api/tester", { tester: session.tester });
    });

    plan.sections.forEach(function (sec) {
      var d = document.createElement("div"); d.className = "sb-section";
      var h = document.createElement("div"); h.className = "sb-head";
      h.textContent = sec.number + ". " + sec.title;
      h.addEventListener("click", function () { closeToc(); scrollTo(sec.id); });
      d.appendChild(h);
      sec.cases.forEach(function (tc) {
        var checks = allChecks(tc);
        var statuses = checks.map(function (c) { return itemState(c.id).status; });
        var icon = checks.length === 0 ? "📋"
          : statuses.every(function (s) { return s === "pass"; }) ? "✅"
          : statuses.some(function (s) { return s === "fail"; }) ? "❌"
          : statuses.some(function (s) { return s !== "pending"; }) ? "🔶" : "⬜";
        var c = document.createElement("div"); c.className = "sb-case"; c.dataset.target = tc.id;
        c.textContent = icon + " " + tc.number + " " + tc.title;
        c.addEventListener("click", function () {
          closeToc();
          scrollTo(tc.id);
          if (checks[0]) { idx = flat.findIndex(function (x) { return x.id === checks[0].id; }); highlight(); }
        });
        d.appendChild(c);
      });
      sb.appendChild(d);
    });
    updateCurrentSection();
  }

  // ── TOC drawer + scroll-spy ──
  function openToc() {
    $("tocbar").classList.add("open");
    $("toc-toggle").setAttribute("aria-expanded", "true");
  }
  function closeToc() {
    $("tocbar").classList.remove("open");
    $("toc-toggle").setAttribute("aria-expanded", "false");
  }
  function toggleToc() { $("tocbar").classList.contains("open") ? closeToc() : openToc(); }
  var spyScheduled = false;
  function scheduleSpy() {
    if (spyScheduled) return; spyScheduled = true;
    requestAnimationFrame(function () { spyScheduled = false; updateCurrentSection(); });
  }
  function updateCurrentSection() {
    if (!plan || !plan.sections.length) return;
    var ct = $("content"); if (!ct) return;
    var base = ct.getBoundingClientRect().top + 12;
    var curSec = null, curCaseId = null;
    plan.sections.forEach(function (sec) {
      var el = $(sec.id);
      if (el && el.getBoundingClientRect().top <= base) curSec = sec;
      sec.cases.forEach(function (tc) {
        var ce = $(tc.id);
        if (ce && ce.getBoundingClientRect().top <= base) curCaseId = tc.id;
      });
    });
    // Above the first section's threshold (e.g. scrolled to top): default to section 1.
    if (!curSec) curSec = plan.sections[0];
    if (!curCaseId && curSec.cases[0]) curCaseId = curSec.cases[0].id;
    var lbl = curSec.number + ". " + curSec.title;
    var cur = $("toc-current"); if (cur) { cur.textContent = lbl; cur.title = lbl; }
    var prev = $("tocpanel").querySelector(".sb-case.active");
    if (prev) prev.classList.remove("active");
    if (curCaseId) {
      var a = $("tocpanel").querySelector('.sb-case[data-target="' + curCaseId + '"]');
      if (a) a.classList.add("active");
    }
  }

  function renderContent() {
    var ct = $("content"); ct.innerHTML = "";
    plan.sections.forEach(function (sec) {
      var s = document.createElement("div"); s.id = sec.id;
      var h = document.createElement("h2"); h.className = "sec"; h.textContent = sec.number + ". " + sec.title; s.appendChild(h);
      if (sec.contextNote) { var bq = document.createElement("blockquote"); bq.textContent = sec.contextNote; s.appendChild(bq); }
      sec.cases.forEach(function (tc) {
        var ce = document.createElement("div"); ce.id = tc.id;
        var ch = document.createElement("h3"); ch.className = "case"; ch.textContent = tc.number + " " + tc.title; ce.appendChild(ch);
        tc.steps.forEach(function (step) {
          var lbl = stepLabel(step.kind);
          if (lbl || step.text) {
            var se = document.createElement("div"); se.className = "step";
            se.innerHTML = (lbl ? '<span class="label">' + esc(lbl) + "</span>" : "") + esc(step.text || "");
            ce.appendChild(se);
          }
          step.checks.forEach(function (c) { ce.appendChild(renderCheck(c)); });
        });
        tc.tableChecks.forEach(function (c) { ce.appendChild(renderCheck(c)); });
        s.appendChild(ce);
      });
      ct.appendChild(s);
    });
  }

  function renderCheck(check) {
    var st = itemState(check.id);
    var el = document.createElement("div");
    el.className = "check s-" + st.status; el.id = "chk-" + check.id; el.dataset.id = check.id; el.tabIndex = 0;
    var shots = (st.screenshots || []).map(function (p) {
      return '<span class="shot"><img src="/api/' + esc(p) + '" alt="' + esc(p) + '" />' +
        '<button class="del" type="button" title="Delete screenshot" data-shot="' + esc(p) + '">×</button></span>';
    }).join("");
    el.innerHTML =
      '<div class="ctrls">' +
        '<button class="p ' + (st.status === "pass" ? "on" : "") + '" data-a="pass" title="Pass (p)">✅</button>' +
        '<button class="f ' + (st.status === "fail" ? "on" : "") + '" data-a="fail" title="Fail (f)">❌</button>' +
        '<button class="s ' + (st.status === "skip" ? "on" : "") + '" data-a="skip" title="Skip (s)">⏭</button>' +
      '</div>' +
      '<div class="ctext">' + esc(check.text) + '</div>' +
      '<div class="cmeta">' +
        '<input type="text" class="note" placeholder="Add note…" value="' + esc(st.note || "") + '" />' +
        '<label class="shotbtn" title="Attach screenshot">📸<input type="file" accept="image/*" multiple /></label>' +
        '<span class="paste" title="Click then Ctrl+V">📋 paste</span>' +
      '</div>' +
      (shots ? '<div class="shots">' + shots + "</div>" : "");

    el.querySelectorAll("[data-a]").forEach(function (b) {
      b.addEventListener("click", function () { setStatus(check.id, b.dataset.a); });
    });
    var note = el.querySelector(".note");
    note.addEventListener("change", function () { saveItem(check.id, { note: note.value }); });
    var file = el.querySelector('input[type=file]');
    file.addEventListener("change", function () {
      if (!file.files) return;
      var queue = Array.prototype.slice.call(file.files);
      (function next() {
        if (!queue.length) return;
        uploadShot(check.id, queue.shift()).then(next);
      })();
    });
    el.querySelector(".paste").addEventListener("click", function (e) {
      e.stopPropagation(); idx = flat.findIndex(function (x) { return x.id === check.id; }); highlight(); el.focus();
    });
    el.querySelectorAll(".shot .del").forEach(function (b) {
      b.addEventListener("click", function (e) {
        e.stopPropagation();
        deleteShot(check.id, b.dataset.shot);
      });
    });
    el.addEventListener("paste", function (e) { handlePaste(e, check.id); });
    el.addEventListener("click", function (e) {
      var t = e.target.tagName;
      if (t === "BUTTON" || t === "INPUT" || e.target.classList.contains("paste")) return;
      idx = flat.findIndex(function (x) { return x.id === check.id; }); highlight();
    });
    return el;
  }

  function rerender(id) {
    var el = $("chk-" + id); if (!el) return;
    var focused = el.classList.contains("focused");
    var check = flat.find(function (x) { return x.id === id; }); if (!check) return;
    var nw = renderCheck(check); if (focused) nw.classList.add("focused");
    el.parentElement.replaceChild(nw, el);
  }

  // ── mutations ──
  function setStatus(id, status) {
    var cur = itemState(id);
    if (!session.items) session.items = {};
    session.items[id] = { status: status, note: cur.note || "", screenshots: cur.screenshots || [] };
    postJSON("/api/item", { checkId: id, status: status });
    rerender(id); updateProgress(); renderSidebar();
    if (status) navigate(1);
  }
  function saveItem(id, patch) {
    var cur = itemState(id);
    if (!session.items) session.items = {};
    session.items[id] = { status: cur.status, note: cur.note || "", screenshots: cur.screenshots || [] };
    if (patch.note != null) session.items[id].note = patch.note;
    postJSON("/api/item", Object.assign({ checkId: id }, patch));
  }
  function uploadShot(id, file) {
    var fd = new FormData(); fd.append("checkId", id); fd.append("file", file);
    return fetch("/api/screenshot", { method: "POST", body: fd })
      .then(function (r) { return r.json(); })
      .then(function () { return getJSON("/api/state"); })
      .then(function (st) { session = st.session; rerender(id); });
  }
  function deleteShot(id, path) {
    return postJSON("/api/screenshot/delete", { checkId: id, path: path })
      .then(function () { return getJSON("/api/state"); })
      .then(function (st) { session = st.session; rerender(id); });
  }
  function handlePaste(e, id) {
    if (!e.clipboardData || !e.clipboardData.items) return;
    var items = Array.prototype.slice.call(e.clipboardData.items);
    var imgs = items.filter(function (it) { return it.type.indexOf("image/") === 0; });
    if (!imgs.length) return;
    e.preventDefault();
    e.stopPropagation(); // prevent the document-level paste handler from also uploading (avoids duplicates)
    imgs.forEach(function (it) {
      var f = it.getAsFile(); if (!f) return;
      var ext = (it.type.split("/")[1] || "png");
      var named = new File([f], "clipboard-" + Date.now() + "." + ext, { type: f.type });
      uploadShot(id, named);
    });
  }

  // ── nav / progress ──
  function navigate(d) {
    var n = idx + d;
    if (n >= 0 && n < flat.length) { idx = n; highlight(); scrollTo("chk-" + flat[idx].id); }
  }
  function highlight() {
    document.querySelectorAll(".check.focused").forEach(function (el) { el.classList.remove("focused"); });
    if (flat.length) { var el = $("chk-" + flat[idx].id); if (el) el.classList.add("focused"); }
  }
  function scrollTo(id) { var el = $(id); if (el) el.scrollIntoView({ behavior: "smooth", block: "center" }); }

  function updateProgress() {
    var total = flat.length, pass = 0, fail = 0, skip = 0, pend = 0;
    flat.forEach(function (c) {
      var s = itemState(c.id).status;
      if (s === "pass") pass++; else if (s === "fail") fail++; else if (s === "skip") skip++; else pend++;
    });
    var pct = total ? Math.round(((total - pend) / total) * 100) : 0;
    var bar = $("bar"); bar.style.width = pct + "%"; bar.className = fail > 0 ? "fail" : "";
    $("ptext").textContent = pass + "✅ " + fail + "❌ " + skip + "⏭ " + pend + "⬜ — " + pct + "% (" + total + " items)";
  }

  // ── keyboard ──
  function onKey(e) {
    var tag = (e.target && e.target.tagName) || "";
    var typing = tag === "INPUT" || tag === "TEXTAREA";
    if (e.key === "Escape" && $("tocbar").classList.contains("open")) { e.preventDefault(); closeToc(); return; }
    if (e.key === "?" && !typing) { e.preventDefault(); toggleHelp(); return; }
    if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === "e") { e.preventDefault(); doExport(); return; }
    if (typing) return;
    if (e.key === "j" || e.key === "ArrowDown") { e.preventDefault(); navigate(1); }
    else if (e.key === "k" || e.key === "ArrowUp") { e.preventDefault(); navigate(-1); }
    else if (e.key === "p") { if (flat[idx]) setStatus(flat[idx].id, "pass"); }
    else if (e.key === "f") { if (flat[idx]) setStatus(flat[idx].id, "fail"); }
    else if (e.key === "s") { if (flat[idx]) setStatus(flat[idx].id, "skip"); }
    else if (e.key === "n") { if (flat[idx]) { var el = $("chk-" + flat[idx].id); var nt = el && el.querySelector(".note"); if (nt) { e.preventDefault(); nt.focus(); } } }
  }

  function toggleHelp() { var h = $("help"); h.style.display = h.style.display === "flex" ? "none" : "flex"; }

  // ── toolbar actions ──
  function doExport() {
    postJSON("/api/export", {}).then(function (d) {
      if (d && d.report) {
        var blob = new Blob([d.report], { type: "text/markdown" });
        var a = document.createElement("a"); a.href = URL.createObjectURL(blob); a.download = "test-report.md"; a.click();
        URL.revokeObjectURL(a.href);
      }
      flash("Report written to " + (d && d.path ? d.path : "test-reports/"));
    });
  }
  function doConclude() {
    if (!confirm("Conclude this run? It will be finalized.")) return;
    postJSON("/api/conclude", {}).then(function () { runId = null; boot(); });
  }
  function doReset() {
    if (!confirm("Abandon the active run? Files are kept in test-reports/.")) return;
    postJSON("/api/reset", {}).then(function () { runId = null; boot(); });
  }
  function flash(msg) { var m = $("tb-meta"); var prev = m.textContent; m.textContent = msg; setTimeout(function () { m.textContent = "run " + (runId || ""); }, 2500); }

  // ── live updates ──
  function connectSSE() {
    try {
      var es = new EventSource("/events");
      es.onmessage = function (ev) {
        try {
          var msg = JSON.parse(ev.data);
          if (msg.type === "session" && runId) {
            session = msg.session;
            // light refresh: update each visible check + progress + sidebar
            flat.forEach(function (c) { rerender(c.id); });
            updateProgress(); renderSidebar(); highlight();
          } else if (msg.type === "run") {
            boot();
          }
        } catch (e) {}
      };
    } catch (e) {}
  }

  // ── wire ──
  $("start-btn").addEventListener("click", function () {
    postJSON("/api/start", { tester: $("start-tester").value }).then(function (d) { runId = d.runId; boot(); });
  });
  $("tb-export").addEventListener("click", doExport);
  $("tb-conclude").addEventListener("click", doConclude);
  $("tb-reset").addEventListener("click", doReset);
  $("tb-help").addEventListener("click", toggleHelp);
  $("help").addEventListener("click", function () { $("help").style.display = "none"; });
  $("toc-toggle").addEventListener("click", toggleToc);
  $("toc-current").addEventListener("click", toggleToc);
  $("toc-scrim").addEventListener("click", closeToc);
  $("content").addEventListener("scroll", scheduleSpy, { passive: true });
  document.addEventListener("keydown", onKey);
  document.addEventListener("paste", function (e) {
    var tag = (e.target && e.target.tagName) || "";
    if (tag === "INPUT" || tag === "TEXTAREA") return;
    if (flat[idx]) handlePaste(e, flat[idx].id);
  });

  connectSSE();
  boot();
})();
</script>
</body>
</html>`;
}
