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
#include <QHash>
#include <QVector>

class QTimer;
#define KXMLQLCVCAudioTriggers QStringLiteral("AudioTriggers")

class AudioCapture;
class VirtualConsole;
class VCAudioTriggers;
struct AubioResults;

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
    Q_PROPERTY(double triggerHigh READ triggerHigh NOTIFY configChanged)
    Q_PROPERTY(double triggerLow READ triggerLow NOTIFY configChanged)
    Q_PROPERTY(double triggerCooldown READ triggerCooldown NOTIFY configChanged)
    Q_PROPERTY(double triggerHold READ triggerHold NOTIFY configChanged)

    Q_PROPERTY(double bandSubMaxHz READ bandSubMaxHz NOTIFY configChanged)
    Q_PROPERTY(double bandBassMaxHz READ bandBassMaxHz NOTIFY configChanged)
    Q_PROPERTY(double bandLowMidMaxHz READ bandLowMidMaxHz NOTIFY configChanged)
    Q_PROPERTY(double bandMidMaxHz READ bandMidMaxHz NOTIFY configChanged)
    Q_PROPERTY(double bandHighMaxHz READ bandHighMaxHz NOTIFY configChanged)
    Q_PROPERTY(double noiseGateThreshold READ noiseGateThreshold NOTIFY configChanged)
    Q_PROPERTY(double noiseGateHold READ noiseGateHold NOTIFY configChanged)
    Q_PROPERTY(double volumeSmoothing READ volumeSmoothing NOTIFY configChanged)
    Q_PROPERTY(double brightnessFloor READ brightnessFloor NOTIFY configChanged)

    // Aubio config
    Q_PROPERTY(double filterbankNorm READ filterbankNorm NOTIFY configChanged)
    Q_PROPERTY(double filterbankPower READ filterbankPower NOTIFY configChanged)
    Q_PROPERTY(double onsetThreshold READ onsetThreshold NOTIFY configChanged)
    Q_PROPERTY(double onsetMinInterval READ onsetMinInterval NOTIFY configChanged)
    Q_PROPERTY(double onsetSilenceDb READ onsetSilenceDb NOTIFY configChanged)
    Q_PROPERTY(double onsetDelayMs READ onsetDelayMs NOTIFY configChanged)
    Q_PROPERTY(QString pitchMethod READ pitchMethod NOTIFY configChanged)
    Q_PROPERTY(double pitchSilenceDb READ pitchSilenceDb NOTIFY configChanged)
    Q_PROPERTY(double pitchTolerance READ pitchTolerance NOTIFY configChanged)
    Q_PROPERTY(double tempoSilenceDb READ tempoSilenceDb NOTIFY configChanged)
    Q_PROPERTY(double tempoThreshold READ tempoThreshold NOTIFY configChanged)
    Q_PROPERTY(int tatumSubdivision READ tatumSubdivision NOTIFY configChanged)
    Q_PROPERTY(double tssAlpha READ tssAlpha NOTIFY configChanged)
    Q_PROPERTY(double tssBeta READ tssBeta NOTIFY configChanged)
    Q_PROPERTY(double tssThreshold READ tssThreshold NOTIFY configChanged)

    // New aubio config (phase vocoder, mel filterbank, onset extras, tempo
    // delay, note detection, MFCC).
    Q_PROPERTY(QString windowType READ windowType NOTIFY configChanged)
    Q_PROPERTY(QString melScale READ melScale NOTIFY configChanged)
    Q_PROPERTY(bool onsetAdaptiveWhitening READ onsetAdaptiveWhitening NOTIFY configChanged)
    Q_PROPERTY(double onsetCompressionLambda READ onsetCompressionLambda NOTIFY configChanged)
    Q_PROPERTY(double tempoDelayMs READ tempoDelayMs NOTIFY configChanged)
    Q_PROPERTY(double noteSilenceDb READ noteSilenceDb NOTIFY configChanged)
    Q_PROPERTY(double noteMinIntervalMs READ noteMinIntervalMs NOTIFY configChanged)
    Q_PROPERTY(double noteReleaseDropDb READ noteReleaseDropDb NOTIFY configChanged)
    Q_PROPERTY(double mfccPower READ mfccPower NOTIFY configChanged)
    Q_PROPERTY(double mfccScale READ mfccScale NOTIFY configChanged)
    Q_PROPERTY(QVariantList onsetMethodsEnabled READ onsetMethodsEnabled NOTIFY configChanged)

    Q_PROPERTY(int windowSize READ windowSizeConst CONSTANT)
    Q_PROPERTY(int hopSize READ hopSizeConst CONSTANT)

    Q_PROPERTY(double volumeRaw READ volumeRaw NOTIFY audioSnapshotChanged)
    Q_PROPERTY(double volumeSmoothedValue READ volumeSmoothedValue NOTIFY audioSnapshotChanged)
    Q_PROPERTY(double volumeNormalized READ volumeNormalized NOTIFY audioSnapshotChanged)
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
    Q_PROPERTY(QVariantList mfccCoeffs READ mfccCoeffs NOTIFY audioSnapshotChanged)

    // Per-method onset descriptor diagnostics (raw aubio_onset_get_descriptor /
    // aubio_onset_get_thresholded_descriptor outputs). Same 9-entry order as
    // onsetFlags. Useful for tuning per-method thresholds in the UI.
    Q_PROPERTY(QVariantList onsetDescriptorValues READ onsetDescriptorValues NOTIFY audioSnapshotChanged)
    Q_PROPERTY(QVariantList onsetThresholdedValues READ onsetThresholdedValues NOTIFY audioSnapshotChanged)

    // Note detection snapshot (raw aubio_notes_do output via the snapshot).
    Q_PROPERTY(double noteMidi READ noteMidi NOTIFY audioSnapshotChanged)
    Q_PROPERTY(double noteVelocity READ noteVelocity NOTIFY audioSnapshotChanged)
    Q_PROPERTY(bool noteOn READ noteOn NOTIFY audioSnapshotChanged)
    Q_PROPERTY(bool noteOff READ noteOff NOTIFY audioSnapshotChanged)

    // Mel band crossover indices (for QML coloring)
    Q_PROPERTY(int melCrossSub READ melCrossSub NOTIFY configChanged)
    Q_PROPERTY(int melCrossBass READ melCrossBass NOTIFY configChanged)
    Q_PROPERTY(int melCrossLowMid READ melCrossLowMid NOTIFY configChanged)
    Q_PROPERTY(int melCrossMid READ melCrossMid NOTIFY configChanged)
    Q_PROPERTY(int melCrossHigh READ melCrossHigh NOTIFY configChanged)

    Q_PROPERTY(AudioProfileListModel* profileListModel READ profileListModel CONSTANT)

    Q_PROPERTY(double lowsPower READ lowsPower NOTIFY audioLevelsChanged)
    Q_PROPERTY(double midsPower READ midsPower NOTIFY audioLevelsChanged)
    Q_PROPERTY(double highsPower READ highsPower NOTIFY audioLevelsChanged)
    Q_PROPERTY(double subPower READ subPower NOTIFY audioLevelsChanged)
    Q_PROPERTY(double bassPower READ bassPower NOTIFY audioLevelsChanged)
    Q_PROPERTY(double lowMidPower READ lowMidPower NOTIFY audioLevelsChanged)
    Q_PROPERTY(double midPower READ midPower NOTIFY audioLevelsChanged)
    Q_PROPERTY(double highPower READ highPower NOTIFY audioLevelsChanged)
    Q_PROPERTY(bool beatActive READ beatActive NOTIFY beatActiveChanged)
    Q_PROPERTY(int lowCutBin READ lowCutBin NOTIFY barsNumberChanged)
    Q_PROPERTY(int highCutBin READ highCutBin NOTIFY barsNumberChanged)

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
    double subPower() const;
    double bassPower() const;
    double lowMidPower() const;
    double midPower() const;
    double highPower() const;
    bool beatActive() const;
    int lowCutBin() const;
    int highCutBin() const;

    double envelopeAttack() const;
    double envelopeRelease() const;
    double triggerHigh() const;
    double triggerLow() const;
    double triggerCooldown() const;
    double triggerHold() const;

    double bandSubMaxHz() const;
    double bandBassMaxHz() const;
    double bandLowMidMaxHz() const;
    double bandMidMaxHz() const;
    double bandHighMaxHz() const;
    double noiseGateThreshold() const;
    double noiseGateHold() const;
    double volumeSmoothing() const;
    double brightnessFloor() const;

    double filterbankNorm() const;
    double filterbankPower() const;
    double onsetThreshold() const;
    double onsetMinInterval() const;
    double onsetSilenceDb() const;
    double onsetDelayMs() const;
    QString pitchMethod() const;
    double pitchSilenceDb() const;
    double pitchTolerance() const;
    double tempoSilenceDb() const;
    double tempoThreshold() const;
    int tatumSubdivision() const;
    double tssAlpha() const;
    double tssBeta() const;
    double tssThreshold() const;

    QString windowType() const;
    QString melScale() const;
    bool onsetAdaptiveWhitening() const;
    double onsetCompressionLambda() const;
    double tempoDelayMs() const;
    double noteSilenceDb() const;
    double noteMinIntervalMs() const;
    double noteReleaseDropDb() const;
    double mfccPower() const;
    double mfccScale() const;
    QVariantList onsetMethodsEnabled() const;

    int windowSizeConst() const { return 1024; }
    int hopSizeConst() const { return 512; }

    double pitchHz() const;
    double pitchConfidence() const;
    double detectedBpm() const;
    double beatConfidence() const;
    double beatPhase() const;
    QVariantList tssTransientNorm() const;
    QVariantList tssSteadyNorm() const;
    int tssBinCount() const;
    QVariantList onsetFlags() const;
    QVariantList melSpectrum() const;
    QVariantList mfccCoeffs() const;
    QVariantList onsetDescriptorValues() const;
    QVariantList onsetThresholdedValues() const;
    double noteMidi() const;
    double noteVelocity() const;
    bool noteOn() const;
    bool noteOff() const;

    int melCrossSub() const;
    int melCrossBass() const;
    int melCrossLowMid() const;
    int melCrossMid() const;
    int melCrossHigh() const;

    double volumeRaw() const;
    double volumeSmoothedValue() const;
    double volumeNormalized() const;
    double rmsDb() const;
    double peakDb() const;
    double flux() const;
    bool noiseGateOpen() const;

    AudioProfileListModel* profileListModel();

    Q_INVOKABLE void setEnvelopeAttack(double ms);
    Q_INVOKABLE void setEnvelopeRelease(double ms);
    Q_INVOKABLE void setTriggerHighThreshold(double value);
    Q_INVOKABLE void setTriggerLowThreshold(double value);
    Q_INVOKABLE void setTriggerCooldown(double ms);
    Q_INVOKABLE void setTriggerHold(double ms);

    Q_INVOKABLE void setBandSubMaxHz(double hz);
    Q_INVOKABLE void setBandBassMaxHz(double hz);
    Q_INVOKABLE void setBandLowMidMaxHz(double hz);
    Q_INVOKABLE void setBandMidMaxHz(double hz);
    Q_INVOKABLE void setBandHighMaxHz(double hz);
    Q_INVOKABLE void setNoiseGateThreshold(double db);
    Q_INVOKABLE void setNoiseGateHold(double ms);
    Q_INVOKABLE void setVolumeSmoothing(double ms);
    Q_INVOKABLE void setBrightnessFloor(double value);

    Q_INVOKABLE void setFilterbankNorm(double norm);
    Q_INVOKABLE void setFilterbankPower(double power);
    Q_INVOKABLE void setOnsetThreshold(double value);
    Q_INVOKABLE void setOnsetMinInterval(double ms);
    Q_INVOKABLE void setOnsetSilenceDb(double db);
    Q_INVOKABLE void setOnsetDelayMs(double ms);
    Q_INVOKABLE void setPitchMethod(const QString &method);
    Q_INVOKABLE void setPitchSilenceDb(double db);
    Q_INVOKABLE void setPitchTolerance(double value);
    Q_INVOKABLE void setTempoSilenceDb(double db);
    Q_INVOKABLE void setTempoThreshold(double value);
    Q_INVOKABLE void setTatumSubdivision(int n);
    Q_INVOKABLE void setTssAlpha(double value);
    Q_INVOKABLE void setTssBeta(double value);
    Q_INVOKABLE void setTssThreshold(double value);

    Q_INVOKABLE void setWindowType(const QString &type);
    Q_INVOKABLE void setMelScale(const QString &scale);
    Q_INVOKABLE void setOnsetAdaptiveWhitening(bool enabled);
    Q_INVOKABLE void setOnsetCompressionLambda(double lambda);
    Q_INVOKABLE void setTempoDelayMs(double ms);
    Q_INVOKABLE void setNoteSilenceDb(double db);
    Q_INVOKABLE void setNoteMinIntervalMs(double ms);
    Q_INVOKABLE void setNoteReleaseDropDb(double db);
    Q_INVOKABLE void setMfccPower(double power);
    Q_INVOKABLE void setMfccScale(double scale);
    Q_INVOKABLE void setOnsetMethodEnabled(int idx, bool enabled);

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
    void updateAudioProfileSnapshotPowers();

private:
    VirtualConsole *m_vc;
    AudioCapture *m_inputCapture;
    bool m_captureEnabled;
    uchar m_volumeLevel;

    QVariantList m_audioLevels;
    quint32 m_audioProfileId = AudioProfile::invalidId();

    double m_lowsPower = 0.0;
    double m_midsPower = 0.0;
    double m_highsPower = 0.0;
    double m_subPower = 0.0;
    double m_bassPower = 0.0;
    double m_lowMidPower = 0.0;
    double m_midPower = 0.0;
    double m_highPower = 0.0;
    bool m_beatActive = false;
    QTimer *m_beatTimer = nullptr;

    AudioSnapshot m_cachedSnapshot;
    QVariantList m_triggerStatesCache;
    QVariantList m_melSpectrumCache;
    QVariantList m_mfccCoeffsCache;
    QVariantList m_onsetDescriptorCache;
    QVariantList m_onsetThresholdedCache;
    AudioProfileListModel *m_profileListModel = nullptr;

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

    struct AudioBar
    {
        BarType m_type = BarType::None;
        uchar m_minThreshold = 51; // 20%
        uchar m_maxThreshold = 204; // 80%
        uchar m_value = 0;
        int m_divisor = 1;

        /** List of individual DMX channels when m_type == DMXBar */
        QList<SceneValue> m_dmxChannels;

        /** List of absolute DMX channel addresses when m_type == DMXBar.
          * This is precalculated to speed up writeDMX */
        QList<int> m_absDmxChannels;

        /** ID of the attached Function when m_type == FunctionBar */
        quint32 m_functionId = Function::invalidId();

        /** Reference to an attached Function when m_type == FunctionBar */
        Function *m_function = nullptr;

        /** ID of the attchaed VCWidget when m_type == VCWidgetBar */
        quint32 m_widgetId = VCWidget::invalidId();

        /** Reference to an attached VCWidget when m_type == VCWidgetBar */
        VCWidget *m_widget = nullptr;

        /** Trigger state for beat-based widget actions */
        bool m_tapped = false;
        int m_skippedBeats = 0;
    };

    Q_INVOKABLE void selectBarForEditing(int index);
    QVariantList barsInfo() const;

    Q_INVOKABLE void setBarType(BarType type);
    Q_INVOKABLE void setBarThresholds(uchar minThr, uchar maxThr);
    Q_INVOKABLE void setBarFunction(quint32 functionId);
    Q_INVOKABLE void setBarWidget(quint32 widgetId);
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
    void updateBarWidgetReference(AudioBar &bar) const;
    void checkWidgetFunctionality(AudioBar &bar) const;
    void rebuildBarAbsDmxChannels(AudioBar &bar) const;

private:
    // first bar is always volume
    QVector <AudioBar> m_spectrumBars;

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
    void setSearchFilter(QString searchFilter);

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
