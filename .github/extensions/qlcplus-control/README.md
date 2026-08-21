# QLC+ Dev Control (canvas extension)

A Copilot CLI **canvas** that builds, runs, and monitors the QLC+ QML app
(`build/qmlui/qlcplus5`) from a side panel — and exposes the same operations
as agent-callable actions.

## What it does

- **Rebuild** — runs `cmake --build . --target qlcplus5 -j8`, auto-running
  `cmake .. -Dqmlui=ON` first if the `build/` dir isn't configured yet. Output
  streams live into the **Build output** tab.
- **Start / Stop / Restart** — launches the binary and terminates it (SIGTERM).
- **Debug flag** — a `Debug (-d)` checkbox plus a free-text field for any extra
  CLI args (e.g. `-o show.qxw`).
- **Live stdout log viewer** — the app's stdout/stderr is captured and streamed
  into the **App stdout** tab.
- **CPU / memory / uptime** — sampled from `ps` every 2s and shown in the header.

## How it behaves

- The QLC+ process is a singleton, so **all open panels show the same live
  state**.
- The app is spawned **detached** with stdout/stderr redirected to a log file,
  so it **survives an `extensions_reload`** — the panel re-discovers the running
  PID and resumes tailing. (It is intentionally *not* killed when the extension
  restarts, matching the repo rule "don't kill/restart QLC+ automatically.")
- If QLC+ is already running but was **not** started by this panel (external
  launch), the panel shows `running (external)` with a blue pill: it can still
  monitor CPU/mem and **Stop** it, but it cannot show stdout (it doesn't own the
  process's output stream).

## Storage

Run metadata and logs live **outside the repo**, under:

```
$COPILOT_HOME/extensions/qlcplus-control/artifacts/
  run.json                 # active managed run: pid, args, logFile, startedAt
  logs/run-<timestamp>.log # captured app stdout/stderr
  logs/build-<timestamp>.log
```

(`$COPILOT_HOME` defaults to `~/.copilot`.)

## Agent actions

| Action      | Kind       | Description |
|-------------|------------|-------------|
| `status`    | read-only  | Process status, pid, CPU/mem, build state. |
| `start`     | write      | Start the app. `{ debug?: bool, extraArgs?: string }`. |
| `stop`      | write      | SIGTERM the running app (managed or external). |
| `restart`   | write      | Stop then start, reusing prior flags unless overridden. |
| `rebuild`   | write      | Configure-if-needed + build; returns immediately, runs async. |
| `tail_log`  | read-only  | Recent lines from the app or build log. `{ type?: "app"\|"build", lines?: number }`. |

`rebuild` is asynchronous — poll `status` (build state flips
`running → success`/`failed`) or read `tail_log` with `type: "build"`.

## Notes

- Builds and runs require the usual toolchain on `PATH` (CMake, Qt 6, a C++
  compiler). Failures are surfaced verbatim in the Build output tab.
- The binary is expected at `build/qmlui/qlcplus5` (the dev layout — no
  `.app` bundle).
