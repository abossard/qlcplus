# RGBW QML Editor Findings

## Scope
Analyzed the QML RGB Matrix editor and the `RGBMatrixEditor` C++ bridge for why the editor color palette/preview becomes black-white in RGBW modes.

## 1. How QML handles control mode changes

`qmlui/qml/fixturesfunctions/RGBMatrixEditor.qml` only exposes a "Color mode" combo box:

- Lines 276-294: combo model maps indices directly to `rgbMatrixEditor.controlMode`.
- RGBW modes are indices 6 and 7:
  - 6: `RGBW (Accurate)`
  - 7: `RGBW (Brighter)`
- `onCurrentIndexChanged: rgbMatrixEditor.controlMode = currentIndex`

There is no QML `onControlModeChanged`, no `colorMode` binding, and no QML-side RGBW-specific color conversion in this file.

Color picking:

- Lines 72-105: `ColorTool` is generic.
- `showTool()` sets `currentRGB = rgbMatrixEditor.colorAtIndex(colorIndex)`.
- `onToolColorChanged` immediately writes the picked RGB color back through `rgbMatrixEditor.setColorAtIndex(colorIndex, previewBtn.color)`.
- It ignores the `w/a/uv` arguments for RGB Matrix colors.

`ColorTool.qml` / `ColorToolBasic.qml` / `ColorToolFull.qml` also do not inspect RGB Matrix `controlMode`; they render normal RGB palettes. The Basic palette always contains grayscale + RGB swatches, and the Full palette always draws a hue spectrum.

## 2. Why the palette/preview appears black-white

The conversion is in C++, not QML:

`qmlui/rgbmatrixeditor.cpp`:

- `setControlMode()` lines 598-609:
  1. stores `m_matrix->setControlMode(...)`
  2. calls `updateColors()`
  3. calls `initPreviewData()`
  4. emits `controlModeChanged()` and `algoColorsChanged()`

- `updateColors()` lines 236-259:
  - If `m_matrix->controlMode() != RGBMatrix::ControlModeRgb`, it sets color 0 to `Qt::white`.
  - This includes RGBW Accurate and RGBW Brighter because their enum values are 6 and 7, not `ControlModeRgb`.
  - Result: switching from RGB to either RGBW mode overwrites the stored first algorithm color with white.

- `setColorAtIndex()` lines 197-215:
  - If `m_matrix->controlMode() != RGBMatrix::ControlModeRgb`, every newly picked color is converted to grayscale via `qGray(color.rgb())`, then stored as `QColor(gray, gray, gray)`.
  - This means after switching to RGBW Accurate/Brightness, selecting red/green/blue from the picker stores only luminance grayscale.

So the visible color buttons and matrix preview become grayscale/white because the stored algorithm colors are forcibly changed or constrained whenever the mode is not plain RGB.

## 3. Does changing controlMode modify stored colors?

Yes.

- Switching to RGBW calls `RGBMatrixEditor::setControlMode()`.
- That calls `updateColors()`.
- `updateColors()` treats every non-RGB mode as single-channel/non-color mode and sets color 0 to white.
- Future selected colors are converted to grayscale in `setColorAtIndex()`.

This is destructive to editor state: a previously stored `#ff0000` can be replaced with `#ffffff` on mode switch, and later color selections become `#4c4c4c`-style grayscale values instead of retaining RGB hue.

## 4. Does the color picker UI change rendering based on mode?

No evidence found.

- `RGBMatrixEditor.qml` does not bind the ColorTool palette rendering to `controlMode`.
- `ColorTool.qml`, `ColorToolBasic.qml`, and `ColorToolFull.qml` do not know about RGB Matrix control mode.
- The picker can still present color choices, but the editor bridge converts or overwrites colors before storing them, so selected/resulting colors appear grayscale/white.

## 5. QColor conversion involved

Yes:

- `RGBMatrixEditor::setColorAtIndex()` uses `qGray(color.rgb())` for all `controlMode() != ControlModeRgb`.
- `RGBMatrixEditor::updateColors()` uses `Qt::white` for all non-RGB modes.

These branches incorrectly include RGBW modes, which are still full-color modes and should preserve RGB hue.

## 6. Preview behavior

The editor preview uses the RGB algorithm map directly:

- `initPreviewData()` lines 988-990 initializes the preview handler with `m_matrix->getColor(0)` and `m_matrix->getColor(1)`.
- `slotPreviewTimeout()` lines 923-927 computes `m_matrix->previewMap(...)`.
- Lines 953-963 copy `m_previewStepHandler->m_map[y][x]` directly into `m_previewData` as `QColor(...)`.
- Engine `RGBMatrix::previewMap()` lines 328-345 calls `algorithm->rgbMap(...)` and applies rotation/mirror only. It does not apply RGBW conversion.

Therefore the preview is RGB-only and would display full color if the stored colors were full color. It appears black/white because the editor has already replaced/constrained the colors to white/grayscale.

## 7. UI-only bug or actual DMX output?

This affects actual DMX output indirectly because it changes the stored RGB Matrix colors (`m_rgbColors`) via `m_matrix->setColor()`.

Engine RGBW output itself appears designed to preserve color:

- `engine/src/rgbmatrix.cpp` lines 925-971 handle `ControlModeRgbw` and `ControlModeRgbwBrighter` by splitting RGB into RGBW:
  - Accurate: subtracts extracted white from RGB (`r-w`, `g-w`, `b-w`, `w`).
  - Brighter: keeps RGB unchanged and also adds white.

So the engine-side RGBW mapping is not inherently black/white. The editor bug feeds it grayscale/white input colors, so the final DMX output becomes grayscale/white even though RGBW output could handle colored RGB input.

## Likely fix direction

Treat `ControlModeRgbw` and `ControlModeRgbwBrighter` as color-preserving modes in `RGBMatrixEditor`:

- `updateColors()` should not force `Qt::white` for RGBW modes.
- `setColorAtIndex()` should not grayscale colors for RGBW modes.
- Only true single-channel modes (`White`, `Amber`, `UV`, `Dimmer`, `Shutter`) should force grayscale/white behavior.

