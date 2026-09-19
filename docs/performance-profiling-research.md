# Live-Effect Profiling on macOS arm64 — Primary-Source Research

Scope: choose an **efficient, agent-controllable (start/stop) profiler** for the active QLC+
effects on this rig with **negligible, MEASURED observer overhead** and **zero work when
inactive**. This report separates *documented capability* (man pages, first-party docs, Qt
source) from *confirmed-local availability*. Availability of build flags is being probed
separately by the parent; where a fact depends on that, a **conditional** recommendation is given.
The research worker did not attach a profiler. The parent subsequently verified a five-second
Time Profiler attachment to PID 97535, automatic recording stop, trace finalization and export.
QLC+ remained running. This checks capability, not observer overhead.

## 0. Verified local environment (primary, this machine)
- `xctrace version 27.0 (27A266a)` present at
  `/Applications/Xcode.app/Contents/Developer/usr/bin/xctrace` (`xcrun --find xctrace`; `xcode-select`
  path `/Applications/Xcode.app/Contents/Developer`). `xctrace list templates` includes **Time
  Profiler**, **CPU Profiler**, **System Trace**, **Allocations** — all locally installed. Confirmed
  flags: `--attach <pid|name>`, `--time-limit`, `--window`, `--recording-options`,
  `--show-recording-options` (no recording, no target required), `--notify-tracing-started`; export
  `--toc` then `--xpath` → XML. Captured option JSON: `sessionfiles/c3-time-profiler-options.json`,
  `c3-cpu-profiler-options.json` (observed **defaults**, not a measured overhead or Hz promise).
- **`/usr/bin/sample` and `/opt/homebrew/bin/qmlprofiler` both present** (Option B and the Qt
  standalone JS/QML profiler for Option D are locally available).
- **This installed Qt build disables QV4 JIT.** The installed `qtqml-config_p.h`
  defines `QT_FEATURE_qml_jit -1`; its conditional recomputation retains that value.
  It also defines `QT_FEATURE_qml_profiler 1`. Upstream call-count thresholds therefore
  do not imply JIT execution here. This matches the recorded `VME::interpret` stacks.
- `sample(1)` and `spindump(8)` man pages present (macOS 26.6). `DevToolsSecurity(8)` present.
- `clock_gettime` man lists `CLOCK_THREAD_CPUTIME_ID` + `CLOCK_PROCESS_CPUTIME_ID`; SDK header
  `mach/thread_info.h` present → per-thread CPU-time APIs are locally available.
- Build: `build/CMakeCache.txt` → `CMAKE_BUILD_TYPE=Release`, `CMAKE_CXX_FLAGS_RELEASE=-O3 -DNDEBUG`
  (**no `-g`, no dSYM** found under `build/`). Binary `build/qmlui/qlcplus5` = Mach-O arm64,
  **not stripped** (`nm -U` → ~14,114 `T` text symbols). PID 97535 maps to this binary (`ps`).
  ⇒ C++ frame *names* will symbolicate from the Mach-O symbol table even without `-g`, but there is
  **no source line / inline info** until a `-g` build or dSYM exists.

## 1. Decision table (viable options)
| Option | Attach to running PID (no restart) | Agent start/stop | On-CPU vs off-CPU | Resolves per-JS-effect? | Added cost when inactive | Setup cost |
|---|---|---|---|---|---|---|
| **A. Instruments Time Profiler via `xctrace`** | Yes (`--attach`) | Yes (`--time-limit`, self-terminates) | Default = running (on-CPU); `recordWaitingThreads`/`contextSwitchSampling` add off-CPU/scheduling | **No** (native QV4 frames only) | None | Symbols/perm setup only |
| **B. `sample(1)`** | Yes (pid) | Yes (duration arg) | **No split** (all thread stacks sampled) | No | None | None (always installed) |
| **C. `spindump(8)`** | Yes (pid) | Yes (duration arg) | Runnable/blocked states; runnable is not measured on-CPU time | No | None | Permissions depend on capture options |
| **D. QV4/QML profiler (qmlprofiler)** | Only if QML-debug build + `-qmljsdebugger` | Yes (`console.profile`/client) | JS-event timeline, not thread-state | **Yes** if the worker engine registers | Debug infrastructure remains; active tracing cost needs measurement | **Rebuild + restart** |
| **E. Per-thread CPU clock in-code** | n/a (already in-proc) | Yes (code-gated) | **Yes** (CPU time vs wall) | Partial (per worker/section) | None if compiled-out | tiny code, no dependency |

Tracy/Perfetto are intentionally excluded as defaults (§6).

## 2. Recommended SHORTEST path — Option A (Instruments Time Profiler via xctrace)
### Measured selection after implementation

Use the small MCP `timing_diagnostics` capture for routine per-effect CPU/wall
measurements, and native Time Profiler for short call-stack investigations.
Leave captures disabled when idle. No special build or new dependency was needed
to identify and fix the Spotlight hotspot.

An isolated production-HUE caller-path experiment at 80x4 pixels compared matched
inputs and binaries. In-process profiling added 1.98, 2.37 and 2.60 microseconds of
**caller-thread CPU** per render across three runs. These exclude worker-side
cost and are not a total overhead bound. Worker instrumentation remains on the
tick's critical path because the caller waits for it. Total caller-wall deltas
were -0.051, +0.013 and -0.0001ms, below the experiment's noise rather than zero.

Native Time Profiler added approximately 0.05-0.13ms of caller wall time per
render in its measured runs. That is a different quantity from caller-only CPU;
no ratio between the two profiler costs is established. The benchmark had one
unregistered-owner HUE renderer and forced reporting-window flushes. These are
boundary measurements, not full live-rig overhead guarantees.

Output hashes matched within each equal-frame-count pair. The 500-, 1500- and
2500-frame chains have different hashes by construction; they must not be
compared across lengths. Manual interruption finalized the owned benchmark's
native trace and left its target alive. Setup and finalization took additional
time outside measured rendering.

Evidence: session artifacts `c3-overhead/PROVENANCE.md` and
`c3-overhead/results/SUMMARY.json`. The original native-first recommendation
below describes tool selection before these measurements.

Rationale: attaches to the already-running PID (no effect/app restart needed for a first look),
is a statistical sampler (low overhead), **attributes to running threads by default** (the CPU-vs-
wall signal the user wants), needs no code change, and is fully agent-controllable via a bounded,
self-terminating capture. Nothing runs when not recording ⇒ zero inactive cost.

Bounded capture (deterministic start/stop, writes a `.trace` and exits on its own):
```
xcrun xctrace record --template 'Time Profiler' \
  --attach <PID> --time-limit <SECONDS>s \
  --output <ABS_OUT_DIR>/tp_<LABEL>.trace
```
Add off-CPU / scheduling detail only when needed (edit the JSON from
`--show-recording-options` and pass it back). Verified-configurable booleans (local):
Time Profiler → `contextSwitchSampling`, `highFrequencySampling`, `recordKernelStacks`,
`recordWaitingThreads`; CPU Profiler → `highFrequency`, `recordKernelCallstacks`.
```
xcrun xctrace record --template 'Time Profiler' --show-recording-options   # prints JSON, no capture
# edit booleans → opts.json, then:
xcrun xctrace record --template 'Time Profiler' --attach <PID> \
  --time-limit <SECONDS>s --recording-options <ABS>/opts.json \
  --output <ABS>/tp_offcpu.trace
```
Continuous ring-buffer variant (bounds retained history, not sampling overhead): add `--window <N>s`.
Export for headless inspection / symbolication:
```
xcrun xctrace export --input <ABS>/tp_<LABEL>.trace --toc                       # list schemas
xcrun xctrace export --input <ABS>/tp_<LABEL>.trace --xpath '<XPATH_FROM_TOC>' --output <ABS>/out.xml
xcrun xctrace symbolicate --input <ABS>/tp_<LABEL>.trace --dsym <ABS>/qlcplus5.dSYM   # if a dSYM exists
```
(No numeric sample-rate field exists — only `highFrequencySampling`; don't cite a Hz — see §0.)

## 3. Fallback if xctrace is unavailable — Options B/C (always-installed)
```
sample <PID> <DURATION_S> <INTERVAL_MS> -mayDie -file <ABS>/sample_<LABEL>.txt   # default 10s / 1ms
spindump <PID> <DURATION_S> <INTERVAL_MS> -onlyTarget -o <ABS>/spindump_<LABEL>.txt  # default 10s/10ms
spindump <PID> <DURATION_S> <INTERVAL_MS> -onlyTarget -onlyRunnable -o <ABS>/sd_run.txt   # on-CPU-ish
spindump <PID> <DURATION_S> <INTERVAL_MS> -onlyTarget -onlyBlocked  -o <ABS>/sd_block.txt  # blocked
```
- `sample(1)` "records the call stacks of **all threads**" at each interval (default 1 ms) → a
  thread parked in a Qt latch still contributes a stack every sample. **Stack counts are presence,
  not CPU time and not latency.** Use it for "what is on-stack", not for a CPU/wall split.
- `spindump(8)` records thread **state** (running/runnable vs suspended) and supports
  `-onlyRunnable`/`-onlyBlocked`; `-onlyTarget` "allows faster sampling rates". This is the
  simple-tool way to distinguish runnable from blocked states. Runnable can include a thread
  waiting for CPU; it is not an exact on-CPU measurement. Permission requirements must be checked
  for the selected options.

## 4. Conditional path for per-JS attribution — Option D (QV4/QML profiler)
Native samplers (A/B/C) attribute to C++/QV4 frames — JIT'd JS as **unsymbolicated machine code**,
interpreted JS as generic `QV4::Moth::VME::interpret` (see §4a) — so they **cannot** name which HUE
JS function/effect is hot. The only tool that maps to JS function ranges is Qt's QML/JS profiler
(the confirmed-local `/opt/homebrew/bin/qmlprofiler`). Key primary findings:
- **A plain `QJSEngine` self-registers with the debug connector.** `QJSEngine::QJSEngine(QObject*)`
  calls `QJSEnginePrivate::addToDebugServer(this)` (qt/qtdeclarative
  `src/qml/jsapi/qjsengine.cpp`), the same call `QQmlEngine`/`QQmlApplicationEngine` make. The
  profiler service handles it: `QQmlProfilerServiceImpl::engineAdded(QJSEngine *engine)` builds a
  `QV4ProfilerAdapter` from `engine->handle()` (`src/plugins/qmltooling/qmldbg_profiler/
  qqmlprofilerservice.cpp`). ⇒ the common claim "QML profiler ignores plain QJSEngine" is **false**
  in current Qt source — *in principle* it can see the shared HUE worker engine.
- **But strict preconditions apply:** `addToDebugServer` is guarded by
  `QQmlDebugConnector::instance()` being non-null, and in a non-debug build the connector's
  `addEngine`/`hasEngine` are **no-ops** (`src/qml/debugger/qqmldebugconnector_p.h`). So it requires:
  (1) build with `QT_QML_DEBUG` (CMake `-DQT_QML_DEBUG` / `target_compile_definitions(... QT_QML_DEBUG)`;
  qmake `CONFIG+=qml_debug`); (2) launch with
  `-qmljsdebugger=port:<N>[,block][,services:<list>]`; (3) a client attached — the confirmed-local
  `qmlprofiler` CLI (recipe below) or Qt Creator; (4) start/stop via the client or
  `console.profile()/console.profileEnd()`.
- **JIT / behavior change:** Qt docs state "the `v4 debug` service disables the JIT". Enabling that
  service makes JS run interpreted, so timings will **not** match the production JIT path. A
  profiler-only service list *may* leave JIT on, but this is unverified locally and must be confirmed.
- **Unverified risks to confirm before relying on D:** whether the HUE worker's QJSEngine, created on
  a **worker thread**, actually registers (connector thread-affinity), and whether the QLC+ build even
  compiles the qmltooling infrastructure. Treat D as a **conditional, restart-required** secondary tool.
  Sources: https://doc.qt.io/qt-6/qtquick-debugging.html (Qt 6.11.2),
  https://doc.qt.io/qt-6/qtquick-profiling.html, https://doc.qt.io/qt-6/qqmldebuggingenabler.html.

Agent-controllable qmlprofiler recipe (confirmed flags from local `qmlprofiler --help`):
```
# Target MUST enable QML debugging on a loopback-only connector.
qmlprofiler --attach 127.0.0.1 --port <N> --record off --interactive \
  --include javascript,memory --output <ABS>/hue_<LABEL>.qtd
# interactive keys: r=toggle record, o=write output, c=clear, f=flush(stop+output+clear), q=quit
# q quits the TARGET only if qmlprofiler LAUNCHED it; on --attach it leaves PID 97535 running.
```
Caveat: the Homebrew `qmlprofiler` and the app's Qt must share a compatible QML-debug protocol
version; a mismatch refuses to connect. Verify the app's Qt version before relying on D.

## 4a. Does native sampling suffice for QJSEngine frames? (QV4 JIT reality)
- QV4 JIT-compiles a JS function after **3 calls** by default (`qv4engine.cpp`
  `s_jitCallCountThreshold = 3`; `QV4_JIT_CALL_THRESHOLD` overrides; `QV4_FORCE_INTERPRETER` forces
  interpreter — `src/qml/doc/src/javascript/finetuning.qdoc`). `canJIT()` also needs
  `QT_CONFIG(qml_jit)` compiled in **and** `m_canAllocateExecutableMemory` (`qv4engine_p.h`); the
  baseline JIT then generates code (`qv4vme_moth.cpp` `QV4::JIT::BaselineJIT::generate`).
- The call threshold applies only when the build and runtime allow JIT. Here the installed
  feature is disabled, so the interpreter stacks are expected. Do not infer execution mode
  from call frequency. A different Qt build could produce unsymbolicated JIT frames instead.
- **So native sampling DOES suffice to:** (1) confirm the JS worker thread is the hot thread,
  (2) split JS-execute vs GC/alloc (`QV4::MemoryManager`) vs marshalling (`QObjectWrapper`/metacall)
  vs blocking, (3) quantify on-CPU time. It does **not** attribute to a specific HUE effect/JS function.
- Sharper native attribution, each with a cost: `QV4_FORCE_INTERPRETER=1` collapses JS to the single
  `VME::interpret` symbol (still not per-function, and **slower → biases A/B**); or use Option D for
  per-JS-function ranges (restart required; debug-service side effects need checking). This
  installed Qt has no JIT to disable, but active profiler overhead still requires measurement.
  Tell JIT vs interpreter apart in a
  trace by presence of `VME::interpret` (interpreter) vs anonymous executable-region frames (JIT).

## 5. What each option can / cannot resolve (evidence map)
- **JS compute vs marshalling vs GC vs scheduling at the C++ layer:** A (best; on-CPU by default),
  C (with runnable/blocked split), B (presence only). In native stacks these appear as QV4 JS
  execution (JIT'd machine code or `VME::interpret`, see §4a), `QV4::MemoryManager`/GC + allocation,
  `QV4::QObjectWrapper`/metacall (marshalling), and Qt condition/latch waits (scheduling/blocking).
  **Resolvable at the category level.**
- **Which specific HUE effect / JS function is hot:** only D. A/B/C **cannot** attribute to JS source.
- **CPU time vs wall time for the worker specifically:** A (default running-thread attribution) or E
  (exact per-thread CPU clock). B cannot; C approximates via thread state.
- **Precise JS function/line attribution:** none of A/B/C — JIT'd JS is unsymbolicated, interpreted JS
  collapses to one `VME::interpret` symbol (§4a). Only D resolves per-JS-function ranges.

## 6. Complementary / rejected — Option E vs Tracy/Perfetto
- **E (recommended tiny complement, no dependency):** the paused implementation could read the HUE
  worker thread's CPU time via `clock_gettime(CLOCK_THREAD_CPUTIME_ID, …)` or mach
  `thread_info(thread, THREAD_BASIC_INFO, …)` (user+system time), sampled around the existing
  TimingDiag wall window. That yields a per-render CPU-versus-wall split. A runtime gate avoids
  clock reads while off but still costs a gate check; compile-time removal requires a separate
  build option. Both APIs are locally available (§0).
- **Tracy / Perfetto — rejected as default:** both require **source instrumentation** (Tracy
  `ZoneScoped`/`FrameMark` + `TRACY_ENABLE` + `TracyClient.cpp`; Perfetto `TRACE_EVENT` + SDK) and a
  new build dependency — more instrumentation/build surface than Apple tooling, which already
  suffices, and against the "simplicity, no speculative telemetry" goal. Verify at
  https://github.com/wolfpld/tracy/releases and https://perfetto.dev/docs if ever revisited.

## 7. Low-overhead A/B validation protocol (this rig)
1. **Hold optimization constant.** Current Release = `-O3 -DNDEBUG` (no symbols); default RelWithDebInfo
   = `-O2 -g -DNDEBUG`. **Never A/B `-O3` vs `-O2`** — that changes codegen and biases the result.
   Simplest unbiased fix: rebuild once as **`-O3 -g -DNDEBUG`** (current Release optimization, symbols
   added) and use that single binary for BOTH the no-profiler and profiler arms. (`-g` adds line/inline
   info and does not change optimization; a matching dSYM improves symbolication.)
2. **Measure observer overhead empirically; never claim zero.** With the same effect set running at
   50 Hz (20 ms), capture the app's own steady-state metric (existing TimingDiag per-render wall
   mean/max, or engine-tick jitter) for a fixed window WITHOUT any profiler; then repeat an identical
   window WITH `xctrace … --attach` (Option A). Report the measured delta as the overhead.
3. **Repeat ≥3×, alternate arm order** (A,B,B,A…) to control for thermal/drift on the M1 Pro.
4. **Bound each capture** with `--time-limit`; use `--window` if watching continuously.
5. **Symbolication:** confirm symbols (`nm -U <bin> | wc -l`); for line/inline info produce a dSYM and
   pass `--dsym`. Frame *names* already resolve on the current non-stripped binary.

## 8. Permissions / setup (documented)
- Authorization depends on developer-tool permissions, signing, target and selected instruments.
  The parent's Time Profiler attach succeeded without elevation. This does not establish
  permissions for every instrument or target. Respect any permission prompt or denial; do not
  change security settings automatically.
- No SIP disable, no `--options runtime` re-signing, no security bypass is involved for Options A/B/C.

## 9. Stop / cleanup & live-state preservation
- Option A: prefer `--time-limit` — the recording **self-terminates** and finalizes the `.trace`. For a
  manual stop, signal **only the `xctrace` subprocess** by its own PID. The observed recorder
  prints `Ctrl-C to stop the recording`. Timed stop was verified on QLC+, and manual
  interruption/finalization was verified on an owned benchmark without killing its target.
  **Never signal the QLC+ target PID to stop profiling.**
- Options B/C/D: `sample`/`spindump` self-terminate and write to `-file`/`-o`; `qmlprofiler` on
  `--attach` leaves the target running on `q`. These commands do not intentionally stop the show,
  but profiling can perturb execution. Verify target liveness and effect state after recording.
- **Trace privacy:** Instruments captures process environment metadata, which can contain
  credentials. Keep raw `.trace` bundles local and owner-only. Do not print or share an
  unfiltered TOC/process-info export. Redact environment metadata and export only the required
  sampling tables for analysis.

## 10. Open local-capability decisions for the parent to confirm
1. Is a `-g` build / dSYM wanted for line-level symbolication, or are C++ frame names (already
   present) sufficient? (Affects only depth, not feasibility.)
2. Attach is verified for the original PID 97535, and manual stop was verified on an owned
   benchmark. Its requested five-second trace had a 12.171-second empirical sample span;
   use recorded timestamps, not the requested limit, when computing occupancy.
3. For Option D only: does the QLC+/Qt build compile the qmltooling/debug infrastructure, does the
   worker-thread QJSEngine register, and does the Homebrew `qmlprofiler` protocol match the app's Qt?
   All **unverified** and gate any per-JS attribution.
4. QV4 JIT is disabled in the installed Qt build. For another build, verify effective feature
   flags and debugger-service behavior rather than applying upstream defaults.

## 11. Citations (primary)
- `sample(1)`, `spindump(8)`, `xctrace(1)`, `DevToolsSecurity(8)`, `clock_gettime(2)` — local man pages, macOS 26.6.
- `xcrun --find xctrace` → `/Applications/Xcode.app/Contents/Developer/usr/bin/xctrace`; `xctrace list templates`; `xctrace help record|export|symbolicate`; `xctrace record --show-recording-options` (saved `sessionfiles/c3-time-profiler-options.json`, `c3-cpu-profiler-options.json`); local tool paths `/usr/bin/sample`, `/opt/homebrew/bin/qmlprofiler` and `qmlprofiler --help` — local output, xctrace 27.0 (27A266a).
- Qt: https://doc.qt.io/qt-6/qtquick-debugging.html ; https://doc.qt.io/qt-6/qtquick-profiling.html ; https://doc.qt.io/qt-6/qqmldebuggingenabler.html (Qt 6.11.2).
- Qt source (qt/qtdeclarative): `src/qml/jsapi/qjsengine.cpp` (`QJSEngine::QJSEngine`, `QJSEnginePrivate::addToDebugServer`); `src/qml/debugger/qqmldebugconnector_p.h` (no-op fallback); `src/plugins/qmltooling/qmldbg_profiler/qqmlprofilerservice.cpp` (`engineAdded(QJSEngine*)`, `QV4ProfilerAdapter`).
- Qt source — QV4 JIT: `src/qml/jsruntime/qv4engine.cpp` (`s_jitCallCountThreshold = 3`, `QV4_FORCE_INTERPRETER`); `src/qml/jsruntime/qv4vme_moth.cpp` (`canJIT` → `QV4::JIT::BaselineJIT::generate` else `VME::interpret`); `src/qml/jsruntime/qv4engine_p.h` (`canJIT`, `QT_CONFIG(qml_jit)`, `m_canAllocateExecutableMemory`); `src/qml/doc/src/javascript/finetuning.qdoc` (`QV4_JIT_CALL_THRESHOLD`, `QV4_FORCE_INTERPRETER`).
- Local build/PID facts: `build/CMakeCache.txt`, `nm -U build/qmlui/qlcplus5`, `ps -p 97535`.
- Installed feature evidence: `/opt/homebrew/opt/qtdeclarative/lib/QtQml.framework/Headers/6.11.2/QtQml/private/qtqml-config_p.h`; `QT_FEATURE_qml_jit -1`, `QT_FEATURE_qml_profiler 1`.
- Parent attach proof: session artifacts `c3-time-profiler-smoke.trace` (owner-only raw data)
  and `c3-time-profiler-smoke-toc.xml` (environment metadata redacted).
- Complementary tools (secondary, verify before use): https://github.com/wolfpld/tracy/releases ; https://perfetto.dev/docs/instrumentation/trace-sdk.
