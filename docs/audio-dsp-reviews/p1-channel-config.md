# P1 Channel Config

Implemented `AudioChannelConfig` as pure C++ structs in `engine/audio/src/audiochannelconfig.h` with the matching implementation in `audiochannelconfig.cpp`.

## Defaults

- Envelope: attack `25.0 ms`, release `180.0 ms`
- AGC: enabled, max gain `18.0 dB`, release `1500.0 ms`, noise floor `-54.0 dB`, input gain `1.6`
- Trigger: high `0.65`, low `0.45`, hold `80.0 ms`, cooldown `120.0 ms`
- Band layout: sub `60 Hz`, bass `250 Hz`, low-mid `500 Hz`, mid `2000 Hz`, high `5000 Hz`
- Noise gate: threshold `-54.0 dB`, hold `120.0 ms`
- Brightness floor: `0.0`
- Volume smoothing: `100.0 ms`

## Legacy slider migration

`AudioChannelConfig::fromLegacySliders()` starts from defaults, clamps legacy slider ranges, and maps:

- `presetGain` to `agc.inputGainLinear = 0.6 + gain * 0.2`
- `presetReactivity` to `envelope.attackMs = -40.0 / log(1.0 - alpha)`, with `alpha = min(0.1 + reactivity * 0.09, 0.999)`
- `presetFloor` to `brightnessFloor = floor / 100.0`
- `presetSensitivity` to `triggers.highThreshold = 0.45 - sensitivity * 0.04` and `triggers.lowThreshold = highThreshold * 0.7`

## Build integration

Added `audiochannelconfig.cpp` and `audiochannelconfig.h` to `engine/audio/src/CMakeLists.txt` under the `qlcplusaudio` static library.

## Verification

Ran:

```bash
cd build && cmake .. -Dqmlui=ON && cmake --build . --target qlcplus-qml -j8 2>&1 | head -50
```

Result: build completed successfully and reported `Built target qlcplus-qml`.
