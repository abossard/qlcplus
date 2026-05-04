# Global Audio Monitor Panel — Implementation Plan

## Concept

A slide-in panel from the toolbar showing live audio data. Always available regardless of context.
Helps users tune audio parameters, debug input issues, and see what scripts receive.

## What Other Tools Do (research findings)

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
│ BPM    128  ● (beat flash)                 │
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
| **BPM** | Beat flash only in MVP | `BeatTracker::getCurrentBpm()` not safely exposed yet |
| **Performance** | Register 32 bands only while panel is open | Zero cost when closed |
| **Persistence** | Remember open/closed state | via QML Settings or app state |

### What It Is / What It Isn't

- **IS**: an input monitor — "is audio coming in? what does it look like?"
- **IS NOT**: a per-script debugger (that's v2 with optional `algo.audioDebug()`)

## Implementation

### New Files

| File | What |
|------|------|
| `qmlui/audiospectrummonitor.h/.cpp` | C++ backend: polls AudioCapture, exposes QML properties |
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
    Q_PROPERTY(QString statusText READ statusText NOTIFY activeChanged)

public:
    void setEnabled(bool enabled);  // registers/unregisters 32 bands
    
private slots:
    void slotDataProcessed();  // connected to AudioCapture::dataProcessed
    void slotBeatDetected();
    
private:
    Doc *m_doc;
    bool m_enabled = false;
    QVariantList m_spectrum;  // 32-element snapshot
    qreal m_lows, m_mids, m_highs, m_volume;
    bool m_beatPulse;
    int m_beatDecay = 0;
};
```

Thread safety: copy data in `slotDataProcessed()` (runs on capture thread) under a mutex,
emit `dataChanged()` via queued connection to main thread.

### QML Panel

- Slide-in from right (same pattern as UndoHistoryPanel)
- `Repeater` for 32 spectrum bars (Rectangle height = band magnitude)
- 3 horizontal bars for lows/mids/highs
- Volume bar
- Beat indicator (circle that flashes on beat, decays over 200ms)
- Toggle via toolbar icon button

## Blocking Issues to Address

1. **Thread-safe data access** — AudioCapture emits from capture thread. Copy under mutex.
2. **BPM not safely exposed** — MVP shows beat flash only, omit numeric BPM.
3. **Label raw vs processed clearly** — prevent user confusion about gain/floor.

## Effort Estimate

| Task | Estimate |
|------|----------|
| C++ AudioSpectrumMonitor | 1.5h |
| QML AudioMonitorPanel | 2h |
| MainView toolbar integration | 30min |
| Thread safety | 30min |
| Testing | 30min |
| **Total** | **~5h** |

## Future Extensions (v2)

- Script debug data via `algo.audioDebug()` API
- Numeric BPM display (needs safe BeatTracker accessor)
- Peak hold on spectrum bars
- Clipping/too-hot indicator
- Compact mode (volume + beat + L/M/H only)
- Mini sparkline in toolbar as toggle button
- Audio input device selector in the panel
