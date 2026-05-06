# Phase 0 Review — Cross-Report Consistency Check

**Reviewer:** GPT 5.5 rubber-duck  
**Status:** 5 blocking issues, 4 non-blocking, 4 conflicts found

## Blocking Issues (must resolve before Phase 1)

1. **AudioFrame contract inconsistent** — capture audit uses `int16_t*`/`double*`, plan uses `float*`/`float*`. Need canonical struct.
2. **Legacy slider migration conflicts** — audioparams audit says exact preservation, profile design maps to new AGC/noise-gate fields. `presetFloor` is brightness floor, NOT noise gate.
3. **Perceptual band defaults conflict** — plan uses index ranges (sub=0..8), profile design uses Hz edges (sub<60Hz≈index 2). These don't match.
4. **Reactivity timebase unresolved** — old alpha applied at consumer frame rate (~25Hz), new envelopes at audio frame rate (~43Hz). Need conversion formula.
5. **DSP formulas underspecified** — can't write deterministic tests without exact formulas for dB floor, centroid, rolloff, flatness, flux.

## Non-Blocking Issues

6. Variable-band support not integrated into analyzer design
7. Silence-gate: analyzer must process silent frames (decay envelopes, advance cooldowns)
8. Legacy `audio.beat` is consumed-on-read; new triggers are frame-stable — keep both
9. AudioProfile should not own channel handle until Phase 2A (keep Phase 1 independent)

## Required Pre-Phase-1 Resolutions

See full review for details on each.
