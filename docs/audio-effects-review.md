# Audio Effects Review — Systematic Verification

## General Notes
- **Always set step time to `1/16`** — effects look static/choppy without it
- **Color properties should use the color selector**, not raw hex strings
- **Redundant `mirror` properties** should be removed (QLC+ has generic mirror)

## Round 1: Spectrum / Meter Effects

| Effect | Status | Notes |
|---|---|---|
| **EXP-1-A Audio Spectrum** | ❌ Broken | All dark, nothing happens. Likely still a rendering bug. |
| **EXP-1-B Audio Equalizer** | ✅ Cool | Works well, good response. |
| **EXP-1-C Audio Wavelength** | ⚠️ UX issue | Works but has a `gradient` property with raw hex values. Should use color selector. |
| **EXP-1-D Audio Power** | ⚠️ UX issues | `mirror` redundant. `sparks_color` uses raw hex. |
| **EXP-1-E Audio Scroll** | ⚠️ UX issues | `mirror` → rename to `Centered`. Colors use raw hex. |

## Round 2: Fills / Color / Strobe

| Effect | Status | Notes |
|---|---|---|
| **EXP-2-A Audio Beat Colors** | ✅ Great | 4-beat color cycling works well. |
| **EXP-2-B Audio Energy** | ✅ Great | 3-band fill looks good. |
| **EXP-2-C Audio Strobe** | ⚠️ UX issue | Has hex `strobe_color` property — needs color selector. |
| **EXP-2-D Audio Glitch 2** | ✅ Colorful | Works but hard to understand what it does. Visually rich. |
| **EXP-2-E Audio Hue Shift** | ✅ Cool | Smooth hue cycling. |

## Round 3: Scanners / Particles / Fire

| Effect | Status | Notes |
|---|---|---|
| **EXP-3-A Audio Scan** | ✅ Good | Works well, but has hex color property — needs color selector. |
| **EXP-3-B Audio Fire** | ✅ Fine | Bass-driven fire works. |
| **EXP-3-C Audio Fireworks** | ⚠️ Too fast | Much too fast. Fireworks should be more epic and slow — longer particle lifetime, slower fade. |
| **EXP-3-D Audio Bass Laser** | ⚠️ Rethink | Should be a laser shooting L→R through the whole line, like a chaser. More bass = more lasers. Current behavior doesn't match concept. |
| **EXP-3-E Audio Shockwave** | ⚠️ Random | Behavior feels random, not clearly audio-reactive. |

### Action items from Round 3
1. Scan: fix hex color properties → color selector
2. Fireworks: slow down particle lifetime/decay significantly
3. Bass Laser: redesign as a bass-triggered L→R chaser beam
4. Shockwave: investigate why it feels random — may need stronger audio coupling
