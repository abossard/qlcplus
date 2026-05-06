# Rubber-duck review — C++ audio engine (Phase 1 implementation)

Reviewer: Opus 4.7 (fresh eyes), reviewing GPT-5.5's implementation against
`docs/audio-dsp-reviews/p05-contracts.md`.

Files reviewed:
- `engine/audio/src/audioframe.h`
- `engine/audio/src/audiochannelconfig.{h,cpp}`
- `engine/audio/src/audioanalyzer.{h,cpp}`
- `engine/audio/src/audiochannel.{h,cpp}`
- `engine/audio/src/audiosnapshot.h`
- `engine/src/audioprofile.{h,cpp}`
- Cross-checked with `engine/audio/src/audiocapture.cpp`

---

## Overall assessment

**Not production-ready yet.** Architecture, threading, memory ownership and the
no-allocation-per-frame story are sound. However there are **four formula
deviations from the contracts doc** that will cause the Phase-1 unit tests in
§6 of `p05-contracts.md` to fail outright, plus one band-edge off-by-one that
breaks the legacy-compat promise. None are deep design problems; all are
localised numerical fixes in `audioanalyzer.cpp` / `audiochannel.cpp`.

Once the bugs below are fixed, this passes review.

---

## Bugs (must fix — fail tests / break contract)

### BUG-1 — `spectralFlatness` on silent frames is `0.0`, contract demands `1.0`
**Severity: bug**
**File:** `engine/audio/src/audioanalyzer.cpp:182`

```cpp
if (frame.silent)
{
    frame.crestFactor = 1.0;
    frame.spectralFlux = 0.0;
    frame.spectralCentroidHz = 0.0;
    frame.spectralRolloffHz = 0.0;
    frame.spectralFlatness = 0.0;          // <-- WRONG
    ...
}
```
Contract `p05-contracts.md` §4 (zero-handling column): `spectralFlatness = 1.0`
on silence (silent ≈ uniform near-zero ≈ flat). Test §6.1 expects `1.0`
(tolerance `< 1e-6`). `AudioSnapshot::features.flatness` already defaults to
`1.0` (consistent with the contract).

**Fix:** assign `frame.spectralFlatness = 1.0;` in the silent branch.

---

### BUG-2 — `spectralFlux` normalised by `binCount` instead of `Σ |M_k[t-1]|`
**Severity: bug**
**File:** `engine/audio/src/audioanalyzer.cpp:243-264`

Contract §4:
```
spectralFlux = Σ_k max(0, |M_k[t]| - |M_k[t-1]|) / Σ_k |M_k[t-1]|
```
Implementation:
```cpp
return flux / double(binCount);   // line 263
```
Dividing by bin count instead of by previous-frame magnitude sum produces
values that are orders of magnitude smaller and have no physical meaning as a
"normalised onset" measure. Test §6.4 (impulse) expects flux to *spike* on
sudden energy rise — the current normalisation will mute that to near-zero.

**Fix:**
```cpp
double prevSum = 0.0;
for (int bin = 0; bin < m_prevBinCount; ++bin) prevSum += m_prevMagnitudes[bin];
return (prevSum > kMinLinear) ? (flux / prevSum) : 0.0;
```
Also restrict the bin loop to the analysis band — see BUG-3.

---

### BUG-3 — `spectralFlux` iterates all bins; contract restricts to 40–5000 Hz
**Severity: bug**
**File:** `engine/audio/src/audioanalyzer.cpp:252-256`

Contract §4: flux is summed "over bins covering 40..5000 Hz". The
centroid/rolloff/flatness all gate by `isAnalysisFrequency()`; flux does not.
At default settings this means DC, 0–40 Hz rumble and 5000+ Hz hiss leak into
the onset metric.

**Fix:** wrap the diff loop with the same `isAnalysisFrequency(binFrequency(frame, bin))`
gate already used elsewhere in this file.

---

### BUG-4 — `noiseFloorDb` adaptive law is wrong, frame-rate dependent, wrong init
**Severity: bug**
**File:** `engine/audio/src/audioanalyzer.{h:58, cpp:347-352}`

Contract §4:
```
nf[t] = min(rmsDb[t], nf[t-1] + r·dt),    r = +6 dB/s,    init nf[0] = -60 dB
```
i.e. snap-down instantly, **linear** rise capped at +6 dB/s, time-step aware.

Implementation:
```cpp
m_noiseFloorDb = -96.0;                    // wrong init: contract says -60
...
if (rmsDb < m_noiseFloorDb)
    m_noiseFloorDb = rmsDb;                // OK (snap-down)
else
    m_noiseFloorDb += 0.01 * (rmsDb - m_noiseFloorDb);   // exponential, no dt
```
Two problems:
1. The rise is a fixed-α one-pole filter, not a `+r·dt` clamp. It is
   **frame-rate dependent**: at 21.5 fps the effective time constant is
   ≈ 4.6 s; if the user picks a smaller FFT the floor will rise much faster.
2. `dt` is computed (`computeAudioDtMs`) but never threaded into the noise-floor
   update.
3. Init `-96` instead of `-60` will pass the §6.1 test ("reaches ≤ -90 within
   200 frames") trivially without exercising the descent code, hiding bugs.

**Fix:**
```cpp
void AudioAnalyzer::updateNoiseFloor(double rmsDb, double dtMs)
{
    if (rmsDb < m_noiseFloorDb)
        m_noiseFloorDb = rmsDb;
    else
        m_noiseFloorDb = std::min(rmsDb,
                                  m_noiseFloorDb + 6.0 * (dtMs / 1000.0));
}
```
and initialise `m_noiseFloorDb = -60.0;`. Pass `audioDtMs` into the call
(currently computed *after* `computeSharedFeatures` runs — re-order so it is
available).

---

### BUG-5 — Default 5-band layout indices drift from contract / legacy
**Severity: bug**
**File:** `engine/audio/src/audiochannel.cpp:36-43`

```cpp
int bandIndexForFrequency(double frequency)
{
    if (frequency <= kMinFrequency) return 0;
    const double index = 32.0 * std::log(frequency/kMinFrequency) / std::log(kFrequencyRange);
    return std::clamp(int(std::ceil(index)), 0, 32);
}
```
Using `ceil` for **every** boundary breaks the legacy compatibility hinge in
contract §2.2: `Bass` upper at 250 Hz must equal `legacy lowCutBin(32) = 12`,
but `ceil(32·ln(250/40)/ln(125)) = ceil(12.146) = 13`. Numerical check:

| edge   | raw index | ceil (code) | contract §2.2 |
|-------:|----------:|------------:|--------------:|
|   60 Hz|    2.687  |          3  |             3 |
|  250 Hz|   12.146  |       **13**|         **12** |
|  500 Hz|   16.739  |         17  |            17 |
| 2000 Hz|   25.927  |         26  |            26 |
| 5000 Hz|   32.000  |         32  |            32 |

So with default `BandLayout`, the code splits the 32-band spectrum as
`[0,3) [3,13) [13,17) [17,26) [26,32)` — bin 12 (= legacy lowCutBin) ends up
in `Bass` instead of `LowMid`. RGBScripts that previously reproduced
`magnitudeSum[0..lowCutBin(32))` via the new `low` alias will get a slightly
different result. Also the `[13,17)` LowMid band is one bin narrower than the
contract `[12,17)`.

**Fix:** match legacy bit-for-bit using truncation (= `floor` for non-negative
indices), which is what `lowCutBin/highCutBin` did:
```cpp
return std::clamp(int(index), 0, 32);   // truncation, matches legacy
```
or, if the analyzer is being more strict about edge semantics, use floor for
the lower edge and ceil for the upper edge as the §2.4 rule states (the call
sites in `updateEnvelopes` pass each `bandLayout.*MaxHz` as an upper edge — so
floor is wrong there too; you need separate `lowerEdge()` / `upperEdge()`
helpers, with the latter using `ceil` for arbitrary user values but truncation
for the four legacy-default values to satisfy §2.2). Simplest workable
solution: keep `ceil` but special-case `floor` for boundaries that fall on a
near-integer (e.g. fractional part `< 0.25`). Cleanest: spec says "indices
derived" — derive once at `setChannelConfig` time and store the integer edges,
so the floor/ceil choice is auditable in tests.

---

## Risks (silent contract drift / fragile)

### RISK-1 — `AudioFrame::samples` is `int16_t*`, contract specifies normalised `double`
**Severity: risk**
**Files:** `engine/audio/src/audioframe.h:71`, vs `p05-contracts.md` §1.

The contract canonical struct uses `std::vector<double>` samples in
`[-1, +1]`; the implementation uses a non-owning `const int16_t*` in
`[-32768, +32767]`. The implementation header documents this, and consumers
that already work in int16 (legacy fillBandsData chain) are happy. But test
fixtures and any new DSP code reading `frame.samples` must remember to divide
by 32768.0. If a consumer assumes the contract type, it gets values
~32768× too large.

**Fix:** either (a) add a normalised-double mirror buffer
(`const double *samplesNormalized`) populated once by capture/analyzer (8 KB
extra, called once per hop, fits in cache), or (b) update the contracts doc
and the fixture-spec in `p05-contracts.md` §1 to declare int16 the canonical
type and remove the `std::vector<double>` example. Pick one — don't leave the
two specs disagreeing.

---

### RISK-2 — Spectral-flux scratch buffer fixed at construction, won't grow
**Severity: risk**
**File:** `engine/audio/src/audioanalyzer.cpp:73-77, 257-261`

`m_prevMagnitudes` is sized to `(AUDIO_DEFAULT_BUFFER_SIZE/2)+1 = 1025` at
construction. If a user changes FFT size to 4096 at runtime, lines 257-258:
```cpp
if (binCount <= m_prevMagnitudeCapacity)
    std::copy(... binCount ..., m_prevMagnitudes);
```
silently *skip* the update for any frame larger than 1025 bins — the previous
buffer freezes at whatever it last held, and `spectralFlux` permanently
returns the diff against a stale frame. No log, no error.

**Fix:** when `binCount > m_prevMagnitudeCapacity`, reallocate (this is rare
— FFT size changes only on settings change, not in the audio hot path):
```cpp
if (binCount > m_prevMagnitudeCapacity) {
    delete[] m_prevMagnitudes;
    m_prevMagnitudeCapacity = binCount;
    m_prevMagnitudes = new double[m_prevMagnitudeCapacity]();
    m_prevBinCount = 0;
}
```

---

### RISK-3 — Analyzer destruction deletes channels still referenced by `AudioProfile`
**Severity: risk**
**Files:** `engine/audio/src/audioanalyzer.cpp:79-86`, `engine/src/audioprofile.cpp:67-70`

`AudioAnalyzer::~AudioAnalyzer()` iterates and `delete`s every `AudioChannel*`
it owns. `AudioProfile` holds raw pointers to channels obtained via
`createChannel()`. If an `AudioAnalyzer` is destroyed before all bound
`AudioProfile`s have called `releaseAnalyzer()`, every profile is left holding
a dangling `m_channel`. There is no `QPointer`/back-reference, no observer.

**Mitigation in current code:** `~AudioProfile()` calls `releaseAnalyzer()`,
so as long as profile lifetime ⊆ analyzer lifetime, things work. But that
ordering is undocumented, and `Doc::~Doc()` deletes children in QObject
parent-child order which can be surprising.

**Fix (minimal):** add a `Q_ASSERT(m_channels.isEmpty())` to the analyzer
destructor — a developer-only tripwire — *or* register channels with their
owning profile via signal/slot so destruction propagates safely.

---

### RISK-4 — `defaultChannel()` allocates under the same mutex used by the hot path
**Severity: risk (low)**
**File:** `engine/audio/src/audioanalyzer.cpp:144-154`

`defaultChannel()` calls `new AudioChannel(...)` while holding
`m_channelsMutex`, which `processFrame()` also holds for the entire channel
update loop. If a UI thread triggers `defaultChannel()` for the first time
while the capture thread is mid-frame, the UI blocks until the frame finishes
— not a correctness bug, but a 1 ms+ stall on the UI thread for a path
labelled "default lazy init". Move the `new` outside the lock:
```cpp
AudioChannel *fresh = new AudioChannel(AudioChannelConfig::defaults());
QMutexLocker locker(&m_channelsMutex);
if (!m_defaultChannel) { m_defaultChannel = fresh; m_channels.append(fresh); fresh = nullptr; }
locker.unlock();
delete fresh;
return m_defaultChannel;
```

---

## Nits (won't cause failures, but worth a follow-up)

- **NIT-1** (`audiochannel.cpp:33-34`): `kFrequencyRange = 125.0` is hard-coded
  but is really `SPECTRUM_MAX_FREQUENCY / SPECTRUM_MIN_FREQUENCY`. If anyone
  ever changes either constant in `audiocapture.h`, the band layout silently
  drifts. Derive it.
- **NIT-2** (`audioanalyzer.cpp:183`): on silent frames the code calls
  `computeSpectralFlux(frame)` purely for its prev-buffer side-effect, then
  discards the return. A short `// keep prev-magnitudes in sync` comment
  prevents future "dead code" deletion.
- **NIT-3** (`audiochannel.cpp:42`): `std::clamp(int(std::ceil(index)), 0, 32)`
  upper bound `32` is one past the array end; this is intentional (used as
  half-open end), but consider naming a constant `kBands32End = 32` to make
  the intent obvious where it is consumed (`updateEnvelopes` line 156 already
  treats it that way — fine — but a reader has to verify).
- **NIT-4** (`audioprofile.cpp:113-114`): `setChannelConfig` calls
  `m_channel->updateConfig` *before* emitting `configChanged()`. That's
  correct, but `updateConfig` only stages a pending swap — if a UI listener
  reacts to `configChanged()` by reading `channel()->config()`, it gets the
  *new* config (because `config()` returns pending-if-set), so this happens
  to behave. Document the pending-swap semantics on the API.
- **NIT-5** (`audioframe.h:69`): comment claims "pre-window, post-DC-removal"
  but `m_audioMixdown` (what `samples` points to) is *neither* DC-removed nor
  windowed — it is the raw int16 mixdown. The DC-removed/normalised data lives
  in `m_fftInputBuffer` which is not exposed. The `rms` field *is* computed
  post-DC-removal. The comment as written conflates buffer state and scalar
  state.

---

## Things in the contract that are NOT implemented

Tracking the contracts doc end-to-end:

- **§3 / `audiobuildup.js` extras** (`buildEnter`, `peak` thresholds): the
  contract calls for a `TriggerConfig::extra` map keyed by name to preserve
  per-script overrides. `TriggerConfig` (in `audiochannelconfig.h`) has no
  such map. If you don't intend to support `audiobuildup.js`'s second
  threshold pair in Phase 1, drop the row from the contract; otherwise add
  `QHash<QString,double> extra;`.
- **§3 `inputGainDb` UI mirror**: contract says the linear gain is
  authoritative but suggests storing `inputGainDb = 20·log10(linear)` for UI
  display. Not present. Optional, but flag it as a deliberate decision.
- **`AudioChannelConfig::fromLegacySliders` releaseMs**: contract §3 says
  "when no script default exists, use `releaseMs = 4·attackMs`" — implemented
  correctly at `audiochannelconfig.cpp:60`. ✓
- **AudioProfile XML schema**: matches the design doc fields. The `Version`
  attribute is written on save (line 280) but never read or validated on load
  (loadXML never inspects `KXMLQLCAudioProfileVersion`). Add a
  forward-compat check: warn (don't fail) if `Version > 1`.

---

## Things that look correct (sanity-check)

- ✅ `bands32` formula `Σ|M_k|/(bandWidth · 2π)` matches legacy `fillBandsData`
  exactly (`audioanalyzer.cpp:239` vs `audiocapture.cpp:260`).
- ✅ `rmsDb`/`peakDb` clamp via `std::max(20·log10(max(v,1e-10)), -96)`
  (line 56-58) matches `p05-contracts.md` §4.
- ✅ Triggers do not fire on silent frames (`audiochannel.cpp:211`).
- ✅ Cooldown advances regardless of silence (line 193). Matches §5.2.
- ✅ AGC clamps target gain to `[0, maxGainDb]` and snaps down instantly,
  releases via time-constant (correct AGC semantics).
- ✅ Snapshot publication is atomic under mutex (`audiochannel.cpp:282-283`),
  reader copies under same mutex (lines 99-101). No torn reads.
- ✅ `processFrame` holds `m_channelsMutex` for the update loop, so
  `destroyChannel()` cannot delete a channel mid-update.
- ✅ Zero allocations in the per-frame hot path: scratch buffers are
  pre-allocated, `AudioFrame` is stack-built in capture, channel state is
  fixed-size arrays. ✓
- ✅ AudioFrame pointer-lifetime contract is correctly documented and
  respected: snapshot copies into fixed-size arrays before publishing
  (`audiochannel.cpp:223-283`), no consumer retains pointers.

---

## Suggested fix order

1. BUG-1 (one-line, unblocks test §6.1).
2. BUG-4 (couples noise-floor with `dt`; small refactor).
3. BUG-2 + BUG-3 (do together — both touch the flux loop).
4. BUG-5 (band-edge legacy compat).
5. RISK-2 (defensive resize on FFT-size change).
6. Everything else can defer to a follow-up PR.

After 1–4, the synthetic-input vectors in `p05-contracts.md` §6 should pass.
