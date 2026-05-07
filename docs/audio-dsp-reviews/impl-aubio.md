# Implementation: External aubio migration (Phase 1)

Migrated QLC+ from a vendored copy of aubio 0.4.9 (under `engine/audio/aubio/`)
to building against an external aubio 0.5.0-alpha source tree at
`/Users/abossard/Desktop/projects/aubio/src`.

This is the **prototype phase** from the review's Issue #2: a CMake path
override against a local checkout. Submodule / vendored snapshot / FetchContent
choice is deferred. License concerns (Issue #1) are out of scope for this
private fork per the task spec.

## What changed

### `engine/audio/aubio/CMakeLists.txt` — rewritten

- Adds cache variable `AUBIO_SOURCE_DIR` (default
  `/Users/abossard/Desktop/projects/aubio/src`) — single, consistent name
  per review Issue #3.
- Validates `${AUBIO_SOURCE_DIR}/aubio.h` exists; fatal error otherwise.
- Globs `${AUBIO_SOURCE_DIR}/*.c` recursively with `CONFIGURE_DEPENDS`.
- Excludes:
  - `io/` — audio I/O handled by QLC+
  - `synth/` — sampler/wavetable not used
  - `effects/` — new in 0.5.x; pulls in librubberband
  - `dct_fftw.c`, `dct_ipp.c` — external libs we don't link
  - `windll.c` — Windows DLL entry (now lives in `utils/`)
  - Non-platform DCT backend (`dct_ooura.c` on Apple, `dct_accelerate.c`
    elsewhere). `dct_plain.c` kept on every platform as fallback.
- New 0.5.x file `spectral/awhitening.c` is picked up automatically by the
  glob. `onset.c` in 0.5.x depends on it, so this is required.
- Generates an include shim at
  `${CMAKE_CURRENT_BINARY_DIR}/aubio_include/aubio/aubio.h` via
  `file(WRITE …)`. The shim is a single forwarding header; **no symlinks**
  (per review Issue #4, Windows-safe).
- `target_include_directories`:
  - PUBLIC = generated shim dir → consumers keep `#include <aubio/aubio.h>`.
  - PRIVATE = `${AUBIO_SOURCE_DIR}` → internal sources find `aubio_priv.h`,
    `fvec.h`, `spectral/fft.h`, etc.
- Compile defines unchanged: `HAVE_*=1`, `AUBIO_LOG_DOMAIN="qlcplus"`,
  `HAVE_ACCELERATE=1` on Apple, `HAVE_WIN_HACKS=1` on Windows. We
  deliberately do not define `HAVE_CONFIG_H`.
- Apple links `-framework Accelerate`. Ooura fallback for non-Apple is kept
  by including `dct_plain.c` + `ooura_fft8g.c` (auto-globbed) and excluding
  `dct_accelerate.c`.
- Vendored-code warning silencing kept (`-w` / `/w`).
- Explicit comment: do **not** call upstream's root or `src/CMakeLists.txt`
  (per review Issue #5) — they are SHARED-only, hard-require PkgConfig,
  list I/O and effects sources unconditionally, and don't wire FFT config.

### Bundled source removed

Deleted everything under `engine/audio/aubio/` **except**
`CMakeLists.txt` (~110 files: the full src/ tree of aubio 0.4.9 plus
`AUTHORS`, `COPYING`, `VERSION`, `wscript_build`).

## Code changes (none)

`engine/audio/src/aubioprocessor.{h,cpp}` continues to use only public aubio
APIs via `#include <aubio/aubio.h>`. All function signatures used
(`aubio_onset_*`, `aubio_pitch_*`, `aubio_tempo_*` including
`aubio_tempo_set_tatum_signature` and `aubio_tempo_was_tatum`,
`aubio_notes_*`, `aubio_mfcc_*`, `aubio_filterbank_*`, `aubio_pvoc_*`,
`aubio_specdesc_*`, `aubio_tss_*`) exist and are signature-compatible in
0.5.0-alpha. No wrapper changes needed.

## Build verification

From `build/` after `cmake .. -Dqmlui=ON`:

| Target              | Result   |
|---------------------|----------|
| `qlcplusaubio`      | ✅ clean (Accelerate backend, all modules) |
| `qlcplus-qml`       | ✅ links and builds |
| `aubio_smoke_test`  | ✅ builds + runs: all 17 aubio objects create/destroy OK |

```
$ ./engine/audio/test/aubio_smoke_test
pvoc: OK
tempo: OK
pitch: OK
onset(energy/hfc/complex/specflux/phase): OK
mfcc: OK
filterbank: OK
specdesc(centroid/spread/rolloff/specflux/hfc): OK
tss: OK
notes: OK
All aubio objects created and destroyed successfully!
```

## Tests not run

`audioanalyzer_test` and `audiochannel_test` **fail to compile** due to
pre-existing references to `AudioFrame::spectralFlatness`,
`spectralFlux`, `spectralCentroidHz`, `spectralRolloffHz` etc. — fields
that are not present on `AudioFrame`. Verified by checking out the
unmodified tree (`git stash` → rebuild → same errors), so these failures
are **independent of the aubio migration**. They should be addressed in a
separate task focused on the AudioFrame/AnalyzerHints refactor.

## How to override the path

```bash
cd build
cmake .. -Dqmlui=ON -DAUBIO_SOURCE_DIR=/path/to/aubio/src
cmake --build . --target qlcplus-qml -j8
```

If `AUBIO_SOURCE_DIR/aubio.h` does not exist, configuration fails with a
helpful message.

## Review issues addressed

| # | Issue                                       | Status |
|---|---------------------------------------------|--------|
| 1 | License compatibility (GPL-3 vs Apache-2.0) | Out of scope (private research fork) |
| 2 | Submodule overcommit                        | ✅ Phase 1 path-override only |
| 3 | `AUBIO_SOURCE_DIR` vs `AUBIO_SRC_DIR` naming| ✅ Single `AUBIO_SOURCE_DIR` cache var |
| 4 | Windows-unsafe symlink shim                 | ✅ `file(WRITE)` forwarding header instead |
| 5 | Don't drive upstream CMake                  | ✅ Explicit comment, source tree only |
| 6 | CI/CD vagueness                             | Deferred (Phase 2) |
| 7 | Pin commit                                  | N/A — local path override only |
| 8 | DSP regression tests                        | Deferred (separate AudioFrame test fix needed first) |
| 9 | Preserve FFT/backend behavior               | ✅ Apple→Accelerate, others→Ooura+plain DCT |
| 10| Upstream contribution                       | N/A |

## Next steps (not done)

- Fix the broken `AudioFrame` references in `audioanalyzer_test` /
  `audiochannel_test` (pre-existing, unrelated).
- Add deterministic DSP regression tests (review Issue #8).
- Decide between submodule / vendored snapshot / FetchContent for the
  committed integration form (review Issue #2 phase 2).
- License review before any public release (review Issue #1).
