# Review: Script fallback fixes

Reviewer: GPT 5.5 | Fixes by: Opus 4.7

## Verdict: All 13 scripts ✅ — no issues

All 13 scripts verified:
- ✅ bandValue() helper with AudioParams.bandPower() fallback
- ✅ No direct audio.bands.* reads outside helper
- ✅ No || !audio.bands guards blanking output
- ✅ node --check passes
- ✅ usesAudio = true set

Global checks clean. No blocking or non-blocking issues found.
