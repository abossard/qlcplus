# Beam Ball Patterns — Generator Plan

## Concept

Patterns are **abstract LED masks** — which of the 10 heads are on and at what relative intensity. Colors are a separate parameter. Speeds are a chaser parameter. The MCP generates the full matrix:

```
Scenes  = Pattern × Color
Chasers = Chase Concept × Color × Speed
```

This keeps the design small and the output comprehensive.

---

## Fixture Reference

| Item | Value |
|------|-------|
| Fixture ID | `7` |
| Heads | 10 RGBW LEDs |
| Master Dimmer | ch `5` (owned by Dimmer layer — **never set by patterns**) |
| Per-head RGBW channels: | |

```
LED:  1    2    3    4    5    6    7    8    9    10
R:    7   11   15   19   23   27   31   35   39   43
G:    8   12   16   20   24   28   32   36   40   44
B:    9   13   17   21   25   29   33   37   41   45
W:   10   14   18   22   26   30   34   38   42   46
```

---

## 1. Pattern Definitions (LED masks)

Each pattern is a list of 10 intensity values (0.0–1.0) — one per head. These are **abstract** — they get multiplied by a color to produce actual RGBW channel values.

| ID | Pattern | LED mask (1=on, 0=off) | Notes |
|----|---------|----------------------|-------|
| `led01` | LED 1 | `1 0 0 0 0 0 0 0 0 0` | Single head |
| `led02` | LED 2 | `0 1 0 0 0 0 0 0 0 0` | |
| `led03` | LED 3 | `0 0 1 0 0 0 0 0 0 0` | |
| `led04` | LED 4 | `0 0 0 1 0 0 0 0 0 0` | |
| `led05` | LED 5 | `0 0 0 0 1 0 0 0 0 0` | |
| `led06` | LED 6 | `0 0 0 0 0 1 0 0 0 0` | |
| `led07` | LED 7 | `0 0 0 0 0 0 1 0 0 0` | |
| `led08` | LED 8 | `0 0 0 0 0 0 0 1 0 0` | |
| `led09` | LED 9 | `0 0 0 0 0 0 0 0 1 0` | |
| `led10` | LED 10 | `0 0 0 0 0 0 0 0 0 1` | |
| `odd` | Odd | `1 0 1 0 1 0 1 0 1 0` | Complementary A |
| `even` | Even | `0 1 0 1 0 1 0 1 0 1` | Complementary B |
| `all` | All | `1 1 1 1 1 1 1 1 1 1` | Full ball |
| `off` | Release | `0 0 0 0 0 0 0 0 0 0` | Blackout / reset |
| `build01` | Build 1 | `1 0 0 0 0 0 0 0 0 0` | Cumulative fills |
| `build02` | Build 2 | `1 1 0 0 0 0 0 0 0 0` | |
| `build03` | Build 3 | `1 1 1 0 0 0 0 0 0 0` | |
| `build04` | Build 4 | `1 1 1 1 0 0 0 0 0 0` | |
| `build05` | Build 5 | `1 1 1 1 1 0 0 0 0 0` | |
| `build06` | Build 6 | `1 1 1 1 1 1 0 0 0 0` | |
| `build07` | Build 7 | `1 1 1 1 1 1 1 0 0 0` | |
| `build08` | Build 8 | `1 1 1 1 1 1 1 1 0 0` | |
| `build09` | Build 9 | `1 1 1 1 1 1 1 1 1 0` | |
| `build10` | Build 10 | `1 1 1 1 1 1 1 1 1 1` | = All |
| `opp01` | Opposite 1 | `1 0 0 0 0 1 0 0 0 0` | Symmetric pairs |
| `opp02` | Opposite 2 | `0 1 0 0 0 0 1 0 0 0` | |
| `opp03` | Opposite 3 | `0 0 1 0 0 0 0 1 0 0` | |
| `opp04` | Opposite 4 | `0 0 0 1 0 0 0 0 1 0` | |
| `opp05` | Opposite 5 | `0 0 0 0 1 0 0 0 0 1` | |
| `comet01` | Comet 1 | `1.0 0 0 0 0 0 0 0 0.2 0.5` | Head + body + tail |
| `comet02` | Comet 2 | `0.5 1.0 0 0 0 0 0 0 0 0.2` | |
| `comet03` | Comet 3 | `0.2 0.5 1.0 0 0 0 0 0 0 0` | |
| `comet04` | Comet 4 | `0 0.2 0.5 1.0 0 0 0 0 0 0` | |
| `comet05` | Comet 5 | `0 0 0.2 0.5 1.0 0 0 0 0 0` | |
| `comet06` | Comet 6 | `0 0 0 0.2 0.5 1.0 0 0 0 0` | |
| `comet07` | Comet 7 | `0 0 0 0 0.2 0.5 1.0 0 0 0` | |
| `comet08` | Comet 8 | `0 0 0 0 0 0.2 0.5 1.0 0 0` | |
| `comet09` | Comet 9 | `0 0 0 0 0 0 0.2 0.5 1.0 0` | |
| `comet10` | Comet 10 | `0 0 0 0 0 0 0 0.2 0.5 1.0` | |

**Total: 39 pattern definitions**

---

## 2. Color Palette

Each color defines RGBW values for a head at full intensity (mask=1.0). When mask < 1.0, all values scale proportionally.

| ID | Color | R | G | B | W | Source |
|----|-------|---|---|---|---|--------|
| `white` | White | 0 | 0 | 0 | 255 | Pure W LED |
| `warm` | Warm White | 255 | 76 | 0 | 63 | Amber/fire tone |
| `fire-red` | Fire Red | 204 | 0 | 0 | 0 | DJX FR Red |
| `fire-amber` | Fire Amber | 204 | 102 | 0 | 0 | DJX FR Amber |
| `fire-orange` | Fire Orange | 255 | 64 | 0 | 0 | DJX FR Orange |
| `ocean-blue` | Ocean Blue | 0 | 0 | 128 | 0 | DJX OC Deep Blue |
| `ocean-aqua` | Ocean Aqua | 0 | 128 | 128 | 0 | DJX OC Aqua |
| `neon-magenta` | Neon Magenta | 255 | 0 | 204 | 0 | DJX NE Magenta |
| `neon-cyan` | Neon Cyan | 0 | 255 | 204 | 0 | DJX NE Cyan |
| `jungle-green` | Jungle Green | 0 | 96 | 48 | 0 | DJX JG Deep Green |
| `full-white` | Full White | 255 | 255 | 255 | 255 | Maximum output |

**To add a color**: add a row here, re-run the generator.

---

## 3. Speed Tiers

Chasers are created at multiple speeds from the same scene sequence.

| ID | Speed | Hold | Fade In | Fade Out | Use |
|----|-------|------|---------|----------|-----|
| `slow` | Slow | 1 beat | ½ beat | ½ beat | Chill, ambient |
| `medium` | Medium | ½ beat | ¼ beat | ¼ beat | Standard energy |
| `fast` | Fast | ¼ beat | 0 | 0 | Drop, high energy |

All chasers use `tempoType=beats`.

---

## 4. Chase Concepts

Each chase concept defines which pattern scenes to step through and in what order.

| ID | Chase | Steps (pattern IDs) | Run Order | Notes |
|----|-------|---------------------|-----------|-------|
| `single` | Single Chase | led01→led02→...→led10 | Loop | One LED walks |
| `pingpong` | Ping Pong | led01→led02→...→led10 | PingPong | Bounces back |
| `comet` | Comet Chase | comet01→comet02→...→comet10 | Loop | Tail follows |
| `opposite` | Opposite | opp01→opp02→...→opp05 | Loop | Symmetric pairs |
| `oddeven` | Odd/Even Flip | odd→even | Loop | Alternating halves |
| `buildup` | Build Up | build01→...→build10→off | Single | Cumulative rise → dark |
| `sparkle` | Random Sparkle | led01→led02→...→led10 | Random | Random pops |

---

## 5. Generated Output

### Scenes: Pattern × Color

For each **pattern** and each **color**, generate one scene.

**Scene name**: `DJX BB {Color} {Pattern}`
**Example**: `DJX BB White LED 01`, `DJX BB Fire Red Odd`, `DJX BB Neon Cyan Comet 05`

**Channel values**: For each LED head (0–9):
```
R_value = color.R × mask[head]
G_value = color.G × mask[head]
B_value = color.B × mask[head]
W_value = color.W × mask[head]
```

**Never set ch5 (master dimmer).**

**Scene count**: 39 patterns × 11 colors = **429 scenes**

> That's a lot. In practice, generate only the colors you actively use.
> Start with `white` + your 4 most-used colors = 39 × 5 = **195 scenes**.
> Add more colors later by re-running the generator for just that color.

### Chasers: Chase × Color × Speed

For each **chase concept**, **color**, and **speed tier**, generate one chaser.

**Chaser name**: `DJX BB {Color} {Chase} {Speed}`
**Example**: `DJX BB White Single Slow`, `DJX BB Fire Red Comet Fast`

**Steps**: Look up the chase concept's step list, resolve each pattern ID to the scene name for that color.

**Chaser count**: 7 chases × 11 colors × 3 speeds = **231 chasers**

> Again, start with a subset. 7 × 3 colors × 3 = **63 chasers** is more practical.

---

## 6. Generator Algorithm (MCP)

```python
# Pseudocode for generating all scenes + chasers

PATTERNS = {
    "led01": [1,0,0,0,0,0,0,0,0,0],
    "odd":   [1,0,1,0,1,0,1,0,1,0],
    "comet01": [1.0,0,0,0,0,0,0,0,0.2,0.5],
    # ... all 39 patterns
}

COLORS = {
    "White":       {"R":0,   "G":0,   "B":0,   "W":255},
    "Fire Red":    {"R":204, "G":0,   "B":0,   "W":0},
    "Neon Cyan":   {"R":0,   "G":255, "B":204, "W":0},
    # ... all colors
}

SPEEDS = {
    "Slow":   {"hold": "1 beat",   "fadeIn": "1/2 beat", "fadeOut": "1/2 beat"},
    "Medium": {"hold": "1/2 beat", "fadeIn": "1/4 beat", "fadeOut": "1/4 beat"},
    "Fast":   {"hold": "1/4 beat", "fadeIn": 0,          "fadeOut": 0},
}

CHASES = {
    "Single":  {"steps": ["led01","led02",...,"led10"], "runOrder": "loop"},
    "Comet":   {"steps": ["comet01",...,"comet10"],     "runOrder": "loop"},
    "OddEven": {"steps": ["odd","even"],                "runOrder": "loop"},
    # ... all 7
}

R_CH = [7,11,15,19,23,27,31,35,39,43]
G_CH = [8,12,16,20,24,28,32,36,40,44]
B_CH = [9,13,17,21,25,29,33,37,41,45]
W_CH = [10,14,18,22,26,30,34,38,42,46]

# Generate scenes
for color_name, color in COLORS.items():
    for pattern_name, mask in PATTERNS.items():
        scene_name = f"DJX BB {color_name} {pattern_name}"
        channel_values = []
        for head in range(10):
            channel_values.append({"ch": R_CH[head], "val": round(color["R"] * mask[head])})
            channel_values.append({"ch": G_CH[head], "val": round(color["G"] * mask[head])})
            channel_values.append({"ch": B_CH[head], "val": round(color["B"] * mask[head])})
            channel_values.append({"ch": W_CH[head], "val": round(color["W"] * mask[head])})
        create_scene(scene_name, channel_values)

# Generate chasers
for chase_name, chase in CHASES.items():
    for color_name in COLORS:
        for speed_name, speed in SPEEDS.items():
            chaser_name = f"DJX BB {color_name} {chase_name} {speed_name}"
            steps = [f"DJX BB {color_name} {s}" for s in chase["steps"]]
            create_chaser(chaser_name, steps, speed, chase["runOrder"])
```

---

## 7. Folder Structure

```
DJ Expression/
├── BB Patterns/
│   ├── White/          (39 scenes)
│   ├── Fire Red/       (39 scenes)
│   ├── Neon Cyan/      (39 scenes)
│   └── .../            (one folder per color)
├── BB Chasers/
│   ├── White/          (7 chases × 3 speeds = 21)
│   ├── Fire Red/       (21)
│   └── .../
└── BB Scripts/         (Fire Flicker, Buildup Explode)
```

---

## 8. Virtual Console

The PATTERN section on Page 3 uses a **SoloFrame** with one button per chase concept. A speed control (speed dial or 3 radio buttons) selects slow/medium/fast. Color comes from MOOD selection or a dedicated BB color picker.

```
┌─ PATTERN / BALL (SoloFrame) ─────────────────────────────────────┐
│ [OFF] [Single] [Comet] [Opposite] [OddEven] [Build] [Sparkle]   │
│ Speed: [Slow] [Med] [Fast]                                       │
└──────────────────────────────────────────────────────────────────┘
```

**Implementation note**: Since QLC+ buttons bind to specific functions, each button needs to point to a specific chaser (color+speed combo). Two approaches:
1. **Simple**: Wire the most common color per button, let user swap via Function Manager
2. **Advanced**: Use a speed dial widget to control chaser speed in real-time (no need for 3 speed chasers per chase — just one chaser + speed dial)

---

## 9. Scripts (not generated — hand-written)

Scripts bypass the scene matrix because they need per-tick randomness.

### DJX BB Fire Flicker
- Sets RGBW directly per head with Gaussian randomness
- Warm fire colors (R=150-255, G=20-100, B=0, W=0-80)
- Holds LED groups for 200-400ms between updates
- Infinite loop — stopped from VC
- **Never sets ch5**

### DJX BB Buildup Explode
- 5-phase state machine: Simmer → Cook → Boil → EXPLODE → Dark
- Scales to `Engine.getEnvelopeDuration()` if used in a chaser
- Warm tones build up, explosion is full RGBW=255
- Single-shot
- **Never sets ch5**

---

## 10. Implementation Order

1. **Start small**: Generate White-only scenes (39) + White chasers (21)
2. **Test**: Run the chasers, verify LED order matches physical layout
3. **Remap if needed**: Adjust pattern masks if LED numbering ≠ physical adjacency
4. **Add colors**: Generate 2-3 more color sets
5. **Create scripts**: Fire Flicker, Buildup Explode
6. **Wire VC**: Add PATTERN SoloFrame to Page 3
7. **Iterate**: Add more colors as needed — just re-run the generator
