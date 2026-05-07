# Review: External aubio migration

Reviewer: GPT 5.5 | Implementation: Opus 4.7

## Verdict: ✅ No blocking issues

### All checks pass
- ✅ Bundled sources deleted (118 files, only CMakeLists.txt remains)
- ✅ External aubio resolves at `/Users/abossard/Desktop/projects/aubio/src`
- ✅ Include shim generates forwarding `<aubio/aubio.h>`
- ✅ Zero QLC+ code changes needed
- ✅ Build passed (qlcplus-qml)
- ✅ Smoke test passed (all aubio objects create/destroy OK)
- ✅ io/synth/effects excluded correctly
- ✅ Accelerate FFT backend on macOS

### Non-blocking
1. Hard-coded path not portable (fine for private research)
2. Generated aubio.h exposes APIs whose impl is excluded (io/synth) — safe now, document for future

### Suggestion
Add CMake guardrails against future glob picking up unwanted dirs
