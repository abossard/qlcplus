/*
  Q Light Controller Plus
  vcaudiotriggers.cpp

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

#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QQmlEngine>
#include <QTimer>
#include <qmath.h>

#include "vcaudiotriggers.h"
#include "fixturemanager.h"
#include "virtualconsole.h"
#include "treemodelitem.h"
#include "fixtureutils.h"
#include "audiocapture.h"
#include "audiochannel.h"
#include "aubioresults.h"
#include "genericfader.h"
#include "fadechannel.h"
#include "vcspeeddial.h"
#include "qlcmacros.h"
#include "vccuelist.h"
#include "vcbutton.h"
#include "vcslider.h"
#include "app.h"
#include "doc.h"
#include "tardis.h"

#define INPUT_ENABLE_CAPTURE    0
#define INPUT_VOLUME_CONTROL    1

#define KXMLQLCAudioBarsNumber      QStringLiteral("BarsNumber")
#define KXMLQLCAudioTriggerBar      QStringLiteral("Bar")
#define KXMLQLCVolumeBar            QStringLiteral("VolumeBar")   // LEGACY
#define KXMLQLCSpectrumBar          QStringLiteral("SpectrumBar") // LEGACY

#define KXMLQLCAudioBarIndex        QStringLiteral("Index")
#define KXMLQLCAudioBarName         QStringLiteral("Name")
#define KXMLQLCAudioBarType         QStringLiteral("Type")
#define KXMLQLCAudioBarDMXChannels  QStringLiteral("DMXChannels")
#define KXMLQLCAudioBarFunction     QStringLiteral("FunctionID")
#define KXMLQLCAudioBarWidget       QStringLiteral("WidgetID")
#define KXMLQLCAudioBarMinThreshold QStringLiteral("MinThreshold")
#define KXMLQLCAudioBarMaxThreshold QStringLiteral("MaxThreshold")
#define KXMLQLCAudioBarDivisor      QStringLiteral("Divisor")
#define KXMLQLCAudioTriggerAudioProfileID QStringLiteral("AudioProfileID")

VCAudioTriggers::VCAudioTriggers(Doc *doc, VirtualConsole *vc, QObject *parent)
    : VCWidget(doc, parent)
    , m_vc(vc)
    , m_captureEnabled(false)
    , m_volumeLevel(100)
    , m_selectedBar(-1)
    , m_fixtureTree(nullptr)
    , m_searchFilter(QString())
    , m_applyToSameType(false)
    , m_isUpdating(false)
{
    setType(VCWidget::AudioTriggersWidget);

    registerExternalControl(INPUT_ENABLE_CAPTURE, tr("Enable Capture"), true);
    registerExternalControl(INPUT_VOLUME_CONTROL, tr("Volume Control"), false);

    QSharedPointer<AudioCapture> capture(m_doc->audioInputCapture());
    m_inputCapture = capture.data();

    // reserve for volume + spectrum bars
    m_spectrumBars.resize(m_inputCapture->defaultBarsNumber() + 1);
    m_audioLevels.resize(m_spectrumBars.count());
    setBarsNumber(m_spectrumBars.count());

    m_beatTimer = new QTimer(this);
    m_beatTimer->setSingleShot(true);
    m_beatTimer->setInterval(200); // beat flash duration
    connect(m_beatTimer, &QTimer::timeout, this, &VCAudioTriggers::slotBeatTimeout);
}

VCAudioTriggers::~VCAudioTriggers()
{
    if (m_item)
        delete m_item;
}

QString VCAudioTriggers::defaultCaption() const
{
    return tr("Audio Trigger %1").arg(id() + 1);
}

void VCAudioTriggers::setupLookAndFeel(qreal pixelDensity, int page)
{
    setPage(page);
    QFont wFont = font();
    wFont.setBold(true);
    wFont.setPointSize(pixelDensity * 5.0);
    setFont(wFont);
}

void VCAudioTriggers::render(QQuickView *view, QQuickItem *parent)
{
    initRenderItem(view, parent, "qrc:/VCAudioTriggersItem.qml", "audioTriggerObj");
}

QString VCAudioTriggers::propertiesResource() const
{
    return QString("qrc:/VCAudioTriggersProperties.qml");
}

VCWidget *VCAudioTriggers::createCopy(VCWidget *parent) const
{
    Q_ASSERT(parent != nullptr);

    VCAudioTriggers *audioTrigger = new VCAudioTriggers(m_doc, m_vc, parent);
    if (audioTrigger->copyFrom(this) == false)
    {
        delete audioTrigger;
        audioTrigger = nullptr;
    }

    return audioTrigger;
}

bool VCAudioTriggers::captureEnabled() const
{
    return m_captureEnabled;
}

void VCAudioTriggers::setCaptureEnabled(bool enable)
{
    if (enable == m_captureEnabled)
        return;

    Tardis::instance()->enqueueAction(Tardis::VCAudioTriggersSetCaptureEnabled, id(), m_captureEnabled, enable);

    m_captureEnabled = enable;

    // in case the audio input device has been changed in the meantime...
    QSharedPointer<AudioCapture> capture(m_doc->audioInputCapture());
    bool captureIsNew = m_inputCapture != capture.data();
    m_inputCapture = capture.data();
    updateAudioProfileSnapshotPowers();

    if (AudioProfile *p = resolvedAudioProfile())
        connect(p, &AudioProfile::configChanged, this, &VCAudioTriggers::configChanged, Qt::UniqueConnection);

    if (enable == true)
    {
        connect(m_inputCapture, &AudioCapture::aubioDataReady,
                this, &VCAudioTriggers::slotAubioDataReady);
        connect(m_inputCapture, SIGNAL(volumeChanged(int)),
                this, SIGNAL(volumeLevelChanged()));
        connect(m_inputCapture, SIGNAL(beatDetected()),
                this, SLOT(slotBeatDetected()));
        m_inputCapture->registerBandsNumber(m_spectrumBars.count() - 1);

        // Invalid ID: Stop every other widget
        emit functionStarting(this, Function::invalidId());

        for (AudioBar &bar : m_spectrumBars)
        {
            if (bar.m_type == VCAudioTriggers::BarType::DMXBar)
            {
                m_doc->masterTimer()->registerDMXSource(this);
                break;
            }
        }
    }
    else
    {
        if (!captureIsNew)
        {
            m_inputCapture->unregisterBandsNumber(m_spectrumBars.count() - 1);
            disconnect(m_inputCapture, &AudioCapture::aubioDataReady,
                       this, &VCAudioTriggers::slotAubioDataReady);
            disconnect(m_inputCapture, SIGNAL(volumeChanged(int)),
                       this, SIGNAL(volumeLevelChanged()));
            disconnect(m_inputCapture, SIGNAL(beatDetected()),
                       this, SLOT(slotBeatDetected()));
        }

        m_doc->masterTimer()->unregisterDMXSource(this);

        // request to delete all the active faders
        foreach (QSharedPointer<GenericFader> fader, m_fadersMap)
        {
            if (!fader.isNull())
                fader->requestDelete();
        }
        m_fadersMap.clear();
    }
    emit captureEnabledChanged();
}

uchar VCAudioTriggers::volumeLevel() const
{
    return m_volumeLevel;
}

void VCAudioTriggers::setVolumeLevel(uchar level)
{
    if (level == m_volumeLevel)
        return;

    m_volumeLevel = level;

    m_doc->audioInputCapture()->setVolume(intensity() * qreal(level) / 100.0);

    emit volumeLevelChanged();
}

int VCAudioTriggers::barsNumber() const
{
    // number of bars includes volume bar
    return m_spectrumBars.count();
}

void VCAudioTriggers::setBarsNumber(int num)
{
    if (num == m_spectrumBars.count())
        return;

    if (num > m_spectrumBars.count())
    {
        int barsToAdd = num - m_spectrumBars.count();
        for (int i = 0 ; i < barsToAdd; i++)
        {
            AudioBar bar;
            m_spectrumBars.append(bar);
        }
    }
    else if (num < m_spectrumBars.count())
    {
        int barsToRemove = m_spectrumBars.count() - num;
        for (int i = 0 ; i < barsToRemove; i++)
            m_spectrumBars.takeLast();
    }

    m_audioLevels.clear();
    m_audioLevels.resize(m_spectrumBars.count());

    if (m_selectedBar >= m_spectrumBars.count())
    {
        m_selectedBar = -1;
        emit selectedBarChanged();
    }

    emit barsNumberChanged();
    emit barsInfoChanged();
}

int VCAudioTriggers::selectedBar() const
{
    return m_selectedBar;
}

void VCAudioTriggers::setSelectedBar(int index)
{
    if (index < -1 || index >= m_spectrumBars.count())
        index = -1;

    if (index == m_selectedBar)
        return;

    m_selectedBar = index;
    updateFixtureTree();
    emit selectedBarChanged();
}

QVariantList VCAudioTriggers::audioLevels() const
{
    return m_audioLevels;
}

quint32 VCAudioTriggers::audioProfileId() const
{
    return m_audioProfileId;
}

void VCAudioTriggers::setAudioProfileId(quint32 id)
{
    if (id == m_audioProfileId)
        return;

    if (m_doc)
    {
        AudioProfile *oldProfile = resolvedAudioProfile();
        if (oldProfile)
            disconnect(oldProfile, &AudioProfile::configChanged, this, &VCAudioTriggers::configChanged);
    }

    m_audioProfileId = id;

    if (m_doc)
    {
        if (AudioProfile *p = resolvedAudioProfile())
            connect(p, &AudioProfile::configChanged, this, &VCAudioTriggers::configChanged, Qt::UniqueConnection);
    }

    updateAudioProfileSnapshotPowers();
    emit audioProfileIdChanged();
    emit configChanged();
    emit audioLevelsChanged();
}

double VCAudioTriggers::lowsPower() const { return m_lowsPower; }
double VCAudioTriggers::midsPower() const { return m_midsPower; }
double VCAudioTriggers::highsPower() const { return m_highsPower; }
double VCAudioTriggers::subPower() const { return m_subPower; }
double VCAudioTriggers::bassPower() const { return m_bassPower; }
double VCAudioTriggers::lowMidPower() const { return m_lowMidPower; }
double VCAudioTriggers::midPower() const { return m_midPower; }
double VCAudioTriggers::highPower() const { return m_highPower; }
bool VCAudioTriggers::beatActive() const { return m_beatActive; }

int VCAudioTriggers::lowCutBin() const
{
    return AudioCapture::lowCutBin(m_spectrumBars.count() - 1);
}

int VCAudioTriggers::highCutBin() const
{
    return AudioCapture::highCutBin(m_spectrumBars.count() - 1);
}

double VCAudioTriggers::envelopeAttack() const
{
    return profileChannelConfig().envelope.attackMs;
}

double VCAudioTriggers::envelopeRelease() const
{
    return profileChannelConfig().envelope.releaseMs;
}

double VCAudioTriggers::triggerHigh() const
{
    return profileChannelConfig().triggers.highThreshold;
}

double VCAudioTriggers::triggerLow() const
{
    return profileChannelConfig().triggers.lowThreshold;
}

double VCAudioTriggers::triggerCooldown() const
{
    return profileChannelConfig().triggers.cooldownMs;
}

double VCAudioTriggers::triggerHold() const
{
    return profileChannelConfig().triggers.holdMs;
}

void VCAudioTriggers::setEnvelopeAttack(double ms)
{
    AudioChannelConfig config = profileChannelConfig();
    config.envelope.attackMs = qMax(0.0, ms);
    applyChannelConfig(config);
}

void VCAudioTriggers::setEnvelopeRelease(double ms)
{
    AudioChannelConfig config = profileChannelConfig();
    config.envelope.releaseMs = qMax(0.0, ms);
    applyChannelConfig(config);
}

void VCAudioTriggers::setTriggerHighThreshold(double value)
{
    AudioChannelConfig config = profileChannelConfig();
    config.triggers.highThreshold = qBound(0.0, value, 1.0);
    applyChannelConfig(config);
}

void VCAudioTriggers::setTriggerLowThreshold(double value)
{
    AudioChannelConfig config = profileChannelConfig();
    config.triggers.lowThreshold = qBound(0.0, value, 1.0);
    applyChannelConfig(config);
}

void VCAudioTriggers::setTriggerCooldown(double ms)
{
    AudioChannelConfig config = profileChannelConfig();
    config.triggers.cooldownMs = qMax(0.0, ms);
    applyChannelConfig(config);
}

void VCAudioTriggers::setTriggerHold(double ms)
{
    AudioChannelConfig config = profileChannelConfig();
    config.triggers.holdMs = qMax(0.0, ms);
    applyChannelConfig(config);
}

double VCAudioTriggers::inputGain() const { return profileChannelConfig().aubio.inputGainLinear; }
double VCAudioTriggers::bandSubMaxHz() const { return profileChannelConfig().bandLayout.subMaxHz; }
double VCAudioTriggers::bandBassMaxHz() const { return profileChannelConfig().bandLayout.bassMaxHz; }
double VCAudioTriggers::bandLowMidMaxHz() const { return profileChannelConfig().bandLayout.lowMidMaxHz; }
double VCAudioTriggers::bandMidMaxHz() const { return profileChannelConfig().bandLayout.midMaxHz; }
double VCAudioTriggers::bandHighMaxHz() const { return profileChannelConfig().bandLayout.highMaxHz; }
double VCAudioTriggers::noiseGateThreshold() const { return profileChannelConfig().noiseGate.thresholdDb; }
double VCAudioTriggers::noiseGateHold() const { return profileChannelConfig().noiseGate.holdMs; }
double VCAudioTriggers::volumeSmoothing() const { return profileChannelConfig().volumeSmoothingMs; }
double VCAudioTriggers::brightnessFloor() const { return profileChannelConfig().brightnessFloor; }

double VCAudioTriggers::volumeRaw() const { return m_cachedSnapshot.volume.raw; }
double VCAudioTriggers::volumeSmoothedValue() const { return m_cachedSnapshot.volume.smoothed; }
double VCAudioTriggers::volumeNormalized() const { return m_cachedSnapshot.volume.normalized; }
double VCAudioTriggers::rmsDb() const { return m_cachedSnapshot.features.rmsDb; }
double VCAudioTriggers::peakDb() const { return m_cachedSnapshot.features.peakDb; }
double VCAudioTriggers::flux() const { return m_cachedSnapshot.features.flux; }
bool VCAudioTriggers::noiseGateOpen() const { return !m_cachedSnapshot.noiseGateClosed; }

void VCAudioTriggers::setInputGain(double gain)
{
    AudioChannelConfig config = profileChannelConfig();
    gain = qBound(0.1, gain, 8.0);
    if (qFuzzyCompare(config.aubio.inputGainLinear + 1.0, gain + 1.0))
        return;
    config.aubio.inputGainLinear = gain;
    applyChannelConfig(config);
}

void VCAudioTriggers::setBandSubMaxHz(double hz)
{
    AudioChannelConfig config = profileChannelConfig();
    hz = qBound(20.0, hz, 5000.0);
    if (qFuzzyCompare(config.bandLayout.subMaxHz + 1.0, hz + 1.0))
        return;
    config.bandLayout.subMaxHz = hz;
    applyChannelConfig(config);
}

void VCAudioTriggers::setBandBassMaxHz(double hz)
{
    AudioChannelConfig config = profileChannelConfig();
    hz = qBound(20.0, hz, 5000.0);
    if (qFuzzyCompare(config.bandLayout.bassMaxHz + 1.0, hz + 1.0))
        return;
    config.bandLayout.bassMaxHz = hz;
    applyChannelConfig(config);
}

void VCAudioTriggers::setBandLowMidMaxHz(double hz)
{
    AudioChannelConfig config = profileChannelConfig();
    hz = qBound(20.0, hz, 5000.0);
    if (qFuzzyCompare(config.bandLayout.lowMidMaxHz + 1.0, hz + 1.0))
        return;
    config.bandLayout.lowMidMaxHz = hz;
    applyChannelConfig(config);
}

void VCAudioTriggers::setBandMidMaxHz(double hz)
{
    AudioChannelConfig config = profileChannelConfig();
    hz = qBound(20.0, hz, 5000.0);
    if (qFuzzyCompare(config.bandLayout.midMaxHz + 1.0, hz + 1.0))
        return;
    config.bandLayout.midMaxHz = hz;
    applyChannelConfig(config);
}

void VCAudioTriggers::setBandHighMaxHz(double hz)
{
    AudioChannelConfig config = profileChannelConfig();
    hz = qBound(20.0, hz, 5000.0);
    if (qFuzzyCompare(config.bandLayout.highMaxHz + 1.0, hz + 1.0))
        return;
    config.bandLayout.highMaxHz = hz;
    applyChannelConfig(config);
}

void VCAudioTriggers::setNoiseGateThreshold(double db)
{
    AudioChannelConfig config = profileChannelConfig();
    db = qBound(-96.0, db, 0.0);
    if (qFuzzyCompare(config.noiseGate.thresholdDb + 1.0, db + 1.0))
        return;
    config.noiseGate.thresholdDb = db;
    applyChannelConfig(config);
}

void VCAudioTriggers::setNoiseGateHold(double ms)
{
    AudioChannelConfig config = profileChannelConfig();
    ms = qMax(0.0, ms);
    if (qFuzzyCompare(config.noiseGate.holdMs + 1.0, ms + 1.0))
        return;
    config.noiseGate.holdMs = ms;
    applyChannelConfig(config);
}

void VCAudioTriggers::setVolumeSmoothing(double ms)
{
    AudioChannelConfig config = profileChannelConfig();
    ms = qMax(0.0, ms);
    if (qFuzzyCompare(config.volumeSmoothingMs + 1.0, ms + 1.0))
        return;
    config.volumeSmoothingMs = ms;
    applyChannelConfig(config);
}

void VCAudioTriggers::setBrightnessFloor(double value)
{
    AudioChannelConfig config = profileChannelConfig();
    value = qBound(0.0, value, 1.0);
    if (qFuzzyCompare(config.brightnessFloor + 1.0, value + 1.0))
        return;
    config.brightnessFloor = value;
    applyChannelConfig(config);
}

double VCAudioTriggers::filterbankNorm() const { return profileChannelConfig().aubio.filterbankNorm; }
double VCAudioTriggers::filterbankPower() const { return profileChannelConfig().aubio.filterbankPower; }
double VCAudioTriggers::onsetThreshold() const { return profileChannelConfig().aubio.onsetThreshold; }
double VCAudioTriggers::onsetMinInterval() const { return profileChannelConfig().aubio.onsetMinIntervalMs; }
double VCAudioTriggers::onsetSilenceDb() const { return profileChannelConfig().aubio.onsetSilenceDb; }
double VCAudioTriggers::onsetDelayMs() const { return profileChannelConfig().aubio.onsetDelayMs; }
QString VCAudioTriggers::pitchMethod() const { return profileChannelConfig().aubio.pitchMethod; }
double VCAudioTriggers::pitchSilenceDb() const { return profileChannelConfig().aubio.pitchSilenceDb; }
double VCAudioTriggers::pitchTolerance() const { return profileChannelConfig().aubio.pitchTolerance; }
double VCAudioTriggers::tempoSilenceDb() const { return profileChannelConfig().aubio.tempoSilenceDb; }
double VCAudioTriggers::tempoThreshold() const { return profileChannelConfig().aubio.tempoThreshold; }
int VCAudioTriggers::tatumSubdivision() const { return profileChannelConfig().aubio.tatumSubdivision; }
double VCAudioTriggers::tssAlpha() const { return profileChannelConfig().aubio.tssAlpha; }
double VCAudioTriggers::tssBeta() const { return profileChannelConfig().aubio.tssBeta; }
double VCAudioTriggers::tssThreshold() const { return profileChannelConfig().aubio.tssThreshold; }

int VCAudioTriggers::onsetVoteCount() const { return m_cachedSnapshot.onsets.voteCount; }
double VCAudioTriggers::pitchHz() const { return m_cachedSnapshot.pitch.hz; }
double VCAudioTriggers::pitchConfidence() const { return m_cachedSnapshot.pitch.confidence; }
double VCAudioTriggers::detectedBpm() const { return m_cachedSnapshot.music.bpm; }
double VCAudioTriggers::beatConfidence() const { return m_cachedSnapshot.music.beatConfidence; }
double VCAudioTriggers::beatPhase() const { return m_cachedSnapshot.music.beatPhase; }
double VCAudioTriggers::tssTransient() const { return m_cachedSnapshot.tss.transientEnergy; }
double VCAudioTriggers::tssSteady() const { return m_cachedSnapshot.tss.steadyEnergy; }
double VCAudioTriggers::tssRatio() const { return m_cachedSnapshot.tss.ratio; }
QVariantList VCAudioTriggers::melSpectrum() const { return m_melSpectrumCache; }
QVariantList VCAudioTriggers::mfccCoeffs() const { return m_mfccCoeffsCache; }

namespace
{
    int melBandIndexForFreq(double hz)
    {
        constexpr double kSampleRateForMel = 44100.0;
        if (hz <= 0.0)
            return 0;
        const double melMax = 2595.0 * std::log10(1.0 + (kSampleRateForMel * 0.5) / 700.0);
        const double mel = 2595.0 * std::log10(1.0 + hz / 700.0);
        const double ratio = (melMax > 0.0) ? (mel / melMax) : 0.0;
        int idx = int(qRound(ratio * double(AUBIO_MEL_BANDS)));
        return qBound(0, idx, AUBIO_MEL_BANDS);
    }
}

int VCAudioTriggers::melCrossSub() const { return melBandIndexForFreq(profileChannelConfig().bandLayout.subMaxHz); }
int VCAudioTriggers::melCrossBass() const { return melBandIndexForFreq(profileChannelConfig().bandLayout.bassMaxHz); }
int VCAudioTriggers::melCrossLowMid() const { return melBandIndexForFreq(profileChannelConfig().bandLayout.lowMidMaxHz); }
int VCAudioTriggers::melCrossMid() const { return melBandIndexForFreq(profileChannelConfig().bandLayout.midMaxHz); }
int VCAudioTriggers::melCrossHigh() const { return melBandIndexForFreq(profileChannelConfig().bandLayout.highMaxHz); }

void VCAudioTriggers::setOnsetThreshold(double value)
{
    AudioChannelConfig config = profileChannelConfig();
    value = qBound(0.0, value, 1.0);
    if (qFuzzyCompare(config.aubio.onsetThreshold + 1.0, value + 1.0))
        return;
    config.aubio.onsetThreshold = value;
    applyChannelConfig(config);
}

void VCAudioTriggers::setOnsetMinInterval(double ms)
{
    AudioChannelConfig config = profileChannelConfig();
    ms = qBound(0.0, ms, 5000.0);
    if (qFuzzyCompare(config.aubio.onsetMinIntervalMs + 1.0, ms + 1.0))
        return;
    config.aubio.onsetMinIntervalMs = ms;
    applyChannelConfig(config);
}

void VCAudioTriggers::setPitchMethod(const QString &method)
{
    AudioChannelConfig config = profileChannelConfig();
    if (config.aubio.pitchMethod == method)
        return;
    config.aubio.pitchMethod = method;
    applyChannelConfig(config);
}

void VCAudioTriggers::setPitchSilenceDb(double db)
{
    AudioChannelConfig config = profileChannelConfig();
    db = qBound(-96.0, db, 0.0);
    if (qFuzzyCompare(config.aubio.pitchSilenceDb + 1.0, db + 1.0))
        return;
    config.aubio.pitchSilenceDb = db;
    applyChannelConfig(config);
}

void VCAudioTriggers::setPitchTolerance(double value)
{
    AudioChannelConfig config = profileChannelConfig();
    value = qBound(0.0, value, 1.0);
    if (qFuzzyCompare(config.aubio.pitchTolerance + 1.0, value + 1.0))
        return;
    config.aubio.pitchTolerance = value;
    applyChannelConfig(config);
}

void VCAudioTriggers::setTatumSubdivision(int n)
{
    AudioChannelConfig config = profileChannelConfig();
    n = qBound(1, n, 16);
    if (config.aubio.tatumSubdivision == n)
        return;
    config.aubio.tatumSubdivision = n;
    applyChannelConfig(config);
}

void VCAudioTriggers::setTssAlpha(double value)
{
    AudioChannelConfig config = profileChannelConfig();
    value = qBound(0.0, value, 10.0);
    if (qFuzzyCompare(config.aubio.tssAlpha + 1.0, value + 1.0))
        return;
    config.aubio.tssAlpha = value;
    applyChannelConfig(config);
}

void VCAudioTriggers::setTssBeta(double value)
{
    AudioChannelConfig config = profileChannelConfig();
    value = qBound(0.0, value, 10.0);
    if (qFuzzyCompare(config.aubio.tssBeta + 1.0, value + 1.0))
        return;
    config.aubio.tssBeta = value;
    applyChannelConfig(config);
}

void VCAudioTriggers::setTssThreshold(double value)
{
    AudioChannelConfig config = profileChannelConfig();
    value = qBound(0.0, value, 1.0);
    if (qFuzzyCompare(config.aubio.tssThreshold + 1.0, value + 1.0))
        return;
    config.aubio.tssThreshold = value;
    applyChannelConfig(config);
}

void VCAudioTriggers::setFilterbankNorm(double norm)
{
    AudioChannelConfig config = profileChannelConfig();
    // aubio_filterbank_set_norm only accepts 0 or 1
    norm = (norm >= 0.5) ? 1.0 : 0.0;
    if (qFuzzyCompare(config.aubio.filterbankNorm + 1.0, norm + 1.0))
        return;
    config.aubio.filterbankNorm = norm;
    applyChannelConfig(config);
}

void VCAudioTriggers::setFilterbankPower(double power)
{
    AudioChannelConfig config = profileChannelConfig();
    power = qBound(0.1, power, 4.0);
    if (qFuzzyCompare(config.aubio.filterbankPower + 1.0, power + 1.0))
        return;
    config.aubio.filterbankPower = power;
    applyChannelConfig(config);
}

void VCAudioTriggers::setOnsetSilenceDb(double db)
{
    AudioChannelConfig config = profileChannelConfig();
    db = qBound(-120.0, db, 0.0);
    if (qFuzzyCompare(config.aubio.onsetSilenceDb + 1.0, db + 1.0))
        return;
    config.aubio.onsetSilenceDb = db;
    applyChannelConfig(config);
}

void VCAudioTriggers::setOnsetDelayMs(double ms)
{
    AudioChannelConfig config = profileChannelConfig();
    ms = qBound(0.0, ms, 500.0);
    if (qFuzzyCompare(config.aubio.onsetDelayMs + 1.0, ms + 1.0))
        return;
    config.aubio.onsetDelayMs = ms;
    applyChannelConfig(config);
}

void VCAudioTriggers::setTempoSilenceDb(double db)
{
    AudioChannelConfig config = profileChannelConfig();
    db = qBound(-120.0, db, 0.0);
    if (qFuzzyCompare(config.aubio.tempoSilenceDb + 1.0, db + 1.0))
        return;
    config.aubio.tempoSilenceDb = db;
    applyChannelConfig(config);
}

void VCAudioTriggers::setTempoThreshold(double value)
{
    AudioChannelConfig config = profileChannelConfig();
    value = qBound(0.0, value, 1.0);
    if (qFuzzyCompare(config.aubio.tempoThreshold + 1.0, value + 1.0))
        return;
    config.aubio.tempoThreshold = value;
    applyChannelConfig(config);
}

QVariantMap VCAudioTriggers::triggerState(int band) const
{
    QVariantMap result;
    if (band < 0 || band >= 5)
    {
        result["active"] = false;
        result["fired"] = false;
        result["heldMs"] = 0.0;
        result["cooldownMs"] = 0.0;
        return result;
    }
    const TriggerState &ts = m_cachedSnapshot.triggers[band];
    result["active"] = ts.active;
    result["fired"] = ts.firedThisFrame;
    result["heldMs"] = ts.heldMs;
    result["cooldownMs"] = ts.cooldownRemainingMs;
    return result;
}

QVariantList VCAudioTriggers::triggerStates() const
{
    return m_triggerStatesCache;
}

void VCAudioTriggers::resetProfileToDefaults()
{
    applyChannelConfig(AudioChannelConfig::defaults());
}

void VCAudioTriggers::deleteCurrentProfile()
{
    if (m_audioProfileId == AudioProfile::invalidId())
        return;

    AudioProfile *profile = m_doc->audioProfile(m_audioProfileId);
    if (!profile || profile->isDefault())
        return;

    quint32 oldId = m_audioProfileId;
    setAudioProfileId(AudioProfile::invalidId());
    m_doc->removeAudioProfile(oldId);

    if (m_profileListModel)
        m_profileListModel->refresh();
}

void VCAudioTriggers::renameCurrentProfile(const QString &name)
{
    AudioProfile *profile = editableAudioProfile();
    if (!profile || name.isEmpty())
        return;

    profile->setName(name);
    m_doc->setModified();

    if (m_profileListModel)
        m_profileListModel->refresh();
}

void VCAudioTriggers::duplicateCurrentProfile(const QString &name)
{
    AudioProfile *source = resolvedAudioProfile();
    if (!source)
        return;

    quint32 newId = 0;
    for (AudioProfile *p : m_doc->audioProfiles())
        if (p->id() >= newId) newId = p->id() + 1;

    AudioProfile *dup = new AudioProfile(newId, m_doc);
    dup->setName(name.isEmpty() ? source->name() + QStringLiteral(" Copy") : name);
    dup->setChannelConfig(source->channelConfig());

    if (m_doc->addAudioProfile(dup))
    {
        setAudioProfileId(newId);
        if (m_profileListModel)
            m_profileListModel->refresh();
    }
    else
    {
        delete dup;
    }
}

AudioProfileListModel* VCAudioTriggers::profileListModel()
{
    if (!m_profileListModel)
        m_profileListModel = new AudioProfileListModel(m_doc, this);
    return m_profileListModel;
}

/*********************************************************************
 * AudioProfileListModel
 *********************************************************************/

AudioProfileListModel::AudioProfileListModel(Doc *doc, QObject *parent)
    : QAbstractListModel(parent), m_doc(doc)
{
    refresh();
}

void AudioProfileListModel::refresh()
{
    beginResetModel();
    m_entries.clear();
    if (m_doc)
    {
        for (AudioProfile *p : m_doc->audioProfiles())
            m_entries.append({p->id(), p->name(), p->isDefault()});
    }
    if (m_entries.isEmpty())
    {
        AudioProfile *def = m_doc ? m_doc->ensureDefaultAudioProfile() : nullptr;
        if (def)
            m_entries.append({def->id(), def->name(), true});
    }
    endResetModel();
}

int AudioProfileListModel::rowCount(const QModelIndex &) const { return m_entries.count(); }

QVariant AudioProfileListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_entries.count())
        return QVariant();
    const Entry &e = m_entries[index.row()];
    switch (role) {
    case IdRole: return e.id;
    case NameRole: return e.name;
    case IsDefaultRole: return e.isDefault;
    default: return QVariant();
    }
}

QHash<int, QByteArray> AudioProfileListModel::roleNames() const
{
    return {{IdRole, "profileId"}, {NameRole, "profileName"}, {IsDefaultRole, "profileIsDefault"}};
}

void VCAudioTriggers::slotBeatDetected()
{
    if (!m_beatActive)
    {
        m_beatActive = true;
        emit beatActiveChanged();
    }
    if (m_beatTimer)
        m_beatTimer->start(); // restart the timeout
}

void VCAudioTriggers::slotBeatTimeout()
{
    if (!m_beatActive)
        return;
    m_beatActive = false;
    emit beatActiveChanged();
}

bool VCAudioTriggers::copyFrom(const VCWidget *widget)
{
    const VCAudioTriggers *audioTrigger = qobject_cast<const VCAudioTriggers*> (widget);
    if (audioTrigger == nullptr)
        return false;

    /* Copy and set properties */
    setAudioProfileId(audioTrigger->audioProfileId());

    /* Copy object lists */

    /* Common stuff */
    return VCWidget::copyFrom(widget);
}

FunctionParent VCAudioTriggers::functionParent() const
{
    return FunctionParent(FunctionParent::AutoVCWidget, id());
}

AudioProfile *VCAudioTriggers::resolvedAudioProfile() const
{
    if (m_doc == nullptr)
        return nullptr;

    if (m_audioProfileId != AudioProfile::invalidId())
    {
        if (AudioProfile *profile = m_doc->audioProfile(m_audioProfileId))
            return profile;
    }

    return m_doc->defaultAudioProfile();
}

AudioProfile *VCAudioTriggers::editableAudioProfile() const
{
    if (AudioProfile *profile = resolvedAudioProfile())
        return profile;

    return m_doc ? m_doc->ensureDefaultAudioProfile() : nullptr;
}

AudioChannelConfig VCAudioTriggers::profileChannelConfig() const
{
    AudioProfile *profile = resolvedAudioProfile();
    return profile ? profile->channelConfig() : AudioChannelConfig::defaults();
}

void VCAudioTriggers::applyChannelConfig(const AudioChannelConfig &config)
{
    AudioProfile *profile = editableAudioProfile();
    if (profile == nullptr)
        return;

    profile->setChannelConfig(config);
    if (m_doc != nullptr)
        m_doc->setModified();
    if (m_inputCapture != nullptr)
        m_inputCapture->setAubioConfig(config.aubio);
    updateAudioProfileSnapshotPowers();
    emit configChanged();
    emit audioLevelsChanged();
}

void VCAudioTriggers::updateAudioProfileSnapshotPowers()
{
    AudioProfile *profile = resolvedAudioProfile();
    AudioChannel *channel = profile ? profile->channel() : nullptr;

    if (channel)
    {
        m_cachedSnapshot = channel->snapshot();
        m_subPower = m_cachedSnapshot.bands.sub;
        m_bassPower = m_cachedSnapshot.bands.bass;
        m_lowMidPower = m_cachedSnapshot.bands.lowMid;
        m_midPower = m_cachedSnapshot.bands.mid;
        m_highPower = m_cachedSnapshot.bands.high;
    }
    else
    {
        m_cachedSnapshot = AudioSnapshot{};
        m_subPower = m_bassPower = m_lowMidPower = m_midPower = m_highPower = 0.0;
    }

    // Rebuild trigger states cache for reactive QML bindings
    m_triggerStatesCache.clear();
    m_triggerStatesCache.reserve(5);
    for (int i = 0; i < 5; ++i)
    {
        const TriggerState &ts = m_cachedSnapshot.triggers[i];
        QVariantMap m;
        m[QStringLiteral("active")] = ts.active;
        m[QStringLiteral("fired")] = ts.firedThisFrame;
        m[QStringLiteral("heldMs")] = ts.heldMs;
        m[QStringLiteral("cooldownMs")] = ts.cooldownRemainingMs;
        m_triggerStatesCache.append(m);
    }

    m_melSpectrumCache.clear();
    m_melSpectrumCache.reserve(AUBIO_MEL_BANDS);
    for (int i = 0; i < AUBIO_MEL_BANDS; ++i)
        m_melSpectrumCache.append(m_cachedSnapshot.mel[i]);

    m_mfccCoeffsCache.clear();
    m_mfccCoeffsCache.reserve(AUBIO_MFCC_COEFFS);
    for (int i = 0; i < AUBIO_MFCC_COEFFS; ++i)
        m_mfccCoeffsCache.append(m_cachedSnapshot.mfcc[i]);

    emit audioSnapshotChanged();
}

void VCAudioTriggers::selectBarForEditing(int index)
{
    setSelectedBar(index);
}

QVariantList VCAudioTriggers::barsInfo() const
{
    QVariantList bList;
    const int spectrumBars = barsNumber() - 1; // exclude volume bar
    const double minFreq = AudioCapture::minFrequency();
    const double maxFreq = m_inputCapture ? m_inputCapture->maxFrequency() : AudioCapture::maxFrequency();
    const double logRange = (spectrumBars > 0 && maxFreq > minFreq) ? qLn(maxFreq / minFreq) : 0.0;

    int index = 0;
    for (const AudioBar &bar : m_spectrumBars)
    {
        QVariantMap barMap;

        if (index == 0)
        {
            barMap.insert("bLabel", "Volume Bar");
        }
        else
        {
            const int bandIndex = index - 1;
            double bandStartFreq = minFreq;
            double bandEndFreq = maxFreq;
            if (logRange > 0.0)
            {
                bandStartFreq = minFreq * qExp(logRange * (double(bandIndex) / double(spectrumBars)));
                bandEndFreq = minFreq * qExp(logRange * (double(bandIndex + 1) / double(spectrumBars)));
            }

            int bandStartHz = qCeil(bandStartFreq);
            int bandEndHz = (bandIndex == spectrumBars - 1) ? int(maxFreq) : (qCeil(bandEndFreq) - 1);
            if (bandEndHz <= bandStartHz)
                bandEndHz = bandStartHz;

            barMap.insert("bLabel", QString("#%1 (%2Hz - %3Hz)").arg(index)
                                       .arg(bandStartHz).arg(bandEndHz));
        }

        barMap.insert("index", index);
        barMap.insert("type", bar.m_type);

        if (bar.m_type == VCAudioTriggers::BarType::DMXBar)
        {
            barMap.insert("intVal", bar.m_dmxChannels.count());
        }
        else if (bar.m_type == VCAudioTriggers::BarType::FunctionBar)
        {
            barMap.insert("intVal", bar.m_functionId == Function::invalidId() ? -1 : int(bar.m_functionId));
        }
        else if (bar.m_type == VCAudioTriggers::BarType::VCWidgetBar)
        {
            barMap.insert("intVal", bar.m_widgetId == VCWidget::invalidId() ? -1 : int(bar.m_widgetId));
            VCWidget *widget = m_vc ? m_vc->widget(bar.m_widgetId) : nullptr;
            barMap.insert("strVal", widget ? widget->caption() : tr("No widget assigned"));
            barMap.insert("iconVal", widget ? VCWidget::typeToIcon(widget->type()) : QString());
        }
        else
        {
            barMap.insert("intVal", 0);
        }

        barMap.insert("minThreshold", qRound(SCALE(float(bar.m_minThreshold), 0.0, 255.0, 0.0, 100.0)));
        barMap.insert("maxThreshold", qRound(SCALE(float(bar.m_maxThreshold), 0.0, 255.0, 0.0, 100.0)));

        bList.append(barMap);
        index++;
    }

    return bList;
}

void VCAudioTriggers::setBarType(BarType type)
{
    if (m_selectedBar < 0 || m_selectedBar >= m_spectrumBars.count())
        return;

    // reset everything in any case
    AudioBar &bar = m_spectrumBars[m_selectedBar];
    bar.m_absDmxChannels.clear();
    bar.m_dmxChannels.clear();
    bar.m_minThreshold = 51;
    bar.m_maxThreshold = 204;
    bar.m_functionId = Function::invalidId();
    bar.m_function = nullptr;
    bar.m_widgetId = VCWidget::invalidId();
    bar.m_widget = nullptr;
    bar.m_tapped = false;
    bar.m_skippedBeats = 0;
    
    // set the type
    bar.m_type = type;

    emit barsInfoChanged();
}

void VCAudioTriggers::setBarThresholds(uchar minThr, uchar maxThr)
{
    if (m_selectedBar < 0 || m_selectedBar >= m_spectrumBars.count())
        return;

    AudioBar &bar = m_spectrumBars[m_selectedBar];
    bar.m_minThreshold = SCALE(float(minThr), 0.0, 100.0, 0.0, 255.0);
    bar.m_maxThreshold = SCALE(float(maxThr), 0.0, 100.0, 0.0, 255.0);
    emit barsInfoChanged();
}

void VCAudioTriggers::setBarFunction(quint32 functionId)
{
    if (m_selectedBar < 0 || m_selectedBar >= m_spectrumBars.count())
        return;

    AudioBar &bar = m_spectrumBars[m_selectedBar];
    bar.m_functionId = functionId;
    bar.m_function = (functionId != Function::invalidId() && m_doc)
                         ? m_doc->function(functionId)
                         : nullptr;
    emit barsInfoChanged();
}

void VCAudioTriggers::setBarWidget(quint32 widgetId)
{
    if (m_selectedBar < 0 || m_selectedBar >= m_spectrumBars.count())
        return;

    AudioBar &bar = m_spectrumBars[m_selectedBar];
    bar.m_widgetId = widgetId;
    bar.m_tapped = false;
    bar.m_skippedBeats = 0;
    updateBarWidgetReference(bar);
    emit barsInfoChanged();
}

void VCAudioTriggers::setBarDmxChannels(QList<SceneValue> list)
{
    if (m_selectedBar < 0 || m_selectedBar >= m_spectrumBars.count())
        return;

    AudioBar &bar = m_spectrumBars[m_selectedBar];
    bar.m_dmxChannels = list;
    rebuildBarAbsDmxChannels(bar);
    emit barsInfoChanged();
}

void VCAudioTriggers::rebuildBarAbsDmxChannels(AudioBar &bar) const
{
    bar.m_absDmxChannels.clear();

    for (const SceneValue &scv : bar.m_dmxChannels)
    {
        if (Fixture *fx = m_doc->fixture(scv.fxi))
        {
            const quint32 absAddr = fx->universeAddress() + scv.channel;
            bar.m_absDmxChannels.append(int(absAddr));
        }
    }
}

void VCAudioTriggers::updateBarWidgetReference(AudioBar &bar) const
{
    if (bar.m_widgetId == VCWidget::invalidId())
    {
        bar.m_widget = nullptr;
        return;
    }

    bar.m_widget = m_vc ? m_vc->widget(bar.m_widgetId) : nullptr;
}

void VCAudioTriggers::checkWidgetFunctionality(AudioBar &bar) const
{
    if (bar.m_widgetId == VCWidget::invalidId())
        return;

    updateBarWidgetReference(bar);
    VCWidget *widget = bar.m_widget;
    if (widget == nullptr)
        return;

    switch (widget->type())
    {
        case VCWidget::ButtonWidget:
        {
            VCButton *button = qobject_cast<VCButton *>(widget);
            if (button == nullptr)
                return;

            if (bar.m_value >= bar.m_maxThreshold && button->state() == VCButton::Inactive)
                button->requestStateChange(true);
            else if (bar.m_value < bar.m_minThreshold && button->state() != VCButton::Inactive)
                button->requestStateChange(false);
        }
        break;
        case VCWidget::SliderWidget:
        {
            VCSlider *slider = qobject_cast<VCSlider *>(widget);
            if (slider != nullptr)
                slider->setValue(bar.m_value, true, true);
        }
        break;
        case VCWidget::SpeedWidget:
        {
            VCSpeedDial *speedDial = qobject_cast<VCSpeedDial *>(widget);
            if (speedDial == nullptr)
                return;

            int divisor = qMax(1, bar.m_divisor);
            if (bar.m_value >= bar.m_maxThreshold && !bar.m_tapped)
            {
                if (bar.m_skippedBeats == 0)
                    speedDial->tap();

                bar.m_tapped = true;
                bar.m_skippedBeats = (bar.m_skippedBeats + 1) % divisor;
            }
            else if (bar.m_value < bar.m_minThreshold)
            {
                bar.m_tapped = false;
            }
        }
        break;
        case VCWidget::CueListWidget:
        {
            VCCueList *cueList = qobject_cast<VCCueList *>(widget);
            if (cueList == nullptr)
                return;

            int divisor = qMax(1, bar.m_divisor);
            if (bar.m_value >= bar.m_maxThreshold && !bar.m_tapped)
            {
                if (bar.m_skippedBeats == 0)
                    cueList->nextClicked();

                bar.m_tapped = true;
                bar.m_skippedBeats = (bar.m_skippedBeats + 1) % divisor;
            }
            else if (bar.m_value < bar.m_minThreshold)
            {
                bar.m_tapped = false;
            }
        }
        break;
        default:
        break;
    }
}

void VCAudioTriggers::slotAubioDataReady(const AubioResults &results, quint32 power)
{
    Q_UNUSED(results)

    // m_spectrumBars[0] is the volume bar; m_spectrumBars[1..N] are perceptual bands.
    // Their values drive DMX/Function/VCWidget triggers below.
    const int bandCount = m_spectrumBars.count() - 1;
    if (bandCount < 0)
        return;

    m_audioLevels.clear();
    m_audioLevels.reserve(bandCount + 1);

    // Volume bar: same conversion legacy widgets used (power -> 0..255)
    static constexpr double kPowerMax = 32767.0; // 0x7FFF
    const int vol255 = qBound(0, int((double(power) * 255.0 / kPowerMax) + 0.5), 255);
    m_spectrumBars[0].m_value = uchar(vol255);
    m_audioLevels.append(vol255);

    // Pull the latest snapshot (built on the capture thread by the analyzer
    // immediately before this signal was emitted).
    updateAudioProfileSnapshotPowers();

    // Resample the 40-band mel spectrum into the configured number of perceptual
    // bands using log-frequency aware splits, with a small temporal smoothing.
    static constexpr double kAlpha = 0.25; // 0..1, higher = snappier
    if (bandCount > 0)
    {
        const double minFreq = AudioCapture::minFrequency();
        const double maxFreq = m_inputCapture
            ? m_inputCapture->maxFrequency() : AudioCapture::maxFrequency();
        const double logRange = (maxFreq > minFreq) ? qLn(maxFreq / minFreq) : 0.0;

        // mel[] is already 0..1-ish linear power. Mel filters are log-spaced
        // already (Slaney filterbank), so a simple linear partitioning across
        // the 40 mel bins approximates a log-frequency split.
        for (int i = 0; i < bandCount; ++i)
        {
            int start, end;
            if (logRange > 0.0)
            {
                const double f0 = minFreq * qExp(logRange * (double(i) / double(bandCount)));
                const double f1 = minFreq * qExp(logRange * (double(i + 1) / double(bandCount)));
                // Map fraction along log-range -> mel bin index
                start = int(double(AUBIO_MEL_BANDS) * qLn(f0 / minFreq) / logRange);
                end   = int(double(AUBIO_MEL_BANDS) * qLn(f1 / minFreq) / logRange);
            }
            else
            {
                start = (i * AUBIO_MEL_BANDS) / bandCount;
                end   = ((i + 1) * AUBIO_MEL_BANDS) / bandCount;
            }
            start = qBound(0, start, AUBIO_MEL_BANDS - 1);
            end   = qBound(start + 1, end, AUBIO_MEL_BANDS);

            double sum = 0.0;
            for (int m = start; m < end; ++m)
                sum += m_cachedSnapshot.mel[m];
            double v = qBound(0.0, sum / double(end - start), 1.0);

            const double old01 = m_spectrumBars[i + 1].m_value / 255.0;
            v = kAlpha * v + (1.0 - kAlpha) * old01;
            const int bandValue = qBound(0, int(v * 255.0 + 0.5), 255);
            m_spectrumBars[i + 1].m_value = uchar(bandValue);
            m_audioLevels.append(bandValue);
        }
    }

    // Lows / Mids / Highs aggregates — derived from the snapshot's perceptual
    // bands (consistent with the rest of the pipeline) instead of from the
    // legacy log-bar slicing.
    m_lowsPower  = qBound(0.0, 0.5 * (m_cachedSnapshot.bands.sub + m_cachedSnapshot.bands.bass), 1.0);
    m_midsPower  = qBound(0.0, 0.5 * (m_cachedSnapshot.bands.lowMid + m_cachedSnapshot.bands.mid), 1.0);
    m_highsPower = qBound(0.0, m_cachedSnapshot.bands.high, 1.0);

    // Drive DMX / Function / VCWidget triggers based on the freshly updated bars.
    for (int i = 0; i < m_spectrumBars.count(); i++)
    {
        AudioBar &bar = m_spectrumBars[i];
        switch (bar.m_type)
        {
            case FunctionBar:
            {
                if (bar.m_function == nullptr && bar.m_functionId != Function::invalidId())
                    bar.m_function = m_doc->function(bar.m_functionId);

                if (bar.m_function != nullptr)
                {
                    if (bar.m_value >= bar.m_maxThreshold)
                        bar.m_function->start(m_doc->masterTimer(), functionParent());
                    else if (bar.m_value < bar.m_minThreshold)
                        bar.m_function->stop(functionParent());
                }
            }
            break;
            case VCWidgetBar:
                checkWidgetFunctionality(bar);
            break;
            case DMXBar:
            case None:
            default:
            break;
        }
    }

    emit audioLevelsChanged();
}

/*********************************************************************
 * Fixture tree methods
 *********************************************************************/

void VCAudioTriggers::updateFixtureTree()
{
    if (m_fixtureTree == nullptr || m_selectedBar < 0 || m_selectedBar >= m_spectrumBars.count())
        return;

    m_fixtureTree->clear();
    FixtureManager::updateGroupsTree(m_doc, m_fixtureTree, m_searchFilter,
                                     FixtureManager::ShowCheckBoxes | FixtureManager::ShowGroups | FixtureManager::ShowChannels,
                                     m_spectrumBars[m_selectedBar].m_dmxChannels);
}

QVariant VCAudioTriggers::groupsTreeModel()
{
    if (m_selectedBar < 0 || m_selectedBar >= m_spectrumBars.count())
        return QVariant();

    if (m_fixtureTree == nullptr)
    {
        m_fixtureTree = new TreeModel(this);
        QQmlEngine::setObjectOwnership(m_fixtureTree, QQmlEngine::CppOwnership);
        QStringList treeColumns;
        treeColumns << "classRef" << "type" << "id" << "subid" << "chIdx" << "inGroup";
        m_fixtureTree->setColumnNames(treeColumns);
        m_fixtureTree->enableSorting(false);
        updateFixtureTree();

        connect(m_fixtureTree, SIGNAL(roleChanged(TreeModelItem*,int,const QVariant&)),
                this, SLOT(slotTreeDataChanged(TreeModelItem*,int,const QVariant&)));
    }

    return QVariant::fromValue(m_fixtureTree);
}

QString VCAudioTriggers::searchFilter() const
{
    return m_searchFilter;
}

void VCAudioTriggers::setSearchFilter(QString searchFilter)
{
    if (m_searchFilter == searchFilter)
        return;

    int currLen = m_searchFilter.length();

    m_searchFilter = searchFilter;

    if ((searchFilter.length() >= SEARCH_MIN_CHARS ||
        (currLen >= SEARCH_MIN_CHARS && searchFilter.length() < SEARCH_MIN_CHARS))
        && m_selectedBar >= 0 && m_selectedBar < m_spectrumBars.count())
    {
        FixtureManager::updateGroupsTree(m_doc, m_fixtureTree, m_searchFilter,
                                         FixtureManager::ShowCheckBoxes | FixtureManager::ShowGroups | FixtureManager::ShowChannels,
                                         m_spectrumBars[m_selectedBar].m_dmxChannels);
        emit groupsTreeModelChanged();
    }

    emit searchFilterChanged();
}

void VCAudioTriggers::applyToSameType(bool enable)
{
    m_applyToSameType = enable;
}

void VCAudioTriggers::checkFixtureTree(TreeModel *tree, Fixture *sourceFixture,
                                      quint32 channelIndex, bool checked)
{
    if (tree == nullptr || m_selectedBar < 0 || m_selectedBar >= m_spectrumBars.count())
        return;

    AudioBar &bar = m_spectrumBars[m_selectedBar];

    for (TreeModelItem *item : tree->items())
    {
        QVariantList itemData = item->data();

        // itemData must be "classRef" << "type" << "id" << "subid" << "chIdx" << "inGroup";
        if (itemData.count() == 6 && itemData.at(1).toInt() == App::ChannelDragItem)
        {
            quint32 itemID = itemData.at(2).toUInt();
            quint32 chIndex = itemData.at(4).toUInt();
            quint32 fixtureID = FixtureUtils::itemFixtureID(itemID);
            quint16 linkedIndex = FixtureUtils::itemLinkedIndex(itemID);
            Fixture *destFixture = m_doc->fixture(fixtureID);

            if (destFixture == nullptr)
                continue;

            if (sourceFixture->fixtureDef() == destFixture->fixtureDef() &&
                sourceFixture->fixtureMode() == destFixture->fixtureMode() &&
                chIndex == channelIndex && linkedIndex == 0)
            {
                tree->setItemRoleData(item, checked, TreeModel::IsCheckedRole);

                SceneValue scv(fixtureID, chIndex);

                if (checked)
                {
                    if (bar.m_dmxChannels.contains(scv) == false)
                        bar.m_dmxChannels.append(scv);
                }
                else
                {
                    bar.m_dmxChannels.removeAll(scv);
                }
            }
        }

        if (item->hasChildren())
            checkFixtureTree(item->children(), sourceFixture, channelIndex, checked);
    }
}

void VCAudioTriggers::slotTreeDataChanged(TreeModelItem *item, int role, const QVariant &value)
{
    if (m_isUpdating || m_selectedBar < 0 || m_selectedBar >= m_spectrumBars.count())
        return;

    qDebug() << "VCAudioTriggers tree data changed" << value.toInt();
    qDebug() << "Item data:" << item->data();

    if (role != TreeModel::IsCheckedRole)
        return;

    QVariantList itemData = item->data();
    // itemData must be "classRef" << "type" << "id" << "subid" << "chIdx" << "inGroup";
    if (itemData.count() != 6)
        return;

    //QString type = itemData.at(1).toString();
    quint32 itemID = itemData.at(2).toUInt();
    quint32 chIndex = itemData.at(4).toUInt();
    quint32 fixtureID = FixtureUtils::itemFixtureID(itemID);

    Fixture *fixture = m_doc->fixture(fixtureID);
    if (fixture == nullptr)
        return;

    bool checked = value.toInt() == 0 ? false : true;
    AudioBar &bar = m_spectrumBars[m_selectedBar];

    if (m_applyToSameType)
    {
        m_isUpdating = true;
        checkFixtureTree(m_fixtureTree, fixture, chIndex, checked);
        m_isUpdating = false;
    }
    else
    {
        SceneValue scv(fixtureID, chIndex);

        if (checked)
        {
            if (bar.m_dmxChannels.contains(scv) == false)
                bar.m_dmxChannels.append(scv);
        }
        else
        {
            bar.m_dmxChannels.removeAll(scv);
        }
    }

    rebuildBarAbsDmxChannels(bar);
    emit barsInfoChanged();
}

/*********************************************************************
 * External input
 *********************************************************************/

void VCAudioTriggers::updateFeedback()
{
    sendFeedback(m_captureEnabled ? UCHAR_MAX : 0, INPUT_ENABLE_CAPTURE,
                 m_captureEnabled ? VCWidget::UpperValue : VCWidget::LowerValue);
}

void VCAudioTriggers::slotInputValueChanged(quint8 id, uchar value)
{
    switch (id)
    {
        case INPUT_ENABLE_CAPTURE:
            setCaptureEnabled(value ? true : false);
        break;
        case INPUT_VOLUME_CONTROL:
            setVolumeLevel(SCALE(value, 0.0, 255.0, 0.0, 100.0));
        break;
    }
}

/*********************************************************************
 * DMXSource
 *********************************************************************/

void VCAudioTriggers::writeDMX(MasterTimer *timer, QList<Universe *> universes)
{
    Q_UNUSED(timer);

    quint32 lastUniverse = Universe::invalid();
    QSharedPointer<GenericFader> fader;

    for (AudioBar &bar : m_spectrumBars)
    {
        if (bar.m_type == VCAudioTriggers::BarType::DMXBar)
        {
            for (int i = 0; i < bar.m_absDmxChannels.count(); i++)
            {
                int absAddress = bar.m_absDmxChannels.at(i);
                //quint32 address = absAddress & 0x01FF;
                quint32 universe = absAddress >> 9;
                if (universe != lastUniverse)
                {
                    fader = m_fadersMap.value(universe, QSharedPointer<GenericFader>());
                    if (fader == NULL)
                    {
                        fader = universes[universe]->requestFader();
                        fader->adjustIntensity(intensity());
                        m_fadersMap[universe] = fader;
                    }
                    fader->setEnabled(m_captureEnabled);
                    lastUniverse = universe;
                }

                FadeChannel *fc = fader->getChannelFader(m_doc, universes[universe], Fixture::invalidId(), absAddress);
                fc->setStart(fc->current());
                fc->setTarget(bar.m_value);
                fc->setReady(false);
                fc->setElapsed(0);
            }
        }
    }
}

/*********************************************************************
 * Load & Save
 *********************************************************************/

bool VCAudioTriggers::loadBarXML(QXmlStreamReader &root)
{
    QXmlStreamAttributes attrs = root.attributes();

    if (attrs.hasAttribute(KXMLQLCAudioBarType) == false)
        return false;

    int barIndex = attrs.value(KXMLQLCAudioBarIndex).toString().toInt();
    // Transpose legacy volume bar index
    if (barIndex == 1000)
        barIndex = 0;

    if (barIndex < 0 || barIndex >= m_spectrumBars.count())
    {
        qDebug() << "Audio Triggers bar index out of bounds!" << barIndex;
        return false;
    }

    AudioBar &bar = m_spectrumBars[barIndex];

    bar.m_type = BarType(attrs.value(KXMLQLCAudioBarType).toString().toInt());
    bar.m_minThreshold = attrs.value(KXMLQLCAudioBarMinThreshold).toString().toInt();
    bar.m_maxThreshold = attrs.value(KXMLQLCAudioBarMaxThreshold).toString().toInt();
    bar.m_divisor = qMax(1, attrs.value(KXMLQLCAudioBarDivisor).toString().toInt());

    switch (bar.m_type)
    {
        case VCAudioTriggers::BarType::FunctionBar:
        {
            if (attrs.hasAttribute(KXMLQLCAudioBarFunction))
            {
                bar.m_functionId = attrs.value(KXMLQLCAudioBarFunction).toUInt();
                Function *func = m_doc->function(bar.m_functionId);
                if (func != NULL)
                    bar.m_function = func;
            }
        }
        break;
        case VCAudioTriggers::BarType::VCWidgetBar:
        {
            if (attrs.hasAttribute(KXMLQLCAudioBarWidget))
            {
                quint32 wid = attrs.value(KXMLQLCAudioBarWidget).toString().toUInt();
                bar.m_widgetId = wid;
                bar.m_widget = nullptr;
                bar.m_tapped = false;
                bar.m_skippedBeats = 0;
            }
        }
        break;
        case VCAudioTriggers::BarType::DMXBar:
        {
            QXmlStreamReader::TokenType tType = root.readNext();

            if (tType == QXmlStreamReader::EndElement)
            {
                root.readNext();
                return true;
            }

            if (tType == QXmlStreamReader::Characters)
                root.readNext();

            if (root.name() == KXMLQLCAudioBarDMXChannels)
            {
                QString dmxValues = root.readElementText();
                if (dmxValues.isEmpty() == false)
                {
                    QList<SceneValue> channels;
                    QStringList varray = dmxValues.split(",");
                    for (int i = 0; i < varray.count(); i+=2)
                    {
                        channels.append(SceneValue(QString(varray.at(i)).toUInt(),
                                                   QString(varray.at(i + 1)).toUInt(), 0));
                    }
                    selectBarForEditing(barIndex);
                    setBarDmxChannels(channels);
                    selectBarForEditing(-1);
                }
            }
        }
        break;
        default:
        break;
    }

    return true;
}

bool VCAudioTriggers::saveBarXML(QXmlStreamWriter *doc, int index) const
{
    Q_ASSERT(doc != NULL);

    if (index < 0 || index >= m_spectrumBars.count())
    {
        qDebug() << "Audio Triggers bar index out of bounds!" << index;
        return false;
    }

    AudioBar bar = m_spectrumBars[index];

    doc->writeStartElement(KXMLQLCAudioTriggerBar);
    doc->writeAttribute(KXMLQLCAudioBarType, QString::number(bar.m_type));
    doc->writeAttribute(KXMLQLCAudioBarMinThreshold, QString::number(bar.m_minThreshold));
    doc->writeAttribute(KXMLQLCAudioBarMaxThreshold, QString::number(bar.m_maxThreshold));
    doc->writeAttribute(KXMLQLCAudioBarDivisor, QString::number(bar.m_divisor));
    doc->writeAttribute(KXMLQLCAudioBarIndex, QString::number(index));

    if (bar.m_type == VCAudioTriggers::BarType::DMXBar && bar.m_dmxChannels.count() > 0)
    {
        QString chans;
        foreach (SceneValue scv, bar.m_dmxChannels)
        {
            if (chans.isEmpty() == false)
                chans.append(",");
            chans.append(QString("%1,%2").arg(scv.fxi).arg(scv.channel));
        }
        if (chans.isEmpty() == false)
        {
            doc->writeTextElement(KXMLQLCAudioBarDMXChannels, chans);
        }
    }
    else if (bar.m_type == VCAudioTriggers::BarType::FunctionBar && bar.m_functionId != Function::invalidId())
    {
        doc->writeAttribute(KXMLQLCAudioBarFunction, QString::number(bar.m_functionId));
    }
    else if (bar.m_type == VCAudioTriggers::BarType::VCWidgetBar && bar.m_widgetId != VCWidget::invalidId())
    {
        doc->writeAttribute(KXMLQLCAudioBarWidget, QString::number(bar.m_widgetId));
    }

    /* End <Bar> tag */
    doc->writeEndElement();

    return true;
}

bool VCAudioTriggers::loadXML(QXmlStreamReader &root)
{
    if (root.name() != KXMLQLCVCAudioTriggers)
    {
        qWarning() << Q_FUNC_INFO << "Audio trigger node not found";
        return false;
    }

    QXmlStreamAttributes attrs = root.attributes();
    if (attrs.hasAttribute(KXMLQLCAudioTriggerAudioProfileID))
        setAudioProfileId(attrs.value(KXMLQLCAudioTriggerAudioProfileID).toString().toUInt());

    if (root.attributes().hasAttribute(KXMLQLCAudioBarsNumber))
    {
        int barsNum = root.attributes().value(KXMLQLCAudioBarsNumber).toInt();
        setBarsNumber(barsNum + 1);
    }

    /* Widget commons */
    loadXMLCommon(root);

    while (root.readNextStartElement())
    {
        if (root.name() == KXMLQLCWindowState)
        {
            bool visible = false;
            int x = 0, y = 0, w = 0, h = 0;
            loadXMLWindowState(root, &x, &y, &w, &h, &visible);
            setGeometry(QRect(x, y, w, h));
        }
        else if (root.name() == KXMLQLCVCWidgetAppearance)
        {
            loadXMLAppearance(root);
        }
        else if (root.name() == KXMLQLCVCWidgetInput)
        {
            loadXMLInputSource(root);
        }
        else if (root.name() == KXMLQLCVCWidgetKey)
        {
            loadXMLInputKey(root);
        }
        else if (root.name() == KXMLQLCAudioTriggerBar ||
                 root.name() == KXMLQLCVolumeBar ||
                 root.name() == KXMLQLCSpectrumBar)
        {
            loadBarXML(root);
            root.skipCurrentElement();
        }
        else
        {
            qWarning() << Q_FUNC_INFO << "Unknown audio trigger tag:" << root.name().toString();
            root.skipCurrentElement();
        }
    }

    return true;
}

bool VCAudioTriggers::saveXML(QXmlStreamWriter *doc) const
{
    Q_ASSERT(doc != nullptr);

    /* VC object entry */
    doc->writeStartElement(KXMLQLCVCAudioTriggers);
    doc->writeAttribute(KXMLQLCAudioBarsNumber, QString::number(barsNumber() - 1));
    doc->writeAttribute(KXMLQLCAudioTriggerAudioProfileID, QString::number(m_audioProfileId));

    saveXMLCommon(doc);

    /* Window state */
    saveXMLWindowState(doc);

    /* Appearance */
    saveXMLAppearance(doc);

    /* External control */
    saveXMLInputControl(doc, INPUT_ENABLE_CAPTURE);
    saveXMLInputControl(doc, INPUT_VOLUME_CONTROL);

    /* Save only configured triggers */
    int barIndex = 0;
    for (const AudioBar &bar : m_spectrumBars)
    {
        if (bar.m_type != VCAudioTriggers::BarType::None)
            saveBarXML(doc, barIndex);
        barIndex++;
    }

    /* Write the <end> tag */
    doc->writeEndElement();

    return true;
}
