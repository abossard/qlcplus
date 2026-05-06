
# Review: Aubio migration plan

## Independent verification summary

- External aubio repo version is `0.5.0-alpha` from `VERSION`.
- Git state: `0.4.9-376-gad5cf975`, recent HEAD `ad5cf975`.
- External `src/` includes the expected core modules plus `effects/`.
- External root `CMakeLists.txt` is minimal and unconditionally enters `src`, `examples`, and `tests`.
- External `src/CMakeLists.txt` builds `aubio` as `SHARED`, requires `PkgConfig`, unconditionally lists I/O/effects sources, and does not wire Accelerate/FFTW config correctly.
- Current QLC+ wrapper links `qlcplusaubio` via `engine/audio/src/CMakeLists.txt`.
- Non-vendored QLC+ code appears to include aubio only through `<aubio/aubio.h>` in `aubioprocessor.h` and `aubio_smoke_test.cpp`; it does **not** directly include `onset/onset.h`, `tempo/tempo.h`, etc.
- The QLC+ wrapper uses public aubio APIs only; no QLC+ code includes `aubio_priv.h`.
- Used public API signatures for onset, tempo, pitch, notes, MFCC, filterbank, pvoc, specdesc, and TSS appear compatible between the bundled tree and external 0.5.0-alpha.
- `fvec_t` and `cvec_t` layouts are unchanged for the fields QLC+ accesses directly: `length`, `data`, `norm`, and `phas`.

## Blocking Issues

### 1. License compatibility is not sufficiently addressed

**Severity:** Blocking

**Issue:**  
The plan states “aubio is GPL-3 — same as currently bundled, no licensing change.” That is true in the narrow sense that the bundled aubio copy is also GPL-3, but it misses the larger project-level problem: the QLC+ repository root advertises Apache 2.0 licensing in `COPYING` and `README.md`, while aubio is GPL-3. Static-linking GPL-3 aubio into an Apache-2.0-distributed application is a serious distribution/compliance concern.

Evidence:
- QLC+ root `COPYING` is Apache License 2.0.
- QLC+ `README.md` says “Licensed under the Apache 2.0 License.”
- Bundled aubio `COPYING` is GPL v3.
- External aubio `COPYING` is also GPL v3.

**Impact:**  
Even if the migration does not worsen the current technical state, it may formalize or expose an existing license incompatibility. This can block public distribution, packaging, CI artifacts, binary releases, or downstream redistribution.

**Recommended fix:**  
Before migrating, explicitly resolve the licensing model:
- Confirm whether QLC+ as distributed is actually intended to be Apache-2.0-only, GPL-compatible, or multi-licensed.
- Document aubio as a GPL-3 component with source availability and license notices.
- If Apache-only distribution is required, do not statically link GPL-3 aubio; consider making aubio an optional system/runtime dependency, replacing it, or getting legal guidance.
- Update the plan’s license section from “no licensing change” to “existing GPL-3 dependency must be reviewed for Apache-2.0 compatibility and release compliance.”

## Non-Blocking Issues

### 2. Submodule is reasonable for reproducible repo builds, but the plan overcommits too early

**Severity:** Non-Blocking

**Issue:**  
A git submodule is not obviously wrong, but the plan treats it as the default before separating local experimentation from committed integration. Since the user already has `/Users/abossard/Desktop/projects/aubio`, the lowest-friction first step is a CMake path override against that local checkout. A submodule is more appropriate once licensing, build behavior, and CI implications are resolved.

**Impact:**  
Submodules add real friction:
- contributors must initialize/update them;
- GitHub Actions checkout must enable submodules;
- source archives/release packaging must include submodule contents;
- shallow clones and downstream package builders often need special handling.

**Recommended fix:**  
Revise the migration to two phases:

1. **Prototype phase:** support `-DAUBIO_SOURCE_DIR=/Users/abossard/Desktop/projects/aubio` or similar and build external aubio through the existing QLC+ wrapper.
2. **Repository phase:** only after validation, choose between:
   - pinned submodule,
   - vendored source snapshot,
   - `FetchContent` source fetch with pinned commit,
   - system/package dependency.

If choosing submodule, add explicit CI/release tasks:
- update GitHub Actions checkout to use submodules;
- document `git submodule update --init --recursive`;
- ensure source release/tarball packaging includes aubio;
- pin to an exact commit, not `master`.

### 3. The plan’s `AUBIO_SOURCE_DIR` / `AUBIO_SRC_DIR` naming is inconsistent

**Severity:** Non-Blocking

**Issue:**  
Step 1 proposes `-DAUBIO_SOURCE_DIR=/Users/abossard/Desktop/projects/aubio`, while Step 2 talks about `AUBIO_SRC_DIR=${CMAKE_SOURCE_DIR}/third_party/aubio/src`. One points to the repo root; the other points to `src/`.

**Impact:**  
This kind of mismatch commonly causes fragile CMake logic and confusing include/source path bugs.

**Recommended fix:**  
Define one public cache variable clearly, preferably:

```cmake
AUBIO_SOURCE_DIR=/path/to/aubio-repo
```

Then derive internally:

```cmake
set(AUBIO_SRC_DIR "${AUBIO_SOURCE_DIR}/src")
```

Also validate:

```cmake
if(NOT EXISTS "${AUBIO_SRC_DIR}/aubio.h")
    message(FATAL_ERROR "AUBIO_SOURCE_DIR must point to an aubio repo containing src/aubio.h")
endif()
```

### 4. Include-path refactoring is smaller than the prompt suggests, but the shim must be Windows-safe

**Severity:** Non-Blocking

**Issue:**  
The plan correctly notices that external aubio’s source layout does not naturally provide `<aubio/aubio.h>` unless installed. However, current non-vendored QLC+ code does not appear to include individual aubio headers directly; it mostly uses `<aubio/aubio.h>`. So the refactor surface is small.

The risky part is the proposed “generated symlink dir” option. Symlinks are fragile on Windows and in some packaging environments.

**Impact:**  
A symlink-based include shim could break Windows builds, source packages, or developer checkouts without symlink privileges.

**Recommended fix:**  
Avoid symlinks. Generate or commit a tiny forwarding include tree, for example:

```text
build/generated/aubio-include/aubio/aubio.h
```

with contents equivalent to:

```c
#pragma once
#include "/absolute/or/configured/path/to/aubio/src/aubio.h"
```

Or use `configure_file()` to generate it with the correct source path. Then expose only the generated include root publicly, and keep `${AUBIO_SRC_DIR}` private for compiling aubio sources.

### 5. Upstream CMake is correctly rejected, but the plan should be more explicit about root CMake side effects

**Severity:** Non-Blocking

**Issue:**  
The plan is right to keep QLC+’s custom wrapper. External aubio’s CMake is not production-ready for this use case:
- root `CMakeLists.txt` enters `examples` and `tests` unconditionally;
- `src/CMakeLists.txt` builds shared only;
- `PkgConfig` is required;
- optional I/O/effects dependencies are probed and sources are still listed;
- Accelerate/FFTW config is incomplete.

**Impact:**  
A future implementer might still try `add_subdirectory(third_party/aubio)` and hit avoidable build failures or dependency creep.

**Recommended fix:**  
State explicitly: do **not** call external aubio’s root `CMakeLists.txt` or `src/CMakeLists.txt` from QLC+. Treat the external repo as a source tree only, and build selected files through QLC+’s `qlcplusaubio` target.

### 6. CI/CD tasks are too vague

**Severity:** Non-Blocking

**Issue:**  
The plan says “Update CLAUDE.md and `.github/copilot-instructions.md` with the submodule init step” but does not mention actual CI checkout behavior.

**Impact:**  
CI will fail if aubio is a submodule and Actions checkout does not fetch it. Release builders may also fail if source archives omit the submodule.

**Recommended fix:**  
Add concrete CI tasks:
- update every GitHub Actions `actions/checkout` step with `submodules: recursive` if submodule is chosen;
- add a configure-time failure message if aubio source is missing;
- decide how release archives include aubio source;
- test clean clone instructions from scratch.

## Suggestions

### 7. Pin the exact commit, not the alpha branch

**Severity:** Suggestion

**Issue:**  
The external repo is `0.5.0-alpha`, and `git describe` reports `0.4.9-376-gad5cf975`. There does not appear to be a `0.5.0` final tag in this checkout.

**Recommended fix:**  
Pin the exact SHA `ad5cf975` or another audited SHA. Do not track `master`. If a final `0.5.0` tag appears later, treat that as a separate upgrade with regression testing.

### 8. Add audio regression tests, not only build/smoke tests

**Severity:** Suggestion

**Issue:**  
The API signatures look compatible, but aubio 0.5.0-alpha can still change DSP behavior. The current smoke test only verifies object creation/destruction.

**Recommended fix:**  
Add at least one deterministic test using a small synthetic or fixture signal:
- onset fires for a known impulse/percussive transition;
- pitch detects a known sine frequency within tolerance;
- tempo stabilizes on a click track;
- MFCC/filterbank outputs are finite and dimensionally valid.

This matters more than API compatibility because QLC+ consumes aubio as a real-time analysis engine.

### 9. Preserve current FFT/backend behavior explicitly

**Severity:** Suggestion

**Issue:**  
External aubio 0.5.0-alpha still supports Accelerate, FFTW3, Intel IPP, and Ooura-style fallback paths. FFTW3 is optional, not required, as long as QLC+ does not define `HAVE_FFTW3` and excludes `dct_fftw.c`.

**Recommended fix:**  
In the wrapper, explicitly preserve:
- Apple: define `HAVE_ACCELERATE=1`, link `-framework Accelerate`, exclude `dct_ooura.c`, `dct_fftw.c`, `dct_ipp.c`.
- Non-Apple: do not define `HAVE_ACCELERATE` or `HAVE_FFTW3`; include Ooura sources; exclude Accelerate/FFTW/IPP DCT backends as appropriate.
- Keep `dct_plain.c` included because `dct.c` can fall back to it.

### 10. Upstream contribution is nice, but should not block QLC+

**Severity:** Suggestion

**Issue:**  
Fixing upstream aubio CMake would be useful, but it is not a practical prerequisite for QLC+ because QLC+ wants a narrow, static, dependency-minimized build.

**Recommended fix:**  
Keep QLC+’s wrapper for this migration. Optionally open upstream issues/PRs later for:
- static/shared option;
- optional PkgConfig;
- conditional I/O/effects sources;
- proper Accelerate/FFTW config;
- install/export targets.

## Overall assessment

The plan is technically solid on the core build/API strategy: **using the external aubio source tree while preserving QLC+’s minimal CMake wrapper is the right direction**. Upstream aubio CMake is not suitable for direct consumption, and the public APIs used by `AubioProcessor` appear compatible with the external 0.5.0-alpha checkout.

The main course correction is licensing: the plan should not treat GPL-3 as a harmless “no change” detail while the repository root declares Apache 2.0. Resolve that before making the migration more official through a submodule or release-facing dependency.