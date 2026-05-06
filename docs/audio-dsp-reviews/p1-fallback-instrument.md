# P1 AudioAnalyzer Fallback + Instrumentation

## Summary

Implemented two small `AudioAnalyzer` updates:

- Added `AudioAnalyzer::defaultChannel()` as a lazy anonymous fallback for scripts without an `AudioProfile`.
- Added lightweight `std::chrono::steady_clock` timing inside `processFrame()`.

## Anonymous default channel fallback

- `defaultChannel()` returns a dedicated `AudioChannel` configured with `AudioChannelConfig::defaults()`.
- If no channels exist, the first `defaultChannel()` call lazily creates the anonymous fallback.
- The fallback channel is inserted into the existing channel list, so `processFrame()` updates it with the same path as configured channels.

## Frame budget instrumentation

`processFrame()` now records exponential moving averages (`alpha = 0.01`) for:

- Shared feature computation time.
- Total channel update time.
- Total frame processing time.

Public getters expose:

- `avgFrameTimeUs()`
- `avgChannelTimeUs()`

A one-time `qWarning()` is emitted if a frame exceeds the 1000 µs budget. Timing values remain plain doubles because they are intended for audio-thread access only.

## Hot-path notes

- No new heap allocation occurs in `processFrame()`.
- The fallback allocation happens only on the first `defaultChannel()` call.
- Timing uses `std::chrono`, not Qt timers.

## Verification

Ran successfully:

```bash
cd build && cmake .. -Dqmlui=ON && cmake --build . --target qlcplus-qml -j8
```

Result: `qlcplus-qml` built successfully.
