# Audio analysis and script data

QLC+ supplies the same audio view to the HUEScript fifth argument and
`Engine.getAudioSnapshot()` in v5 Script functions. Its `version` is 6.
HUEScript's separate script `apiVersion` remains 3. The shipped audio
algorithms retain their names and Artistic defaults. Current delivery includes
63 audio Hue scripts plus seven non-audio Hue scripts.

## Setup and routing

The default Audio Trigger panel has six editable controls: source/device,
analysis profile, input level, noise floor, reaction preset and beat source.
Use Advanced for saved tuning and Diagnostics for traces. Diagnostic processing
is disabled by default. Enable it explicitly under Advanced for the selected
microphone profile to compute MFCC, TSS and other advanced diagnostics. The
diagnostic view reports disabled, pending or enabled processing. Toggling it
preserves saved tuning and does not enable another profile's diagnostics. Opening the panel
does not reset a profile. The status display includes raw dB/clip, gate state,
availability, selected/applied device and negotiated capture format.
Bank editors accept 4-32 bands and frequencies up to 15,000 Hz. Rejected
range or trigger-threshold edits restore the applied value and log the reason.

New profiles use LedFx analysis defaults. **LedFx defaults** resets the selected
profile to those values, including mel normalization, smoothing and noise floor.
It replaces that profile's custom analysis tuning, not its name, source selection
or other profiles. A saved profile named "Default Audio" can still contain custom
values; loading it does not reset them. Duplicate it first to keep its tuning.

The reference defaults use `matt_mel`, 24 bins per bank, filterbank power/norm 1,
and peak isolation 0.4 (mel exponent approximately 1.96261). AGC decay/rise is
0.01/0.99; smoothing 0.7/0.99; common filtering 0.99/0.01; novelty filtering
0.15/0.99. The noise floor is -80 dB with no hold, equivalent to LedFx's
normalized minimum volume of 0.2. No beat or bass is required to publish mids
or highs above that floor.

Each profile owns its analysis settings and state. A widget can select a
profile by ID; unqualified HUE, Script and the global Audio beat source follow
the active profile. Profile edits take effect together at the next analysis
frame. The published `configRevision`, bank metadata and values describe that
applied configuration.

Capture keeps one owner through device restarts. Default-device selection
follows the OS default; an absent explicit device reports unavailable instead
of selecting another microphone. Saved descriptions remain readable, while
new explicit selections use stable device IDs. OSC profiles retain their ports
and independent subscriptions.

### Selectable Low Latency profile

After restarting into a rebuilt binary at a time you choose, open an Audio
Trigger's properties and select **Low Latency** in the existing Profile chooser.
Opening the chooser makes the preset available; it does not select it or
enable mappings. Existing profile IDs, tuning, sources, defaults and widget
bindings are retained. A conflicting custom name gets a numbered preset name.
Renaming or editing the built-in preset does not cause it to be recreated.

Low Latency changes only three settings from the ordinary defaults:

| Setting | Ordinary | Low Latency |
|---|---|---|
| Scalar-power FFT window | 4096 samples | 2048 samples, independent bank and normalization state |
| Visual update gate | 33 ms | 16 ms, within the existing 16-ms polling timer |
| Shared microphone buffer request | Platform default | 20 ms |

The ordinary spectrum, cumulative banks, MFCC, spectral diagnostics and raw
pitch/tempo/onset/notes detectors keep their 4096-sample windows. Capture still
publishes 500 samples at 30000 Hz. Crossovers, boost and smoothing are unchanged.
Mapping dispatch and event recovery precede the visual gate; this feature does
not change the lighting output rate.

Only actual microphone subscribers affect the buffer request. Stored presets,
disabled widgets and OSC consumers do not request input. Widget selections are
independent; unqualified HUE/Script/Audio-clock consumers follow the global
active profile. A first 20-ms requester immediately reopens the shared capture
on its capture thread, briefly interrupting every microphone consumer. A second
identical requester does not reopen it. The platform default is restored after
the last requester leaves. The source epoch resets on these normal restarts.

Input status distinguishes the profile/shared request, request submitted before
opening, negotiated capacity, queued-audio duration, last successful PCM-read
age, and shared analysis processing time. A zero request means platform default,
not zero latency. Unavailable/unsupported or failed values are explicit. The
device may negotiate a capacity different from 20 ms. Queue duration describes
software backlog; it does not measure acoustic arrival time.

Synthetic production-PCM replay verifies the power response and unchanged
non-power/raw processing. It does **not** measure native hardware or end-to-end
latency. FFT history is not a fixed delay, and a smaller requested buffer does
not guarantee a physical latency reduction. Save/preserve work and restart or
select the new profile yourself when appropriate; no live restart or automatic
selection is part of build verification.

### Spectrum Bar Mappings

Enable mappings explicitly in the widget or its properties. Meters can update
while mappings are paused; opening or loading the widget does not enable them.
The first rows match the chart in order and color: Kick power, Bass, Lows, Mids,
High, followed by Volume. These are continuous levels. A slider follows the
level scaled to 0–255; a Function or Button is active while a power level is
nonzero. A submaster affects functions controlled by children of its own frame,
not unrelated functions elsewhere in the console.

Low/Mid/High bank triggers retain their original bank values and thresholds.
Beat pulse follows musical beat events, not low-frequency power. Kick hit
follows the kick detector, not Kick power. Several assignments to one slider
remain independent: the last update wins, so they can overwrite one another.
Test each assignment alone when diagnosing a stationary slider.

Stored source keys and MCP indices 0–5 retain their meanings: `Low`, `Mid`,
`High`, `Volume`, `Beat`, `Kick`. New indices 6–10 use `KickPower`, `BassPower`,
`LowsPower`, `MidsPower`, `HighsPower`. Historical `Bass` still loads as the
Low bank trigger. MCP widget queries report each source's key, label and color.
The mapping table grows with its rows and has no internal scrolling.

## Analysis and lifetime

Capture mixes interleaved Int16 or Float32 PCM to mono, resamples to 30,000 Hz,
and assembles 500-sample frames. Spectral analysis uses a 4,096-sample window.
Packet boundaries do not determine analysis cadence.

Raw PCM supplies volume, onset, pitch and tempo. Pre-emphasis applies to the
spectral branch. New reference-default profiles use a -80 dB noise floor;
saved profiles keep their tuning. Delivered silence zeros public banks while
freezing spectral filter state. Loss of callbacks marks the source unavailable
within 250 ms.

The active profile's canonical tempo supplies global Audio BPM and pulses.
Restarting or replacing the audio source also clears queued catch-up pulses.
Internal and Plugin beat sources retain their ownership. The historical
BeatTracker remains available to its regression tests but no longer processes
live capture or supplies a second audio clock.

## Published fields

| Fields | Meaning |
|---|---|
| `sourceId`, `profileId`, `sourceEpoch` | Source/profile identity and reset boundary. Active-profile selection adds a selection generation to the unqualified view's epoch. |
| `frameSequence`, `sampleTime` | Ordered source frame and first sample index at 30,000 Hz. |
| `configRevision`, `publishTimeNs` | Applied profile revision and monotonic publication time. |
| `available`, `status`, `staleAgeMs`, `gateOpen` | Freshness and gate state. Unavailable views expose zero levels, zero bank values and no event deltas. |
| `volume.rawRms`, `volume.rmsDb`, `volume.peakDb` | Raw amplitude and dBFS. |
| `volume.normalized` | Smoothed, clamped `1 + dBFS/100`. |
| `beat`, `bass`, `low`, `mid`, `high` | Smoothed scalar powers in 0..1. `low = (beat + bass)/2`; `beat` is energy, not an event. |
| `powers.raw.beat`, `powers.raw.bass`, `powers.raw.low`, `powers.raw.mid`, `powers.raw.high` | Unfiltered scalar powers. `raw.low = (raw.beat + raw.bass)/2`. Non-finite and unavailable/stale snapshots publish zeros. |
| `pitch.valid`, `pitch.hz`, `pitch.midi`, `pitch.confidence` | Canonical native pitch publication. `valid` requires finite positive Hz on an available snapshot. `midi = 69 + 12*log2(hz/440)`. Published `hz` and `midi` depend on canonical Hz only, not on display unit/value (`Midi`, `Cent`, `Bin`). Invalid or stale snapshots publish `valid=false` and finite zeros. |
| `banks.low`, `banks.mid`, `banks.full` | Cumulative banks with `count`, `minHz`, `maxHz`, `centersHz`, `processed`, `novelty`. |
| `tempo` | `bpm`, `valid`, `confidence`, `beatPhase`, `barPhase`, `beatInBar`, `beatsPerBar`. |
| `events.counters`, `events.delta` | Independent `onset`, `beat`, `kick`, `bar` counters and this consumer's increments. |
| `timing.deltaSeconds`, `timing.elapsedBeats` | Time since this consumer's previous committed render/read. Elapsed beats requires valid tempo. |

Default banks contain 24 bins over 20–350, 20–2,000 and 20–15,000 Hz.
`processed` contains AGC-normalized, smoothed energy. `novelty` contains
smoothed deviation from a slow baseline. These arrays can exceed 1; preserve
their values until the effect's output transform.

Legacy flat fields remain: `onset`, `onsetIntensity`, `beatFired`, `downbeat`,
`bpm`, `phase`, `barPhase`, `dt`, `cosPulse`, and the five scalar powers.
`kickFired` is separate from `beatFired`. Boolean events summarize positive
counter deltas; use the deltas to recover several events after a render stall.
Readers keep independent cursors. First reads, real source/profile/epoch resets,
and repeated reads at the same render time add no past events. Temporary
unavailable/reset frames on the same profile preserve elapsed time continuity
without emitting event deltas; recovery establishes a new event baseline.

Flat `bpm` uses 120 for visual motion when `tempo.valid` is false. Flat `dt`
uses that visual BPM and real elapsed seconds. Do not interpret the fallback
as a detected beat. `downbeat` means a counted bar-cycle wrap, not musical
downbeat detection.

`Engine.getAudioLevel()` retains raw RMS scaled to 0..255.
`Engine.getAudioFrequency(index, 3)` maps to low/mid/high.
Other positive counts resample the processed full bank in bin-index space,
then clamp and scale to 0..255. Invalid counts or indices return zero.
The built-in **Audio Spectrum** renders this full bank; it is distinct from
the HUE **Audio Spectrum Bars** algorithm.

## Reference modes and measured boundaries

Select Reference mode on Audio Spectrum Bars, Audio Energy, Audio Barcode
(Scroll), Audio Energy 2 (Wavelength), or Audio Strobe to use the corresponding
reference behavior. These modes retain palette, blur, mirror and brightness
controls. Audio Strobe defaults to detected tempo; its Extrapolated option
matches the reference's clock before tempo acquisition.

Offline comparison executes the external LedFx checkout at revision
`87583f9638fbd749bc0953f35334d8d5140c882f`. The native analysis corpus covers
36 fixtures and 25,320 frames. Native effect comparisons cover 150,000 renders,
plus 19,200 alternate-palette renders, at 24×1, 37×1, 1×9 and 7×11.
The tests check intermediate and final pixels against frozen error limits.

The named distinct-buffer oracle fixes the reference's initial bank
output/state alias at its call boundary. Original traces and mutation controls
remain available. QLC+ copies Spectrum history rather than reproducing the
reference's mutable-array alias. No LedFx implementation belongs in QLC+
source; the reference runner executes that separately licensed checkout.

Quantitative parity does not establish musical preference. A controlled
listening/preview comparison with lawful recordings remains outstanding.

## Runtime measurements

Two ten-minute Debug replays on an Apple M1 Max used profiles 7/29 and four
Reference effects. Analysis p99 was 4.92 ms with diagnostics off and 7.11 ms
with diagnostics on. Four-effect blocking p99 was 17.42/23.30 ms, above the
5 ms budget; some rounds exceeded the 20 ms timer deadline. Their pending-work
counter was sampled only after synchronous rendering, so its zero values do
not establish an in-flight queue bound. The memory-floor checks passed.

An optimized-build probe also exceeded the deadline (433.82 ms maximum)
before its early stop at 190 seconds. These shared-desktop measurements do
not establish runtime acceptance. Retest on a quiet host and profile
synchronous HUE dispatch before claiming the latency budgets.

The correction replay retains the analyzer's default channel alongside both
profiles, schedules analysis and rendering independently, and samples pending
work during rendering. It records wall duration, separate JS-thread CPU time,
per-effect dispatch duration, frame age and actual scheduling timestamps.
Reference-mode convolution loops avoid per-pixel callbacks; display-size updates
and audio rendering use one JS dispatch. The replay also follows the production
timer's overrun reanchor and keeps the GUI event loop responsive.

The final ten-minute Release off/on runs still fail: blocking p99 is
10.694/16.081 ms, with maximum rounds of 477.668/416.904 ms. Analysis p99 is
4.082/9.045 ms. The timer misses 206/284 of the required 30,000 rounds.
Pending HUE work is observed in flight and peaks at one; these observations
do not prove a physical device-buffer bound. Pixel parity remains unchanged,
but the latency budgets are not met.

## Workspace compatibility

Version 2 profiles keep IDs, source/OSC settings and supported nondefault
tuning. New saves include analysis contract revision 3. Older supported
profiles report the changed analysis cadence/window and retained tuning.
Tempo coast/decay fields remain readable but no longer override the native
tempo estimate. Unsupported configurations or future analysis contracts
produce a diagnostic rather than an assumed migration.

HUEMatrix script parameters are persisted by attribute name in workspace XML.
Script list growth can shift internal PatternAttr numeric positions in memory,
so migration tooling and tests should resolve by name rather than relying on
stable numeric indices.

`Artistic`, `Reference`, and the named LedFx modes select built-in algorithm
variants. They are not a user-preset library. Separate named HUEMatrix functions
store separate configurations of the same script, including mode, parameters,
palette and timing. Saving the workspace preserves those settings, not running
particle buffers, history or animation phase.

For Audio Melt, Melt and Sparkle, Blocks, Crawler, Fire, Lava and Water, choose
the named LedFx mode to use the first three palette colors. Artistic mode keeps
its procedural hues and ignores those colors. Fill all three slots for a
multicolor gradient: one assigned color repeats in all slots, while two colors
A/B with the third unset produce A/B/A. A new HUEMatrix starts with red, so its
LedFx output uses red until you change the palette. Audio Energy 2 Reference
continues to use its RGB Palette string.

Float properties may declare bounds with `type:float|values:min,max`. The
RGB/HUE editors and animation controls use finite, increasing bounds; legacy
unbounded float controls retain their existing behavior. Invalid bounds produce
a diagnostic without discarding the property or its authored value.

### Matrix-aware source responses

With both dimensions greater than one, `LedFx Scan Multi` places the three
frequency scanners in separate lanes. Axis now selects travel direction for
this response; it was previously ignored. Fractional cell coverage keeps thin
matrices usable, but can give lanes different peak brightness. Colors still
add where footprints overlap; separated lanes reduce that overlap.

Audio Bands Matrix maps frequency groups to columns and amplitude to height.
Visible groups are capped at the matrix width without dropping any frequency
range. Uneven column groups use floor boundaries, so spare columns go toward
the last groups rather than the strip renderer's first groups. Alternating
fill and gradient direction stays attached to its frequency group when
Flip Band Order is enabled.

These adaptations change saved matrix looks under the same existing choices.
Artistic and single-row/single-column rendering stay unchanged. Shared
flip, mirror and blur remain the existing one-dimensional output transforms.

## Focused 320-pixel performance assessment

A later isolated Release replay used the user's 80x4 WLED layout with Audio
Melt and Audio Scan and Flare. Median Qt rendering time for the pair was
0.393-0.441 ms. Live sampling ranked 2D fixture updates above audio capture,
MasterTimer and script execution. The running engine differed from the replay
build, and host contention made latency tails unstable. These measurements do
not replace the broader runtime-budget results above.

An artifact-only QtQuick comparison used the actual fixture delegates, four
fixtures with 80 heads each. Per-fixture batching reduced median update time
from 0.804 to 0.643 ms with constant intensity and from 1.052 to 0.902 ms with
varying intensity. It won all eight paired comparisons. State, notification
counts and rendered pixels matched for the tested RGB paths.

That is a 14-20% reduction in this isolated update path, not whole-application
CPU savings. No production batching change was made. The benchmark excluded
GPU presentation, live audio, channel extraction and network output.
