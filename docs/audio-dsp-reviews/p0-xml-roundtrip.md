# P0 — VCAudioTriggers XML round-trip review

Goal: confirm we can safely add a `Version` attribute (and future child elements like `<Bands>`, `<Envelope>`) to `<AudioTriggers>` without breaking older QLC+ versions that read the same `.qxw` file.

## 1. Current XML schema

### v5 (QML UI) — `qmlui/virtualconsole/vcaudiotriggers.cpp`

Element constants:
- `KXMLQLCVCAudioTriggers = "AudioTriggers"` (`vcaudiotriggers.h:29`)
- `KXMLQLCAudioBarsNumber = "BarsNumber"` (cpp:46)
- `KXMLQLCAudioTriggerBar = "Bar"` (cpp:47)
- Bar attributes from `ui/src/audiobar.h`: `Index`, `Type`, `MinThreshold`, `MaxThreshold`, `Divisor`, `FunctionID`, `WidgetID`, `DMXChannels` (child element)

Saved structure (`saveXML`, cpp:1124):

```xml
<AudioTriggers BarsNumber="N">          <!-- N = barsNumber()-1 -->
  <!-- saveXMLCommon: Caption / ID / etc. (writes attrs on the open element) -->
  <WindowState .../>
  <Appearance>...</Appearance>
  <Input .../>            <!-- INPUT_ENABLE_CAPTURE  -->
  <Input .../>            <!-- INPUT_VOLUME_CONTROL -->
  <Bar Type="…" MinThreshold="…" MaxThreshold="…" Divisor="…" Index="…"
       [FunctionID="…" | WidgetID="…"]>
    <DMXChannels>fxi,ch,fxi,ch,…</DMXChannels>   <!-- only for DMXBar type -->
  </Bar>
  <!-- one <Bar> per configured (non-None) entry in m_spectrumBars -->
</AudioTriggers>
```

`loadXML` (cpp:1068) accepts these children:
`WindowState`, `Appearance` (`KXMLQLCVCWidgetAppearance`), `Input` (`KXMLQLCVCWidgetInput`), `Key` (`KXMLQLCVCWidgetKey`), and **any of** `Bar` / `VolumeBar` / `SpectrumBar` (legacy element names from v4 are still accepted by `loadBarXML`).

`loadBarXML` (cpp:931) requires `Type` attribute, reads `Index`, `MinThreshold`, `MaxThreshold`, `Divisor`, then dispatches on type to read `FunctionID`, `WidgetID`, or the `<DMXChannels>` child. Legacy bar index `1000` is remapped to `0` (volume bar transposition).

### v4 (Qt Widgets UI) — `ui/src/virtualconsole/vcaudiotriggers.cpp`

Same root element name `AudioTriggers`, but:
- attribute is `BarsNumber` via `KXMLQLCVCATBarsNumber`
- bars are saved as separate `<VolumeBar>` / `<SpectrumBar>` elements (not `<Bar>`)
- `loadXML` (cpp:616) accepts: `WindowState`, `Appearance`, `Input`, `Key`, `VolumeBar`, `SpectrumBar`

(Pre-existing fact: v5-saved `<Bar>` elements are not parsed by v4 — that's an existing forward-compat gap, not introduced by Version.)

### Test fixture

`./audio_tests.qxw` shows current real-world output, e.g.:

```xml
<AudioTriggers BarsNumber="16" Caption="Audio Trigger 9" ID="8">
  <WindowState .../>
  <Appearance>...</Appearance>
  ...
</AudioTriggers>
```

No `Version` attribute is present today.

## 2. Unknown attribute handling

**Safe — silently ignored** in both v4 and v5.

Both loaders only inspect specific attributes via `attrs.hasAttribute(KXML…)` followed by `attrs.value(KXML…)`. Any attribute not explicitly named is never read and never reported. Adding `Version="2"` to `<AudioTriggers>` will not produce a warning, an error, or a parse failure on older builds.

Same is true at the `<Bar>` / `<VolumeBar>` / `<SpectrumBar>` level — `loadBarXML` only checks the bar attributes it knows about.

## 3. Unknown child element handling

**Safe — skipped with a `qWarning`** in both v4 and v5.

v5 (`vcaudiotriggers.cpp:1114`):
```cpp
else
{
    qWarning() << Q_FUNC_INFO << "Unknown audio trigger tag:" << root.name().toString();
    root.skipCurrentElement();
}
```

v4 (`vcaudiotriggers.cpp:670`): identical pattern.

So future children like `<Bands>`, `<Envelope>`, `<Smoothing>`, etc. will:
- emit a single debug-log warning per unknown tag,
- be fully consumed by `skipCurrentElement()` (including any nested content),
- not abort the parse — siblings continue to load normally.

No crash, no data loss for the rest of the widget.

## 4. Precedent: how other widgets version their XML

`engine/src/scriptv4.cpp` is the closest example (`KXMLQLCScriptVersion = "Version"`):

- **Default when missing:** `int version = 1;` then `if (attrs.hasAttribute(KXMLQLCScriptVersion)) version = attrs.value(...).toInt();`
- **Always write current:** `doc->writeAttribute(KXMLQLCScriptVersion, QString::number(2));`
- **Branch on version inside the loader** to handle legacy semantics (e.g. percent-encoding differences between v1 and v2 lines).

Engine-level `KXMLQLCCreatorVersion = "Version"` (`engine/src/qlcfile.h:60`) is used as a *child element* on the workspace root, not as an attribute, so it's a separate convention.

## 5. Recommendation

Adopt the `scriptv4.cpp` pattern for `<AudioTriggers>`:

1. Add `#define KXMLQLCAudioTriggersVersion QStringLiteral("Version")` in `qmlui/virtualconsole/vcaudiotriggers.cpp`.
2. In `saveXML`, after `writeStartElement(KXMLQLCVCAudioTriggers)` and the existing `BarsNumber` attribute, write:
   ```cpp
   doc->writeAttribute(KXMLQLCAudioTriggersVersion, QString::number(2));
   ```
3. In `loadXML`, before/after the existing `BarsNumber` read:
   ```cpp
   int version = 1;
   if (attrs.hasAttribute(KXMLQLCAudioTriggersVersion))
       version = attrs.value(KXMLQLCAudioTriggersVersion).toInt();
   ```
   Pass `version` down to any per-feature loader that needs to disambiguate legacy vs new semantics (e.g. for new `<Bands>` children or any field whose meaning changes).
4. New child elements (`<Bands>`, `<Envelope>`, …) can be added to the `else if` ladder. They will be silently skipped by older builds and by older v5 builds without changes.
5. Do **not** mirror the attribute on `<Bar>` unless bar-level semantics change; keep `Version` on the root element only to minimise file diff noise.

## 6. Risks / edge cases

- **Forward compatibility (old reads new):** ✅ `Version="2"` and any new child elements are silently dropped by both v4 and v5 loaders. Existing data still loads. Only side-effect is `qWarning` log lines for unknown children.
- **Backward compatibility (new reads old):** ✅ Loader must default `version = 1` when the attribute is missing. As long as semantics for `version == 1` match the current behaviour, old files load unchanged.
- **v4 loader and `<Bar>` elements:** ⚠️ Pre-existing — v5 already writes `<Bar>` instead of `<VolumeBar>`/`<SpectrumBar>`, so v4 cannot reconstruct bars saved by v5. Adding `Version` does not make this worse.
- **`saveXMLCommon` ordering:** `<AudioTriggers>` writes `BarsNumber` *before* `saveXMLCommon`. Add `Version` in the same window (between `writeStartElement` and any child element) so it lands on the open tag. Adding it after a child element starts would put it on the wrong element.
- **Legacy `barIndex == 1000`:** Volume-bar transposition lives in `loadBarXML`. New version logic should not collide with this; if a v2 file uses different indexing, gate the remap on `version == 1`.
- **Numeric parsing:** `attrs.value(...).toInt()` returns `0` on missing/invalid values. If `Version` is absent we explicitly default to `1`, so a malformed `Version=""` would degrade to `0` — treat unknown/zero as `1` defensively:
  ```cpp
  if (version <= 0) version = 1;
  ```
- **Round-trip stability:** loader currently does not preserve unknown attributes/children; a load+save cycle on an old build will *strip* any `Version` or new child elements. This is the standard QLC+ behaviour for every widget — flag it to users in release notes if a workflow involves shuttling `.qxw` files between versions.

## 7. Conclusion

The XML round-trip for `<AudioTriggers>` is **stable enough to safely introduce a `Version` attribute and additional child elements**. Both v4 and v5 loaders ignore unknown attributes silently and skip unknown children with only a debug warning. Follow the `scriptv4.cpp` pattern (default to 1, always write the current version, branch in the loader) and the change is non-breaking in both directions.
