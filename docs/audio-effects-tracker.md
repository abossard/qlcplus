# Audio-Reactive Effects — Implementation Tracker

## Plan

### Step 0: Fix audiospectrum.js sensitivity
- [ ] GPT-5.5 implement: 3x gain, minimum bar height, sensitivity scaling
- [ ] Opus 4.7 review
- [ ] Build passes

### Step 1: Audio Split Tower
- [ ] GPT-5.5 implement `audiosplittower.js`
- [ ] Opus 4.7 review
- [ ] Build passes

### Step 2: Audio Hue Shift
- [ ] GPT-5.5 implement `audiohueshift.js`
- [ ] Opus 4.7 review
- [ ] Build passes

### Step 3: Audio Bass Laser
- [ ] GPT-5.5 implement `audiobasslaser.js`
- [ ] Opus 4.7 review
- [ ] Build passes

### Step 4: Audio Shockwave
- [ ] GPT-5.5 implement `audioshockwave.js`
- [ ] Opus 4.7 review
- [ ] Build passes

### Step 5: Audio Fireworks
- [ ] GPT-5.5 implement `audiofireworks.js`
- [ ] Opus 4.7 review
- [ ] Build passes

### Step 6: Final build + commit
- [ ] Full build
- [ ] All scripts load without errors
- [ ] Commit

## API Rules (all scripts must follow)

```js
var algo = new Object;
algo.apiVersion = 3;
algo.name = "Audio Something";
algo.author = "QLC+ contributors";
algo.acceptColors = N;  // 0-5
algo.usesAudio = true;
algo.properties = new Array();
// presets via algo.presetXxx + properties push

algo.rgbMapStepCount = function(width, height) { return 1; };
algo.rgbMapSetColors = function(rawColors) { };
algo.rgbMapGetColors = function() { return []; };

algo.rgbMap = function(width, height, rgb, step, audio) {
    var map = LedFx.createMap(width, height);
    if (!audio || !audio.spectrum || audio.spectrum.length === 0) return map;
    // ... effect logic ...
    return map;
};
```

### Available helpers
- ✅ `new LedFx.ExpFilter(decay, rise)`
- ✅ `LedFx.lows_power(audio)`, `mids_power(audio)`, `high_power(audio)`
- ✅ `LedFx.melbank(audio, numBands)`
- ✅ `LedFx.hsv2rgb(h, s, v)` — NOT hslToRgb
- ✅ `LedFx.rgb(r, g, b)`
- ✅ `LedFx.createMap(width, height)`
- ✅ `algo.displayWidth`, `algo.displayHeight` — physical size for rotation

### NOT available
- ❌ `ExpFilter(...)` without `new LedFx.`
- ❌ `LedFx.hslToRgb()`
- ❌ `LedFx.rgbw()`

### Design rules
- Never fully black when audio is playing — always have minimum activity
- Use 3x+ amplification on melbank values
- Include `Sensitivity` / `Gain` presets
- Use `algo.displayWidth` for band calculations (rotation support)
- Additive blending: `Math.min(255, existing + new)` manually
- State persists on `algo` object between `rgbMap()` calls

## Progress Log

(Updated as steps complete)

---

## Audio Power Enhancement Plan (refined after Opus 4.7 critique)

### v1 Scope: 7 parameters, Classic/Modern mode

| Param | Type | Default | Behavior |
|-------|------|---------|----------|
| **Mode** | Classic/Modern | Classic | Classic = exact current behavior. Modern = 2D particles + new features |
| **Reactivity** | 1-10 | 5 | ExpFilter rise speed. 10=instant punch, 1=smooth |
| **Gain** | 1-10 | 5 | Pre-amplification on all audio bands |
| **Floor** | 0-10 | 1 | Noise gate: `bright = max(0, bright - floor*0.05)` |
| **SparkDensity** | 1-10 | 3 | Particles per beat/transient event |
| **SparkGravity** | Float Up / Burst | Float Up | Particle physics mode |
| **BassMode** | Edge Fill / Pulse / Off | Edge Fill | How bass overlay renders |

### Dropped from v1 (deferred)
- ~~Contrast~~ — Floor+Gain are sufficient
- ~~SparkSize~~ — 1px fine on typical matrices
- ~~SparkTrail~~ — tied to gravity mode implicitly
- ~~SparkColor~~ — needs gradient-interaction design
- ~~UseY~~ — belongs in a separate spectrum script

### Key design decisions
- **Classic mode = backward compat** — must reproduce exact current behavior
- **Particles**: pool of 64, pre-allocated in init(), Float Up or Burst physics
- **Bass/mids/highs**: each gets own ExpFilter, controlled by Reactivity
- **Spawn zones**: bass=bottom, mids=middle, highs=top
- **NO gamma contrast curve** — mathematically wrong, use Floor+Gain instead
- **Spark colors follow user gradient** (Color1→Color2), not hardcoded R/G/B

### Effort: ~3h implementation + 1h testing

---

## Shared Audio Params — Status After Rollout

### AudioParams helper: ✅ Implemented
- `audio_common.js` loaded globally via engine
- `installContinuous()` / `installTrigger()` available to all scripts
- All 29 scripts call install

### Wiring Status

| Param | Wired | Not wired | Notes |
|-------|-------|-----------|-------|
| **Reactivity** | 24/29 | 5 | Most scripts use createFilter — good coverage |
| **Gain** | 9/29 | 20 | Many scripts still use hardcoded `* 3` instead of gainFactor() |
| **Floor** | 2/29 | 27 | Most scripts don't apply floor to their brightness output |
| **Sensitivity** | 5/6 | 1 (audiostrobe has old presetThreshold) | Trigger scripts mostly wired |

### Known Issue: Inert Sliders
Scripts that register params via install but don't use them show non-functional UI sliders. 
Priority: wire Gain and Floor into the top 10 most-used scripts next.

### Next Steps
- [ ] Wire gainFactor() into 20 remaining scripts
- [ ] Wire applyFloor() into ~15 continuous scripts  
- [ ] Fix audiostrobe duplicate threshold
- [ ] Fix audioshot/audiowater to use the params
