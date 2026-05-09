# Aubio Audio Guide for QLC+

Practical guide to QLC+'s aubio-based audio analysis: what each feature gives you, what to configure, and what it looks like on stage.

QLC+ runs aubio at fixed **window = 1024, hop = 512** (`AubioProcessor::windowSize()` / `hopSize()`). At 44.1 kHz: **~11.6 ms hop, ~23.2 ms window**. Mel filterbank produces **40 bands** (`AUBIO_MEL_BANDS`); MFCC gives **13 coefficients**.

Source: `engine/audio/src/aubioprocessor.{h,cpp}`, `aubioresults.h`, `audiochannelconfig.h`.

---

## Quick Start (5 steps)

1. **Pick an input device** in QLC+ audio settings. Use a wired interface or system loopback. QLC+ does not apply pre-aubio gain — set OS/interface input gain so program material sits around -25 to -15 dBFS.
2. **Confirm capture is mono** — all channels are averaged to mono before aubio (`AudioCapture::processData`). Send a mono aux when possible.
3. **Start with these defaults** for any reactive lighting:
   - Onset: `complex`, threshold 0.3, minioi 80 ms, silence -65 dB, adaptive whitening on
   - Tempo: `default` (maps to `specflux`), threshold 0.3, silence -90 dB, tatum 4
4. **Play a known track for 10+ seconds** so the tempo tracker locks (it accumulates ~5.8 s of detection-function before committing).
5. **Tune by symptom** — see the [Troubleshooting](#troubleshooting) table.

---

## 1. Features at a Glance

| Feature | What you get | Typical lighting use |
|---|---|---|
| `aubio_onset` ×9 | Boolean trigger flags per detector | Strobes, flashes, hit chases |
| `aubio_tempo` | BPM, beat flag, beat phase, tatum | BPM-locked chases, sub-beat shimmer |
| `aubio_pitch` | f0 (Hz) + confidence | Color/position from key (mono sources only) |
| `aubio_notes` | MIDI note on/off | Note-driven cues, MIDI bridge |
| `aubio_filterbank` | **40 mel bands** | **Spectrum-to-gradient visualizers** (see §3) |
| `aubio_mfcc` | 13 timbre coefficients | Timbre clustering (advanced) |
| `aubio_specdesc` | centroid, spread, rolloff, flux, hfc | Color temperature, intensity LFO |
| `aubio_tss` | Transient/steady split | Strobe on transients, wash on steady |

QLC+ wires a broad aubio surface but not all of it — `decrease`, `kurtosis`, `skewness`, `slope` descriptors are not exposed; aubio's file source/sink and synth side aren't used.

---

## 2. Onset Detection

All 9 detectors run in parallel and share **one** `AubioConfig` (threshold, minioi, silence, whitening, compression). Pick the detector(s) you trust per program type.

| Method | Picks up | Good for |
|---|---|---|
| `energy` | Local spectral-frame energy | Loud isolated hits in quiet mix |
| `hfc` | High-frequency-weighted bins | Snare, claps, cymbals, sharp transients (not a fixed band-pass) |
| `complex` | Magnitude + phase difference | General-purpose mixed material |
| `phase` / `wphase` | Phase deviation | Tonal onsets (piano, vocals) |
| `specdiff` | Magnitude difference | Mixed material |
| `kl` / `mkl` | Kullback-Liebler divergence | Pitched + percussive mix |
| `specflux` | Spectral flux | Percussive in busy mixes; **aubio's tempo "default"** uses this |

### Important: aubio defaults vs QLC+ defaults

Aubio's `new_aubio_onset()` followed by `aubio_onset_set_default_parameters()` applies *method-specific* defaults (e.g. `complex` ships with threshold 0.15 + whitening + compression 1; `specflux` with 0.18 + whitening + compression 10; `energy`/`phase` no compression or whitening). QLC+ now keeps those per-method defaults intact and only overrides the user-intent globals (threshold, silence, minioi, delay) on top. Threshold values you see here are **QLC+ presets**, not aubio canonical values, and are not directly comparable across methods until per-detector overrides are added.

### Tuning levers

- `onsetThreshold` — peak-pick threshold on the normalized descriptor. Higher = fewer onsets.
- `onsetMinIntervalMs` — gap between consecutive onsets. 30–50 ms hi-hats; 80–120 ms kicks/snares; 150 ms+ ballads.
- `onsetSilenceDb` — gate floor in dBFS. Lower (-90) = fire on quiet input; raise (-40) to ignore room noise.
- Compression and adaptive whitening are *not* exposed: aubio's per-method defaults are used as-is. Adding a global override would erase aubio's tuning for every method that ships with whitening/compression on (specflux, complex, kl, mkl).

### Kicks: pick a band, not a method

Full-band onsets do not isolate kicks. For kick triggers, use the **low band envelope** from `BandLayout` plus the TSS transient flag. Use full-band onsets for snare/clap/general accents.

### Scripts that use onsets
- Strobe on onset → `audiostrobe.js`
- One-shot triggered effects → `audioshot.js`, `audiobasslaser.js`, `audioshockwave.js`
- State-machine buildup driven by onset density → `audiobuildup.js`

---

## 3. Mel Bands as a Visual Spectrum (core pattern)

The 40 mel bands are the most useful aubio output for everyday lighting. Treat them as a **40-pixel frequency spectrum** updated every hop (~12 ms).

### The gradient pattern

```
band index (0..39)  →  position along a color gradient
band energy (0..1)  →  brightness / intensity at that position
```

Define a gradient (any palette works):

```
red → orange → yellow → green → cyan → blue → purple
 0.0    0.17     0.33    0.50    0.67   0.83    1.0
```

For each mel band `i`:
- `t = i / 39.0` → look up gradient color
- `energy = mel[i]` (normalize per-band; see below) → brightness multiplier
- Output color = `gradient(t) * energy`

Result: low frequencies on one end of the gradient, high on the other, intensity follows the music.

### Normalization

Raw mel values depend on `filterbankNorm` and `filterbankPower`. Two practical options:
- **Per-band running max** (preferred): keep an EMA of each band's recent peak, divide. Reacts to mix balance.
- **Global peak across all bands** with smoothing: simpler, but bass-heavy material washes out the highs.

### Mapping to fixture types

**LED strip (N pixels):** interpolate the 40 mels to N. For N=40, direct mapping. For N=144, linear-interpolate between bands. Gradient flows along the strip; the strip becomes a live spectrum.
- See `audiospectrum.js`, `audioequalizer.js` for working examples.

**LED matrix (W×H):**
- *Spectrum-bar mode*: X axis = frequency (W mel bins), Y axis = bar height from `mel[x]`. Gradient applied per-column or per-row.
- *Scrolling waterfall*: X axis = frequency, each frame shifts Y by 1, top row = current spectrum. Beautiful, sub-frame readable.
- See `audiospectrum.js`, `audiosplittower.js`.

**Moving heads / wash bars:**
- Pan/tilt from spectral centroid: low centroid → one position, high centroid → another.
- Brightness from a mel slice (e.g., sum of mels 5–15 for mid-band reactivity).
- Color from gradient indexed by centroid.

**Three-band blend** (cheap & effective): collapse 40 mels into low/mid/high sums, map each to an RGB channel.
- See `audioaurora.js`, `audioenergy.js`, `audiopower.js`.

### Tuning

- `filterbankPower` (1.0 default = magnitude, 2.0 = power): higher = more contrast between loud/quiet bands. Hot-reloadable.
- `filterbankNorm` (0 or 1): triggers full rebuild. Norm=1 normalizes filter areas (Slaney style).
- `melScale` ("htk" or "slaney"): triggers rebuild. HTK spaces bands more linearly at low freqs; Slaney is the default.

---

## 4. Tempo / Beat

### How it works (briefly)

`aubio_tempo` runs a spectral descriptor — **`"default"` maps to `specflux`** — into a peak-picker and a beat tracker. The tracker maintains a rolling **detection-function buffer** (~5.8 s at 44.1 kHz / hop 512) and runs autocorrelation/comb filtering on that buffer to estimate period and phase. It does **not** autocorrelate onset timestamps.

Practical implications:
- Needs ~5–10 s of stable input to converge. Don't bind chases until `bpm > 0` and confidence is non-trivial.
- The `tempoMethod` parameter defaults to `"default"` (→ `specflux`). Non-default values are passed through to aubio as spectral descriptor names — treat as experimental.
- The lag range easily covers 60–240 BPM. Half-time / double-time errors come from the tracker's bias toward a musically plausible tactus (around 120 BPM), not from autocorrelation range limits.

### Configuration

| Parameter | Default | Notes |
|---|---|---|
| `tempoThreshold` | 0.3 | Lower (0.1–0.2) → more sensitive, more octave errors. Higher (0.4–0.6) → cleaner, may miss soft beats. |
| `tempoSilenceDb` | -90 | Raise to -60 if room noise causes phantom beats during silence. |
| `tempoDelayMs` | 0 | Only set if you measured system latency (Bluetooth, network audio). |
| `tatumSubdivision` | 4 | 1–64. 4 = 16ths in 4/4; 8 = 32nds; 3/6 = triplet feel. |

### Beat phase

QLC+ derives `beatPhase ∈ [0, 1)` from `aubio_tempo_get_period_s` + `get_last_s` + the sample counter. It's smooth between beats. Use it to:
- Drive a BPM-locked sine/triangle LFO
- Anticipate the next beat (fade up at phase 0.9, hit on 0.0)
- Cross-fade scenes on bar boundaries

### Tatum

`aubio_tempo_was_tatum()` returns `2` on a beat boundary, `1` on a sub-beat, `0` otherwise. Drive *fast* effects from tatum (per-16th color flicker) and *slow* effects from beat (per-bar scene change). Best feel upgrade for any beat-locked rig.

### Scripts
- BPM-locked color blending → `audioaurora.js`, `audioenergy.js`
- Beat-step chasers → `audiochaser.js`, `audioscan.js`

---

## 5. Pitch & Notes

Useful only on **isolated mono sources** (vocals, lead synth, solo guitar). On a full mix it jitters.

- `pitchMethod`: `yinfft` (default, balanced), `yinfast` (cheap), `mcomb`, `yin`, `schmitt`, `fcomb`.
- Always gate on `pitchConfidence > 0.5` and silence threshold.
- `aubio_notes` quantizes pitch to MIDI with note-on/off — better than raw pitch for cue triggers.

Mapping:
```
hue        = (midi % 12) / 12.0
brightness = 0.3 + 0.7 * pitchConfidence
elevation  = clamp((midi - 36) / 60.0, 0, 1)
```

Don't use full-mix pitch. Stop reading this section if you're not feeding aubio a clean solo source.

---

## 6. Spectral Descriptors & TSS

Continuous values (not triggers) per hop:

| Field | Use |
|---|---|
| `centroidHz` | "Brightness" — map to color temperature (low = warm, high = cool) |
| `rolloffHz` | Top-end energy cutoff — drives high-band intensity |
| `spread` | Spectrum width — narrow (tonal) vs wide (noisy) |
| `flux` | Rate of spectral change — direct intensity LFO without onset detection |
| `hfc` | Continuous high-frequency content |
| `tssTransientNorm` / `tssSteadyNorm` | Split signal into "punch" vs "wash" channels |

### Scripts
- Low-energy ambient / soft motion → `audiomelt.js`, `audioplasma.js`, `audiolava.js`
- Noise/spatial diffusion → `audiosoap.js`, `audiowater.js`
- Hue from spectral content → `audiohueshift.js`, `audiowavelength.js`

---

## 7. Recommended Configurations

| Scenario | Onset | Threshold | minioi | Silence | Tempo thr | Notes |
|---|---|---|---|---|---|---|
| **Club / EDM** | `specflux` + `hfc` | 0.5 | 60 | -70 | 0.3 | Tatum 4 or 8. Whitening on. Disable `energy` (sub pumps it). |
| **Live band** | `complex` | 0.3 | 80 | -65 | 0.3 | Whitening on to tame PA EQ. |
| **Acoustic / ambient** | `wphase` | 0.5 | 150 | -50 | 0.4 | High minioi; low silence to catch quiet swells. |
| **DJ set (variable BPM)** | `specflux` | 0.4 | 50 | -70 | 0.25 | Lower tempo threshold to relock faster. |
| **Theater / cue-driven** | `complex` | 0.7 | 200 | -40 | n/a | Disable tempo. Onsets as accents only. |
| **Drum solo / percussion** | `hfc` + `specflux` + `energy` | 0.4 | 40 | -70 | 0.3 | Tatum 8. |

For all except theater: leave `tatumSubdivision = 4`. Adaptive whitening is applied automatically by aubio for methods that benefit from it (`complex`, `specflux`, `kl`, `mkl`).

---

## 8. Latency Budget

End-to-end audio→light delay at 44.1 kHz, default settings:

| Stage | Delay |
|---|---|
| Capture buffer (2048 samples) | ~46 ms |
| Aubio onset reporting (`onsetDelay` ≈ 4.3 hops) | ~50 ms |
| QLC+ MasterTimer scheduling (50 Hz default) | up to 20 ms |
| DMX frame to fixture | 25–40 ms typical |
| **Total** | **~120–160 ms** |

For tight strobes:
- Use wired audio. Bluetooth adds 100–250 ms.
- Reduce capture buffer if your backend allows.
- Tune `onsetDelayMs` / `tempoDelayMs` only after measuring real system latency.
- Avoid downstream smoothing/hold times on the trigger widget.

---

## 9. QLC+ Integration

### Pipeline

In `AudioCapture::processData()`:
1. Read PCM, average all input channels to mono int16.
2. `AubioProcessor::process()` runs all enabled detectors hop-by-hop. Onsets are OR-aggregated across hops in the buffer; everything else is "last hop wins".
3. `frame.aubio = &results` is attached to each `AudioFrame`.
4. `AudioAnalyzer::processFrame()` walks `AudioChannel`s, which can read `frame.aubio` plus their own band envelopes.
5. `AudioChannel::buildSnapshot()` copies aubio fields into `AudioSnapshot` for downstream consumers.

Aubio runs on the capture thread. Reconfiguration is staged: `setAubioConfig()` writes `m_pendingConfig` under a `QMutex`; the new config is picked up at the start of the next `process()` pass. **Mutex-protected, not lockless.**

### Hot-reloadable vs rebuild

- **Hot (no rebuild):** thresholds, silence levels, minioi, delays, whitening, compression, `tatumSubdivision`, `filterbankPower`, MFCC params, note params, TSS params, pitch tolerance/silence.
- **Triggers full rebuild:** `pitchMethod`, `tempoMethod`, `windowType`, `melScale`, `filterbankNorm`. Onset method enable/disable rebuilds only the affected detector.

### Snapshot exposure vs trigger usage

| Field | In `AudioSnapshot` | Used by VC triggers today |
|---|---|---|
| Mel bands, volume, beat | ✅ | ✅ |
| BPM, tatum, beat phase | ✅ | partial |
| MFCC, TSS, descriptors, pitch, notes | ✅ | ❌ (computed and exposed, but no built-in trigger source yet) |

Adding a new trigger source is mostly UI plumbing — the data is already in the snapshot.

---

## 10. Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| Strobe firing constantly | Threshold too low | Raise `onsetThreshold` by 0.1; raise `onsetMinIntervalMs` to 80–120. |
| Misses on quiet drum hits | Silence gate too high | Lower `onsetSilenceDb` to -80/-90. Try `specflux`. |
| Phantom onsets in silence | Room/preamp noise | Raise `onsetSilenceDb` to -50; enable adaptive whitening. |
| No input detected | Wrong device or muted | Check OS device + QLC+ audio input setting. |
| Works in headphones, fails on PA | Reverb tail double-triggers | Raise `onsetMinIntervalMs` to 100–150. Disable `energy` (sub pumping). |
| BPM stuck at 2× / ½× | Tactus bias | Raise `tempoThreshold` to 0.4. Wait 10 s for relock. |
| BPM unstable / drifts | Insufficient periodic content or low confidence | Lower `tempoSilenceDb` to -95; lower `tempoThreshold` to 0.2; treat low-confidence frames as stale. |
| Strobe arrives late on big hits | Capture + onset delay | See §8. Use wired audio; reduce buffer; subtract `onsetDelayMs`. |
| Kicks missed | Full-band onset is wrong tool | Use low-band envelope + TSS transient, not full-band onsets. |
| Pitch jittering | Mixing into pitch detector | Pitch needs mono solo source; gate on `pitchConfidence > 0.5`. |
| Sample-rate mismatch in logs | Backend fell back from 44.1 kHz | Pick a device that natively supports 44.1 kHz or accept the fallback rate. |

### Debugging fields

- `AubioResults::onsetDescriptors[]` and `onsetThresholdedDescriptors[]` — raw and post-threshold descriptor values per detector. Plot these vs the boolean onset flags to see *why* a detector is or isn't firing.
- `bpm` + `beatConfidence` + `beatPhase` over time — confidence near zero with non-zero BPM = aubio guessing, treat as unreliable.
- Match capture gain so program material sits around -25 dBFS RMS — every silence threshold is a dBFS comparison.

---

## References

- aubio API: <https://aubio.org/doc/latest/>
- aubio Python (parameter ranges & defaults): <https://aubio.readthedocs.io/en/latest/py_analysis.html>
- QLC+ source: `engine/audio/src/aubioprocessor.{h,cpp}`, `aubioresults.h`, `audiochannelconfig.h`, `audiocapture.{h,cpp}`, `audiochannel.cpp`
- RGB scripts: `resources/rgbscripts/audio*.js`
