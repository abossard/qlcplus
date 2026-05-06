# Fix B3: Dangling audio profile channel pointer

## Problem

`RGBScript` cached an `AudioChannel *` resolved from the owning RGB matrix audio profile. Removing an audio profile or clearing the document destroys that channel, leaving the cached pointer dangling. The next `buildAudioDataObject()` call could dereference freed memory.

## Fix

- Removed the cached `AudioChannel *` from `RGBScript`.
- Removed `resolveAudioProfile()` and profile ID cache state.
- `buildAudioDataObject()` now re-resolves the owning matrix profile on each call, reads the channel only if the profile and channel still exist, and otherwise returns the legacy audio fields.
- Kept the profile selection debug message as a one-shot log using `m_audioProfileLogged`.
- Verified `Doc::clearContents()` releases audio profile analyzers before destroying `AudioCapture` and `AudioAnalyzer` infrastructure.

## Safety impact

Re-resolving avoids holding ownership-sensitive raw pointers across profile deletion. If a profile is removed, subsequent script frames see no channel and skip enriched v2 audio fields instead of dereferencing stale memory.
