# P4 — RGBUtil JS Namespace

## Goal

Extract the **non-audio** visual helpers currently exposed under
`LedFx.*` (in `resources/rgbscripts/ledfx_compat.js`) into a standalone
`RGBUtil` namespace at `resources/rgbscripts/rgbutil.js`, so that
generic colour / map / noise utilities are no longer coupled to the
LedFx audio compatibility shim.

The implementation is a mechanical lift: each function body is byte-for-byte
identical to its LedFx counterpart. A future step will rename
`LedFx.<fn>` → `RGBUtil.<fn>` in the bundled scripts (`s/LedFx\./RGBUtil./g`
for the helpers below).

## What was done

- Created `resources/rgbscripts/rgbutil.js` with the `RGBUtil` namespace.
- Added `rgbutil.js` to `resources/rgbscripts/CMakeLists.txt` (alphabetical
  position between `randomsingle.js` and `sinewave.js`) so it is installed,
  Android-bundled, and dev-symlinked alongside the other scripts.
- Did **not** modify the preload order in `rgbscriptv4.cpp` (deferred to the
  script-porting step).

## Helpers ported

Selected by walking the LedFx surface and keeping anything that does not
touch `audioData` / spectra / beats. Cross-checked against the LedFx
usage table in `p0-inventory-scripts.md` (28 scripts).

| RGBUtil function | LedFx source | Behaviour | Used by N scripts |
|---|---|---|---:|
| `RGBUtil.rgb(r, g, b)` | `LedFx.rgb` (ll. 283-288) | Clamp 0-255, round, pack `(r<<16)\|(g<<8)\|b` → `0xRRGGBB` | 28 |
| `RGBUtil.hsv2rgb(h, s, v)` | `LedFx.hsv2rgb` (ll. 261-278) | HSV→[r,g,b] 0-255, hue wraps via `((h%1)+1)%1` | 8 |
| `RGBUtil.createMap(w, h)` | `LedFx.createMap` (ll. 293-301) | `height × width` 2D array of zeros, indexed `map[y][x]` | 28 |
| `RGBUtil.interpolate(arr, size)` | `LedFx.interpolate` (ll. 73-88) | Linear resample of an array to a new length (numpy.interp on an even grid). Edge cases (`length==0`, `==1`, `==size`) preserved. | (internal; published for parity) |
| `RGBUtil.simplex2d(x, y)` | `LedFx.simplex2d` (ll. 315-354) | Stefan Gustavson 2D simplex noise, range −1..1. Same `_grad3` and `_perm` tables. | (internal) |
| `RGBUtil.noiseField2d(w, h, freq, ox, oy)` | `LedFx.noiseField2d` (ll. 359-372) | 2D simplex noise field normalised to 0-1 | 1 (`audiosoap`) |

`interpolate` is kept on RGBUtil even though no current bundled
non-audio script calls it directly — it is the building block underneath
`noiseField2d`'s sibling resamplers and is part of the documented
"generic visual helper" surface, so future ports stay decoupled from
LedFx.

## Helpers intentionally left in LedFx

Audio-coupled, stay in `ledfx_compat.js`:

`ExpFilter`, `melbank`, `melbank_thirds`, `melbank_lows`, `melbank_mids`,
`melbank_highs`, `bandSplitIndices`, `lows_power`, `mids_power`,
`high_power`, `avg`, `beat_oscillator`, `bar_oscillator`, and the
`_LOG_RANGE` / `_LOW_CUT_RATIO` / `_HIGH_CUT_RATIO` constants.

These either consume the `audioData` parameter, depend on the
audiocapture log-frequency split, or are utility helpers (`avg`)
that today only have audio callers.

## Verification

Implementation parity was verified by evaluating both files in Node and
comparing every output. All 11 cases match exactly (including
floating-point return values from `simplex2d` and `noiseField2d`):

```
OK rgb byte order
OK rgb clamp/round
OK hsv red
OK hsv mid
OK hsv wrap
OK createMap shape
OK interpolate up
OK interpolate 2->5
OK simplex2d
OK simplex2d neg
OK noiseField2d
```

Syntax: `node --check resources/rgbscripts/rgbutil.js` → OK.

## Comparison table — function-by-function diff vs LedFx

| Aspect | LedFx | RGBUtil | Diff |
|---|---|---|---|
| `rgb` clamp/round formula | `Math.max(0, Math.min(255, Math.round(x)))` | identical | none |
| `rgb` packing | `(r<<16) \| (g<<8) \| b` | identical | none |
| `hsv2rgb` hue wrap | `((h%1)+1)%1` | identical | none |
| `hsv2rgb` sector switch | 6-case 0..5 | identical | none |
| `hsv2rgb` output | `[round(r*255), round(g*255), round(b*255)]` | identical | none |
| `createMap` shape | `map[height][width]` | identical | none |
| `createMap` fill | explicit `0` per cell | identical | none |
| `interpolate` empty/single/equal-size short-circuits | preserved | identical | none |
| `interpolate` ratio | `(arr.length-1)/(size-1)` | identical | none |
| `simplex2d` constants | `F2 = 0.5*(√3-1)`, `G2 = (3-√3)/6` | identical | none |
| `simplex2d` permutation table | 256-entry Ken Perlin table, doubled to 512 | byte-identical copy | none |
| `simplex2d` scale factor | `70 * (n0+n1+n2)` | identical | none |
| `noiseField2d` normalisation | `(n+1)*0.5` | identical | none |
| `noiseField2d` axis denominator | `Math.max(1, dim-1)` | identical | none |

## Next steps (out of scope here)

1. Rename `LedFx.{rgb,hsv2rgb,createMap,simplex2d,noiseField2d}` →
   `RGBUtil.*` inside the 28 bundled `audio*.js` scripts and in any
   non-audio script that picks them up later.
2. Update preload order in `engine/src/rgbscriptv4.cpp` so that
   `rgbutil.js` is loaded before any script that depends on it
   (and before `ledfx_compat.js`, since LedFx will eventually
   depend on RGBUtil for the visual primitives).
3. Once all callers have migrated, delete the duplicated
   `rgb / hsv2rgb / createMap / simplex2d / noiseField2d / interpolate`
   bodies from `ledfx_compat.js` and have it re-export them from
   RGBUtil for backwards compatibility, or drop them entirely if
   external scripts are not a concern.
