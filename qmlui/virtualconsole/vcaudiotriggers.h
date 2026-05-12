/*
  Q Light Controller Plus
  vcaudiotriggers.h

  Copyright (c) Massimo Callegari

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#ifndef VCAUDIOTRIGGER_H
#define VCAUDIOTRIGGER_H

#include "vcwidget.h"
#include "treemodel.h"
#include "dmxsource.h"
#include "audioprofile.h"
#include "audiosnapshot.h"

#include <QAbstractListModel>
#include <QDateTime>
#include <QElapsedTimer>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QVector>
#include <atomic>

class QTimer;
#define KXMLQLCVCAudioTriggers QStringLiteral("AudioTriggers")

class AudioCapture;
class VirtualConsole;
class VCAudioTriggers;
struct AubioResults;

struct TimelineFrame
{
    float pitchHz = 0.0f;
    float pitchConfidence = 0.0f;
    quint16 onsetMask = 0; // 9 bits: energy=0x001, hfc=0x002, complex=0x004, ...
    bool beat = false;
    float bpm = 0.0f;
    float noteMidi = 0.0f;
    float noteVelocity = 0.0f;
    bool noteOn = false;
    bool noteOff = false;
};

class AudioProfileListModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles { IdRole = Qt::UserRole + 1, NameRole, IsDefaultRole };

    explicit AudioProfileListModel(Doc *doc, QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void refresh();

private:
    Doc *m_doc;
    struct Entry { quint32 id; QString name; bool isDefault; };
    QVector<Entry> m_entries;
};

class VCAudioTriggers : public VCWidget, public DMXSource
{
    Q_OBJECT

    Q_PROPERTY(bool captureEnabled READ captureEnabled WRITE setCaptureEnabled NOTIFY captureEnabledChanged)
    Q_PROPERTY(uchar volumeLevel READ volumeLevel WRITE setVolumeLevel NOTIFY volumeLevelChanged FINAL)
    Q_PROPERTY(int barsNumber READ barsNumber WRITE setBarsNumber NOTIFY barsNumberChanged FINAL)
    Q_PROPERTY(int selectedBar READ selectedBar WRITE setSelectedBar NOTIFY selectedBarChanged FINAL)
    Q_PROPERTY(QVariantList audioLevels READ audioLevels NOTIFY audioLevelsChanged)
    Q_PROPERTY(QVariantList barsInfo READ barsInfo NOTIFY barsInfoChanged)
    Q_PROPERTY(quint32 audioProfileId READ audioProfileId WRITE setAudioProfileId NOTIFY audioProfileIdChanged FINAL)
    Q_PROPERTY(double envelopeAttack READ envelopeAttack NOTIFY configChanged)
    Q_PROPERTY(double envelopeRelease READ envelopeRelease NOTIFY configChanged)
    // Per-band Schmitt trigger config. 12 properties: 3 bands × {High, Low, Hold, Cooldown}.
    Q_PROPERTY(double triggerLowHigh      READ triggerLowHigh      NOTIFY configChanged)
    Q_PROPERTY(double triggerLowLow       READ triggerLowLow       NOTIFY configChanged)
    Q_PROPERTY(double triggerLowHold      READ triggerLowHold      NOTIFY configChanged)
    Q_PROPERTY(double triggerLowCooldown  READ triggerLowCooldown  NOTIFY configChanged)
    Q_PROPERTY(double triggerMidHigh      READ triggerMidHigh      NOTIFY configChanged)
    Q_PROPERTY(double triggerMidLow       READ triggerMidLow       NOTIFY configChanged)
    Q_PROPERTY(double triggerMidHold      READ triggerMidHold      NOTIFY configChanged)
    Q_PROPERTY(double triggerMidCooldown  READ triggerMidCooldown  NOTIFY configChanged)
    Q_PROPERTY(double triggerHighHigh     READ triggerHighHigh     NOTIFY configChanged)
    Q_PROPERTY(double triggerHighLow      READ triggerHighLow      NOTIFY configChanged)
    Q_PROPERTY(double triggerHighHold     READ triggerHighHold     NOTIFY configChanged)
    Q_PROPERTY(double triggerHighCooldown READ triggerHighCooldown NOTIFY configChanged)

    // Kick / bass detector config (LedFx volume_beat_now). All durations in ms.
    Q_PROPERTY(double kickBeatMaxHz READ kickBeatMaxHz NOTIFY configChanged)
    Q_PROPERTY(double kickBeatMinPercentDiff READ kickBeatMinPercentDiff NOTIFY configChanged)
    Q_PROPERTY(double kickBeatMinAmplitude READ kickBeatMinAmplitude NOTIFY configChanged)
    Q_PROPERTY(double kickBeatRefractorySec READ kickBeatRefractorySec NOTIFY configChanged)
    Q_PROPERTY(int kickBeatHistoryLen READ kickBeatHistoryLen NOTIFY configChanged)
    Q_PROPERTY(bool kickEnabled READ kickEnabled NOTIFY configChanged)

    // Mel post-processing pipeline config.
    Q_PROPERTY(bool melPostEnabled READ melPostEnabled NOTIFY configChanged)
    Q_PROPERTY(double melPowerFactor READ melPowerFactor NOTIFY configChanged)
    Q_PROPERTY(double melGaussianSigma READ melGaussianSigma NOTIFY configChanged)
    Q_PROPERTY(double melSmoothDecay READ melSmoothDecay NOTIFY configChanged)
    Q_PROPERTY(double melSmoothRise READ melSmoothRise NOTIFY configChanged)
    Q_PROPERTY(double melCommonDecay READ melCommonDecay NOTIFY configChanged)
    Q_PROPERTY(double melCommonRise READ melCommonRise NOTIFY configChanged)
    Q_PROPERTY(double melDiffDecay READ melDiffDecay NOTIFY configChanged)
    Q_PROPERTY(double melDiffRise READ melDiffRise NOTIFY configChanged)
    // Per-band FreqPower decay/rise alphas — exposes the 4 independent bands
    // (beat/bass/mids/high) that the engine tracks via FreqPowerBandConfig.
    Q_PROPERTY(double freqPowerBeatDecay READ freqPowerBeatDecay NOTIFY configChanged)
    Q_PROPERTY(double freqPowerBeatRise  READ freqPowerBeatRise  NOTIFY configChanged)
    Q_PROPERTY(double freqPowerBassDecay READ freqPowerBassDecay NOTIFY configChanged)
    Q_PROPERTY(double freqPowerBassRise  READ freqPowerBassRise  NOTIFY configChanged)
    Q_PROPERTY(double freqPowerMidsDecay READ freqPowerMidsDecay NOTIFY configChanged)
    Q_PROPERTY(double freqPowerMidsRise  READ freqPowerMidsRise  NOTIFY configChanged)
    Q_PROPERTY(double freqPowerHighDecay READ freqPowerHighDecay NOTIFY configChanged)
    Q_PROPERTY(double freqPowerHighRise  READ freqPowerHighRise  NOTIFY configChanged)

    // Legacy 5-band Q_PROPERTYs (bandSub/Bass/LowMid/Mid/HighMaxBin) removed
    // along with the underlying AudioChannelConfig::bandLayout struct.
    Q_PROPERTY(double noiseGateThreshold READ noiseGateThreshold NOTIFY configChanged)
    Q_PROPERTY(double noiseGateHold READ noiseGateHold NOTIFY configChanged)
    Q_PROPERTY(double volumeSmoothing READ volumeSmoothing NOTIFY configChanged)
    Q_PROPERTY(double brightnessFloor READ brightnessFloor NOTIFY configChanged)

    // Aubio config
    Q_PROPERTY(double filterbankNorm READ filterbankNorm NOTIFY configChanged)
    Q_PROPERTY(double filterbankPower READ filterbankPower NOTIFY configChanged)
    Q_PROPERTY(QString pitchMethod READ pitchMethod NOTIFY configChanged)
    Q_PROPERTY(QString pitchUnit READ pitchUnit NOTIFY configChanged)
    Q_PROPERTY(double pitchSilenceDb READ pitchSilenceDb NOTIFY configChanged)
    Q_PROPERTY(double pitchTolerance READ pitchTolerance NOTIFY configChanged)
    Q_PROPERTY(double tempoSilenceDb READ tempoSilenceDb NOTIFY configChanged)
    Q_PROPERTY(double tempoThreshold READ tempoThreshold NOTIFY configChanged)
    Q_PROPERTY(int tatumSubdivision READ tatumSubdivision NOTIFY configChanged)
    Q_PROPERTY(int beatsPerBar READ beatsPerBar NOTIFY configChanged)
    Q_PROPERTY(bool preEmphasisEnabled READ preEmphasisEnabled NOTIFY configChanged)
    Q_PROPERTY(double tssAlpha READ tssAlpha NOTIFY configChanged)
    Q_PROPERTY(double tssBeta READ tssBeta NOTIFY configChanged)
    Q_PROPERTY(double tssThreshold READ tssThreshold NOTIFY configChanged)

    // New aubio config (phase vocoder, mel filterbank, onset extras, tempo
    // delay, note detection, MFCC).
    Q_PROPERTY(QString windowType READ windowType NOTIFY configChanged)
    Q_PROPERTY(QString melScale READ melScale NOTIFY configChanged)
    Q_PROPERTY(double tempoDelayMs READ tempoDelayMs NOTIFY configChanged)
    Q_PROPERTY(double noteSilenceDb READ noteSilenceDb NOTIFY configChanged)
    Q_PROPERTY(double noteMinIntervalMs READ noteMinIntervalMs NOTIFY configChanged)
    Q_PROPERTY(double noteReleaseDropDb READ noteReleaseDropDb NOTIFY configChanged)
    Q_PROPERTY(double mfccPower READ mfccPower NOTIFY configChanged)
    Q_PROPERTY(double mfccScale READ mfccScale NOTIFY configChanged)
    Q_PROPERTY(QVariantList onsetMethodsEnabled READ onsetMethodsEnabled NOTIFY configChanged)
    // Per-method onset parameter overrides. Each entry is a JS object with
    // doubles {threshold, silenceDb, minioiMs, delayMs, compression, awhitening}.
    // Sentinel values (negative threshold, silenceDb<=-900, etc.) signal
    // "use aubio's tuned default for this method" — see audiochannelconfig.h.
    Q_PROPERTY(QVariantList onsetMethodOverrides READ onsetMethodOverrides NOTIFY configChanged)

    Q_PROPERTY(int windowSize READ windowSizeConst CONSTANT)
    Q_PROPERTY(int hopSize READ hopSizeConst CONSTANT)
    Q_PROPERTY(int sampleRate READ sampleRateValue CONSTANT)
    Q_PROPERTY(int framesPerSecond READ framesPerSecond NOTIFY configChanged)
    Q_PROPERTY(int onsetHistorySeconds READ onsetHistorySeconds WRITE setOnsetHistorySeconds NOTIFY configChanged)

    Q_PROPERTY(double volumeRaw READ volumeRaw NOTIFY audioSnapshotChanged)
    Q_PROPERTY(double volumeSmoothedValue READ volumeSmoothedValue NOTIFY audioSnapshotChanged)
    Q_PROPERTY(double volumeNormalized READ volumeNormalized NOTIFY audioSnapshotChanged)
    // LedFx audio.py:1021 — 0..1 normalized volume (1 + db_spl/100).
    // Diagnostic display alongside rmsDb; comparable to LedFx min_volume = 0.2.
    Q_PROPERTY(double volumeNorm READ volumeNorm NOTIFY audioSnapshotChanged)
    Q_PROPERTY(double rmsDb READ rmsDb NOTIFY audioSnapshotChanged)
    Q_PROPERTY(double peakDb READ peakDb NOTIFY audioSnapshotChanged)
    Q_PROPERTY(double flux READ flux NOTIFY audioSnapshotChanged)
    Q_PROPERTY(bool noiseGateOpen READ noiseGateOpen NOTIFY audioSnapshotChanged)
    Q_PROPERTY(QVariantList triggerStates READ triggerStates NOTIFY audioSnapshotChanged)

    // Aubio snapshot fields
    Q_PROPERTY(double pitchHz READ pitchHz NOTIFY audioSnapshotChanged)
    Q_PROPERTY(double pitchConfidence READ pitchConfidence NOTIFY audioSnapshotChanged)
    Q_PROPERTY(double detectedBpm READ detectedBpm NOTIFY audioSnapshotChanged)
    Q_PROPERTY(double beatConfidence READ beatConfidence NOTIFY audioSnapshotChanged)
    Q_PROPERTY(double beatPhase READ beatPhase NOTIFY audioSnapshotChanged)
    Q_PROPERTY(double barPhase READ barPhase NOTIFY audioSnapshotChanged)
    // Per-bin transient/steady cvec norms from aubio_tss_do (last hop). The
    // QML side does any derivations (sums, ratios, viz tinting).
    // Per-method onset booleans from aubio (last hop). 9 entries in a fixed
    // order: energy, hfc, complex, phase, wphase, specdiff, kl, mkl, specflux.
    // Any vote-counting / aggregation is a QLC+-side derivation done in QML.
    Q_PROPERTY(QVariantList onsetFlags READ onsetFlags NOTIFY audioSnapshotChanged)
    Q_PROPERTY(QVariantList tssTransientNorm READ tssTransientNorm NOTIFY audioSnapshotChanged)
    Q_PROPERTY(QVariantList tssSteadyNorm READ tssSteadyNorm NOTIFY audioSnapshotChanged)
    Q_PROPERTY(int tssBinCount READ tssBinCount NOTIFY audioSnapshotChanged)
    Q_PROPERTY(QVariantList melSpectrum READ melSpectrum NOTIFY audioSnapshotChanged)
    // Post-processed mel spectrum (LedFx pipeline) and novelty signal. Equal
    // to melSpectrum / zeroes respectively when post-processing is bypassed.
    Q_PROPERTY(QVariantList melSpectrumProcessed READ melSpectrumProcessed NOTIFY audioSnapshotChanged)
    Q_PROPERTY(QVariantList melSpectrumNovelty READ melSpectrumNovelty NOTIFY audioSnapshotChanged)
    Q_PROPERTY(QVariantList mfccCoeffs READ mfccCoeffs NOTIFY audioSnapshotChanged)

    // Kick detector live state (for QML lamp + meter). Use kickActive (with
    // hold) for the QML lamp — kickFired is single-hop and the QML rate-limit
    // may miss it.
    Q_PROPERTY(double kickValue READ kickValue NOTIFY audioSnapshotChanged)
    Q_PROPERTY(bool kickActive READ kickActive NOTIFY audioSnapshotChanged)
    Q_PROPERTY(bool kickFired READ kickFired NOTIFY audioSnapshotChanged)
    // UI-only sticky lamp state. Latches on every audio hop where the kick
    // was active or fired, then stays true until kKickLampHoldMs after the
    // last hit. Bridges the gap between the ~86Hz audio loop and the ~25Hz
    // QML refresh so short kicks (holdMs=50) are never visually missed.
    Q_PROPERTY(bool kickLampActive READ kickLampActive NOTIFY audioSnapshotChanged)

    // Per-method onset descriptor diagnostics (raw aubio_onset_get_descriptor /
    // aubio_onset_get_thresholded_descriptor outputs). Same 9-entry order as
    // onsetFlags. Useful for tuning per-method thresholds in the UI.
    Q_PROPERTY(QVariantList onsetDescriptorValues READ onsetDescriptorValues NOTIFY audioSnapshotChanged)
    Q_PROPERTY(QVariantList onsetThresholdedValues READ onsetThresholdedValues NOTIFY audioSnapshotChanged)

    // Note: timelineFrames() is intentionally NOT a Q_PROPERTY anymore.
    // It used to be bound from QML and was rebuilt on every audioSnapshotChanged
    // (~43Hz), allocating ~2064 QVariantMaps per emit and freezing the UI after
    // ~90 seconds. Use the Q_INVOKABLE timelineFramesSnapshot() if you need a
    // one-shot diagnostic dump from QML.

    // Note detection snapshot (raw aubio_notes_do output via the snapshot).
    Q_PROPERTY(double noteMidi READ noteMidi NOTIFY audioSnapshotChanged)
    Q_PROPERTY(double noteVelocity READ noteVelocity NOTIFY audioSnapshotChanged)
    Q_PROPERTY(bool noteOn READ noteOn NOTIFY audioSnapshotChanged)
    Q_PROPERTY(bool noteOff READ noteOff NOTIFY audioSnapshotChanged)

    // Mel band crossover indices (for QML coloring) — only Low/Mid splits
    // remain; the legacy 5-band bandLayout was deleted.
    Q_PROPERTY(int melCrossLowMid READ melCrossLowMid NOTIFY configChanged)
    Q_PROPERTY(int melCrossMid READ melCrossMid NOTIFY configChanged)

    // Multi-resolution mel banks: live spectrum values for each of the three
    // nested matt-mel banks (Phase 3+4). Each list contains the bank's
    // processed mel values (length = configured band count, default 24).
    Q_PROPERTY(QVariantList melLowValues READ melLowValues NOTIFY audioSnapshotChanged)
    Q_PROPERTY(QVariantList melMidValues READ melMidValues NOTIFY audioSnapshotChanged)
    Q_PROPERTY(QVariantList melHighValues READ melHighValues NOTIFY audioSnapshotChanged)
    // Q_PROPERTY(QString melPreset ...) DELETED — see plan-clean-engineering.md §0.
    Q_PROPERTY(double melLowMinHz READ melLowMinHz NOTIFY configChanged)
    Q_PROPERTY(double melLowMaxHz READ melLowMaxHz NOTIFY configChanged)
    Q_PROPERTY(int    melLowBands READ melLowBands NOTIFY configChanged)
    Q_PROPERTY(double melMidMinHz READ melMidMinHz NOTIFY configChanged)
    Q_PROPERTY(double melMidMaxHz READ melMidMaxHz NOTIFY configChanged)
    Q_PROPERTY(int    melMidBands READ melMidBands NOTIFY configChanged)
    Q_PROPERTY(double melHighMinHz READ melHighMinHz NOTIFY configChanged)
    Q_PROPERTY(double melHighMaxHz READ melHighMaxHz NOTIFY configChanged)
    Q_PROPERTY(int    melHighBands READ melHighBands NOTIFY configChanged)

    // Per-bank AGC alpha (LedFx melbank.py:375 mel_gain alpha_decay/alpha_rise).
    Q_PROPERTY(double melLowAgcDecay  READ melLowAgcDecay  NOTIFY configChanged)
    Q_PROPERTY(double melLowAgcRise   READ melLowAgcRise   NOTIFY configChanged)
    Q_PROPERTY(double melMidAgcDecay  READ melMidAgcDecay  NOTIFY configChanged)
    Q_PROPERTY(double melMidAgcRise   READ melMidAgcRise   NOTIFY configChanged)
    Q_PROPERTY(double melHighAgcDecay READ melHighAgcDecay NOTIFY configChanged)
    Q_PROPERTY(double melHighAgcRise  READ melHighAgcRise  NOTIFY configChanged)

    // Per-bank MelPostConfig fields (LedFx melbank.py:374-378). Each bank owns
    // its own ExpFilter chain — these expose the non-AGC knobs to QML.
    Q_PROPERTY(double melLowPowerFactor   READ melLowPowerFactor   NOTIFY configChanged)
    Q_PROPERTY(double melLowGaussianSigma READ melLowGaussianSigma NOTIFY configChanged)
    Q_PROPERTY(double melLowSmoothDecay   READ melLowSmoothDecay   NOTIFY configChanged)
    Q_PROPERTY(double melLowSmoothRise    READ melLowSmoothRise    NOTIFY configChanged)
    Q_PROPERTY(double melLowCommonDecay   READ melLowCommonDecay   NOTIFY configChanged)
    Q_PROPERTY(double melLowCommonRise    READ melLowCommonRise    NOTIFY configChanged)
    Q_PROPERTY(double melLowDiffDecay     READ melLowDiffDecay     NOTIFY configChanged)
    Q_PROPERTY(double melLowDiffRise      READ melLowDiffRise      NOTIFY configChanged)
    Q_PROPERTY(bool   melLowEnabled       READ melLowEnabled       NOTIFY configChanged)
    Q_PROPERTY(double melMidPowerFactor   READ melMidPowerFactor   NOTIFY configChanged)
    Q_PROPERTY(double melMidGaussianSigma READ melMidGaussianSigma NOTIFY configChanged)
    Q_PROPERTY(double melMidSmoothDecay   READ melMidSmoothDecay   NOTIFY configChanged)
    Q_PROPERTY(double melMidSmoothRise    READ melMidSmoothRise    NOTIFY configChanged)
    Q_PROPERTY(double melMidCommonDecay   READ melMidCommonDecay   NOTIFY configChanged)
    Q_PROPERTY(double melMidCommonRise    READ melMidCommonRise    NOTIFY configChanged)
    Q_PROPERTY(double melMidDiffDecay     READ melMidDiffDecay     NOTIFY configChanged)
    Q_PROPERTY(double melMidDiffRise      READ melMidDiffRise      NOTIFY configChanged)
    Q_PROPERTY(bool   melMidEnabled       READ melMidEnabled       NOTIFY configChanged)
    Q_PROPERTY(double melHighPowerFactor   READ melHighPowerFactor   NOTIFY configChanged)
    Q_PROPERTY(double melHighGaussianSigma READ melHighGaussianSigma NOTIFY configChanged)
    Q_PROPERTY(double melHighSmoothDecay   READ melHighSmoothDecay   NOTIFY configChanged)
    Q_PROPERTY(double melHighSmoothRise    READ melHighSmoothRise    NOTIFY configChanged)
    Q_PROPERTY(double melHighCommonDecay   READ melHighCommonDecay   NOTIFY configChanged)
    Q_PROPERTY(double melHighCommonRise    READ melHighCommonRise    NOTIFY configChanged)
    Q_PROPERTY(double melHighDiffDecay     READ melHighDiffDecay     NOTIFY configChanged)
    Q_PROPERTY(double melHighDiffRise      READ melHighDiffRise      NOTIFY configChanged)
    Q_PROPERTY(bool   melHighEnabled       READ melHighEnabled       NOTIFY configChanged)

    // Tempo decay-on-silence + selectable aubio tempo method.
    Q_PROPERTY(QString tempoMethod READ tempoMethod NOTIFY configChanged)
    Q_PROPERTY(double coastBeats READ coastBeats NOTIFY configChanged)
    Q_PROPERTY(double tempoDecayHalfLifeBeats READ tempoDecayHalfLifeBeats NOTIFY configChanged)
    Q_PROPERTY(double tempoDecayTargetBpm READ tempoDecayTargetBpm NOTIFY configChanged)

    // Primary onset method selector (index into onsetMethodEnabled[]).
    Q_PROPERTY(int onsetMethodIndex READ onsetMethodIndex NOTIFY configChanged)

    Q_PROPERTY(AudioProfileListModel* profileListModel READ profileListModel CONSTANT)

    Q_PROPERTY(double lowsPower READ lowsPower NOTIFY audioLevelsChanged)
    Q_PROPERTY(double midsPower READ midsPower NOTIFY audioLevelsChanged)
    Q_PROPERTY(double highsPower READ highsPower NOTIFY audioLevelsChanged)
    Q_PROPERTY(bool triggerLowActive  READ triggerLowActive  NOTIFY audioLevelsChanged)
    Q_PROPERTY(bool triggerMidActive  READ triggerMidActive  NOTIFY audioLevelsChanged)
    Q_PROPERTY(bool triggerHighActive READ triggerHighActive NOTIFY audioLevelsChanged)
    Q_PROPERTY(bool beatActive READ beatActive NOTIFY beatActiveChanged)
    Q_PROPERTY(int lowCutBin READ lowCutBin NOTIFY barsNumberChanged)
    Q_PROPERTY(int highCutBin READ highCutBin NOTIFY barsNumberChanged)

    // -------------------------------------------------------------------
    // Display-ready scalars (Phase A — see docs/audio-dsp-plans/plan-widget-impl.md).
    // All math (slicing, log scaling, dB mapping, peak normalisation) is
    // done in C++ inside updateAudioProfileSnapshotPowers(). QML reads these
    // as 0..1 (or formatted strings) and renders them as-is.
    // -------------------------------------------------------------------

    /// Power slices — Fix 2 of plan-dsp-harmonize.md. These properties now
    /// alias the canonical AudioChannel values (`snap.lows/.mids/.highs`,
    /// computed via the LedFx-parity `freq_power_filter`,
    /// audio.py:1306 get_freq_power(i, filtered=True)). Names are kept for
    /// QML binding compatibility (VCAudioTriggersItem.qml). The widget no
    /// longer re-slices the master mel — it surfaces the real pipeline value.
    Q_PROPERTY(double lowsPowerSliced  READ lowsPowerSliced  NOTIFY audioSnapshotChanged)
    Q_PROPERTY(double midsPowerSliced  READ midsPowerSliced  NOTIFY audioSnapshotChanged)
    Q_PROPERTY(double highsPowerSliced READ highsPowerSliced NOTIFY audioSnapshotChanged)

    /// LedFx audio.py:1306 get_freq_power(0/1, filtered=True) — beat (0-100 Hz)
    /// and bass (100-250 Hz). The sliced lowsPower above is (beat+bass)/2.
    Q_PROPERTY(double beatPower READ beatPower NOTIFY audioSnapshotChanged)
    Q_PROPERTY(double bassPower READ bassPower NOTIFY audioSnapshotChanged)

    /// AGC scalar (`mel_gain`) currently applied by the MelPostProcessor.
    /// 1.0 when post-processing is disabled. NOTIFY audioSnapshotChanged
    /// since it updates every hop.
    Q_PROPERTY(double melAgcGain READ melAgcGain NOTIFY audioSnapshotChanged)

    /// Per-method onset descriptors normalised by a C++-tracked running
    /// peak (decay 0.995 / floor 0.001 per audio hop). 9 doubles, each 0..1.
    /// QML draws sparkline bars at `value * lane_height` — no further math.
    Q_PROPERTY(QVariantList onsetDescriptorDisplay READ onsetDescriptorDisplay NOTIFY audioSnapshotChanged)

    /// Pitch display value: log2(pitchHz / 20) / log2(20000 / 20), clamped
    /// to 0..1. 0 when pitchHz <= 0 (silence). The raw `pitchHz` Q_PROPERTY
    /// stays available for the numeric note display.
    Q_PROPERTY(double pitchDisplay READ pitchDisplay NOTIFY audioSnapshotChanged)

    /// Note-name string for the current pitch ("C4", "A#3", "--" when no
    /// audible pitch). Computed in C++ from pitchHz.
    Q_PROPERTY(QString pitchNoteText READ pitchNoteText NOTIFY audioSnapshotChanged)

    /// Spectral feature display scalars (all 0..1, ready for pixel mapping).
    /// - centroid: log-scaled over [20 .. 15000] Hz
    /// - flatness: passthrough (already 0..1 in the snapshot)
    /// - flux:     normalised against a C++-tracked running peak (decay 0.995)
    /// - rms:      (rmsDb + 96) / 96, clamped 0..1
    Q_PROPERTY(double spectralCentroidDisplay READ spectralCentroidDisplay NOTIFY audioSnapshotChanged)
    Q_PROPERTY(double spectralFlatnessDisplay READ spectralFlatnessDisplay NOTIFY audioSnapshotChanged)
    Q_PROPERTY(double fluxDisplay  READ fluxDisplay  NOTIFY audioSnapshotChanged)
    Q_PROPERTY(double rmsDisplay   READ rmsDisplay   NOTIFY audioSnapshotChanged)

    /// Normalized 0..1 scale factor (multiplier) used by the QML signed-bar
    /// widget. The QML applies: barHeight = clamp(sectionH/2,
    /// |mfccCoeffs[i]| * mfccDisplayScale * (parent.height/2)).
    /// Owned by C++ so QML stays math-free; recomputed on configChanged.
    Q_PROPERTY(double mfccDisplayScale READ mfccDisplayScale NOTIFY configChanged)

    /// TSS scalar levels (mean of per-bin transient / steady cvec norms in
    /// the current snapshot, clamped 0..1). Sparklines use these — the raw
    /// `tssTransientNorm` / `tssSteadyNorm` arrays remain for diagnostics.
    Q_PROPERTY(double tssTransientLevel READ tssTransientLevel NOTIFY audioSnapshotChanged)
    Q_PROPERTY(double tssSteadyLevel    READ tssSteadyLevel    NOTIFY audioSnapshotChanged)

    /// Power-bar Hz crossovers (LedFx parity defaults: 100/250/3000/10000).
    /// Validated to stay strictly increasing via setBeatCutoffHz() etc.
    Q_PROPERTY(double beatCutoffHz  READ beatCutoffHz  NOTIFY configChanged)
    Q_PROPERTY(double bassCutoffHz  READ bassCutoffHz  NOTIFY configChanged)
    Q_PROPERTY(double midsCutoffHz  READ midsCutoffHz  NOTIFY configChanged)
    Q_PROPERTY(double highsCutoffHz READ highsCutoffHz NOTIFY configChanged)

    Q_PROPERTY(QVariant groupsTreeModel READ groupsTreeModel NOTIFY groupsTreeModelChanged)
    Q_PROPERTY(QString searchFilter READ searchFilter WRITE setSearchFilter NOTIFY searchFilterChanged)

    /*********************************************************************
     * Initialization
     *********************************************************************/
public:
    VCAudioTriggers(Doc* doc = nullptr, VirtualConsole *vc = nullptr, QObject *parent = nullptr);
    virtual ~VCAudioTriggers();

    /** @reimp */
    QString defaultCaption() const override;

    /** @reimp */
    void setupLookAndFeel(qreal pixelDensity, int page) override;

    /** @reimp */
    void render(QQuickView *view, QQuickItem *parent) override;

    /** @reimp */
    QString propertiesResource() const override;

    /** @reimp */
    VCWidget *createCopy(VCWidget *parent) const override;

    /** Get/Set the capture enable status of this widget */
    bool captureEnabled() const;
    void setCaptureEnabled(bool enable);

    /** Get/Set the capture volume level */
    uchar volumeLevel() const;
    void setVolumeLevel(uchar level);

    /** Get/Set the number of spectrum bars */
    int barsNumber() const;
    void setBarsNumber(int num);
    int selectedBar() const;
    void setSelectedBar(int index);

    QVariantList audioLevels() const;

    quint32 audioProfileId() const;
    void setAudioProfileId(quint32 id);

    double lowsPower() const;
    double midsPower() const;
    double highsPower() const;
    bool triggerLowActive() const;
    bool triggerMidActive() const;
    bool triggerHighActive() const;
    bool beatActive() const;
    int lowCutBin() const;
    int highCutBin() const;

    // Phase A display-ready scalars — see Q_PROPERTY block above for units.
    // *Sliced getters alias the canonical snapshot values (Fix 2).
    double lowsPowerSliced() const  { return m_cachedSnapshot.lows;  }
    double midsPowerSliced() const  { return m_cachedSnapshot.mids;  }
    double highsPowerSliced() const { return m_cachedSnapshot.highs; }
    // LedFx audio.py:1306 get_freq_power(0, filtered=True) — beat (0-100 Hz)
    double beatPower() const        { return m_cachedSnapshot.beatPower;  }
    // LedFx audio.py:1306 get_freq_power(1, filtered=True) — bass (100-250 Hz)
    double bassPower() const        { return m_cachedSnapshot.bassPower;  }
    double melAgcGain() const       { return m_melAgcGain;       }
    QVariantList onsetDescriptorDisplay() const { return m_onsetDescriptorDisplayCache; }
    double pitchDisplay() const     { return m_pitchDisplay; }
    QString pitchNoteText() const   { return m_pitchNoteText; }
    double spectralCentroidDisplay() const { return m_spectralCentroidDisplay; }
    double spectralFlatnessDisplay() const { return m_spectralFlatnessDisplay; }
    double fluxDisplay() const      { return m_fluxDisplay; }
    double rmsDisplay() const       { return m_rmsDisplay;  }
    double mfccDisplayScale() const;
    double tssTransientLevel() const { return m_tssTransientLevel; }
    double tssSteadyLevel() const    { return m_tssSteadyLevel;    }

    double beatCutoffHz()  const { return m_beatCutoffHz;  }
    double bassCutoffHz()  const { return m_bassCutoffHz;  }
    double midsCutoffHz()  const { return m_midsCutoffHz;  }
    double highsCutoffHz() const { return m_highsCutoffHz; }

    /// Power-bar Hz crossover setters. Each call clamps the new value to
    /// keep the sequence strictly increasing (`beat < bass < mids < highs`)
    /// and within `[10 Hz, Nyquist]`. Returns silently when the request
    /// would violate ordering. Triggers configChanged + a snapshot rebuild
    /// (diagnostic / legacy — these no longer drive the canonical
    /// lows/mids/highs, which come from AudioChannel snap.*).
    Q_INVOKABLE void setBeatCutoffHz(double hz);
    Q_INVOKABLE void setBassCutoffHz(double hz);
    Q_INVOKABLE void setMidsCutoffHz(double hz);
    Q_INVOKABLE void setHighsCutoffHz(double hz);

    double envelopeAttack() const;
    double envelopeRelease() const;
    double triggerLowHigh() const;
    double triggerLowLow() const;
    double triggerLowHold() const;
    double triggerLowCooldown() const;
    double triggerMidHigh() const;
    double triggerMidLow() const;
    double triggerMidHold() const;
    double triggerMidCooldown() const;
    double triggerHighHigh() const;
    double triggerHighLow() const;
    double triggerHighHold() const;
    double triggerHighCooldown() const;

    double kickBeatMaxHz() const;
    double kickBeatMinPercentDiff() const;
    double kickBeatMinAmplitude() const;
    double kickBeatRefractorySec() const;
    int kickBeatHistoryLen() const;
    bool kickEnabled() const;

    bool melPostEnabled() const;
    double melPowerFactor() const;
    double melGaussianSigma() const;
    double melSmoothDecay() const;
    double melSmoothRise() const;
    double melCommonDecay() const;
    double melCommonRise() const;
    double melDiffDecay() const;
    double melDiffRise() const;
    double freqPowerBeatDecay() const;
    double freqPowerBeatRise() const;
    double freqPowerBassDecay() const;
    double freqPowerBassRise() const;
    double freqPowerMidsDecay() const;
    double freqPowerMidsRise() const;
    double freqPowerHighDecay() const;
    double freqPowerHighRise() const;

    double noiseGateThreshold() const;
    double noiseGateHold() const;
    double volumeSmoothing() const;
    double brightnessFloor() const;

    double filterbankNorm() const;
    double filterbankPower() const;
    QString pitchMethod() const;
    QString pitchUnit() const;
    double pitchSilenceDb() const;
    double pitchTolerance() const;
    double tempoSilenceDb() const;
    double tempoThreshold() const;
    int tatumSubdivision() const;
    int beatsPerBar() const;
    bool preEmphasisEnabled() const;
    double tssAlpha() const;
    double tssBeta() const;
    double tssThreshold() const;

    QString windowType() const;
    QString melScale() const;
    double tempoDelayMs() const;
    double noteSilenceDb() const;
    double noteMinIntervalMs() const;
    double noteReleaseDropDb() const;
    double mfccPower() const;
    double mfccScale() const;
    QVariantList onsetMethodsEnabled() const;
    QVariantList onsetMethodOverrides() const;

    int windowSizeConst() const { return 1024; }
    int hopSizeConst() const { return 512; }
    int sampleRateValue() const;
    int framesPerSecond() const;
    int onsetHistorySeconds() const { return m_onsetHistorySeconds; }

    double pitchHz() const;
    double pitchConfidence() const;
    double detectedBpm() const;
    double beatConfidence() const;
    double beatPhase() const;
    double barPhase() const;
    QVariantList tssTransientNorm() const;
    QVariantList tssSteadyNorm() const;
    int tssBinCount() const;
    QVariantList onsetFlags() const;
    QVariantList melSpectrum() const;
    QVariantList melSpectrumProcessed() const;
    QVariantList melSpectrumNovelty() const;
    QVariantList mfccCoeffs() const;
    QVariantList onsetDescriptorValues() const;
    QVariantList onsetThresholdedValues() const;
    Q_INVOKABLE QVariantList timelineFramesSnapshot() const;
    double noteMidi() const;
    double noteVelocity() const;
    bool noteOn() const;
    bool noteOff() const;

    int melCrossLowMid() const;
    int melCrossMid() const;

    QVariantList melLowValues() const;
    QVariantList melMidValues() const;
    QVariantList melHighValues() const;
    // QString melPreset() DELETED — see plan-clean-engineering.md §0.
    double melLowMinHz() const;
    double melLowMaxHz() const;
    int    melLowBands() const;
    double melMidMinHz() const;
    double melMidMaxHz() const;
    int    melMidBands() const;
    double melHighMinHz() const;
    double melHighMaxHz() const;
    int    melHighBands() const;

    double melLowAgcDecay()  const;
    double melLowAgcRise()   const;
    double melMidAgcDecay()  const;
    double melMidAgcRise()   const;
    double melHighAgcDecay() const;
    double melHighAgcRise()  const;

    double melLowPowerFactor() const;
    double melLowGaussianSigma() const;
    double melLowSmoothDecay() const;
    double melLowSmoothRise() const;
    double melLowCommonDecay() const;
    double melLowCommonRise() const;
    double melLowDiffDecay() const;
    double melLowDiffRise() const;
    bool   melLowEnabled() const;
    double melMidPowerFactor() const;
    double melMidGaussianSigma() const;
    double melMidSmoothDecay() const;
    double melMidSmoothRise() const;
    double melMidCommonDecay() const;
    double melMidCommonRise() const;
    double melMidDiffDecay() const;
    double melMidDiffRise() const;
    bool   melMidEnabled() const;
    double melHighPowerFactor() const;
    double melHighGaussianSigma() const;
    double melHighSmoothDecay() const;
    double melHighSmoothRise() const;
    double melHighCommonDecay() const;
    double melHighCommonRise() const;
    double melHighDiffDecay() const;
    double melHighDiffRise() const;
    bool   melHighEnabled() const;

    QString tempoMethod() const;
    double  coastBeats() const;
    double  tempoDecayHalfLifeBeats() const;
    double  tempoDecayTargetBpm() const;

    int onsetMethodIndex() const;

    double volumeRaw() const;
    double volumeSmoothedValue() const;
    double volumeNormalized() const;
    double volumeNorm() const;
    double rmsDb() const;
    double peakDb() const;
    double flux() const;
    bool noiseGateOpen() const;

    double kickValue() const;
    bool kickActive() const;
    bool kickFired() const;
    bool kickLampActive() const;

    AudioProfileListModel* profileListModel();

    Q_INVOKABLE void setEnvelopeAttack(double ms);
    Q_INVOKABLE void setEnvelopeRelease(double ms);
    Q_INVOKABLE void setTriggerLowHigh(double value);
    Q_INVOKABLE void setTriggerLowLow(double value);
    Q_INVOKABLE void setTriggerLowHold(double ms);
    Q_INVOKABLE void setTriggerLowCooldown(double ms);
    Q_INVOKABLE void setTriggerMidHigh(double value);
    Q_INVOKABLE void setTriggerMidLow(double value);
    Q_INVOKABLE void setTriggerMidHold(double ms);
    Q_INVOKABLE void setTriggerMidCooldown(double ms);
    Q_INVOKABLE void setTriggerHighHigh(double value);
    Q_INVOKABLE void setTriggerHighLow(double value);
    Q_INVOKABLE void setTriggerHighHold(double ms);
    Q_INVOKABLE void setTriggerHighCooldown(double ms);

    Q_INVOKABLE void setKickEnabled(bool enabled);
    Q_INVOKABLE void setKickBeatMaxHz(double hz);
    Q_INVOKABLE void setKickBeatMinPercentDiff(double value);
    Q_INVOKABLE void setKickBeatMinAmplitude(double value);
    Q_INVOKABLE void setKickBeatRefractorySec(double sec);
    Q_INVOKABLE void setKickBeatHistoryLen(int frames);

    Q_INVOKABLE void setMelPostEnabled(bool enabled);
    Q_INVOKABLE void setMelPowerFactor(double value);
    Q_INVOKABLE void setMelGaussianSigma(double value);
    Q_INVOKABLE void setMelSmoothDecay(double value);
    Q_INVOKABLE void setMelSmoothRise(double value);
    Q_INVOKABLE void setMelCommonDecay(double value);
    Q_INVOKABLE void setMelCommonRise(double value);
    Q_INVOKABLE void setMelDiffDecay(double value);
    Q_INVOKABLE void setMelDiffRise(double value);
    Q_INVOKABLE void setFreqPowerBeatDecay(double value);
    Q_INVOKABLE void setFreqPowerBeatRise(double value);
    Q_INVOKABLE void setFreqPowerBassDecay(double value);
    Q_INVOKABLE void setFreqPowerBassRise(double value);
    Q_INVOKABLE void setFreqPowerMidsDecay(double value);
    Q_INVOKABLE void setFreqPowerMidsRise(double value);
    Q_INVOKABLE void setFreqPowerHighDecay(double value);
    Q_INVOKABLE void setFreqPowerHighRise(double value);
    // applyMelPreset(raw/ledfx/punchy/smooth) DELETED — see plan-clean-engineering.md §0.

    /// Multi-resolution mel banks (always enabled): per-bank Hz range and band
    /// count plus a preset selector. These write to the associated
    /// AudioProfile's MelBankConfig and persist with the profile.
    Q_INVOKABLE void setMelBankLow(double minHz, double maxHz, int bands);
    Q_INVOKABLE void setMelBankMid(double minHz, double maxHz, int bands);
    Q_INVOKABLE void setMelBankHigh(double minHz, double maxHz, int bands);
    /// Per-bank AGC alpha setters (LedFx melbank.py:375).
    Q_INVOKABLE void setMelLowAgcDecay(double value);
    Q_INVOKABLE void setMelLowAgcRise(double value);
    Q_INVOKABLE void setMelMidAgcDecay(double value);
    Q_INVOKABLE void setMelMidAgcRise(double value);
    Q_INVOKABLE void setMelHighAgcDecay(double value);
    Q_INVOKABLE void setMelHighAgcRise(double value);

    /// Per-bank MelPostConfig non-AGC setters. Each marks preset = "Custom"
    /// (matches setMelLowAgcDecay etc.) so per-bank tuning isn't lost on a
    /// preset re-apply.
    Q_INVOKABLE void setMelLowPowerFactor(double value);
    Q_INVOKABLE void setMelLowGaussianSigma(double value);
    Q_INVOKABLE void setMelLowSmoothDecay(double value);
    Q_INVOKABLE void setMelLowSmoothRise(double value);
    Q_INVOKABLE void setMelLowCommonDecay(double value);
    Q_INVOKABLE void setMelLowCommonRise(double value);
    Q_INVOKABLE void setMelLowDiffDecay(double value);
    Q_INVOKABLE void setMelLowDiffRise(double value);
    Q_INVOKABLE void setMelLowEnabled(bool enabled);
    Q_INVOKABLE void setMelMidPowerFactor(double value);
    Q_INVOKABLE void setMelMidGaussianSigma(double value);
    Q_INVOKABLE void setMelMidSmoothDecay(double value);
    Q_INVOKABLE void setMelMidSmoothRise(double value);
    Q_INVOKABLE void setMelMidCommonDecay(double value);
    Q_INVOKABLE void setMelMidCommonRise(double value);
    Q_INVOKABLE void setMelMidDiffDecay(double value);
    Q_INVOKABLE void setMelMidDiffRise(double value);
    Q_INVOKABLE void setMelMidEnabled(bool enabled);
    Q_INVOKABLE void setMelHighPowerFactor(double value);
    Q_INVOKABLE void setMelHighGaussianSigma(double value);
    Q_INVOKABLE void setMelHighSmoothDecay(double value);
    Q_INVOKABLE void setMelHighSmoothRise(double value);
    Q_INVOKABLE void setMelHighCommonDecay(double value);
    Q_INVOKABLE void setMelHighCommonRise(double value);
    Q_INVOKABLE void setMelHighDiffDecay(double value);
    Q_INVOKABLE void setMelHighDiffRise(double value);
    Q_INVOKABLE void setMelHighEnabled(bool enabled);

    Q_INVOKABLE void setTempoMethod(const QString &method);
    Q_INVOKABLE void setCoastBeats(double beats);
    Q_INVOKABLE void setTempoDecayHalfLifeBeats(double beats);
    Q_INVOKABLE void setTempoDecayTargetBpm(double bpm);
    /// Primary onset method selector (0..AUBIO_ONSET_METHODS-1).
    Q_INVOKABLE void setOnsetMethodIndex(int idx);
    /// Recognized presets (case-insensitive): "EDM", "Live", "Acoustic",
    /// "Speech", "Custom" (no-op — keeps the current per-bank config).
    Q_INVOKABLE void applyMelBankPreset(const QString &preset);

    Q_INVOKABLE void setNoiseGateThreshold(double db);
    Q_INVOKABLE void setNoiseGateHold(double ms);
    Q_INVOKABLE void setVolumeSmoothing(double ms);
    Q_INVOKABLE void setBrightnessFloor(double value);

    Q_INVOKABLE void setFilterbankNorm(double norm);
    Q_INVOKABLE void setFilterbankPower(double power);
    Q_INVOKABLE void setPitchMethod(const QString &method);
    Q_INVOKABLE void setPitchUnit(const QString &unit);
    Q_INVOKABLE void setPitchSilenceDb(double db);
    Q_INVOKABLE void setPitchTolerance(double value);
    Q_INVOKABLE void setTempoSilenceDb(double db);
    Q_INVOKABLE void setTempoThreshold(double value);
    Q_INVOKABLE void setTatumSubdivision(int n);
    Q_INVOKABLE void setBeatsPerBar(int n);
    Q_INVOKABLE void setPreEmphasisEnabled(bool enabled);
    Q_INVOKABLE void setTssAlpha(double value);
    Q_INVOKABLE void setTssBeta(double value);
    Q_INVOKABLE void setTssThreshold(double value);

    Q_INVOKABLE void setWindowType(const QString &type);
    Q_INVOKABLE void setMelScale(const QString &scale);
    Q_INVOKABLE void setTempoDelayMs(double ms);
    Q_INVOKABLE void setNoteSilenceDb(double db);
    Q_INVOKABLE void setNoteMinIntervalMs(double ms);
    Q_INVOKABLE void setNoteReleaseDropDb(double db);
    Q_INVOKABLE void setMfccPower(double power);
    Q_INVOKABLE void setMfccScale(double scale);
    Q_INVOKABLE void setOnsetMethodEnabled(int idx, bool enabled);
    Q_INVOKABLE void setOnsetHistorySeconds(int s);
    // Per-method onset override editors. resetOnsetMethodOverride sets every
    // field of overrides[idx] back to its sentinel ("use aubio default").
    // setOnsetMethodOverrideField uses fieldName in {threshold, silenceDb,
    // minioiMs, delayMs, compression, awhitening}. Pass NaN to clear a single
    // field back to its sentinel.
    Q_INVOKABLE void setOnsetMethodOverrideField(int idx, const QString &fieldName, double value);
    Q_INVOKABLE void resetOnsetMethodOverride(int idx);
    // Reads aubio's tuned defaults for the given method by spinning up a
    // throwaway aubio_onset_t — useful for showing "(default = 0.30)" hints.
    Q_INVOKABLE QVariantMap onsetMethodDefaults(int idx) const;

    Q_INVOKABLE void resetProfileToDefaults();
    Q_INVOKABLE void deleteCurrentProfile();
    Q_INVOKABLE void renameCurrentProfile(const QString &name);
    Q_INVOKABLE void duplicateCurrentProfile(const QString &name);

    Q_INVOKABLE QVariantMap triggerState(int band) const;
    QVariantList triggerStates() const;

signals:
    void captureEnabledChanged();
    void volumeLevelChanged();
    void barsNumberChanged();
    void selectedBarChanged();
    void audioLevelsChanged();
    void beatActiveChanged();
    void audioProfileIdChanged();
    void configChanged();
    void audioSnapshotChanged();

private slots:
    void slotBeatDetected();
    void slotBeatTimeout();

protected:
    /** @reimp */
    bool copyFrom(const VCWidget* widget) override;

private:
    FunctionParent functionParent() const;
    AudioProfile *resolvedAudioProfile() const;
    AudioProfile *editableAudioProfile() const;
    AudioChannelConfig profileChannelConfig() const;
    void applyChannelConfig(const AudioChannelConfig &config);
    void updateAudioProfileSnapshotPowers(bool emitVisuals = true);

private:
    VirtualConsole *m_vc;
    AudioCapture *m_inputCapture;
    bool m_captureEnabled;
    uchar m_volumeLevel;

    QVariantList m_audioLevels;
    quint32 m_audioProfileId = AudioProfile::invalidId();

    bool m_beatActive = false;
    QTimer *m_beatTimer = nullptr;

    AudioSnapshot m_cachedSnapshot;
    // Sticky kick lamp state. Refreshed on every audio hop in
    // updateAudioProfileSnapshotPowers — when kick is active or fired we
    // bump m_kickLampHoldRemainingMs to kKickLampHoldMs; otherwise we
    // decrement by the snapshot's audioDtMs. Lamp is "on" while > 0.
    static constexpr double kKickLampHoldMs = 150.0;
    double m_kickLampHoldRemainingMs = 0.0;
    QVariantList m_triggerStatesCache;
    QVariantList m_melSpectrumCache;
    QVariantList m_melSpectrumProcessedCache;
    QVariantList m_melSpectrumNoveltyCache;
    QVariantList m_melLowValuesCache;
    QVariantList m_melMidValuesCache;
    QVariantList m_melHighValuesCache;
    QVariantList m_mfccCoeffsCache;
    QVariantList m_onsetDescriptorCache;
    QVariantList m_onsetThresholdedCache;
    AudioProfileListModel *m_profileListModel = nullptr;

    // -------------------------------------------------------------------
    // Phase A — display-ready cache. All fields are recomputed in
    // updateAudioProfileSnapshotPowers() from the cached snapshot. QML
    // binds to them via Q_PROPERTY and renders without further math.
    // (Fix 2: m_lowsPowerSliced/m_midsPowerSliced/m_highsPowerSliced
    // removed — *Sliced getters now alias m_cachedSnapshot.lows/.mids/.highs.)
    // -------------------------------------------------------------------
    double m_melAgcGain       = 1.0;
    double m_pitchDisplay     = 0.0;
    QString m_pitchNoteText   = QStringLiteral("--");
    double m_spectralCentroidDisplay = 0.0;
    double m_spectralFlatnessDisplay = 0.0;
    double m_fluxDisplay      = 0.0;
    double m_rmsDisplay       = 0.0;
    double m_tssTransientLevel = 0.0;
    double m_tssSteadyLevel    = 0.0;
    QVariantList m_onsetDescriptorDisplayCache;

    // Per-method onset running peaks for descriptor normalisation. Decay
    // each audio hop by kOnsetPeakDecay, floored at kOnsetPeakFloor; we
    // bump up to the current descriptor whenever it exceeds the tracker
    // (LedFx / typical sparkline auto-range behaviour).
    static constexpr double kOnsetPeakDecay = 0.995;
    static constexpr double kOnsetPeakFloor = 0.001;
    double m_methodPeak[AUBIO_ONSET_METHODS] = {};

    // Long-window flux peak tracker for fluxDisplay (same shape as the
    // onset descriptor peaks).
    static constexpr double kFluxPeakDecay = 0.995;
    static constexpr double kFluxPeakFloor = 0.001;
    double m_fluxPeak = kFluxPeakFloor;

    // Power-bar crossover defaults match LedFx audio.py:1107-1112
    // freq_max_mels = [100, 250, 3000, 10000].
    double m_beatCutoffHz  = 100.0;
    double m_bassCutoffHz  = 250.0;
    double m_midsCutoffHz  = 3000.0;
    double m_highsCutoffHz = 10000.0;

    static constexpr int kTimelineCapacity = 2064; // ~24s at 86Hz (44100/512)
    QVector<TimelineFrame> m_timeline;
    int m_timelineWriteIdx = 0;
    // Time-based throttle for QML audioSnapshotChanged emissions (~30 Hz).
    // Engine snapshot/DMX still update every aubio hop; only the QML-facing
    // signal is rate-limited to avoid binding storms.
    QElapsedTimer m_uiThrottleTimer;
    static constexpr int kUiUpdateIntervalMs = 33; // ~30 Hz
    int m_onsetHistorySeconds = 5;

    /*********************************************************************
     * Spectrum & Volume bars
     *********************************************************************/
public:
    enum BarType
    {
        None = 0,
        DMXBar,
        FunctionBar,
        VCWidgetBar
    };
    Q_ENUM(BarType)

    enum BandSource
    {
        BandLow = 0,
        BandMid,
        BandHigh,
        BandVolume,
        BandBeat,
        BandKick,
        BandSourceCount = 6
    };
    Q_ENUM(BandSource)

    struct BandMapping
    {
        BandSource source = BandLow;
        BarType type = BarType::None;

        // DMX
        QList<SceneValue> dmxChannels;
        QList<int> absDmxChannels;
        double dmxScale = 1.0;
        uchar dmxFloor = 0;
        int beatHoldMs = 80;

        // Function
        quint32 functionId = Function::invalidId();
        Function *function = nullptr;

        // Widget
        quint32 widgetId = VCWidget::invalidId();
        VCWidget *widget = nullptr;
        int divisor = 1;
        bool tapped = false;
        int skippedBeats = 0;

        // Runtime
        // Written on GUI thread, read on MasterTimer thread.
        // On x86-64, aligned double writes are atomic at hardware level.
        // Use volatile to prevent compiler reordering.
        volatile double lastNorm = 0.0;
        uchar m_value = 0; // 0..255 mirror of lastNorm for DMX/widget compat
    };

    Q_INVOKABLE void selectBarForEditing(int index);
    QVariantList barsInfo() const;

    Q_INVOKABLE void setBarType(BarType type);
    Q_INVOKABLE void setBarThresholds(uchar minThr, uchar maxThr);
    Q_INVOKABLE void setBarFunction(quint32 functionId);
    Q_INVOKABLE void setBarWidget(quint32 widgetId);
    Q_INVOKABLE void setBarDmxScale(double scale);
    Q_INVOKABLE void setBarDmxFloor(int floor0to255);
    Q_INVOKABLE void setBarBeatHoldMs(int ms);
    void setBarDmxChannels(QList<SceneValue>list);

protected slots:
    /** Receives one analyzed audio frame from AudioCapture (aubio + snapshot already
     *  computed on the capture thread). Updates spectrum bars from the snapshot,
     *  drives DMX/Function/Widget triggers, and emits audioLevelsChanged(). */
    void slotAubioDataReady(const AubioResults &results, quint32 power);

signals:
    void barsInfoChanged();
    /** Notify the listeners that the fixture tree model has changed */
    void groupsTreeModelChanged();
    /** Notify the listeners that the search filter has changed */
    void searchFilterChanged();

private:
    void updateBarWidgetReference(BandMapping &bm) const;
    void checkWidgetFunctionality(BandMapping &bm, const TriggerState &ts) const;
    void rebuildBarAbsDmxChannels(BandMapping &bm) const;

private:
    /** Fixed-size 7-entry array of source -> action mappings. */
    QVector<BandMapping> m_bandMappings;
    mutable QMutex m_mappingsMutex;  // protects m_bandMappings access across GUI/MasterTimer threads

    /** Hold-until timestamp (ms since epoch, monotonic-ish via QDateTime) for
     *  Beat source DMX strobe. Set on the audio thread when a beat fires,
     *  read on the MasterTimer thread inside writeDMX(). */
    std::atomic<qint64> m_beatUntilMs { 0 };

    /** Index of the bar currently being edited.
     *  This is needed to simplify the widget editing */
    int m_selectedBar;

    /*********************************************************************
     * Fixture tree methods
     *********************************************************************/
public:
    /** Returns the data model to display a tree of FixtureGroups/Fixtures */
    QVariant groupsTreeModel();

    /** Get/Set a string to filter Group/Fixture/Channel names */
    QString searchFilter() const;
    void setSearchFilter(const QString &searchFilter);

    Q_INVOKABLE void applyToSameType(bool enable);

private:
    void updateFixtureTree();

    /** Recursive method to check/uncheck channels for fixtures of the same type */
    void checkFixtureTree(TreeModel *tree, Fixture *sourceFixture, quint32 channelIndex, bool checked);

protected slots:
    void slotTreeDataChanged(TreeModelItem *item, int role, const QVariant &value);

private:
    /** Data model used by the QML UI to represent groups/fixtures/channels */
    TreeModel *m_fixtureTree;
    /** A string to filter the displayed tree items */
    QString m_searchFilter;

    /** Flag to apply a channel selection to all
     *  the fixtures of the same type */
    bool m_applyToSameType, m_isUpdating;

    /*********************************************************************
     * External input
     *********************************************************************/
public:
    /** @reimp */
    void updateFeedback() override;

public slots:
    /** @reimp */
    void slotInputValueChanged(quint8 id, uchar value) override;

    /*********************************************************************
     * DMXSource
     *********************************************************************/
public:
    /** @reimpl */
    void writeDMX(MasterTimer* timer, QList<Universe*> universes) override;

private:
    /** Map used to lookup a GenericFader instance for a Universe ID */
    QMap<quint32, QSharedPointer<GenericFader> > m_fadersMap;

    /*********************************************************************
     * Load & Save
     *********************************************************************/
public:
    bool loadBarXML(QXmlStreamReader &root);
    bool saveBarXML(QXmlStreamWriter *doc, int index) const;

    bool loadXML(QXmlStreamReader &root) override;
    bool saveXML(QXmlStreamWriter *doc) const override;
};

#endif
