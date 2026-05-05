# Audio DSP Modernization Plan

## Goal

Replace QLC+'s minimal audio analysis (32 log bands + spectral-flux beat detector, mic-only) with a scientifically grounded pipeline that powers RGB scripts, Audio Trigger widgets, and AI-driven cue generation, and that is musically aware enough for EDM stage lighting.

The pipeline must, in build priority order:

1. **Live first, low-latency, best-possible.** Drive scripts, widgets, and MCP tools from rich live features computed on the audio thread with end-to-end onset latency under ~10 ms and a per-frame budget under ~1 ms per channel. This is the foundation the rest builds on.
2. Expose a single uniform `AudioFeatures` view so consumers don't care where the data came from.
3. Pre-analyze any audio file in the user's library and cache rich features (BPM, beat grid, key/chroma, multi-band envelopes, structural drops, spectral shape) — added once the live path is shipped and stable.
4. Identify what is currently playing on stage via one-shot acoustic fingerprinting, switching the same `AudioFeatures` view from live to cached values for richer (key-aware, structural) lighting.
5. Track the play position continuously via a tiered source — DJ-software protocols when wired, chromagram cross-correlation otherwise — so cached features replay in sync through DJ EQ, pitch shift, and tempo bend.

Live analysis works on its own and is shippable independently. Cached, identified, and tiered position-tracked features each upgrade the same view incrementally.

This project is young. **No backwards compatibility is required** — old XML, old per-script DSP, and the bundled `ledfx_compat.js` shim can all be removed. Simple and high quality wins over preservation.

## End State

| Area | Final direction |
| --- | --- |
| Offline analysis | **Essentia** (AGPL-3.0 accepted) computes BPM, beat/bar grid, key, chroma, multi-band envelopes, structural drops, danceability, MFCC. Run once per file. |
| Identification | **Olaf** (GPL-3.0, native C) builds a constellation-hash index. Used **one-shot** at session start (and on suspected track change) to identify the current song and an initial play position. Olaf's tempo-shift brittleness is acceptable here because identification needs only one good match, not continuous lock. |
| Position tracking | A tiered `PositionSource` abstraction. Tier 1: DJ-software protocols (OS2L beat counter + cached beat grid, Pro DJ Link, StagelinQ). Tier 2: **chromagram cross-correlation** against cached chroma in a ±2 s window every ~5 s, with a small speed-search yielding tempo-deviation as a bonus. Tier 3: aubio onsets + internal clock when nothing else is wired. |
| Live analysis | **aubio** (GPL-3.0) computes onsets, tempo, multi-band on the live capture stream. Always-on; the only feature source when no track is identified. |
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
┌────────────────────────────────────────────────────────────────────┐
│  Offline (one-shot per file, batch over the user library)          │
│                                                                    │
│  Audio file ──▶ QAudioDecoder ──▶ PCM                              │
│                                    ├─▶ Essentia ──▶ Features+Chroma│
│                                    └─▶ Olaf ─────▶ Hashes          │
│                                              │       │             │
│                                              ▼       ▼             │
│                                       ┌──────────────────┐         │
│                                       │  SQLite audio.db │         │
│                                       └──────────────────┘         │
└────────────────────────────────────────────────────────────────────┘

┌────────────────────────────────────────────────────────────────────┐
│  Live                                                              │
│                                                                    │
│  Mic / line ──▶ AudioCapture ──▶ AudioFrame ──▶ LiveAnalyzer       │
│                                                  (aubio + spectral)│
│                                                                    │
│  Identification (one-shot, ~5 s window, re-runs on drift/silence): │
│   AudioFrame ──▶ Olaf ──▶ (track_id, initial position_ms)          │
│                                                                    │
│  Position tracking (continuous, tiered, picks highest available):  │
│   ┌────────────────────────────────────────────────────────┐       │
│   │ Tier 1 protocols (when wired)                          │       │
│   │   OS2L listener  ──▶ beat# + BPM                       │       │
│   │   ProDJLink/StagelinQ ──▶ track + position             │       │
│   │ Tier 2 chromagram tracker                              │       │
│   │   live chroma ⨯ cached chroma → position + speed       │       │
│   │ Tier 3 internal clock + aubio drift correction         │       │
│   └─────────────────────────┬──────────────────────────────┘       │
│                             ▼                                      │
│                       PositionSource                               │
│                  (priority + confidence + drift)                   │
│                             │                                      │
│                             ▼                                      │
│                  CachedAudioAnalyzer reads SQLite                  │
│                  at the locked position                            │
│                             │                                      │
│         live ──┐            ▼                                      │
│                └──▶  AudioFeatures (unified view)                  │
│                             │                                      │
│              ┌──────────────┼───────────────┐                      │
│              ▼              ▼               ▼                      │
│      AudioProfile      RGBMatrix       VCAudioTrigger              │
│       (channels)       (scripts)       (UI / triggers)             │
└────────────────────────────────────────────────────────────────────┘
```

## Library Responsibilities

| Library | Mode | Computes | Why |
| --- | --- | --- | --- |
| **Essentia** | Offline | BPM (RhythmExtractor2013), beat/bar grid, key + scale (KeyExtractor / HPCP), 12-bin chroma at ~10 Hz, danceability, multi-band mel, spectral centroid/rolloff/flatness, MFCC, structural segmentation, drop candidates from novelty curve | Single library covers nearly all wanted features including the chroma needed for position tracking. AGPL accepted. |
| **Olaf** | Offline (index) + Live (one-shot ID) | Constellation-hash fingerprints; on the live side runs over a rolling ~5 s window to identify the track and produce an initial position estimate | Native C, embedded-friendly. Used only for identification, not continuous lock — Olaf's well-known brittleness to >3% time-stretch is acceptable when one good match is enough. |
| **Chromagram tracker** | Live | Cross-correlates a live chroma window against the cached chroma stream around the expected position, searching a small speed range (±10%) | Tempo-tolerant continuous position tracking. Runs at a low rate (~5 s cadence). Reuses Essentia code path on the live side for chroma. |
| **OS2L listener** | Live | Receives `beat`/`btn`/`cmd` JSON over TCP from VirtualDJ et al, advertised over Bonjour | Exact beat phase + BPM with no microphone latency, when a supported DJ app is running. Already partly supported in QLC+ v4. |
| **aubio** | Live | Real-time onsets, tempo, pitch, multi-band — sub-10 ms latency | Always-on live feature source; sole driver when no track is identified. Mature C API. |
| **Qt Multimedia** | Offline + Live | MP3/WAV/FLAC/OGG/M4A decoding via FFmpeg | Already in tree. ffmpeg bundled with the app for cross-platform consistency. |
| **SQLite** (Qt `QSqlDatabase` with `QSQLITE` driver) | Storage | Tracks, features, chroma, fingerprint hashes, profiles | Already a transitive Qt dependency. Single file. Inspectable. |

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

-- Olaf fingerprint hashes (used only for one-shot identification)
CREATE TABLE fingerprints (
    hash            INTEGER NOT NULL,             -- Olaf 64-bit packed hash
    track_id        INTEGER NOT NULL REFERENCES tracks(id) ON DELETE CASCADE,
    time_ms         INTEGER NOT NULL,
    PRIMARY KEY (hash, track_id, time_ms)
);
CREATE INDEX fp_hash ON fingerprints(hash);

-- 12-bin chroma at ~10 Hz, used by the chromagram position tracker.
-- Stored as one BLOB row per track (12 floats * frames). Easier to mmap as
-- a contiguous matrix than per-frame rows for cross-correlation queries.
CREATE TABLE chroma (
    track_id        INTEGER PRIMARY KEY REFERENCES tracks(id) ON DELETE CASCADE,
    frame_rate_hz   REAL NOT NULL,                -- typically 10
    frame_count     INTEGER NOT NULL,
    matrix_blob     BLOB NOT NULL                 -- 12*frame_count floats, row-major
);

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

Runs on the audio thread. Computes shared spectrum/loudness features, feeds aubio for onsets/tempo, and produces a rolling 12-bin chroma at ~10 Hz for the position tracker. Emits `AudioFeatures` with `source = Live`, `match.identified = false` initially.

### AudioIdentifier (Olaf, one-shot)

Maintains a rolling ~5 s sample buffer. On request — at session start, on chromagram tracker drift exceeding ~2 s, on extended low-confidence position, or every 60 s as a sanity check — it runs Olaf against the SQLite fingerprint index and returns `(track_id, initial_position_ms, confidence)` or `none`. Runs on a worker thread; never on the audio thread. Track changes detected here invalidate the current position source and re-seed Tier 2.

### PositionSource (tiered abstraction)

Single object that all consumers read. Holds the highest-priority source currently confident, plus a `lastUpdateMs` for staleness checks. Tiers, in priority order:

| Tier | Source | Provides | Priority condition |
| --- | --- | --- | --- |
| 1a | Pro DJ Link / StagelinQ | `track_id`, `position_ms`, `bpm` | Reachable on LAN, reporting active deck |
| 1b | OS2L | `beat#`, `bpm`, `change` flag | TCP connection alive; needs Tier-2 or AudioIdentifier-supplied `track_id` to bind beat# to song timeline |
| 2 | ChromaPositionTracker | `position_ms`, `speed_factor` | Track identified by AudioIdentifier; correlation peak above threshold |
| 3 | Aubio + internal clock | drifting `position_ms` only | Always available |

Handoff is automatic. Each tier publishes `confidence` and `staleness`; the source picks the highest-priority tier whose values are fresh and confident. Per-source latency offsets are configurable and calibrated by cross-correlating Tier 1/2 timestamps against onset events from Tier 3.

### ChromaPositionTracker

Given `(track_id, expected_position_ms)`, slices a window of cached chroma around the expected position (default ±2 s) and the current ~5 s of live chroma. Computes normalized cross-correlation across a small grid of speed factors (0.92, 0.94, …, 1.08). Returns the (offset_ms, speed_factor, peak_value) maximum. Updates at ~5 s cadence; ~5–10 ms CPU per query (12-bin chroma, 100 frames vs ~50 frames live, on a worker thread).

### OS2LPositionSource

Listens on TCP for OS2L `beat` events. Once the AudioIdentifier has supplied a `track_id`, the cached beat grid lets us map an incoming `beat#` to a `position_ms`. If the OS2L client provides a beat-zero anchor relative to the song start (VirtualDJ does, via the `change:true` flag and known beat counter semantics), use it directly; otherwise calibrate by aligning the first OS2L beat to the nearest cached beat above the AudioIdentifier's reported position.

### CachedAudioAnalyzer

Given the current `PositionSource` value, queries SQLite for surrounding feature frames, beats, onsets, and structural events, and reconstructs an `AudioFeatures` view with `source = Cached`. When `speed_factor != 1.0` from the chroma tracker, beats and onsets are time-warped accordingly so triggers fire on the right musical moments. Falls back to `source = Live` when no track is identified.

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

The widget is rebuilt as the audio control center. The live-data panels (bands, envelope, AGC, triggers, spectral) ship in M4. The library browser, recognition badge, drop/build/key indicators, and position-source picker are added incrementally in M6–M9 — they sit in the same chrome but stay greyed out until their backend lands. Layout:

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

### DD1. Live features are the foundation, low-latency is non-negotiable

The live path ships first and works on its own. Targets: onset detection latency <10 ms (audio buffer 256 samples at 48 kHz, FFT hop 256, aubio default), per-frame analyzer budget <1 ms per `AudioChannel`, no heap allocation in the hot path, no locks held while user-thread code runs. Cached/identified features are upgrades to the same `AudioFeatures` view — they don't replace the live path, they enrich it.

### DD2. Single SQLite database

One file at `~/.local/share/qlcplus/audio.db`. Holds tracks, features, fingerprints, and `AudioProfile` configs. No per-file sidecars, no `.qxw` blob inflation.

### DD3. Identification one-shot, position via tiered source

Olaf is used **only for identification**, not continuous lock. It runs in a worker thread over a rolling ~5 s window, on demand or every ~60 s, and reports `(track_id, initial_position_ms, confidence)`. Continuous position is owned by a `PositionSource` abstraction with three tiers: (1) DJ-software protocols (OS2L, Pro DJ Link, StagelinQ) when wired; (2) chromagram cross-correlation against cached chroma, ±10% speed search; (3) aubio + internal clock fallback. The highest-priority confident-and-fresh tier wins. This sidesteps Olaf's tempo-shift brittleness because Olaf only has to find one good match, not stay locked through a DJ pitch-bend.

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

### DD18. Live-features-first sequencing

The first milestones build the live analyzer, profile/channel system, RGBMatrix wiring, scripts, and VCAudioTrigger UI on **live audio only**. This is shippable on its own and gives users a noticeably better audio-reactive engine before any cached/identified/positioned work begins. Offline analysis (M6), identification (M7), Tier-2 chromagram tracking (M8), and Tier-1 DJ protocols (M9) each plug behind the unchanged `AudioFeatures` view.

### DD19. Tiered PositionSource with calibrated latency offsets

`PositionSource` exposes the highest-priority confident tier. Each tier publishes `confidence` and `lastUpdateMs`; staleness or confidence drop demotes it. Per-source latency offsets are configured in the UI and self-calibrated by cross-correlating Tier-1/2 timestamps against Tier-3 onsets, since DJ-software clocks run ahead of the room PA's actual audio by 5–100 ms.

### DD20. Fingerprint engine is replaceable

`AudioIdentifier` is an interface; the default implementation is Olaf. A Panako backend ships as a build option for environments where DJ pitch-bend during the identification window is common (Panako handles ±10% time-stretch but pulls in a JVM). Users can switch backends in settings without re-indexing their library if both backends have indexed.

### DD21. Live latency target

End-to-end onset latency target: <10 ms (input buffer + FFT hop + analyzer + signal emit). Per-`AudioChannel` analyzer budget: <1 ms. Per-`AudioAnalyzer` shared-feature budget: <0.5 ms. No heap allocation per frame; fixed-size arrays only. Instrumented from M1.

---

## Implementation Milestones

Sequencing: live ships first (M0–M5). Cached/identified/position-tracked features extend the same `AudioFeatures` view in M6–M9. Verification across the corpus is M10. Anything past M5 is optional from a "QLC+ has great audio reactivity" standpoint; everything past M5 is "QLC+ knows what's playing."

### M0. Foundation — live dependencies only

- [ ] Vendor `aubio` as a git submodule under `thirdparty/`. Pin version.
- [ ] CMake option `audio_aubio=ON` (default). `audio_essentia` and `audio_olaf` flags exist but stay OFF until M6/M7.
- [ ] Add `qlcplusaudioanalysis` static library target.
- [ ] `engine/audio/test/` harness scaffolding with a synthetic-frame fixture and a tone/sweep/impulse generator.
- [ ] `scripts/audiobench` CLI that injects synthetic frames into the analyzer and prints features and timing — used to lock down latency targets before any UI work.

### M1. Live AudioAnalyzer — best-possible, low-latency

- [ ] Define `AudioFrame` (mono samples, FFT bins, raw RMS, peak, sample rate, `audioDtMs`, frame index) in `engine/audio/src/audioframe.h`.
- [ ] Modify `AudioCapture::processData()` to populate `AudioFrame` and pass directly to `LiveAudioAnalyzer`. Tune buffer to 256 samples / 48 kHz (~5 ms) where the platform backend allows.
- [ ] Define `AudioFeatures` and supporting structs (live-only fields populated; `match` and `events.drop` left default).
- [ ] Implement `LiveAudioAnalyzer` shared features: 32 log bands, `bandsDb`, `bandsNormalized`, perceptual bands, `rmsDb`, `peakDb`, `crestFactor`, `spectralFlux`, `spectralCentroidHz`, `spectralRolloffHz`, `spectralFlatness`, `noiseFloorDb`, 12-bin live chroma.
- [ ] Integrate aubio: onset detection (HFC default, configurable), tempo estimation, optional pitch.
- [ ] No heap allocation per frame. Fixed-size arrays. Lock-free SPSC ring for snapshots to consumers.
- [ ] Frame-budget instrumentation: per-frame analyzer time histogram. Assert <1 ms shared, <0.5 ms per-channel.
- [ ] Synthetic tests: silence, white noise, sine sweeps, kick impulse, hat impulse, ramp, threshold hover, variable frame interval.
- [ ] End-to-end live latency measurement (input click → onset signal) on Linux/macOS/Windows. Target <10 ms.

### M2. AudioProfile + AudioChannel + RGBMatrix wiring

- [ ] `AudioProfile` document-model class: ID, name, isDefault, `AudioChannelConfig`. Persisted under `Doc` for now (SQLite move comes in M6 alongside the library DB).
- [ ] `AudioAnalyzer::createChannel()` / `updateConfig()` / `snapshot()` / `close()` handle API. Atomic queued config updates applied at frame boundary. Immutable `AudioSnapshot` value copies.
- [ ] Per-channel envelope (per-band attack/release), AGC (max gain dB, release ms, noise floor dB), triggers (Schmitt + hold + cooldown), volume smoothing.
- [ ] `RGBMatrix.audioProfileId` property; resolution chain (explicit → default-flagged → first → anonymous fallback).
- [ ] Auto-create "Default Audio" profile on first audio script use.
- [ ] RGBMatrix editor: profile selector, intensity scale (0–200%), "Edit profile…" button. Old per-script DSP sliders removed.
- [ ] Frame budget instrumentation: assert <1 ms with five active channels.

### M3. RGBUtil + thin script vertical slice

- [ ] Add `RGBUtil`: `rgb`, `hsv2rgb`, `createMap`, `interpolate`, `simplex2d`, `noiseField2d`. Verify byte order matches engine pixel format.
- [ ] Add `AudioDSP.Filter(decayMs, riseMs)` for optional per-pixel exponential smoothing in scripts.
- [ ] Wire `buildAudioDataObject()` in `rgbscriptv4.cpp` to read `AudioSnapshot`: `audio.bands.*`, `audio.triggers.*`, `audio.volume.*`, `audio.music.bpm`, `audio.features.*`, `audio.audioDtMs`, `audio.consumerDtMs`.
- [ ] Port 3 pilot scripts representative of the major patterns (one trigger-first, one three-band blend, one spectrum visual). Visual side-by-side compare against the legacy version.
- [ ] Decision gate: if pilots look or feel worse on live audio than the legacy versions, fix the live analyzer before proceeding.

### M4. VCAudioTrigger live UI rewrite

- [ ] Delete the old per-bar trigger UI and `slotSpectrumDataChanged()`-driven DSP.
- [ ] Header: profile selector, live status badge (recognition badge stays placeholder until M7).
- [ ] Bands tab: perceptual band edge editors, frequency labels, presets.
- [ ] Envelope tab: per-band attack/release sliders with live mini-graph.
- [ ] AGC tab: max gain / release / noise-floor + live gain meter.
- [ ] Triggers tab: per-band Schmitt thresholds, hold/cooldown, lamp + fires/sec.
- [ ] Spectral tab: live centroid, rolloff, flatness readouts.
- [ ] Live monitor strip: envelope curves and trigger lamps, ~30 Hz refresh.

### M5. Port remaining scripts + delete legacy

- [ ] Port the remaining 25 audio scripts (the 28 audio scripts minus M3 pilots).
- [ ] Delete `ledfx_compat.js`, `audio_common.js`, `BeatTracker`, the old `AudioParams` DSP fields, and the legacy `dataProcessed()` consumer path.
- [ ] `rg "LedFx\." resources/rgbscripts` returns empty.
- [ ] CMakeLists no longer installs the deleted JS files.
- [ ] **Live shippable here.** Tag and ship if the rest of the work slips.

### M6. Offline feature pipeline — Essentia + SQLite

- [ ] Add `audio_essentia=ON` build flag. Vendor Essentia. Update LICENSE / About for AGPL.
- [ ] Single SQLite at `~/.local/share/qlcplus/audio.db` with `tracks`, `feature_frames`, `beats`, `onsets`, `structural_events`, `chroma`, `audio_profiles`. Migrate `AudioProfile` storage from `Doc` XML to SQLite.
- [ ] `AudioLibraryIndexer` running Essentia: BPM, beats, key, danceability, multi-band envelopes, spectral shape, MFCC, structural events (drops/builds), 12-bin chroma at 10 Hz.
- [ ] CLI `qlcplus-audio-index` for batch operation.
- [ ] `CachedAudioAnalyzer` that publishes the same `AudioFeatures` shape, populated from SQLite at a given position.
- [ ] FMA/Jamendo corpus fetch script and accuracy spot-checks: BPM ±0.5, key Camelot-neighbor, drops ±1 s on 5 hand-annotated tracks.

### M7. AudioIdentifier — Olaf one-shot

- [ ] Add `audio_olaf=ON` build flag. Vendor Olaf.
- [ ] Extend the indexer to write Olaf hashes into `fingerprints` alongside Essentia features.
- [ ] `AudioIdentifier` interface; default `OlafAudioIdentifier` implementation. Worker-thread rolling-buffer match. On-demand + every 60 s + on chroma-tracker drift > 2 s.
- [ ] When identification succeeds, switch `AudioFeatures.source` to `Cached` with `match.identified=true`. On loss, fall back to live.
- [ ] Benchmark: cold ID lock <1.5 s on 90% of corpus; ±100 ms initial position; behaviour under EQ and +5% pitch shift.
- [ ] If Olaf misses commonly under DJ pitch-bend at the identification window, add `PanakoAudioIdentifier` behind `audio_panako=ON` (JVM dependency documented).

### M8. ChromaPositionTracker — Tier 2

- [ ] Implement `ChromaPositionTracker`: live 12-bin chroma window vs cached chroma matrix, normalized cross-correlation, ±10% speed search grid (0.92, 0.94, …, 1.08).
- [ ] Update at ~5 s cadence on a worker thread. Publish `(position_ms, speed_factor, confidence)`.
- [ ] Time-warp cached beat/onset events by `speed_factor` so triggers fire on the right musical moments.
- [ ] Tests: tracker reacquires within ~5 s after a 100 ms manual seek; survives ±5% pitch-bend without losing lock.

### M9. Tier-1 protocols — OS2L, Pro DJ Link, StagelinQ

- [ ] `PositionSource` abstraction with priority + confidence + staleness; per-source latency offsets.
- [ ] OS2L listener: TCP JSON over Bonjour/Avahi. Decode `beat`/`btn`/`cmd`. Bind incoming `beat#` to song timeline using cached beat grid + AudioIdentifier track ID.
- [ ] Pro DJ Link client (use existing dysentery/beat-link protocol notes). Provides track + position when CDJ-3000s are on the LAN.
- [ ] StagelinQ client. Provides track + position from Denon/Numark Prime.
- [ ] UI: PositionSource active-tier indicator with calibratable latency offset.

### M10. MCP tools + verification

MCP tools (under `mcp/tools/audio_tools.cpp`):

- [ ] `analyze_audio_file`, `analyze_audio_library`, `get_audio_features`, `get_audio_match_state`, `get_position_source_state`.
- [ ] `list_audio_profiles` / `create_audio_profile` / `update_audio_profile`.
- [ ] Annotations: read-only on getters; idempotent on profile create/update; long-running on indexing.

Verification matrix:

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
| Live latency | <10 ms input-click to onset signal |
| Live frame budget | <1 ms shared, <0.5 ms per channel |
| FMA corpus BPM | ±0.5 BPM on 90% of tracks |
| FMA corpus key | Camelot-neighbor on 80% of tracks |
| FMA corpus drops | ±1 s of hand-annotated drops on 5 tracks |
| Identification cold lock | <1.5 s on 90% of corpus |
| Chroma tracker drift | <50 ms under +5% pitch shift after acquisition |
| OS2L sync | Beat events align with cached beat grid within calibrated offset |

---

## Done Criteria

### Live milestone (M0–M5, shippable on its own)

- [ ] `AudioFeatures` is the single shared view consumed by scripts, widgets, and MCP tools.
- [ ] `LiveAudioAnalyzer` produces all live features below 1 ms shared / 0.5 ms per channel.
- [ ] End-to-end live onset latency under 10 ms on Linux, macOS, and Windows.
- [ ] `AudioProfile` document objects own all DSP config; multiple profiles coexist; `RGBMatrix` references one by ID.
- [ ] VCAudioTrigger rebuilt with bands/envelope/AGC/triggers/spectral panels working off live data.
- [ ] All 28 audio scripts ported to read `audio.bands.*`, `audio.triggers.*`, `audio.music.*`.
- [ ] `ledfx_compat.js`, `audio_common.js`, `BeatTracker`, and old `AudioParams` DSP fields deleted.
- [ ] Synthetic test matrix passes. Frame budget instrumentation in CI.

### Cached + identified + tiered position (M6–M10)

- [ ] Single SQLite `audio.db` holds tracks, features, chroma, fingerprints, and profiles.
- [ ] `qlcplus-audio-index` indexes a directory with Essentia + Olaf in one pass.
- [ ] `AudioIdentifier` identifies a known track within 1.5 s of session start.
- [ ] `ChromaPositionTracker` keeps position drift under 50 ms during +5% pitch-bend.
- [ ] `PositionSource` correctly hands off between OS2L / Pro DJ Link / StagelinQ / chroma / live tiers under simulated venue conditions.
- [ ] MCP tools exposed for batch analysis, feature lookup, identification state, and position-source state.
- [ ] LICENSE and About box reflect AGPL-3.0 combined work; build flag for Essentia-free builds documented.
- [ ] FMA corpus benchmarks pass.
