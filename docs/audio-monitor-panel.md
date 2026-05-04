# Global Audio Monitor Panel — Implementation Plan

## Status

**Step 1 (band splits) and Step 2 (VCAudioTriggers enhancement) are DONE.**

The VCAudioTriggers widget now has:
- Frequency-colored bars (orange lows / yellow mids / cyan highs)
- Low/mid/high split markers at band boundaries
- Monitor row with beat dot + L/M/H percentage readouts
- Beat flash overlay
- Monitor row auto-hides when widget is too small

Band split logic is centralized in `AudioCapture::lowCutBin(N)` / `AudioCapture::highCutBin(N)`.
JS scripts use a documented mirror of the same formula in `ledfx_compat.js`.

## What Remains: Global Audio Monitor Panel

The VCAudioTriggers widget is per-page and requires placement. A **global panel**
would be always-available from the toolbar, independent of any VC page.

## Architecture

### Centralized Band Split (DONE)

The single source of truth for frequency-to-bin mapping:

```
AudioCapture (engine/audio/src/audiocapture.h)
  ├─ SPECTRUM_MIN_FREQUENCY = 40Hz
  ├─ SPECTRUM_MAX_FREQUENCY = 5000Hz
  ├─ lowCutBin(N)   → first mid bin (~250Hz crossover)
  └─ highCutBin(N)  → first high bin (~2000Hz crossover)

Consumers:
  ├─ VCAudioTriggers (C++) → calls AudioCapture::lowCutBin/highCutBin
  ├─ ledfx_compat.js       → mirrors formula (documented, can't call C++)
  └─ Future: AudioSpectrumMonitor (C++) → will call AudioCapture::lowCutBin/highCutBin
```

### What Other Tools Do (research findings)

- **WLED**: separate debug page showing FFT bands, sensitivity, noise gate, AGC mode
- **LedFX**: inline melbank visualizer in effect editor + device-wide audio config page
- **Resolume**: dedicated Audio FFT panel, BPM tap + visual waveform
- **MadMapper**: audio analysis column with frequency bands + volume + beat + BPM

Common pattern: **global audio scope, not per-effect**. Shows raw input data.

## MVP Design

### Panel Sections

```
┌─ 🔊 Audio Monitor ─────────────────── [×] ┐
│                                            │
│ Input: Built-in Microphone ● ACTIVE        │
│                                            │
│ Spectrum (32 bands, 40Hz─5kHz)             │
│ █▆ ▇█▅▃▂ ▅▆▃▁ ▂▃▂▁ ▁▁ ▁▂ ▁ ▁ ▁          │
│ ─── LOWS ──── MIDS ──── HIGHS ───         │
│                                            │
│ LOWS  ▓▓▓▓▓▓▓ 0.71                        │
│ MIDS  ▓▓▓     0.28                        │
│ HIGHS ▓       0.09                        │
│                                            │
│ Volume ████████░░ 0.42                     │
│ Beat   ● (flash)                           │
│                                            │
│ ⚠ Labels show RAW input, not post-gain     │
└────────────────────────────────────────────┘
```

### Key Design Decisions

| Decision | Choice | Why |
|----------|--------|-----|
| **RAW vs processed** | Show RAW input | Global panel can't know per-script gain/floor. Label clearly. |
| **Band count** | Fixed 32 | Generic, matches common melbank use. Not per-script. |
| **Script debug state** | NOT in MVP | Would need `algo.audioDebug()` API — defer to v2 |
| **BPM** | Beat flash only in MVP | BeatTracker is onset detector, not tempo tracker. Numeric BPM would be misleading. |
| **Performance** | Register 32 bands only while panel is open | Zero cost when closed |
| **Band splits** | Use `AudioCapture::lowCutBin/highCutBin` | Centralized, consistent with VCAudioTriggers and JS scripts |

## Implementation

### New Files

| File | What |
|------|------|
| `qmlui/audiospectrummonitor.h/.cpp` | C++ backend: connects to AudioCapture, exposes QML properties |
| `qmlui/qml/AudioMonitorPanel.qml` | QML slide-in panel with spectrum + power + beat |

### Edits

| File | Change |
|------|--------|
| `qmlui/qml/MainView.qml` | Add 🔊 toolbar button + host AudioMonitorPanel |
| `qmlui/app.cpp` | Register AudioSpectrumMonitor as QML context property |
| `qmlui/CMakeLists.txt` | Add new source files |
| `qmlui/qmlui.qrc` | Register QML file |

### C++ Backend (`AudioSpectrumMonitor`)

```cpp
class AudioSpectrumMonitor : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(QVariantList spectrum READ spectrum NOTIFY dataChanged)
    Q_PROPERTY(qreal lowsPower READ lowsPower NOTIFY dataChanged)
    Q_PROPERTY(qreal midsPower READ midsPower NOTIFY dataChanged)
    Q_PROPERTY(qreal highsPower READ highsPower NOTIFY dataChanged)
    Q_PROPERTY(qreal volume READ volume NOTIFY dataChanged)
    Q_PROPERTY(bool beatPulse READ beatPulse NOTIFY beatChanged)
    Q_PROPERTY(int lowCutBin READ lowCutBin CONSTANT)
    Q_PROPERTY(int highCutBin READ highCutBin CONSTANT)
    Q_PROPERTY(QString statusText READ statusText NOTIFY activeChanged)

public:
    void setEnabled(bool enabled);  // registers/unregisters 32 bands

    // Delegate to centralized AudioCapture methods
    int lowCutBin() const { return AudioCapture::lowCutBin(32); }
    int highCutBin() const { return AudioCapture::highCutBin(32); }

private slots:
    void slotDataProcessed(double *bands, int size, double maxMag, quint32 power);
    void slotBeatDetected();
};
```

Thread safety: the `dataProcessed` signal carries a raw `double*` across threads
(pre-existing design in AudioCapture). Copy the data immediately in the slot.
This is also a pre-existing issue in VCAudioTriggers — see Known Issues below.

### QML Panel

- Slide-in from right (same pattern as existing panels)
- `Repeater` for 32 spectrum bars (colored by L/M/H using `lowCutBin`/`highCutBin`)
- 3 horizontal bars for lows/mids/highs
- Volume bar
- Beat indicator (circle that flashes on beat, decays over 200ms)
- Toggle via toolbar icon button

## Known Issues

1. **Thread-safe data access** — `AudioCapture::dataProcessed(double*)` passes a raw
   pointer across threads. The buffer may be mutated before the slot runs. This is a
   pre-existing issue in the codebase (VCAudioTriggers has the same pattern). A proper
   fix would change the signal to pass `QVector<double>` by value, but that affects
   all consumers and is out of scope for the monitor panel.

2. **BPM not safely exposed** — BeatTracker is an onset detector, not a tempo tracker.
   MVP shows beat flash only, omit numeric BPM.

## Future Extensions (v2)

- Script debug data via `algo.audioDebug()` API
- Numeric BPM display (needs proper tempo tracker, not just onset detection)
- Peak hold on spectrum bars
- Clipping/too-hot indicator
- Compact mode (volume + beat + L/M/H only)
- Mini sparkline in toolbar as toggle button
- Audio input device selector in the panel
- Pass split indices in `audioData` so JS scripts use `audioData.lowCut` / `audioData.highCut`
  instead of computing their own (eliminates the JS mirror of the C++ formula)
