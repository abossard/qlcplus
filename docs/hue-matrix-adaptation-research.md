# Matrix-aware LedFx responses

Research date: 2026-09-17.

Make existing LedFx responses useful on matrices without changing Artistic
profiles or adding response modes. The recommendations below are proposed
geometry changes, not claims of identical upstream pixels.

## Sources and preservation boundary

Inspected source pins:

- LedFx: `87583f9638fbd749bc0953f35334d8d5140c882f`
- WLED: `915565fcb896f3d3eec52aba0d53190fb34cd0cd`
- FastLED: `df106dfe6a2124c03143ef611c7d1174ea70f06c`

LedFx separates audio preparation from drawing and renders its 2D effects at
matrix dimensions. This supports adapting the rasterizer rather than replacing
the analyzer or introducing an effect framework. [LedFx 2D base][twod],
[Equalizer audio preparation][eq-audio]

**Local requirement:** new geometry belongs only inside the affected LedFx
response branches when both dimensions exceed one. Preserve Artistic code,
shared helpers, calls and state updates. Fixed-input, fixed-time, seeded output
comparisons must prove Artistic preservation; these external sources cannot.

## Coordinates and aspect ratio

For endpoint positioning, propose `u = x / (width - 1)` and
`v = y / (height - 1)`, using `0.5` for a singleton axis. WLED maps normalized
positions through each axis's length minus one. Periodic fields instead need a
half-open sampling interval, such as `x / width`, to avoid repeating an endpoint.
[WLED normalized coordinates][xy-normalized]

For round heads, radial fields and isotropic noise, equal pixel steps matter.
Propose centered coordinates divided by `max(1, min(width, height))`. This
assumes equal physical pixel pitch: an 80x4 matrix then shows a wide crop rather
than a stretched circle. Axis-by-axis normalization is appropriate when an
ellipse should fill the rectangle. FastLED applies one noise scale to both
axes; LedFx explicitly uses separate radii for elliptical distance.
[FastLED noise sampling][noise-scale], [LedFx ellipse][ellipse]

Useful shape checks:

- **80x4:** retain 80 horizontal samples and four vertical samples; fractional
  coverage can express amplitude changes smaller than one row.
- **32x32:** equal scales preserve centered symmetry.
- **7x11 and 11x7:** use actual axis lengths and centers; do not assume even
  halves. LedFx Waterfall distinguishes odd and even centered scrolling.
- **1xN, Nx1 and 1x1:** keep the existing strip path and guard singleton
  divisions. LedFx Waterfall skips scrolling below two rows.

[LedFx centered scrolling and small-height guards][scroll-guards]

Keep logical coordinates separate from physical wiring. FastLED confines
serpentine reversal to its XY mapping function. Do not reverse alternate rows
again inside individual response implementations. [FastLED XY mapping][fast-xy]

## Small adaptations, ranked

### 1. Spectrum and bands: frequency by amplitude

Keep the existing processed audio vector and controls. At rasterization, map
frequency onto columns and magnitude onto height. LedFx separates band
aggregation and peak filtering from drawing; WLED GEQ uses frequency columns
and amplitude heights. [LedFx band preparation][eq-audio], [WLED GEQ][geq]

Proposed bottom-up coverage for amplitude `a` in `[0, 1]`:
`clamp(a * height - (height - 1 - y), 0, 1)`. Silence fills no cells, full scale
fills the column, and shallow matrices retain fractional changes. Partition
visible bands with exclusive upper boundaries and cap visible band count at
width, without changing configured audio-bank resolution.

This is the first candidate for high visual benefit with little code. It is a
proposal, not a requirement to replace already-correct 2D equalizers.

### 2. Analytic fields: evaluate both coordinates

Use X and Y in the existing field rather than interpreting `width * height`
as one long coordinate. LedFx Plasma combines Cartesian and radial terms;
WLED Noise2D evaluates X, Y and time in a short nested loop.
[LedFx Plasma][plasma], [WLED Noise2D][wnoise]

Preserve current audio modulation, color conversion and normalization. Do not
add per-frame minimum/maximum normalization casually: that changes amplitude
semantics. LedFx's own Plasma normalization has an explicit constant-field
guard. An existing multidimensional sampler or a small analytic expression is
preferable to a new dependency or general coordinate framework. [LedFx Plasma][plasma]

### 3. Scans and trails: add position or history

For scans, retain phase and bounce state, then map heads onto bounded
two-coordinate trajectories with compact footprints. WLED Lissajous demonstrates
separate sine/cosine coordinates and fading. For particles, use bounded
population and lifetime rather than creating a simulation per pixel.
[WLED Lissajous][lissajous], [LedFx rain population][rain-count],
[LedFx rain lifetime][rain-life]

For scroll families, one axis can represent frequency and the other elapsed
history. Keep one width-by-height history surface and the fractional time
remainder. LedFx Waterfall and WLED Funky Plank provide concrete examples.
On a wide, shallow matrix, horizontal travel may suit four lanes with 80
history positions, but only if it respects the response's direction controls.
[LedFx history][history], [LedFx remainder][remainder], [WLED Funky Plank][plank]

## What should not change

Preserve audio source/range/gain, smoothing, thresholds, speed meaning, phase,
direction, palette, brightness, seed policy and saved settings. Adapt geometry
after those calculations. LedFx Scan expresses speed as a fraction of length
per second and advances with elapsed time: changing geometry should not silently
change traversal duration. [LedFx Scan][scan]

Whole-field pulses and scalar brightness responses do not need artificial
spatial motion. LedFx Magnitude multiplies an existing gradient by one audio
scalar. Uniform response is not inherently a rendering defect.
[LedFx Magnitude][magnitude], [LedFx gradient][gradient]

Budget at most the required output/history plane plus bounded particle state.
Do not copy FastLED NoisePlusPalette's example-specific square allocation:
using the larger dimension squared would allocate 6,400 cells for an 80x4
matrix rather than 320. [FastLED allocation][noise-memory]

Reject generic topology frameworks, new response enums and per-pixel particle
systems. WLED treats extrusion as an explicit mapping choice. Repeating a row
may be a valid look, but it does not establish independent 2D behavior.
[WLED extrusion][extrusion]

## Verification limits

This research did not inspect local QLC+ implementations or prove Artistic
preservation. Arithmetic checks covered finite coordinates, gap-free band
partitions and fractional coverage on wide, square, odd and singleton layouts.
The local source audit and native previews must determine which adaptations
are necessary; visual feedback must judge whether they improve the effects.

[twod]: https://github.com/LedFx/LedFx/blob/87583f9638fbd749bc0953f35334d8d5140c882f/ledfx/effects/twod.py#L117-L160
[eq-audio]: https://github.com/LedFx/LedFx/blob/87583f9638fbd749bc0953f35334d8d5140c882f/ledfx/effects/equalizer2d.py#L186-L209
[xy-normalized]: https://github.com/Aircoookie/WLED/blob/915565fcb896f3d3eec52aba0d53190fb34cd0cd/wled00/FX_2Dfcn.cpp#L188-L203
[noise-scale]: https://github.com/FastLED/FastLED/blob/df106dfe6a2124c03143ef611c7d1174ea70f06c/examples/NoisePlusPalette/NoisePlusPalette.h#L156-L166
[ellipse]: https://github.com/LedFx/LedFx/blob/87583f9638fbd749bc0953f35334d8d5140c882f/ledfx/effects/equalizer2d.py#L291-L298
[scroll-guards]: https://github.com/LedFx/LedFx/blob/87583f9638fbd749bc0953f35334d8d5140c882f/ledfx/effects/waterfall2d.py#L193-L224
[fast-xy]: https://github.com/FastLED/FastLED/blob/df106dfe6a2124c03143ef611c7d1174ea70f06c/examples/XYMatrix/XYMatrix.ino#L94-L125
[geq]: https://github.com/Aircoookie/WLED/blob/915565fcb896f3d3eec52aba0d53190fb34cd0cd/wled00/FX.cpp#L7567-L7592
[plasma]: https://github.com/LedFx/LedFx/blob/87583f9638fbd749bc0953f35334d8d5140c882f/ledfx/effects/plasma2d.py#L81-L108
[wnoise]: https://github.com/Aircoookie/WLED/blob/915565fcb896f3d3eec52aba0d53190fb34cd0cd/wled00/FX.cpp#L5834-L5848
[lissajous]: https://github.com/Aircoookie/WLED/blob/915565fcb896f3d3eec52aba0d53190fb34cd0cd/wled00/FX.cpp#L5684-L5700
[rain-count]: https://github.com/LedFx/LedFx/blob/87583f9638fbd749bc0953f35334d8d5140c882f/ledfx/effects/digitalrain2d.py#L207-L227
[rain-life]: https://github.com/LedFx/LedFx/blob/87583f9638fbd749bc0953f35334d8d5140c882f/ledfx/effects/digitalrain2d.py#L248-L285
[history]: https://github.com/LedFx/LedFx/blob/87583f9638fbd749bc0953f35334d8d5140c882f/ledfx/effects/waterfall2d.py#L115-L133
[remainder]: https://github.com/LedFx/LedFx/blob/87583f9638fbd749bc0953f35334d8d5140c882f/ledfx/effects/waterfall2d.py#L226-L238
[plank]: https://github.com/Aircoookie/WLED/blob/915565fcb896f3d3eec52aba0d53190fb34cd0cd/wled00/FX.cpp#L7627-L7643
[scan]: https://github.com/LedFx/LedFx/blob/87583f9638fbd749bc0953f35334d8d5140c882f/ledfx/effects/scan.py#L106-L162
[magnitude]: https://github.com/LedFx/LedFx/blob/87583f9638fbd749bc0953f35334d8d5140c882f/ledfx/effects/magnitude.py#L29-L33
[gradient]: https://github.com/LedFx/LedFx/blob/87583f9638fbd749bc0953f35334d8d5140c882f/ledfx/effects/gradient.py#L202-L209
[noise-memory]: https://github.com/FastLED/FastLED/blob/df106dfe6a2124c03143ef611c7d1174ea70f06c/examples/NoisePlusPalette/NoisePlusPalette.h#L69-L98
[extrusion]: https://github.com/Aircoookie/WLED/blob/915565fcb896f3d3eec52aba0d53190fb34cd0cd/wled00/FX_fcn.cpp#L801-L810
