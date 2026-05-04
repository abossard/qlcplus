# RGB Script Hot-Reload — Implementation Plan

## Overview

Enable auto-reload of RGB Matrix JS scripts from disk without restarting QLC+.
Gated behind a `--watch-scripts` CLI flag for dev builds.

## Key Findings

- Scripts are loaded **from disk**, NOT from qrc
- `RGBScriptsCache` stores `name → filename` map, constructs fresh `RGBScript` on each request
- `RGBScript::load(filename)` re-reads from disk, `evaluate()` re-evaluates JS — **already idempotent**
- Single shared `QJSEngine` on a dedicated `JSThread` — thread-safe via BlockingQueuedConnection
- User scripts already supported at `~/.qlcplus/RGBScripts/` (or macOS equivalent)
- No existing file watcher or dev flag

## The Building Blocks Already Exist

1. `RGBScript::load(filename)` — reads file, stores contents
2. `RGBScript::evaluate()` — clears old JS bindings, re-evaluates, rebinds callables
3. `s_jsThread` — marshals all JS execution to one thread
4. `RGBMatrix::m_algorithmMutex` — protects algorithm access during MasterTimer writes

## Implementation Plan

### Step 1: CLI flag `--watch-scripts`

**Files**: `qmlui/main.cpp`, `qmlui/app.h`, `qmlui/app.cpp`

- Add `--watch-scripts` command-line option parsing in `main.cpp`
- Pass to `App` constructor or set via `App::setWatchScripts(bool)`
- Only enable the file watcher when this flag is present
- Default: OFF (no behavior change for normal users)

**Verify**: `qlcplus-qml --watch-scripts -d` starts with watcher enabled (log message)

### Step 2: Add `QFileSystemWatcher` to `RGBScriptsCache`

**Files**: `engine/src/rgbscriptscache.h`, `engine/src/rgbscriptscache.cpp`

Add:
```cpp
// In header
Q_SIGNAL void scriptFileChanged(QString name, QString filePath);
void enableWatching();

// In cpp
QFileSystemWatcher *m_watcher;  // nullptr unless enabled
```

`enableWatching()`:
- Create `QFileSystemWatcher`
- Watch `systemScriptsDirectory()` and `userScriptsDirectory()` (directory-level)
- On `directoryChanged`: rescan dir, compare with `m_scriptsMap`, emit for new/changed files
- On `fileChanged`: look up which script name maps to that path, emit `scriptFileChanged(name, path)`

**Verify**: editing a `.js` file in the scripts dir triggers `scriptFileChanged` signal

### Step 3: Re-evaluate changed scripts on JSThread

**Files**: `engine/src/rgbscriptv4.h`, `engine/src/rgbscriptv4.cpp`

Add a static reload method:
```cpp
static bool RGBScript::reloadFromDisk(const QString &fileName);
```

This:
1. Re-reads the file contents
2. Marshals `evaluate()` onto `s_jsThread` (already the pattern for all JS calls)
3. Re-binds `m_rgbMap`, `m_rgbMapStepCount`, etc.

The tricky part: `RGBScript` instances are **cloned per RGBMatrix** — there's no global registry of "all live RGBScript instances using file X". So:

**Option A**: RGBMatrix listens to the cache signal and reloads its own algorithm
**Option B**: Cache maintains a weak-ref list of live scripts per filename

Option A is simpler — each `RGBMatrix` connects to `RGBScriptsCache::scriptFileChanged` and checks if its algorithm uses that file.

### Step 4: RGBMatrix auto-refresh on script change

**Files**: `engine/src/rgbmatrix.h`, `engine/src/rgbmatrix.cpp`

When `scriptFileChanged(name, path)` fires:
1. Check if `m_algorithm->name() == name`
2. If yes, lock `m_algorithmMutex`
3. Call `m_algorithm->load(path)` (re-reads file from disk)
4. Call `m_algorithm->evaluate()` (re-evaluates JS)
5. Re-apply saved properties (`m_algorithm->setProperty(...)` for each user-set preset)
6. Unlock mutex
7. Log: `"[RGBMatrix] Script reloaded: <name>"`

**Thread safety**:
- `m_algorithmMutex` is already locked by `RGBMatrix::write()` during playback
- The reload happens on the main thread (signal from QFileSystemWatcher → main event loop)
- `evaluate()` self-marshals to `s_jsThread` via BlockingQueuedConnection
- So: main thread holds `m_algorithmMutex` while waiting for `s_jsThread` to evaluate
- `MasterTimer` thread tries to lock `m_algorithmMutex` in `write()` → blocks until reload completes
- This is safe (no deadlock) as long as `s_jsThread` doesn't try to lock `m_algorithmMutex` (it doesn't)

### Step 5: Handle `ledfx_compat.js` reload

**Files**: `engine/src/rgbscriptv4.cpp`

`ledfx_compat.js` is loaded once into the shared engine's global scope. If it changes:
1. Detect via the same file watcher
2. Re-evaluate the shim file on `s_jsThread`
3. Since globals are replaced by re-declaration, this is safe

### Step 6: Preview refresh

**Files**: `qmlui/rgbmatrixeditor.cpp`

When the script reloads, the preview needs to refresh:
- Connect to the same `scriptFileChanged` signal
- Reset `m_previewStepHandler` and `m_previewElapsed`
- Force a preview redraw

### Step 7: (Optional) UI indicator

Show a small "Live Reload" badge or icon in the RGB Matrix editor when `--watch-scripts` is active.

## Property Preservation

When a script is reloaded, user-set preset values (Sensitivity, Gain, etc.) must survive:

1. Before reload: snapshot `m_properties` map from the current algorithm
2. Reload + re-evaluate
3. After reload: call `setProperty(key, value)` for each saved property
4. Any NEW properties from the updated script get their defaults

The existing `RGBScript` copy-constructor (lines 76-93) already does this pattern — reuse it.

## Edge Cases

- **File deleted**: remove from cache, log warning, keep running with stale algorithm
- **Syntax error in new script**: `evaluate()` returns error → log it, keep old algorithm, don't crash
- **Script renamed**: directory watcher detects add + remove → old entry removed, new entry added
- **Rapid saves**: QFileSystemWatcher may fire multiple times for one save → debounce with a 200ms QTimer
- **Script running during reload**: mutex ensures atomic swap between MasterTimer ticks

## Performance Impact

- `QFileSystemWatcher` is OS-native (inotify/kqueue/ReadDirectoryChanges) — near-zero CPU
- Reload only triggers on actual file change, not per tick
- Only the changed script is re-evaluated, not all scripts
- No impact when `--watch-scripts` is not set (watcher not created)

## Files Changed Summary

| File | Change |
|------|--------|
| `qmlui/main.cpp` | `--watch-scripts` CLI flag |
| `qmlui/app.h/.cpp` | Pass flag, call `enableWatching()` |
| `engine/src/rgbscriptscache.h/.cpp` | `QFileSystemWatcher`, `scriptFileChanged` signal |
| `engine/src/rgbmatrix.h/.cpp` | Connect signal, reload algorithm with mutex |
| `engine/src/rgbscriptv4.h/.cpp` | Ensure `load()` + `evaluate()` is re-entrant safe |
| `qmlui/rgbmatrixeditor.cpp` | Preview refresh on reload |

## Effort Estimate

| Step | Estimate |
|------|----------|
| 1: CLI flag | 15 min |
| 2: File watcher in cache | 1h |
| 3: Script re-evaluate | 30 min |
| 4: RGBMatrix auto-refresh | 1h |
| 5: ledfx_compat reload | 15 min |
| 6: Preview refresh | 30 min |
| 7: Debounce + error handling | 30 min |
| Testing | 1h |
| **Total** | **~5h** |

## Dev Workflow After Implementation

```bash
# Terminal 1: run QLC+ with script watching
cd build && ./qmlui/qlcplus-qml -d --watch-scripts

# Terminal 2: edit a script
vim resources/rgbscripts/audiobuildup.js
# Save → QLC+ automatically reloads → preview updates live
```

No rebuild needed. No restart needed. Just save the file.
