# Review: Strip old-engine fallbacks

Reviewer: GPT 5.5 | Strip by: Opus 4.7

## Blocking Issues

### B1: buildAudioDataObject() can still emit audio WITHOUT bands/triggers
When no AudioProfile/channel is resolved, early-return skips new fields.
Scripts now crash on `audio.bands.low` in that path.
Fix: always populate zero-valued bands/triggers, or guarantee channel resolution.

### B2: 5 scripts lack minimum audio guard
audiobasslaser, audiobuildup, audiofireworks, audiohueshift, audioshockwave
read `audio.bands.*` without `if (!audio || !audio.spectrum) return map;`
Fix: add the guard.

## Non-Blocking
- 3 scripts still have beat trigger fallback (audioshot, audiostrobe, audiopower)
- `bandPower` local var in audiochaser (false positive, not a function call)

## Clean
- All syntax valid, all deprecated helpers removed, no LedFx calls, no onset fallbacks
