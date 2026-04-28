# Beam Ball Pattern Layer — Revised DJ Expression Plan

## Purpose

Integrate the Beam Ball 10-LED effects into **DJ Expression** as a maintainable fifth layer: **Pattern**.

The Beam Ball must not become a parallel control system. It is one fixture inside DJ Expression, and every function must respect DJX layer separation.

---

## Fixture Reference

| Item | Value |
|------|-------|
| Fixture | Beam Ball |
| Fixture ID | `7` |
| Mode | 49-channel |
| Heads | 10 RGBW LEDs |
| Movement | ch `0-4` |
| Master dimmer | ch `5` |
| Strobe | ch `6` |
| Per-head RGBW | ch `7-46` |
| Programmes | ch `47` |
| Mic sensitivity | ch `48` |

Per-head channels:

```text
R_CH = [7, 11, 15, 19, 23, 27, 31, 35, 39, 43]
G_CH = [8, 12, 16, 20, 24, 28, 32, 36, 40, 44]
B_CH = [9, 13, 17, 21, 25, 29, 33, 37, 41, 45]
W_CH = [10, 14, 18, 22, 26, 30, 34, 38, 42, 46]
```

`Engine.setFixture(7, channel, value)` uses these same fixture-local channel indexes.

---

## 1. Layer Architecture Update

DJ Expression becomes a five-layer system:

| Layer | Channels Owned | Controlled By | Rule |
|-------|----------------|---------------|------|
| Color | Beam Ball RGB only: ch `7,8,9 ... 43,44,45`; other fixtures keep their normal color channels | MOOD buttons | Sets color hue; never sets dimmer or Beam Ball W |
| Pattern | Beam Ball W only: ch `10,14,18,22,26,30,34,38,42,46` | PATTERN buttons | Sets per-head white intensity masks/accents; never sets RGB or dimmer |
| Dimmer | Beam Ball ch `5`, Hero dimmer, other master dimmers | PHASE + intensity slider | Sets global output level; never sets color or pattern |
| Movement | Pan/Tilt/Speed ch `0-4` | MOVEMENT buttons | Sets spatial motion only |
| FX | Strobe / momentary dimmer effects | FX buttons | Short-lived accents only |

### Decision: Pattern Owns Beam Ball White Channels

The Beam Ball 49-channel mode exposes RGBW per head but does **not** provide separate per-head dimmer channels. To keep DJX composable, the only stable split is:

- **Color owns Beam Ball RGB.**
- **Pattern owns Beam Ball W.**
- **Dimmer owns Beam Ball master dimmer ch5.**

This is the best tradeoff because it preserves live composability:

- MOOD can run continuously and define hue.
- PATTERN can add sparse white geometry, sparkles, comets, and build shapes.
- PHASE can still control intensity through ch5 without HTP collision.

### Non-Negotiable Channel Rules

- Ball Pattern scenes must **never** set ch `5`.
- Ball Pattern scenes must **never** set RGB channels.
- DJX Color scenes must stop setting Beam Ball W channels.
- DJX Dimmer scenes must continue to be the only owner of Beam Ball ch `5`.
- Scripts must not set ch `5` and must not set RGB unless they are explicitly documented as non-composable one-shot macros. This plan keeps scripts W-only.

### Required DJX Color Migration

Update all DJX Color functions:

- For Beam Ball, remove these W channels from every MOOD and Color Utility scene:
  `10,14,18,22,26,30,34,38,42,46`.
- Keep Beam Ball RGB channels in MOOD scenes.
- Keep Hero Spot Wash RGBW unchanged.
- `DJX White Hit` must set Beam Ball RGB to `255` but not Beam Ball W.
- `DJX Blackout Color` must set Beam Ball RGB to `0` but not Beam Ball W.
- Pattern blackout is handled by `DJX Pattern Release`.

---

## 2. Revised Scene Inventory

Target inventory: **39 Pattern scenes**.

These are intentionally building blocks. User-facing behavior comes from chasers, scripts, collections, and VC buttons.

All scenes live under `DJ Expression/Pattern/Scenes/` and use fixture `7` only.

### Universal Scene Rules

Every Pattern scene must:

- Remove channel `5` completely.
- Set only Beam Ball W channels.
- Set every W channel explicitly, including `0` for off heads, so scenes are self-contained.
- Leave all RGB channels untouched.
- Use zero-padded names for ordering.

### Keep + Modify

#### A. Singles — keep 10

Folder: `DJ Expression/Pattern/Scenes/Singles/`

| Scene | W values |
|-------|----------|
| `DJX Pattern LED 01` | LED 1 W=`255`, all other W=`0` |
| `DJX Pattern LED 02` | LED 2 W=`255`, all other W=`0` |
| `DJX Pattern LED 03` | LED 3 W=`255`, all other W=`0` |
| `DJX Pattern LED 04` | LED 4 W=`255`, all other W=`0` |
| `DJX Pattern LED 05` | LED 5 W=`255`, all other W=`0` |
| `DJX Pattern LED 06` | LED 6 W=`255`, all other W=`0` |
| `DJX Pattern LED 07` | LED 7 W=`255`, all other W=`0` |
| `DJX Pattern LED 08` | LED 8 W=`255`, all other W=`0` |
| `DJX Pattern LED 09` | LED 9 W=`255`, all other W=`0` |
| `DJX Pattern LED 10` | LED 10 W=`255`, all other W=`0` |

Purpose: chaser building blocks for single chase, sparkle, random, and ping-pong.

#### B. Core Patterns — keep 4

Folder: `DJ Expression/Pattern/Scenes/Core/`

| Scene | W values |
|-------|----------|
| `DJX Pattern Odd` | LEDs 1,3,5,7,9 W=`255`; others `0` |
| `DJX Pattern Even` | LEDs 2,4,6,8,10 W=`255`; others `0` |
| `DJX Pattern All` | all W=`255` |
| `DJX Pattern Release` | all W=`0` |

`Release` replaces the old `All Off` concept and is the reset function for the Pattern layer.

#### C. Builds — keep 10

Folder: `DJ Expression/Pattern/Scenes/Builds/`

| Scene | W values |
|-------|----------|
| `DJX Pattern Build 01` | LED 1 on |
| `DJX Pattern Build 02` | LEDs 1-2 on |
| `DJX Pattern Build 03` | LEDs 1-3 on |
| `DJX Pattern Build 04` | LEDs 1-4 on |
| `DJX Pattern Build 05` | LEDs 1-5 on |
| `DJX Pattern Build 06` | LEDs 1-6 on |
| `DJX Pattern Build 07` | LEDs 1-7 on |
| `DJX Pattern Build 08` | LEDs 1-8 on |
| `DJX Pattern Build 09` | LEDs 1-9 on |
| `DJX Pattern Build 10` | LEDs 1-10 on |

Purpose: build-ups, countdowns, rain-down by reverse ordering, and envelope-scaled one-shots.

#### D. Opposites — keep 5

Folder: `DJ Expression/Pattern/Scenes/Opposites/`

| Scene | W values |
|-------|----------|
| `DJX Pattern Opp 01` | LEDs 1 + 6 W=`255` |
| `DJX Pattern Opp 02` | LEDs 2 + 7 W=`255` |
| `DJX Pattern Opp 03` | LEDs 3 + 8 W=`255` |
| `DJX Pattern Opp 04` | LEDs 4 + 9 W=`255` |
| `DJX Pattern Opp 05` | LEDs 5 + 10 W=`255` |

Purpose: clean geometric movement without needing all adjacent pair scenes.

#### E. Comets — keep 10

Folder: `DJ Expression/Pattern/Scenes/Comets/`

| Scene | W values |
|-------|----------|
| `DJX Pattern Comet 01` | LED 1=`255`, LED 10=`128`, LED 9=`50`, others `0` |
| `DJX Pattern Comet 02` | LED 2=`255`, LED 1=`128`, LED 10=`50`, others `0` |
| `DJX Pattern Comet 03` | LED 3=`255`, LED 2=`128`, LED 1=`50`, others `0` |
| `DJX Pattern Comet 04` | LED 4=`255`, LED 3=`128`, LED 2=`50`, others `0` |
| `DJX Pattern Comet 05` | LED 5=`255`, LED 4=`128`, LED 3=`50`, others `0` |
| `DJX Pattern Comet 06` | LED 6=`255`, LED 5=`128`, LED 4=`50`, others `0` |
| `DJX Pattern Comet 07` | LED 7=`255`, LED 6=`128`, LED 5=`50`, others `0` |
| `DJX Pattern Comet 08` | LED 8=`255`, LED 7=`128`, LED 6=`50`, others `0` |
| `DJX Pattern Comet 09` | LED 9=`255`, LED 8=`128`, LED 7=`50`, others `0` |
| `DJX Pattern Comet 10` | LED 10=`255`, LED 9=`128`, LED 8=`50`, others `0` |

Purpose: highest-value animated pattern; works well with all moods.

### Cut

Delete these existing scene families:

| Existing family | Reason |
|-----------------|--------|
| `Ball/Strips/*` | Duplicate of Build scenes played backward; not worth maintaining separate scenes |
| `Ball/Pairs/*` | Opposites + Singles + Comets cover the useful geometry with fewer scenes |
| `Ball/Warm/*` | Color belongs to MOOD, not Pattern |
| `Ball/Breathe/*` | Brightness belongs to Dimmer; Pattern breathe is better as a chaser/script over W only |
| Old `Ball - All Off` | Replaced by `DJX Pattern Release` |

---

## 3. Revised Chaser Inventory

Target inventory: **8 Pattern chasers**.

All Pattern chasers live under `DJ Expression/Pattern/Chasers/`.

All chasers must use:

- `tempoType = beats`
- no millisecond timing
- only Pattern scenes as steps
- no direct dimmer or RGB control

| Chaser | Steps | Timing | Fade | Run order | Use |
|--------|-------|--------|------|-----------|-----|
| `DJX Pattern Single Slow` | LED 01 → LED 10 | hold `1` beat | in/out `1/2` beat | Loop | Chill motion, subtle sparkle |
| `DJX Pattern Single Fast` | LED 01 → LED 10 | hold `1/4` beat | none | Loop | Drop energy |
| `DJX Pattern Ping Pong` | LED 01 → LED 10 | hold `1/2` beat | in/out `1/4` beat | PingPong | Spatial sweep |
| `DJX Pattern Comet` | Comet 01 → Comet 10 | hold `1/2` beat | out `1/4` beat | Loop | Main moving highlight |
| `DJX Pattern Opposite Pulse` | Opp 01 → Opp 05 | hold `1` beat | in/out `1/2` beat | Loop | Symmetric geometric motion |
| `DJX Pattern Odd Even Flip` | Odd ↔ Even | hold `1` beat | none | Loop | Simple beat pulse |
| `DJX Pattern Build Rise` | Build 01 → Build 10 → All → Release | hold `1/2` beat, All hold `1` beat, Release hold `1/4` beat | none | Single | Pre-drop rise |
| `DJX Pattern Sparkle` | LED 01 → LED 10 | hold `1/4` beat | out `1/4` beat | Random | Texture over any mood |

### Delete Existing Chasers

Delete all old `Ball/Chasers/*` chasers because they are millisecond-timed and reference wrong scenes. Recreate the 8 chasers above from clean Pattern scenes.

---

## 4. Script Designs

Scripts live under `DJ Expression/Pattern/Scripts/`.

Scripts are W-only so they remain composable with MOOD RGB, PHASE dimmer, and MOVEMENT.

### Shared Helpers

```javascript
var FIXTURE_ID = 7;
var W_CH = [10, 14, 18, 22, 26, 30, 34, 38, 42, 46];

function clamp(v, lo, hi) {
  return Math.max(lo, Math.min(hi, Math.round(v)));
}

function clearPattern(fadeMs) {
  for (var i = 0; i < W_CH.length; i++) {
    Engine.setFixture(FIXTURE_ID, W_CH[i], 0, fadeMs || 0);
  }
}

function gaussRand(mean, std) {
  var u1 = Math.random();
  var u2 = Math.random();
  if (u1 < 0.000001) u1 = 0.000001;
  var z = Math.sqrt(-2 * Math.log(u1)) * Math.cos(2 * Math.PI * u2);
  return mean + std * z;
}
```

### `DJX Pattern Fire Flicker`

Purpose: ember-like flicker over the current MOOD. Best paired with `DJX FR Amber`, `DJX FR Orange`, or `DJX FR Heat`.

Behavior:

- Infinite loop; user stops it from VC.
- Does not set master dimmer.
- Does not set RGB.
- Updates LED groups every `200-400ms`, not every frame, so it reads as embers instead of digital noise.
- On normal completion it clears W, but because external stop may interrupt cleanup, VC must also provide `DJX Pattern Release`.

Pseudocode:

```javascript
var FIXTURE_ID = 7;
var W_CH = [10, 14, 18, 22, 26, 30, 34, 38, 42, 46];

function clamp(v, lo, hi) {
  return Math.max(lo, Math.min(hi, Math.round(v)));
}

function clearPattern(fadeMs) {
  for (var i = 0; i < W_CH.length; i++) {
    Engine.setFixture(FIXTURE_ID, W_CH[i], 0, fadeMs || 0);
  }
}

function gaussRand(mean, std) {
  var u1 = Math.random();
  var u2 = Math.random();
  if (u1 < 0.000001) u1 = 0.000001;
  return mean + std * Math.sqrt(-2 * Math.log(u1)) * Math.cos(2 * Math.PI * u2);
}

while (true) {
  var activeCount = Engine.random(3, 7);
  var active = {};

  for (var i = 0; i < activeCount; i++) {
    active[Engine.random(0, 9)] = true;
  }

  for (var led = 0; led < 10; led++) {
    var value;
    if (active[led]) {
      value = clamp(gaussRand(120, 45), 25, 220);
    } else {
      value = clamp(gaussRand(12, 10), 0, 45);
    }
    Engine.setFixture(FIXTURE_ID, W_CH[led], value, Engine.random(80, 180));
  }

  Engine.waitTime(Engine.random(200, 400));
}

clearPattern(250);
```

### `DJX Pattern Buildup Explode`

Purpose: one-shot build that scales to the parent chaser step duration when used inside a beat-timed cue.

Behavior:

- Uses `Engine.getElapsed()` for timing.
- Uses `Engine.getEnvelopeDuration()` to auto-scale.
- Does not set master dimmer.
- Does not set RGB.
- Explosion is W=`255` on all heads; combine with `DJX White Hit` if a full RGB white blast is desired.
- Always waits inside loops.
- Clears W at the end.

Pseudocode:

```javascript
var FIXTURE_ID = 7;
var W_CH = [10, 14, 18, 22, 26, 30, 34, 38, 42, 46];

function clamp(v, lo, hi) {
  return Math.max(lo, Math.min(hi, Math.round(v)));
}

function clearPattern(fadeMs) {
  for (var i = 0; i < W_CH.length; i++) {
    Engine.setFixture(FIXTURE_ID, W_CH[i], 0, fadeMs || 0);
  }
}

function setAll(value, fadeMs) {
  for (var i = 0; i < W_CH.length; i++) {
    Engine.setFixture(FIXTURE_ID, W_CH[i], value, fadeMs || 0);
  }
}

function randomSubset(count) {
  var picked = {};
  while (Object.keys(picked).length < count) {
    picked[Engine.random(0, 9)] = true;
  }
  return picked;
}

var totalMs = Engine.getEnvelopeDuration();
if (totalMs <= 0) totalMs = 8000;

var explodeAt = totalMs * 0.82;
var releaseAt = totalMs * 0.94;
var doneAt = totalMs;

while (Engine.getElapsed() < doneAt) {
  var elapsed = Engine.getElapsed();
  var progress = elapsed / totalMs;

  if (elapsed < explodeAt) {
    var activeCount;
    var minValue;
    var maxValue;
    var tickMs;

    if (progress < 0.35) {
      activeCount = Engine.random(2, 3);
      minValue = 20;
      maxValue = 90;
      tickMs = Engine.random(220, 360);
    } else if (progress < 0.65) {
      activeCount = Engine.random(4, 6);
      minValue = 70;
      maxValue = 170;
      tickMs = Engine.random(140, 240);
    } else {
      activeCount = Engine.random(7, 9);
      minValue = 150;
      maxValue = 240;
      tickMs = Engine.random(70, 140);
    }

    var active = randomSubset(activeCount);
    for (var led = 0; led < 10; led++) {
      var value = active[led] ? Engine.random(minValue, maxValue) : Engine.random(0, 30);
      Engine.setFixture(FIXTURE_ID, W_CH[led], value, Math.min(120, tickMs));
    }
    Engine.waitTime(tickMs);
  } else if (elapsed < releaseAt) {
    setAll(255, 0);
    Engine.waitTime(40);
  } else {
    clearPattern(180);
    Engine.waitTime(40);
  }
}

clearPattern(0);
```

### Stop/Cleanup Rule

QLC+ may stop a script without letting the final cleanup lines run. Therefore every Pattern script must have a nearby VC reset action:

- `DJX Pattern Release` button
- `DJX Pattern Off` collection containing only `DJX Pattern Release`
- Optional panic macro: stop Pattern scripts, then start `DJX Pattern Release`

---

## 5. Collection Designs

Collections should compose layers without stealing channel ownership.

### Pattern-Only Collections

Folder: `DJ Expression/Pattern/Collections/`

| Collection | Contains | Use |
|------------|----------|-----|
| `DJX Pattern Off` | `DJX Pattern Release` | Reset Pattern layer |
| `DJX Pattern Ambient Sparkle` | `DJX Pattern Single Slow` or `DJX Pattern Sparkle` | Chill texture |
| `DJX Pattern Geometry` | `DJX Pattern Opposite Pulse` | Symmetric movement |
| `DJX Pattern Comet Ride` | `DJX Pattern Comet` | Main moving highlight |
| `DJX Pattern Fire Embers` | `DJX Pattern Fire Flicker` | Organic fire texture |
| `DJX Pattern Rise Burst` | `DJX Pattern Build Rise` or `DJX Pattern Buildup Explode` | Pre-drop moment |

### Example Layer Combinations

These are recommended live combinations, not separate duplicate scenes.

| Song section | Phase | Mood | Pattern | Movement | Result |
|--------------|-------|------|---------|----------|--------|
| Intro | `DJX Phase Chill` | `DJX OC Deep Blue` | `DJX Pattern Ambient Sparkle` | Slow Circle from phase | Low blue room with small white glints |
| Verse | `DJX Phase Chill` | `DJX JG Teal` | `DJX Pattern Opposite Pulse` | Slow Sweep | Calm geometric texture |
| Pre-chorus | `DJX Phase Build` | `DJX FR Amber` | `DJX Pattern Fire Embers` | Fast Sweep | Heating, ember-like motion |
| Build | `DJX Phase Build` | `DJX NE Magenta` | `DJX Pattern Rise Burst` | Fast Sweep | Increasing white energy over neon color |
| Drop | `DJX Phase Drop` | `DJX NE Cyan` | `DJX Pattern Comet Ride` | Fast Circle | Bright high-energy orbit |
| Breakdown | `DJX Phase Freeze` | `DJX OC Storm` | `DJX Pattern Off` | None | Static tension, no sparkle |

### Optional Cross-Layer Macros

If desired, create a small `DJ Expression/Macros/` folder for one-button recipes. These macros may combine one function from each layer, but must not introduce new channel ownership.

Examples:

| Macro | Contains |
|-------|----------|
| `DJX Macro Fire Build` | `DJX Phase Build` + `DJX FR Amber` + `DJX Pattern Fire Embers` |
| `DJX Macro Neon Drop` | `DJX Phase Drop` + `DJX NE Cyan` + `DJX Pattern Comet Ride` |
| `DJX Macro Reset Visuals` | `DJX Blackout Color` + `DJX Pattern Off` + `DJX Dimmer Off` |

---

## 6. Virtual Console Integration

Keep everything on Page 3: **DJ Expression**. Do not create a separate Ball page for normal performance; that would split the architecture.

Add a new **PATTERN** SoloFrame between MOOD and MOVEMENT.

```text
┌──────────────────────────────────────────────────────────────────┐
│ 🎧 DJ EXPRESSION                                         Page 3 │
│                                                                  │
│ ┌─ ENERGY / PHASE ─────────────────────────┐ ┌─ MASTER ───────┐ │
│ │ [CHILL] [FREEZE] [BUILD] [DROP]          │ │ [Intensity]    │ │
│ └──────────────────────────────────────────┘ │ [Blackout]     │ │
│                                              └─────────────────┘ │
│ ┌─ MOOD / COLOR ───────────────────────────────────────────────┐ │
│ │ Jungle / Ocean / Fire / Neon color buttons                   │ │
│ └───────────────────────────────────────────────────────────────┘ │
│                                                                  │
│ ┌─ PATTERN / BALL WHITE LAYER ─────────────────────────────────┐ │
│ │ [OFF] [Sparkle] [Geometry] [Comet] [Fire Embers] [Rise Burst]│ │
│ └───────────────────────────────────────────────────────────────┘ │
│                                                                  │
│ ┌─ MOVEMENT ───────────────────────────────┐ ┌─ TEMPO ────────┐ │
│ │ [Slow Circle] [Slow Sweep] [Fast Circle] │ │ BPM / Beat src │ │
│ └──────────────────────────────────────────┘ └─────────────────┘ │
│                                                                  │
│ ┌─ FX / MOMENTS ────────────────────────────────────────────────┐ │
│ │ [STROBE BUILD] [FAST STROBE] [WHITE HIT] [RESET]             │ │
│ └───────────────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────────────┘
```

### VC Behavior

- PATTERN should be a SoloFrame so only one looping Pattern texture runs at a time.
- `OFF` starts `DJX Pattern Off`.
- `RESET` should trigger:
  - `DJX Pattern Off`
  - `DJX Blackout Color` if a full visual reset is wanted
  - stop Pattern scripts if the VC wiring supports it
- `Rise Burst` should be a flash or single-shot button, not a persistent toggle.
- `Fire Embers` should be a toggle because the script is infinite.

---

## 7. Folder Structure

Replace the separate `Ball/` tree with a unified DJX tree:

```text
DJ Expression/
├── Color/
│   ├── Jungle/
│   ├── Ocean/
│   ├── Fire/
│   ├── Neon/
│   └── Utility/
├── Dimmer/
├── Chasers/
│   └── Color theme chasers
├── Movement/
├── FX/
├── Pattern/
│   ├── Scenes/
│   │   ├── Singles/
│   │   ├── Core/
│   │   ├── Builds/
│   │   ├── Opposites/
│   │   └── Comets/
│   ├── Chasers/
│   ├── Scripts/
│   └── Collections/
├── Macros/              optional cross-layer recipes
└── Phases/
```

---

## 8. Migration Plan

Do **not** patch the current `Ball/` system in place. It contains too many wrong assumptions: ch5 in every scene, millisecond chasers, color/dimmer duplication, and a separate folder architecture.

Use a clean migration.

### Step 1 — Backup

Save a copy of the current QLC+ project before deleting functions.

### Step 2 — Update Existing DJX Color Scenes

For every DJX Color scene and color chaser step scene:

- Remove Beam Ball W channels `10,14,18,22,26,30,34,38,42,46`.
- Keep Beam Ball RGB channels.
- Keep Hero RGBW channels.
- Confirm no Color scene sets Beam Ball ch `5`.

Special cases:

- `DJX White Hit`: Beam Ball RGB=`255`, Beam Ball W omitted.
- `DJX Blackout Color`: Beam Ball RGB=`0`, Beam Ball W omitted.

### Step 3 — Delete Old Ball Functions

Delete in dependency order:

1. Old `Ball/Chasers/*` functions, IDs `206-219`.
2. Old `Ball/*` scenes, IDs `137-205`.
3. Any old planned/partial `Ball/Scripts/*` or `Ball/Collections/*` if present.
4. Old `Ball/` folders after empty.

### Step 4 — Recreate Pattern Scenes

Create the 39 scenes from this plan under `DJ Expression/Pattern/Scenes/`.

Validation checklist:

- No Pattern scene contains ch `5`.
- No Pattern scene contains any Beam Ball RGB channel.
- Every Pattern scene contains all ten W channels with explicit values.

### Step 5 — Recreate Beat-Synced Chasers

Create the 8 Pattern chasers with `tempoType=beats`.

Validation checklist:

- No Pattern chaser uses millisecond timing.
- One-shot chasers end with `DJX Pattern Release` when they should leave no residue.
- Looping chasers are controlled by the PATTERN SoloFrame.

### Step 6 — Create Scripts

Create:

- `DJX Pattern Fire Flicker`
- `DJX Pattern Buildup Explode`

Validation checklist:

- No script writes ch `5`.
- No script writes Beam Ball RGB.
- Every loop contains `Engine.waitTime()`.
- VC has an accessible `DJX Pattern Release` reset.

### Step 7 — Create Pattern Collections

Create the 6 Pattern collections listed above.

### Step 8 — Update Virtual Console

Add the PATTERN SoloFrame to Page 3 `DJ Expression`.

Wire buttons:

| Button | Function |
|--------|----------|
| `OFF` | `DJX Pattern Off` |
| `Sparkle` | `DJX Pattern Ambient Sparkle` |
| `Geometry` | `DJX Pattern Geometry` |
| `Comet` | `DJX Pattern Comet Ride` |
| `Fire Embers` | `DJX Pattern Fire Embers` |
| `Rise Burst` | `DJX Pattern Rise Burst` |

### Step 9 — Verification

Run these visual checks:

1. Start `DJX Phase Chill`, then `DJX Pattern Comet Ride`. Beam Ball dimmer must remain at Chill level, not jump to full.
2. Switch MOOD while Pattern is running. Hue changes, white Pattern continues independently.
3. Start `DJX Pattern Off`. All Beam Ball W channels go to `0`; RGB mood remains active.
4. Start `DJX Dimmer Off`. Entire Beam Ball goes dark even if Pattern and Mood are active.
5. Start `DJX Pattern Fire Embers`, then reset. W channels clear.
6. Start `DJX Pattern Build Rise` at different BPM values. It stays beat-locked.

---

## 9. Naming Convention

Use `DJX` for every function that belongs to DJ Expression.

Do not use the old `Ball - ...` prefix for DJX-integrated functions.

### Naming Rules

| Function type | Pattern |
|---------------|---------|
| Pattern scenes | `DJX Pattern <Scene Name>` |
| Pattern chasers | `DJX Pattern <Chaser Name>` |
| Pattern scripts | `DJX Pattern <Script Name>` |
| Pattern collections | `DJX Pattern <Collection Name>` |
| Optional macros | `DJX Macro <Recipe Name>` |

Examples:

- `DJX Pattern LED 01`
- `DJX Pattern Comet 07`
- `DJX Pattern Comet Ride`
- `DJX Pattern Fire Flicker`
- `DJX Pattern Off`
- `DJX Macro Neon Drop`

Use zero-padded numbers for LED-indexed scene names: `01` through `10`.

---

## Final Architecture Summary

The revised system is:

- **Composable**: Mood RGB, Pattern W, Dimmer ch5, Movement pan/tilt, and FX do not fight.
- **Beat-synced**: Pattern chasers use beats, matching DJX timing.
- **Maintainable**: 39 scenes and 8 chasers replace 69 scenes and 14 drifting chasers.
- **Unified**: Everything lives under `DJ Expression/`, not a separate `Ball/` island.
- **Safe**: No Pattern function sets master dimmer, so PHASE remains authoritative.
