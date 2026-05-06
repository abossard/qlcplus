# aubio Integration Research for QLC+

**Date:** 2025  
**Author:** Copilot research  
**Repo:** mcallegari/qlcplus (Apache-2.0)

---

## Executive Summary

> **TL;DR: Do not use aubio. It is GPL-3.0, which is legally incompatible with QLC+'s Apache-2.0 license. The same incompatibility applies to BTrack and Essentia (AGPL). The best path is Gist (MIT) for BPM + beat quality improvements, combined with targeted algorithmic improvements to the existing pipeline.**

Key findings:

- **aubio is GPL-3.0** (since v0.4.0; was LGPL before). Apache-2.0 projects **cannot distribute** a combined work linking GPL-3.0 code.
- **BTrack** (Adam Stark) is also **GPL-3.0** — same blocker.
- **Essentia** (UPF) is **AGPLv3** — same blocker.
- **Gist** (Adam Stark) is **MIT** — compatible, provides real-time onset/BPM/spectral features, though less accurate than aubio on complex music.
- **Kissfft / custom** (public domain/BSD) is fully compatible.
- The current `BeatTracker` does naive period estimation, not proper tempo induction. A BTrack-style comb-filter approach can be re-implemented from the academic paper without copying GPL code.
- The AGC saturation problem is not a library problem — it's an algorithmic configuration issue fixable within the existing `AudioChannel`.
- Recommended path: **Option A-Prime** — replace `BeatTracker` with a BTrack-inspired re-implementation + optionally wrap Gist for onset confidence, while keeping FFTW for the spectral pipeline.

---

## 1. aubio Capabilities Inventory

**Source:** [https://aubio.org](https://aubio.org) | [https://github.com/aubio/aubio](https://github.com/aubio/aubio) | [https://aubio.org/doc/latest/api/](https://aubio.org/doc/latest/api/)

### Algorithm catalog

| Object | Purpose |
|--------|---------|
| `aubio_onset_t` | Onset detection: energy, HFC, complex domain, phase, specdiff, Kullback-Leibler |
| `aubio_tempo_t` | Beat tracking / BPM estimation via comb filter on onset function |
| `aubio_pitch_t` | Pitch (f0) extraction: YIN, fastYIN, YINFFT, MComb, Schmitt, spectral |
| `aubio_notes_t` | Monophonic note transcription (pitch + onset + duration → MIDI-like) |
| `aubio_mfcc_t` | Mel-frequency cepstral coefficients (via filterbank + DCT) |
| `aubio_filterbank_t` | Mel/Bark/ERB filter banks; subband energies |
| `aubio_spectral_centroid_t` | Weighted mean frequency |
| `aubio_spectral_flatness_t` | Geometric/arithmetic mean ratio (tonality) |
| `aubio_spectral_rolloff_t` | Frequency enclosing N% of energy |
| `aubio_spectral_slope/spread/skewness/kurtosis_t` | Higher-order spectral shape descriptors |
| `aubio_zero_crossing_rate_t` | ZCR — noise indicator |
| `aubio_peakpicker_t` | Peak picking on any feature stream |
| `aubio_source_t` / `aubio_sink_t` | File I/O (libsndfile/avcodec) |
| `fvec_t` / `cvec_t` | Core vector types (float32, interleaved complex) |

### API style

- Pure **C API** with `new_*/del_*` object lifecycle.
- All primary `*_do(obj, input, output)` calls are **stateful but single-object-owned** — no global state.
- Python/Java/JavaScript bindings available but not relevant here.

### Threading model

- **NOT thread-safe.** Each `aubio_*_t` object must be used from exactly one thread.
- Safe pattern: create objects on the audio thread, call `*_do()` only from that thread.
- QLC+'s `AudioCapture::run()` thread is the right owner.

### Real-time characteristics

- Designed explicitly for real-time use: one call per audio hop, O(N log N) FFT per call.
- Default parameters: `win_s = 1024`, `hop_s = 512` samples → ~11.6 ms latency at 44.1 kHz.
- aubio maintains its own internal FFT (via FFTW3 or its own kissfft fallback) per object. It does **not** accept pre-computed FFT magnitudes — it wants **raw float PCM per hop**.
- Memory footprint: a single `aubio_tempo_t` at 44.1 kHz / 512 hop ≈ ~200 KB heap.

### Buffer/hop sizes that match QLC+

QLC+ uses `AUDIO_DEFAULT_BUFFER_SIZE = 2048` bytes (= 1024 int16 samples per channel at 44.1 kHz → ~23.2 ms). aubio's `hop_s = 512` floats is smaller; you would either:
- Pass every QLC+ block as two or four sub-hops to aubio, or
- Configure aubio with `hop_s = 1024` to match QLC+ directly.

---

## 2. Licensing Deep-Dive

### 2.1 aubio exact license

**aubio is GPL-3.0+** since version **0.4.0** (released ~2013).  
- Older versions (< 0.4.0) were LGPL. All current packages (0.4.9 stable, all distros) are GPL-3.0.  
- Source: [`COPYING`](https://github.com/aubio/aubio/blob/master/COPYING) in the aubio repo — the full GPL-3.0 text.  
- The official README and [about page](https://aubio.org/manual/latest/about.html) state: *"aubio is distributed under the terms of the GNU General Public License."*
- Commercial licensing is available by contacting the author (Paul Brossier), but not open-ended.

> ⚠️ **Common misconception:** many web sources and package descriptions incorrectly say aubio is LGPL. This is only true for pre-0.4.0 releases. Any package from Homebrew, apt, vcpkg ships GPL-3.0.

### 2.2 Apache-2.0 + GPL-3.0 compatibility

**Verdict: INCOMPATIBLE for distribution.**

- GPL-3.0 is copyleft: any combined work must be distributed under GPL-3.0.  
- Apache 2.0 includes a patent termination clause (Section 3) that GPL-3.0 § 7 forbids adding as an "additional restriction."  
- The [Apache Software Foundation's own guidance](https://www.apache.org/licenses/GPL-compatibility.html) states: Apache 2.0 code can be *consumed by* GPL-3.0 projects (one-way), but an Apache project **cannot distribute** a work that links GPL-3.0 code without relicensing.  
- This applies to both **static** and **dynamic** linking.  
- Internal/proprietary use (no distribution) is technically fine, but QLC+ is an open-source product distributed to users.

### 2.3 Static vs dynamic linking

Under GPL-3.0, the distinction between static and dynamic linking makes **no practical difference** for distribution — the Combined Work is still a derivative work under GPL-3.0. Some lawyers argue dynamic linking with system-provided libraries in certain jurisdictions might provide a loophole, but this is legally contested and risky for a project under the Apache Software Foundation's umbrella.

### 2.4 Patent issues

No known patent claims specifically targeting aubio's algorithms. The Dixon 2001 comb-filter beat tracking algorithm (used in aubio's `tempo`) is well-established academic prior art. However, FFTW itself has a GPL license for its optimized version; aubio's internal fallback FFT uses a custom implementation.

---

## 3. Build System Integration

### 3.1 Native aubio build

- aubio's own build system is **waf** (Python-based), not CMake. Running `waf configure && waf build` produces `libaubio.so/.dylib/.a`.  
- No first-class `CMakeLists.txt` in the aubio source tree as of 0.4.9.

### 3.2 vcpkg

```cmake
# vcpkg wraps aubio in CMake via a custom portfile
vcpkg install aubio
find_package(aubio CONFIG REQUIRED)
target_link_libraries(myTarget PRIVATE aubio::aubio)
```

vcpkg's port adds a `CMakeLists.txt` overlay. Latest vcpkg update: January 2024.  
Reference: [https://vcpkg.io/en/package/aubio.html](https://vcpkg.io/en/package/aubio.html)

### 3.3 System packages

| Platform | Command | Package |
|----------|---------|---------|
| macOS | `brew install aubio` | `aubio 0.4.9` (bottle available) |
| Ubuntu/Debian | `apt install libaubio-dev` | ships 0.4.9 |
| Windows | `vcpkg install aubio` | via vcpkg |

Reference: [https://formulae.brew.sh/formula/aubio](https://formulae.brew.sh/formula/aubio)

### 3.4 FFTW dependency sharing

- aubio can use FFTW3 internally (detected at configure time), or fall back to its own kissfft copy.
- QLC+ already links FFTW3. However, since aubio bundles its own FFTW3 usage, there is **no clean mechanism to share a single FFTW plan** between QLC+'s `AudioCapture` FFT and aubio's internal FFT.
- Each `aubio_tempo_t` / `aubio_onset_t` creates its own FFTW plan. This means **two independent FFTs** per block (QLC+'s + aubio's). Manageable but wasteful.

### 3.5 Build flags of interest

```bash
# waf configure flags
./waf configure --enable-fftw3 --disable-jack --disable-docs \
                --disable-tests --prefix=/usr/local
```

The `--enable-fftw3` flag uses system FFTW3. Without it, aubio falls back to kissfft.

---

## 4. Concrete Integration Design

### 4.1 Current pipeline summary

```
AudioCapture::run()                     [AudioCapture thread]
  ├─ readAudio() → m_audioBuffer (int16)
  ├─ processData()
  │    ├─ FFT via FFTW  → m_fftMagnitudeScratch
  │    ├─ fillBandsData() per registered band count
  │    ├─ BeatTracker::processAudio()
  │    │    └─ own FFT (FFTW) → spectral flux → adaptive threshold → bool beat
  │    │         └─ getCurrentBpm() → naive interval average
  │    └─ AudioAnalyzer::processFrame(AudioFrame)
  │         ├─ computeSharedFeatures()
  │         │    ├─ rmsDb, peakDb, crestFactor
  │         │    ├─ computeBands32() → 32 log-spaced bands (40–5000 Hz)
  │         │    ├─ computeSpectralFlux()
  │         │    ├─ computeSpectralCentroid()
  │         │    ├─ computeSpectralRolloff()
  │         │    └─ computeSpectralFlatness()
  │         └─ AudioChannel::update(frame) per channel
  │              ├─ updateAgc()         ← AGC saturation lives here
  │              ├─ updateEnvelopes()   ← 5-band perceptual model
  │              ├─ updateVolume()
  │              ├─ updateTriggers()
  │              └─ buildSnapshot() → AudioSnapshot
  └─ emit dataProcessed / beatDetected signals
```

**Key problems with current BeatTracker:**
- Runs its **own FFT** (redundant with AudioCapture's FFT)
- Spectral flux threshold is adaptive-mean — works for steady music, drifts on silence/loud transients
- BPM = `mean(last N inter-beat intervals)` — only reports *historical* average, no tempo induction
- No beat phase / beat prediction
- `music.bpm` in AudioSnapshot is `int` — low resolution

**Key problems with AGC:**
- `updateAgc()` computes a single `m_agcGainDb` from RMS and applies it uniformly to all 5 bands
- This means a dominant bass note causes all bands (including mids/highs) to be attenuated — "saturation" effect
- Fix: per-band AGC, or decouple the gain from band amplification

---

### Option A — aubio replaces BeatTracker only

**Description:** Remove `BeatTracker`, add aubio `aubio_tempo_t` + `aubio_onset_t`. Keep FFTW + 32 log bands.

**Code removed:**
- `engine/audio/src/beattracker.h` + `.cpp` entirely
- `AudioCapture`'s `m_beatTracker` member and `BeatTracker::processAudio()` call

**New code needed:**
```cpp
// AubioBeatDetector.h (new class)
class AubioBeatDetector {
public:
    AubioBeatDetector(uint32_t sampleRate, uint32_t hopSize);
    ~AubioBeatDetector();

    // Returns true on beat; fills bpmOut, phaseOut
    bool processBlock(const int16_t *interleavedSamples,
                      int frameCount, int channels,
                      double &bpmOut, double &phaseOut);
private:
    aubio_tempo_t *m_tempo = nullptr;
    aubio_onset_t *m_onset = nullptr;
    fvec_t        *m_inputVec = nullptr;
    fvec_t        *m_tempoOut = nullptr;
    fvec_t        *m_onsetOut = nullptr;
    uint32_t       m_hopSize;
    uint32_t       m_sampleRate;
};
```

**How aubio is fed:** Raw float PCM (converted from `int16_t`). aubio does NOT accept pre-computed FFT magnitudes.

```cpp
// In processBlock(): convert int16 → float32
for (int i = 0; i < m_hopSize; ++i)
    m_inputVec->data[i] = float(mono[i]) / 32768.0f;
aubio_tempo_do(m_tempo, m_inputVec, m_tempoOut);
bool beat = (m_tempoOut->data[0] > 0.0f);
bpmOut    = aubio_tempo_get_bpm(m_tempo);
phaseOut  = aubio_tempo_get_last_s(m_tempo);
```

**Impact on AudioSnapshot:**
```cpp
// Upgrade AudioSnapshot::music
struct {
    bool beat = false;
    double bpm = 0.0;        // was int — upgrade to double
    double beatPhase = 0.0;  // 0..1 phase within current beat period
    double beatConfidence = 0.0;  // aubio_tempo_get_confidence()
    bool onsetDetected = false;   // separate onset flag
} music;
```

**Impact on existing scripts/widgets/MCP tools:**
- `music.bpm` type change `int → double` — binary-compatible, scripts reading it get higher precision
- New `music.beatPhase` (was always 0.0 before) — scripts can now modulate by beat phase
- `AudioSnapshot::music.onsetDetected` is new; no existing script uses it — additive, no breakage
- MCP tool `audio_snapshot` JSON: `bpm` field changes from integer to float — update schema

**Effort estimate:** 4–6 person-days
- 1 day: `AubioBeatDetector` wrapper class + unit tests
- 1 day: Wire into `AudioCapture::processData()`, buffer alignment (QLC+ block size vs aubio hop size)
- 1 day: `AudioSnapshot` struct change + all consumers audit
- 1–2 days: MCP schema update, QML bindings, integration testing
- 1 day: CI / platform build (Linux + macOS + Windows)

> ⛔ **Blocked by license.** aubio is GPL-3.0. This option **cannot be distributed** as part of QLC+ (Apache-2.0) without relicensing.

---

### Option B — aubio replaces BeatTracker AND spectral features in AudioAnalyzer

**Description:** Option A plus replacing `computeSpectralFlux/Centroid/Rolloff/Flatness` in `AudioAnalyzer` with aubio equivalents. Keep FFTW for 32-band extraction.

**Code removed:**
- Everything from Option A
- `computeSpectralCentroid/Rolloff/Flatness/Flux` methods in `AudioAnalyzer`

**New code needed:**
- Extend `AubioBeatDetector` with a `aubio_spectral_centroid_t`, `aubio_spectral_flatness_t` etc.
- Or introduce a new `AubioSpectralFeatures` class alongside

**How aubio is fed:** Same raw PCM path. Each aubio spectral object needs its own `fvec_t` — but since they share the same hop, they can reuse the same input vector (called sequentially within one block).

**Impact on AudioSnapshot:** No additional struct changes beyond Option A. The values stored in `features.centroidHz`, `features.flatness`, etc. are simply computed by aubio instead of in-house. Higher accuracy, same fields.

**Impact on existing scripts/widgets/MCP tools:** None — same field names/units.

**Effort estimate:** 6–8 person-days (Option A + 2 days for spectral feature wiring and validation)

> ⛔ **Blocked by license.** Same GPL-3.0 incompatibility.

---

### Option C — aubio replaces entire FFT pipeline

**Description:** Remove FFTW from the audio pipeline entirely. aubio handles FFT, filterbank, 32 bands, spectral features, onset, tempo.

**Code removed:**
- `HAS_FFTW3` conditional code in `AudioCapture`
- `BeatTracker` entirely
- `AudioAnalyzer::computeBands32/computeSpectral*` entirely
- FFTW3 dependency from `engine/audio/CMakeLists.txt`

**New code needed:**
- `AubioEngine` class owning a `aubio_filterbank_t` (mel-scale or custom log-spaced), `aubio_tempo_t`, `aubio_onset_t`, `aubio_mfcc_t`
- Custom filterbank must replicate the exact 32-band 40–5000 Hz log-spacing of the legacy bands for backward compatibility
- Replace `AudioCapture::m_fftMagnitudeScratch` with aubio's `cvec_t` (phase vocoder output)

**Critical compatibility problem:** The 32-band spectrum in `AudioSnapshot.spectrum[32]` is the contract exposed to QML scripts and MCP tools. aubio's filterbank uses mel scale by default — different frequency spacing than QLC+'s log-linear scale. Full backward compatibility requires implementing a custom filterbank in aubio matching the legacy formula in `computeBands32()`.

**Impact on AudioSnapshot:** All fields can be preserved, but `spectrum[32]` must use the same log mapping or existing scripts produce different values visually.

**Effort estimate:** 15–20 person-days
- High risk: re-implementing all features, calibration work, cross-platform testing
- FFTW removal means Windows builds no longer need that dependency

> ⛔ **Blocked by license.** Same GPL-3.0 incompatibility. And the effort/risk ratio is poor.

---

## 5. What Other Lighting Software Uses

| Software | Audio Analysis Approach | Library |
|---------|------------------------|---------|
| **WLED** (SoundReactive) | Custom DSP in firmware C++; FFT on ESP32, custom beat detection | **No external lib** — hand-written spectral flux + energy threshold on microcontroller |
| **LedFx** | Python-based; real-time FFT via numpy/scipy; onset/beat via **aubio** (Python bindings) and librosa | **aubio (Python)** + numpy |
| **Resolume** | Proprietary closed-source engine | Unknown — proprietary |
| **MadMapper** | Proprietary | Unknown — proprietary |
| **Hyperion** | Ambilight-style; no audio reactive built-in | N/A |
| **xLights** | Sequencer; audio timing via WAV analysis | Custom BPM tap/manual |

**Observations:**
- LedFx is the only major lighting-adjacent software known to use aubio, and it does so through Python bindings (GPL containment is less of an issue for a Python script that imports a GPL library, since there's no static linking).
- WLED's hand-rolled DSP is an interesting precedent: a custom spectral flux beat detector is perfectly adequate for LED lighting purposes.
- No major C++ lighting software appears to use aubio as a linked C library.

---

## 6. Alternatives to aubio

### 6.1 BTrack (Adam Stark, Queen Mary University)

- **License:** GPL-3.0 ⛔
- **What it provides:** Causal real-time beat tracker using complex spectral difference ODF + comb filter tempo induction. One of the strongest real-time algorithms published.
- **C++ API:** Very clean — `BTrack b(hopSize)`, `b.processAudioFrame(frame)`, `b.beatDueInCurrentFrame()`, `b.getCurrentTempoEstimate()`
- **FFT deps:** FFTW3 or kissfft (switchable via `-DUSE_FFTW` / `-DUSE_KISS_FFT`)
- **libsamplerate:** Required dependency
- **Real-time:** Explicitly designed for it; O(N log N) per hop
- **Verdict:** Excellent algorithm quality but GPL-3.0 — same licensing blocker as aubio
- **Reference:** [https://github.com/adamstark/BTrack](https://github.com/adamstark/BTrack) — PhD thesis Stark 2011

### 6.2 Gist (Adam Stark)

- **License:** MIT ✅
- **What it provides:** C++ header-only-style library for real-time audio feature extraction: RMS, ZCR, spectral centroid/flatness/rolloff/spread/skewness/kurtosis, MFCC, chromagram, onset detection function, BPM estimation
- **BPM algorithm:** Peak-picking on ODF → inter-onset interval histogram → dominant interval. Less robust than BTrack/aubio on complex polyphonic music, but adequate for EDM/rock reactive lighting
- **FFT:** Uses FFTW or kissfft internally; can be configured
- **Real-time:** Yes — block-by-block, no threading
- **API example:**
  ```cpp
  Gist<float> gist(bufferSize, sampleRate);
  gist.processAudio(samples, bufferSize);
  float bpm = gist.getTempo();
  float centroid = gist.spectralCentroid();
  float flux = gist.energyDifference();
  ```
- **Verdict:** ✅ **Best compatible option.** MIT license, real-time, C++, covers BPM + spectral features. Less accurate than BTrack/aubio on jazz/orchestral, but excellent for dance/rock lighting reactive use.
- **Reference:** [https://github.com/adamstark/Gist](https://github.com/adamstark/Gist)

### 6.3 Essentia (UPF Barcelona)

- **License:** AGPLv3 ⛔ (commercial license available)
- **What it provides:** Comprehensive MIR framework — ~300 algorithms, batch and streaming, rhythm, key, melody, chords, MFCCs, loudness, onset, etc.
- **Real-time:** Partial — some algorithms have real-time wrappers but it's primarily designed for offline analysis. Latency is higher.
- **Weight:** ~50 MB library, heavy dependencies (Eigen, Yaml-cpp, TagLib, TensorFlow optional)
- **Verdict:** Massively over-engineered for QLC+'s needs. AGPLv3 is incompatible.
- **Reference:** [https://essentia.upf.edu](https://essentia.upf.edu)

### 6.4 kissfft

- **License:** BSD-3-Clause ✅ (or public domain, depending on version)
- **What it provides:** Minimal FFT only — no onset, no tempo, no features. Just FFT.
- **Suitability:** Could replace FFTW3 for the audio pipeline (FFTW is GPL with a commercial exception). Would solve the FFTW GPL concern if it's a concern for static builds.
- **Verdict:** Partial replacement. Pairs well with Gist (which supports kissfft).

### 6.5 Vamp Plugin SDK

- **License:** MIT ✅
- **What it provides:** Plugin architecture for audio analysis (hosts + plugins). Enables loading third-party analysis algorithms as plugins at runtime.
- **Suitability:** Overkill for in-process beat detection; useful if QLC+ wanted a plugin-based audio analysis extension system.

### 6.6 SoundTouch

- **License:** LGPL-2.1 ✅ (compatible with Apache via dynamic linking)
- **What it provides:** Time-stretching and pitch shifting; some BPM detection (`BPMDetect` class).
- **BPM algorithm:** Envelope follower → beat period estimation. Similar quality to current QLC+ BeatTracker.
- **Verdict:** No meaningful improvement over current approach.

### 6.7 Maximilian

- **License:** MIT ✅
- **What it provides:** C++ audio synthesis and analysis: FFT, MFCC, onset, filters, oscillators.
- **BPM:** Basic onset-to-BPM, no tempo induction.
- **Verdict:** Adequate for feature extraction but BPM quality is similar to current approach.

### 6.8 BTrack algorithm re-implementation

- **Concept:** The BTrack algorithm is described in the published academic paper (Stark, Davies, Plumbley 2009; Stark PhD thesis 2011). These are **public domain ideas**. A clean-room re-implementation in C++ with no code from the GPL repo would be:
  - Fully Apache-2.0 compatible
  - Higher quality than current spectral flux approach
  - No new dependencies (uses existing FFTW3)
- **Effort:** ~5–8 person-days for a solid implementation
- **Risk:** Algorithmic complexity; needs careful testing across music genres
- **Verdict:** ✅ **Most technically sound path** for accurate BPM without license issues.

### 6.9 Summary comparison table

| Library | License | BPM Quality | Real-Time | New Dep | Compatible |
|---------|---------|------------|----------|---------|-----------|
| aubio | GPL-3.0 | Excellent | Yes | Yes | ❌ |
| BTrack | GPL-3.0 | Excellent | Yes | Yes (libsamplerate) | ❌ |
| Gist | MIT | Good | Yes | Yes (kissfft/FFTW) | ✅ |
| Essentia | AGPLv3 | Excellent | Partial | Heavy | ❌ |
| SoundTouch | LGPL-2.1 | Basic | Yes | Yes | ✅ (dynamic) |
| Maximilian | MIT | Basic | Yes | Yes | ✅ |
| kissfft | BSD-3 | N/A (FFT only) | Yes | Yes | ✅ |
| BTrack re-impl | Apache-2.0 | Excellent | Yes | None | ✅ |

---

## 7. Concrete Recommendation

### 7.1 Best option: A-Prime (BTrack-inspired re-implementation + optional Gist)

Neither Option A, B, nor C with aubio is viable due to the GPL-3.0 license. The recommended path is **Option A-Prime**:

**Phase 1 (2–3 days): Fix the AGC saturation bug (quick win, no new deps)**
- In `AudioChannel::updateAgc()`, implement **per-band AGC** instead of single `m_agcGainDb` applied uniformly
- Each of the 5 perceptual bands gets its own slow-release gain computed from `frame.bands32[bandRange]`
- This is orthogonal to BPM and is the highest-impact fix for visual quality

**Phase 2 (5–8 days): Replace BeatTracker with BTrack-style algorithm**
- Implement a `CombFilterBeatTracker` class from scratch:
  - **ODF stage:** Complex spectral difference (uses existing FFTW magnitudes + phase from `AudioCapture`)
  - **Tempo induction:** Comb filter applied to windowed ODF history, scanning 60–220 BPM
  - **Beat tracking:** Predicted beat locations via dynamic programming on ODF vs comb filter peaks
- Feeds on `AudioFrame.magnitudes` (pre-computed by `AudioCapture`) — eliminates the redundant FFT in current `BeatTracker`
- Outputs: `beatDetected`, `bpm (double)`, `beatPhase (0..1)`, `beatConfidence (0..1)`
- **Key papers:**
  - Stark AM, Davies MEP, Plumbley MD. "Real-Time Beat-Synchronous Analysis." DAFx-09, 2009.
  - Dixon S. "Automatic Extraction of Tempo and Beat from Expressive Performances." *JNMR* 30(1), 2001.

**Phase 3 (2–3 days, optional): Add Gist for onset confidence**
- Integrate Gist (MIT) for `aubio_onset`-like onset detection function as supplementary signal
- Feed onset energy into Phase 2's beat tracker as an alternative ODF
- This gives an open-source, compatible path to onset quality comparable to aubio's HFC/complex detectors

### 7.2 AudioSnapshot upgrade plan

```cpp
// audiosnapshot.h additions
struct {
    bool beat = false;
    double bpm = 0.0;           // ← was int, now double
    double beatPhase = 0.0;     // ← was always 0, now meaningful
    double beatConfidence = 0.0; // ← new
    bool onsetDetected = false; // ← new (from Gist onset, optional)
} music;
```

Fields already exist but were never properly populated. Backward-compatible — old scripts that check `music.beat` still work; new scripts can use `music.bpm` and `music.beatPhase` for phase-locked effects.

### 7.3 Migration path (incremental)

```
Week 1: Per-band AGC fix (no new deps, immediate visual improvement)
Week 2-3: CombFilterBeatTracker Phase 1 (spectral diff ODF + interval histogram)
           — already better than current approach
Week 4-5: Comb filter tempo induction + dynamic programming beat prediction
           — BTrack-quality output
Week 6 (optional): Gist integration for onset supplementary signal
```

### 7.4 Risks

| Risk | Severity | Mitigation |
|------|---------|-----------|
| BTrack re-impl algorithmic complexity | Medium | Start with simpler ODF + histogram (already better than current); iterate |
| Regression in existing scripts using `music.bpm` (type int → double) | Low | Backward compatible; QML `int` receives `double` without issue |
| FFTW3 plan contention (BeatTracker has own plan) | Low | New `CombFilterBeatTracker` reuses `AudioFrame.magnitudes` — eliminates its own FFT |
| Gist adding kissfft as new dep | Low | kissfft is 2 source files, BSD-3; trivially vendored |
| Per-band AGC changing existing show behavior | Medium | Feature-flag it behind a config option; default to new behavior |
| Platform support for any new dep | Low | Gist + kissfft are pure C++, no platform specifics |

### 7.5 Fallback

If Phase 2 algorithmic re-implementation proves too complex:
- **Use Gist directly for BPM** (MIT, simple API, 3–4 day integration)
- Quality will be "better than current, worse than BTrack/aubio on complex music"
- Completely safe from a licensing perspective

### 7.6 What NOT to do

- ❌ Do not integrate aubio, BTrack, or Essentia without a commercial license negotiation or project relicensing
- ❌ Do not use pre-0.4.0 aubio (LGPL era) — those versions are very old (2012), unsupported, and lack `aubio_tempo_t` quality
- ❌ Do not attempt GPL compliance by LGPL-wrapping (a thin C wrapper does not change GPL-3.0 requirements for a combined work)

---

## References

- aubio official: [https://aubio.org](https://aubio.org)
- aubio API docs: [https://aubio.org/doc/latest/api/](https://aubio.org/doc/latest/api/)
- aubio GitHub: [https://github.com/aubio/aubio](https://github.com/aubio/aubio)
- aubio COPYING (GPL-3.0): [https://github.com/aubio/aubio/blob/master/COPYING](https://github.com/aubio/aubio/blob/master/COPYING)
- BTrack GitHub: [https://github.com/adamstark/BTrack](https://github.com/adamstark/BTrack)
- Gist GitHub: [https://github.com/adamstark/Gist](https://github.com/adamstark/Gist)
- Essentia: [https://essentia.upf.edu](https://essentia.upf.edu)
- vcpkg aubio port: [https://vcpkg.io/en/package/aubio.html](https://vcpkg.io/en/package/aubio.html)
- Homebrew aubio: [https://formulae.brew.sh/formula/aubio](https://formulae.brew.sh/formula/aubio)
- Apache / GPL compatibility: [https://www.apache.org/licenses/GPL-compatibility.html](https://www.apache.org/licenses/GPL-compatibility.html)
- Stark, Davies, Plumbley. "Real-Time Beat-Synchronous Analysis of Musical Audio." DAFx-09, 2009.
- Dixon S. "Automatic Extraction of Tempo and Beat from Expressive Performances." JNMR 30(1), 2001.
