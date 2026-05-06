# B10 — VCAudioTrigger profile editing

## Fix

- Added QML-readable `VCAudioTriggers` properties for envelope, AGC, and trigger profile config.
- Added invokable setters that update the resolved `AudioProfile` through `AudioProfile::setChannelConfig()` so the live `AudioChannel` receives `updateConfig()`.
- Setters auto-create the default profile when no profile exists yet.
- Replaced QML placeholder/TODO values with real widget bindings and editable controls for:
  - envelope attack/release
  - AGC max gain/noise floor
  - trigger high/low thresholds and hold/cooldown timing
  - spectral low/high bin and bar-count reads

## Files

- `qmlui/virtualconsole/vcaudiotriggers.h`
- `qmlui/virtualconsole/vcaudiotriggers.cpp`
- `qmlui/qml/virtualconsole/VCAudioTriggersProperties.qml`
