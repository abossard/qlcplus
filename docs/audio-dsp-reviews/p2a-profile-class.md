# P2A AudioProfile class

Implemented the Phase 2A `AudioProfile` document-model class.

## Added

- `engine/src/audioprofile.h`
- `engine/src/audioprofile.cpp`

`AudioProfile` now owns:

- stable document ID
- editable name
- default-profile flag
- `AudioChannelConfig`
- simple analyzer/channel binding
- XML load/save for schema version 1
- legacy slider migration via `AudioChannelConfig::fromLegacySliders()`

## XML shape

Profiles persist as direct `<Engine>` children:

```xml
<AudioProfile ID="0" Name="Default" IsDefault="True" Version="1">
  <Envelope Attack="25" Release="180"/>
  <Agc MaxGain="18" Release="1500" NoiseFloor="-54" InputGain="1.6" Enabled="True"/>
  <Triggers High="0.65" Low="0.45" Hold="80" Cooldown="120"/>
  <Bands SubMax="60" BassMax="250" LowMidMax="500" MidMax="2000" HighMax="5000"/>
  <NoiseGate Threshold="-54" Hold="120"/>
  <Volume Smoothing="100" BrightnessFloor="0"/>
</AudioProfile>
```

## Doc integration

`Doc` now stores audio profiles in `m_audioProfiles` and exposes minimal accessors:

- `audioProfile(id)`
- `addAudioProfile(profile)`
- `removeAudioProfile(id)`
- `audioProfiles()`
- `defaultAudioProfile()`

`Doc::loadXML()` loads `<AudioProfile>` elements without affecting unknown-element skipping, and `Doc::saveXML()` writes profiles before functions.
