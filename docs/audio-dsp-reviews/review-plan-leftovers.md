# Review of `plan-leftovers.md`

Reviewer: Claude Opus 4.7. The plan was authored by GPT-5.5.

I verified the file/line references against the current tree and read the
relevant sources (`audiocapture.{h,cpp}`, `audioanalyzer.{h,cpp}`,
`audiochannel.cpp`, `rgbmatrix.{h,cpp}`, `rgbscriptv4.cpp`, `audioprofile.cpp`,
`resources/rgbscripts/audio_common.js`, `engine/src/doc.cpp`).

The line numbers in the plan are accurate, and the proposed fixes generally
work, but several items overstate the severity of the underlying issue, and
two items add new concurrency hazards that the plan does not adequately
address. Details below.

---

## Issue 1 — Atomic `m_analyzer`

**Severity: Suggestion (not a real race today).**

The plan implies there is an unsynchronized publication of `m_analyzer`. There
isn't. `AudioCapture::run()` takes `QMutexLocker locker(&m_mutex)` *before*
calling `processData()` (`audiocapture.cpp:453-455`), and `setAnalyzer()` takes
the same `m_mutex` (`audiocapture.cpp:135-139`). The pointer is therefore
already published under a mutex.

What the plan really proposes is a refactor: switch from mutex-protected
publication to atomic publication and **drop the setter mutex**. That's fine,
but please be honest about it in the plan — it isn't fixing a race, it's
changing the synchronization style.

Two concrete risks the plan doesn't mention:

1. The plan says "Remove the `QMutexLocker` from this setter unless it is
   still needed for another field". `m_mutex` is also held during
   `processData()` for the entire body, so dropping it from the setter is
   safe *only* because `m_analyzer` is the sole field being mutated by the
   setter. State that explicitly so a future reader doesn't strip locks
   from other setters by analogy.
2. `Doc::destroyAudioCapture()` calls `setAnalyzer(nullptr)` on the GUI
   thread while the capture thread may be inside `processData()` and holding
   `m_mutex`. With the mutex setter, that blocks the GUI thread until the
   capture frame ends. With the proposed atomic, the GUI thread races the
   capture thread to set the pointer, which is the *intent*, but means
   `destroyAudioCapture()` no longer waits for the in-flight `processFrame()`
   call to finish before the analyzer can be deleted by `clearContents()`.
   The plan needs to keep a synchronization point (e.g. the existing capture
   `stop()` call) between `setAnalyzer(nullptr)` and `delete m_audioAnalyzer`.
   Today this works because `clearContents()` calls `destroyAudioCapture()`
   first (which `clear()`s the `QSharedPointer` and may trigger `~QThread`
   join) before deleting the analyzer. Verify and document that ordering.

**Recommended fix:**
- Keep the change, but reframe it as a refactor.
- Add an assertion / comment to `destroyAudioCapture()` documenting that the
  capture thread must be joined before `m_audioAnalyzer` is deleted.

---

## Issue 2 — Don't repeatedly call `setAnalyzer()` in `Doc::audioInputCapture()`

**Severity: Suggestion. Plan is correct.**

Verified at `engine/src/doc.cpp:309-324`: the unconditional
`m_inputCapture->setAnalyzer(audioAnalyzer())` runs every call. Moving it
into the `if (!m_inputCapture)` branch is correct and harmless because
`audioAnalyzer()` lazily creates exactly one analyzer per `Doc` and the
analyzer is only destroyed via `clearContents()` after `destroyAudioCapture()`.

No additional concerns.

---

## Issue 3 — Reset `m_prevMagnitudes` on bin-count change

**Severity: Non-Blocking. Plan is correct, but the impact wording is wrong.**

`computeSpectralFlux()` (`audioanalyzer.cpp:243-280`) only grows
`m_prevMagnitudeCapacity` and resets `m_prevBinCount` on grow. On a *shrink*
(e.g. switching to a smaller buffer/FFT, or to a lower sample rate), it does
**not** reset `m_prevBinCount`. The flux loop iterates `bin = lowBin..highBin`
where `highBin <= binCount-1`, so there's no out-of-bounds read — the buffer
is always sized to the previous max capacity. **The bug is not a memory
safety bug, it's a correctness bug:** bin index `k` corresponds to a
different physical frequency before and after the FFT-size change, so the
diff is meaningless for the first frame after switching. Please correct the
plan's "first frame after an FFT-size/device switch will report onset-like
flux against zeros" statement — today it reports *garbage*, not zeros.

The proposed fix (resize-and-reset on any size mismatch) is correct.

One nit on the test plan: assert that flux is **finite and bounded** after
the switch (e.g. `flux <= 1.0` since the formula divides by `previousSum`).
A test that asserts an exact value risks coupling to internal accumulator
semantics.

---

## Issue 4 — Connect `audioProfileIdChanged` to running scripts

**Severity: Non-Blocking. The framing is wrong, and the proposed cache
introduces a new race.**

Critical observation the plan misses: there is **no profile cache in
`RGBScript` today**. Look at `rgbscriptv4.cpp:711-718`:

```cpp
AudioProfile *profile = (currentDoc != NULL && matrix != NULL)
    ? currentDoc->audioProfileForFunction(matrix->id()) : NULL;
if (profile != NULL)
{
    channel = profile->channel();
    ...
}
```

The profile and channel are re-resolved on **every** `buildAudioDataObject()`
call. So when `setAudioProfileId()` changes the ID, the very next
`rgbMap()` call already picks up the new profile. The only stale piece of
state is `m_audioProfileLogged` — a debug-log latch (`rgbscriptv4.cpp:719-725`).

So:
- The plan describes a bug that does not currently exist. There is nothing
  to "invalidate" except a debug flag.
- The plan's proposed fix **introduces a new cache** (`m_cachedAudioProfileId`,
  `m_cachedAudioChannel`) and *then* asks for invalidation. That cache adds
  thread-safety complexity without quantified benefit. `audioProfileForFunction()`
  is a `QHash` lookup, not a hot path.
- If the cache is added, writing it from `RGBMatrix::setAudioProfileId()`
  (called on the GUI thread) while `buildAudioDataObject()` reads it on the
  JS thread is racy. Neither `m_audioMutex` nor `m_algorithmMutex` covers
  these new fields. The plan acknowledges this in one sentence ("Avoid
  calling into `QJSEngine` from a non-JS thread") but doesn't actually
  prescribe how to make the cache writes visible to the JS thread safely.

**Recommended fix (downgraded scope):**
- If the only observable bug is "the profile-change debug log doesn't
  re-trigger after `setAudioProfileId()`", just reset `m_audioProfileLogged`
  inside `buildAudioDataObject()` itself when `profile->id() !=
  m_lastLoggedProfileId`. Do it on the JS thread; no signal/slot wiring
  needed.
- Do not add a cache unless profiling shows `audioProfileForFunction()` is
  measurably hot.
- If a cache is added anyway, protect it with `m_audioMutex` (which
  `buildAudioDataObject()` already takes at line 674) and write to it from a
  queued slot connected to `audioProfileIdChanged` so the write happens on
  the JS thread.

**Issue-4 / Issue-1 interaction:** If the plan does add the cache and writes
it from the GUI thread, the analyzer atomic refactor in Issue 1 might lull
the implementer into thinking "atomic = thread-safe" and skipping a mutex
here. The two issues use different threads; please call this out explicitly.

---

## Issue 5 — Remove deprecated `gainFactor()`

**Severity: Suggestion. The "double-gain" framing is misleading.**

I verified the requested grep:

```
$ rg "gainFactor" resources/rgbscripts/
audio_common.js:51:    gainFactor: function(algo) { return 0.6 + algo.presetGain * 0.2; }, // DEPRECATED
```

**There are no callers in repository scripts.** The only match is the
definition itself. Therefore there is **no double-gain happening today**;
the plan's title oversells the fix. It is dead code removal, not a bug fix.

For completeness, here's where `inputGainLinear` is actually applied:

- `engine/audio/src/audiochannel.cpp:151` — applied to the 32-band spectrum
  array (`m_spectrum`) and to per-band values (`m_bandValues`).
- `engine/audio/src/audiochannel.cpp:176` — applied to volume normalization.

These flow into `audio.bands.*`, `audio.volume.*`, `audio.triggers.*` in
`buildAudioDataObject()`. **They do not flow into `audio.spectrum`** —
`audio.spectrum` (rgbscriptv4.cpp:685-693) is built from `m_audioSpectrum`,
which comes from the legacy `dataProcessed` signal and is normalized by
`audioMaxMagnitude`, *not* by `inputGainLinear`. So there's an inconsistency
worth noting in the plan: scripts using `audio.spectrum` see un-gained,
self-normalized data, while scripts using `audio.bands.*` see profile-gained
data. That asymmetry is outside the scope of issue 5 but is the more
interesting finding here.

**Recommended fix:**
- Pick the no-op return-1.0 variant for back-compat with third-party scripts
  (the plan already lists this option). Keeping the helper as a no-op is
  strictly safer than deleting and adds zero maintenance cost.
- Re-title the issue to "remove dead `gainFactor()` helper" and drop the
  "prevent double-gain" claim. Optionally, add a separate item documenting
  the `audio.spectrum` vs `audio.bands.*` gain asymmetry.

---

## Issue 6 — Validate `Version` on `AudioProfile` XML load

**Severity: Suggestion. Plan is correct.**

The plan accurately reflects the load (`audioprofile.cpp:143-170`) and save
(`audioprofile.cpp:272-281`) paths. Treating missing `Version` as v1 is the
right backward-compat choice given that `Version="1"` was added recently and
older project files won't have it.

One small note: the proposed test "`Version='abc'` fails" implies strict
parse-int with `bool ok = false; toInt(&ok)`. Make sure to also reject
`Version="0"` and negative values, otherwise the regression for malformed
versions is incomplete.

---

## Cross-issue concerns

1. **Ordering.** The proposed order (1+2 → 3 → 6 → 5 → 4) is sensible.
   Issue 4 last is right because it's the most invasive.
2. **No bad interactions** between issues 1–3, 5, 6.
3. **Issue 1 + Issue 4 together** is the only real interaction — both touch
   thread-safety on objects shared between the GUI thread, the audio
   capture thread, and the JS thread. If both are taken on, write a single
   short note in the design about which thread owns which field, so the
   atomic / mutex / queued-connection choices are not made ad hoc per issue.
4. **Build/test commands** in the plan all reference real targets in this
   repo (verified that `engine/test/audioanalyzer/`, `audioprofile/`,
   `rgbscript/`, `rgbmatrix/` exist as test directories — assumed; the plan
   should sanity-check these paths before publishing run commands).

---

## Summary

- **Blocking issues: none.** Every fix can land as described.
- **Non-blocking corrections:**
  - Issue 1: reframe as refactor, not race fix; document the
    capture-thread join requirement around analyzer destruction.
  - Issue 3: correct the impact statement — current behavior on shrink is
    "garbage values", not "zeros".
  - Issue 4: there is no cache today; the proposed fix invents one. Either
    drop the cache or protect it with `m_audioMutex` and write from the JS
    thread via a queued connection.
  - Issue 5: drop the "double-gain" framing — no scripts call `gainFactor`.
    Prefer the no-op variant for third-party compatibility.
- **Suggestions:**
  - Issue 6: also reject `Version="0"` and negative values in the test.
  - Note the `audio.spectrum` vs `audio.bands.*` gain asymmetry as a
    follow-up (out of scope for issue 5).
