# Audio Blueprints — Nested Minimal Flows

**Date:** 2026-11-03
**Status:** Proposed. This file drives execution; `audio-dsp-upgrade-plan.md` stays the long-range reference.

## Summary

The end goal is still: point QLC+ at a folder, it analyzes every track, recognizes what is playing, follows the play position, and lights react to the music's structure (build, drop, key, phrase) instead of only to loudness.

That is too big to build in one go. Instead we build **small nested flows**. Each flow:

- runs end to end, from audio in to a light changing;
- is shippable and demo-able on its own;
- reuses what already exists before adding anything new;
- becomes the base the next flow builds on.

```
Flow A  Live          A1 features reach a light ──▶ A2 real beat (aubio)
                              │
Flow B  Track-aware           ▼
        B1 analyze one file ──▶ B2 follow own playback ──▶ B3 follow VirtualDJ
                                        │
Flow C  Library + recognition           ▼
        C1 folder library ─────────▶ C2 recognize from mic (fingerprint)
```

The key insight from the audit: **we only need fingerprinting when we don't know what's playing.** When QLC+ plays the file itself (B2) or VirtualDJ reports it (B3), the track and position are known exactly. Fingerprinting (C2) is the last step, not the first.

---

## What already exists (consolidated)

### Reusable building blocks

| Block | Where | Why it matters |
| --- | --- | --- |
| `AudioFeatures` + `LiveAudioAnalyzer` + tests | `engine/audio/src/audiofeatures.h`, `liveaudioanalyzer.*`, `engine/test/liveaudioanalyzer/` | The shared feature view exists. Filled by `AudioCapture` (`audiocapture.cpp:409`), **but nothing reads it yet.** |
| aubio static lib | `thirdparty/aubio`, `thirdparty/aubio.cmake` (`29f2cb7f5`) | Builds and runs; not yet linked into `qlcplusaudio`. |
| Audio decoders | `engine/audio/plugins/{mad,sndfile}`, `audiodecoder.h:68-83` | MP3 (libmad) and WAV/FLAC/OGG (libsndfile) with `seek`/`read`. An offline analyzer needs no new decoder. |
| Audio function playback | `engine/audio/src/audio.cpp`, `audiorenderer.cpp:145` | QLC+ knows exactly which file plays and its elapsed time. Renderer read is a tap point for analyzing what QLC+ plays. |
| Beat routing | `inputoutputmap.cpp:1045-1155`, `mastertimer.h:182-218` | Internal / Plugin / Audio beat sources into `MasterTimer::requestBeat()`. Pulse only: no phase, no position. |
| 1/16 beat grid | `function.cpp:553`, `engine/test/beatquantize` | Beat-fraction timing for chasers and sequences; tested. |
| OS2L plugin | `plugins/os2l/os2lplugin.cpp:426-512` | Beat clock works. Parses a `song` event (name, bpm, key, elapsed) but only logs it. Diagnostics page exists. |
| VirtualDJ protocol research | `docs/research/dmx-desktop-virtualdj-protocol.md` | VDJ's WebSocket plugin data gives per deck: `filepath`, `elapsedTime`, `bpm`, Camelot `key`, `beatPosition`, `isMaster`. OS2L alone only gives beats. |
| DJ Expression show | `docs/dj-expression.md` | Chill / Freeze / Build / Drop phase collections, triggered by hand today. **The natural target for detected events.** |
| VCAudioTriggers monitor row | `qmlui/virtualconsole/vcaudiotriggers.cpp:287` | Colored band bars and beat dot. Still does its own DSP on `dataProcessed`. |

### Gaps

1. Nothing consumes `AudioFeatures`; scripts and widgets still read `dataProcessed` / `beatDetected`.
2. No offline analysis, no track profile, no library.
3. No phase or position anywhere; OS2L `elapsed` is logged and dropped.
4. Live onset/tempo quality: 46 ms frames, BeatTracker averages 16 intervals, duplicate FFT, spectral shape from 32 bands.
5. This runner has no Qt dev packages, so end-to-end builds must run elsewhere (CI or a dev machine).

### Contradictions resolved here

| Conflict | Resolution |
| --- | --- |
| `audio-effects-tracker.md` / `audio-effects-plan.md` keep per-script Gain/Floor and `LedFx.*`; DSP plan deletes them | Stop extending per-script DSP. Existing scripts keep working until Flow A migrates them. |
| Three-band split (monitor panel, `ledfx_compat.js`) vs five perceptual bands | Five bands in `AudioFeatures`; three-band `low/mid/high` derived as aliases. |
| DSP plan says OS2L needs a fingerprint to know the track | Wrong for VirtualDJ: its WebSocket reports file path and position. VDJ becomes the first external source (B3). |
| DSP plan sequences recognition before playback-following | Reversed: own playback (B2) and VDJ (B3) come first; fingerprinting is C2. |
| Detected build/drop had no consumer | They fire the DJ Expression phase collections. |
| DSP plan's M0-M5 snapshot says M0 missing | Superseded by this file (aubio landed in `29f2cb7f5`). |

---

## Flow A — Live

### A1. Live features reach a light

**Goal:** a script lights up from `AudioFeatures`, proving the shared view works end to end. No new DSP.

```
mic ─▶ AudioCapture ─▶ LiveAudioAnalyzer ─▶ AudioFeatures ─▶ rgbscriptv4 audio object ─▶ 1 pilot script
```

- **Reuses:** `AudioFeatures`, `LiveAudioAnalyzer`, `AudioCapture::audioFeatures()`, existing script transport in `rgbscriptv4.cpp:567-645`.
- **New:** add `audio.bands {sub,bass,lowMid,mid,high}`, `audio.features {rmsDb, centroidHz, flux, onset}` to the script audio object, alongside the existing fields. Port one script (`audiostrobe.js`) to read them.
- **Done when:** with a mic and music, the pilot script reacts to `audio.bands.sub` and `audio.features.onset`; existing scripts unchanged; liveaudioanalyzer tests still pass.
- **Out of scope:** profiles, channels, widget rewrite, deleting anything.

### A2. Real live beat (aubio)

**Goal:** beat flashes land on the kick, and BPM is stable and has a phase.

```
A1 flow + AudioCapture samples ─▶ aubio onset/tempo ─▶ AudioFeatures {onset, beat, bpm, beatPhase, beatConfidence}
                                                     └─▶ Audio beat source (InputOutputMap) uses aubio beat
```

- **Reuses:** A1, `thirdparty/aubio`, the Audio beat-source path in `InputOutputMap`.
- **New:** link `qlc::aubio` into `qlcplusaudio`; aubio onset + tempo inside `LiveAudioAnalyzer` fed with the mono frame (hop 512, ~11 ms at 44.1 kHz) instead of the 2048 block; `beatPhase`, `beatConfidence` fields. BeatTracker stays as fallback when `audio_aubio=OFF`.
- **Done when:** a 128 BPM test track reports 128 ± 0.5 within 4 s; synthetic kick-impulse test fires one onset per kick; onset latency measured and logged.
- **Out of scope:** pitch, MFCC, chroma, buffer tuning per platform backend.

---

## Flow B — Track-aware

### B1. Analyze one file

**Goal:** turn one MP3 into an inspectable track profile.

```
file.mp3 ─▶ AudioDecoder (libmad/sndfile) ─▶ aubio offline pass ─▶ track profile JSON
```

- **Reuses:** decoder plugins, aubio, the band/energy maths in `LiveAudioAnalyzer`.
- **New:** `TrackAnalyzer` (engine) + a small CLI `qlcplus-audio-analyze <file>` writing `<file>.qlcaudio.json`: duration, BPM, beat times, onset times, per-100 ms energy in five bands, and **coarse build/drop candidates** from the energy envelope (rising energy over N bars, then a full-band jump after a dip).
- **Done when:** on 5 hand-checked EDM tracks, BPM matches the label ±0.5 and at least 4 of 5 marked drops are found within ±1 bar.
- **Out of scope:** key, chroma, fingerprints, SQLite, Essentia. A JSON file per track is enough to prove the profile is useful.

### B2. Follow our own playback

**Goal:** when QLC+ plays a track, lights know the drop is coming. This is the first real "magic" demo.

```
Audio function plays file ─▶ load file's profile ─▶ position = Audio function elapsed time
                                                 ─▶ CachedAudioAnalyzer ─▶ AudioFeatures {source=Cached, events.drop, events.build, barPhase}
                                                 ─▶ event ─▶ DJ Expression phase collection (Build / Drop)
```

- **Reuses:** B1 profiles, A1 feature view, Audio function elapsed time (exact, no detection), DJ Expression collections.
- **New:** `CachedAudioAnalyzer` that serves `AudioFeatures` from a profile at a position; a minimal event→function binding (event name → Function ID) so a drop starts the Drop collection. Analyze on first play if no profile exists.
- **Done when:** playing an analyzed track from a Show or Audio function, the Drop collection fires within one beat of the drop, and the Build collection starts at the build.
- **Out of scope:** external DJ software, mic recognition, library browser.

### B3. Follow VirtualDJ

**Goal:** same as B2, but the DJ plays in VirtualDJ.

```
VirtualDJ ─▶ (WebSocket plugin data, or VDJ script → OS2L `song` with elapsed) ─▶ filepath + elapsed + bpm + key
          ─▶ PositionSource ─▶ load profile by path ─▶ B2 pipeline
```

- **Reuses:** B2 pipeline, OS2L plugin's existing `song` parser and diagnostics, the VDJ protocol research.
- **New:** a `PositionSource` interface with two implementations: own playback (B2) and VirtualDJ. Latency offset setting. VDJ `key` and `bpm` override the profile when present.
- **Done when:** with VDJ playing an analyzed track, drops fire the Drop collection on time, including after a seek and with ±6% pitch.
- **Out of scope:** Pro DJ Link, StagelinQ, fingerprinting.

---

## Flow C — Library and recognition

### C1. Folder library

**Goal:** point at a folder, everything gets analyzed in the background.

```
folder ─▶ scan (hash per file) ─▶ B1 analyzer in a worker pool ─▶ SQLite library ─▶ B2/B3 load by hash or path
```

- **Reuses:** B1 analyzer, B2/B3 lookups.
- **New:** SQLite store replacing per-file JSON; progress and per-track status in the UI; optional Essentia pass (key, structure) behind `audio_essentia`.
- **Done when:** 500 files index unattended, survive restarts, and B2/B3 find profiles after files move (hash match).

### C2. Recognize from the mic

**Goal:** no cables, no QLC+ playback: lights still follow the track.

```
mic ─▶ Olaf one-shot ID (rolling 5 s) ─▶ track + start position ─▶ clock + periodic re-check ─▶ PositionSource ─▶ B2 pipeline
```

- **Reuses:** C1 library (add fingerprints at index time), B3 `PositionSource`, A2 live beat for drift correction.
- **New:** Olaf vendored; fingerprints in SQLite; `AudioIdentifier`. Chroma position tracker only if drift under DJ pitch-bend proves to be a problem.
- **Done when:** a known track is identified within 3 s from the mic and drops fire within one beat.

---

## Order and decision gates

1. **A1** — smallest possible proof that the feature view reaches a light.
2. **B1** — prove a profile can find drops. **Gate:** if drop candidates are unusable, fix the analysis before B2.
3. **B2** — the first real demo. **Gate:** show it to an operator; if it doesn't feel right, iterate here before going further.
4. **A2** and **B3** — in either order, by need (mic shows vs VirtualDJ shows).
5. **C1**, then **C2**.

Everything in `audio-dsp-upgrade-plan.md` that is not named here (AudioProfile/AudioChannel, VCAudioTrigger rewrite, script port, legacy deletion, Pro DJ Link, StagelinQ, MCP tools) is deferred until a blueprint needs it.
