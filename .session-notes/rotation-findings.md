# RGB Matrix Rotation Bug Investigation — Audio Block at 90°/270°

## TL;DR

The hypothesis (rotated map dims don't match fixture group dims) is **NOT** the
root cause. The rotation/transform code in `engine/src/rgbmatrix.cpp` is
mathematically correct and the destination map produced by `applyTransforms()`
**always** matches the fixture group dimensions, regardless of rotation angle.
This is verified by the unit tests in `mcp/test/rgb_transform_test.cpp`, which
all pass (including `rotation90_nonSquare` and `rotation270_nonSquare` on a
4×2 grid).

The all-black-at-90/270 symptom — when only "Audio Block" / audio scripts are
affected — points instead at the **audio script's behaviour when the algorithm
size is the swapped (height, width)**, in particular the v3 LedFx audio scripts
in `resources/rgbscripts/`.

## Code references — what actually happens during rotation

### 1. `effectiveAlgorithmSize` swaps width/height for 90°/270°

`engine/src/rgbmatrix.cpp:1280-1286`:
```cpp
QSize RGBMatrix::effectiveAlgorithmSize(const FixtureGroup *grp) const
{
    QSize s = grp->size();
    if (m_rotation == 1 || m_rotation == 3)
        s = QSize(s.height(), s.width());
    return s;
}
```

For a 16×5 group:
- rotation 0 / 180: algoSize = `(16, 5)`
- rotation 90 / 270: algoSize = `(5, 16)` (swapped)

### 2. The algorithm is asked to render at the swapped size

`engine/src/rgbmatrix.cpp:763-768` (the live `write()` path):
```cpp
QSize algoSize = effectiveAlgorithmSize(m_group);
m_runAlgorithm->rgbMap(algoSize, m_stepHandler->stepColor().rgb(),
                       m_stepHandler->currentStepIndex(), m_stepHandler->m_map);
if (m_rotation || m_mirror)
    applyTransforms(m_stepHandler->m_map, algoSize, m_group->size(),
                    m_rotation, m_mirror, m_mirrorBlend);
```

Same pattern in `previewMap()` at line 339-344.

So the algorithm receives e.g. `(width=5, height=16)` and is expected to
return a map with **16 rows × 5 cols**. After that, `applyTransforms` rotates
back to 5 rows × 16 cols (matching the fixture group).

### 3. The rotation transform itself

`engine/src/rgbmatrix.cpp:1330-1428` (`applyTransforms`):
```cpp
int dw = dstSize.width();   // group width
int dh = dstSize.height();  // group height
int sh = map.size();
int sw = (sh > 0) ? map[0].size() : 0;

RGBMap rotated;
rotated.resize(dh);
for (int y = 0; y < dh; y++) {
    rotated[y].resize(dw);
    rotated[y].fill(0);
}

for (int dy = 0; dy < dh; dy++) {
    for (int dx = 0; dx < dw; dx++) {
        int sx, sy;
        switch (rotation) {
            case 0: sx = dx;          sy = dy;          break;
            case 1: sx = dy;          sy = sh - 1 - dx; break;  // 90° CW
            case 2: sx = dw - 1 - dx; sy = dh - 1 - dy; break;
            case 3: sx = sw - 1 - dy; sy = dx;          break;  // 270° CW
        }
        if (sy >= 0 && sy < sh && sx >= 0 && sx < (int)map[sy].size())
            rotated[dy][dx] = map[sy][sx];
    }
}
…
map = rotated;   // map now has dst dimensions = group size
```

Bounds analysis when the algorithm honours the swapped algoSize
(group W=16, H=5):

| rot | sw | sh | dw | dh | sx range | sy range | covers all dst? |
|-----|----|----|----|----|----------|----------|-----------------|
| 0   | 16 | 5  | 16 | 5  | [0,16)   | [0,5)    | ✅              |
| 1   | 5  | 16 | 16 | 5  | dy∈[0,5) | 15-dx∈[0,15] | ✅          |
| 2   | 16 | 5  | 16 | 5  | [0,16)   | [0,5)    | ✅              |
| 3   | 5  | 16 | 16 | 5  | 4-dy∈[0,4] | dx∈[0,16) | ✅          |

So the destination map is fully populated and correctly sized in all four
rotations — **provided the source map dimensions match the swapped algoSize**.

### 4. The unit tests prove the transform works

`mcp/test/rgb_transform_test.cpp` (`mcp_rgb_transform_test`) — all 15 cases
pass on this checkout:
```
PASS   : RGBTransform_Test::rotation0_identity()
PASS   : RGBTransform_Test::rotation90_nonSquare()
PASS   : RGBTransform_Test::rotation180_nonSquare()
PASS   : RGBTransform_Test::rotation270_nonSquare()
PASS   : RGBTransform_Test::rotation_roundTrip()
…
Totals: 15 passed, 0 failed
```

The `rotation90_nonSquare` test specifically verifies that a 2-wide × 4-tall
source rotated 90° produces a fully populated 4-wide × 2-tall destination
with the expected pixel-for-pixel mapping.

## So where is the bug?

The transform is correct. `effectiveAlgorithmSize` is correct. The
fixture-group readout in `updateMapChannels` is correct (`map[pt.y()][pt.x()]`
with `pt` in group coordinates). The 0/180 vs 90/270 split is therefore
**not** caused by a mismatch between the rotated map and the fixture group.

The bug instead lives in the **audio-aware algorithm code paths** that are
sensitive to the algoSize being passed in swapped:

### Root cause candidate A — `RGBAudio::rgbMap` registers `m_bandsNumber = size.width()`

`engine/src/rgbaudio.cpp:155-161`:
```cpp
if (m_bandsNumber == -1)
{
    m_bandsNumber = size.width();
    m_audioInput->registerBandsNumber(m_bandsNumber);
    return;            // first frame: map left all-zero
}
```

For a 16×5 group:
- rot 0/180 → registers **16** bands
- rot 90/270 → registers **5** bands (because algoSize is swapped)

`AudioCapture::dataProcessed` only emits to `slotAudioBarsChanged` for sizes
that have been registered, so this in itself is not fatal — but if the
capture pipeline has any minimum-band assumption or the FFT bin count
collapses at very small N, the spectrum delivered for 5 bands may be empty
or noisy enough that nothing ever exceeds the brightness threshold.

This would explain the observed pattern *for the C++ "Audio Spectrum"
algorithm* — but the user reports "Audio Block", which is a JS script
(see candidate B).

### Root cause candidate B — v3 audio scripts (audioblocks.js, etc.)

`resources/rgbscripts/audioblocks.js:81-91`:
```js
algo.rgbMap = function(width, height, rgb, step, audio)
{
    var blockW = Math.max(1, algo.presetBlockSize);          // default 5
    var numBlocks = Math.ceil(width / blockW);
    if (!initialized || blockBrightness.length !== numBlocks) {
        blockBrightness = new Array(numBlocks);
        …
    }
    var map = LedFx.createMap(width, height);
    if (!audio || !audio.spectrum || audio.spectrum.length === 0) return map;
    …
}
```

For a 16×5 group with default `presetBlockSize = 5`:
- rot 0/180 → `width = 16` → `numBlocks = ceil(16/5) = 4` → 4 reactive blocks
- rot 90/270 → `width = 5`  → `numBlocks = ceil(5/5)  = 1` → **1** block

With `numBlocks = 1`:
1. `bands = LedFx.melbank(audio, 1)` — interpolates the entire 32-bin
   spectrum down to a single bin, which `LedFx.interpolate` computes as
   `arr[0] * 1 + arr[1] * 0 = arr[0]` (only the very lowest sub-bass bin).
2. `var trigger = bands[bi] * power * 3` — power is `lows_power(audio)`,
   already an average of just the lowest third.
3. The result is `(lowest_bin) × (avg of bottom third) × 3`. On most
   real-world audio (especially anything without strong sub-bass), this
   product stays below the `bright < 0.01 → continue` cutoff (line 121)
   for every frame.
4. The **decay-only branch** (`blockBrightness[bi] *= decayRate`) then drives
   the single block toward zero, and the whole grid stays black.

So 90°/270° turn the 4-band block visualiser into a 1-band sub-bass-only
visualiser whose threshold is almost never exceeded → all black.

The same pattern applies to every script that derives any per-x parameter
from `width`:
- `audioblocks.js`     → `numBlocks = ceil(width/blockW)`
- `audiowavelength.js` → `bands = LedFx.melbank(audio, width)`
- `audiospectrum.js`   → `bands = LedFx.melbank(audio, width)`
- `audiochaser.js`, `audioscan.js`, `audiotunnel.js`, `audiovortex.js`,
  `audiomelt.js`, `audioplasma.js` — each uses `width` as the horizontal
  axis of a horizontal-sweep / spectrum-bar animation.

When the matrix is rotated 90°/270°, each of these scripts is handed
`width = group.height()` (typically a tiny number like 5 for short LED
strips on a wide bar). At that small width, either:

a. The per-band brightness collapses below the decay/visibility threshold
   (audioblocks).
b. `LedFx.melbank(audio, width)` returns degenerate/quantised data.
c. Effects that index `width` are squashed into so few cells that nothing
   visibly lights up.

Then `applyTransforms` faithfully rotates that mostly-zero map back to the
group dimensions — so the user sees all-black at 90°/270°, while 0°/180°
(where `width` is unchanged) work fine.

## Is this a general rotation bug or audio-specific?

**Audio-specific.** Non-audio algorithms (`RGBPlain`, `RGBText`, `RGBImage`,
non-audio scripts) treat `width` and `height` symmetrically, or render
content that is invariant to orientation, so they show the expected rotated
output at 90°/270°.

The math in `applyTransforms` does not care about audio at all — it is the
combination of:

1. `effectiveAlgorithmSize` swapping (W,H) for rot 1/3, and
2. Audio scripts using the *swapped* `width` to drive band-count /
   block-count parameters that determine whether anything triggers.

…that produces the all-black result.

## Recommended fixes

The cleanest fix is to feed audio scripts the **post-rotation** (display)
width — i.e. always `group.width()` — for any band-count / block-count
computation. There are two ways to do this:

### Option 1 (engine-side) — pass display size to audio scripts

In `engine/src/rgbmatrix.cpp:763-768`, when the algorithm `usesAudio()`,
pass `m_group->size()` instead of the rotation-swapped `algoSize`, and
**still** call `applyTransforms` with `dstSize == srcSize == m_group->size()`.
This keeps non-audio algorithms benefiting from rotated rendering, while
audio scripts always see the display geometry their band layout was tuned
for. (Practically: the rotation simply has no visual effect on audio
scripts — which is actually fine for spectrum-bar style visuals where
"rotated bars" rarely makes physical sense anyway.)

### Option 2 (script-side) — clamp minimum band count

In each `resources/rgbscripts/audio*.js`, replace
`var numBlocks = Math.ceil(width / blockW);` (and similar) with
```js
var numBlocks = Math.max(MIN_BLOCKS, Math.ceil(width / blockW));
```
and render those bands across `width` even if some are off-screen. This
keeps the algorithm reactive at small widths but doesn't truly solve the
root cause (bands still sample wrong frequency ranges).

**Option 1 is recommended.** It's a one-line change in `rgbmatrix.cpp`
gated on `m_runAlgorithm->usesAudio()`.

## Files / line numbers consulted

- `engine/src/rgbmatrix.cpp:1273-1286` — `effectiveAlgorithmSize`
- `engine/src/rgbmatrix.cpp:1330-1428` — `applyTransforms`
- `engine/src/rgbmatrix.cpp:328-346`   — `previewMap`
- `engine/src/rgbmatrix.cpp:759-770`   — runtime `write()` rendering path
- `engine/src/rgbmatrix.cpp:870-915`   — `updateMapChannels`
- `engine/src/rgbalgorithm.h:38,84,110` — `RGBMap` typedef, `rgbMap` API,
  `usesAudio()`
- `engine/src/rgbaudio.cpp:136-185`    — built-in C++ Audio Spectrum
- `engine/src/rgbscriptv4.cpp:377-435` — JS script `rgbMap` adapter
- `engine/src/rgbscriptv4.cpp:531-629` — audio-capture wiring,
  `buildAudioDataObject`
- `resources/rgbscripts/audioblocks.js:81-144` — Audio Blocks script
- `resources/rgbscripts/ledfx_compat.js:98-148, 226-234` — `melbank`,
  `lows_power`, `createMap`
- `mcp/test/rgb_transform_test.cpp` — passing transform unit tests
