# Phase 2B — VCAudioTriggers AudioChannel Handle

## Summary

- Added an `audioProfileId` property to `VCAudioTriggers` and persisted it as the `AudioProfileID` XML attribute.
- Kept the existing `AudioCapture` spectrum path intact for legacy bars, DMX output, function triggers, and widget triggers.
- Added a parallel AudioProfile/AudioChannel snapshot path that reads perceptual band levels when a bound analyzer is available.
- Exposed the new perceptual QML properties: `subPower`, `bassPower`, `lowMidPower`, `midPower`, and `highPower`.

## Behavior

- If `audioProfileId` resolves to a Doc profile, that profile is used.
- If no explicit profile is set, or the saved profile no longer exists, the widget falls back to `Doc::defaultAudioProfile()`.
- If the resolved profile has no bound `AudioChannel`, the new perceptual powers are reset to zero and legacy behavior continues unchanged.

## Compatibility

- Existing `lowsPower`, `midsPower`, `highsPower`, `audioLevels`, and per-bar trigger behavior still use the legacy path.
- Older loaders ignore the additive `AudioProfileID` attribute.
