# Architecture Review — Audio DSP Modernization

## Executive summary

The implementation is **not architecturally coherent yet**. The C++ DSP building blocks exist, and several model/XML pieces are present, but the runtime pipeline is not actually connected end-to-end:

- `AudioAnalyzer` exists, but no production code appears to install it on `AudioCapture`.
- `AudioProfile::bindAnalyzer()` exists, but no production code calls it.
- `RGBScript` resolves an `AudioProfile`, but then sees `profile->channel() == nullptr`, so scripts fall back to the legacy audio object.
- `VCAudioTrigger` exposes profile monitor properties, but they read zeros unless a profile channel is somehow bound externally.
- QML panels are primarily read-only placeholders, not the “Audio Control Center” profile editor described by DD2.
- Legacy per-script sliders are still active UI properties, not removed with one-shot warnings.
- Old slider values are not converted to generated profiles on XML load.
- Golden tests are mostly “sensible output”/smoke tests and source-inspection tests, not true old-vs-new behavioral comparisons.

There are some solid foundations: `AudioProfile` is in the document model, `RGBMatrix.audioProfileId` is VC-independent, legacy per-bar `VCAudioTrigger` behavior still exists, and `ledfx_compat.js` was removed. But the implementation currently behaves much closer to “legacy audio path with new scaffolding” than to the planned modern DSP architecture.

---

## Per-DD assessment

| DD | Assessment | Evidence / notes |
|---|---|---|
| DD1 — Audio Profiles in Doc model | **Partially implemented** | `Doc` stores `AudioProfile` objects and loads/saves them (`doc.cpp:1406-1418`, `doc.cpp:1500-1507`). `defaultAudioProfile()` falls back to first profile (`doc.cpp:1060-1073`), and `ensureDefaultAudioProfile()` creates “Default Audio” (`doc.cpp:1076-1097`). However, profiles are not bound to analyzer channels in production: `AudioProfile::bindAnalyzer()` exists (`audioprofile.cpp:118-127`), but no caller was found. Without channels, profiles are configuration-only and not runtime DSP owners. Anonymous fallback is implemented in `AudioAnalyzer::defaultChannel()` (`audioanalyzer.cpp:144-154`) but not used by `RGBScript`; `RGBScript` creates a doc default profile instead, then reads `profile->channel()` which is null (`rgbscriptv4.cpp:656-663`). |
| DD2 — VCAudioTrigger as editor/monitor | **Mostly missing** | VCAudioTrigger has `audioProfileId` and monitor properties (`vcaudiotriggers.cpp:285-309`, `360-394`). The QML properties panel explicitly uses placeholder values and TODOs for envelope, AGC, trigger, and spectral data (`VCAudioTriggersProperties.qml:60-64`). There are no visible setters/bindings for profile config editing in the QML search results. This is a read-only/placeholder monitor, not a profile editor. |
| DD3 — Core DSP in C++ AudioAnalyzer | **Partially implemented but not wired** | `AudioAnalyzer` computes shared features and updates channels (`audioanalyzer.cpp:88-119`). `AudioCapture` has `setAnalyzer()` and calls `m_analyzer->processFrame(frame)` when present (`audiocapture.cpp:135-139`, `396-397`). But no production caller installs an analyzer, so runtime capture does not feed the new DSP pipeline. |
| DD4 — Analyzer receives internal AudioFrame | **Partially implemented** | `AudioCapture::processData()` constructs `AudioFrame` every block, including silent blocks, and calls the analyzer if present (`audiocapture.cpp:372-397`). However, because no analyzer is installed in production, this path is dormant. Also, silence detection is only `rms < kSilenceRms` (`audiocapture.cpp:323-325`), while the contract required both RMS and FFT magnitude checks. |
| DD5 — Functions reference profiles, not widgets | **Mostly implemented structurally; runtime incomplete** | `RGBMatrix` owns `audioProfileId` and persists `<AudioProfileID>` (`rgbmatrix.cpp:241-253`, `521-524`). `Doc::audioProfileForFunction()` resolves explicit → default/first (`doc.cpp:1099-1117`). This is decoupled from `VCAudioTrigger` widget IDs. However, the runtime channel is null unless profiles are bound to an analyzer (`rgbscriptv4.cpp:660-663`), so the reference does not produce new DSP data. |
| DD6 — Perceptual bands replace low/mid/high | **Partially implemented** | `AudioChannel` computes five bands and aliases `low = (sub + bass) / 2` (`audiochannel.cpp:221-232`). `RGBScript` exposes `audio.bands.{sub,bass,lowMid,mid,high,low}` when a channel exists (`rgbscriptv4.cpp:758-765`). Because channels are not bound in production, scripts usually do not receive these fields. Legacy low/mid/high fallback remains in `AudioParams.bandPower()` (`audio_common.js:97-112`). |
| DD7 — Per-script DSP sliders removed | **Not implemented** | `AudioParams.installContinuous()` and `installTrigger()` still register `presetGain`, `presetReactivity`, `presetFloor`, and `presetSensitivity` as active script properties (`audio_common.js:15-49`). Deprecated helpers have comments (`audio_common.js:51-61`, `172-174`, `222-225`, `254-257`) but no one-shot runtime warnings. Scripts still actively call `AudioParams.filterRise`, `applyFloor`, and `triggerThreshold` in many files. |
| DD8 — Per-consumer state via channel handles | **Partially implemented** | `AudioAnalyzer::createChannel()` / `destroyChannel()` and `AudioChannel::updateConfig()` exist (`audioanalyzer.cpp:121-141`, `audiochannel.cpp:103-114`). But profile/channel lifecycle is not integrated into `Doc`/`AudioCapture`; raw `AudioChannel*` pointers are cached by `RGBScript` (`rgbscriptv4.cpp:660-664`), creating deletion risks. |
| DD9 — Two dtMs values | **Partially implemented** | `audioDtMs` is exposed from snapshot and `consumerDtMs` is set in JS object (`rgbscriptv4.cpp:796-798`). But analyzer computes `audioDtMs` from FFT size/sample rate only (`audioanalyzer.cpp:194-200`), ignoring the host-time delta already computed in `AudioCapture` (`audiocapture.cpp:277-281`, then `Q_UNUSED(audioDtNs)`). |
| DD10 — Don’t layer on legacy smoothed power | **Partially implemented** | `AudioChannel` uses `frame.rmsDb`/`frame.rms` for AGC/volume (`audiochannel.cpp:116-129`, `167-176`). But `RGBScript` still always builds `audio.spectrum` from legacy `dataProcessed()` normalized by `maxMagnitude` and leaves it unchanged even when v2 data is available (`rgbscriptv4.cpp:722-731`, `753-800`). Several port docs say scripts expect pre-processed C++ spectrum, but the runtime object still contains legacy-normalized spectrum. |
| DD11 — Legacy per-bar triggers coexist with new per-band triggers | **Partially implemented** | Legacy per-bar VCAudioTrigger logic remains in `slotSpectrumDataChanged()` and still starts/stops functions/widgets/DMX (`vcaudiotriggers.cpp:646-750`, `951-988`). New per-band triggers exist in `AudioChannel` and JS object builder (`audiochannel.cpp:178-218`, `rgbscriptv4.cpp:767-775`), but are dormant in production without bound channels. Coexistence is structurally present but runtime incomplete. |
| DD12 — Trigger state machine frame-stable | **Partially implemented** | `TriggerState` fields are exposed to JS (`rgbscriptv4.cpp:65-75`, `767-775`) and state is not consumed on read. But actual script availability depends on channel binding. Also, beat trigger uses same hold/cooldown thresholds as band triggers, which may or may not be musically appropriate. |
| DD13 — Backward-compatible XML with schema versioning | **Partially implemented / missing migration** | `AudioProfile` writes `Version="1"` (`audioprofile.cpp:276-281`) and old files without `<AudioProfile>` still load. `RGBMatrix` omits `AudioProfileID` by default and loads older files fine (`rgbmatrix.cpp:521-524`). However, old per-script slider values are not converted to generated profiles on load. `AudioProfile::configFromLegacySliders()` exists (`audioprofile.cpp:318-321`) but no production caller was found. Old slider XML remains as script properties (`rgbmatrix.cpp:551-556`). |
| DD14 — AudioAnalyzer on audio thread with budget | **Partially implemented** | Analyzer runs synchronously on `AudioCapture` thread if installed (`audiocapture.cpp:396-397`) and has timing warnings (`audioanalyzer.cpp:102-118`). But production wiring is absent, so the budget is not exercised in real runtime. No CI/performance assertion was observed beyond instrumentation. |
| DD15 — Golden tests gate migration | **Weak / partially implemented** | Golden tests do not instantiate real `VCAudioTriggers`; they replicate aggregation logic (`vcaudiotriggers_golden_test.cpp:35-71`) and assert “sensible” positive values (`98-130`). The docs acknowledge the real widget is not tested (`p2b-golden-expose.md:58-80`). `audioslice_test` source-inspects `rgbscriptv4.cpp` rather than invoking `buildAudioDataObject()` (`audioslice_test.cpp:153-199`). These are not true old-vs-new golden comparisons. |
| DD16 — Pilot scripts before full port | **Implemented procedurally, weakly verified** | Pilot and batch port docs exist, with `node --check` verification. That catches syntax only, not visual parity or runtime audio behavior. |
| DD17 — Keep ExpFilter shape | **Implemented** | `AudioDSP.Filter` was added and documented as matching `LedFx.ExpFilter` (`p4-expfilter.md`). `AudioParams.createFilter()` now returns `new AudioDSP.Filter(...)` (`audio_common.js:59-62`). |
| DD18 — Phase 2 split for risk reduction | **Partially followed** | Backend/profile/panel slices exist, but the split left critical runtime wiring and editing incomplete. The risk reduction worked for scaffolding but not for deployable architecture coherence. |

---

## Blocking issues

### 1. New DSP pipeline is not wired into production runtime

**Severity:** Blocking  
**Impact:** The app likely does not deliver `audio.bands`, `audio.triggers`, `audio.volume.*`, or `audio.features.*` to scripts in real use. VCAudioTrigger perceptual meters also stay zero unless channels are bound elsewhere.

**Evidence:**

- `AudioCapture::setAnalyzer()` exists (`engine/audio/src/audiocapture.cpp:135-139`).
- `AudioCapture::processData()` only invokes analyzer if `m_analyzer` is set (`audiocapture.cpp:396-397`).
- `AudioProfile::bindAnalyzer()` creates the channel (`audioprofile.cpp:118-127`).
- Search found no production calls to either `AudioCapture::setAnalyzer()` or `AudioProfile::bindAnalyzer()`.
- `RGBScript::resolveAudioProfile()` caches `profile->channel()` (`rgbscriptv4.cpp:660-663`), which will be null if `bindAnalyzer()` never ran.
- `buildAudioDataObject()` returns legacy-only data when `channel == NULL` (`rgbscriptv4.cpp:749-751`).

**Recommended fix:**

Add a single owner/lifecycle for the analyzer, likely in `Doc` or `AudioCapture` creation:

1. Create/own `AudioAnalyzer` with the document/audio subsystem.
2. Install it on `Doc::audioInputCapture()` via `setAnalyzer()`.
3. Bind every existing `AudioProfile` to that analyzer after load and when profiles are added.
4. Bind newly created profiles immediately.
5. Unbind safely on profile removal/document teardown.
6. Add an integration test that creates a real `Doc`, calls `audioInputCapture()`, creates/loads profiles, feeds capture/analyzer, and asserts `RGBScript` receives v2 audio fields.

---

### 2. VCAudioTrigger is not the planned Audio Control Center editor

**Severity:** Blocking for DD2 / UX completeness  
**Impact:** Users cannot actually edit profile DSP config through the intended UI. The architecture says profiles are edited/monitored by VCAudioTrigger, but the implementation is monitor scaffolding plus legacy per-bar editing.

**Evidence:**

- QML `placeholderValue()` explicitly says envelope, AGC, trigger, and spectral feature values are TODOs (`VCAudioTriggersProperties.qml:60-64`).
- `VCAudioTriggers` exposes `audioProfileId` and band powers, but no profile config setters were found (`vcaudiotriggers.cpp:285-309`, `360-394`).
- Existing QML still focuses on legacy spectrum bars and thresholds (`VCAudioTriggersProperties.qml:123+`).

**Recommended fix:**

Implement actual profile editing bindings:

- Profile selector bound to document profiles.
- Band edge controls → `AudioChannelConfig.bandLayout`.
- Envelope attack/release controls → `AudioChannelConfig.envelope`.
- AGC controls → `AudioChannelConfig.agc`.
- Trigger high/low/hold/cooldown controls → `AudioChannelConfig.triggers`.
- Noise gate and brightness floor controls.
- Use `AudioProfile::setChannelConfig()` so changes push to bound `AudioChannel::updateConfig()`.

---

### 3. Old per-script slider values are not converted to profiles on XML load

**Severity:** Blocking for DD13 backward compatibility  
**Impact:** Old show files load, but the promised migration does not happen. Users’ old gain/reactivity/floor/sensitivity values remain as script properties instead of becoming AudioProfile config. If per-script UI is later removed, those values will be lost or ignored.

**Evidence:**

- `AudioProfile::configFromLegacySliders()` exists but no production caller was found (`audioprofile.cpp:318-321`).
- RGBMatrix XML still loads script properties directly (`rgbmatrix.cpp:551-556`).
- `Doc::loadXML()` loads `<AudioProfile>` if present but has no migration path for old RGBMatrix script properties (`doc.cpp:1406-1423`).

**Recommended fix:**

During RGBMatrix/script XML load or post-load migration:

1. Detect audio scripts with legacy `presetGain`, `presetReactivity`, `presetFloor`, `presetSensitivity`.
2. Generate an `AudioProfile` using `AudioProfile::configFromLegacySliders()`.
3. Assign it to `RGBMatrix.audioProfileId`.
4. Optionally preserve original properties for one release but stop treating them as primary DSP config.
5. Add XML fixture tests with old `.qxw` snippets.

---

### 4. Deprecated per-script sliders remain active and do not emit one-shot warnings

**Severity:** Blocking for DD7 completion  
**Impact:** The project still exposes old DSP controls and scripts actively depend on them. This contradicts “per-script DSP sliders removed” and makes it difficult to know whether profile config or script-local config is authoritative.

**Evidence:**

- `installContinuous()` still registers `presetGain`, `presetReactivity`, `presetFloor` (`audio_common.js:15-31`).
- `installTrigger()` still registers `presetGain`, `presetReactivity`, `presetSensitivity` (`audio_common.js:33-49`).
- Helpers only have comments, not runtime warnings (`audio_common.js:51-61`, `172-174`, `222-225`, `254-257`).
- Many scripts still call `AudioParams.filterRise`, `applyFloor`, and `triggerThreshold`.

**Recommended fix:**

For the transition release:

- Keep compatibility only for loading/legacy fallback.
- Add one-shot `console.warn`/QLC logging per deprecated helper/property path.
- Hide/remove these sliders from the RGBMatrix editor once profile selector/editing exists.
- Convert saved legacy values to profiles before removing them from UI.

---

### 5. `audio.spectrum` remains legacy-normalized, not channel-processed

**Severity:** Blocking for script-port correctness  
**Impact:** Several ported scripts removed gain multiplication assuming C++ profile/channel now processes spectrum. But `buildAudioDataObject()` still builds `audio.spectrum` from legacy `dataProcessed()` normalized by frame max magnitude. This may change brightness/response significantly versus the intended profile-controlled path.

**Evidence:**

- `audio.spectrum` is always built before channel snapshot from `m_audioSpectrum / m_audioMaxMagnitude` (`rgbscriptv4.cpp:722-731`).
- When a snapshot exists, code adds bands/triggers/volume/features but does not replace `audio.spectrum` with `snap.spectrum` (`rgbscriptv4.cpp:753-800`).
- `AudioChannel` does compute processed `snapshot.spectrum` (`audiochannel.cpp:150-152`, `232`), but it is not exposed to JS.

**Recommended fix:**

When `channel != NULL`, expose the processed `AudioSnapshot::spectrum` as `audio.spectrum`, or add `audio.spectrumProcessed` and update scripts deliberately. Tests should assert gain/noise-gate/envelope config affects script-visible spectrum.

---

## Non-blocking issues

### 6. Profile deletion can leave raw `AudioChannel*` dangling

**Severity:** Non-blocking now only because channels are not wired; blocking once analyzer binding is fixed  
**Impact:** If a profile is deleted while scripts/widgets are running, cached raw channel pointers can become use-after-free.

**Evidence:**

- `Doc::removeAudioProfile()` deletes the `AudioProfile` directly (`doc.cpp:1039-1051`).
- `AudioProfile::~AudioProfile()` releases/destroys analyzer channel (`audioprofile.cpp:67-70`, `129-136`).
- `RGBScript` caches raw `AudioChannel*` in `m_audioChannel` (`rgbscriptv4.cpp:660-664`) and later uses it without revalidation (`rgbscriptv4.cpp:749-753`).
- `VCAudioTriggers` resolves each time in `updateAudioProfileSnapshotPowers()`, so it is less exposed, but scripts are at risk.

**Recommended fix:**

Use a handle/token abstraction rather than raw pointers, or invalidate/re-resolve all script/widget consumers on profile removal. At minimum, connect profile removal to script teardown/re-resolution.

---

### 7. Tests do not exercise the real QML/VCAudioTrigger integration

**Severity:** Non-blocking but important coverage gap  
**Impact:** Q_PROPERTY updates, QML bindings, profile selection, and real widget lifecycle can break without tests catching it.

**Evidence:**

- Golden test explicitly uses a replica of widget aggregation logic (`vcaudiotriggers_golden_test.cpp:35-71`).
- Review doc states real `VCAudioTriggers` was not instantiated (`p2b-golden-expose.md:58-80`).
- QML placeholders remain untested.

**Recommended fix:**

Add a `qmlui/test` integration suite or a lower-level widget fixture that:

- Instantiates `VCAudioTriggers`.
- Binds it to a real `Doc` profile/channel.
- Feeds analyzer frames.
- Asserts Q_PROPERTY values and QML-visible properties change.

---

### 8. `audioDtMs` ignores actual capture timing

**Severity:** Non-blocking / correctness risk  
**Impact:** Envelope/trigger timing may be wrong under device jitter, buffer underruns, or nonstandard capture cadence.

**Evidence:**

- `AudioCapture` computes host-time delta but discards it (`audiocapture.cpp:277-281`).
- `AudioAnalyzer::computeAudioDtMs()` returns fixed FFT hop duration (`audioanalyzer.cpp:194-200`).

**Recommended fix:**

Add `dtMs`/host delta to `AudioFrame` and use it for channel update timing, with FFT-hop fallback when unavailable.

---

### 9. Silence contract is only partially followed

**Severity:** Non-blocking  
**Impact:** Frames with low RMS but nontrivial spectral content may be classified silent incorrectly. This affects noise floor, trigger suppression, and spectrum decay.

**Evidence:**

- Contract requires RMS and max FFT magnitude checks.
- Implementation uses only RMS: `const bool silent = (rms < kSilenceRms);` (`audiocapture.cpp:323-325`).

**Recommended fix:**

Implement the contract’s two-condition silent frame check after FFT magnitudes are available.

---

## Test coverage gaps

| Gap | Why it matters |
|---|---|
| Production analyzer/profile binding | Current tests create `AudioAnalyzer`/`AudioChannel` manually; they do not prove the app wires them. |
| Real `RGBScript::buildAudioDataObject()` execution with QJSEngine | `audioslice_test` source-inspects instead of invoking the JS object builder. |
| Old `.qxw` migration | No evidence of fixture tests proving old per-script slider values become generated profiles. |
| QML profile editing | The intended editor UI is not implemented/tested. |
| Real `VCAudioTriggers` widget | Golden tests use replicated logic, not the widget. |
| Profile deletion while running | No lifecycle/race tests. |
| Multiple widgets sharing same profile | No test that both monitor same channel and config edits propagate safely. |
| No audio device startup | No test proving capture/analyzer/profile setup behaves safely with unavailable input. |
| Visual parity of ported scripts | `node --check` only validates syntax, not behavior. |
| Processed spectrum exposure | No test verifies profile gain/noise gate/envelope affects `audio.spectrum` visible to scripts. |

---

## Runtime risk matrix

| Scenario | Current behavior / risk | Severity | Recommended mitigation |
|---|---|---:|---|
| App starts with no audio device | `Doc::audioInputCapture()` creates capture lazily (`doc.cpp:285-299`). Legacy paths likely produce no frames/empty data. New profile/analyzer path is currently unwired, so scripts receive legacy object only. Risk is degraded behavior rather than crash, but untested. | Medium | Add startup test with unavailable device/mock capture. Ensure audio object still has stable fields and no null dereferences. |
| Profile deleted while scripts are running | If analyzer binding is fixed, `removeAudioProfile()` can delete a profile/channel while `RGBScript` holds raw `AudioChannel*`. Potential UAF. | High | Introduce safe handles or consumer invalidation/re-resolution on profile deletion. |
| Multiple AudioTrigger widgets reference same profile | Today they only read snapshot powers and cannot edit config. If channel is unbound, all show zeros. Once bound, shared monitor reads are probably okay, but editing conflicts are undefined because editing is missing. | Medium | Define shared-edit semantics, add profile selector/editor tests, emit config/profile change notifications. |
| Old `.qxw` show file loaded | It loads because new XML is additive, but old per-script slider values are not converted into profiles. Scripts continue using legacy properties. Future removal would break old shows. | High | Implement migration and XML fixture tests before removing sliders. |
| Ported scripts run expecting new audio object | Because channel binding is missing, `audio.bands`/`audio.triggers` are absent; many scripts use fallbacks, but removed gain behavior and legacy spectrum assumptions may change output. | High | Wire analyzer/channel and test script audio object v2. |
| Quiet room / noise floor | New analyzer has noise floor logic, but production scripts likely still use legacy normalized spectrum. Quiet-room stability goal not met. | High | Expose channel-processed bands/spectrum and ensure scripts use them. |
| Live VCAudioTrigger monitor | Perceptual bands read zero unless channel exists; triggers/envelope/AGC/spectral values are placeholders. | Medium | Bind profiles and expose full snapshot fields. |

---

## Recommended follow-up items

### Must fix before calling the architecture complete

1. **Wire production analyzer lifecycle**
   - Own `AudioAnalyzer` in `Doc` or audio subsystem.
   - Call `AudioCapture::setAnalyzer()`.
   - Bind all `AudioProfile`s to it.
   - Bind profiles on add/load/create.

2. **Make `RGBScript` actually receive v2 audio**
   - Ensure `m_audioChannel` is non-null after resolution.
   - Use anonymous analyzer fallback only when no profile exists and auto-create fails.
   - Add direct JS object integration tests.

3. **Expose channel-processed spectrum**
   - Replace or supplement legacy `audio.spectrum` with `AudioSnapshot::spectrum`.
   - Verify profile gain/noise gate affects script-visible data.

4. **Implement legacy XML slider migration**
   - Convert old `preset*` properties to generated `AudioProfile`.
   - Assign `RGBMatrix.audioProfileId`.
   - Add `.qxw` fixture tests.

5. **Implement actual VCAudioTrigger profile editor**
   - Profile selection.
   - Envelope/AGC/band/trigger/noise gate controls.
   - Live snapshot bindings for all planned monitor fields.

6. **Replace deprecated stubs with one-shot warnings**
   - Keep compatibility but log when old DSP helpers/properties are used.
   - Stop exposing old sliders as normal script UI once profile editing exists.

### Should fix soon

7. **Harden profile deletion and rebinding**
   - Avoid raw channel pointer lifetime hazards.
   - Add profile-deletion runtime tests.

8. **Improve golden tests**
   - Capture actual old widget outputs for deterministic inputs.
   - Compare new pipeline within tolerance.
   - Avoid relying on replicated logic and source inspection.

9. **Test real QML/VCAudioTrigger**
   - Add qmlui/widget integration tests for Q_PROPERTY updates.

10. **Use real audio frame delta**
   - Carry host `dtMs` through `AudioFrame`.
   - Use fixed hop only as fallback.

11. **Complete silence contract**
   - Include FFT magnitude condition in `silent`.

---

## Bottom line

The modernization has useful scaffolding, but the core architecture is **not yet realized at runtime**. The biggest gap is that `AudioAnalyzer`, `AudioProfile`, `AudioChannel`, `RGBScript`, and `VCAudioTrigger` are not wired into a single live pipeline. Until that is fixed, most of the new DSP model exists only in unit tests and dormant code paths, while production behavior remains largely legacy.