// Renderer for the qlcplus-control canvas. renderHtml() returns a fully
// self-contained HTML document (inline CSS + JS) loaded in the host iframe.
// The iframe talks to the extension's loopback HTTP server via same-origin
// fetch + an SSE /events stream. No nested template literals / ${} inside the
// inner <script> so it can sit inside this outer template literal safely.

export function renderHtml() {
    return `<!doctype html>
<html>
<head>
<meta charset="utf-8" />
<title>QLC+ Dev Control</title>
<style>
  :root { color-scheme: light dark; }
  * { box-sizing: border-box; }
  body {
    margin: 0;
    background: var(--background-color-default, #ffffff);
    color: var(--text-color-default, #1f2328);
    font-family: var(--font-sans, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif);
    font-size: var(--text-body-medium, 13px);
    line-height: var(--leading-body-medium, 20px);
    height: 100vh;
    display: flex;
    flex-direction: column;
  }
  header {
    padding: 10px 14px;
    border-bottom: 1px solid var(--border-color-default, #d0d7de);
    display: flex;
    flex-wrap: wrap;
    align-items: center;
    gap: 10px 16px;
  }
  h1 {
    margin: 0;
    font-size: var(--text-title-medium, 16px);
    font-weight: var(--font-weight-semibold, 600);
    display: flex;
    align-items: center;
    gap: 8px;
  }
  .pill {
    display: inline-flex;
    align-items: center;
    gap: 6px;
    padding: 2px 10px;
    border-radius: 999px;
    font-size: 12px;
    font-weight: 600;
    border: 1px solid var(--border-color-default, #d0d7de);
  }
  .dot { width: 8px; height: 8px; border-radius: 50%; background: var(--text-color-muted, #888); }
  .pill.running { color: var(--true-color-green, #1a7f37); border-color: var(--true-color-green-muted, #4ac26b); }
  .pill.running .dot { background: var(--true-color-green, #1a7f37); }
  .pill.building { color: var(--true-color-yellow, #9a6700); border-color: var(--true-color-yellow-muted, #d4a72c); }
  .pill.building .dot { background: var(--true-color-yellow, #9a6700); animation: pulse 1s infinite; }
  .pill.stopped { color: var(--text-color-muted, #656d76); }
  .pill.external { color: var(--true-color-blue, #0969da); border-color: var(--true-color-blue-muted, #54aeff); }
  .pill.external .dot { background: var(--true-color-blue, #0969da); }
  @keyframes pulse { 50% { opacity: 0.35; } }
  .metrics { display: flex; gap: 14px; margin-left: auto; align-items: center; flex-wrap: wrap; }
  .metric { display: flex; flex-direction: column; line-height: 1.15; }
  .metric .v { font-family: var(--font-mono, monospace); font-weight: 600; font-size: 14px; }
  .metric .k { font-size: 10px; text-transform: uppercase; letter-spacing: 0.04em; color: var(--text-color-muted, #656d76); }
  .controls { padding: 10px 14px; border-bottom: 1px solid var(--border-color-default, #d0d7de); display: flex; flex-wrap: wrap; gap: 8px 10px; align-items: center; }
  button {
    font: inherit;
    padding: 5px 12px;
    border-radius: 6px;
    border: 1px solid var(--border-color-default, #d0d7de);
    background: var(--background-color-default, #f6f8fa);
    color: var(--text-color-default, #1f2328);
    cursor: pointer;
  }
  button:hover:not(:disabled) { border-color: var(--text-color-muted, #888); }
  button:disabled { opacity: 0.4; cursor: not-allowed; }
  button.primary { background: var(--true-color-green, #1f883d); border-color: var(--true-color-green, #1f883d); color: var(--color-white, #fff); }
  button.danger { background: var(--true-color-red, #cf222e); border-color: var(--true-color-red, #cf222e); color: var(--color-white, #fff); }
  button.accent { background: var(--true-color-blue, #0969da); border-color: var(--true-color-blue, #0969da); color: var(--color-white, #fff); }
  label.opt { display: inline-flex; align-items: center; gap: 5px; font-size: 12px; user-select: none; }
  input[type=text] {
    font: inherit;
    padding: 4px 8px;
    border-radius: 6px;
    border: 1px solid var(--border-color-default, #d0d7de);
    background: var(--background-color-default, #fff);
    color: var(--text-color-default, #1f2328);
    min-width: 160px;
  }
  .sep { width: 1px; align-self: stretch; background: var(--border-color-default, #d0d7de); margin: 0 2px; }
  .tabs { display: flex; gap: 4px; padding: 8px 14px 0; align-items: center; }
  .tab { padding: 4px 12px; border-radius: 6px 6px 0 0; border: 1px solid transparent; cursor: pointer; font-size: 12px; color: var(--text-color-muted, #656d76); }
  .tab.active { color: var(--text-color-default, #1f2328); border-color: var(--border-color-default, #d0d7de); border-bottom-color: transparent; background: var(--background-color-default, #fff); font-weight: 600; }
  .tab .badge { display: inline-block; margin-left: 6px; padding: 0 6px; border-radius: 999px; font-size: 10px; background: var(--border-color-default, #eaeef2); color: var(--text-color-muted, #656d76); }
  .logwrap { flex: 1; margin: 0 14px 14px; border: 1px solid var(--border-color-default, #d0d7de); border-radius: 0 6px 6px 6px; overflow: hidden; display: flex; flex-direction: column; min-height: 120px; }
  .logtools { display: flex; gap: 12px; align-items: center; padding: 6px 10px; border-bottom: 1px solid var(--border-color-default, #d0d7de); font-size: 11px; color: var(--text-color-muted, #656d76); }
  pre.log {
    margin: 0; flex: 1; overflow: auto; padding: 8px 12px;
    font-family: var(--font-mono, "SFMono-Regular", Consolas, monospace);
    font-size: var(--text-code-inline, 12px); line-height: 1.45;
    white-space: pre-wrap; word-break: break-word;
    background: var(--n-0, var(--background-color-default, #fff));
  }
  pre.log .err { color: var(--true-color-red, #cf222e); }
  .empty { color: var(--text-color-muted, #656d76); font-style: italic; }
  .hint { font-size: 11px; color: var(--text-color-muted, #656d76); }
  .toast { position: fixed; bottom: 14px; left: 50%; transform: translateX(-50%); background: var(--true-color-red, #cf222e); color: #fff; padding: 8px 14px; border-radius: 8px; font-size: 12px; max-width: 80%; box-shadow: 0 4px 12px rgba(0,0,0,0.2); opacity: 0; transition: opacity .2s; pointer-events: none; }
  .toast.show { opacity: 1; }
</style>
</head>
<body>
<header>
  <h1>QLC+ Dev Control <span id="pill" class="pill stopped"><span class="dot"></span><span id="pillText">unknown</span></span></h1>
  <div class="metrics">
    <div class="metric"><span class="v" id="mCpu">--</span><span class="k">CPU %</span></div>
    <div class="metric"><span class="v" id="mMem">--</span><span class="k">Mem MB</span></div>
    <div class="metric"><span class="v" id="mUp">--</span><span class="k">Uptime</span></div>
    <div class="metric"><span class="v" id="mPid">--</span><span class="k">PID</span></div>
  </div>
</header>

<div class="controls">
  <button id="bRebuild" class="accent">Rebuild</button>
  <button id="bStart" class="primary">Start</button>
  <button id="bStop" class="danger">Stop</button>
  <button id="bRestart">Restart</button>
  <div class="sep"></div>
  <label class="opt"><input type="checkbox" id="optDebug" checked /> Debug (-d)</label>
  <input type="text" id="optArgs" placeholder="extra args (e.g. -o show.qxw)" />
  <span id="envHint" class="hint"></span>
</div>

<div class="tabs">
  <div class="tab active" data-tab="app">App stdout</div>
  <div class="tab" data-tab="build">Build output</div>
</div>
<div class="logwrap">
  <div class="logtools">
    <label class="opt"><input type="checkbox" id="autoscroll" checked /> Auto-scroll</label>
    <button id="bClear" style="padding:2px 8px;font-size:11px;">Clear view</button>
    <span id="logNote" class="hint" style="margin-left:auto;"></span>
  </div>
  <pre class="log" id="logApp"><span class="empty">No output yet.</span></pre>
  <pre class="log" id="logBuild" style="display:none;"><span class="empty">No build output yet.</span></pre>
</div>

<div class="toast" id="toast"></div>

<script>
(function () {
  var current = "app";
  var buffers = { app: [], build: [] };
  var hasContent = { app: false, build: false };
  var lastState = null;

  function $(id) { return document.getElementById(id); }

  function toast(msg) {
    var t = $("toast");
    t.textContent = msg;
    t.classList.add("show");
    setTimeout(function () { t.classList.remove("show"); }, 3200);
  }

  function esc(s) {
    return String(s).replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
  }

  function fmtUptime(sec) {
    if (sec == null || isNaN(sec)) return "--";
    sec = Math.floor(sec);
    var h = Math.floor(sec / 3600);
    var m = Math.floor((sec % 3600) / 60);
    var s = sec % 60;
    if (h > 0) return h + "h" + (m < 10 ? "0" : "") + m + "m";
    if (m > 0) return m + "m" + (s < 10 ? "0" : "") + s + "s";
    return s + "s";
  }

  function renderLog(which) {
    var el = which === "app" ? $("logApp") : $("logBuild");
    if (!hasContent[which]) return;
    var lines = buffers[which];
    var html = "";
    for (var i = 0; i < lines.length; i++) {
      var ln = lines[i];
      if (ln.err) html += '<span class="err">' + esc(ln.text) + "</span>\\n";
      else html += esc(ln.text) + "\\n";
    }
    el.innerHTML = html;
    if ($("autoscroll").checked) el.scrollTop = el.scrollHeight;
  }

  function pushLine(which, text, isErr) {
    if (!hasContent[which]) { buffers[which] = []; hasContent[which] = true; }
    buffers[which].push({ text: text, err: !!isErr });
    if (buffers[which].length > 5000) buffers[which].splice(0, buffers[which].length - 5000);
    if (which === current) {
      var el = which === "app" ? $("logApp") : $("logBuild");
      var span = document.createElement("span");
      if (isErr) span.className = "err";
      span.textContent = text + "\\n";
      if (el.querySelector(".empty")) el.innerHTML = "";
      el.appendChild(span);
      if ($("autoscroll").checked) el.scrollTop = el.scrollHeight;
    }
  }

  function applyState(s) {
    lastState = s;
    var pill = $("pill"), txt = $("pillText");
    pill.className = "pill ";
    var proc = s.proc || {};
    var build = s.build || {};
    if (build.state === "running") { pill.className += "building"; txt.textContent = "building"; }
    else if (proc.running && proc.managed) { pill.className += "running"; txt.textContent = "running"; }
    else if (proc.running && !proc.managed) { pill.className += "external"; txt.textContent = "running (external)"; }
    else { pill.className += "stopped"; txt.textContent = "stopped"; }

    $("mPid").textContent = proc.pid ? String(proc.pid) : "--";

    var building = build.state === "running";
    var env = s.env || {};
    $("bStart").disabled = proc.running || building;
    $("bStop").disabled = !proc.running || building;
    $("bRestart").disabled = !(proc.running && proc.managed) || building;
    $("bRebuild").disabled = building;
    $("bRebuild").textContent = building ? "Building…" : "Rebuild";

    var hint = "";
    if (!env.buildDirExists) hint = "No build dir — Rebuild will configure + build.";
    else if (!env.binaryExists) hint = "Binary not built yet — run Rebuild.";
    $("envHint").textContent = hint;

    if (proc.running && !proc.managed) {
      $("logNote").textContent = "stdout not captured (app was not started by this panel)";
    } else if (proc.running && proc.managed) {
      $("logNote").textContent = "tailing " + (proc.logFile || "log");
    } else {
      $("logNote").textContent = "";
    }
  }

  function applyMetrics(m) {
    if (!m) {
      $("mCpu").textContent = "--"; $("mMem").textContent = "--"; $("mUp").textContent = "--";
      return;
    }
    $("mCpu").textContent = (m.cpu != null ? m.cpu.toFixed(1) : "--");
    $("mMem").textContent = (m.mem != null ? m.mem.toFixed(0) : "--");
    $("mUp").textContent = fmtUptime(m.uptime);
  }

  function post(path, body) {
    return fetch(path, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(body || {})
    }).then(function (r) {
      return r.json().then(function (j) {
        if (!r.ok || (j && j.error)) throw new Error((j && j.error) || ("HTTP " + r.status));
        return j;
      });
    }).catch(function (e) { toast(e.message || String(e)); throw e; });
  }

  function startOpts() {
    var args = $("optArgs").value.trim();
    return { debug: $("optDebug").checked, extraArgs: args };
  }

  $("bStart").onclick = function () { post("/api/start", startOpts()); };
  $("bStop").onclick = function () { post("/api/stop", {}); };
  $("bRestart").onclick = function () { post("/api/restart", startOpts()); };
  $("bRebuild").onclick = function () {
    buffers.build = []; hasContent.build = false;
    $("logBuild").innerHTML = '<span class="empty">Starting build…</span>';
    switchTab("build");
    post("/api/rebuild", {});
  };
  $("bClear").onclick = function () {
    buffers[current] = []; hasContent[current] = false;
    var el = current === "app" ? $("logApp") : $("logBuild");
    el.innerHTML = '<span class="empty">Cleared.</span>';
  };

  function switchTab(which) {
    current = which;
    var tabs = document.querySelectorAll(".tab");
    for (var i = 0; i < tabs.length; i++) tabs[i].classList.toggle("active", tabs[i].getAttribute("data-tab") === which);
    $("logApp").style.display = which === "app" ? "" : "none";
    $("logBuild").style.display = which === "build" ? "" : "none";
    renderLog(which);
  }
  var tabEls = document.querySelectorAll(".tab");
  for (var i = 0; i < tabEls.length; i++) {
    tabEls[i].onclick = function () { switchTab(this.getAttribute("data-tab")); };
  }

  // Load any backlog of recent log text, then connect to live stream.
  function loadBacklog(type) {
    fetch("/api/logs?type=" + type + "&tail=200").then(function (r) { return r.json(); }).then(function (j) {
      if (j && j.lines && j.lines.length) {
        hasContent[type] = false;
        for (var k = 0; k < j.lines.length; k++) pushLine(type, j.lines[k], false);
        renderLog(type);
      }
    }).catch(function () {});
  }

  function connect() {
    var es = new EventSource("/events");
    es.onmessage = function (ev) {
      var msg;
      try { msg = JSON.parse(ev.data); } catch (e) { return; }
      if (msg.type === "state") applyState(msg.state);
      else if (msg.type === "metrics") applyMetrics(msg.metrics);
      else if (msg.type === "log") pushLine(msg.stream, msg.line, msg.err);
    };
    es.onerror = function () { /* EventSource auto-reconnects */ };
  }

  loadBacklog("app");
  loadBacklog("build");
  connect();
})();
</script>
</body>
</html>`;
}
