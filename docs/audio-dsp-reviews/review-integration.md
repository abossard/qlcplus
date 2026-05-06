# Integration Wiring Review (Opus 4.7, fresh eyes)

Scope: `audiocapture.{h,cpp}`, `rgbscriptv4.{h,cpp}`, `rgbmatrix.{h,cpp}`,
`doc.{h,cpp}`, `qmlui/virtualconsole/vcaudiotriggers.{h,cpp}` against the
contracts in `engine/audio/src/audioframe.h`, `audiosnapshot.h`,
`audioanalyzer.h`, `audiochannel.h`, `engine/src/audioprofile.{h,cpp}`.

---

## Blocking Issues

### B1. AudioAnalyzer is never instantiated or attached in production

**Impact: the entire enriched pipeline is dead code at runtime.**

- `AudioCapture::setAnalyzer(...)` is defined at `audiocapture.cpp:135` but is
  **never called** anywhere except possibly tests. A repo-wide grep finds
  zero call sites in `engine/src/`, `engine/audio/src/`, `qmlui/`.
- The `m_analyzer` field stays `nullptr`, so the guard at
  `audiocapture.cpp:396` (`if (m_analyzer) m_analyzer->processFrame(frame);`)
  is always false. The carefully constructed `AudioFrame` is thrown away.
- Consequence: `AudioChannel::update()` never runs; every snapshot returned
  by `channel->snapshot()` is the default-constructed one.

**Severity: Blocking.** Without this wiring, the enriched JS object
(`bands`, `triggers`, `volume`, `music`, `features`) and the new VC
perceptual band properties (`subPower`, `bassPower`, …) are permanently
zero/silence-defaults regardless of input.

**Recommended fix:** decide where the analyzer lives. Most natural place is
`Doc` — make `Doc::audioInputCapture()` (or an `audioAnalyzer()` accessor)
lazily construct an `AudioAnalyzer`, attach it to the capture via
`m_inputCapture->setAnalyzer(m_analyzer.get())`, and tear it down inside
`Doc::destroyAudioCapture()` (before destroying the capture). Document the
ownership model: analyzer is created with the capture, destroyed before it.

---

### B2. AudioProfile::bindAnalyzer() is never called

**Impact: `profile->channel()` is always nullptr; both consumers degrade.**

- `AudioProfile::bindAnalyzer()` is defined at `audioprofile.cpp:118` but
  has zero call sites in non-test code.
- `m_channel` therefore stays nullptr forever:
  - In `RGBScript::resolveAudioProfile()` (`rgbscriptv4.cpp:660`):
    `channel = profile->channel()` → NULL → `m_audioChannel = NULL`.
    `buildAudioDataObject()` then early-returns at line 750-751 *before*
    setting `version=2`, `bands`, `triggers`, `volume`, `music`, `features`,
    `audioDtMs`, `brightnessFloor`, `consumerDtMs`. JS scripts only see the
    legacy `spectrum/volume/beat/bpm/maxMagnitude` shape — the v2 contract
    never ships.
  - In `VCAudioTriggers::updateAudioProfileSnapshotPowers()`
    (`vcaudiotriggers.cpp:374-394`): channel is always null →
    `m_subPower`/`m_bassPower`/etc. permanently 0. The new perceptual band
    Q_PROPERTYs never carry a non-zero value.

**Severity: Blocking.** Same root cause as B1, but the *profile-side*
binding is also missing even after B1 is fixed. After `Doc` owns an
analyzer, every profile registered via `addAudioProfile()` (and the one
created in `ensureDefaultAudioProfile()`) must have `bindAnalyzer(m_analyzer)`
called; every `removeAudioProfile()` and `clearContents()` must call
`releaseAnalyzer()` *before* destroying the analyzer/capture.

**Recommended fix:** in `Doc::addAudioProfile()`, after insert, if
`m_audioAnalyzer` exists call `profile->bindAnalyzer(m_audioAnalyzer)`. In
`Doc::removeAudioProfile()` and the `clearContents()` profile-loop, call
`releaseAnalyzer()` *first* (i.e. before profile delete). Reverse the order
in `clearContents()`: release every profile's analyzer binding *before*
`destroyAudioCapture()` so channels are cleanly returned to the analyzer
before it dies.

---

### B3. Lifetime of `RGBScript::m_audioChannel` raw pointer is unsafe

**Impact: latent use-after-free under realistic user actions.**

Once B1+B2 are fixed and channels actually exist, the cached raw pointer
becomes a hazard:

1. **`Doc::removeAudioProfile(id)`** (called from any UI/MCP path):
   `m_audioProfiles.take(id); delete profile;` runs `~AudioProfile()` →
   `releaseAnalyzer()` → `m_analyzer->destroyChannel(m_channel)`. The
   `RGBScript` running on the JS thread still holds `m_audioChannel`
   pointing at the freed channel; the next `buildAudioDataObject()` call at
   `rgbscriptv4.cpp:753` (`channel->snapshot()`) is UB.
2. **`Doc::clearContents()`** has a worse ordering bug. Today it calls
   `destroyAudioCapture()` *first* (`doc.cpp:132`), then deletes profiles
   (`doc.cpp:154-160`). If the analyzer lives inside / next to the capture
   (per B1's fix), the analyzer is gone before any profile's
   `~AudioProfile() → releaseAnalyzer()` runs — and that destructor will
   call `m_analyzer->destroyChannel(...)` on a dangling analyzer pointer.
3. **VCAudioTriggers** has a similar but shorter-lived window: it resolves
   the profile and reads `channel->snapshot()` synchronously each
   `slotSpectrumDataChanged` tick. As long as both run on the main thread
   it's "merely" a bug that the channel can disappear between
   `resolvedAudioProfile()` and the snapshot read; cross-thread, it would
   be a use-after-free too.

**Severity: Blocking.** The integration must define and enforce a
lifetime story for the cached channel pointer before it's safe to ship.

**Recommended fix (pick one):**
- **Easiest:** make the cache cheap and lookup-on-each-use. Replace
  `m_audioChannel` with `m_audioProfileId`; in `buildAudioDataObject()`,
  resolve the profile via `doc()->audioProfile(id)` each call (under a
  mutex held by `Doc` if profiles can mutate cross-thread), then read
  `profile->channel()`. Keeps the hot path single-pointer-deref but makes
  removal safe because `audioProfile()` returns null for removed IDs.
- **Or:** wrap `AudioChannel` in a `QSharedPointer<AudioChannel>` owned by
  the analyzer, hand a `QWeakPointer` to consumers, lock+check on each use.
- **Either way:** fix `Doc::clearContents()` ordering — release every
  profile's analyzer binding (or delete profiles) *before*
  `destroyAudioCapture()`. The current ordering is wrong even with the raw
  pointer left in place.

---

## Non-Blocking Issues

### N1. Hot-swap of `RGBMatrix::audioProfileId` is not honored

`RGBMatrix::audioProfileIdChanged` is emitted (`rgbmatrix.cpp:252`) but
`RGBScript::setupAudioCapture()` is the only place `resolveAudioProfile()`
runs, and it isn't connected to that signal. Editing the audio profile of a
running matrix has no effect until the function is fully restarted.

**Recommended:** in `setupAudioCapture()`, after `m_audioInput` is bound,
also `connect(matrix, &RGBMatrix::audioProfileIdChanged, ...,
[this]{ QMutexLocker l(&m_audioMutex); resolveAudioProfile(); })`. Tie the
connection lifetime to `m_audioDataConn`-style storage so `teardown` clears
it.

### N2. `requestedProfileId` dead code in `resolveAudioProfile()`

`rgbscriptv4.cpp:653-654` reads `matrix->audioProfileId()` then does
`Q_UNUSED(...)`. The "explicit → default → first → anonymous" chain
described in the task lives entirely inside
`Doc::audioProfileForFunction()`, which already reads
`matrix->audioProfileId()`. The dead local just confuses future readers.
Either delete it or, if you want the script to log "explicit=N
resolved=M", actually use it for logging.

### N3. Missing "anonymous" fallback step

The fallback chain in the task spec promises "explicit → default → first →
anonymous". `audioProfileForFunction` does explicit → default; `default`
itself falls back to the first profile (`doc.cpp:1070-1071`); and
`ensureDefaultAudioProfile()` creates a fresh anonymous one if the map is
empty. So all four steps exist, but they're split across two helpers in a
way that's easy to break. If the design really requires an "anonymous,
not-persisted" channel for a script when *both* `audioProfileForFunction`
and `ensureDefaultAudioProfile` return non-null but the user wants
isolation, that case isn't handled. Worth confirming with the design doc.

### N4. `setupAudioCapture()` early-return when capture is already bound

`rgbscriptv4.cpp:617-618`: `if (m_audioInput != NULL && capture.data() ==
m_audioInput) return;` — but this skips `resolveAudioProfile()`. If the
function is restarted with the same capture but a changed profile (or
after Doc added a default profile lazily), the channel pointer is stale.
Move `resolveAudioProfile()` before the early-return, or always call it.

### N5. AudioCapture pre-fills scalar features with silence-sane defaults

`audiocapture.cpp:386-393` writes `rmsDb=-96`, `peakDb=-96`,
`crestFactor=1.0`, `spectralFlux=0`, etc. The `AudioFrame` contract says
the analyzer fills these. If the analyzer is ever skipped (B1 today; or a
future "raw-only" path), consumers reading `frame.rmsDb` get a misleading
"silence" reading even on loud input. Either compute true `rmsDb`/`peakDb`
in capture (cheap — `rms` and `peak` are already known), or document
clearly that these fields are *required* analyzer outputs and unsafe to
read upstream of the analyzer.

### N6. `setChannelConfig()` racing the analyzer thread

`AudioProfile::setChannelConfig()` (`audioprofile.cpp:113-114`) calls
`m_channel->updateConfig(m_config)` on the main thread while the analyzer
thread reads/writes channel state. `AudioChannel` declares a `m_mutex`
plus `m_pendingConfig`/`m_hasPendingConfig` (`audiochannel.h:46-48`), so
this is presumably protected — but I didn't read `audiochannel.cpp` for
this review. Worth a 5-minute confirmation that `updateConfig` only
touches the pending fields under the mutex and the analyzer thread reads
them under the same mutex.

---

## Suggestions

### S1. `m_loggedAudioProfileId` vs the JS thread

`resolveAudioProfile()` is called from `setupAudioCapture()`, which is
itself invoked from `RGBScript::rgbMap(...)` / on the JS thread. The
`qDebug()` log gate at `rgbscriptv4.cpp:666-672` is fine for one matrix,
but if multiple matrices share the same profile they'll each log once —
which is probably what you want, just confirm.

### S2. AudioFrame designated-initializer would be safer

The aggregate-initializer at `audiocapture.cpp:372-395` lists 22 values
positionally. If `AudioFrame` ever gains a field, this silently shifts
every subsequent value. C++20 designated initializers (`{.frameIndex=...,
.hostTimeNs=...}`) would catch any reordering at compile time and make
the contract obvious. Same applies to any future test fixtures.

### S3. Minor: spectrum bin scratch on silent frames

`audiocapture.cpp:369` zeroes `m_fftMagnitudeScratch` only on the
non-FFTW build. In the FFTW build, on a silent frame, `fft[k]` may carry
microscopic floor noise from rounding; analyzer's silence path is supposed
to ignore content when `frame.silent==true`, so this is fine — but worth
asserting it's intentional.

---

## What Looks Solid

- `AudioFrame` aggregate field order matches the header definition exactly
  (22 fields, types align). Frame counter is incremented unconditionally
  (`audiocapture.cpp:274`). Silent-frame early-return preserves legacy
  behavior. Frame is built and `processFrame` is called *before* the
  silent return path, so the analyzer sees every frame including silent
  ones — matches the contract.
- `buildAudioDataObject()` field-name mapping into JS is correct: every
  property name spells out as documented (`sub`, `bass`, `lowMid`, `mid`,
  `high`, `low`, plus the trigger/volume/music/features sub-objects). No
  typos. The `volume` object is built as a Number wrapper preserving
  legacy numeric coercion and adding `.raw/.smoothed/.agc/.normalized/.legacy`,
  which is a nice backward-compat trick.
- Beat consumption (`audioBeat = false; ` after read at `rgbscriptv4.cpp:717`)
  preserves the "consumed-on-read" semantics expected by legacy scripts.
- Doc XML round-trip: `RGBMatrix` reads/writes `AudioProfileID`
  (`rgbmatrix.cpp:521-523, 633-634`); `VCAudioTriggers` reads/writes it
  (`vcaudiotriggers.cpp:1140-1141, 1197`); `Doc` registers profiles from XML
  (`doc.cpp:1406-1411`). The ID-allocation in `ensureDefaultAudioProfile()`
  (`doc.cpp:1081-1083`) safely avoids `invalidId()` collisions.
- `VCAudioTriggers` dual-path: legacy `lowsPower/midsPower/highsPower`
  computed from raw spectrum (lines 700-712) is unchanged; new perceptual
  band properties read from the snapshot and gracefully degrade to 0 when
  channel is null (lines 378-386). Old QML keeps working; new QML reads
  zeros until B1+B2 land. No null-deref risk in this file.
- `rgbmatrix.cpp` Q_PROPERTY/notify wiring for `audioProfileId` is correct.

---

## Summary

The wiring inside each individual file is mostly correct, but the
**top-level glue that creates the AudioAnalyzer and binds it to the
capture and to every profile is missing entirely** (B1 + B2). The
integration as committed compiles, runs, and silently produces zero output
on every new code path. Closely related: the cached `m_audioChannel` raw
pointer (B3) needs a defined lifetime story before this is safe under
profile removal or `clearContents()`.

If B1, B2, and B3 are addressed, the rest of the issues are quality
polish.
