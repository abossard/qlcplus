# P0 — Inventory of Bundled Audio RGB Scripts

Scope: every `resources/rgbscripts/audio*.js` file shipped with QLC+ (excluding the `audio_common.js` helper). Total: **28 scripts**.

Columns:
- **Mode** — `installContinuous` (per-frame audio) vs `installTrigger` (event-driven).
- **AudioParams** — helpers actually called (besides the install hook).
- **LedFx** — rendering / DSP helpers from the LedFx layer.
- **Pattern** — observed behaviour archetype.
- **ExpFilter** — does the script instantiate its own `LedFx.ExpFilter` (i.e. holds private smoothing state).
- **Raw thresholds** — hand-tuned numeric comparisons against audio energy (not against `dt` clamps, render skips, or `Math.random`).

## Full inventory

| # | Script | Display name | Mode | AudioParams (besides install) | LedFx | Pattern | ExpFilter | Raw audio thresholds |
|---|---|---|---|---|---|---|---|---|
| 1 | `audioaurora.js` | Audio Aurora | continuous | applyFloor, createFilter, gainFactor | rgb, createMap, lows_power, mids_power, high_power | three-band blend + spatial | 0 | — |
| 2 | `audiobasslaser.js` | Audio Bass Laser | trigger | createFilter, gainFactor, triggerThreshold | rgb, createMap, lows_power, high_power | trigger-first laser beams | 0 | `bass > 0.15` (spawn fallback) |
| 3 | `audioblocks.js` | Audio Blocks | continuous | applyFloor, createFilter, gainFactor | rgb, createMap, melbank, lows_power, mids_power, high_power | spectrum visual (melbank + bands) | 0 | render skip `bright < 0.01` |
| 4 | `audiobuildup.js` | Audio Buildup | trigger | createFilter, gainFactor | rgb, hsv2rgb, createMap, ExpFilter, melbank, lows_power, mids_power, high_power | **state machine** (drop detection) | **1** | `energyTrend > 0.4`, `highRatio > 0.5`, `flux > 0.4`, `lows < 0.3`, `lows < 0.15` |
| 5 | `audiochaser.js` | Audio Chaser | continuous | applyFloor, createFilter, gainFactor | rgb, createMap, lows_power, mids_power, high_power | three-band blend + spatial | 0 | — |
| 6 | `audiocrawler.js` | Audio Crawler | continuous | applyFloor, createFilter, gainFactor | rgb, createMap, lows_power | low-energy driver (spatial) | 0 | — |
| 7 | `audioenergy.js` | Audio Energy | continuous | applyFloor, createFilter, gainFactor | rgb, createMap, lows_power, mids_power, high_power | three-band blend | 0 | — |
| 8 | `audioequalizer.js` | Audio Equalizer | continuous | applyFloor, createFilter, gainFactor | rgb, createMap, melbank | spectrum visual | 0 | render skip `peak > 0.01` |
| 9 | `audiofire.js` | Audio Fire | continuous | applyFloor, createFilter, gainFactor | rgb, createMap, melbank, lows_power | low-energy + spectrum (sparks) | 0 | — |
| 10 | `audiofireworks.js` | Audio Fireworks | trigger | createFilter, gainFactor, triggerThreshold | rgb, hsv2rgb, createMap, lows_power, mids_power, high_power | trigger-first particles | 0 | `bri > 0.3` (×2), `bass+mids+highs > 0.1` |
| 11 | `audioglitch.js` | Audio Glitch | continuous | applyFloor, createFilter, gainFactor | rgb, hsv2rgb, createMap, lows_power | low-energy driver | 0 | — |
| 12 | `audiohueshift.js` | Audio Hue Shift | continuous | applyFloor, createFilter, gainFactor | rgb, hsv2rgb, createMap, lows_power, mids_power, high_power | three-band blend + hue rotation | 0 | — |
| 13 | `audiolava.js` | Audio Lava Lamp | continuous | applyFloor, createFilter, gainFactor | rgb, createMap, lows_power, mids_power, high_power | three-band blend + spatial | 0 | — |
| 14 | `audiomelt.js` | Audio Melt | continuous | applyFloor, createFilter, gainFactor | rgb, createMap, lows_power | low-energy driver (spatial) | 0 | — |
| 15 | `audioplasma.js` | Audio Plasma | continuous | applyFloor, createFilter, gainFactor | rgb, createMap, lows_power | low-energy spatial | 0 | — |
| 16 | `audiopower.js` | Audio Power | continuous | applyFloor, createFilter, gainFactor | rgb, createMap, melbank, lows_power | low-energy + spectrum | 0 | `sparksPixels > 0.1` |
| 17 | `audioscan.js` | Audio Scan | continuous | applyFloor, createFilter, gainFactor | rgb, createMap, lows_power | low-energy spatial | 0 | — |
| 18 | `audioscroll.js` | Audio Scroll | continuous | applyFloor, gainFactor | rgb, hsv2rgb, createMap, melbank, melbank_thirds, avg | spectrum visual | 0 | — |
| 19 | `audioshockwave.js` | Audio Shockwave | trigger | createFilter, gainFactor, triggerThreshold | rgb, createMap, lows_power | trigger-first waves | 0 | `bass > 0.1`, `totalBri > 0.005`, `intensity < 0.01` |
| 20 | `audioshot.js` | Audio Shot | trigger | gainFactor, triggerThreshold | rgb, hsv2rgb, createMap, lows_power, mids_power, high_power | pure trigger (flash) | 0 | — |
| 21 | `audiosoap.js` | Audio Soap | continuous | applyFloor, createFilter, gainFactor | rgb, createMap, noiseField2d, lows_power | low-energy + noise field | 0 | — |
| 22 | `audiospectrum.js` | Audio Spectrum | continuous | applyFloor, createFilter, gainFactor | rgb, hsv2rgb, createMap, melbank | spectrum visual | 0 | render skip `val > 0.01` |
| 23 | `audiosplittower.js` | Audio Split Tower | continuous | applyFloor, gainFactor | rgb, createMap, melbank | spectrum visual | 0 | render skip `magnitude > 0.01` |
| 24 | `audiostrobe.js` | Audio Strobe | trigger | gainFactor, triggerThreshold | rgb, hsv2rgb, createMap, lows_power, mids_power, high_power | pure trigger (strobe) | 0 | — |
| 25 | `audiotunnel.js` | Audio Tunnel | continuous | applyFloor, createFilter, gainFactor | rgb, createMap, lows_power | low-energy spatial | 0 | — |
| 26 | `audiovortex.js` | Audio Vortex | continuous | applyFloor, createFilter, gainFactor | rgb, createMap, lows_power | low-energy spatial | 0 | — |
| 27 | `audiowater.js` | Audio Water | continuous | applyFloor, gainFactor | rgb, createMap, melbank_thirds, avg | spectrum visual | 0 | `bright > 0.8` (highlight) |
| 28 | `audiowavelength.js` | Audio Wavelength | continuous | applyFloor, createFilter, gainFactor | rgb, createMap, melbank | spectrum visual | 0 | — |

## Summary counts

### AudioParams usage (28 scripts)

| Function | Count | Notes |
|---|---:|---|
| `gainFactor` | 28 | Universal — every script gain-stages input. |
| `createFilter` | 23 | Missing in 5 (`audioscroll`, `audioshot`, `audiostrobe`, `audiosplittower`, `audiowater`). |
| `applyFloor` | 22 | All continuous scripts use it; trigger scripts mostly skip it (`audioshot`, `audiostrobe`, `audiobasslaser`, `audiobuildup`, `audiofireworks`, `audioshockwave`). |
| `installContinuous` | 22 | |
| `installTrigger` | 6 | basslaser, buildup, fireworks, shockwave, shot, strobe. |
| `triggerThreshold` | 5 | basslaser, fireworks, shockwave, shot, strobe (buildup defines its own internal logic instead). |
| `filterRise` | **0** | Not yet adopted by any bundled script. |
| `adaptiveGain` | **0** | Not yet adopted. |
| `hysteresisTrigger` | **0** | Not yet adopted. |

### LedFx usage (28 scripts)

| Function | Count |
|---|---:|
| `rgb` | 28 |
| `createMap` | 28 |
| `lows_power` | 22 |
| `high_power` | 11 |
| `mids_power` | 10 |
| `melbank` | 9 |
| `hsv2rgb` | 8 |
| `melbank_thirds` | 2 |
| `avg` | 2 |
| `noiseField2d` | 1 (`audiosoap`) |
| `ExpFilter` | 1 (`audiobuildup`) |

## Risk assessment — porting / migration impact

Ordering is from highest to lowest expected effort if the AudioParams / LedFx surface changes.

1. **`audiobuildup.js` — HIGHEST.** The only script that builds its own `ExpFilter`, owns a feature-vote state machine with five hand-tuned thresholds (`energyTrend > 0.4`, `highRatio > 0.5`, `flux > 0.4`, `lows < 0.3`, `lows < 0.15`), and is the broadest LedFx consumer (lows/mids/high + melbank + hsv2rgb). Any change to filter semantics, gain staging, or melbank scaling will shift every threshold. Strong candidate to migrate to `hysteresisTrigger` + `adaptiveGain`.
2. **Trigger scripts with raw band thresholds — `audiobasslaser`, `audiofireworks`, `audioshockwave`.** They combine `installTrigger` + `triggerThreshold` with extra raw checks (`bass > 0.1 / 0.15`, `bri > 0.3`, etc.) for spawn fallbacks and culling. Migration to `hysteresisTrigger` would make the duplicated logic redundant; care needed to preserve "second-spawn" behaviour.
3. **Scripts without `createFilter` — `audioscroll`, `audiowater`, `audiosplittower`, `audioshot`, `audiostrobe`.** They consume melbank or trigger pulses raw, so any change in default smoothing in the audio pipeline affects them disproportionately. `audioscroll` and `audiowater` additionally rely on the rarer `melbank_thirds` + `avg` pair.
4. **`audiosoap.js`.** Sole user of `LedFx.noiseField2d`; that helper has no other test coverage in the inventory, so it must keep working in isolation.
5. **Pure trigger scripts — `audioshot`, `audiostrobe`.** Minimal AudioParams surface (`gainFactor` + `triggerThreshold` only), no floor, no filter. Easiest to break if `installTrigger` semantics change, but trivial to port.
6. **Three-band blend + spatial group — `audioaurora`, `audiochaser`, `audioenergy`, `audiohueshift`, `audiolava`.** Identical AudioParams shape (`applyFloor + createFilter + gainFactor` over lows/mids/highs). Low individual risk; high collective surface area — a change to band power scaling shifts the look of all five at once.
7. **Low-energy spatial group — `audiocrawler`, `audiomelt`, `audioplasma`, `audioscan`, `audiotunnel`, `audiovortex`, `audioglitch`.** Uniform pattern (only `lows_power` consumed), so they migrate as a batch with one helper change.
8. **Spectrum visualisers without raw thresholds — `audioequalizer`, `audiowavelength`, `audiospectrum`, `audiofire`, `audiopower`, `audioblocks`.** Render-side `> 0.01` skips are cosmetic and tolerant to small DSP shifts; lowest risk overall.

### Cross-cutting observations

- None of the 28 scripts use the new helpers (`filterRise`, `adaptiveGain`, `hysteresisTrigger`); every smoothing/threshold decision today lives either in `createFilter` config or in inline numeric constants.
- Only `audiobuildup` instantiates its own `ExpFilter` — porting that helper away from public API would only break one script.
- All 28 use `LedFx.rgb` and `LedFx.createMap` — these are the hard compatibility floor.
- 22/28 scripts depend on `lows_power`; it is the single most load-bearing LedFx function in the bundle.
