# P1 Channel API

Implemented the handle-style audio channel API in `engine/audio/src`.

- `AudioSnapshot` is a copyable value object with five perceptual bands, 32-band spectrum, trigger states, volume, music, feature, timing, and brightness-floor fields.
- `AudioChannel` owns per-consumer DSP state: band envelopes, AGC gain, volume smoothing, noise-gate state, and seven Schmitt triggers.
- `AudioChannel::updateConfig()` records a pending config under `QMutex`; the audio thread applies it at the next `update()`.
- `AudioChannel::snapshot()` takes a short `QMutex` lock and returns the latest snapshot by value.
- `AudioAnalyzer` now creates/destroys channel handles and updates all registered channels once per analyzed frame after shared feature computation.

Processing order per frame:

1. Apply any pending config.
2. Compute AGC gain from frame RMS dB.
3. Extract configured five-band layout from `frame.bands32`, apply gain/noise gate, and update attack/release envelopes.
4. Smooth full-range volume.
5. Advance Schmitt trigger hold/cooldown state for bands, volume, and beat.
6. Publish an immutable snapshot.
