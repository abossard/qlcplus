# Audio UI Exposure — Implementation Plan

## Status: PLANNING (not yet started)

## Problem Statement

QLC+ has a rich audio analysis system (AudioCapture → AudioAnalyzer → AudioChannel →
AudioSnapshot → AudioProfile) fully built in C++ but only **partially exposed** in the UI.
Users can't tune most audio parameters, can't see what the engine is doing, and can't
assign profiles to effects.

## Architecture Overview

```
Audio Input → AudioCapture (FFT, 40-5000Hz log bands, beat detection)
                ↓
            AudioAnalyzer (32-band spectrum, spectral features)
                ↓
            AudioChannel (envelope, AGC, triggers, noise gate, volume)
                ↓
            AudioSnapshot (complete processed data)
                ↓
            AudioProfile (named config, saved to XML)
                ↓
    ┌───────────┴────────────┐
    ↓                        ↓
VCAudioTriggers          RGBMatrix
(VC widget, partial)     (audioProfileId exists,
                          NO UI selector)
```

## What Already Works

### C++ Backend (fully built)
- AudioSnapshot: bands (sub/bass/lowMid/mid/high), spectrum[32], triggers[5] with
  hold/cooldown, volume (raw/smoothed/normalized/agc), music (beat/bpm/phase/confidence),
  features (rmsDb/peakDb/crest/centroid/rolloff/flatness/flux)
- AudioProfile: named config with envelope, AGC, triggers, band layout, noise gate
- Doc: profile map, default profile, CRUD operations

### VCAudioTriggers Properties Panel (6 sections)
1. ✅ Spectrum Bars — bar count, per-bar mappings
2. ✅ Perceptual Bands — live sub/bass/lowMid/mid/high bars
3. ✅ Envelope — attack/release sliders
4. ✅ AGC — max gain + noise floor sliders
5. ✅ Triggers — 5 band dots + beat dot, threshold controls
6. ✅ Spectral — low/high cut bin readout

### VCAudioTriggers Item Widget
- ✅ Color-coded bars (orange/yellow/cyan)
- ✅ Split markers, monitor row, beat dot

## Gaps (prioritized)

### Critical Gaps
| # | Gap | Impact |
|---|-----|--------|
| 1 | No AudioProfile selector (widget or RGBMatrix) | Can't choose which "audio brain" to tune |
| 2 | Missing AGC controls (enabled/release/inputGain) | Can't tune the main feel of effects |
| 3 | Missing volume controls (smoothing, brightness floor) | Can't control jitter vs smoothness |
| 4 | No noise gate UI or indicator | Users don't know why effects are dead |
| 5 | No band layout editor | Can't customize frequency splits per genre |
| 6 | Trigger dots are fake (compare power, not actual state) | Misleading, ignores hold/cooldown |
| 7 | No live snapshot data in VC widget (volume/features) | Can't see what engine is doing |
| 8 | RGBMatrix has no profile picker | Scripts stuck on default profile |
| 9 | `ledfx_compat.js` deleted from disk | Band split fixes are dead code |

## Implementation Phases

### Phase 1a: AudioProfile Selector (~1 day)

**What:** Profile dropdown in VCAudioTriggers properties + shared `AudioProfileComboBox.qml`.

```
┌─ Audio Profile ──────────────────────────┐
│                                          │
│  Profile      [ Default Live Mix     ▾ ] │
│                                          │
└──────────────────────────────────────────┘
```

**Backend:** `audioProfileId` Q_PROPERTY already exists. Need QML-accessible profile list
from `Doc::audioProfiles()`.

**Files:** vcaudiotriggers.h/.cpp (model helper), new AudioProfileComboBox.qml,
VCAudioTriggersProperties.qml.

### Phase 1b: Missing Config Controls (~1.5 days)

**What:** AGC enabled/release/inputGain, volume smoothing, brightness floor.

```
┌─ AGC ────────────────────────────────────┐
│                                          │
│  Enabled      [●] On                     │
│  Max Gain     [████████──────]    18 dB  │
│  Release      [██████────────]  1500 ms  │
│  Noise Floor  [███████───────]   -54 dB  │
│  Input Gain   [██████────────]    1.60x  │
│                                          │
└──────────────────────────────────────────┘

┌─ Volume Response ────────────────────────┐
│                                          │
│  Smoothing    [██████────────]   100 ms  │
│  Bright Floor [██──────────────]     8 % │
│                                          │
└──────────────────────────────────────────┘
```

**Backend needed:** 5 new Q_PROPERTYs + setters (agcEnabled, agcRelease, agcInputGain,
volumeSmoothing, brightnessFloor). Pattern: read from profileChannelConfig(), apply via
applyChannelConfig().

**⚠ Slider spam fix:** Add coalesce timer — only call `m_doc->setModified()` on slider
release, not per-tick drag.

### Phase 2: Band Layout Editor (~1.5 days)

**What:** Expose crossover frequencies (sub/bass/lowMid/mid/high boundaries).

```
┌─ Band Layout ────────────────────────────┐
│                                          │
│  Sub ≤        [██──────────────]   60 Hz │
│  Bass ≤       [█████───────────]  250 Hz │
│  LowMid ≤     [███████─────────]  500 Hz │
│  Mid ≤        [████████████────] 2000 Hz │
│  High ≤       [██████████████──] 5000 Hz │
│                                          │
│  [Sub][Bass][LowMid][  Mid  ][ High ]    │
│                                          │
└──────────────────────────────────────────┘
```

**Validation:** Monotonic order, all values within 40-5000Hz range (capped to
`SPECTRUM_MAX_FREQUENCY` — don't allow exceeding it, the spectrum only covers up to 5kHz).

**Backend needed:** 5 Q_PROPERTYs + setters with monotonic validation.

### Phase 3: Noise Gate UI (~1 day)

**What:** Threshold, hold, live open/closed indicator, RMS readout.

```
┌─ Noise Gate ─────────────────────────────┐
│                                          │
│  State        ● Open                     │
│  Threshold    [███████───────]   -54 dB  │
│  Hold         [████────────────]  120 ms │
│  Input RMS    [██████────────]   -38 dB  │
│                                          │
└──────────────────────────────────────────┘
```

**🚨 Blocking requirement:** Must add `bool noiseGateClosed` to `AudioSnapshot` (from
`AudioChannel::m_noiseGateClosed`). Deriving from `rmsDb >= threshold` is wrong because
it ignores the hold timer.

### Phase 4: Real Trigger States (~1 day)

**What:** Replace fake trigger dots with actual TriggerState from AudioSnapshot.

```
┌─ Triggers ───────────────────────────────┐
│                                          │
│  Band       State   Power   Held  Cooldn │
│  Sub        ● Fire    74 %   12ms    0ms │
│  Bass       ● Hold    68 %   96ms    0ms │
│  LowMid     ○ Cool    31 %    0ms   80ms │
│  Mid        ○ Off     22 %    0ms    0ms │
│  High       ○ Off     18 %    0ms    0ms │
│  Beat       ● Fire   conf 82% phase 0.10│
│                                          │
└──────────────────────────────────────────┘
```

**Backend:** Expose as 5 stable child QObjects (not QVariantList rebuilt per frame).

### Phase 5: Rich Live VC Widget (~2 days)

**What:** Volume chain, spectral features, gate indicator in the VC widget item.

```
┌──────────────────────────────────────────┐
│ Audio: Live Mix                          │
│ ▁▃▆█▇▅▂ ▁▃▆█▇▅▂ ▁▃▆█▇▅▂                │
│ [Sub][Bass][LMid][ Mid ][ High ]         │
│ Sub 72%● Bass 63%● LMid 38%○ Mid 24%○   │
│ Beat ● 128BPM  Gate ● Open  Vol 68%     │
│ [Enabled ●]                     Vol 80% │
└──────────────────────────────────────────┘
```

**⚠ Performance:** Cache AudioSnapshot once per tick in VCAudioTriggers (not per-getter
call). Individual Q_PROPERTYs with selective notify, NOT a QVariantMap rebuilt at 25Hz.

**Widget size breakpoints:**
- ≥200px: full display
- ≥140px: hide spectral features
- ≥80px: hide monitor row
- <80px: spectrum bars + beat dot only

### Phase 6: RGBMatrix AudioProfile Selector (~1 day)

**What:** Profile picker in RGBMatrixEditor.qml (reuse AudioProfileComboBox from Phase 1a).

```
Audio Profile    [ Default Live Mix                 ▾ ]
```

**Backend:** `RGBMatrix::audioProfileId` Q_PROPERTY already exists. Wire to editor QML.

### Phase 7: Script Integration Cleanup (~0.5 days)

**What:**
1. Restore `ledfx_compat.js` to disk (deleted from working tree, exists in HEAD) and
   add to preload list. OR merge band split helpers into `audio_common.js` and drop the
   separate file entirely.
2. Use detected BPM from AudioSnapshot for `audio.bpm` in v2 mode (fall back to
   MasterTimer BPM for v1).

**Decision needed:** Merge `ledfx_compat.js` content into `audio_common.js`? The LedFx
namespace (`LedFx.lows_power()` etc.) is used by ~23 scripts. Changing the namespace
would break all of them. Best approach: restore the file and add to preload.

### Phase 8: Profile Management Polish (~2.5 days)

Duplicate/rename/delete/set-default with shared AudioProfileComboBox.
Usage counting. Safe deletion with reassignment.

### Phase 9: Tests (spread across phases)

AudioProfile config round-trip, trigger state mapping, script BPM fallback,
band layout validation.

## Shipping Order

```
1a → 1b → 2 → 3 → 6 → 4 → 5 → 7 → 8 → 9
```

Rationale: Profile selector unblocks everything (1a). Config controls make profiles
useful (1b). Band layout lets users differentiate profiles (2). Noise gate answers
"why is it silent?" (3). RGBMatrix selector comes after profiles are tunable (6).
Trigger/monitor polish comes last (4, 5).

## Key Design Decisions

| Decision | Choice | Why |
|----------|--------|-----|
| Snapshot caching | Cache once per tick, getters read cache | Avoid mutex contention at 25Hz |
| Trigger state exposure | 5 stable QObjects, not QVariantList | Avoid per-frame allocation |
| Slider save timing | Coalesce with timer, save on release | Prevent `setModified()` spam |
| Band layout range | Clamp to ≤5000Hz (SPECTRUM_MAX_FREQUENCY) | Spectrum doesn't cover higher |
| Noise gate state | Add to AudioSnapshot from AudioChannel | Don't derive from RMS heuristic |
| `ledfx_compat.js` | Restore file, add to preload list | 23 scripts use LedFx namespace |
| `audio.bpm` | v2: BeatTracker, v1: MasterTimer | Backward compat |

## Rough Effort

```
Phase 1a: 1 day
Phase 1b: 1.5 days
Phase 2:  1.5 days
Phase 3:  1 day
Phase 4:  1 day
Phase 5:  2 days
Phase 6:  1 day
Phase 7:  0.5 days
Phase 8:  2.5 days
Phase 9:  1.5 days (spread)
Total:    ~13 engineering days
```
