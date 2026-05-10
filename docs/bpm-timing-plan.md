# BPM-relative timing for audio RGB scripts

## Goal

Make all animation phase across `audio*.js` BPM-locked. `presetSpeed` becomes
**cycles per beat** (musical units), independent of frame rate or wall-clock
seconds. Effects keep animating when no BPM is detected via a sensible fallback
tempo.

---

## 1. New utility — `RGBUtil.beatTime()`

Add to `resources/rgbscripts/rgbutil.js` next to `time01()`.

### API

```js
/**
 * BPM-locked sawtooth 0→1.
 *
 * Returns a phase that completes `speed` cycles per beat.
 *   speed = 1.0  → 1 cycle per beat        (quarter note)
 *   speed = 2.0  → 2 cycles per beat       (8th note)
 *   speed = 4.0  → 4 cycles per beat       (16th note)
 *   speed = 0.5  → 1 cycle every 2 beats   (half note)
 *   speed = 0.25 → 1 cycle every 4 beats   (1 bar)
 *
 * Falls back to a default 120 BPM equivalent when bpm <= 0 (no audio /
 * no beat tracker), so visuals keep animating.
 *
 * Implementation strategy: convert BPM to a beat-period in ms, then run a
 * monotonic phase accumulator across frames. We do NOT use audio.beat.phase
 * directly because it resets each beat — leading to discontinuities when
 * speed != 1.0 or speed isn't an integer divisor of 1.
 *
 * @param {number} speed       Cycles per beat (>0). 0/NaN treated as 1.
 * @param {Object} state       Persistent accumulator: { phase: 0 } owned by caller.
 * @param {number} bpm         Tempo in beats/min. 0 / NaN → uses 120 fallback.
 * @param {number} dtMs        Frame delta from audio.timing.consumerDtMs.
 * @returns {number} sawtooth in [0, 1).
 */
RGBUtil.beatTime = function(speed, state, bpm, dtMs) {
    if (!state) return 0;
    if (!isFinite(speed) || speed <= 0) speed = 1;
    if (!isFinite(dtMs) || dtMs < 0) dtMs = 0;
    var effectiveBpm = (isFinite(bpm) && bpm > 0) ? bpm : 120;
    var beatMs = 60000 / effectiveBpm;
    // Advance phase by (dtMs / beatMs) * speed cycles.
    var inc = (dtMs / beatMs) * speed;
    var p = (state.phase || 0) + inc;
    p = p - Math.floor(p);     // wrap to [0,1)
    state.phase = p;
    return p;
};
```

### Why Option B (BPM→ms → accumulator), not Option A (use `audio.beat.phase`)

- **Option A** (read `audio.beat.phase` and detect wraps to count beats) gives
  perfect sync to detected beats but breaks for non-integer `speed` values:
  the in-beat fraction wraps from ~1 back to ~0 mid-cycle, producing visible
  glitches in any animation whose period isn't `1/N` beats.
- **Option B** is monotonic and stable. It uses BPM as the *rate* and lets the
  per-frame `dtMs` advance the accumulator. Drift relative to detected beat
  edges is bounded by BPM detector accuracy (which is fine for visuals).
- Beat-edge sync is still available: scripts that need a **kick** on the
  downbeat use `audio.beat.cosPulse`, `audio.beat.kick`, `audio.beat.fired`,
  or `audio.bar.downbeat` — those are independent of the phase accumulator.

### Helper convenience signature

To minimise script-level boilerplate, also add:

```js
/**
 * Same as beatTime(), but returns the value scaled to a 0..2π radians angle.
 * Convenient for `Math.sin(RGBUtil.beatAngle(...))`.
 */
RGBUtil.beatAngle = function(speed, state, bpm, dtMs) {
    return RGBUtil.beatTime(speed, state, bpm, dtMs) * 2 * Math.PI;
};
```

---

## 2. Caller pattern

Each script keeps **one accumulator state object per phase rate**. Multi-rate
scripts (glitch, crawler, melt) hold one state object per phase track:

```js
// module scope (closure)
var phaseSlow = { phase: 0 };
var phaseMed  = { phase: 0 };
var phaseFast = { phase: 0 };

// inside rgbMap:
var bpm = (audio.beat && audio.beat.bpm) || 0;
var dt  = audio.timing.consumerDtMs;
var speed = algo.presetSpeed; // now in beats

var t1 = RGBUtil.beatTime(speed * 0.5,  phaseSlow, bpm, dt);
var t2 = RGBUtil.beatTime(speed * 1.0,  phaseMed,  bpm, dt);
var t3 = RGBUtil.beatTime(speed * 4.0,  phaseFast, bpm, dt);
```

The relative ratios between phase tracks are preserved by multiplying
`presetSpeed` by per-track constants — exactly the same shape as the existing
`SWAY_SPEED * speed * sway` pattern, but now in musical units.

### Audio-reactivity boost

Existing code adds `lowPower * reactivity * AUDIO_TIME_BOOST_PER_FRAME_MS` to
the timestep. Convert that to extra dtMs for the accumulator:

```js
var boostMs = lowPower * reactivity * AUDIO_TIME_BOOST_PER_FRAME_MS;
RGBUtil.beatTime(speed, phaseSlow, bpm, dt + boostMs);
```

The "boost" stays in ms, so it is independent of BPM (an audio kick adds the
same temporal jolt regardless of tempo). This matches the existing intent.

---

## 3. New `presetSpeed` defaults — per-script mapping

| Script | Old default | Old units | New default | New unit (cycles/beat) | Musical meaning |
|---|---|---|---|---|---|
| `audiocrawler.js`  | 0.5 | gain on `65536*0.0003/(π·20)` | **1.0** | cycles/beat | sway = 1× quarter, chop derived |
| `audiomelt.js`     | 0.5 | gain on `65536*0.0005` | **0.5** | cycles/beat | half-note melt waves |
| `audioglitch.js`   | 0.5 | gain on five constants | **2.0** | cycles/beat | 8th-note base, fast layer = 2 bars worth of 16ths |
| `audioglitch2.js`  | 0.5 | seconds-based | **2.0** | cycles/beat | 8th-note glitch |
| `audiolava.js`     | 7   | int range 1..15, gain on 65536*0.0001 | **0.25** (range 0.0625..1.0) | cycles/beat | very slow lava: 1 cycle / 4 bars at default |
| `audioplasma.js`   | 1.0 | seconds gain | **0.5** | cycles/beat | half-note plasma drift |
| `audioaurora.js`   | 0.8 | seconds gain | **0.25** | cycles/beat | atmospheric, 1 cycle / 4 beats |
| `audiomeltsparkle.js` | 0.5 | LedFx parity (period = 65.536/(speed·20)) | **1.0** | cycles/beat | quarter-note hue drift |
| `audiosoap.js`     | 0.5 | seconds gain | **1.0** | cycles/beat | quarter-note phase advance |
| `audiotunnel.js`   | 1.0 | seconds gain | **0.5** | cycles/beat | half-note rotation |
| `audiovortex.js`   | 1.0 | seconds gain | **0.5** | cycles/beat | half-note swirl |
| `audioenergy2.js`  | 0.3 | seconds gain | **0.5** | cycles/beat | half-note phase |
| `audiowater.js`    | 5   | int range 1..15 | **2.0** (range 0.25..8.0) | cycles/beat | 8th-note ripple drops |
| `audiofire.js`     | 0.04 | per-ms heat advance | **n/a** (keep) | not phase-based | physics simulation, see §5 |
| `audioscanflare.js`| 30 | int 1..100, deg/frame | **0.5** (range 0..4) | cycles/beat | scan completes 1 sweep per 2 beats default |
| `audioscanmulti.js`| 30 | int 1..100 | **0.5** (range 0..4) | cycles/beat | same |
| `audiowaterfall.js`| 25 cols/s | scroll speed | **n/a** (keep cols/s) | physical scroll | see §5 — keep cols/sec |
| `audiodjlight.js`  | 30 | drift speed | **0.25** (range 0..2) | cycles/beat | slow color drift |
| `audioblocks.js`   | 0.5 | seconds-based modifier | **1.0** | cycles/beat | quarter-note block flips |
| `audiobuildup.js`  | 0.5 | (BuildSpeed, separate semantic) | **0.5** | unchanged scalar | see §6 |

### Property metadata change

For float speeds, change UI hints from arbitrary floats to a beat-aware label
in the property declaration:

```diff
- "name:presetSpeed|type:float|display:Speed|" +
+ "name:presetSpeed|type:float|display:Speed (cyc/beat)|" +
```

Range-typed speeds (lava, water, scanflare, scanmulti, djlight) should be
rewritten to use a float type with an explicit min/max via the existing
`type:float` slider — or kept as `type:range` with multiplied integer values.
Recommendation: convert these to `type:float` for consistency.

---

## 4. Per-script changes

For each script, the change is mechanical:

1. Add a per-rate state object at module scope.
2. Replace `timestep += dt + ...` accumulator and `RGBUtil.time01(K * speed, timestep)`
   calls with `RGBUtil.beatTime(speedMultiplier, stateObj, bpm, dt + boostMs)`.
3. Drop the per-script `K = 65536 * X` magic constants; encode the relative
   ratios as plain multipliers on `speed` (beats/cycle).

### 4.1 `audiocrawler.js`

**Before:**
```js
var SWAY_SPEED = 65536 * 0.0003 / (Math.PI * 20);
var CHOP_SPEED = 65536 * 0.0005 / (Math.PI * 20);
var PHASE_WRAP = Math.PI * 20;
var AUDIO_TIME_BOOST_PER_FRAME_MS = 50;
var timestep = 0;
// ...
timestep += dt;
timestep += lowPower * reactivity * AUDIO_TIME_BOOST_PER_FRAME_MS;
var t1 = RGBUtil.time01(SWAY_SPEED * speed * sway, timestep) * PHASE_WRAP;
var t2 = RGBUtil.time01(CHOP_SPEED * speed * chop, timestep) * PHASE_WRAP;
```

**After:**
```js
// Sway/chop ratios preserved: chop is ~1.67× sway (same as 0.0005/0.0003).
var SWAY_BEATS = 1.0;          // 1 cycle/beat at speed=1, sway=1
var CHOP_BEATS = 1.67;
var AUDIO_BOOST_MS_PER_FRAME = 50;
var swayState = { phase: 0 };
var chopState = { phase: 0 };
// inside rgbMap:
var bpm = (audio.beat && audio.beat.bpm) || 0;
var boost = lowPower * reactivity * AUDIO_BOOST_MS_PER_FRAME;
var t1 = RGBUtil.beatTime(SWAY_BEATS * speed * sway, swayState, bpm, dt + boost) * 2 * Math.PI;
var t2 = RGBUtil.beatTime(CHOP_BEATS * speed * chop, chopState, bpm, dt + boost) * 2 * Math.PI;
```
`PHASE_WRAP` was only there to multiply a 0..1 value back into radians; the
`* 2π` does the same thing.

Default `presetSpeed`: **0.5 → 1.0**.

### 4.2 `audiomelt.js`

**Before:**
```js
var MELT_SPEED_1 = 65536 * 0.0005;
var MELT_SPEED_2 = 65536 * 0.00065;
var COLOR_SPEED  = 65536 * 0.0001;
var timestep = 0;
timestep += dt;
timestep += lowPower * reactivity / speed * AUDIO_TIME_BOOST_PER_FRAME_MS;
var t1 = RGBUtil.time01(MELT_SPEED_1 * speed, timestep);
var t2 = RGBUtil.time01(MELT_SPEED_2 * speed, timestep);
var colorT = RGBUtil.time01(COLOR_SPEED * algo.presetColorSpeed, timestep);
```

**After:**
```js
var MELT_BEATS_1 = 1.0;        // ratio matches 0.0005 → 1.0
var MELT_BEATS_2 = 1.3;        // ratio matches 0.00065 / 0.0005
var COLOR_BEATS  = 0.2;        // ratio matches 0.0001 / 0.0005
var meltState1 = { phase: 0 };
var meltState2 = { phase: 0 };
var colorState = { phase: 0 };
// inside rgbMap:
var bpm = (audio.beat && audio.beat.bpm) || 0;
var boost = lowPower * reactivity / Math.max(0.001, speed) * AUDIO_TIME_BOOST_PER_FRAME_MS;
var t1     = RGBUtil.beatTime(MELT_BEATS_1 * speed, meltState1, bpm, dt + boost);
var t2     = RGBUtil.beatTime(MELT_BEATS_2 * speed, meltState2, bpm, dt + boost);
var colorT = RGBUtil.beatTime(COLOR_BEATS * algo.presetColorSpeed, colorState, bpm, dt + boost);
```

`presetColorSpeed` (range 1..10) keeps its semantics but now means
"colorBeats × that many cycles per beat" → at default 5 → 1 cycle per beat
for the color drift. Acceptable.

Default `presetSpeed`: **0.5 → 0.5** (unchanged numerically, semantics differ).

### 4.3 `audioglitch.js`

**Before:** five `time01()` calls with hand-tuned `65536 * X` constants.

**After:** preserve the **ratios** between PHASE_SPEED_* by encoding them
relative to the slow rate:

```js
// Old constants: 0.0005, 0.0025, 0.001, 0.00025, 0.01
//   → ratios vs SLOW (0.0005): 1, 5, 2, 0.5, 20
var BEATS_SLOW = 1.0;          // base = 1 cycle/beat at speed=1
var BEATS_MED  = 5.0;
var BEATS_T4   = 2.0;
var BEATS_T5   = 0.5;
var BEATS_FAST = 20.0;
var phaseSlow = { phase: 0 };
var phaseMed  = { phase: 0 };
var phaseT4   = { phase: 0 };
var phaseT5   = { phase: 0 };
var phaseFast = { phase: 0 };
// inside rgbMap:
var bpm = (audio.beat && audio.beat.bpm) || 0;
var boost = lowPower * reactivity / Math.max(0.001, speed) * AUDIO_TIME_BOOST_PER_FRAME_MS;
var t1 = RGBUtil.beatTime(BEATS_SLOW * speed, phaseSlow, bpm, dt + boost) * 2 * Math.PI;
var t2 = RGBUtil.beatTime(BEATS_SLOW * speed, phaseSlow, bpm, dt + boost); // same accumulator
var t3 = RGBUtil.beatTime(BEATS_MED  * speed, phaseMed,  bpm, dt + boost);
var t4 = RGBUtil.beatTime(BEATS_T4   * speed, phaseT4,   bpm, dt + boost) * 2 * Math.PI;
var t5 = RGBUtil.beatTime(BEATS_T5   * speed, phaseT5,   bpm, dt + boost);
var t6 = RGBUtil.beatTime(BEATS_FAST * speed, phaseFast, bpm, dt + boost);
```

Note: `t1` and `t2` shared the same `timestep` modifier in the original; keep
them sharing the same accumulator and call `beatTime()` only once per frame
per accumulator (read the returned value and the prior `state.phase` if
needed).

Default `presetSpeed`: **0.5 → 2.0** (so default = 8th-note base on the slow
phase, matching the original's perceived tempo).

### 4.4 `audioglitch2.js`

Already seconds-based. Convert the same way:

**Before:**
```js
var SPEED_SCALE = 0.5;
var PHASE_MULT_T1 = 0.5; ... var PHASE_MULT_T6 = 10.0;
algo.timestep += dt + (lowPower * reactivity01) / Math.max(0.001, speed);
var ts = algo.timestep * speed;
var t1 = ts * PHASE_MULT_T1;
```

**After:**
```js
var BEATS_T1 = 0.5; var BEATS_T2 = 0.5; var BEATS_T3 = 2.5;
var BEATS_T4 = 1.0; var BEATS_T5 = 0.25; var BEATS_T6 = 10.0;
var stT1 = { phase: 0 }; ... var stT6 = { phase: 0 };
// boost remains in ms; use 1000× because old code added seconds
var boostMs = (lowPower * reactivity01) / Math.max(0.001, speed) * 1000;
var t1 = RGBUtil.beatTime(BEATS_T1 * speed, stT1, bpm, dt + boostMs);
// etc.
```

Default `presetSpeed`: **0.5 → 2.0**.

### 4.5 `audiolava.js`

**Before:**
```js
var LAVA_SPEED_1 = 65536 * 0.0001;
var LAVA_SPEED_2 = 65536 * 0.0002;
var BASS_MOD_1 = 0.004;
var BASS_MOD_2 = 0.007;
elapsedMs += audio.timing.consumerDtMs;
var t1 = RGBUtil.time01(LAVA_SPEED_1 * speed * Math.max(1, 1 + lowPower * BASS_MOD_1), elapsedMs);
var t2 = RGBUtil.time01(LAVA_SPEED_2 * speed * Math.max(1, 1 + lowPower * BASS_MOD_2), elapsedMs);
```

**After:**
```js
var LAVA_BEATS_1 = 1.0;        // ratio: SPEED_1 == base
var LAVA_BEATS_2 = 2.0;        // ratio: SPEED_2 / SPEED_1 == 2
var BASS_MOD_1 = 0.004;
var BASS_MOD_2 = 0.007;
var lavaState1 = { phase: 0 };
var lavaState2 = { phase: 0 };
// inside rgbMap:
var bpm = (audio.beat && audio.beat.bpm) || 0;
var bassMul1 = Math.max(1, 1 + lowPower * BASS_MOD_1);
var bassMul2 = Math.max(1, 1 + lowPower * BASS_MOD_2);
var t1 = RGBUtil.beatTime(LAVA_BEATS_1 * speed * bassMul1, lavaState1, bpm, dt);
var t2 = RGBUtil.beatTime(LAVA_BEATS_2 * speed * bassMul2, lavaState2, bpm, dt);
```

Bass modulation is now multiplicative on rate (cycles/beat goes up with bass)
— same as before, but in beat units.

Default `presetSpeed`: **7 → 0.25** (range becomes `type:float` 0.0625..2.0).

### 4.6 `audioplasma.js`

**Before:**
```js
elapsedSec += dt * speed * (1 + power * algo.presetReactivity) * (1 + 0.5 * noveltyMax);
// elapsedSec used directly inside Math.sin(...)
```

**After:** plasma uses `elapsedSec` as a continuous angle, not as a
periodic 0..1 phase. Convert to **beat-locked angle**:

```js
var plasmaState = { phase: 0 };
var bpm = (audio.beat && audio.beat.bpm) || 0;
var rateMul = (1 + power * algo.presetReactivity) * (1 + 0.5 * noveltyMax);
// 1 "cycle" of beatTime here = 2π radians of plasma rotation
var theta = RGBUtil.beatTime(speed * rateMul, plasmaState, bpm, dt) * 2 * Math.PI;
// pass theta where elapsedSec was used:
//   v1 = Math.sin(px * F + theta) * Math.cos(py * F - theta);
```

Default `presetSpeed`: **1.0 → 0.5**.

### 4.7 `audioaurora.js`

**Before:**
```js
elapsedSec += dt * speed;
// used as: elapsedSec * layerSpeed inside Math.sin
```

**After:**
```js
var auroraState = { phase: 0 };
var bpm = (audio.beat && audio.beat.bpm) || 0;
var theta = RGBUtil.beatTime(speed, auroraState, bpm, dt) * 2 * Math.PI;
// inside layer loop, replace `elapsedSec * layerSpeed` with `theta * layerSpeed`
```

Default `presetSpeed`: **0.8 → 0.25** (atmospheric).

### 4.8 `audiomeltsparkle.js`

This script uses `algo.timestep` as a continuous angle (not 0..1) and feeds it
through trig functions plus a special `t1Period = 65.536 / (speed*20)` window.
Two changes:

1. Replace the timestep accumulator with a beat-locked rate.
2. Make `t1` use `beatTime()` directly instead of the LedFx period formula.

**Before:**
```js
var T1_PERIOD_BASE = 65.536;
var T1_SPEED_MULT = 20.0;
algo.timestep += dt * algo.direction;
algo.timestep += lowPower * reactivity01 * speed01 * dt * algo.direction;
var t1Period = T1_PERIOD_BASE / Math.max(0.001, speed01 * T1_SPEED_MULT);
var t1 = ((algo.timestep / t1Period) % 1.0 + 1.0) % 1.0;
// also: hue uses `algo.timestep * HUE_DRIFT_FAST`
```

**After:**
```js
var T1_BEATS = 4.0;            // 4 cycles/beat at speed=1 (LedFx parity ≈ 16ths)
var HUE_FAST_BEATS = 0.3 * 0.05; // tuned so hue drift matches old visual rate
var HUE_SLOW_BEATS = 0.1 * 0.05;
var t1State = { phase: 0 };
var hueFastState = { phase: 0 };
var hueSlowState = { phase: 0 };
// inside rgbMap:
var bpm = (audio.beat && audio.beat.bpm) || 0;
var boost = lowPower * reactivity01 * speed01 * dtMs; // ms
var dirDt = (dtMs + boost) * algo.direction;          // direction can flip → negative dt OK (beatTime handles)
var t1 = RGBUtil.beatTime(T1_BEATS * speed01, t1State, bpm, dirDt);
var hueFast = RGBUtil.beatTime(HUE_FAST_BEATS * speed01, hueFastState, bpm, dirDt);
var hueSlow = RGBUtil.beatTime(HUE_SLOW_BEATS * speed01, hueSlowState, bpm, dirDt);
// In the strip loop, replace `algo.timestep * HUE_DRIFT_FAST` with `hueFast`
// (already 0..1 wrapped) and likewise HUE_DRIFT_SLOW → hueSlow.
// `bassFactor * algo.direction` stays as a pixel-level shift, unrelated to time.
```

⚠ **Caveat:** `beatTime` as specified takes positive `dtMs`. We need to
generalise it to accept signed dt for direction-flip scripts:

```js
RGBUtil.beatTime = function(speed, state, bpm, dtMs) {
    if (!state) return 0;
    if (!isFinite(speed) || speed <= 0) speed = 1;
    if (!isFinite(dtMs)) dtMs = 0;
    var effectiveBpm = (isFinite(bpm) && bpm > 0) ? bpm : 120;
    var beatMs = 60000 / effectiveBpm;
    var inc = (dtMs / beatMs) * speed;
    var p = (state.phase || 0) + inc;
    p = p - Math.floor(p);     // works for negative inc too
    state.phase = p;
    return p;
};
```
(The `Math.floor` wrap handles negative phases correctly.)

Default `presetSpeed`: **0.5 → 1.0**.

### 4.9 `audiosoap.js`

**Before:**
```js
phaseX += move;        move = audioSpeed^2 * 0.5 * dt;     audioSpeed = speed * (1 + power*react*6)
phaseY += move * Y_MOVE_RATIO;
```

These are 2D noise offsets, not periodic phases. Replacement:

```js
// drive offset advance with a beat-locked rate, but keep it continuous.
var phaseXState = { phase: 0 };
var phaseYState = { phase: 0 };
var rateBeats = speed * audioGain;     // audioGain = 1 + power*react*6
// One full "cycle" advances phaseX by 1.0; tune NOISE_SCROLL_PER_CYCLE to
// match prior visual speed.
var NOISE_SCROLL_PER_CYCLE = 1.0;
var pxNew = RGBUtil.beatTime(rateBeats, phaseXState, bpm, dt) * NOISE_SCROLL_PER_CYCLE;
// Use the *delta* of the wrapped phase to advance phaseX continuously:
phaseX += deltaSinceLastFrame;  // see helper below
```

Soap's offsets are non-periodic — they need a **continuous** position, not a
sawtooth. So for this script, expose a complementary helper:

```js
/**
 * Beat-locked continuous accumulator (does NOT wrap). Useful for noise
 * field offsets where any phase wrap creates visible discontinuity.
 *
 * Returns the current cumulative position; caller should not assume any
 * range. `state.position` is the persistent counter.
 */
RGBUtil.beatPosition = function(speed, state, bpm, dtMs) {
    if (!state) return 0;
    if (!isFinite(speed) || speed <= 0) speed = 1;
    if (!isFinite(dtMs)) dtMs = 0;
    var bpmEff = (isFinite(bpm) && bpm > 0) ? bpm : 120;
    var inc = (dtMs / (60000 / bpmEff)) * speed;
    state.position = (state.position || 0) + inc;
    return state.position;
};
```

Use `beatPosition` for noise scroll (`audiosoap`, `audiowater` x/y phases),
and `beatTime` for sawtooth phases.

Default `presetSpeed`: **0.5 → 1.0**.

### 4.10 `audiotunnel.js` / `audiovortex.js`

Both use a single `angle += dt * speed * (1 + power*react)` continuous angle.
Use `beatPosition`:

```js
var angleState = { position: 0 };
var rateBeats = speed * (1 + power * algo.presetReactivity);
var angle = RGBUtil.beatPosition(rateBeats, angleState, bpm, dt) * 2 * Math.PI;
```

Default `presetSpeed`: **1.0 → 0.5**.

### 4.11 `audioenergy2.js`

Same pattern as tunnel/vortex — `algo.phase = mod1(phase + dt*speed*0.5)`:

```js
algo.phase = RGBUtil.beatTime(speed * SPEED_SCALE_BEATS, energyState, bpm, dt);
// (replaces the manual mod1 + accumulator)
```

Constant `SPEED_SCALE = 0.5` becomes `SPEED_SCALE_BEATS = 0.5`. Default
`presetSpeed`: **0.3 → 0.5**.

### 4.12 `audioblocks.js`

Has its own `ledFxTime()` clone. Replace with `beatTime`:

**Before:**
```js
algo.timestep += dtMs / 1000.0;
var t1 = ledFxTime(1.0 * speed, algo.timestep);
```

**After:**
```js
var blocksState = { phase: 0 };
var t1 = RGBUtil.beatTime(1.0 * speed, blocksState, bpm, dtMs);
```

Default `speed`: **0.5 → 1.0**.

### 4.13 `audiowater.js`

Multiple phases (mid, high1, high2). Each becomes a `beatPosition`:

```js
var midPhaseState = { position: 0 };
midPhase = RGBUtil.beatPosition(0.0002 * 1000 * speed_in_beats_scaled, midPhaseState, bpm, dt) % 1;
```
A simpler restatement: keep `presetSpeed` in beats; ratios 0.0002/0.0003/0.00025
preserved relative to each other. Default **5 → 2.0** (8th-note ripple drops).

The `speedSteps = floor(presetSpeed / 3)` simulation-sub-step count is *physical*,
not phase-based — keep it but rederive from speed in beats:
`speedSteps = clamp(round(presetSpeed * 1.5), 1, 5)`.

### 4.14 `audioscanflare.js` / `audioscanmulti.js`

Scanner position is accumulated in `position += speed * dt` form (deg/frame
units in original). Convert to "scan completes N sweeps per beat" via
`beatPosition`. Position wraps at end-of-strip → use sawtooth `beatTime`:

```js
var scanState = { phase: 0 };
var rateBeats = speed; // cycles per beat
var pos01 = RGBUtil.beatTime(rateBeats, scanState, bpm, dt);
var scanX = pos01 * width; // 0..width
```

Default speed: **30 → 0.5** (one full sweep every 2 beats), with bouncing
handled by the sawtooth → triangle conversion already in the script.

### 4.15 `audiodjlight.js`

Drift speed becomes cycles/beat for the color drift. Default **30 → 0.25**.

### 4.16 `audiowaterfall.js`

`presetSpeedHz` (cols/sec) is a **physical scroll rate**, not a phase. Keep it.
This script does **NOT** need conversion — the user sets a literal scroll
speed in columns per second, which is BPM-independent on purpose (you scroll
audio waterfalls by wall-clock time so old audio leaves the screen at a
predictable rate).

### 4.17 `audiofire.js`

Fire is a thermal physics simulation. `deltaScaled = deltaMs * adjustedSpeed`
controls heat advection rate. Keep ms-based — physics realism, not musical
phase.

**Optional refinement:** offer a `presetTempoSync` toggle that re-interprets
`presetSpeed` as cycles per beat for the *spark spawn* rate only. But not
required for this conversion.

### 4.18 `audiochaser.js`

Chaser sequence step uses `audio.power.low` / `audio.power.mid` as a level
trigger and is already beat-aware via `audio.beat.fired`. **No change needed.**

### 4.19 Other scripts that take `consumerDtMs` but don't accumulate phase

These are already physical/decay-based and **should not be converted**:

- `audiobarcode.js` — barcode shift, decay rates
- `audioblurz.js` — blur lifetime
- `audiocellular.js` — cellular automaton stepping
- `audiochromatic.js` — color blend decay
- `audioflowfield.js` — particle physics (ageMs)
- `audiogravimeter.js` — peak-hold age
- `audiopuddles.js` — drop spawn cadence (could optionally beat-sync)
- `audioreaction.js` / `audioreactor.js` — reactive only
- `audioscan.js` — scan position uses dt for smoothing
- `audiowavelength.js` — frame-relative dt scaler
- `audiostrobe.js` — strobe cooldowns are wall-clock musical timing (kept as ms)

Decision: leave these alone. They are reactive/decay effects, not phase
animators. Beat sync there should come from `audio.beat.fired`/`audio.onset.fired`,
which already works.

---

## 5. Scripts that DON'T need conversion

| Script | Reason |
|---|---|
| `audio_colors.js` | Direct color mapping, no time accumulator |
| `audiobarcode.js` | Decay/scroll based on dt physics |
| `audiobasslaser.js` | Pure beat/onset-reactive |
| `audiobeatcolors.js` | Already uses `audio.beat.bpm`, `audio.beat.phase`, `audio.bar.beat` |
| `audioblurz.js` | Blur lifetime in ms (physical) |
| `audiocellular.js` | Cellular automaton |
| `audiochaser.js` | Beat-fired stepping |
| `audiochromatic.js` | Color decay |
| `audioequalizer.js` | Spectrum bars only |
| `audiofire.js` | Heat physics |
| `audiofireworks.js` | Particle physics |
| `audioflowfield.js` | Particle ageMs |
| `audiogravimeter.js` | Peak-hold |
| `audiohueshift.js` | Already band-driven |
| `audiopower.js` | Pure level meter |
| `audiopuddles.js` | Spawn cadence (physical) |
| `audioreaction.js` | Reactive only |
| `audioreactor.js` | Reactive only |
| `audioscan.js` | dt smoothing |
| `audiosplittower.js` | Spectrum mirror |
| `audiospectrum.js` | Spectrum bars |
| `audioshockwave.js` | Onset-triggered ring |
| `audioshot.js` | Onset triggered |
| `audiostrobe.js` | Strobe cooldowns are intentionally wall-clock |
| `audiowaterfall.js` | cols/s scroll is intentional physical rate |
| `audiowavelength.js` | dt scaler only |

**Yes, these scripts get converted:**
audiocrawler, audiomelt, audioglitch, audioglitch2, audiolava, audioplasma,
audioaurora, audiomeltsparkle, audiosoap, audiotunnel, audiovortex,
audioenergy2, audioblocks, audiowater, audioscanflare, audioscanmulti,
audiodjlight. (17 scripts total.)

Plus `audiobuildup.js` (special case below).

---

## 6. `audiobuildup.js` — beat-relative state machine

`audiobuildup` was recently converted from frame-count to ms-based timing.
Its phase durations are perceptual (build = "a few seconds", drop flash =
"~1 frame"), and they are not currently musical.

### Recommendation: **musical-aware durations gated by an opt-in toggle**

Add a property `presetTempoSync` (Yes/No, default **No** to preserve current
behaviour). When **Yes**:

| Constant | Wall-clock value | Beat-relative value @ 120 BPM | Musical meaning |
|---|---|---|---|
| `BUILD_MIN_MS`     | 500   | 1 beat × beatMs       | 1 beat |
| `BUILD_MAX_MS`     | 4000  | 8 beats × beatMs      | 2 bars |
| `BUILD_ABORT_MS`   | 1000  | 2 beats × beatMs      | half bar |
| `PEAK_MAX_MS`      | 700   | 1.5 beats × beatMs    | 1.5 beats |
| `DROP_MS`          | 400   | 1 beat × beatMs       | 1 beat |
| `DROP_FLASH_MS`    | 60    | 0.125 beats × beatMs  | 32nd note |
| `DROP_EXIT_MS`     | 400   | 1 beat × beatMs       | 1 beat |
| `POSTDROP_MS`      | 1000  | 2 beats × beatMs      | half bar |
| `POSTDROP_EXIT_MS` | 700   | 1.5 beats × beatMs    | 1.5 beats |
| `COOLDOWN_MS`      | 1500  | 4 beats × beatMs      | 1 bar |
| `BASS_ABSENT_MS`   | 200   | 0.5 beats × beatMs    | 8th note |

Implementation:

```js
function durMs(constMs, beats, bpm) {
    if (!algo.presetTempoSync) return constMs;
    var b = (isFinite(bpm) && bpm > 0) ? bpm : 120;
    return beats * (60000 / b);
}
// e.g.
if (algo.stateElapsedMs > durMs(BUILD_MIN_MS, 1.0, audioBpm)) ...
```

The buildup keeps a defensible default (off) so users with existing presets
don't lose tuning. Power users on a beat-locked DJ workflow flip it on.

Also: the per-frame decay/progress rates (`IDLE_BUILD_DECAY_PER_S`,
`BUILD_PROGRESS_INC_PER_S`, `BUILD_RAINBOW_SPEED_PER_S`) stay in per-second
units — they're modulation rates, not durations. Optionally rescale them to
"per beat" when sync is on:

```js
var rateScale = algo.presetTempoSync ? (60 / Math.max(40, bpm)) : 1; // 1 at 60BPM
algo.buildProgress += features.buildScore * buildSpeedFactor *
                      BUILD_PROGRESS_INC_PER_S * dtSec * rateScale;
```

---

## 7. Edge cases & fallbacks

1. **No audio object** — already handled by `if (!audio) return map;` early return.
2. **`audio.beat.bpm === 0`** — `beatTime` substitutes 120 BPM internally.
   Effects continue to animate at the fallback tempo.
3. **`audio.beat` undefined** (legacy `audio` object without beat tracker) —
   guard with `var bpm = (audio.beat && audio.beat.bpm) || 0;` everywhere.
4. **dtMs spike** (e.g., GC pause) — `beatTime` advances proportionally; no
   special handling. The phase stays continuous.
5. **`speed = 0`** — clamped to 1 inside `beatTime`. Scripts that want a
   "frozen" effect should not pass 0 here; they should bypass the call.
6. **Negative `speed`** — clamped to 1. Direction reversal handled via signed
   `dtMs` (used by audiomeltsparkle).
7. **Save/load** — the per-script accumulator state lives in module-level
   closure variables and resets to 0 each time the script is reloaded
   (matrix re-init). That's the same behaviour as today's `timestep` /
   `elapsedMs` / `elapsedSec` resets, so no migration required.
8. **Project files (.qxw)** — `presetSpeed` is persisted as a float. Existing
   workspaces will load with the same numeric value but a new semantic.
   Document this in the release notes; optionally add a one-time scaling step
   per script in its loader (out of scope of this plan).

---

## 8. Order of work

1. **rgbutil.js** — add `beatTime`, `beatAngle`, `beatPosition`. (Single PR.)
2. **Pilot conversion** — `audiocrawler.js` + `audiomelt.js` (familiar, low risk).
   Test by running QLC+ and comparing visuals at 120 BPM (should match prior
   default), then 60 BPM (visibly half-speed) and 180 BPM (visibly faster).
3. **Bulk conversion** — remaining 14 scripts (§4.3 onward), one commit each
   for reviewability.
4. **audiobuildup.js** — add `presetTempoSync` toggle (separate PR, since it
   changes UI surface).
5. **Docs** — add a short note in `docs/` (or in each script header) clarifying
   that `presetSpeed` is now in cycles/beat.
6. **No tests** — RGB scripts have no unit-test harness. Validate by visual
   playback in QLC+ with known BPM source (audio file with strong beat or
   internal beat generator at fixed BPM).

---

## 9. Summary

- Add **3** util functions: `beatTime` (sawtooth), `beatAngle` (radian helper),
  `beatPosition` (continuous accumulator).
- Convert **17** scripts to BPM-locked phase.
- Special-case **1** script (`audiobuildup`) with an opt-in tempo-sync toggle.
- Leave **~25** scripts unchanged because they're reactive/physical, not
  phase-driven.
- Fallback BPM = **120**, applied transparently inside `beatTime` so no script
  needs to handle the no-audio case explicitly.
- All `presetSpeed` defaults rescaled to musically-sensible "cycles per beat"
  values so default playback at 120 BPM matches roughly the prior visual rate.
