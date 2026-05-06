# Fix B1+B2 — Production AudioAnalyzer Wiring

## What changed

- `Doc` now owns the production `AudioAnalyzer` through `Doc::audioAnalyzer()`.
- `Doc::audioInputCapture()` attaches the analyzer to every created `AudioCapture` with `setAnalyzer()`.
- `Doc::destroyAudioCapture()` detaches the analyzer before releasing the capture, so audio device switches do not leave a stale analyzer pointer in an old capture instance.
- `Doc::addAudioProfile()` binds every accepted profile to the analyzer, including XML-loaded profiles and profiles created by `ensureDefaultAudioProfile()`.
- `Doc::removeAudioProfile()` releases the analyzer channel before deleting the profile.
- `Doc::clearContents()` now releases and deletes profiles before destroying capture and deleting the analyzer.

## Runtime effect

The capture thread now sends each `AudioFrame` into `AudioAnalyzer::processFrame()`, and each `AudioProfile` owns a live analyzer channel. Consumers that resolve `profile->channel()` can receive enriched DSP snapshots instead of always seeing `nullptr`.

## Lifetime notes

`AudioAnalyzer` is lazy-created by `Doc::audioAnalyzer()`. `clearContents()` tears down profile bindings first, then capture, then the analyzer. If later code adds a new profile or capture after a clear, the analyzer is recreated and wired again through the same `Doc` accessors.
