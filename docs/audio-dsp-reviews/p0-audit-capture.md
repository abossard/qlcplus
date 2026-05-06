# P0 Audit — `AudioCapture::processData()` AudioFrame Insertion Points

**File:** `engine/audio/src/audiocapture.cpp` (lines 256–372)
**Header:** `engine/audio/src/audiocapture.h`
**Thread:** `AudioCapture` QThread (see `run()` at L374). Holds `m_mutex` for the duration of `processData()` (L392).

## 1. Pipeline data flow (with line numbers)

| Stage | Lines | Output |
|---|---|---|
| Frame timing constants computed | L259 (`frameSec = m_bufferSize / m_sampleRate`) | `frameSec` (seconds per frame) — exact `audioDtMs` candidate |
| Attack/release alphas for power smoothing | L262–263 | — |
| `smoothPower()` lambda (single-pole IIR, asymmetric) | L265–272 | smoothed `quint32` |
| **Mono mixdown** (int16 → int16) | L274–280 | `m_audioMixdown[0..m_bufferSize)` |
| DC mean | L284–287 | `mean` |
| **Raw RMS** + DC removal + normalization to [-1,1] into `m_fftInputBuffer` | L289–297 | `rms` (double, [0,1]) and `m_fftInputBuffer[i] = (sample - mean)/32768` |
| Silence gate (early return) | L301–318 | emits zeroed bands; `m_signalPower = smoothPower(0.0)` |
| **Hanning window** (in-place on `m_fftInputBuffer`) | L328–332 (under `USE_HANNING`) | windowed time-domain samples |
| **FFT** (`fftw_execute(m_plan_forward)`) | L339 | `m_fftOutputBuffer` (fftw_complex, `m_bufferSize/2 + 1` bins) |
| Low-bin noise clear (< 20 Hz) | L345–352 | — |
| Per-registered-N log-band magnitudes via `fillBandsData(N)` | L356–371 (helper L203–254) | `m_fftMagnitudeMap[N].m_fftMagnitudeBuffer`, `maxMagnitude` |
| Raw power sum → `smoothPower` → emit `dataProcessed` | L362–370 | `m_signalPower` (smoothed) |

Note: there is **no peak amplitude** computed today. It must be added (cheap: track `qAbs(x)` max in the L289–296 loop).

## 2. Best insertion point for AudioFrame

**Single point: immediately after the FFT and noise-clear, before the band loop (between L354 and L356).**

At that line, all raw sources exist simultaneously:

| AudioFrame field | Source at insertion point | Lifetime |
|---|---|---|
| `samples` (mono time-domain) | `m_audioMixdown` (int16, length `m_bufferSize`) — L67 heap-allocated, persists | heap, owned by AudioCapture, overwritten next frame |
| `sampleCount` | `m_bufferSize` | const after ctor |
| `fftMagnitudes` | computed from `m_fftOutputBuffer` (fftw_complex, `m_bufferSize/2 + 1` bins) — L71 heap | heap, owned by AudioCapture, overwritten by next `fftw_execute` |
| `binCount` | `m_bufferSize / 2 + 1` (or `/2` if excluding Nyquist, matching `maxBin` at L212) | — |
| `rmsRaw` | local `rms` (L297) — must be hoisted in scope | local double; copy by value |
| `peakRaw` | **not yet computed** — add `peak = max(peak, qAbs(x))` in L291–296 loop | local double; copy |
| `sampleRate` | `m_sampleRate` | member |
| `audioDtMs` | `frameSec * 1000.0` (already computed at L259) | local double |
| `frameIndex` | **not present today** — add monotonic `quint64 m_frameIndex` member, increment once per `processData()` call (e.g. top of L256 or end before return) | new member |

### Why here (not later)

- After FFT but before per-band aggregation: AudioAnalyzer can compute its own bands/features without re-running FFT.
- Before `smoothPower` is applied to `m_signalPower`: AudioFrame sees **raw** RMS / power.
- Before `dataProcessed` emits: analyzer can run synchronously and influence what the rest of the pipeline sees, if needed.

### Silence-gate path

Currently returns at L317. To deliver consistent frames, also emit an AudioFrame with zeroed FFT magnitudes and `peakRaw = 0` in that branch (or compute analyzer pre-gate so silence frames still flow). Recommend: build the AudioFrame **before** the silence gate, with FFT magnitudes nullable/zero-filled — analyzer can decide.

## 3. Data lifetime / pointer scope concerns

- `m_audioMixdown`, `m_fftInputBuffer`, `m_fftOutputBuffer` are **heap, AudioCapture-owned, mutated every frame**. Pointers in AudioFrame are only valid for the duration of one `processData()` call.
- `m_mutex` is held across the entire `processData()` call (locked at L392 in `run()`). Any synchronous analyzer call from inside `processData()` runs under that lock — safe for direct pointer access; **must not block on the main thread** (would deadlock with UI registering bands).
- Stack-allocated locals (`rms`, `mean`, `frameSec`) must be copied into the AudioFrame **by value** (not referenced) if the frame is queued for cross-thread delivery.
- `m_fftOutputBuffer` is `fftw_complex*` (interleaved real/imag doubles, length `m_bufferSize/2 + 1`). If the analyzer wants `magnitudes` rather than complex, compute `sqrt(re²+im²)` once into a small heap or AudioCapture-owned scratch buffer; do not allocate per frame.

## 4. Threading concerns

- `processData()` runs on the AudioCapture thread; UI consumers receive data via `dataProcessed` queued signal (cross-thread).
- An `AudioAnalyzer` invoked here runs on the AudioCapture thread under `m_mutex`. Two safe patterns:
  1. **Synchronous, in-thread**: analyzer is a plain object owned by AudioCapture; its `process(const AudioFrame&)` runs inline. Cheapest, no copies, no deadlock risk *as long as* analyzer does not call back into AudioCapture's mutex-protected API.
  2. **Cross-thread, copy-out**: build an `AudioFrame` with **owned** sample/magnitude vectors (deep copies) and emit a Qt signal. Adds two `m_bufferSize`-element allocations per frame at 86 Hz (`44100/512` ≈ 86) — ~700 KB/s — acceptable but wasteful.
- `m_frameIndex` is private to the AudioCapture thread; readers from other threads need an atomic or signal-based snapshot (rarely needed — usually only the analyzer cares).

## 5. Existing frame counter / timing

- **No frame counter exists today.** Must be added as `quint64 m_frameIndex {0};` in the header and incremented once per `processData()`.
- Timing: `frameSec` (L259) gives exact `audioDtMs = frameSec * 1000.0`. No wall-clock timestamping today; if needed, capture `QDateTime::currentMSecsSinceEpoch()` once at top of `processData()` (cheap).

## 6. Recommended approach

### Header changes (`audiocapture.h`)
- Add a forward-declared `class AudioAnalyzer;` and a member `AudioAnalyzer *m_analyzer = nullptr;` with setter (analyzer is optional, registered by qmlui or tests).
- Add `quint64 m_frameIndex = 0;`.
- Define `AudioFrame` in a new header `engine/audio/src/audioframe.h` as a plain struct of **non-owning const pointers + scalars** (POD, no Qt). All buffers point into AudioCapture-owned memory; struct is valid only for the synchronous `process()` call.

```cpp
struct AudioFrame {
    const int16_t *samples;       // m_audioMixdown
    int            sampleCount;   // m_bufferSize
    const double  *fftMagnitudes; // pre-computed scratch buffer (sqrt(re²+im²))
    int            binCount;      // m_bufferSize/2 + 1 (or maxBin = m_bufferSize/2)
    double         rmsRaw;        // pre-smoothing
    double         peakRaw;       // pre-smoothing, |x|max in normalized [0,1]
    int            sampleRate;
    double         audioDtMs;
    quint64        frameIndex;
};
```

### `processData()` insertion (between L354 and L356)
1. Hoist `peak` accumulator into the L291–296 loop (track `qAbs(x)` max).
2. Increment `m_frameIndex` at top of `processData()` (or before invoking analyzer).
3. Compute `m_fftMagnitudeScratch[i] = sqrt(re² + im²)` for `i = 0..maxBin` (one new heap buffer of size `m_bufferSize/2 + 1`, allocated in ctor next to `m_fftInputBuffer`).
4. Construct stack `AudioFrame frame{...}`.
5. `if (m_analyzer) m_analyzer->process(frame);` — synchronous, no allocation, no thread crossing.

### Silence path
Build a degenerate `AudioFrame` with `fftMagnitudes = m_zeroBins` (preallocated zero buffer) and `peakRaw = 0`, deliver before the early `return` at L317. Keeps analyzer state consistent (no lost frames).

### Why synchronous in-thread is preferred
- Zero allocation per frame.
- AudioAnalyzer can be unit-tested with a plain `AudioFrame` fixture (no Qt).
- No mutex contention beyond what already exists.
- AudioFrame pointer lifetime is well-defined: valid during `process()`, invalid after — analyzer must copy out anything it wants to keep (its smoothed state).
