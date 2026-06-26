// Extension: manual-review-canvas
//
// A Copilot canvas that drives the repo's MANUAL_REVIEW.md checklist — a
// drop-in replacement for the Vite+Express runner in tools/manual-review/.
//
// - Parses MANUAL_REVIEW.md (sections / cases / steps / ☐ checks / table rows).
// - Renders an interactive iframe (pass/fail/skip, notes, screenshots, keyboard).
// - Persists runs to <repoRoot>/test-reports/<runId>/{session.json,report.md,screenshots/}
//   — same format/convention as the old tool, so reports stay compatible.
// - Active run is tracked in test-reports/.active-run (never keyed by instanceId),
//   so reopening the panel rehydrates the same run.
// - Exposes agent actions (summary, list_items, set_status, start, conclude,
//   get_report) that operate on the active run.
// - Live-syncs every open iframe via SSE when the agent mutates the run.

import { createServer } from "node:http";
import { readFileSync, writeFileSync, mkdirSync, existsSync, readdirSync, statSync, unlinkSync } from "node:fs";
import { resolve, dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { joinSession, createCanvas, CanvasError } from "@github/copilot-sdk/extension";
import { renderHtml } from "./client.mjs";
import {
    parseManualReview, allCheckItems, flatChecksWithContext,
    computeStats, verdict, generateReport,
} from "./parser.mjs";

const session = await joinSession({
    canvases: [createCanvas(buildCanvas())],
});

function log(msg, level) {
    try { session.log(msg, { level: level || "info" }); } catch { /* ignore */ }
}

// ── repo + storage paths ─────────────────────────────────────────────────────

let REPO_ROOT = null;

// This file lives at <repoRoot>/.github/extensions/manual-review-canvas/extension.mjs
const EXT_DIR = dirname(fileURLToPath(import.meta.url));

function hasManualReview(dir) {
    return existsSync(join(dir, "MANUAL_REVIEW.md"));
}

function walkUpFor(start) {
    let dir = start;
    for (let i = 0; i < 10 && dir; i++) {
        if (hasManualReview(dir)) return dir;
        const parent = dirname(dir);
        if (parent === dir) break;
        dir = parent;
    }
    return null;
}

function resolveRepoRoot(ctx) {
    if (REPO_ROOT) return REPO_ROOT;
    // Primary: derive from this file's location (.github/extensions/<name>/ → repo root).
    const fromExt = resolve(EXT_DIR, "..", "..", "..");
    const candidates = [
        fromExt,
        ctx && ctx.session && ctx.session.workingDirectory,
        session && session.workspacePath,
        process.cwd(),
    ].filter(Boolean);
    for (const c of candidates) {
        if (hasManualReview(c)) { REPO_ROOT = c; return REPO_ROOT; }
    }
    for (const c of candidates) {
        const up = walkUpFor(c);
        if (up) { REPO_ROOT = up; return REPO_ROOT; }
    }
    // Last resort: the derived extension-relative root (so writes stay in-repo).
    REPO_ROOT = fromExt;
    return REPO_ROOT;
}

function reportsDir() { return resolve(resolveRepoRoot(), "test-reports"); }
function activePointer() { return resolve(reportsDir(), ".active-run"); }
function runDir(runId) { return resolve(reportsDir(), runId); }
function manualReviewPath() { return resolve(resolveRepoRoot(), "MANUAL_REVIEW.md"); }

// ── plan + session io ────────────────────────────────────────────────────────

function loadPlan() {
    let md = "# No MANUAL_REVIEW.md found\n";
    try { md = readFileSync(manualReviewPath(), "utf-8"); }
    catch { log("MANUAL_REVIEW.md not found at " + manualReviewPath(), "warn"); }
    return parseManualReview(md);
}

function getActiveRunId() {
    try {
        if (existsSync(activePointer())) {
            const id = readFileSync(activePointer(), "utf-8").trim();
            if (id && existsSync(runDir(id))) return id;
        }
    } catch { /* ignore */ }
    return null;
}

function setActiveRunId(runId) {
    mkdirSync(reportsDir(), { recursive: true });
    writeFileSync(activePointer(), runId);
}

function clearActiveRunId() {
    try { if (existsSync(activePointer())) writeFileSync(activePointer(), ""); } catch { /* ignore */ }
}

function makeRunId() {
    const d = new Date();
    const p = (n) => String(n).padStart(2, "0");
    return `${d.getFullYear()}-${p(d.getMonth() + 1)}-${p(d.getDate())}-${p(d.getHours())}${p(d.getMinutes())}${p(d.getSeconds())}`;
}

function emptySession() {
    const now = new Date().toISOString();
    return { sourceFile: "MANUAL_REVIEW.md", startedAt: now, updatedAt: now, tester: "", items: {} };
}

function readSession(runId) {
    try {
        return JSON.parse(readFileSync(resolve(runDir(runId), "session.json"), "utf-8"));
    } catch { return emptySession(); }
}

function writeSession(runId, sess) {
    sess.updatedAt = new Date().toISOString();
    const dir = runDir(runId);
    mkdirSync(resolve(dir, "screenshots"), { recursive: true });
    writeFileSync(resolve(dir, "session.json"), JSON.stringify(sess, null, 2));
    writeFileSync(resolve(dir, "report.md"), generateReport(loadPlan(), sess));
}

function startRun(tester) {
    const runId = makeRunId();
    const sess = emptySession();
    if (tester) sess.tester = tester;
    writeSession(runId, sess);
    setActiveRunId(runId);
    return runId;
}

function mostRecentRun() {
    try {
        const entries = readdirSync(reportsDir())
            .filter((e) => !e.startsWith("."))
            .map((e) => ({ e, full: resolve(reportsDir(), e) }))
            .filter((x) => { try { return statSync(x.full).isDirectory(); } catch { return false; } });
        entries.sort((a, b) => a.e < b.e ? 1 : -1);
        return entries.length ? entries[0].e : null;
    } catch { return null; }
}

function checkIdExists(plan, checkId) {
    return allCheckItems(plan).some((c) => c.id === checkId);
}

function mutateItem(sess, checkId, patch) {
    if (!sess.items[checkId]) sess.items[checkId] = { status: "pending", note: "", screenshots: [] };
    const it = sess.items[checkId];
    if (patch.status != null) it.status = patch.status;
    if (patch.note != null) it.note = patch.note;
    if (patch.screenshot != null) it.screenshots.push(patch.screenshot);
    return it;
}

// ── SSE broadcast (shared across all open iframes) ───────────────────────────

const sseClients = new Set();

function broadcast(payload) {
    const data = "data: " + JSON.stringify(payload) + "\n\n";
    for (const res of sseClients) { try { res.write(data); } catch { /* ignore */ } }
}

function broadcastSession() {
    const runId = getActiveRunId();
    if (runId) broadcast({ type: "session", session: readSession(runId) });
}

// ── minimal multipart parser (screenshot upload) ─────────────────────────────

function readBody(req) {
    return new Promise((res, rej) => {
        const chunks = [];
        req.on("data", (c) => chunks.push(c));
        req.on("end", () => res(Buffer.concat(chunks)));
        req.on("error", rej);
    });
}

function parseMultipart(buf, contentType) {
    const m = contentType.match(/boundary=(?:"([^"]+)"|([^\s;]+))/);
    if (!m) return { fields: {}, files: [] };
    const boundary = Buffer.from("--" + (m[1] || m[2]));
    const fields = {}; const files = [];
    let start = buf.indexOf(boundary);
    while (start !== -1) {
        start += boundary.length;
        if (buf[start] === 0x0d && buf[start + 1] === 0x0a) start += 2;
        if (buf[start] === 0x2d && buf[start + 1] === 0x2d) break;
        const next = buf.indexOf(boundary, start);
        if (next === -1) break;
        const part = buf.subarray(start, next);
        const headerEnd = part.indexOf("\r\n\r\n");
        if (headerEnd === -1) { start = next; continue; }
        const headerStr = part.subarray(0, headerEnd).toString("utf-8");
        let body = part.subarray(headerEnd + 4);
        if (body.length >= 2 && body[body.length - 2] === 0x0d && body[body.length - 1] === 0x0a) {
            body = body.subarray(0, body.length - 2);
        }
        const name = headerStr.match(/name="([^"]+)"/);
        const filename = headerStr.match(/filename="([^"]+)"/);
        const mime = headerStr.match(/Content-Type:\s*(\S+)/i);
        if (name && filename) files.push({ filename: filename[1], mimetype: (mime && mime[1]) || "application/octet-stream", data: body });
        else if (name) fields[name[1]] = body.toString("utf-8");
        start = next;
    }
    return { fields, files };
}

// ── HTTP handler (one server per open canvas instance) ───────────────────────

function jsonRes(res, status, data) {
    res.writeHead(status, { "Content-Type": "application/json" });
    res.end(JSON.stringify(data));
}

async function handle(req, res) {
    const url = (req.url || "").split("?")[0];
    const method = req.method || "GET";

    if (method === "GET" && (url === "/" || url === "/index.html")) {
        res.writeHead(200, { "Content-Type": "text/html; charset=utf-8" });
        res.end(renderHtml());
        return;
    }

    if (method === "GET" && url === "/api/state") {
        const plan = loadPlan();
        const runId = getActiveRunId();
        jsonRes(res, 200, {
            plan,
            runId,
            session: runId ? readSession(runId) : emptySession(),
            lastRun: mostRecentRun(),
        });
        return;
    }

    if (method === "GET" && url === "/events") {
        res.writeHead(200, {
            "Content-Type": "text/event-stream",
            "Cache-Control": "no-cache",
            Connection: "keep-alive",
        });
        res.write("retry: 3000\n\n");
        sseClients.add(res);
        req.on("close", () => sseClients.delete(res));
        return;
    }

    if (method === "GET" && url.startsWith("/api/screenshots/")) {
        const runId = getActiveRunId();
        const name = decodeURIComponent(url.replace("/api/screenshots/", ""));
        if (!runId || name.includes("..")) { res.writeHead(404); res.end(); return; }
        const file = resolve(runDir(runId), "screenshots", name);
        if (!existsSync(file)) { res.writeHead(404); res.end(); return; }
        const ext = (name.split(".").pop() || "").toLowerCase();
        const mimes = { png: "image/png", jpg: "image/jpeg", jpeg: "image/jpeg", gif: "image/gif", webp: "image/webp" };
        res.writeHead(200, { "Content-Type": mimes[ext] || "application/octet-stream" });
        res.end(readFileSync(file));
        return;
    }

    if (method === "POST" && url === "/api/start") {
        const body = JSON.parse((await readBody(req)).toString("utf-8") || "{}");
        const runId = startRun(body.tester || "");
        broadcast({ type: "run" });
        jsonRes(res, 200, { runId, session: readSession(runId) });
        return;
    }

    if (method === "POST" && url === "/api/tester") {
        const runId = getActiveRunId();
        if (!runId) { jsonRes(res, 409, { error: "no active run" }); return; }
        const body = JSON.parse((await readBody(req)).toString("utf-8") || "{}");
        const sess = readSession(runId);
        sess.tester = body.tester || "";
        writeSession(runId, sess);
        broadcastSession();
        jsonRes(res, 200, { ok: true });
        return;
    }

    if (method === "POST" && url === "/api/item") {
        const runId = getActiveRunId();
        if (!runId) { jsonRes(res, 409, { error: "no active run" }); return; }
        const body = JSON.parse((await readBody(req)).toString("utf-8") || "{}");
        if (!body.checkId) { jsonRes(res, 400, { error: "checkId required" }); return; }
        const sess = readSession(runId);
        const it = mutateItem(sess, body.checkId, { status: body.status, note: body.note });
        writeSession(runId, sess);
        broadcastSession();
        jsonRes(res, 200, { ok: true, item: it });
        return;
    }

    if (method === "POST" && url === "/api/screenshot") {
        const runId = getActiveRunId();
        if (!runId) { jsonRes(res, 409, { error: "no active run" }); return; }
        const buf = await readBody(req);
        const { fields, files } = parseMultipart(buf, req.headers["content-type"] || "");
        const checkId = fields.checkId || fields.checkid;
        if (!checkId || files.length === 0) { jsonRes(res, 400, { error: "checkId and file required" }); return; }
        const ssDir = resolve(runDir(runId), "screenshots");
        mkdirSync(ssDir, { recursive: true });
        const sess = readSession(runId);
        let item = null;
        for (const f of files) {
            const ext = (f.filename.split(".").pop() || "png");
            const fname = `${checkId}-${Date.now()}.${ext}`;
            writeFileSync(resolve(ssDir, fname), f.data);
            item = mutateItem(sess, checkId, { screenshot: `screenshots/${fname}` });
        }
        writeSession(runId, sess);
        broadcastSession();
        jsonRes(res, 200, { ok: true, item });
        return;
    }

    if (method === "POST" && url === "/api/screenshot/delete") {
        const runId = getActiveRunId();
        if (!runId) { jsonRes(res, 409, { error: "no active run" }); return; }
        const body = JSON.parse((await readBody(req)).toString("utf-8") || "{}");
        const checkId = body.checkId;
        const path = body.path;
        if (!checkId || !path) { jsonRes(res, 400, { error: "checkId and path required" }); return; }
        if (path.includes("..")) { jsonRes(res, 400, { error: "invalid path" }); return; }
        const sess = readSession(runId);
        const it = sess.items[checkId];
        let removed = false;
        if (it && Array.isArray(it.screenshots)) {
            const i = it.screenshots.indexOf(path);
            if (i !== -1) { it.screenshots.splice(i, 1); removed = true; }
        }
        // Best-effort unlink, constrained to the run's screenshots dir.
        try {
            const ssDir = resolve(runDir(runId), "screenshots");
            const file = resolve(runDir(runId), path);
            if (file.startsWith(ssDir + "/") && existsSync(file)) unlinkSync(file);
        } catch { /* ignore */ }
        writeSession(runId, sess);
        broadcastSession();
        jsonRes(res, 200, { ok: true, removed, item: it || null });
        return;
    }

    if (method === "POST" && url === "/api/export") {
        const runId = getActiveRunId();
        if (!runId) { jsonRes(res, 409, { error: "no active run" }); return; }
        const sess = readSession(runId);
        const report = generateReport(loadPlan(), sess);
        writeFileSync(resolve(runDir(runId), "report.md"), report);
        jsonRes(res, 200, { ok: true, report, path: `test-reports/${runId}/report.md` });
        return;
    }

    if (method === "POST" && url === "/api/conclude") {
        const runId = getActiveRunId();
        if (!runId) { jsonRes(res, 409, { error: "no active run" }); return; }
        const sess = readSession(runId);
        sess.concludedAt = new Date().toISOString();
        writeSession(runId, sess);
        clearActiveRunId();
        broadcast({ type: "run" });
        jsonRes(res, 200, { ok: true, path: `test-reports/${runId}/report.md` });
        return;
    }

    if (method === "POST" && url === "/api/reset") {
        clearActiveRunId();
        broadcast({ type: "run" });
        jsonRes(res, 200, { ok: true });
        return;
    }

    res.writeHead(404, { "Content-Type": "text/plain" });
    res.end("not found");
}

// ── per-instance loopback server ─────────────────────────────────────────────

const servers = new Map();

async function startServer() {
    const server = createServer((req, res) => {
        handle(req, res).catch((e) => {
            try { jsonRes(res, 500, { error: String(e) }); } catch { /* ignore */ }
        });
    });
    await new Promise((r) => server.listen(0, "127.0.0.1", r));
    const addr = server.address();
    const port = typeof addr === "object" && addr ? addr.port : 0;
    return { server, url: `http://127.0.0.1:${port}/` };
}

// ── canvas declaration + agent actions ───────────────────────────────────────

function requireActivePlanSession() {
    const runId = getActiveRunId();
    if (!runId) throw new CanvasError("no_active_run", "No active review run. Call the 'start' action first.");
    return { runId, plan: loadPlan(), sess: readSession(runId) };
}

function buildCanvas() {
    return {
        id: "manual-review",
        displayName: "Manual Review",
        description: "Run and track the repo's MANUAL_REVIEW.md checklist: pass/fail/skip items, notes, screenshots, and a saved report.",
        inputSchema: {
            type: "object",
            properties: {
                runId: { type: "string", description: "Optional existing run id to resume." },
            },
            additionalProperties: false,
        },
        actions: [
            {
                name: "summary",
                description: "Read-only stats for the active review run (pass/fail/skip/pending counts, verdict, report path).",
                handler: () => {
                    resolveRepoRoot();
                    const runId = getActiveRunId();
                    if (!runId) return { activeRun: false, hint: "No active run. Use 'start' to begin." };
                    const plan = loadPlan();
                    const sess = readSession(runId);
                    const stats = computeStats(plan, sess);
                    return {
                        activeRun: true, runId, tester: sess.tester || null,
                        ...stats, verdict: verdict(stats),
                        reportPath: `test-reports/${runId}/report.md`,
                    };
                },
            },
            {
                name: "list_items",
                description: "Read-only list of checklist items with their current status and note. Optionally filter by status.",
                inputSchema: {
                    type: "object",
                    properties: {
                        status: { type: "string", description: "Filter: pass | fail | skip | pending" },
                    },
                    additionalProperties: false,
                },
                handler: (ctx) => {
                    resolveRepoRoot();
                    const plan = loadPlan();
                    const runId = getActiveRunId();
                    const sess = runId ? readSession(runId) : emptySession();
                    const filter = ctx.input && ctx.input.status;
                    const items = flatChecksWithContext(plan).map((c) => {
                        const s = sess.items[c.id] || { status: "pending", note: "", screenshots: [] };
                        return { ...c, status: s.status, note: s.note || "", screenshots: s.screenshots || [] };
                    }).filter((c) => !filter || c.status === filter);
                    return { runId: runId || null, count: items.length, items };
                },
            },
            {
                name: "set_status",
                description: "Set a checklist item's status (pass/fail/skip/pending) and optionally a note. Requires an active run.",
                inputSchema: {
                    type: "object",
                    properties: {
                        checkId: { type: "string", description: "Check item id (see list_items)." },
                        status: { type: "string", description: "pass | fail | skip | pending" },
                        note: { type: "string", description: "Optional note." },
                    },
                    required: ["checkId"],
                    additionalProperties: false,
                },
                handler: (ctx) => {
                    const { runId, plan, sess } = requireActivePlanSession();
                    const checkId = ctx.input && ctx.input.checkId;
                    if (!checkIdExists(plan, checkId)) throw new CanvasError("unknown_check", "Unknown checkId: " + checkId);
                    const valid = ["pass", "fail", "skip", "pending"];
                    const status = ctx.input.status;
                    if (status != null && !valid.includes(status)) throw new CanvasError("bad_status", "status must be one of " + valid.join(", "));
                    const it = mutateItem(sess, checkId, { status, note: ctx.input.note });
                    writeSession(runId, sess);
                    broadcastSession();
                    return { ok: true, runId, checkId, item: it };
                },
            },
            {
                name: "start",
                description: "Start a new review run. Returns the new run id.",
                inputSchema: {
                    type: "object",
                    properties: { tester: { type: "string", description: "Optional tester name." } },
                    additionalProperties: false,
                },
                handler: (ctx) => {
                    resolveRepoRoot();
                    const runId = startRun((ctx.input && ctx.input.tester) || "");
                    broadcast({ type: "run" });
                    return { ok: true, runId, reportPath: `test-reports/${runId}/report.md` };
                },
            },
            {
                name: "conclude",
                description: "Finalize the active run and clear it. Returns the report path.",
                handler: () => {
                    const { runId, sess } = requireActivePlanSession();
                    sess.concludedAt = new Date().toISOString();
                    writeSession(runId, sess);
                    clearActiveRunId();
                    broadcast({ type: "run" });
                    return { ok: true, runId, reportPath: `test-reports/${runId}/report.md` };
                },
            },
            {
                name: "get_report",
                description: "Read-only: returns the markdown report for the active run.",
                handler: () => {
                    const { runId, plan, sess } = requireActivePlanSession();
                    return { runId, report: generateReport(plan, sess), path: `test-reports/${runId}/report.md` };
                },
            },
        ],
        open: async (ctx) => {
            resolveRepoRoot(ctx);
            if (ctx.input && ctx.input.runId && existsSync(runDir(ctx.input.runId))) {
                setActiveRunId(ctx.input.runId);
            }
            let entry = servers.get(ctx.instanceId);
            if (!entry) {
                entry = await startServer();
                servers.set(ctx.instanceId, entry);
            }
            const runId = getActiveRunId();
            return {
                title: "Manual Review",
                status: runId ? "run " + runId : "no active run",
                url: entry.url,
            };
        },
        onClose: async (ctx) => {
            const entry = servers.get(ctx.instanceId);
            if (entry) {
                servers.delete(ctx.instanceId);
                for (const res of sseClients) { try { res.end(); } catch { /* ignore */ } }
                await new Promise((r) => entry.server.close(() => r()));
            }
        },
    };
}

log("manual-review-canvas ready");
