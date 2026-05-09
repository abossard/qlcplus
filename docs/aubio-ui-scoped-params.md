Agent is idle (waiting for messages). agent_id: plan-aubio-scoped-ui, agent_type: general-purpose, status: idle, description: Plan aubio-scoped UI params, elapsed: 192s, total_turns: 1, model: claude-opus-4.7

[Turn 0]
I now have the full picture. Time to deliver the analysis.

---

# Aubio UI parameter audit & redesign

## Step 3 — Per-object parameter tables

Legend: **[rebuild]** = recreates the aubio object; **[live]** = aubio setter callable while running; **[readonly]** = diagnostic getter; **[default]** = aubio owns it.

---

### aubio: Phase Vocoder (`aubio_pvoc_t`)
One global pvoc shared by all spectral consumers.

| Parameter | aubio function | Type | Range | Default | Currently exposed? | Mode |
|---|---|---|---|---|---|---|
| `win_s` (window size) | `new_aubio_pvoc` ctor | uint | 64..8192 (pow2) | 1024 | ✅ readonly text | [rebuild] |
| `hop_s` (hop size) | `new_aubio_pvoc` ctor | uint | win_s/8..win_s/2 | 512 | ✅ readonly text | [rebuild] |
| window type | `aubio_pvoc_set_window` | string | default/rectangle/hamming/hanning/hanningz/blackman/blackman_harris/gaussian/welch/parzen | "default" | ✅ combo (9 of 10) | [rebuild]¹ |
| sample rate | (capture) | uint | hardware | 44100 | ✅ readonly | n/a |

¹ aubio docs say live but implementation bakes window into buffers — we treat as rebuild.

**Gaps:** window-size and hop-size are not user-editable (currently constants `AUBIO_WIN_SIZE`/`AUBIO_HOP_SIZE`). `hanningz` is missing from the combo list. No "default" choice in window combo (it IS in widgetRef list).

**Recommended controls:**
- Window size → ComboBox `[256, 512, 1024, 2048, 4096]` **[rebuild]**
- Hop size → ComboBox `[win/8, win/4, win/2]` (relative) **[rebuild]**
- Window type → ComboBox (add `hanningz`) **[rebuild]**

---

### aubio: Mel Filterbank (`aubio_filterbank_t`)
Driven by the band-energy path; same filterbank also feeds the bar grouping.

| Parameter | aubio function | Type | Range | Default | Currently exposed? | Mode |
|---|---|---|---|---|---|---|
| `n_filters` | ctor | uint | typ. 26/40/80 | 40 (fixed `AUBIO_MEL_BANDS`) | ✅ readonly "40" | [rebuild] |
| `win_s` | ctor | uint | = pvoc win | from pvoc | implicit | [rebuild] |
| mel scale | `set_mel_coeffs_slaney` / `set_mel_coeffs_htk` / `set_mel_coeffs(fmin,fmax)` | string | slaney/htk/custom | slaney | ✅ combo (slaney/htk only) | [rebuild] |
| fmin / fmax (custom range) | `set_mel_coeffs(fmin,fmax)` | smpl | 0..Nyquist | n/a | ❌ not exposed | [rebuild] |
| norm | `set_norm` | smpl | 0 or 1 | 1 | ✅ checkbox | [rebuild]² |
| power | `set_power` | smpl | >0 (UI 0.5..4) | 1 | ✅ slider | [live] |

² aubio requires norm be set BEFORE `set_mel_coeffs_slaney`, so we treat as rebuild.

**Gaps:** `n_filters` hard-coded; no custom mel range (forced slaney/htk full-band).

---

### aubio: Onset (per-method `aubio_onset_t`)
Up to 9 separate onset objects, one per enabled method.

| Parameter | aubio function | Type | Range | Default (per-method, set by `set_default_parameters`) | Currently exposed? | Mode |
|---|---|---|---|---|---|---|
| method | ctor | string | energy/hfc/complex/phase/wphase/specdiff/kl/mkl/specflux | — | ✅ 9 checkboxes | [rebuild] |
| `buf_size` | ctor | uint | = pvoc win | 1024 | implicit | [rebuild] |
| `hop_size` | ctor | uint | = pvoc hop | 512 | implicit | [rebuild] |
| threshold | `set_threshold` | smpl | 0..1 | per-method | ❌ aubio default | [live] |
| silence | `set_silence` | smpl dB | -90..0 | -70 | ❌ aubio default | [live] |
| minioi (ms) | `set_minioi_ms` | smpl | ≥0 | 50 | ❌ aubio default | [live] |
| delay (samples) | `set_delay` | uint | ≥0 | per-method (e.g. 4·hop) | ❌ aubio default | [live] |
| compression (λ) | `set_compression` | smpl | ≥0 (0=off) | per-method (specflux=10, hfc=1, others=0) | ❌ aubio default | [live] |
| awhitening | `set_awhitening` | uint | 0/1 | per-method (specflux=on) | ❌ aubio default | [live] |

#### Onset getters (diagnostic):
| Getter | What it shows | Currently exposed? |
|---|---|---|
| `get_descriptor` | raw novelty | ✅ `onsetDescriptorValues` |
| `get_thresholded_descriptor` | post-threshold novelty | ✅ `onsetThresholdedValues` |
| `get_last_ms` | last-onset timestamp | ❌ |
| `get_threshold` | current threshold | ❌ |
| `get_silence` | current silence | ❌ |
| `get_minioi_ms` | current minioi | ❌ |
| `get_delay_ms` | current delay | ❌ |
| `get_compression` | current λ | ❌ |
| `get_awhitening` | current whitening flag | ❌ |

---

### aubio: Tempo (`aubio_tempo_t`)

| Parameter | aubio function | Type | Range | Default | Currently exposed? | Mode |
|---|---|---|---|---|---|---|
| method | ctor | string | "default" only | default | ❌ no UI (tempoMethod field exists) | [rebuild] |
| threshold | `set_threshold` | smpl | 0..1 | 0.3 | ✅ slider | [live] |
| silence | `set_silence` | smpl dB | -90..0 | -90 | ✅ slider | [live] |
| tatum signature | `set_tatum_signature` | uint | 1..64 (UI 1..16) | 4 | ✅ spinbox | [live] |
| delay | `set_delay_ms` | smpl ms | -500..500 | 0 | ✅ spinbox | [live] |

#### Tempo getters:
| Getter | What it shows | Currently exposed? |
|---|---|---|
| `get_bpm` | current BPM | ✅ `detectedBpm` |
| `get_confidence` | beat confidence | ✅ `beatConfidence` |
| `get_period_s` | beat period | ❌ (derivable) |
| `get_last_ms` | last beat | ✅ via `beatPhase` |
| `was_tatum` | tatum flag this frame | ❌ |
| `get_last_tatum` | last tatum sample | ❌ |
| `get_delay_ms` | current delay | mirrors setter |

---

### aubio: Pitch (`aubio_pitch_t`)

| Parameter | aubio function | Type | Range | Default | Currently exposed? | Mode |
|---|---|---|---|---|---|---|
| method | ctor | string | yinfft/yin/yinfast/fcomb/mcomb/schmitt/default | yinfft | ✅ combo | [rebuild] |
| `buf_size` | ctor | uint | ≥pvoc win | from pvoc | implicit | [rebuild] |
| `hop_size` | ctor | uint | =pvoc hop | from pvoc | implicit | [rebuild] |
| unit | `set_unit` | string | Hz/midi/cent/bin | Hz | ⚠️ combo present but **no-op** (display only, doesn't call setter) | [live] |
| silence | `set_silence` | smpl dB | -90..0 | -40 | ✅ slider | [live] |
| tolerance | `set_tolerance` | smpl | 0..1 | 0.7 (yinfft 0.85, yin 0.15) | ✅ slider | [live] |

#### Pitch getters:
| Getter | What | Exposed? |
|---|---|---|
| `aubio_pitch_do` output | current pitch (Hz) | ✅ `pitchHz` |
| `get_confidence` | confidence | ✅ `pitchConfidence` |

**Bug:** "Display unit" combo is wired to nothing (no `onActivated` handler).

---

### aubio: Notes (`aubio_notes_t`)

| Parameter | aubio function | Type | Range | Default | Currently exposed? | Mode |
|---|---|---|---|---|---|---|
| method | ctor | string | "default" | default | ❌ | [rebuild] |
| silence | `set_silence` | smpl dB | -90..0 | -70 | ✅ slider | [live] |
| minioi (ms) | `set_minioi_ms` | smpl | 0..500 | 30 | ✅ spinbox | [live] |
| release_drop (dB) | `set_release_drop` | smpl | 0..40 | 10 | ✅ slider | [live] |

#### Notes getters:
| Getter | What | Exposed? |
|---|---|---|
| `aubio_notes_do` out[0] | midi note on | ✅ `noteMidi`/`noteOn` |
| out[1] | velocity | ✅ `noteVelocity` |
| out[2] | midi note off | ✅ `noteOff` |

---

### aubio: MFCC (`aubio_mfcc_t`)

| Parameter | aubio function | Type | Range | Default | Currently exposed? | Mode |
|---|---|---|---|---|---|---|
| `buf_size` | ctor | uint | =pvoc win | from pvoc | implicit | [rebuild] |
| `n_filters` | ctor | uint | typ 40 | 40 | ❌ fixed | [rebuild] |
| `n_coeffs` | ctor | uint | ≤n_filters, typ 13 | hard-coded | ❌ fixed | [rebuild] |
| `samplerate` | ctor | uint | from capture | system | implicit | [rebuild] |
| power | `set_power` | smpl | >0 | 1 | ✅ stored, **no slider in UI** | [live] |
| scale | `set_scale` | smpl | >0 | 1 | ✅ stored, **no slider in UI** | [live] |
| fmin/fmax (custom mel) | `set_mel_coeffs(_htk)` | smpl,smpl | 0..Nyquist | full band | ❌ | [rebuild] |
| coeffs init | slaney vs htk vs custom | enum | slaney/htk | slaney | ❌ shares filterbank's melScale? | [rebuild] |

**Gaps:** `mfccPower`/`mfccScale` are in `AubioConfig` but not surfaced in the QML MFCC section. `n_coeffs` not configurable.

---

### aubio: TSS (`aubio_tss_t`)

| Parameter | aubio function | Type | Range | Default | Currently exposed? | Mode |
|---|---|---|---|---|---|---|
| `buf_size` | ctor | uint | =pvoc win | from pvoc | implicit | [rebuild] |
| `hop_size` | ctor | uint | =pvoc hop | from pvoc | implicit | [rebuild] |
| alpha | `set_alpha` | smpl | typ 1..10 | 3 | ✅ presumed slider | [live] |
| beta | `set_beta` | smpl | typ 1..10 | 3 | ✅ presumed slider | [live] |
| threshold | `set_threshold` | smpl | 0..1 | 0.25 | ✅ presumed slider | [live] |

---

## Step 4 — Edit Panel mockup (revised)

```
┌─ Audio Profile ────────────────────────────────────────────────┐
│ Profile [combo]   New profile…   Rename   Delete               │
│ Channel [combo]                                                │
└────────────────────────────────────────────────────────────────┘

┌─ aubio: Phase Vocoder ─────────────────────────────────────────┐
│ Window           [combo: default/rectangle/hamming/hanning/    │
│                   hanningz/blackman/blackman_harris/gaussian/  │
│                   welch/parzen]                       [rebuild]│
│ Window size      [combo: 256/512/1024/2048/4096]      [rebuild]│  ← NEW editable
│ Hop size         [combo: win/8, win/4, win/2]         [rebuild]│  ← NEW editable
│ Sample rate      44100 Hz                            [readonly]│
│ Frames / second  86 fps                              [readonly]│
└────────────────────────────────────────────────────────────────┘

┌─ aubio: Mel Filterbank ────────────────────────────────────────┐
│ Bands            40                                  [readonly]│
│ Mel scale        [combo: slaney / htk / custom]       [rebuild]│
│   ↳ when custom: Fmin [____] Hz   Fmax [____] Hz      [rebuild]│  ← NEW
│ Norm             [✓] normalize filter weights         [rebuild]│
│ Power            [slider 0.5–4.0]   1.0                  [live]│
└────────────────────────────────────────────────────────────────┘

┌─ aubio: Onset Detection ───────────────────────────────────────┐
│ Methods (each enables a separate aubio_onset_t)                │
│   [✓] energy     [✓] hfc       [✓] complex                     │
│   [✓] phase      [✓] wphase    [✓] specdiff                    │
│   [✓] kl         [✓] mkl       [✓] specflux            [rebuild]│
│                                                                │
│ ▸ Per-method tuning  (collapsed by default)                    │
│   Each row = one method, columns are the 6 setters.            │
│   Empty cell = aubio default. Click to override.               │
│   ┌────────┬──────┬───────┬────────┬───────┬────────┬───────┐  │
│   │ method │thr   │silence│minioi  │delay  │compress│awhite │  │
│   ├────────┼──────┼───────┼────────┼───────┼────────┼───────┤  │
│   │ energy │ —    │ —     │ —      │ —     │ —      │ —     │  │
│   │ hfc    │ —    │ —     │ —      │ —     │ —      │ —     │  │
│   │ specflux│ —   │ —     │ —      │ —     │ —      │ —     │  │
│   └────────┴──────┴───────┴────────┴───────┴────────┴───────┘  │
│   "—" = aubio default; click to enter override.          [live]│
│                                                                │
│ Diagnostics: descriptor & thresholded values rendered live     │
│ in the timeline view.                                [readonly]│
└────────────────────────────────────────────────────────────────┘

┌─ aubio: Tempo / Beat ──────────────────────────────────────────┐
│ Threshold        [slider 0–1]   0.30                     [live]│
│ Silence          [slider -90–0 dB]   -90 dB              [live]│
│ Tatum subdivision[spin 1–16]   4                         [live]│
│ Delay            [spin -500..500 ms]   0 ms              [live]│
│ ─ Diagnostics ─                                                │
│ BPM              128.4                               [readonly]│
│ Confidence       0.87                                [readonly]│
│ Period           0.468 s                             [readonly]│  ← NEW (get_period_s)
│ Last beat        125 ms ago                          [readonly]│
└────────────────────────────────────────────────────────────────┘

┌─ aubio: Pitch Detection ───────────────────────────────────────┐
│ Method           [combo: yinfft/yin/yinfast/fcomb/mcomb/       │
│                          schmitt]                     [rebuild]│
│ Silence          [slider -90..0 dB]   -40 dB             [live]│
│ Tolerance        [slider 0..1]   0.70                    [live]│
│ Output unit      [combo: Hz/midi/cent/bin]  Hz           [live]│  ← FIX: actually wire setter
│ ─ Diagnostics ─                                                │
│ Pitch            440.0 Hz / A4                       [readonly]│
│ Confidence       0.92                                [readonly]│
└────────────────────────────────────────────────────────────────┘

┌─ aubio: Note Detection ────────────────────────────────────────┐
│ Silence          [slider -90..0 dB]   -70 dB             [live]│
│ Min interval     [spin 0..500 ms]   30 ms                [live]│
│ Release drop     [slider 0..40 dB]   10 dB               [live]│
│ ─ Diagnostics ─                                                │
│ Note on / off    A4 / —     vel 0.71                 [readonly]│
└────────────────────────────────────────────────────────────────┘

┌─ aubio: MFCC ──────────────────────────────────────────────────┐
│ Coefficients     13                                  [readonly]│  ← currently fixed
│ Filters          40                                  [readonly]│
│ Power            [slider 0.5–4.0]   1.0                  [live]│  ← NEW (mfccPower exists)
│ Scale            [slider 0.1–10]    1.0                  [live]│  ← NEW (mfccScale exists)
│ ─ Diagnostics ─                                                │
│ Coeff bars   [c0..c12 mini-bar render]               [readonly]│
└────────────────────────────────────────────────────────────────┘

┌─ aubio: TSS (Transient/Steady) ────────────────────────────────┐
│ Threshold        [slider 0..1]   0.25                    [live]│
│ Alpha            [slider 1..10]   3.0                    [live]│
│ Beta             [slider 1..10]   3.0                    [live]│
│ ─ Diagnostics ─                                                │
│ Bins             1024                                [readonly]│
│ (transient/steady mini-spectrum rendered in main view)         │
└────────────────────────────────────────────────────────────────┘

══════ QLC+ post-processing (not aubio) ═════════════════════════
┌─ QLC+: Band Grouping ─────────────────── (current section) ────┐
┌─ QLC+: Envelope ─────────────────────── (current section) ─────┐
┌─ QLC+: Noise Gate ────────────────────── (current section) ────┐
┌─ QLC+: Triggers ─────────────────────── (current section) ─────┐
┌─ QLC+: Volume & Display ─────────────── (current section) ─────┐
┌─ Spectrum Bar Mappings ─────────────── (current section) ──────┐
```

---

## Gap list — what's missing

| Section | Gap | Severity |
|---|---|---|
| Phase Vocoder | Window size / hop size not editable (compile-time const) | **High** — biggest perf/latency lever |
| Phase Vocoder | "hanningz" missing from window list | Low |
| Mel Filterbank | n_filters fixed at 40 | Med |
| Mel Filterbank | No custom fmin/fmax range (always full band) | Med |
| Onset | All per-method tuning hidden behind aubio defaults | **High** — see recommendation below |
| Onset | No live diagnostic of effective threshold/silence/etc | Low |
| Tempo | `get_period_s` not exposed (derivable but useful) | Low |
| Tempo | `was_tatum` / `get_last_tatum` not exposed | Med (useful for tatum-driven cues) |
| Pitch | "Display unit" combo is **wired to nothing** | Bug |
| Notes | No method selector (only "default" exists upstream — fine to leave) | None |
| MFCC | `mfccPower` / `mfccScale` exist in config but **not in UI** | **High** — config field with no control |
| MFCC | n_coeffs hard-coded | Med |
| TSS | Verify alpha/beta sliders actually exist (section truncated in read) | Verify |
| Cross-cutting | `framesPerSecond` not shown anywhere | Low |

---

## Step 5 — Onset per-method recommendation

**Recommend: D — show aubio defaults with a per-cell "override" toggle.**

| Option | Tradeoffs |
|---|---|
| A) leave at defaults (status quo) | ✅ simplest, ✅ aubio's tuned defaults are good. ❌ power users can't tune for their material; ❌ no visibility into what aubio is actually doing. |
| B) expose every per-method param always | ✅ maximum power. ❌ 9×6 = 54 controls, overwhelming. ❌ encourages bad tuning by users who don't read aubio docs. |
| C) read-only display of defaults | ✅ informative, no risk. ❌ frustrating for users who DO know what they want. |
| **D) defaults shown, per-cell override toggle** | ✅ defaults visible (transparency), ✅ override only when needed, ✅ row-level "reset to defaults", ✅ persists in `AubioConfig` as a sparse map `{method: {param: value}}` rather than a flat 54-field struct. ❌ slightly more UI work. |

**Implementation sketch for D:**

```cpp
struct OnsetMethodOverride {
    std::optional<double> threshold;
    std::optional<double> silenceDb;
    std::optional<double> minioiMs;
    std::optional<double> delayMs;
    std::optional<double> compression;
    std::optional<bool>   awhitening;
};
struct AubioConfig {
    OnsetMethodOverride onsetOverrides[9]; // indexed like onsetMethodEnabled
    // ...
};
```

In AubioProcessor, after `aubio_onset_set_default_parameters(...)` for each created onset, apply only the overrides that are set. The UI shows the **effective** value (default if no override, override value otherwise) with a visual marker (italic = default, bold = overridden) and a small "↺" reset-to-default button per cell.

This keeps aubio's good defaults as the floor while giving the depth lighting designers want for difficult material (e.g., raising `specflux` threshold for very busy electronic music).

---

## Summary of immediate fixes (before any redesign)

1. **Wire the pitch unit combo** — call `setPitchUnit()` (which doesn't exist yet — add `aubio_pitch_set_unit` invocation).
2. **Add MFCC power & scale sliders** — config fields `mfccPower`/`mfccScale` already exist but have no UI control.
3. **Add `hanningz` to the window-type combo** — aubio supports it and it's the actual default for pvoc.
4. **Make window size and hop size editable** — biggest functional gap; everything else flows from these two.