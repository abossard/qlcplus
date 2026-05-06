import { joinSession } from "@github/copilot-sdk/extension";
import { join } from "node:path";
import { execFile } from "node:child_process";
import { CopilotWebview } from "./lib/copilot-webview.js";

const REPO = process.cwd();
const BUILD_DIR = join(REPO, "build");
const BINARY = join(BUILD_DIR, "qmlui", "qlcplus-qml");

let buildProc = null;
let appPid = null;
let buildLog = "";

function run(cmd, args, opts = {}) {
    return new Promise((resolve) => {
        const proc = execFile(cmd, args, { cwd: opts.cwd || REPO, maxBuffer: 10 * 1024 * 1024 }, (err, stdout, stderr) => {
            resolve({ ok: !err, stdout, stderr, code: err?.code });
        });
        if (opts.onData) {
            proc.stdout?.on("data", opts.onData);
            proc.stderr?.on("data", opts.onData);
        }
        if (opts.ref) opts.ref.proc = proc;
    });
}

async function getStatus() {
    if (appPid) {
        try { process.kill(appPid, 0); } catch { appPid = null; }
    }
    return {
        appRunning: appPid !== null,
        appPid,
        building: buildProc !== null,
    };
}

const webview = new CopilotWebview({
    extensionName: "qlcplus_dev",
    contentDir: join(import.meta.dirname, "content"),
    title: "QLC+ Dev",
    width: 520,
    height: 480,
    callbacks: {
        log: (msg, opts) => session.log(msg, opts),

        getStatus: async () => getStatus(),

        configure: async () => {
            buildLog = "";
            const r = await run("cmake", ["..", "-Dqmlui=ON"], { cwd: BUILD_DIR });
            buildLog = r.stdout + r.stderr;
            return { ok: r.ok, log: buildLog.slice(-2000) };
        },

        build: async () => {
            buildLog = "";
            const ref = {};
            buildProc = true;
            const r = await run("cmake", ["--build", ".", "-j8"], {
                cwd: BUILD_DIR,
                ref,
                onData: (chunk) => { buildLog += chunk; },
            });
            buildProc = null;
            return { ok: r.ok, log: buildLog.slice(-3000) };
        },

        start: async () => {
            if (appPid) return { ok: false, error: "Already running (PID " + appPid + ")" };
            const proc = execFile(BINARY, ["-d"], {
                cwd: BUILD_DIR,
                detached: true,
                stdio: "ignore",
            });
            proc.unref();
            appPid = proc.pid;
            return { ok: true, pid: appPid };
        },

        stop: async () => {
            if (!appPid) return { ok: false, error: "Not running" };
            try {
                process.kill(appPid, "SIGTERM");
                const pid = appPid;
                appPid = null;
                return { ok: true, stopped: pid };
            } catch (e) {
                appPid = null;
                return { ok: false, error: e.message };
            }
        },

        restart: async () => {
            if (appPid) {
                try { process.kill(appPid, "SIGTERM"); } catch {}
                appPid = null;
                await new Promise(r => setTimeout(r, 1000));
            }
            const proc = execFile(BINARY, ["-d"], {
                cwd: BUILD_DIR,
                detached: true,
                stdio: "ignore",
            });
            proc.unref();
            appPid = proc.pid;
            return { ok: true, pid: appPid };
        },

        buildAndRestart: async () => {
            buildLog = "";
            buildProc = true;
            const r = await run("cmake", ["--build", ".", "-j8"], {
                cwd: BUILD_DIR,
                onData: (chunk) => { buildLog += chunk; },
            });
            buildProc = null;
            if (!r.ok) return { ok: false, phase: "build", log: buildLog.slice(-3000) };

            if (appPid) {
                try { process.kill(appPid, "SIGTERM"); } catch {}
                appPid = null;
                await new Promise(r => setTimeout(r, 1000));
            }
            const proc = execFile(BINARY, ["-d"], {
                cwd: BUILD_DIR,
                detached: true,
                stdio: "ignore",
            });
            proc.unref();
            appPid = proc.pid;
            return { ok: true, phase: "running", pid: appPid };
        },
    },
});

const session = await joinSession({
    tools: webview.tools,
    commands: [{
        name: "qlcplus-dev",
        description: "Open the QLC+ dev build/start dashboard",
        handler: webview.show,
    }],
    hooks: { onSessionEnd: webview.close },
});
