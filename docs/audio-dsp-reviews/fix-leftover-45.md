# Leftovers #4 and #5 — Fix Summary

Implemented the simplified versions recommended in `review-plan-leftovers.md`.

## #4 — Profile hot-swap log flag

**Problem:** `m_audioProfileLogged` (bool) latched on the first profile and never reset, so swapping the matrix's `audioProfileId` at runtime never re-logged the new profile. Per the critique, no stale-pointer cache fix was needed — `buildAudioDataObject()` already re-resolves `profile->channel()` every frame.

**Fix:** Replaced the bool with `quint32 m_loggedAudioProfileId` (init `AudioProfile::invalidId()`).

- `engine/src/rgbscriptv4.h`: renamed member, added comment explaining intent.
- `engine/src/rgbscriptv4.cpp`:
  - Both constructors initialise to `AudioProfile::invalidId()`.
  - `buildAudioDataObject()` re-logs once whenever `profile->id()` differs from the last logged ID, and resets to `invalidId()` when no profile is assigned (so a future assignment is logged again).

No new caches, no cross-thread signal connections — the per-frame re-resolve already guarantees correctness; this only governs the debug print.

## #5 — Dead audio helpers

`grep -rn "AudioParams.<fn>" resources/rgbscripts/audio*.js` (excluding `audio_common.js`) — caller counts:

| Function | Callers | Action |
|---|---|---|
| `gainFactor` | 0 | Marked dead-code DEPRECATED |
| `createFilter` | 0 | Marked dead-code DEPRECATED |
| `adaptiveGain` | 0 | Already documented as superseded by C++ AGC — kept |
| `hysteresisTrigger` | 0 | Already documented as superseded by C++ triggers — kept |
| `frameNormalizedDecay` | 0 | Already documented as superseded by C++ ms smoothing — kept |
| `filterRise` | 14 | **Still in use** — removed misleading "DEPRECATED" comment |

**File touched:** `resources/rgbscripts/audio_common.js`
- Added `// DEPRECATED: no longer called by bundled scripts. Kept for community script compatibility.` above `gainFactor` and `createFilter`.
- Removed the incorrect deprecation tag from `filterRise` (it's actively used by 14 bundled scripts via `AudioDSP.Filter(decay, AudioParams.filterRise(algo))`).
- `adaptiveGain` / `hysteresisTrigger` / `frameNormalizedDecay` already had richer DEPRECATED docblocks — left untouched.

No function bodies were removed (per critique: keep helpers callable so community scripts don't break).

## Verification

```
cd build && cmake --build . --target qlcplus-qml -j8
```

Result: clean build, `qlcplus-qml` linked successfully. No new warnings touching the modified files.
