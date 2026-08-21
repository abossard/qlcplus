// Extension: qlcplus-control
// A Copilot CLI canvas that builds, starts, stops, and monitors the QLC+ QML
// app (`build/qmlui/qlcplus5`). It streams the app's stdout/stderr and build
// output to a log viewer and samples CPU/memory via `ps`.
//
// Design notes:
//  - The QLC+ process is a singleton, so state is global (not per canvas
//    instance). Every open canvas instance renders the same live view.
//  - The app is spawned DETACHED with stdout/stderr redirected to a log file,
//    then unref()'d. This means the app SURVIVES an `extensions_reload` (it is
//    not killed when this provider restarts). On reload we re-discover the
//    running PID from run.json and resume tailing the same log file.
//  - Durable run metadata lives under $COPILOT_HOME/extensions/qlcplus-control/
//    artifacts/ (NOT in the repo), per the canvas state-model guidance.

import { createServer } from "node:http";
import { spawn, execFileSync } from "node:child_process";
import {
    mkdirSync, openSync, closeSync, statSync, readSync, readFileSync,
    writeFileSync, existsSync, appendFileSync, unlinkSync,
} from "node:fs";
import { join, resolve, dirname } from "node:path";
import { homedir } from "node:os";
import { fileURLToPath } from "node:url";
import { joinSession, createCanvas, CanvasError } from "@github/copilot-sdk/extension";

const EXT_DIR = dirname(fileURLToPath(import.meta.url));

// ---------------------------------------------------------------------------
// Paths
// ---------------------------------------------------------------------------

function resolveRepoRoot() {
    // Extension lives at <repoRoot>/.github/extensions/qlcplus-control/.
    const candidate = resolve(EXT_DIR, "..", "..", "..");
    if (existsSync(join(candidate, "CMakeLists.txt")) && existsSync(join(candidate, "qmlui"))) {
        return candidate;
    }
    // Fallback: walk upward looking for the repo markers.
    let dir = EXT_DIR;
    for (let i = 0; i < 8; i++) {
        if (existsSync(join(dir, "CMakeLists.txt")) && existsSync(join(dir, "qmlui"))) return dir;
        const parent = dirname(dir);
        if (parent === dir) break;
        dir = parent;
    }
    return candidate;
}

const REPO_ROOT = resolveRepoRoot();
const BUILD_DIR = join(REPO_ROOT, "build");
const BINARY = join(BUILD_DIR, "qmlui", "qlcplus5");

const COPILOT_HOME = process.env.COPILOT_HOME || join(homedir(), ".copilot");
const ARTIFACTS = join(COPILOT_HOME, "extensions", "qlcplus-control", "artifacts");
const LOGS_DIR = join(ARTIFACTS, "logs");
const RUN_JSON = join(ARTIFACTS, "run.json");

function ensureDirs() {
    try { mkdirSync(LOGS_DIR, { recursive: true }); } catch (e) { /* ignore */ }
}
ensureDirs();

let sessionRef = null;
function log(msg, level) {
    try { if (sessionRef) sessionRef.log(String(msg), { level: level || "info" }); } catch (e) { /* ignore */ }
}

// ---------------------------------------------------------------------------
// Global state
// ---------------------------------------------------------------------------

// Managed run (we started it). Loaded from disk on boot so we re-attach after
// an extensions_reload. Shape: { pid, startedAt, args, debug, extraArgs, logFile }
let managed = null;

// Build state.
const build = { state: "idle", code: null, startedAt: null, finishedAt: null };
let buildChild = null;
let buildLogFile = null;

// App-log tail bookkeeping.
let appTail = null; // { file, offset, partial }

function loadRun() {
    try {
        if (!existsSync(RUN_JSON)) return null;
        const data = JSON.parse(readFileSync(RUN_JSON, "utf8"));
        if (data && data.pid && alive(data.pid)) return data;
    } catch (e) { /* ignore */ }
    return null;
}
function saveRun(rec) {
    try { writeFileSync(RUN_JSON, JSON.stringify(rec, null, 2)); } catch (e) { /* ignore */ }
}
function clearRun() {
    try { if (existsSync(RUN_JSON)) unlinkSync(RUN_JSON); } catch (e) { /* ignore */ }
    managed = null;
}

function makeRunId() {
    const d = new Date();
    const p = (n) => String(n).padStart(2, "0");
    return d.getFullYear() + "-" + p(d.getMonth() + 1) + "-" + p(d.getDate()) +
        "-" + p(d.getHours()) + p(d.getMinutes()) + p(d.getSeconds());
}

// ---------------------------------------------------------------------------
// Process helpers
// ---------------------------------------------------------------------------

function alive(pid) {
    if (!pid) return false;
    try { process.kill(pid, 0); return true; } catch (e) { return e.code === "EPERM"; }
}

// Find a qlcplus5 app process we did NOT start (e.g. user launched it, or it
// predates this extension). Excludes compiler/build invocations that mention
// the binary name.
function findExternalPid() {
    try {
        const out = execFileSync("pgrep", ["-f", "qmlui/qlcplus5"], { encoding: "utf8", timeout: 3000 }).trim();
        if (!out) return null;
        const pids = out.split(/\s+/).map((s) => parseInt(s, 10)).filter(Boolean);
        for (const pid of pids) {
            if (pid === process.pid) continue;
            let cmd = "";
            try { cmd = execFileSync("ps", ["-p", String(pid), "-o", "command="], { encoding: "utf8", timeout: 2000 }).trim(); } catch (e) { continue; }
            if (!cmd.includes("/qmlui/qlcplus5")) continue;
            if (/cmake|ninja|clang|\bld\b|c\+\+|\bcc\b|\bmake\b/.test(cmd)) continue;
            return pid;
        }
    } catch (e) { /* pgrep exits 1 when no match */ }
    return null;
}

function currentPid() {
    if (managed && alive(managed.pid)) return managed.pid;
    if (managed && !alive(managed.pid)) clearRun();
    return findExternalPid();
}

function parseEtime(s) {
    // ps etime: [[DD-]HH:]MM:SS
    if (!s) return null;
    s = s.trim();
    let days = 0;
    if (s.includes("-")) { const parts = s.split("-"); days = parseInt(parts[0], 10); s = parts[1]; }
    const seg = s.split(":").map((x) => parseInt(x, 10));
    let sec = 0;
    if (seg.length === 3) sec = seg[0] * 3600 + seg[1] * 60 + seg[2];
    else if (seg.length === 2) sec = seg[0] * 60 + seg[1];
    else sec = seg[0] || 0;
    return days * 86400 + sec;
}

function sampleMetrics() {
    const pid = currentPid();
    if (!pid) return null;
    try {
        const out = execFileSync("ps", ["-p", String(pid), "-o", "%cpu=,%mem=,rss=,etime="], { encoding: "utf8", timeout: 2500 }).trim();
        if (!out) return null;
        const m = out.split(/\s+/);
        const cpu = parseFloat(m[0]);
        const memPct = parseFloat(m[1]);
        const rssKb = parseFloat(m[2]);
        const etime = m[3];
        let uptime;
        if (managed && managed.pid === pid && managed.startedAt) uptime = (Date.now() - managed.startedAt) / 1000;
        else uptime = parseEtime(etime);
        return { cpu, memPct, mem: rssKb / 1024, uptime, pid };
    } catch (e) { return null; }
}

// ---------------------------------------------------------------------------
// SSE
// ---------------------------------------------------------------------------

const sseClients = new Set();

function broadcast(obj) {
    const payload = "data: " + JSON.stringify(obj) + "\n\n";
    for (const res of sseClients) {
        try { res.write(payload); } catch (e) { /* ignore */ }
    }
}

function envInfo() {
    return {
        buildDirExists: existsSync(BUILD_DIR),
        configured: existsSync(join(BUILD_DIR, "CMakeCache.txt")),
        binaryExists: existsSync(BINARY),
    };
}

function procSnapshot() {
    if (managed && alive(managed.pid)) {
        return {
            running: true, managed: true, pid: managed.pid,
            startedAt: managed.startedAt, args: managed.args || [],
            debug: !!managed.debug, extraArgs: managed.extraArgs || "",
            logFile: managed.logFile || null,
        };
    }
    if (managed && !alive(managed.pid)) clearRun();
    const ext = findExternalPid();
    if (ext) return { running: true, managed: false, pid: ext };
    return { running: false, managed: false, pid: null };
}

function snapshot() {
    return {
        build: { state: build.state, code: build.code, startedAt: build.startedAt, finishedAt: build.finishedAt },
        proc: procSnapshot(),
        env: envInfo(),
        repoRoot: REPO_ROOT,
    };
}

function broadcastState() { broadcast({ type: "state", state: snapshot() }); }

// ---------------------------------------------------------------------------
// App log tailing
// ---------------------------------------------------------------------------

function attachAppTail(file) {
    let offset = 0;
    try { offset = statSync(file).size; } catch (e) { offset = 0; }
    appTail = { file, offset, partial: "" };
}

function pollAppLog() {
    if (!appTail) return;
    if (!(managed && alive(managed.pid))) return;
    try {
        const sz = statSync(appTail.file).size;
        if (sz < appTail.offset) { appTail.offset = 0; appTail.partial = ""; }
        if (sz > appTail.offset) {
            const len = sz - appTail.offset;
            const fd = openSync(appTail.file, "r");
            const buf = Buffer.alloc(len);
            readSync(fd, buf, 0, len, appTail.offset);
            closeSync(fd);
            appTail.offset = sz;
            const text = appTail.partial + buf.toString("utf8");
            const parts = text.split("\n");
            appTail.partial = parts.pop();
            for (const line of parts) broadcast({ type: "log", stream: "app", line });
        }
    } catch (e) { /* ignore */ }
}

function readTail(file, n) {
    try {
        if (!file || !existsSync(file)) return [];
        const sz = statSync(file).size;
        const cap = 256 * 1024;
        const start = sz > cap ? sz - cap : 0;
        const len = sz - start;
        const fd = openSync(file, "r");
        const buf = Buffer.alloc(len);
        readSync(fd, buf, 0, len, start);
        closeSync(fd);
        const lines = buf.toString("utf8").split("\n");
        if (lines.length && lines[lines.length - 1] === "") lines.pop();
        return n ? lines.slice(-n) : lines;
    } catch (e) { return []; }
}

// ---------------------------------------------------------------------------
// Build
// ---------------------------------------------------------------------------

function appendBuild(line) {
    try { if (buildLogFile) appendFileSync(buildLogFile, line + "\n"); } catch (e) { /* ignore */ }
}

function runStep(cmd, args, cwd) {
    return new Promise((resolveStep) => {
        broadcast({ type: "log", stream: "build", line: "$ " + cmd + " " + args.join(" ") });
        appendBuild("$ " + cmd + " " + args.join(" "));
        let child;
        try {
            child = spawn(cmd, args, { cwd, env: process.env });
        } catch (e) {
            broadcast({ type: "log", stream: "build", line: "spawn failed: " + e.message, err: true });
            resolveStep(127);
            return;
        }
        buildChild = child;
        let outPartial = "", errPartial = "";
        const onData = (chunk, isErr) => {
            const acc = (isErr ? errPartial : outPartial) + chunk.toString("utf8");
            const parts = acc.split("\n");
            const tail = parts.pop();
            if (isErr) errPartial = tail; else outPartial = tail;
            for (const line of parts) {
                broadcast({ type: "log", stream: "build", line, err: isErr });
                appendBuild(line);
            }
        };
        child.stdout.on("data", (c) => onData(c, false));
        child.stderr.on("data", (c) => onData(c, true));
        child.on("error", (e) => {
            broadcast({ type: "log", stream: "build", line: "error: " + e.message, err: true });
            appendBuild("error: " + e.message);
        });
        child.on("close", (code) => {
            if (outPartial) { broadcast({ type: "log", stream: "build", line: outPartial }); appendBuild(outPartial); }
            if (errPartial) { broadcast({ type: "log", stream: "build", line: errPartial, err: true }); appendBuild(errPartial); }
            buildChild = null;
            resolveStep(code == null ? 1 : code);
        });
    });
}

async function startBuild() {
    if (build.state === "running") throw new CanvasError("build_in_progress", "A build is already running.");
    build.state = "running";
    build.code = null;
    build.startedAt = Date.now();
    build.finishedAt = null;
    buildLogFile = join(LOGS_DIR, "build-" + makeRunId() + ".log");
    try { writeFileSync(buildLogFile, ""); } catch (e) { /* ignore */ }
    broadcastState();
    broadcast({ type: "log", stream: "build", line: "=== Build started " + new Date().toISOString() + " ===" });

    (async () => {
        let code = 0;
        try {
            if (!existsSync(BUILD_DIR)) {
                try { mkdirSync(BUILD_DIR, { recursive: true }); } catch (e) { /* ignore */ }
            }
            if (!existsSync(join(BUILD_DIR, "CMakeCache.txt"))) {
                broadcast({ type: "log", stream: "build", line: "Configuring (cmake .. -Dqmlui=ON)…" });
                code = await runStep("cmake", ["..", "-Dqmlui=ON"], BUILD_DIR);
            }
            if (code === 0) {
                code = await runStep("cmake", ["--build", ".", "--target", "qlcplus5", "-j8"], BUILD_DIR);
            }
        } catch (e) {
            broadcast({ type: "log", stream: "build", line: "exception: " + e.message, err: true });
            code = 1;
        }
        build.state = code === 0 ? "success" : "failed";
        build.code = code;
        build.finishedAt = Date.now();
        broadcast({ type: "log", stream: "build", line: "=== Build " + build.state + " (exit " + code + ") ===" });
        broadcastState();
        log("qlcplus build " + build.state + " (exit " + code + ")", code === 0 ? "info" : "warn");
    })();

    return { started: true, logFile: buildLogFile };
}

function cancelBuild() {
    if (build.state !== "running" || !buildChild) return { cancelled: false };
    try { buildChild.kill("SIGTERM"); } catch (e) { /* ignore */ }
    return { cancelled: true };
}

// ---------------------------------------------------------------------------
// Start / stop
// ---------------------------------------------------------------------------

function parseExtraArgs(s) {
    if (!s) return [];
    return String(s).trim().split(/\s+/).filter(Boolean);
}

function startApp({ debug, extraArgs }) {
    const snap = procSnapshot();
    if (snap.running) throw new CanvasError("already_running", "QLC+ is already running (pid " + snap.pid + ").");
    if (!existsSync(BINARY)) throw new CanvasError("not_built", "Binary not found at build/qmlui/qlcplus5 — run Rebuild first.");

    const args = [];
    if (debug) args.push("-d");
    for (const a of parseExtraArgs(extraArgs)) args.push(a);

    const logFile = join(LOGS_DIR, "run-" + makeRunId() + ".log");
    let fd;
    try {
        fd = openSync(logFile, "a");
    } catch (e) {
        throw new CanvasError("log_open_failed", "Could not open log file: " + e.message);
    }

    let child;
    try {
        child = spawn(BINARY, args, {
            cwd: BUILD_DIR,
            detached: true,
            stdio: ["ignore", fd, fd],
            env: process.env,
        });
    } catch (e) {
        try { closeSync(fd); } catch (e2) { /* ignore */ }
        throw new CanvasError("spawn_failed", "Failed to start QLC+: " + e.message);
    }
    child.unref();
    try { closeSync(fd); } catch (e) { /* ignore */ }

    managed = {
        pid: child.pid,
        startedAt: Date.now(),
        args,
        debug: !!debug,
        extraArgs: extraArgs || "",
        logFile,
    };
    saveRun(managed);
    attachAppTail(logFile);
    appTail.offset = 0; // fresh file: stream from the very beginning
    appTail.partial = "";
    broadcast({ type: "log", stream: "app", line: "=== Started pid " + child.pid + " : qlcplus5 " + args.join(" ") + " ===" });
    broadcastState();
    log("Started QLC+ (pid " + child.pid + ") args: " + args.join(" "));
    return { pid: child.pid, args, logFile };
}

function stopApp() {
    const snap = procSnapshot();
    if (!snap.running) throw new CanvasError("not_running", "QLC+ is not running.");
    const pid = snap.pid;
    try {
        process.kill(pid, "SIGTERM");
    } catch (e) {
        throw new CanvasError("kill_failed", "Failed to stop pid " + pid + ": " + e.message);
    }
    const wasManaged = snap.managed;
    if (wasManaged) clearRun();
    appTail = null;
    broadcast({ type: "log", stream: "app", line: "=== Stopped pid " + pid + " ===" });
    broadcastState();
    log("Stopped QLC+ (pid " + pid + ")");
    return { stopped: true, pid, managed: wasManaged };
}

async function restartApp(opts) {
    const snap = procSnapshot();
    const prev = managed ? { debug: managed.debug, extraArgs: managed.extraArgs } : {};
    if (snap.running) {
        stopApp();
        // brief grace period for the OS to release the port / single-instance lock
        await new Promise((r) => setTimeout(r, 700));
    }
    const debug = opts && opts.debug != null ? opts.debug : (prev.debug != null ? prev.debug : true);
    const extraArgs = opts && opts.extraArgs != null ? opts.extraArgs : (prev.extraArgs || "");
    return startApp({ debug, extraArgs });
}

// ---------------------------------------------------------------------------
// HTTP server (one per canvas instance; all share global state)
// ---------------------------------------------------------------------------

let renderHtml = () => "<!doctype html><title>loading</title>";
import("./client.mjs").then((m) => { renderHtml = m.renderHtml; }).catch((e) => log("client load failed: " + e.message, "error"));

const servers = new Map();

function readBody(req) {
    return new Promise((res) => {
        let data = "";
        req.on("data", (c) => { data += c; if (data.length > 1e6) req.destroy(); });
        req.on("end", () => res(data));
        req.on("error", () => res(data));
    });
}

function json(res, code, obj) {
    const body = JSON.stringify(obj);
    res.writeHead(code, { "Content-Type": "application/json; charset=utf-8" });
    res.end(body);
}

async function handle(req, res) {
    const url = new URL(req.url, "http://127.0.0.1");
    const path = url.pathname;

    if (path === "/" || path === "/index.html") {
        res.writeHead(200, { "Content-Type": "text/html; charset=utf-8" });
        res.end(renderHtml());
        return;
    }

    if (path === "/events") {
        res.writeHead(200, {
            "Content-Type": "text/event-stream",
            "Cache-Control": "no-cache",
            "Connection": "keep-alive",
        });
        res.write("retry: 3000\n\n");
        res.write("data: " + JSON.stringify({ type: "state", state: snapshot() }) + "\n\n");
        const metrics = sampleMetrics();
        res.write("data: " + JSON.stringify({ type: "metrics", metrics }) + "\n\n");
        sseClients.add(res);
        req.on("close", () => sseClients.delete(res));
        return;
    }

    if (path === "/api/state" && req.method === "GET") {
        json(res, 200, { state: snapshot(), metrics: sampleMetrics() });
        return;
    }

    if (path === "/api/logs" && req.method === "GET") {
        const type = url.searchParams.get("type") === "build" ? "build" : "app";
        const n = parseInt(url.searchParams.get("tail") || "200", 10) || 200;
        const file = type === "build" ? buildLogFile : (managed && managed.logFile);
        json(res, 200, { type, lines: readTail(file, n) });
        return;
    }

    if (req.method === "POST") {
        let body = {};
        try { const raw = await readBody(req); body = raw ? JSON.parse(raw) : {}; } catch (e) { body = {}; }
        try {
            if (path === "/api/start") { json(res, 200, startApp({ debug: !!body.debug, extraArgs: body.extraArgs || "" })); return; }
            if (path === "/api/stop") { json(res, 200, stopApp()); return; }
            if (path === "/api/restart") { json(res, 200, await restartApp(body)); return; }
            if (path === "/api/rebuild") { json(res, 200, await startBuild()); return; }
            if (path === "/api/build/cancel") { json(res, 200, cancelBuild()); return; }
        } catch (e) {
            json(res, 400, { error: e.message || String(e) });
            return;
        }
    }

    json(res, 404, { error: "not found" });
}

async function startServer() {
    const server = createServer((req, res) => { handle(req, res).catch((e) => { try { json(res, 500, { error: e.message }); } catch (e2) { /* ignore */ } }); });
    await new Promise((r) => server.listen(0, "127.0.0.1", r));
    const port = server.address().port;
    return { server, url: "http://127.0.0.1:" + port + "/" };
}

// ---------------------------------------------------------------------------
// Background pollers
// ---------------------------------------------------------------------------

setInterval(pollAppLog, 600);

let lastProcRunning = null;
setInterval(() => {
    if (sseClients.size === 0) return;
    const metrics = sampleMetrics();
    broadcast({ type: "metrics", metrics });
    const running = !!(metrics);
    if (running !== lastProcRunning) { lastProcRunning = running; broadcastState(); }
}, 2000);

// ---------------------------------------------------------------------------
// Canvas + session
// ---------------------------------------------------------------------------

function buildCanvas() {
    return createCanvas({
        id: "qlcplus-control",
        displayName: "QLC+ Dev Control",
        description: "Build, start/stop, and monitor the QLC+ app — live stdout log viewer plus CPU/memory usage.",
        actions: [
            {
                name: "status",
                description: "Read-only: current process status (running/stopped/building), pid, CPU/memory, and build state.",
                handler: async () => ({ state: snapshot(), metrics: sampleMetrics() }),
            },
            {
                name: "start",
                description: "Start the QLC+ app. Optionally enable the -d debug flag and pass extra CLI args.",
                inputSchema: {
                    type: "object",
                    properties: {
                        debug: { type: "boolean", description: "Pass the -d debug flag (default true)." },
                        extraArgs: { type: "string", description: "Extra CLI args, space-separated (e.g. '-o show.qxw')." },
                    },
                    additionalProperties: false,
                },
                handler: async (ctx) => {
                    const debug = ctx.input && ctx.input.debug != null ? ctx.input.debug : true;
                    return startApp({ debug, extraArgs: (ctx.input && ctx.input.extraArgs) || "" });
                },
            },
            {
                name: "stop",
                description: "Stop the running QLC+ app (SIGTERM). Works for both panel-started and externally-started instances.",
                handler: async () => stopApp(),
            },
            {
                name: "restart",
                description: "Stop (if running) then start the QLC+ app, reusing the previous flags unless overridden.",
                inputSchema: {
                    type: "object",
                    properties: {
                        debug: { type: "boolean" },
                        extraArgs: { type: "string" },
                    },
                    additionalProperties: false,
                },
                handler: async (ctx) => restartApp(ctx.input || {}),
            },
            {
                name: "rebuild",
                description: "Rebuild qlcplus5 (configures the build dir first if needed). Returns immediately; build runs async — poll 'status' or read 'tail_log' type=build.",
                handler: async () => startBuild(),
            },
            {
                name: "tail_log",
                description: "Read-only: recent lines from the app stdout log or the build log.",
                inputSchema: {
                    type: "object",
                    properties: {
                        type: { type: "string", description: "'app' (default) or 'build'." },
                        lines: { type: "number", description: "How many trailing lines (default 100)." },
                    },
                    additionalProperties: false,
                },
                handler: async (ctx) => {
                    const type = ctx.input && ctx.input.type === "build" ? "build" : "app";
                    const n = (ctx.input && ctx.input.lines) || 100;
                    const file = type === "build" ? buildLogFile : (managed && managed.logFile);
                    return { type, lines: readTail(file, n) };
                },
            },
        ],
        open: async (ctx) => {
            let entry = servers.get(ctx.instanceId);
            if (!entry) {
                entry = await startServer();
                servers.set(ctx.instanceId, entry);
            }
            const snap = procSnapshot();
            const status = snap.running
                ? (snap.managed ? "running (pid " + snap.pid + ")" : "running — external (pid " + snap.pid + ")")
                : (build.state === "running" ? "building" : "stopped");
            return { title: "QLC+ Dev Control", url: entry.url, status };
        },
        onClose: async (ctx) => {
            const entry = servers.get(ctx.instanceId);
            if (entry) {
                servers.delete(ctx.instanceId);
                await new Promise((r) => entry.server.close(() => r()));
            }
        },
    });
}

// Re-attach to a surviving managed run after a reload.
managed = loadRun();
if (managed) attachAppTail(managed.logFile);

const session = await joinSession({ canvases: [buildCanvas()] });
sessionRef = session;
log("qlcplus-control ready (repo: " + REPO_ROOT + ")");
