# P3 RGBScript Profile Resolution + Audio Object Enrichment

Implemented runtime AudioProfile resolution for v4 RGB scripts.

## Resolution

- `RGBScript::setupAudioCapture()` now resolves the owning `RGBMatrix`.
- The matrix function ID is passed through `Doc::audioProfileForFunction()` so explicit profile, default profile, first profile, and `nullptr` fallback behavior stays centralized in `Doc`.
- If no profile exists, `Doc::ensureDefaultAudioProfile()` is called.
- The resolved `AudioChannel*` is cached on `RGBScript` and cleared during teardown.
- The selected profile is logged once per resolved profile ID:
  `RGBScript <name>: using audio profile <profileName> (ID: <id>)`.

## Audio object v2

When the resolved profile has a bound channel, `buildAudioDataObject()` takes exactly one `AudioSnapshot` copy and adds:

- `version = 2`
- `bands.{sub,bass,lowMid,mid,high,low}`
- `triggers.{sub,bass,lowMid,mid,high,volume,beat}` with value, active, edge, hold, and cooldown fields
- `volume.{raw,smoothed,agc,normalized,legacy}`
- `music.{beat,bpm,beatPhase,beatConfidence}`
- `features.{rmsDb,peakDb,crestFactor,centroidHz,rolloffHz,flatness,flux}`
- `audioDtMs`, `consumerDtMs`, and `brightnessFloor`

Legacy fields remain present: `spectrum`, `volume`, `beat`, `bpm`, and `maxMagnitude`. `volume` is a JavaScript `Number` object when v2 data is available, so numeric coercion remains compatible while allowing `volume.raw` style access.

## Verification

```bash
cd build && cmake .. -Dqmlui=ON && cmake --build . --target qlcplus-qml -j8
```

Result: passed.
