# Audio DSP Modernization Plan

## Goal

Replace QLC+'s minimal audio analysis (32 log bands + spectral-flux beat detector, mic-only) with a scientifically grounded pipeline that powers RGB scripts, Audio Trigger widgets, and AI-driven cue generation, and that is musically aware enough for EDM stage lighting.

The pipeline must:

1. Pre-analyze any audio file in the user's library and cache rich features (BPM, beat grid, key/chroma, multi-band envelopes, structural drops, spectral shape).
2. Recognize what is currently playing on stage from the live mic/line input via acoustic fingerprinting, lock onto playback position, and replay the cached features in sync — even when the DJ has applied EQ, pitch shift, or tempo bend.
3. Fall back to live analysis when no fingerprint match exists.
4. Drive scripts, widgets, and MCP tools from a single uniform `AudioFeatures` view, regardless of whether the source is live or cache-replayed.

This project is young. **No backwards compatibility is required** — old XML, old per-script DSP, and the bundled `ledfx_compat.js` shim can all be removed. Simple and high quality wins over preservation.

## End State

| Area | Final direction |
| --- | --- |
| Offline analysis | **Essentia** (AGPL-3.0 accepted) computes BPM, beat/bar grid, key, chroma, multi-band envelopes, structural drops, danceability, MFCC. Run once per file. |
| Fingerprinting | **Olaf** (GPL-3.0, native C) builds a constellation-hash index. Used live to recognize the playing track and lock onto position with ~100 ms accuracy. |
| Live analysis | **aubio** (GPL-3.0) computes onsets, tempo, multi-band on the live capture stream. Used as a fallback when no fingerprint match is locked. |
| Storage | A single SQLite database holds tracks, features, fingerprint hashes, and analyzer profiles. No per-file sidecars, no project-embedded blobs. |
| Decode | Qt 6 `QAudioDecoder` (FFmpeg backend) handles MP3/WAV/FLAC/OGG/M4A across platforms. ffmpeg is bundled with the app. |
| Core feature view | One `AudioFeatures` struct is read by all consumers. Computed live by `LiveAudioAnalyzer` or replayed from the cache by `CachedAudioAnalyzer`. |
| Audio Profiles | Document-level `AudioProfile` objects hold per-channel envelope/AGC/trigger configuration. Multiple named profiles per project. |
| Audio Trigger Widget | Rebuilt as the audio control center: library browser, recognition-lock badge, key indicator, drop/build lamps, perceptual band editor, envelope/AGC/trigger/spectral panels. |
| Scripts | Read enriched `audio` object (`audio.bands.*`, `audio.triggers.*`, `audio.music.bpm`, `audio.music.key`, `audio.events.drop`, `audio.match.locked`). No DSP in JS. |
| JS helpers | `RGBUtil` namespace for color/map/noise. `ledfx_compat.js` and `audio_common.js` deleted. |
| MCP | Tools for batch analysis, feature lookup, recognition state, and event subscription. |
| Verification | Synthetic injection tests, deterministic feature tests, FMA/Jamendo CC corpus benchmarks for BPM, key, drop accuracy and recognition lock latency. |

## Architecture

```
┌──────────────────────────────────────────────────────────────┐
│  Offline (one-shot per file, batch over the user library)    │
│                                                              │
│  Audio file ──▶ QAudioDecoder ──▶ PCM                        │
│                                    ├─▶ Essentia ──▶ Features │
│                                    └─▶ Olaf ─────▶ Hashes    │
│                                              │       │       │
│                                              ▼       ▼       │
│                                       ┌─────────────────┐    │
│                                       │  SQLite (single │    │
│                                       │  audio.db file) │    │
│                                       └─────────────────┘    │
└──────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────┐
│  Live                                                        │
│                                                              │
│  Mic / line ──▶ AudioCapture ──▶ AudioFrame                  │
│                                    │                         │
│                       ┌────────────┼────────────┐            │
│                       ▼            ▼            ▼            │
│                 LiveAnalyzer  AudioMatcher  (fingerprint     │
│                 (aubio)       (Olaf)         streaming)      │
│                       │            │                         │
│                       │     match  │                         │
│                       │            ▼                         │
│                       │      track + position_ms             │
│                       │            │                         │
│                       │            ▼                         │
│                       │   CachedAnalyzer (reads SQLite,      │
│                       │   replays features at position)      │
│                       │                                      │
│                       └────────┐    ┌─────────┐              │
│                                ▼    ▼                        │
│                          AudioFeatures (unified view)        │
│                                │                             │
│                  ┌─────────────┼──────────────┐              │
│                  ▼             ▼              ▼              │
│         AudioProfile     RGBMatrix       VCAudioTrigger      │
│         (channels)       (scripts)       (UI / triggers)     │
└──────────────────────────────────────────────────────────────┘
```

## Library Responsibilities

| Library | Mode | Computes | Why |
| --- | --- | --- | --- |
| **Essentia** | Offline | BPM (RhythmExtractor2013), beat/bar grid, key + scale (KeyExtractor / HPCP), chroma, danceability, multi-band mel, spectral centroid/rolloff/flatness, MFCC, structural segmentation, drop candidates from novelty curve | Single library covers nearly all wanted features. AGPL accepted. |
| **Olaf** | Offline (index) + Live (match) | Constellation-hash fingerprints, continuous matching, locked `(track_id, position_ms)` | Native C, embedded-friendly, robust to EQ/noise/moderate pitch shift, ~100 ms position resolution, sub-second lock. |
| **aubio** | Live | Real-time onsets, tempo, pitch, multi-band — sub-10 ms latency | Fast fallback while no fingerprint match is locked. Mature C API. |
| **Qt Multimedia** | Offline + Live | MP3/WAV/FLAC/OGG/M4A decoding via FFmpeg | Already in tree. ffmpeg bundled with the app for cross-platform consistency. |
| **SQLite** (Qt `QSqlDatabase` with `QSQLITE` driver) | Storage | Tracks, features, fingerprint hashes, profiles | Already a transitive Qt dependency. Single file. Inspectable. |

## Single SQLite Schema (`~/.local/share/qlcplus/audio.db`)

```sql
-- One row per analyzed file
CREATE TABLE tracks (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    path            TEXT NOT NULL,                -- absolute path or library-relative
    sha256          TEXT NOT NULL UNIQUE,         -- content hash for re-detection across moves
    duration_ms     INTEGER NOT NULL,
    sample_rate     INTEGER NOT NULL,
    channels        INTEGER NOT NULL,
    title           TEXT,
    artist          TEXT,
    bpm             REAL,
    key             TEXT,                         -- e.g., "F# minor"
    danceability    REAL,
    analyzed_at     INTEGER NOT NULL,             -- unix epoch
    analyzer_version INTEGER NOT NULL
);
CREATE INDEX tracks_sha ON tracks(sha256);
CREATE INDEX tracks_path ON tracks(path);

-- Per-frame features (one row per ~23 ms frame)
CREATE TABLE feature_frames (
    track_id        INTEGER NOT NULL REFERENCES tracks(id) ON DELETE CASCADE,
    frame_index     INTEGER NOT NULL,
    time_ms         INTEGER NOT NULL,
    rms_db          REAL,
    centroid_hz     REAL,
    rolloff_hz      REAL,
    flatness        REAL,
    flux            REAL,
    bands_blob      BLOB,                         -- 5 perceptual band floats, packed
    PRIMARY KEY (track_id, frame_index)
);
CREATE INDEX ff_time ON feature_frames(track_id, time_ms);

-- Discrete beat events
CREATE TABLE beats (
    track_id        INTEGER NOT NULL REFERENCES tracks(id) ON DELETE CASCADE,
    time_ms         INTEGER NOT NULL,
    beat_index      INTEGER NOT NULL,             -- position in track
    bar_index       INTEGER,                      -- nullable until downbeats settled
    confidence      REAL,
    PRIMARY KEY (track_id, beat_index)
);
CREATE INDEX beats_time ON beats(track_id, time_ms);

-- Onsets (kicks, snares, etc.) for fast trigger replay
CREATE TABLE onsets (
    track_id        INTEGER NOT NULL,
    time_ms         INTEGER NOT NULL,
    band            TEXT NOT NULL,                -- 'sub'|'bass'|'lowMid'|'mid'|'high'
    strength        REAL,
    PRIMARY KEY (track_id, time_ms, band)
);

-- Structural events (build start, drop, breakdown, outro)
CREATE TABLE structural_events (
    track_id        INTEGER NOT NULL,
    time_ms         INTEGER NOT NULL,
    kind            TEXT NOT NULL,                -- 'build'|'drop'|'break'|'outro'
    confidence      REAL,
    PRIMARY KEY (track_id, time_ms, kind)
);

-- Olaf fingerprint hashes
CREATE TABLE fingerprints (
    hash            INTEGER NOT NULL,             -- Olaf 64-bit packed hash
    track_id        INTEGER NOT NULL REFERENCES tracks(id) ON DELETE CASCADE,
    time_ms         INTEGER NOT NULL,
    PRIMARY KEY (hash, track_id, time_ms)
);
CREATE INDEX fp_hash ON fingerprints(hash);

-- AudioProfile DSP configuration (replaces XML embedding for shareability;
-- still mirrored to .qxw for project portability)
CREATE TABLE audio_profiles (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    name            TEXT NOT NULL,
    is_default      INTEGER NOT NULL DEFAULT 0,
    config_json     TEXT NOT NULL                 -- AudioChannelConfig serialized
);
```

The `feature_frames` BLOB packs 5 little-endian floats (sub/bass/lowMid/mid/high). Per-track storage at 23 ms frames is ~30 kB / minute, ~150 MB for 5000 four-minute tracks — acceptable on modern disks.

## AudioFeatures (Unified View)

```cpp
struct AudioFeatures {
    // Source
    enum Source { Live, Cached } source;
    qint64 trackId = -1;       // valid when source == Cached
    double positionMs = 0;     // 0 when source == Live

    // Loudness
    float rmsDb;
    float peakDb;
    float crestFactor;

    // Spectrum
    std::array<float, 32> bandsLog;
    std::array<float, 32> bandsDb;
    std::array<float, 32> bandsNormalized;

    // Perceptual bands (post-envelope, AGC-adjusted)
    struct PerceptualBands {
        float sub, bass, lowMid, mid, high;
    } bands;

    // Spectral shape
    float spectralFlux;
    float spectralCentroidHz;
    float spectralRolloffHz;
    float spectralFlatness;

    // Music
    struct Music {
        float bpm;
        float beatPhase01;       // 0..1 within current beat
        float barPhase01;        // 0..1 within current bar (cached only)
        int   beatIndex;         // -1 live
        int   barIndex;          // -1 live
        float beatConfidence;
        QString key;             // e.g. "F# minor", empty live
        float keyConfidence;
    } music;

    // Discrete events for the current frame
    struct Events {
        bool onset;
        bool beat;
        bool drop;               // structural drop
        bool buildStart;
        bool breakStart;
    } events;

    // Recognition state
    struct Match {
        bool   locked;
        qint64 trackId;
        double positionMs;
        float  confidence;
        float  driftMs;          // estimated drift between live and locked timeline
    } match;

    // Frame timing
    quint64 frameIndex;
    double  audioDtMs;
};
```

`AudioFeatures` is produced by `LiveAudioAnalyzer` or `CachedAudioAnalyzer` and consumed by `AudioChannel`s, scripts, and the widget.

## C++ Analysis Layer

### Internal Frame (AudioCapture → analyzers)

```cpp
struct AudioFrame {
    const float* mono;
    int sampleCount;
    const float* fftMagnitudes;
    int fftBins;
    int sampleRate;
    double rms;
    double peak;
    quint64 frameIndex;
    double dtMs;
};
```

### LiveAudioAnalyzer (aubio + Essentia-online subset)

Runs on the audio thread. Computes shared spectrum/loudness features and feeds aubio for onsets/tempo. Emits `AudioFeatures` with `source = Live`, `match.locked = false` initially.

### AudioMatcher (Olaf)

Owns an `olaf_db` view of the SQLite fingerprints table and a streaming matcher. On every `AudioFrame`, feeds samples into Olaf. When confidence crosses threshold, emits `(trackId, positionMs)` and switches the `AudioFeatures` source to `Cached`. On loss of confidence, switches back to `Live`.

### CachedAudioAnalyzer

Given a locked `(trackId, positionMs)`, queries SQLite for the surrounding feature frames, beats, onsets, and structural events. Reconstructs an `AudioFeatures` view at the play position, with key, BPM, drop events, and bar phase already populated. Position advances at audio-frame rate.

### AudioChannel (per-profile)

Same handle-based API as in the prior plan. Reads `AudioFeatures`, applies envelope/AGC/triggers per `AudioChannelConfig`, exposes immutable `AudioSnapshot`.

```cpp
struct AudioChannelConfig {
    EnvelopeConfig envelope;   // attack/release per band, ms
    AgcConfig      agc;        // maxGainDb, releaseMs, noiseFloorDb
    TriggerConfig  triggers;   // Schmitt thresholds, hold, cooldown
    BandLayout     bandLayout; // perceptual 5 by default
    VolumeConfig   volume;
};

AudioChannelHandle h = analyzer->createChannel(config);
h.updateConfig(newConfig);   // applied at next frame boundary
AudioSnapshot snap = h.snapshot();
h.close();
```

### AudioLibraryIndexer (offline)

Walks a directory tree, runs each new file through Essentia + Olaf in a `QThreadPool`, writes results to SQLite in a single transaction per track. Skips files whose `sha256` already exists. Reports progress to UI.

## Perceptual Bands

Five named groups, mapped from the existing 32 log-spaced bands and verified against sine sweeps. Names and ranges live in a single header constant, not in scripts.

| Group | Index range | Frequency intent |
| --- | --- | --- |
| `sub` | 0..8 | Kick fundamental, sub pressure |
| `bass` | 9..12 | Bass body |
| `lowMid` | 13..18 | Warmth, body |
| `mid` | 19..25 | Vocals, snare body |
| `high` | 26..31 | Hats, clap edge |

## JS Layer

JS consumes pre-computed values. No DSP in JS.

```javascript
function rgbMap(width, height, rgb, step, audio) {
    var sub = audio.bands.sub;
    var fired = audio.triggers.bass.firedThisFrame;
    if (audio.events.drop) { /* hard hit */ }
    if (audio.match.locked) {
        var key = audio.music.key; // e.g. "F# minor"
        var bar = audio.music.barPhase01;
    }
}
```

| API | Purpose |
| --- | --- |
| `RGBUtil.rgb(r,g,b)` | Color packing |
| `RGBUtil.hsv2rgb(h,s,v)` | HSV conversion |
| `RGBUtil.createMap(w,h)` | Pixel map allocation |
| `RGBUtil.interpolate(a,b,t)` | Linear interpolation |
| `RGBUtil.simplex2d(x,y)` | Simplex noise |
| `RGBUtil.noiseField2d(...)` | Noise field |
| `AudioDSP.Filter(decayMs, riseMs)` | Optional per-pixel exponential smoothing |

`ledfx_compat.js` and `audio_common.js` are deleted.

## VCAudioTrigger (Full Rewrite)

The widget is rebuilt as the audio control center. Layout:

```
┌────────────────────────────────────────────────────────────┐
│ [Profile ▼]  [● Live]  [♪ "Strobe" — 1:23.4 lock 92%]      │ ← header
├────────────────────────────────────────────────────────────┤
│ ┌──────────┐ Bands  Envelope  AGC  Triggers  Spectral      │ ← tabs
│ │ Library  │                                               │
│ │ ▸ Folder │ ┌── live monitor strip ─────────────────┐     │
│ │ ▸ Track  │ │ [sub bass lowMid mid high]            │     │
│ │   3 unan │ │ envelope curves · AGC gain · lamps   │     │
│ │ Index ▶  │ └───────────────────────────────────────┘     │
│ └──────────┘ ┌── tab content ────────────────────────┐     │
│              │ band edges, envelope sliders, etc.   │     │
│              └───────────────────────────────────────┘     │
├────────────────────────────────────────────────────────────┤
│ Drop ◯  Build ◯  Break ◯  Key: F# min  BPM: 128  Bar: 3.4 │ ← status
└────────────────────────────────────────────────────────────┘
```

Panels:

- **Library** — browse and index a folder of audio files. Shows progress, sha mismatches, missing files. "Re-analyze" per track.
- **Bands** — perceptual band edge editors with frequency labels.
- **Envelope** — per-band attack/release sliders, live mini-graph.
- **AGC** — max gain dB, release ms, noise floor dB, live gain meter.
- **Triggers** — per-band Schmitt thresholds, hold/cooldown ms, lamp + fires/sec.
- **Spectral** — live centroid, rolloff, flatness readouts.
- **Status strip** — drop/build/break lamps, key indicator, BPM, bar phase.
- **Recognition badge** — green when `match.locked`, shows track name and position.

The legacy per-bar trigger UI is removed. Per-band Schmitt triggers replace it.

## Math Standards

| Area | Requirement |
| --- | --- |
| Time constants | `alpha = 1 - exp(-dtMs / tauMs)`. No frame-count fade math. |
| AGC | dB-domain envelope, capped max gain, configurable noise floor. |
| Triggers | Adaptive baseline + Schmitt hysteresis + hold + cooldown. |
| Onsets | aubio default (HFC or specdiff) live; Essentia OnsetDetectionGlobal offline. |
| Beat | aubio tempo live; Essentia RhythmExtractor2013 offline (more accurate, slower). |
| Key | Essentia KeyExtractor with EDMA profile. |
| Drop | Essentia novelty curve + percussive/tonal split + low-frequency energy ramp. Heuristic: rising flux + rising rms over 4–16 bars, then sudden full-band energy after a partial silence. |
| Compression | Soft-knee compression bounded to `[0, 1]`. |
| Silence | Gate using `rmsDb` and `match.confidence`. |
| Units | ms, dB, Hz, normalized 0..1. No bare slider numbers. |

## Design Decisions

### DD1. Offline pre-analysis is the primary feature path

Cached features beat live derivation: smoother, richer, position-accurate. Live analysis is the fallback while no fingerprint is locked.

### DD2. Single SQLite database

One file at `~/.local/share/qlcplus/audio.db`. Holds tracks, features, fingerprints, and `AudioProfile` configs. No per-file sidecars, no `.qxw` blob inflation.

### DD3. Recognition via Olaf, position-locked playback follow

`AudioMatcher` runs continuously on live capture, switches `AudioFeatures.source` to `Cached` when locked. Position is advanced by audio frame time and corrected by re-locks. Drift > 50 ms triggers a re-lock attempt.

### DD4. AGPL-3.0 acceptable

The combined binary becomes AGPL-3.0 once Essentia is linked. The repository LICENSE and About box are updated. Optional build flag `-Daudio_essentia=OFF` exists for downstream redistributors who must avoid AGPL — those builds lose offline analysis but keep live aubio.

### DD5. Document-level Audio Profiles

Profiles live in the SQLite `audio_profiles` table. Functions reference profiles by ID. The `.qxw` file stores profile IDs only; the project is portable when shipped alongside `audio.db` (or features are re-indexed on import).

### DD6. VCAudioTrigger is the audio control center

Single widget for editing/monitoring profiles, browsing the library, and watching recognition state. Multiple instances may coexist; each binds to one profile.

### DD7. Functions reference profiles, not widgets

`RGBMatrix` has an `audioProfileId` property. Resolution chain: explicit → default-flagged → first → anonymous fallback (analyzer creates an internal default channel).

### DD8. Five perceptual bands replace low/mid/high

`sub`, `bass`, `lowMid`, `mid`, `high`. Names are configurable in the profile but defaults are verified against sine sweeps.

### DD9. Per-script DSP sliders deleted

`AudioParams` retains only non-DSP plumbing. RGBMatrix editor shows: profile selector, intensity scale (0–200%), "Edit profile…" button.

### DD10. Per-consumer state via AudioChannel handles

Atomic queued config updates applied at frame boundary. Snapshots are immutable value copies.

### DD11. Two `dtMs` values

`audioDtMs` (audio frame) for envelope/AGC/trigger timing. `consumerDtMs` (MasterTimer tick) exposed to JS for visual pacing.

### DD12. Trigger state machine — frame-stable

`value`, `active`, `firedThisFrame`, `releasedThisFrame`, `heldMs`, `cooldownRemainingMs`. All fields stable across reads in the same frame.

### DD13. AudioAnalyzer on audio thread, fixed budget

No heap allocation per frame. <1 ms per channel. Olaf streaming matcher must also stay <2 ms per frame; if not, move to a dedicated low-priority thread.

### DD14. No backwards compatibility

`ledfx_compat.js`, `audio_common.js`, legacy per-bar triggers, old `AudioParams` slider semantics, and `BeatTracker` are deleted in M7. No migration code, no XML version shims, no compatibility warnings. The project is young enough to absorb the break.

### DD15. Test corpus from FMA / Jamendo CC

EDM-leaning Creative Commons tracks pulled into `tests/audio/corpus/` (gitignored, fetched by a script). Used for BPM/key/drop accuracy benchmarks and recognition-lock latency measurements.

### DD16. Optional Essentia, mandatory aubio + Olaf

Essentia is the only AGPL dependency. aubio (GPL-3) and Olaf (GPL-3) are required at build time. Essentia is gated by `-Daudio_essentia=ON` (default ON). Without Essentia, `analyzed_at`-style fields and structural events stay null; live analysis is unaffected.

### DD17. Decode via Qt's FFmpeg backend, ffmpeg bundled

Qt 6.5+ MP3 support is provided by FFmpeg. The macOS/Windows installers bundle FFmpeg shared libs via `macdeployqt`/`windeployqt`. On Linux we link against system FFmpeg and document the dependency.

### DD18. Fingerprint-first sequencing

The first milestone proves recognition works end-to-end on real EDM. If Olaf cannot lock cleanly through DJ EQ and pitch-shift, the rest of the architecture is reconsidered before more code is written.

---

## Implementation Milestones

### M0. Foundation — dependencies and build

- [ ] Vendor `aubio`, `Olaf`, `Essentia` as git submodules under `thirdparty/`. Pin versions.
- [ ] Add CMake options `audio_essentia=ON`, `audio_aubio=ON`, `audio_olaf=ON`.
- [ ] Wire CMake to find FFmpeg (system on Linux, bundled on macOS/Windows).
- [ ] Add `qlcplusaudioanalysis` static library target containing the new analyzer modules.
- [ ] Add `engine/audio/test/` harness scaffolding with a synthetic-frame fixture.
- [ ] Update top-level LICENSE / About box for AGPL note.
- [ ] Add `scripts/fetch_test_corpus.sh` pulling ~50 CC EDM tracks from FMA/Jamendo into `tests/audio/corpus/` (gitignored).

### M1. Fingerprint-first proof — Olaf indexing + live matching

- [ ] Build a standalone CLI `qlcplus-audio-index` that, given a directory, computes Olaf fingerprints for every file and writes them to `audio.db`.
- [ ] Build a standalone CLI `qlcplus-audio-match` that takes a microphone or file input, streams to Olaf, and prints `(track_id, position_ms, confidence)` updates.
- [ ] Index the M0 test corpus (~50 tracks).
- [ ] Benchmark: lock latency from cold (target <1.5 s), position accuracy (target ±100 ms), behavior under EQ / +5% pitch shift / club PA noise simulation.
- [ ] Decision gate: if lock latency or accuracy fails on EDM corpus, revisit Panako or add chromagram cross-correlation pre-filter before continuing.

### M2. Offline feature pipeline — Essentia + SQLite

- [ ] Define `AudioFeatures` and supporting structs in `engine/audio/src/audiofeatures.{h,cpp}`.
- [ ] Implement `AudioLibraryIndexer` running Essentia on each track: BPM, beats, key, danceability, multi-band envelopes, spectral shape, MFCC, structural events (drops/builds).
- [ ] Implement SQLite schema migrations and writer: `tracks`, `feature_frames`, `beats`, `onsets`, `structural_events`.
- [ ] Extend the indexing CLI to compute features alongside fingerprints in the same pass.
- [ ] Index the test corpus end-to-end. Spot-check: BPMs match published values within ±0.5 BPM, keys match within Camelot-adjacent tolerance, drops align within 1 s of human-marked timestamps for 5 hand-annotated tracks.

### M3. Live AudioAnalyzer — aubio integration

- [ ] Define `AudioFrame` in `engine/audio/src/audioframe.h`.
- [ ] Modify `AudioCapture::processData()` to populate `AudioFrame` and pass it directly to `LiveAudioAnalyzer` (bypass `dataProcessed()` for new path; the legacy signal stays live until M7).
- [ ] Implement `LiveAudioAnalyzer` with shared features: 32 log bands, `bandsDb`, `bandsNormalized`, perceptual bands, `rmsDb`, `peakDb`, `crestFactor`, `spectralFlux`, `spectralCentroidHz`, `spectralRolloffHz`, `spectralFlatness`, `noiseFloorDb`, aubio onsets/tempo.
- [ ] Implement `AudioMatcher` running Olaf streaming on the live frame.
- [ ] Implement `CachedAudioAnalyzer` reading SQLite at the locked position.
- [ ] Implement source-switching: live ↔ cached based on `match.locked`.
- [ ] Synthetic tests: silence, white noise, sine sweeps, kick impulse, hat impulse, ramp, threshold hover, varying frame intervals.

### M4. AudioProfile + AudioChannel + RGBMatrix wiring

- [ ] `AudioProfile` document model: ID, name, isDefault, `AudioChannelConfig`. Persist to `audio_profiles` table.
- [ ] `AudioAnalyzer::createChannel()`/`updateConfig()`/`snapshot()`/`close()` handle API.
- [ ] Per-channel envelope, AGC, triggers (Schmitt + hold + cooldown).
- [ ] `RGBMatrix.audioProfileId` property; resolution chain.
- [ ] Auto-create "Default Audio" profile on first audio script use.
- [ ] Frame budget instrumentation; assert <1 ms / channel.

### M5. VCAudioTrigger full rewrite

- [ ] Delete the old per-bar trigger UI.
- [ ] Header: profile selector, live/cached badge, recognition lock indicator.
- [ ] Library panel: folder picker, indexing progress, per-track status.
- [ ] Bands / Envelope / AGC / Triggers / Spectral tabs.
- [ ] Status strip: drop/build/break lamps, key, BPM, bar phase.
- [ ] Live monitor strip with envelope curves and trigger lamps.

### M6. MCP tools

Tools (added under `mcp/tools/audio_tools.cpp`):

- [ ] `analyze_audio_file` — analyze one path; return summary features.
- [ ] `analyze_audio_library` — batch-analyze a directory; report progress.
- [ ] `get_audio_features` — query features for a track at a position.
- [ ] `get_audio_match_state` — return current `match.locked`, track, position, confidence.
- [ ] `list_audio_profiles` / `create_audio_profile` / `update_audio_profile`.
- [ ] `list_indexed_tracks` — paginated list of tracks in `audio.db`.
- [ ] Annotations: read-only on getters; idempotent on profile create/update; long-running on indexing.

### M7. Script port + cleanup

- [ ] Add `RGBUtil` + `AudioDSP.Filter`.
- [ ] Port the 28 audio scripts to read `audio.bands.*`, `audio.triggers.*`, `audio.events.*`, `audio.music.*`.
- [ ] Add a new demo script `audiodrop.js` that flashes blackout-then-blast on `audio.events.drop` and shifts hue by `audio.music.key`.
- [ ] Delete `ledfx_compat.js`, `audio_common.js`, `BeatTracker`, the old `AudioParams` DSP fields, and the legacy `dataProcessed()` consumer.
- [ ] Strip `LedFx.*` references — `rg "LedFx\." resources/rgbscripts` returns empty.

### M8. Verification

| Test | Expected |
| --- | --- |
| Silence | Features near zero, no trigger chatter |
| White noise | High flatness, no false beat |
| Sine sweeps | Energy in expected perceptual band |
| Kick impulse | `triggers.bass.firedThisFrame` once, cooldown holds |
| Hat impulse | `triggers.high.firedThisFrame` without bass |
| Quiet→loud ramp | AGC adapts smoothly |
| Variable frame interval | Envelopes decay by `audioDtMs` |
| Schmitt hover | No chatter |
| Multiple profiles | Independent snapshots from same audio |
| FMA corpus BPM | ±0.5 BPM on 90% of tracks |
| FMA corpus key | Camelot-neighbor on 80% of tracks |
| FMA corpus drops | ±1 s of hand-annotated drops on 5 tracks |
| Recognition cold lock | <1.5 s on 90% of corpus |
| Recognition position | ±100 ms after lock, ±300 ms under +5% pitch shift |
| Frame budget | <1 ms per channel, <2 ms matcher |

---

## Done Criteria

- [ ] Single SQLite `audio.db` holds tracks, features, fingerprints, and profiles.
- [ ] `qlcplus-audio-index` indexes a directory with Essentia + Olaf in one pass.
- [ ] Live `AudioMatcher` locks onto a recognized track within 1.5 s and stays locked through normal DJ play.
- [ ] `AudioFeatures` is the single shared view consumed by scripts, widgets, and MCP tools.
- [ ] `AudioProfile` document objects own all DSP config; multiple profiles coexist; `RGBMatrix` references one by ID.
- [ ] VCAudioTrigger rebuilt: library browser, recognition badge, drop/build/key indicators, envelope/AGC/trigger/spectral panels.
- [ ] All 28 audio scripts ported. `audiodrop.js` demonstrates structural event use.
- [ ] `ledfx_compat.js`, `audio_common.js`, `BeatTracker`, and old `AudioParams` DSP fields are deleted.
- [ ] MCP tools exposed for batch analysis, feature lookup, and recognition state.
- [ ] LICENSE and About box reflect AGPL-3.0 combined work; build flag for Essentia-free builds documented.
- [ ] Synthetic + corpus tests pass. Frame budgets respected.
