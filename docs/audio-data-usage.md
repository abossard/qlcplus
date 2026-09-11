# Audio analysis and script data

QLC+ supplies the same audio view to the HUEScript fifth argument and
`Engine.getAudioSnapshot()` in v5 Script functions. Its `version` is 6.
HUEScript's separate script `apiVersion` remains 3. The 41 shipped audio
algorithms retain their names and Artistic defaults.

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
Readers keep independent cursors. First reads, source resets and repeated reads
at the same render time add no past events. Recovery after unavailable input
establishes a new event baseline, including in the diagnostic timeline.

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
