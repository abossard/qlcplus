# RGB Matrix RGBW Mode Analysis — Findings

> Note: Saved here instead of `/tmp/rgbw-engine-findings.md` because the runtime
> blocks all writes to `/tmp`. Read this file from
> `/Users/abossard/Desktop/projects/qlcplus/.session-notes/rgbw-engine-findings.md`.

## 1. ControlMode enum — `engine/src/rgbmatrix.h:322-331`

```cpp
enum ControlMode {
    ControlModeRgb = 0,
    ControlModeWhite,        // 1
    ControlModeAmber,        // 2
    ControlModeUV,           // 3
    ControlModeDimmer,       // 4
    ControlModeShutter,      // 5
    ControlModeRgbw = 6,         // "Accurate"   — subtractWhite = true
    ControlModeRgbwBrighter = 7  // "Brightness" — subtractWhite = false
};
```

XML token strings (`rgbmatrix.cpp:50-58`): `RGB`, `White`, `Amber`, `UV`, `Dimmer`,
`Shutter`, `RGBW`, `RGBWBrighter`.

## 2. Engine RGB→RGBW conversion — `engine/src/rgbmatrix.cpp:925-951`

```cpp
else if (m_controlMode == ControlModeRgbw || m_controlMode == ControlModeRgbwBrighter)
{
    bool subtractWhite = (m_controlMode == ControlModeRgbw);   // <-- "Accurate" subtracts
    quint32 rCh = head.channelNumber(QLCChannel::Red,   QLCChannel::MSB);
    quint32 gCh = head.channelNumber(QLCChannel::Green, QLCChannel::MSB);
    quint32 bCh = head.channelNumber(QLCChannel::Blue,  QLCChannel::MSB);
    quint32 wCh = head.channelNumber(QLCChannel::White, QLCChannel::MSB);

    uchar r = uchar(qRed(col));
    uchar g = uchar(qGreen(col));
    uchar b = uchar(qBlue(col));
    uchar w = wCh == QLCChannel::invalid() ? 0 : qMin(r, qMin(g, b));

    if (rCh && gCh && bCh valid) {
        write(rCh, subtractWhite ? r - w : r);
        write(gCh, subtractWhite ? g - w : g);
        write(bCh, subtractWhite ? b - w : b);
        if (wCh valid) write(wCh, w);
    }
}
```

**Formulas** (single source of truth — engine is correct):

| Mode | W | R out | G out | B out |
|------|---|-------|-------|-------|
| `RGB` (0) | — | R | G | B |
| `RGBW` Accurate (6) | `min(R,G,B)` | R−W | G−W | B−W |
| `RGBW` Brightness (7) | `min(R,G,B)` | R | G | B |

The engine **looks up channels by role** via `head.channelNumber(QLCChannel::Red/Green/Blue/White, MSB)` —
i.e., it does *not* assume R=0,G=1,B=2,W=3. As long as the fixture definition
correctly tags one channel as the `White` role, the W byte goes to the right slot.

## 3. Worked examples (engine output, assuming a properly-tagged RGBW head)

### Input #FF0000 (pure red) — R=255,G=0,B=0
- RGB:        R=255, G=0,   B=0
- Accurate:   W=min(255,0,0)=0  → R=255, G=0,   B=0,   W=0    ← still bright red ✓
- Brightness: W=0                → R=255, G=0,   B=0,   W=0    ← identical to Accurate when W=0 ✓

### Input #00FF00 — green
- Accurate / Brightness: W=0 → only G=255 lit. Same as RGB.

### Input #0000FF — blue
- Accurate / Brightness: W=0 → only B=255 lit. Same as RGB.

### Input #FFFFFF (white) — R=G=B=255
- RGB:        R=255, G=255, B=255         (no W channel touched)
- Accurate:   W=255 → R=0,   G=0,   B=0,   W=255   ← only the W LED lights
- Brightness: W=255 → R=255, G=255, B=255, W=255   ← all four LEDs at full

### Input #808080 (50% gray)
- Accurate:   W=128 → R=0,   G=0,   B=0,   W=128
- Brightness: W=128 → R=128, G=128, B=128, W=128

The engine math is **correct and standard**. Pure red in Accurate mode would NOT
produce black on hardware.

## 4. THE ACTUAL BUG — UI-side grayscale forcing

### `qmlui/rgbmatrixeditor.cpp:197-215` — `setColorAtIndex`

```cpp
void RGBMatrixEditor::setColorAtIndex(int index, QColor color)
{
    if (m_matrix == nullptr || m_matrix->getColor(index) == color)
        return;

    if (m_matrix->controlMode() != RGBMatrix::ControlModeRgb)
    {
        // Convert color to grayscale for non-RGB control modes
        uchar gray = qGray(color.rgb());
        color = QColor(gray, gray, gray);
    }
    ...
    m_matrix->setColor(index, color);
}
```

**Every** non-RGB ControlMode — including `ControlModeRgbw` and
`ControlModeRgbwBrighter` — causes the picked color to be flattened to
grayscale `(g,g,g)` before being stored on the matrix.

### `qmlui/rgbmatrixeditor.cpp:236-258` — `updateColors`

```cpp
if (m_matrix->blendMode() == Universe::MaskBlend ||
    m_matrix->controlMode() != RGBMatrix::ControlModeRgb)
{
    m_matrix->setColor(0, Qt::white);
    // ...clears colors 1..4
}
```

When the user **switches into** RGBW Accurate or RGBW Brightness mode, all
algorithm colors are reset: color 0 → white, colors 1‑4 → invalid. That's why
"the palette turns white" the moment the mode is selected.

### Combined effect on the user's symptoms

User picks pure red (#FF0000) in the palette while in RGBW Accurate:

1. `setColorAtIndex` runs: `qGray(0xFF0000) = 0.299*255 + 0.587*0 + 0.114*0 ≈ 76`.
   Stored color = `QColor(76,76,76)` — the swatch in the QML picker now shows
   a dark gray (looks "black-ish").
2. Engine sees R=G=B=76 → W=76 → outputs R=0,G=0,B=0,W=76 → fixture lights up
   as a *dim warm white*. From the user's POV: "almost black."

User in RGBW Brightness:

1. After selecting the mode, `updateColors` already forced color0 = white
   (255,255,255). Picking another color also goes through the grayscale clamp.
2. Engine: R=255,G=255,B=255,W=255 → "only white output."

That matches the report exactly.

## 5. Why this code exists (intent vs. actual)

The grayscale clamp was written for the **single-channel** modes
(`White`, `Amber`, `UV`, `Dimmer`, `Shutter`) — those modes only emit a single
8-bit value via `RGBMatrix::rgbToGrey(col)`, so the picker showing a colored
swatch would mislead the user. Forcing the swatch to gray is a UI affordance
for those modes.

When `ControlModeRgbw` (6) and `ControlModeRgbwBrighter` (7) were added, the
condition `controlMode() != ControlModeRgb` was kept verbatim, lumping the
RGBW modes together with the single-channel modes — even though the RGBW path
needs *full* RGB triples to produce useful output. This is a regression
specific to the RGBW modes.

## 6. Channel-mapping sanity check

- `engine/src/rgbmatrix.cpp:928-931` resolves channels by role
  (`QLCChannel::Red/Green/Blue/White`), so for a Generic RGBPanel head defined
  with channels in order R,G,B,W the addresses come out as `addr+0..3` and W
  is written to `addr+3`. No bug there.
- `head.whiteChannel()` is **not** the API used; the engine uses
  `head.channelNumber(QLCChannel::White, QLCChannel::MSB)`. As long as the
  Generic RGBPanel is created with `Color Mode = RGBW`, this returns a valid
  index. (Generic / RGB Panel definition lives in fixture creation logic; if
  the user accidentally created an RGB panel and then chose ControlModeRgbw,
  `wCh` is invalid → W stays 0 and Accurate produces black for everything.
  Worth verifying in the actual project file.)

## 7. Fix recommendation (for the GPT-5.5 agent)

Two-line fix in `qmlui/rgbmatrixeditor.cpp`:

```cpp
// setColorAtIndex
- if (m_matrix->controlMode() != RGBMatrix::ControlModeRgb)
+ if (m_matrix->controlMode() != RGBMatrix::ControlModeRgb &&
+     m_matrix->controlMode() != RGBMatrix::ControlModeRgbw &&
+     m_matrix->controlMode() != RGBMatrix::ControlModeRgbwBrighter)

// updateColors — same condition
```

Or cleaner: introduce a helper `RGBMatrix::controlModeAcceptsColor()` returning
`true` for `Rgb | Rgbw | RgbwBrighter` and use it in both places.

After the fix:
- The QML color picker keeps real RGB swatches in RGBW modes.
- Engine's existing W = min(R,G,B) extraction (already correct) does the right thing.
- Pure red in Accurate → R=255,G=0,B=0,W=0 → red on hardware. ✓

## 8. Open questions for verification

1. Does the project's Generic RGB Panel fixture instance have `Color Mode = RGBW`?
   If it's RGB, `wCh` is invalid — Accurate writes R=255-0=255,G,B normally
   but with no W role tag, **the head also has no `Red/Green/Blue` role tags
   for non-Generic RGBPanel?** — re-check `fixture.cpp` channel role assignment
   for Generic RGB Panel rows.
2. `updateColors()` runs every time the mode switches; ideally it should
   only reset colors when transitioning *into* a single-channel mode, not
   into RGBW. Worth double-checking call sites.
