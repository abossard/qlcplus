# Plan: Switch QLC+ to External aubio

## 1. Current state

### Bundled aubio (`engine/audio/aubio/`)
- **Version:** 0.4.9 (partial upstream `src/` tree — `aubio.h`, `pitch/`, `tempo/`, `notes/`, `onset/`, `spectral/`, `temporal/`, `utils/`, `io/`, `synth/`, `mathutils`, `vecutils`, `cvec`, `fvec`, `lvec`, `fmat`).
- **Build:** custom `engine/audio/aubio/CMakeLists.txt` produces a STATIC lib `qlcplusaubio`:
  - `file(GLOB_RECURSE *.c)` then excludes `io/`, `synth/`, `dct_fftw`, `dct_ipp`, `windll`, and the non-platform DCT backend.
  - FFT backend: Accelerate on macOS (`HAVE_ACCELERATE=1`, links `-framework Accelerate`); Ooura everywhere else.
  - Public include dir is the parent (`engine/audio/`) so consumers `#include <aubio/aubio.h>`.
  - Warnings silenced (`-w`) to keep parent `-Werror` clean.
- **Linked from:** `engine/audio/src/CMakeLists.txt` (target `audio` links `qlcplusaubio`).

### QLC+ wrapper / API surface
- Wrapper: `engine/audio/src/aubioprocessor.{h,cpp}` (+ `aubioresults.h`, used by `audiocapture.cpp`, `audiochannel.cpp`, `audiosnapshot.h`).
- Single include: `#include <aubio/aubio.h>`.
- aubio API actually used (all stable since 0.4.x):
  - `aubio_onset_*` (+ threshold, silence, minioi_ms, delay_ms)
  - `aubio_pitch_*` (+ tolerance, unit, silence, confidence)
  - `aubio_tempo_*` (+ bpm, confidence, last_s, period_s, tatum_signature, was_tatum, threshold, silence)
  - `aubio_notes_*`
  - `aubio_mfcc_*`, `aubio_filterbank_*` (+ mel_coeffs_slaney, norm, power)
  - `aubio_pvoc_*`, `aubio_specdesc_*`, `aubio_tss_*`
- All usage is via the public `aubio.h`; no private headers are included from QLC+ code.

## 2. External aubio (`/Users/abossard/Desktop/projects/aubio`)

- **Version:** `0.5.0-alpha` (`AUBIO_MAJOR=0 MINOR=5 PATCH=0`, libtool current 5/rev 4/age 8). Git: `master` at `ad5cf975`.
- **Build systems:** waf (canonical), meson, and an early-stage CMake. Layout matches our bundled tree.
- **Modules (vs. bundled):** same core (`pitch/ tempo/ notes/ onset/ spectral/ temporal/ utils/ synth/ io/`) **plus new** `effects/` with `pitchshift_rubberband.c` and `timestretch_rubberband.c` (require rubberband).
- **CMake status (important):** `src/CMakeLists.txt` is incomplete for embedding:
  - Builds `aubio` as **SHARED only** (no option), `SOVERSION 5.4.8`.
  - `find_package(PkgConfig REQUIRED)` — hard requirement just to detect optional deps.
  - I/O and effects sources are listed unconditionally; `sink_sndfile`, `sink_flac`, `sink_vorbis`, `source_avcodec`, `source_sndfile`, `pitchshift_rubberband`, `timestretch_rubberband` will fail to link when those libs are absent.
  - **No FFT backend wiring**: `HAVE_ACCELERATE` / `HAVE_FFTW3` are TODO in `config.h.cmake.in`. Both `dct_accelerate.c` and `dct_fftw.c` are in the source list, no exclusion. Out of the box this won't build cleanly with our Accelerate setup.
  - No `install(EXPORT)` / target alias.
- **Conclusion:** upstream CMake is not production-ready to consume directly via `add_subdirectory`.

## 3. Recommended integration approach

**Pick: Hybrid Option D + A — git submodule + override CMake (keep our minimal wrapper CMakeLists).**

Rationale:
- Upstream CMake is incomplete and would require patching anyway. Vendoring our own minimal CMake against upstream's `src/` tree is exactly what we already do — and it works.
- Submodule cleanly replaces the embedded copy with the full upstream tree, gives us version pinning, and makes it trivial to update.
- Avoids dragging in PkgConfig, sndfile, libav, FLAC, vorbis, rubberband as requirements.
- Keeps Accelerate/Ooura FFT backend selection deterministic.
- Static link preserves single-binary deployment story.
- Local path can be used during development; submodule path is the committed form.

Rejected:
- **A pure `add_subdirectory`** of the external repo: SHARED-only, missing FFT defines, requires PkgConfig, drags in I/O sources we don't need.
- **`FetchContent`/`ExternalProject`** with upstream's waf/meson: heavyweight, slow incremental builds, harder cross-platform.
- **`find_package` / pkg-config against a pre-installed aubio:** forces every dev/CI box to install aubio 0.5; loses the bundled-build property; macOS bottling unclear.

## 4. Migration steps

- [ ] **Step 1 — Add submodule.** `git submodule add https://github.com/aubio/aubio.git third_party/aubio` (pin to commit `ad5cf975` or a tag). For dev, allow override via `-DAUBIO_SOURCE_DIR=/Users/abossard/Desktop/projects/aubio`.
- [ ] **Step 2 — Move our wrapper CMake.** Replace `engine/audio/aubio/CMakeLists.txt` with a relocated version (e.g. `cmake/qlcplusaubio.cmake` or keep file, make `AUBIO_SRC_DIR` configurable). Default `AUBIO_SRC_DIR=${CMAKE_SOURCE_DIR}/third_party/aubio/src`.
- [ ] **Step 3 — Adapt globs and exclusions for 0.5.0 layout:**
  - Glob `${AUBIO_SRC_DIR}/*.c` recursively.
  - Exclude: `io/`, `synth/`, `effects/` (rubberband), `dct_fftw.c`, `dct_ipp.c`, `windll.c`, plus the non-platform DCT backend.
  - Confirm new files compile: `effects/` is excluded; `synth/` is excluded; `awhitening.c` and any new `spectral/` files build with our existing defines.
- [ ] **Step 4 — Fix include layout for `<aubio/aubio.h>`:** upstream `aubio.h` lives in `src/`. Either (a) keep current trick — set `PUBLIC` include dir to a path that lets `#include <aubio/aubio.h>` resolve (create `engine/audio/aubio_include/aubio/` with a forwarding header, or use a generated symlink dir during configure), or (b) change QLC+ wrapper to `#include "aubio.h"` and add `src/` directly to include path. Prefer (a) to avoid touching wrapper code and to keep public API contract.
- [ ] **Step 5 — Verify FFT backend defines still apply:** keep `HAVE_ACCELERATE=1` on Apple, link `-framework Accelerate`; otherwise Ooura. Verify `spectral/fft.c` in 0.5 still gates on `HAVE_ACCELERATE` (it does — same code path).
- [ ] **Step 6 — Delete `engine/audio/aubio/` bundled tree** (keep only the CMakeLists if we kept it in place pointing elsewhere; otherwise remove the directory entirely and update `engine/audio/CMakeLists.txt` `add_subdirectory(aubio)` to the new location).
- [ ] **Step 7 — Build & smoke test:**
  - `cmake --build build --target qlcplusaubio -j8`
  - `cmake --build build --target audio -j8`
  - `cmake --build build --target qlcplus-qml -j8`
  - Run app, verify audio capture / AudioAnalyzer + onset/tempo/pitch still work on a known clip.
  - Run `audiocapture` / `audiosnapshot` related tests if any exist under `engine/audio/test/`.
- [ ] **Step 8 — CI / docs:**
  - Update `CLAUDE.md` and `.github/copilot-instructions.md` with the submodule init step (`git submodule update --init --recursive`).
  - Update README/build docs.
- [ ] **Step 9 — (Optional) bump pinned commit / track upstream tag** once 0.5.0 final ships.

## 5. New features unlocked (from 0.5.0-alpha)

API used by QLC+ is unchanged in signature; existing wrapper code keeps working. Newly available:

- **`aubio_tempo_get_period_s`, `aubio_tempo_was_tatum`, `aubio_tempo_set_tatum_signature`** — already used in our wrapper but not present in 0.4.9 stock; with 0.5 src this is officially supported (no more relying on locally patched 0.4.9).
- **`effects/` module** — `aubio_pitchshift_*` and `aubio_timestretch_*` (rubberband-backed). *Not enabled by default* in our build (excluded), but trivial to opt in if rubberband is added.
- **`spectral/awhitening.c`** — adaptive spectral whitening; can be wired into AudioAnalyzer to stabilise onset/pitch on noisy inputs.
- **More robust `aubio_notes`** — fewer false positives in 0.5 line.
- **Bug-fix delta** vs. 0.4.9: numerous fixes in `pitch_yinfft`, `tempo`, sampler/io (we don't use io/synth).

Not unlocked unless we opt in:
- File I/O (sink/source) — we feed audio in directly; intentionally excluded.
- Synth/sampler — out of scope for AudioAnalyzer.

## 6. Risks & backward compatibility

- **API drift in 0.5-alpha:** alpha label means upstream may still change signatures. Mitigation: pin submodule to a specific commit, vet our exact API surface (listed in §1) against the pinned tree once.
- **Header layout (`<aubio/aubio.h>`):** upstream's `src/aubio.h` is meant to be installed under `<prefix>/include/aubio/`. Our build never installs; we need the include shim (Step 4). Risk: easy to get wrong on Windows; verify on all platforms.
- **FFT backend regression:** if upstream renames the `HAVE_ACCELERATE` gate or restructures `dct_*`, our exclusion globs may need tweaking. Mitigation: build matrix on macOS + Linux before merging.
- **Warnings:** new files may add new warnings; we already pass `-w` to the aubio target so parent `-Werror` is unaffected.
- **License:** aubio is GPL-3 — same as currently bundled, no licensing change.
- **Compiler defines:** `AUBIO_LOG_DOMAIN="qlcplus"` and `HAVE_*` set must be re-applied; if upstream `config.h.cmake.in` is now consulted (`HAVE_CONFIG_H`), we should keep **not** defining `HAVE_CONFIG_H` so our existing per-target `-D` defines remain authoritative (current behaviour).
- **Submodule friction:** contributors must run `git submodule update --init`. Mitigation: add to setup docs and check in CI; consider `cmake -DAUBIO_AUTOFETCH=ON` to `FetchContent_Declare` as a fallback.
- **Backward compat for users:** zero — the public surface (`AudioAnalyzer`, `AudioCapture`) is unchanged. No `.qxw` format changes. No plugin API changes.
