# P1 — Wire `AudioCapture` to `AudioFrame`

## What landed

- `AudioCapture` now owns a monotonic `m_frameIndex` and `m_lastFrameTimeNs` timestamp state.
- `processData()` increments the frame counter once per capture block, including silent blocks.
- The RMS pass now also tracks normalized peak amplitude (`peakAbs`).
- A preallocated FFT magnitude scratch buffer is filled after FFT + low-bin noise clearing.
- A stack `AudioFrame` is populated on every block and synchronously passed to an optional `AudioAnalyzer` via `processFrame()`.
- Silent frames no longer skip frame construction; the analyzer sees them before legacy zero-band emission and return.
- Legacy `dataProcessed()` output and beat signal order are preserved when no analyzer is installed.

## AudioFrame fields

- Identity/timing: `frameIndex`, `hostTimeNs`, `sampleRate`, `fftSize`, `binCount`, `silent`.
- Time-domain: `samples`, `sampleCount`, `rms`, `peak`, `dcOffset`.
- Frequency-domain: `magnitudes`; `bands32` remains `nullptr` until analyzer band generation lands.
- Scalar features use silence-sane defaults for now: dB values/noise floor `-96`, crest factor `1`, flux/centroid/rolloff `0`, flatness `1`.
- Beat state is copied from the existing `BeatTracker` result for the current block.

## Notes

`AudioAnalyzer` is still only an optional synchronous interface. No new thread or hot-path heap allocation was added; all buffers are owned by `AudioCapture` and valid only during the analyzer callback.
