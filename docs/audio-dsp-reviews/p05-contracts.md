# P0.5 Contracts — Canonical DSP Definitions

> Resolves the 5 blocking inconsistencies found in the Phase 0 rubber-duck review.
> All values verified against `engine/audio/src/audiocapture.{h,cpp}` (sample-by-sample audit on
> the current `main`). Phase 1 implementations MUST match these contracts exactly.

Audit constants (from header):

| Constant | Value | Notes |
|---|---|---|
| `AUDIO_DEFAULT_SAMPLE_RATE` | `44100` Hz | overridable via `QSettings` |
| `AUDIO_DEFAULT_BUFFER_SIZE` | `2048` samples/channel | aka `m_bufferSize` (FFT size) |
| `AUDIO_DEFAULT_CHANNELS` | `1` | mixdown is always mono |
| `SPECTRUM_MIN_FREQUENCY` | `40` Hz | analyzer lower bound |
| `SPECTRUM_MAX_FREQUENCY` | `5000` Hz | analyzer upper bound |
| `FREQ_SUBBANDS_MAX_NUMBER` | `32` | hard cap |
| `FREQ_SUBBANDS_DEFAULT_NUMBER` | `16` | UI default |

Frame rate at defaults: `44100 / 2048 ≈ 21.53 fps` (~46.44 ms/frame). Phase 1 envelopes
operate at this rate, **not** at the legacy 25 Hz consumer tick.

---

## 1. Canonical `AudioFrame` struct

Verified types in `audiocapture.cpp`:

- `m_audioMixdown` is **`int16_t*`** (header line 178). Mono mixdown samples are int16, range
  `[-32768, +32767]`.
- The DSP working buffer `m_fftInputBuffer` is **`double*`** (line 184), filled with mixdown
  samples normalized to `[-1.0, +1.0]` via `(s - mean) / 32768.0` (cpp line 293).
- FFTW r2c output `m_fftOutputBuffer` is **`fftw_complex` (a.k.a. `double[2]`)**; per-bin
  magnitude is `sqrt(re² + im²)` and is held as **`double`** in `BandsData::m_fftMagnitudeBuffer`
  (`QVector<double>`, header line 56).
- Valid FFT bin count for a real-input length-N transform is `N/2 + 1`. For default
  `N = m_bufferSize = 2048`, that is **1025 bins** (DC … Nyquist). The current code uses
  `maxBin = N/2 = 1024` and iterates `[1 .. maxBin+1)` which covers bins 1..1024 (skips DC and
  Nyquist alias), so the **usable bin count is 1024** in practice.

Canonical struct (final):

```cpp
// engine/audio/src/audioframe.h
#ifndef AUDIOFRAME_H
#define AUDIOFRAME_H

#include <cstdint>
#include <vector>

namespace QLCPlus { namespace Audio {

/// One block of analyzed audio. Produced by AudioAnalyzer once per FFT hop.
/// Lifetime: owned by the analyzer; consumers must copy if they need to retain.
struct AudioFrame
{
    // ---- Identity & timing ---------------------------------------------------
    quint64        frameIndex   = 0;     ///< Monotonic, increments every frame, never skips on silence.
    qint64         hostTimeNs   = 0;     ///< CLOCK_MONOTONIC nanoseconds at end-of-capture.
    quint32        sampleRate   = 44100; ///< Hz, snapshot of capture settings.
    quint32        fftSize      = 2048;  ///< Time-domain window length N (== m_bufferSize).
    quint32        binCount     = 1025;  ///< == fftSize/2 + 1 (DC..Nyquist, FFTW r2c convention).
    bool           silent       = false; ///< See "Silence handling" §5.

    // ---- Time-domain (mono mixdown after DC removal, normalized) -------------
    /// Float copy of the windowed mono mixdown, range [-1.0, +1.0]. Length == fftSize.
    /// Source bytes are int16 in m_audioMixdown; this is the normalized double form.
    std::vector<double> samples;

    double         rms          = 0.0;   ///< Linear, [0..1], pre-window, post-DC-removal.
    double         peak         = 0.0;   ///< Linear, [0..1], abs-max of `samples`.
    double         dcOffset     = 0.0;   ///< Mean of mixdown / 32768, [-1..1] (rarely nonzero post-removal).

    // ---- Frequency-domain ----------------------------------------------------
    /// Magnitude per bin, length == binCount. magnitudes[k] = sqrt(re²+im²).
    /// Bin k corresponds to frequency  k * sampleRate / fftSize.
    /// Units: linear amplitude, NOT normalized (raw FFTW output magnitude).
    std::vector<double> magnitudes;

    /// 32 log-spaced bands covering 40..5000 Hz. See §2 for index layout.
    /// Units match `fillBandsData()` exactly: averaged linear magnitude / (bandWidth * 2π).
    std::vector<double> bands32;

    // ---- Scalar features (see §4 for formulas) -------------------------------
    double rmsDb            = -96.0;
    double peakDb           = -96.0;
    double crestFactor      = 1.0;
    double spectralFlux     = 0.0;
    double spectralCentroidHz = 0.0;
    double spectralRolloffHz = 0.0;
    double spectralFlatness = 0.0;
    double noiseFloorDb     = -96.0;

    // ---- Beat (forwarded from BeatTracker) -----------------------------------
    bool   beatDetected     = false;
};

}} // namespace
#endif
```

Notes:

- **Type promotion `int16 → double`** happens once, at the analyzer entry (the FFT input
  buffer is already `double` in legacy code, so no change in numerical fidelity).
- `magnitudes.size() == binCount` (1025 at defaults) — full r2c output is exposed so
  consumers can compute their own band layouts. `bands32` is the canonical pre-computed
  layout for envelopes / triggers.
- All scalar features default to "silence-sane" values (see §4 zero-handling column).
- Struct is **copyable** but expensive (~25 KB at default sizes). Consumers should hold
  references during the analyzer callback and copy only on demand.

---

## 2. Band layout contract — Hz authoritative, indices derived

**Authoritative form is Hz.** Sample-rate independence means every consumer (RGBMatrix
script, VCAudioTrigger, scope view) refers to the same musical band even if the user later
changes sample rate or FFT size. Indices are recomputed at `AudioFrame` build time.

### 2.1 32-band log-spaced layout (spans 40 → 5000 Hz)

Centers computed with `f_b = 40·exp(ln(125)·b/32)`. Edges `[f_b, f_{b+1})`:

| idx | start Hz | end Hz | center Hz | idx | start Hz | end Hz | center Hz |
|----:|---------:|-------:|----------:|----:|---------:|-------:|----------:|
|   0 |   40.00  |   46.51 |    43.13 |  16 |  447.21  |  520.05 |   482.26 |
|   1 |   46.51  |   54.09 |    50.16 |  17 |  520.05  |  604.74 |   560.80 |
|   2 |   54.09  |   62.90 |    58.33 |  18 |  604.74  |  703.23 |   652.13 |
|   3 |   62.90  |   73.14 |    67.83 |  19 |  703.23  |  817.77 |   758.34 |
|   4 |   73.14  |   85.06 |    78.87 |  20 |  817.77  |  950.95 |   881.85 |
|   5 |   85.06  |   98.91 |    91.72 |  21 |  950.95  | 1105.82 |  1025.47 |
|   6 |   98.91  |  115.02 |   106.66 |  22 | 1105.82  | 1285.92 |  1192.48 |
|   7 |  115.02  |  133.75 |   124.03 |  23 | 1285.92  | 1495.35 |  1386.69 |
|   8 |  133.75  |  155.53 |   144.23 |  24 | 1495.35  | 1738.89 |  1612.53 |
|   9 |  155.53  |  180.86 |   167.72 |  25 | 1738.89  | 2022.08 |  1875.15 |
|  10 |  180.86  |  210.32 |   195.03 |  26 | 2022.08  | 2351.41 |  2180.54 |
|  11 |  210.32  |  244.57 |   226.80 |  27 | 2351.41  | 2734.36 |  2535.67 |
|  12 |  244.57  |  284.40 |   263.73 |  28 | 2734.36  | 3179.69 |  2948.63 |
|  13 |  284.40  |  330.72 |   306.69 |  29 | 3179.69  | 3697.54 |  3428.85 |
|  14 |  330.72  |  384.58 |   356.63 |  30 | 3697.54  | 4299.73 |  3987.29 |
|  15 |  384.58  |  447.21 |   414.72 |  31 | 4299.73  | 5000.00 |  4636.67 |

### 2.2 Default 5-band semantic layout (`AudioChannelConfig`)

Hz edges chosen to (a) make musical sense and (b) preserve the legacy
`lowCutBin(32)=12` (250 Hz) and `highCutBin(32)=26` (2000 Hz) cut points exactly.

| Band     | Hz range      | 32-band index range (half-open) | Width | Notes |
|----------|---------------|---------------------------------|-------|-------|
| `Sub`    | 40 – 60 Hz    | `[0, 3)`                        | 3     | Boundary at b=2.687 → ceil = 3. |
| `Bass`   | 60 – 250 Hz   | `[3, 12)`                       | 9     | Upper boundary = legacy `lowCutBin(32)`. |
| `LowMid` | 250 – 500 Hz  | `[12, 17)`                      | 5     | Boundary at b=16.74 → ceil = 17. |
| `Mid`    | 500 – 2000 Hz | `[17, 26)`                      | 9     | Upper boundary = legacy `highCutBin(32)`. |
| `High`   | 2000 – 5000 Hz| `[26, 32)`                      | 6     | Tail. |

### 2.3 Legacy-compat aliases (for scripts that called `lowCutBin`/`highCutBin`)

| Alias  | Hz range      | 32-band index range | Composition         |
|--------|---------------|---------------------|---------------------|
| `low`  | 40 – 250 Hz   | `[0, 12)`           | `Sub + Bass`        |
| `mid`  | 250 – 2000 Hz | `[12, 26)`          | `LowMid + Mid`      |
| `high` | 2000 – 5000 Hz| `[26, 32)`          | `High`              |

These aliases are bit-exact equivalents of `AudioCapture::lowCutBin(32)` /
`highCutBin(32)`, so `RGBScript`s that previously did
`magnitudeSum[0..lowCutBin(32))` get identical output by reading `low`.

### 2.4 Index derivation rule (any N, any sampleRate)

```cpp
// Pseudocode; matches fillBandsData() semantics
int bandStart(double f, int N) {
    return std::clamp(int(N * std::log(f / 40.0) / std::log(125.0)), 0, N);
}
// Use floor for the lower edge of an inclusive range, ceil for the upper edge.
```

---

## 3. Legacy migration contract

All four legacy `presetXxx` sliders are mapped 1:1 to fields on `AudioChannelConfig`.
The rubber-duck review found that two of them (`presetFloor`, `presetReactivity`) had
ambiguous targets — resolved here.

| Legacy slider | Slider range | Legacy formula | New field (location) | New value formula | Notes |
|---|---|---|---|---|---|
| `presetGain` | `1..10` | `gainFactor = 0.6 + v·0.2`  → `[0.8, 2.6]` | `AudioChannelConfig::inputGainLinear` | `0.6 + clamp(v,1,10)·0.2` | Default `v=5 → 1.6`. Optionally also store `inputGainDb = 20·log10(linear)` for UI display, but `inputGainLinear` is authoritative. |
| `presetReactivity` | `1..10` | `filterRise = 0.1 + v·0.09` → `[0.19, 1.0]` (one-pole α at 25 Hz tick) | `EnvelopeConfig::attackMs` (per band) | `attackMs = -40 / ln(1 - α)` where `α = 0.1 + v·0.09`; clamp `α ≤ 0.999` to avoid `ln(0)`. | See §3.1 table. The conversion uses the **OLD** 40 ms tick because that is the rate at which the legacy alpha was tuned. The resulting `attackMs` is then re-applied at the new analyzer rate (`fftSize/sampleRate ≈ 46.4 ms`) — i.e., it stays a time constant, the rate it gets sampled at changes. `releaseMs` defaults from the script's per-channel base decay (legacy `createFilter(decay, rise)`); when no script default exists, use `releaseMs = 4·attackMs`. |
| `presetFloor` | `0..100` | `applyFloor(b) = floor + (1-floor)·b` (BRIGHTNESS floor on visual output, NOT a noise gate) | `AudioChannelConfig::brightnessFloor` (new field, `double` in `[0,1]`) | `clamp(v,0,100) / 100.0` | **Resolved**: `presetFloor` is a visual-output minimum, applied AFTER envelope/AGC by the consumer (RGBMatrix script). It is NOT `NoiseGateConfig::thresholdDb`. The two are independent: `noiseGate` silences input below a dB threshold; `brightnessFloor` lifts output above a fractional minimum. Add `double brightnessFloor = 0.0;` to `AudioChannelConfig`. Only `audiosplittower.js` uses non-zero default (`15 → 0.15`). |
| `presetSensitivity` | `1..10` | `triggerThreshold = 0.45 - v·0.04` → `[0.41, 0.05]` | `TriggerConfig::highThreshold` | `0.45 - clamp(v,1,10)·0.04` | Direct mapping. `lowThreshold = max(0.0, highThreshold - 0.20)` for hysteresis (matches Schmitt-trigger behavior expected by current scripts). |
| `audiobuildup.js` extra | `1..10` | build-state thresholds | `TriggerConfig` (script-specific overrides) | `buildEnter = lerp(0.65, 0.35, v/10)`, `peak = lerp(0.80, 0.55, v/10)` | This script bypasses `triggerThreshold()`; preserve the two extra thresholds in a `TriggerConfig::extra` map keyed by name. |

### 3.1 Reactivity α → attackMs lookup (at 40 ms legacy tick)

| `presetReactivity` | α (legacy) | `attackMs` (Phase 1) |
|:---:|:---:|---:|
|  1 | 0.19 | 189.82 |
|  2 | 0.28 | 121.76 |
|  3 | 0.37 |  86.57 |
|  4 | 0.46 |  64.92 |
|  5 | 0.55 |  50.09 |
|  6 | 0.64 |  39.15 |
|  7 | 0.73 |  30.55 |
|  8 | 0.82 |  23.33 |
|  9 | 0.91 |  16.61 |
| 10 | 0.999¹ |  ≈ 5.79 |

¹ Clamped from raw `α=1.0` (which would give `ln(0)`) to `α=0.999` to avoid singularity;
yields a near-instant attack equivalent to legacy "always follow current sample".

---

## 4. DSP formula contract

Every feature is a **pure function** of one or more `AudioFrame` fields. Inputs are the
linear-amplitude `samples` and `magnitudes`. Output domains are documented so consumers
can rely on safe ranges without runtime checks.

| Feature | Formula | Input domain | Output range | Zero-handling (silent frame) |
|---|---|---|---|---|
| `rms` | `sqrt( Σ(x_i - mean)² / N )` | `samples ∈ [-1, 1]`, `N = fftSize` | `[0, 1]` | Actual near-zero value. |
| `peak` | `max( |x_i| )` | `samples ∈ [-1, 1]` | `[0, 1]` | Actual near-zero value. |
| `rmsDb` | `20·log10(max(rms, 10^-4.8))` ⇒ `max(20·log10(rms), -96)` | `rms ∈ [0, 1]` | `[-96, 0]` dBFS | `-96` dB (clamp floor). |
| `peakDb` | `20·log10(max(peak, 10^-4.8))` ⇒ same clamp | `peak ∈ [0, 1]` | `[-96, 0]` dBFS | `-96` dB. |
| `crestFactor` | `peak / max(rms, ε)`, `ε = 1e-9` | `rms, peak ∈ [0, 1]` | `[1.0, ~∞)` (typically `[1, 30]`) | `1.0` (silence convention). |
| `spectralFlux` | `Σ_k max(0, |M_k[t]| - |M_k[t-1]|) / Σ_k |M_k[t-1]|` over bins covering 40..5000 Hz | `magnitudes ≥ 0` | `[0, ~2]` (rare > 1 on transients) | `0.0`. Previous-frame buffer initialized to zeros. |
| `spectralCentroidHz` | `Σ_k (f_k · |M_k|) / Σ_k |M_k|` over bins where `40 ≤ f_k ≤ 5000` | `magnitudes ≥ 0`, `f_k = k·SR/N` | `[40, 5000]` Hz | `0.0` Hz (sentinel: "no centroid defined"). |
| `spectralRolloffHz` | smallest `f_k` such that `Σ_{j ≤ k} |M_j|² ≥ 0.85·Σ_all |M_j|²`, restricted to 40..5000 Hz band | `magnitudes ≥ 0` | `[40, 5000]` Hz | `0.0` Hz (sentinel). |
| `spectralFlatness` | `exp(mean(ln(|M_k| + ε))) / (mean(|M_k|) + ε)`, `ε = 1e-10`, over 40..5000 Hz | `magnitudes ≥ 0` | `[0, 1]` (1 = white, 0 = pure tone) | `1.0` (silent ≈ uniform near-zero ≈ flat). |
| `noiseFloorDb` | adaptive: `nf[t] = min(rmsDb[t], nf[t-1] + r·dt)` with **slow-release** rate `r = +6 dB/s` (rises slowly, snaps down instantly to current `rmsDb`) | `rmsDb ∈ [-96, 0]` | `[-96, 0]` dB | Ratchets toward current `rmsDb` (silent frames pull it down to `-96`). Init `nf[0] = -60`. |
| `bands32[b]` | `(Σ_{k∈[s_b, e_b)} |M_k|) / ((e_b - s_b) · 2π)` (matches `fillBandsData()` exactly) | `magnitudes ≥ 0`, `s_b, e_b` from §2.4 | `[0, ~∞)` raw | `0.0`. |

ε constants are spec-mandated to keep cross-platform reproducibility; do not tune locally.

---

## 5. Silence-handling rules

The analyzer **MUST** be invoked on every captured block, including silent ones.
No early-return; the only branch is value-vs-decay logic.

### 5.1 "Silent frame" definition (boolean `AudioFrame::silent`)

A frame is silent **iff BOTH** conditions hold:

1. `rms < kSilenceRms` where `kSilenceRms = 0.002` (matches legacy `audiocapture.cpp:301`,
   ≈ −54 dBFS).
2. `max(magnitudes) < kSilenceMag` where `kSilenceMag = 1e-6` (well below FFT noise floor
   for normalized double inputs).

Both checks are required because (a) silence may have residual DC that lifts `rms` while
the FFT is empty, and (b) high-frequency hiss can lift FFT bins while RMS is below gate.

### 5.2 Required behavior on silent frames

| Action | Required |
|---|---|
| Increment `frameIndex` | **Yes** — frame counter is monotonic, never skips. |
| Populate `samples` | **Yes** — actual (near-zero) data; consumers may need it for waveform display. |
| Populate `magnitudes`, `bands32` | **Yes** — actual (near-zero) values. Do **not** zero-fill: consumers reading raw FFT may rely on real noise floor. (The legacy code does zero-fill `bands32` in this branch; Phase 1 keeps the actual values for analyzer fidelity, and the noise gate at `AudioChannelConfig::noiseGate` is what suppresses output.) |
| Set `rms`, `peak` | **Yes** — actual measured values. |
| Set scalar features (`rmsDb`, `crestFactor`, …) | **Yes** — apply zero-handling per §4 column 5. |
| Decay envelopes (consumer side) | **Yes** — envelope filters tick by `releaseMs`. |
| Advance trigger cooldowns | **Yes** — `holdMs`, `cooldownMs` countdown continues. |
| Fire triggers | **No** — silent frames must not raise `highThreshold` events. |
| Update `noiseFloorDb` tracker | **Yes** — silent frames are exactly when the tracker should pull down. |
| Forward `beatDetected` from BeatTracker | **Yes**, but BeatTracker itself returns false on silence (verified in `beattracker.cpp`). |
| Emit `AudioFrame` to subscribers | **Yes** — exactly one frame per capture block, silent or not. |

### 5.3 Rationale

The legacy code (`audiocapture.cpp:302`) `return`s early on `rms < kSilenceRms`, which
breaks: (a) cooldown advance (triggers stay armed across long silences and re-fire on
first sound), (b) noise-floor tracking, (c) frame-rate consistency for downstream
visualizers. Phase 1 must process silent frames fully.

---

## 6. Phase 1 test expectations

Synthetic-input vectors used by `engine/audio/test/audioanalyzer_test.cpp`. All values
assume `sampleRate = 44100`, `fftSize = 2048`, Hanning window. Tolerances stated per row.

### 6.1 Pure silence (all samples = 0)

| Field | Expected | Tolerance |
|---|---|---|
| `silent` | `true` | exact |
| `rms` | `0.0` | `< 1e-12` |
| `peak` | `0.0` | exact |
| `rmsDb` | `-96.0` | exact (clamp) |
| `peakDb` | `-96.0` | exact |
| `crestFactor` | `1.0` | exact |
| `spectralFlux` | `0.0` | `< 1e-9` |
| `spectralCentroidHz` | `0.0` | exact (sentinel) |
| `spectralRolloffHz` | `0.0` | exact |
| `spectralFlatness` | `1.0` | `< 1e-6` |
| `noiseFloorDb` | tracks down toward `-96` | reaches `≤ -90` within 200 frames |
| `magnitudes[k]` | `0.0` | `< 1e-9` ∀k |
| `bands32[b]` | `0.0` | `< 1e-9` ∀b |
| `beatDetected` | `false` | exact |

### 6.2 1 kHz sine at −20 dBFS (`x[n] = 0.1 · sin(2π · 1000 · n / 44100)`)

Bin index of 1 kHz at `N=2048`: `1000·2048/44100 ≈ 46.44` → energy concentrated in
bins 46–47.

| Field | Expected | Tolerance |
|---|---|---|
| `silent` | `false` | exact |
| `rms` | `0.1 / √2 ≈ 0.0707` | `±5%` |
| `peak` | `≈ 0.10` | `±5%` |
| `rmsDb` | `≈ -23.0` (Hanning is ~0.5 of peak so post-window RMS is lower) | `±2 dB` |
| `peakDb` | `≈ -20` | `±0.5 dB` |
| `crestFactor` | `≈ √2 ≈ 1.414` | `±10%` |
| `spectralFlux` | `≈ 0` after warm-up (steady tone) | `< 0.05` after frame 5 |
| `spectralCentroidHz` | `≈ 1000` | `±50 Hz` |
| `spectralRolloffHz` | `≈ 1000` (single tone, all energy in one bin) | `±100 Hz` |
| `spectralFlatness` | `< 0.05` (pure tone → very non-flat) | exact bound |
| `bands32[21]` (centered 1025 Hz) | dominant | argmax of `bands32` ∈ {20,21} |
| `bands32[b]` for `b ∉ {20,21,22}` | `< 5%` of peak band | exact bound |

### 6.3 White noise, full-scale (`x[n] ∼ Uniform(-1, 1)`)

| Field | Expected | Tolerance |
|---|---|---|
| `silent` | `false` | exact |
| `rms` | `≈ 1/√3 ≈ 0.577` | `±5%` |
| `peak` | `≈ 1.0` | `±2%` |
| `crestFactor` | `≈ 1.73` | `±15%` |
| `spectralFlatness` | `> 0.5` (white = high flatness) | exact bound |
| `spectralCentroidHz` | `≈ geometric mid of 40..5000 ≈ 447 Hz` (log-spaced bin weighting) | `±150 Hz` |
| `spectralRolloffHz` | `≈ 4250` (85% of cumulative energy in flat spectrum lands near `0.85·5000`) | `±300 Hz` |
| `bands32` | roughly equal across bands (variance / mean `< 0.6`) | bound |

### 6.4 Click / impulse (`x[0] = 1.0`, rest = 0)

| Field | Expected | Tolerance |
|---|---|---|
| `silent` | `false` | exact |
| `rms` | `≈ 1/√2048 ≈ 0.0221` | `±5%` |
| `peak` | `≈ 1.0` | exact |
| `crestFactor` | `≈ 45` (very high — defining feature of impulses) | `> 20` |
| `spectralFlatness` | `≈ 1.0` (impulse → flat spectrum) | `> 0.7` |
| `spectralFlux` (frame containing impulse, vs. silent prev) | high | `> 0.5` |

### 6.5 Step from silence to 1 kHz sine (transient test)

| Frame | `spectralFlux` |
|---|---|
| Before onset (silent) | `0.0` |
| Onset frame | `> 0.3` |
| Frames 2–N after onset (steady) | `< 0.05` |

### 6.6 Legacy-compat regression: `bands32` matches `fillBandsData(32)`

For the same input buffer, `AudioFrame::bands32[b]` MUST equal
`AudioCapture::fillBandsData(32)`'s output buffer at `[b]` to within `< 1e-9` per element.
This is the bit-exact contract that lets existing RGB scripts read `bands32` and behave
identically to the legacy `bandMagnitude(b, 32)` call.

### 6.7 Legacy-compat regression: cut bins

| `AudioCapture::lowCutBin(N)` | Expected (per §2.3) |
|---|---|
| `N = 16` | `6` |
| `N = 32` | `12` |
| `N = 64` | `24` |

| `AudioCapture::highCutBin(N)` | Expected |
|---|---|
| `N = 16` | `13` |
| `N = 32` | `26` |
| `N = 64` | `52` |

Phase 1 must keep these helpers (or document a 1:1 replacement that returns identical values).

---

## Cross-reference

- Audit: `p0-audit-capture.md` §2 (FFTW types), §4 (silence gate)
- Audit: `p0-audit-audioparams.md` §3.1 (gain), §3.3 (reactivity), §3.4 (floor), §3.5 (sensitivity)
- Design: `p0-design-profile.md` §AudioChannelConfig (now adds `brightnessFloor`)
- Inventory: `p0-inventory-scripts.md` (script-by-script default values feeding §3.1 table)
