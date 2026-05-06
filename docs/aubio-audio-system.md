# Aubio Audio System

End-to-end reference for the audio analysis pipeline that powers the Audio
Triggers virtual-console widget and audio-reactive RGB scripts.

> Status as of this document: aubio is the **only** path. The legacy FFTW-based
> spectrum / BeatTracker / AGC compressor have been removed or stubbed out.

## 1. Pipeline overview

```mermaid
flowchart LR
    A[QAudioSource<br/>(Qt Multimedia)] --> B[AudioCapture<br/>(QThread)]
    B --> C[AubioProcessor<br/>(per-block aubio)]
    C --> D[AudioFrame<br/>(rms/peak/dB + AubioResults*)]
    D --> E[AudioAnalyzer]
    E --> F[AudioChannel.update<br/>(per-profile DSP)]
    F --> G[AudioSnapshot<br/>(mutex-guarded)]
    G --> H1[VCAudioTriggers<br/>(slotAubioDataReady)]
    G --> H2[RGBScript v3<br/>(audio object)]
    G --> H3[RGBAudio<br/>(spectrum bars)]
```

- **Capture thread** (AudioCapture) owns the audio-side state. It mixes input
  channels to mono, computes RMS/peak/dB, runs `AubioProcessor::process`, builds
  one `AudioFrame`, and synchronously calls `AudioAnalyzer::processFrame`.
  AudioAnalyzer fans the frame out to every registered `AudioChannel`, each of
  which runs its own profile-specific DSP (input gain, envelope follower,
  noise gate, perceptual band aggregation, hysteresis triggers) and atomically
  swaps a fresh `AudioSnapshot` into place under a mutex.
- After the synchronous channel work is done, AudioCapture emits
  `aubioDataReady(AubioResults, power)`. Consumers may then read each channel's
  snapshot — it is guaranteed to be the one built from the same frame.

## 2. Source map

| File | Role |
| --- | --- |
| `engine/audio/src/audiocapture.{h,cpp}` | Capture thread, mixdown, RMS/peak, refcount-based start/stop, signal emission. |
| `engine/audio/src/aubioprocessor.{h,cpp}` | Wraps aubio: pitch, onset (9 detectors), tempo, mel filterbank, MFCC, spectral centroid/spread/rolloff/flux/HFC. 512-sample hop, 1024 window, 4 hops per 2048-sample buffer. |
| `engine/audio/src/aubioresults.h` | `AubioResults` plain struct populated by `AubioProcessor`. |
| `engine/audio/src/audioframe.h` | One block of analyzed audio. Carries a non-owning pointer to `AubioResults`. |
| `engine/audio/src/audioanalyzer.{h,cpp}` | Fans frames to N `AudioChannel` instances. |
| `engine/audio/src/audiochannel.{h,cpp}` | Per-profile DSP and snapshot builder. |
| `engine/audio/src/audiochannelconfig.{h,cpp}` | All tuning constants for one channel: envelope, triggers, band layout, noise gate, volume smoothing, brightness floor, `AubioConfig`. |
| `engine/audio/src/audiosnapshot.h` | The published-out result. |
| `engine/src/audioprofile.{h,cpp}` | Persistable wrapper around `AudioChannelConfig`; binds an `AudioChannel` into the analyzer. |
| `qmlui/virtualconsole/vcaudiotriggers.{h,cpp}` | Audio-Triggers widget: spectrum bars, DMX/Function/Widget triggers, profile editor backend. |
| `qmlui/qml/virtualconsole/VCAudioTriggersItem.qml` | Widget runtime UI. |
| `qmlui/qml/virtualconsole/VCAudioTriggersProperties.qml` | Edit panel UI. |
| `engine/src/rgbscriptv4.cpp` | Builds the v3 `audio` object passed to RGB scripts (`buildAudioDataObject`). |
| `engine/src/rgbaudio.cpp` | Built-in "Audio Spectrum" RGB algorithm — consumes mel directly. |
| `resources/rgbscripts/audio_common.js` | Shared helpers for audio-reactive scripts. |

## 3. AudioSnapshot field reference

All fields are written by `AudioChannel::update` under `m_mutex`. Consumers
must call `AudioChannel::snapshot()` to obtain a copy.

| Field | Type | Source | Notes |
| --- | --- | --- | --- |
| `mel[40]` | `double` | aubio mel filterbank (Slaney) | linear ~0..1 powers |
| `mfcc[13]` | `double` | aubio | published to scripts; no widget UI |
| `bands.sub` / `.bass` / `.lowMid` / `.mid` / `.high` | `double` | mel summed by `BandLayout` Hz cuts | 0..1 with envelope follower |
| `bands.low` | `double` | `(sub+bass)/2` | convenience |
| `triggers[5]` | `TriggerState` | per-band hysteresis on `bands.*` vs `Triggers.{low,high,hold,cooldown}` | indices: sub=0, bass=1, lowMid=2, mid=3, high=4 |
| `volumeTrigger` / `beatTrigger` | `TriggerState` | volume RMS / aubio beat | |
| `volume.raw` / `.smoothed` / `.normalized` | `double` | from frame RMS, smoothed by `volumeSmoothingMs` | |
| `features.rmsDb` / `.peakDb` / `.crestFactor` | `double` | from frame | |
| `features.centroidHz` / `.spread` / `.rolloffHz` / `.flux` / `.hfc` | `double` | aubio specdesc | only `flux` exposed in widget UI |
| `music.beat` / `.bpm` / `.beatConfidence` / `.tatum` | `bool/double` | aubio tempo | `beatPhase` is declared but **not currently populated**. |
| `pitch.hz` / `.confidence` | `double` | aubio pitch (yinfft) | |
| `note.midi` / `.velocity` / `.noteOn` / `.noteOff` | int/bool | aubio notes | exposed to scripts; no widget UI |
| `onsets.{energy,hfc,complex_,phase,wphase,specdiff,kl,mkl,specflux}` | `bool` | per-method onset firings ORed across hops | |
| `onsets.voteCount` | `int` | sum of the nine flags | this is what the widget exposes as "onset vote count" |
| `audioDtMs` | `double` | block duration | for time-based math in scripts |
| `brightnessFloor` | `double` | echoes config | floor to keep scripts above black |
| `noiseGateClosed` | `bool` | gate state | when true, snapshot bands are zero |

## 4. Configuration model

`AudioChannelConfig` is the single source of truth. `AudioProfile` persists it
to XML and binds an `AudioChannel` into the analyzer. The Audio-Triggers widget
edits the *current profile*; changes propagate via `AudioProfile::setChannelConfig`
and are immediately picked up by the running channel.

### Persisted keys (audioprofile.cpp)

```text
<AudioProfile ID="..." Name="..." IsDefault="..." Version="1">
  <Envelope Attack="…" Release="…" />
  <Agc InputGain="…" />          # XML element name kept for compat;
                                 # only InputGain is meaningful now.
  <Triggers High="…" Low="…" Hold="…" Cooldown="…" />
  <Bands SubMax="…" BassMax="…" LowMidMax="…" MidMax="…" HighMax="…" />
  <NoiseGate Threshold="…" Hold="…" />
  <Volume Smoothing="…" BrightnessFloor="…" />
  <Aubio OnsetThreshold="…" OnsetMinInterval="…" PitchMethod="…"
         PitchSilenceDb="…" PitchTolerance="…"
         TempoMinBpm="…" TempoMaxBpm="…" TatumSubdivision="…"
         TssAlpha="…" TssBeta="…" TssThreshold="…" />
</AudioProfile>
```

### Currently a no-op (config exists but `AubioProcessor` ignores it)

`AubioProcessor` is hard-wired:

- `pitch_method` = `"yinfft"`
- `pitch silence` = `-40 dB`
- `tempo method` = `"default"`, default BPM range
- TSS allocated and called every hop, but `transGrain`/`steadGrain` outputs
  are not consumed and `tssAlpha/Beta/Threshold` are never pushed in.

The QML edit panel reads/writes these fields; they round-trip through XML, but
they have no audible effect today. Wiring them through is a follow-up.

## 5. Signals and slots

`AudioCapture` (`audiocapture.h`):

- `aubioDataReady(const AubioResults &results, quint32 power)` — primary
  per-block signal. Emitted **after** the analyzer has updated all channels'
  snapshots, so consumers can safely call `channel->snapshot()`.
- `volumeChanged(int power)` — emitted only when the smoothed power changes.
  Used by the widget to repaint the volume LED quickly.
- `beatDetected()` — fired when `aubio.beat == true` for the current block.

The widget connects all three in `setCaptureEnabled(true)` and disconnects on
disable. The capture thread is refcounted via
`registerBandsNumber/unregisterBandsNumber`; the `int` argument is ignored —
only the start/stop pairing matters.

> ⚠️ **Removed**: `AudioCapture::dataProcessed(double*, int, double, quint32)`.
> Earlier code in `qmlui/virtualconsole/vcaudiotriggers.cpp` used a Qt4-style
> `SIGNAL(...)` connect to this; it silently failed at runtime, so the entire
> bar / DMX / function-trigger path was dead. The widget now uses
> `slotAubioDataReady(const AubioResults&, quint32)` and resamples `mel[40]`
> into the configured perceptual bar count.

## 6. RGB script v3 contract

`RGBScript::buildAudioDataObject()` constructs the `audio` object on the JS
engine thread once per script step. Shape:

```js
audio = {
  mel:       Float[40],     // ~0..1 mel powers (Slaney)
  mfcc:      Float[13],
  bands:     { sub, bass, low, lowMid, mid, high },
  triggers:  { sub, bass, lowMid, mid, high, volume, beat }
                            // each: { firedThisFrame, active, releasedThisFrame,
                            //         heldMs, cooldownRemainingMs }
  volume:    { raw, smoothed, normalized },
  music:     { beat, bpm, beatPhase, beatConfidence, tatum },
  features:  { rmsDb, peakDb, crestFactor,
               centroidHz, spread, rolloffHz, flux, hfc },
  onsets:    { energy, hfc, complex, phase, wphase,
               specdiff, kl, mkl, specflux, voteCount },
  pitch:     { hz, confidence },
  note:      { midi, velocity, noteOn, noteOff },
  audioDtMs, brightnessFloor, noiseGateClosed, consumerDtMs
}
```

Scripts opt in via `algo.usesAudio = true`. The matrix-owning script's
`AudioProfile` (looked up via `Doc::audioProfileForFunction`) supplies the
snapshot; if no profile is bound, a default-constructed snapshot is used so
the shape is always present.

`audio_common.js` exposes a tiny `AudioParams` helper to seed per-script
preset values; all heavy DSP lives in C++.

## 7. VCAudioTriggers widget

Spectrum bars (`m_spectrumBars`):

- Index `0` is the **volume bar**, driven directly by AudioCapture's smoothed
  power.
- Indices `1..N` are **perceptual bars**, derived by log-frequency resampling
  the snapshot's `mel[40]` array into N bins (with a small temporal
  smoothing). Each bar carries a type (`None`, `DMXBar`, `FunctionBar`,
  `VCWidgetBar`) and min/max thresholds.

Trigger logic (per-frame, in `slotAubioDataReady`):

- `FunctionBar`: starts the bound Function when `bar.value >= maxThreshold`,
  stops it when `bar.value < minThreshold`.
- `VCWidgetBar`: forwards to `checkWidgetFunctionality` which delegates to
  Button / Slider / Cuelist / SpeedDial widget actions.
- `DMXBar`: handled by the registered DMX source on the master timer
  (untouched by this slot).

Lows / Mids / Highs aggregate Q_PROPERTYs are computed from the snapshot's
perceptual bands directly:

- `lowsPower  = (bands.sub  + bands.bass)   / 2`
- `midsPower  = (bands.lowMid + bands.mid)  / 2`
- `highsPower =  bands.high`

## 8. Threading

- `AudioCapture::processData` runs on the capture thread; it mutates the
  channel snapshots under the channel's mutex and **then** emits signals.
- Qt's auto-connection means slots in QObjects living on the GUI thread are
  invoked via queued connection. By the time the slot runs, the snapshot the
  signal was emitted "for" is still readable, but a newer one may have been
  swapped in. Consumers always read the *latest* snapshot — this is intentional
  and matches "best-effort, drop-old" semantics.
- `RGBScript::buildAudioDataObject` runs on the JS engine thread and obtains
  the snapshot the same way.

## 9. Dead / no-op surface area (called out for follow-up)

| Item | Where | Status |
| --- | --- | --- |
| `BeatTracker` class | `engine/audio/src/beattracker.{h,cpp}` | Dead — not in `CMakeLists.txt`, no users. Safe to delete. |
| TSS path in aubio | `aubioprocessor.cpp` | Allocated + called, outputs unused. Either consume or delete. |
| `AubioConfig` → processor wiring | — | XML round-trips, UI edits, but `AubioProcessor` ignores them all. Either wire up or hide UI. |
| `music.beatPhase` | `audiosnapshot.h` | Field declared; never populated. Q_PROPERTY exists. |
| `mfcc`, `note.*`, `features.{centroidHz,spread,rolloffHz,hfc}` | snapshot | Published to scripts; no widget UI. |
| `bands.low` | snapshot | Published; no widget UI. |
| Individual `onsets.*` flags | snapshot | Published; widget only consumes `voteCount`. |
| `AudioCapture::bandMagnitude` / `bandMaxMagnitude` | `audiocapture.h` | Returns `0.0`; kept for binary compatibility. No external consumers. |

## 10. Known gaps in tooltips

The widget surface (`VCAudioTriggersItem.qml`) has one tooltip — the
enable/disable IconButton. The Edit Panel
(`VCAudioTriggersProperties.qml`) is largely tooltip-less. The "Input"
SectionBox now ships with one tooltip (`Input gain`); see that block as the
canonical pattern (`ToolTip.visible: hovered`, `ToolTip.text: qsTr(...)`).

Sections that should grow tooltips, in priority order:

1. **Triggers** — `High`, `Low`, `Hold`, `Cooldown`
2. **Envelope** — `Attack`, `Release`
3. **Noise Gate** — `Threshold`, `Hold`
4. **Band Layout** — `Sub max`, `Bass max`, `Low-mid max`, `Mid max`, `High max`
5. **Volume Response** — `Smoothing`, `Brightness floor`
6. **Onset / Pitch / Tempo / Spectral** sections (only relevant once
   `AubioConfig` is actually plumbed into `AubioProcessor`).

Tooltip copy should be lighting-friendly, not DSP jargon (e.g.
*"How fast bands rise on a hit. Lower = snappier."*).
