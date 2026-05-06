# P3 RGBMatrix AudioProfile ID

Implemented the Phase 3 profile-reference storage for `RGBMatrix`.

## Added

- `RGBMatrix::audioProfileId()` getter
- `RGBMatrix::setAudioProfileId(quint32 id)` setter
- `audioProfileIdChanged()` notification signal
- `m_audioProfileId`, defaulting to `AudioProfile::invalidId()`

## XML shape

When a matrix references a valid audio profile, the ID is persisted as a direct child of the RGB matrix function:

```xml
<Function Type="RGBMatrix" ...>
  ...
  <AudioProfileID>0</AudioProfileID>
</Function>
```

`<AudioProfileID>` is omitted when the value is `AudioProfile::invalidId()`, so existing RGBMatrix XML remains unchanged by default. Loading older files leaves the field at the invalid sentinel.

## Scope

This phase only stores and restores the profile reference. Runtime script-engine resolution is intentionally deferred to `p3-profile-resolution`.
