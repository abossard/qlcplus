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
#include <QDebug>
#include <QSet>
#include <qmath.h>
#include <algorithm>
#include <cmath>

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

#define KXMLQLCAudioBarsNumber      QStringLiteral("BarsNumber")  // LEGACY (ignored on load)
#define KXMLQLCAudioTriggerBar      QStringLiteral("Bar")         // LEGACY
#define KXMLQLCVolumeBar            QStringLiteral("VolumeBar")   // LEGACY
#define KXMLQLCSpectrumBar          QStringLiteral("SpectrumBar") // LEGACY

#define KXMLQLCAudioMapping         QStringLiteral("Mapping")
#define KXMLQLCAudioMappingSource   QStringLiteral("Source")
#define KXMLQLCAudioBarIndex        QStringLiteral("Index")
#define KXMLQLCAudioBarName         QStringLiteral("Name")
#define KXMLQLCAudioBarType         QStringLiteral("Type")
#define KXMLQLCAudioBarDMXChannels  QStringLiteral("DMXChannels")
#define KXMLQLCAudioBarFunction     QStringLiteral("FunctionID")
#define KXMLQLCAudioBarWidget       QStringLiteral("WidgetID")
#define KXMLQLCAudioBarMinThreshold QStringLiteral("MinThreshold") // LEGACY
#define KXMLQLCAudioBarMaxThreshold QStringLiteral("MaxThreshold") // LEGACY
#define KXMLQLCAudioBarDivisor      QStringLiteral("Divisor")
#define KXMLQLCAudioMappingDmxScale QStringLiteral("DmxScale")
#define KXMLQLCAudioMappingDmxFloor QStringLiteral("DmxFloor")
#define KXMLQLCAudioMappingBeatHold QStringLiteral("BeatHoldMs")
#define KXMLQLCAudioTriggerAudioProfileID QStringLiteral("AudioProfileID")

static QString bandSourceToKey(VCAudioTriggers::BandSource s)
{
    switch (s)
    {
        case VCAudioTriggers::BandLow:    return QStringLiteral("Low");
        case VCAudioTriggers::BandMid:    return QStringLiteral("Mid");
        case VCAudioTriggers::BandHigh:   return QStringLiteral("High");
        case VCAudioTriggers::BandVolume: return QStringLiteral("Volume");
        case VCAudioTriggers::BandBeat:   return QStringLiteral("Beat");
        case VCAudioTriggers::BandKick:   return QStringLiteral("Kick");
        default: return QString();
    }
}

static int bandSourceFromKey(const QString &key, bool *ok)
{
    if (ok) *ok = true;
    if (key == QLatin1String("Low"))    return VCAudioTriggers::BandLow;
    if (key == QLatin1String("Mid"))    return VCAudioTriggers::BandMid;
    if (key == QLatin1String("High"))   return VCAudioTriggers::BandHigh;
    if (key == QLatin1String("Volume")) return VCAudioTriggers::BandVolume;
    if (key == QLatin1String("Beat"))   return VCAudioTriggers::BandBeat;
    if (key == QLatin1String("Kick"))   return VCAudioTriggers::BandKick;
    // Legacy 5-band keys map to closest equivalent (Sub/Bass -> Low,
    // LowMid -> Mid). Old projects load without errors.
    if (key == QLatin1String("Sub"))    return VCAudioTriggers::BandLow;
    if (key == QLatin1String("Bass"))   return VCAudioTriggers::BandLow;
    if (key == QLatin1String("LowMid")) return VCAudioTriggers::BandMid;
    if (ok) *ok = false;
    return -1;
}

static QString bandSourceLabel(VCAudioTriggers::BandSource s)
{
    switch (s)
    {
        case VCAudioTriggers::BandLow:    return QStringLiteral("Low");
        case VCAudioTriggers::BandMid:    return QStringLiteral("Mid");
        case VCAudioTriggers::BandHigh:   return QStringLiteral("High");
        case VCAudioTriggers::BandVolume: return QStringLiteral("Volume");
        case VCAudioTriggers::BandBeat:   return QStringLiteral("Beat");
        case VCAudioTriggers::BandKick:   return QStringLiteral("Kick");
        default: return QString();
    }
}

static QString bandSourceColor(VCAudioTriggers::BandSource s)
{
    switch (s)
    {
        case VCAudioTriggers::BandLow:    return QStringLiteral("#ff3333");
        case VCAudioTriggers::BandMid:    return QStringLiteral("#33cc66");
        case VCAudioTriggers::BandHigh:   return QStringLiteral("#33ccff");
        case VCAudioTriggers::BandVolume: return QStringLiteral("#aaaaaa");
        case VCAudioTriggers::BandKick:   return QStringLiteral("#ffaa55");
        case VCAudioTriggers::BandBeat:
        default: return QStringLiteral("#ffffff");
    }
}

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

    // Fixed 6 source mappings (Low, Mid, High, Volume, Beat, Kick)
    m_bandMappings.resize(BandSourceCount);
    for (int i = 0; i < BandSourceCount; i++)
        m_bandMappings[i].source = BandSource(i);

    m_audioLevels.clear();
    m_audioLevels.reserve(BandSourceCount);
    for (int i = 0; i < BandSourceCount; i++)
        m_audioLevels.append(0);

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
        m_inputCapture->registerBandsNumber(BandSourceCount - 1);

        // Push current profile's aubio config to the global processor
        m_inputCapture->setAubioConfig(profileChannelConfig().aubio);

        // Invalid ID: Stop every other widget
        emit functionStarting(this, Function::invalidId());

        for (BandMapping &bm : m_bandMappings)
        {
            if (bm.type == VCAudioTriggers::BarType::DMXBar)
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
            m_inputCapture->unregisterBandsNumber(BandSourceCount - 1);
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
    // Fixed: 6 source mappings (Low, Mid, High, Volume, Beat, Kick).
    return BandSourceCount;
}

void VCAudioTriggers::setBarsNumber(int num)
{
    // No-op: the mapping count is fixed at BandSourceCount. Kept for legacy
    // QML/MCP callers; emit signals so any cached binding refreshes.
    Q_UNUSED(num)
    emit barsNumberChanged();
    emit barsInfoChanged();
}

int VCAudioTriggers::selectedBar() const
{
    return m_selectedBar;
}

void VCAudioTriggers::setSelectedBar(int index)
{
    if (index < -1 || index >= m_bandMappings.count())
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

    // Push the resolved profile's aubio config to the global processor
    if (m_inputCapture)
        m_inputCapture->setAubioConfig(profileChannelConfig().aubio);

    emit audioProfileIdChanged();
    emit configChanged();
    emit audioLevelsChanged();
}

// Canonical lows/mids/highs come from AudioChannel::buildSnapshot() via the
// LedFx-parity freq_power_filter (audio.py:1306 get_freq_power(i, filtered=True)).
// The widget MUST display the real pipeline value — never re-slice or recompute.
double VCAudioTriggers::lowsPower() const
{
    return m_cachedSnapshot.lows;
}

int VCAudioTriggers::sampleRateValue() const
{
    return m_inputCapture ? int(m_inputCapture->sampleRate()) : 44100;
}

int VCAudioTriggers::framesPerSecond() const
{
    // Hop size is fixed at 512; aubio emits one result per hop.
    return m_inputCapture ? int(m_inputCapture->sampleRate() / 512) : (44100 / 512);
}

// LedFx ref: audio.py:1306 get_freq_power(i, filtered=True).
double VCAudioTriggers::midsPower() const
{
    return m_cachedSnapshot.mids;
}

// LedFx ref: audio.py:1306 get_freq_power(i, filtered=True).
double VCAudioTriggers::highsPower() const
{
    return m_cachedSnapshot.highs;
}

// Trigger-active flags: 3 mel banks map straight onto triggers[0..2]
// (low / mid / high). Volume / beat / kick live in their own snapshot fields.
bool VCAudioTriggers::triggerLowActive() const
{
    return m_cachedSnapshot.triggers[0].active;
}
bool VCAudioTriggers::triggerMidActive() const
{
    return m_cachedSnapshot.triggers[1].active;
}
bool VCAudioTriggers::triggerHighActive() const
{
    return m_cachedSnapshot.triggers[2].active;
}
bool VCAudioTriggers::beatActive() const { return m_beatActive; }

int VCAudioTriggers::lowCutBin() const
{
    return AudioCapture::lowCutBin(BandSourceCount - 1);
}

int VCAudioTriggers::highCutBin() const
{
    return AudioCapture::highCutBin(BandSourceCount - 1);
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

// ---- Kick detector ---------------------------------------------------------

double VCAudioTriggers::kickBeatMaxHz() const { return profileChannelConfig().kick.beatMaxHz; }
double VCAudioTriggers::kickBeatMinPercentDiff() const { return profileChannelConfig().kick.beatMinPercentDiff; }
double VCAudioTriggers::kickBeatMinAmplitude() const { return profileChannelConfig().kick.beatMinAmplitude; }
double VCAudioTriggers::kickBeatRefractorySec() const { return profileChannelConfig().kick.beatRefractorySec; }
int VCAudioTriggers::kickBeatHistoryLen() const { return profileChannelConfig().kick.beatHistoryLen; }
bool VCAudioTriggers::kickEnabled() const { return profileChannelConfig().kick.enabled; }

double VCAudioTriggers::kickValue() const { return m_cachedSnapshot.kickTrigger.value; }
bool VCAudioTriggers::kickActive() const { return m_cachedSnapshot.kickTrigger.active; }
bool VCAudioTriggers::kickFired() const { return m_cachedSnapshot.kickTrigger.firedThisFrame; }
bool VCAudioTriggers::kickLampActive() const { return m_kickLampHoldRemainingMs > 0.0; }

void VCAudioTriggers::setKickEnabled(bool enabled)
{
    AudioChannelConfig config = profileChannelConfig();
    config.kick.enabled = enabled;
    applyChannelConfig(config);
}

void VCAudioTriggers::setKickBeatMaxHz(double hz)
{
    AudioChannelConfig config = profileChannelConfig();
    config.kick.beatMaxHz = qBound(20.0, hz, 1000.0);
    applyChannelConfig(config);
}

void VCAudioTriggers::setKickBeatMinPercentDiff(double value)
{
    AudioChannelConfig config = profileChannelConfig();
    config.kick.beatMinPercentDiff = qBound(0.0, value, 5.0);
    applyChannelConfig(config);
}

void VCAudioTriggers::setKickBeatMinAmplitude(double value)
{
    AudioChannelConfig config = profileChannelConfig();
    config.kick.beatMinAmplitude = qBound(0.0, value, 10.0);
    applyChannelConfig(config);
}

void VCAudioTriggers::setKickBeatRefractorySec(double sec)
{
    AudioChannelConfig config = profileChannelConfig();
    config.kick.beatRefractorySec = qBound(0.0, sec, 2.0);
    applyChannelConfig(config);
}

void VCAudioTriggers::setKickBeatHistoryLen(int frames)
{
    AudioChannelConfig config = profileChannelConfig();
    config.kick.beatHistoryLen = qBound(1, frames, 500);
    applyChannelConfig(config);
}

// ---- Mel post-processing ---------------------------------------------------

bool VCAudioTriggers::melPostEnabled() const { return profileChannelConfig().melPost.enabled; }
double VCAudioTriggers::melPowerFactor() const { return profileChannelConfig().melPost.powerFactor; }
double VCAudioTriggers::melGaussianSigma() const { return profileChannelConfig().melPost.gaussianSigma; }
double VCAudioTriggers::melSmoothDecay() const { return profileChannelConfig().melPost.smoothDecay; }
double VCAudioTriggers::melSmoothRise() const { return profileChannelConfig().melPost.smoothRise; }
double VCAudioTriggers::melCommonDecay() const { return profileChannelConfig().melPost.commonDecay; }
double VCAudioTriggers::melCommonRise() const { return profileChannelConfig().melPost.commonRise; }
double VCAudioTriggers::melDiffDecay() const { return profileChannelConfig().melPost.diffDecay; }
double VCAudioTriggers::melDiffRise() const { return profileChannelConfig().melPost.diffRise; }
double VCAudioTriggers::freqPowerDecay() const { return profileChannelConfig().freqPowerDecay; }
double VCAudioTriggers::freqPowerRise() const { return profileChannelConfig().freqPowerRise; }

void VCAudioTriggers::setMelPostEnabled(bool enabled)
{
    AudioChannelConfig config = profileChannelConfig();
    config.melPost.enabled = enabled;
    applyChannelConfig(config);
}

void VCAudioTriggers::setMelPowerFactor(double value)
{
    AudioChannelConfig config = profileChannelConfig();
    config.melPost.powerFactor = qBound(0.1, value, 5.0);
    applyChannelConfig(config);
}

void VCAudioTriggers::setMelGaussianSigma(double value)
{
    AudioChannelConfig config = profileChannelConfig();
    config.melPost.gaussianSigma = qBound(0.1, value, 20.0);
    applyChannelConfig(config);
}

void VCAudioTriggers::setMelSmoothDecay(double value)
{
    AudioChannelConfig config = profileChannelConfig();
    config.melPost.smoothDecay = qBound(0.001, value, 1.0);
    applyChannelConfig(config);
}

void VCAudioTriggers::setMelSmoothRise(double value)
{
    AudioChannelConfig config = profileChannelConfig();
    config.melPost.smoothRise = qBound(0.001, value, 1.0);
    applyChannelConfig(config);
}

void VCAudioTriggers::setMelCommonDecay(double value)
{
    AudioChannelConfig config = profileChannelConfig();
    config.melPost.commonDecay = qBound(0.001, value, 1.0);
    applyChannelConfig(config);
}

void VCAudioTriggers::setMelCommonRise(double value)
{
    AudioChannelConfig config = profileChannelConfig();
    config.melPost.commonRise = qBound(0.001, value, 1.0);
    applyChannelConfig(config);
}

void VCAudioTriggers::setMelDiffDecay(double value)
{
    AudioChannelConfig config = profileChannelConfig();
    config.melPost.diffDecay = qBound(0.001, value, 1.0);
    applyChannelConfig(config);
}

void VCAudioTriggers::setMelDiffRise(double value)
{
    AudioChannelConfig config = profileChannelConfig();
    config.melPost.diffRise = qBound(0.001, value, 1.0);
    applyChannelConfig(config);
}

void VCAudioTriggers::setFreqPowerDecay(double value)
{
    AudioChannelConfig config = profileChannelConfig();
    config.freqPowerDecay = qBound(0.001, value, 1.0);
    applyChannelConfig(config);
}

void VCAudioTriggers::setFreqPowerRise(double value)
{
    AudioChannelConfig config = profileChannelConfig();
    config.freqPowerRise = qBound(0.001, value, 1.0);
    applyChannelConfig(config);
}

// applyMelPreset(raw/ledfx/punchy/smooth) was DELETED — see plan §0.
// No QML consumer; legacy preset surface that hid coupled MelPost defaults.

void VCAudioTriggers::setMelBankLow(double minHz, double maxHz, int bands)
{
    AudioChannelConfig config = profileChannelConfig();
    // Clamp to reasonable audio ranges; band count caps at the snapshot's
    // fixed-size buffer so we never overrun MelBankSnapshot::raw[].
    const double mn = qBound(0.0, minHz, 24000.0);
    const double mx = qBound(mn + 1.0, maxHz, 24000.0);
    config.aubio.melBanks.low.minHz = mn;
    config.aubio.melBanks.low.maxHz = mx;
    config.aubio.melBanks.low.bands = qBound(4, bands, AudioSnapshot::kMelBankBandsMax);
    config.aubio.melBanks.preset = QStringLiteral("Custom");
    applyChannelConfig(config);
}

void VCAudioTriggers::setMelBankMid(double minHz, double maxHz, int bands)
{
    AudioChannelConfig config = profileChannelConfig();
    const double mn = qBound(0.0, minHz, 24000.0);
    const double mx = qBound(mn + 1.0, maxHz, 24000.0);
    config.aubio.melBanks.mid.minHz = mn;
    config.aubio.melBanks.mid.maxHz = mx;
    config.aubio.melBanks.mid.bands = qBound(4, bands, AudioSnapshot::kMelBankBandsMax);
    config.aubio.melBanks.preset = QStringLiteral("Custom");
    applyChannelConfig(config);
}

void VCAudioTriggers::setMelBankHigh(double minHz, double maxHz, int bands)
{
    AudioChannelConfig config = profileChannelConfig();
    const double mn = qBound(0.0, minHz, 24000.0);
    const double mx = qBound(mn + 1.0, maxHz, 24000.0);
    config.aubio.melBanks.high.minHz = mn;
    config.aubio.melBanks.high.maxHz = mx;
    config.aubio.melBanks.high.bands = qBound(4, bands, AudioSnapshot::kMelBankBandsMax);
    config.aubio.melBanks.preset = QStringLiteral("Custom");
    applyChannelConfig(config);
}

void VCAudioTriggers::applyMelBankPreset(const QString &preset)
{
    AudioChannelConfig config = profileChannelConfig();
    const QString key = preset.trimmed().toLower();

    if (key == QLatin1String("edm"))
    {
        config.aubio.melBanks.low  = { 20.0,    350.0, 24 };
        config.aubio.melBanks.mid  = { 20.0,   2000.0, 24 };
        config.aubio.melBanks.high = { 20.0,  15000.0, 24 };
        config.aubio.melBanks.preset = QStringLiteral("EDM");
    }
    else if (key == QLatin1String("live"))
    {
        config.aubio.melBanks.low  = { 20.0,    500.0, 24 };
        config.aubio.melBanks.mid  = { 80.0,   4000.0, 24 };
        config.aubio.melBanks.high = { 500.0, 16000.0, 24 };
        config.aubio.melBanks.preset = QStringLiteral("Live");
    }
    else if (key == QLatin1String("acoustic"))
    {
        config.aubio.melBanks.low  = { 40.0,    300.0, 16 };
        config.aubio.melBanks.mid  = { 100.0,  3000.0, 16 };
        config.aubio.melBanks.high = { 1000.0,12000.0, 16 };
        config.aubio.melBanks.preset = QStringLiteral("Acoustic");
    }
    else if (key == QLatin1String("speech"))
    {
        config.aubio.melBanks.low  = { 80.0,    500.0, 16 };
        config.aubio.melBanks.mid  = { 200.0,  4000.0, 16 };
        config.aubio.melBanks.high = { 2000.0, 8000.0, 16 };
        config.aubio.melBanks.preset = QStringLiteral("Speech");
    }
    else if (key == QLatin1String("custom"))
    {
        config.aubio.melBanks.preset = QStringLiteral("Custom");
    }
    else
    {
        return; // unknown preset -> no-op
    }
    applyChannelConfig(config);
}

// Legacy 5-band bandLayout getters / setters removed; the engine now drives
// low/mid/high decomposition through the multi-resolution mel banks.
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
QString VCAudioTriggers::pitchMethod() const { return profileChannelConfig().aubio.pitchMethod; }
QString VCAudioTriggers::pitchUnit() const { return profileChannelConfig().aubio.pitchUnit; }
double VCAudioTriggers::pitchSilenceDb() const { return profileChannelConfig().aubio.pitchSilenceDb; }
double VCAudioTriggers::pitchTolerance() const { return profileChannelConfig().aubio.pitchTolerance; }
double VCAudioTriggers::tempoSilenceDb() const { return profileChannelConfig().aubio.tempoSilenceDb; }
double VCAudioTriggers::tempoThreshold() const { return profileChannelConfig().aubio.tempoThreshold; }
int VCAudioTriggers::tatumSubdivision() const { return profileChannelConfig().aubio.tatumSubdivision; }
int VCAudioTriggers::beatsPerBar() const { return profileChannelConfig().aubio.beatsPerBar; }
bool VCAudioTriggers::preEmphasisEnabled() const { return profileChannelConfig().aubio.preEmphasisEnabled; }
double VCAudioTriggers::tssAlpha() const { return profileChannelConfig().aubio.tssAlpha; }
double VCAudioTriggers::tssBeta() const { return profileChannelConfig().aubio.tssBeta; }
double VCAudioTriggers::tssThreshold() const { return profileChannelConfig().aubio.tssThreshold; }

QString VCAudioTriggers::windowType() const { return profileChannelConfig().aubio.windowType; }
QString VCAudioTriggers::melScale() const { return profileChannelConfig().aubio.melScale; }
double VCAudioTriggers::tempoDelayMs() const { return profileChannelConfig().aubio.tempoDelayMs; }
double VCAudioTriggers::noteSilenceDb() const { return profileChannelConfig().aubio.noteSilenceDb; }
double VCAudioTriggers::noteMinIntervalMs() const { return profileChannelConfig().aubio.noteMinIntervalMs; }
double VCAudioTriggers::noteReleaseDropDb() const { return profileChannelConfig().aubio.noteReleaseDropDb; }
double VCAudioTriggers::mfccPower() const { return profileChannelConfig().aubio.mfccPower; }
double VCAudioTriggers::mfccScale() const { return profileChannelConfig().aubio.mfccScale; }

QVariantList VCAudioTriggers::onsetMethodsEnabled() const
{
    const AubioConfig &cfg = profileChannelConfig().aubio;
    QVariantList list;
    list.reserve(9);
    for (int i = 0; i < 9; i++)
        list.append(cfg.onsetMethodEnabled[i]);
    return list;
}

QVariantList VCAudioTriggers::onsetMethodOverrides() const
{
    const AubioConfig &cfg = profileChannelConfig().aubio;
    QVariantList list;
    list.reserve(9);
    for (int i = 0; i < 9; i++)
    {
        const OnsetMethodOverride &ov = cfg.onsetOverrides[i];
        QVariantMap m;
        m[QStringLiteral("threshold")]   = ov.threshold;
        m[QStringLiteral("silenceDb")]   = ov.silenceDb;
        m[QStringLiteral("minioiMs")]    = ov.minioiMs;
        m[QStringLiteral("delayMs")]     = ov.delayMs;
        m[QStringLiteral("compression")] = ov.compression;
        m[QStringLiteral("awhitening")]  = ov.awhitening;
        list.append(m);
    }
    return list;
}

double VCAudioTriggers::noteMidi() const { return m_cachedSnapshot.note.midi; }
double VCAudioTriggers::noteVelocity() const { return m_cachedSnapshot.note.velocity; }
bool VCAudioTriggers::noteOn() const { return m_cachedSnapshot.note.noteOn; }
bool VCAudioTriggers::noteOff() const { return m_cachedSnapshot.note.noteOff; }

QVariantList VCAudioTriggers::onsetDescriptorValues() const { return m_onsetDescriptorCache; }
QVariantList VCAudioTriggers::onsetThresholdedValues() const { return m_onsetThresholdedCache; }

double VCAudioTriggers::pitchHz() const { return m_cachedSnapshot.pitch.hz; }
double VCAudioTriggers::pitchConfidence() const { return m_cachedSnapshot.pitch.confidence; }
double VCAudioTriggers::detectedBpm() const { return m_cachedSnapshot.music.bpm; }
double VCAudioTriggers::beatConfidence() const { return m_cachedSnapshot.music.beatConfidence; }
double VCAudioTriggers::beatPhase() const { return m_cachedSnapshot.music.beatPhase; }
double VCAudioTriggers::barPhase() const { return m_cachedSnapshot.music.barPhase; }

QVariantList VCAudioTriggers::tssTransientNorm() const
{
    QVariantList list;
    const int n = m_cachedSnapshot.tss.binCount;
    list.reserve(n);
    for (int i = 0; i < n; i++)
        list.append(m_cachedSnapshot.tss.transientNorm[i]);
    return list;
}

QVariantList VCAudioTriggers::tssSteadyNorm() const
{
    QVariantList list;
    const int n = m_cachedSnapshot.tss.binCount;
    list.reserve(n);
    for (int i = 0; i < n; i++)
        list.append(m_cachedSnapshot.tss.steadyNorm[i]);
    return list;
}

int VCAudioTriggers::tssBinCount() const { return m_cachedSnapshot.tss.binCount; }

QVariantList VCAudioTriggers::onsetFlags() const
{
    const auto &o = m_cachedSnapshot.onsets;
    QVariantList list;
    list.reserve(9);
    list.append(o.energy);
    list.append(o.hfc);
    list.append(o.complex_);
    list.append(o.phase);
    list.append(o.wphase);
    list.append(o.specdiff);
    list.append(o.kl);
    list.append(o.mkl);
    list.append(o.specflux);
    return list;
}

QVariantList VCAudioTriggers::melSpectrum() const { return m_melSpectrumCache; }
QVariantList VCAudioTriggers::melSpectrumProcessed() const { return m_melSpectrumProcessedCache; }
QVariantList VCAudioTriggers::melSpectrumNovelty() const { return m_melSpectrumNoveltyCache; }
QVariantList VCAudioTriggers::mfccCoeffs() const { return m_mfccCoeffsCache; }

QVariantList VCAudioTriggers::melLowValues() const { return m_melLowValuesCache; }
QVariantList VCAudioTriggers::melMidValues() const { return m_melMidValuesCache; }
QVariantList VCAudioTriggers::melHighValues() const { return m_melHighValuesCache; }
// QString VCAudioTriggers::melPreset() DELETED — see plan-clean-engineering.md §0.
double VCAudioTriggers::melLowMinHz() const { return profileChannelConfig().aubio.melBanks.low.minHz; }
double VCAudioTriggers::melLowMaxHz() const { return profileChannelConfig().aubio.melBanks.low.maxHz; }
int    VCAudioTriggers::melLowBands() const { return profileChannelConfig().aubio.melBanks.low.bands; }
double VCAudioTriggers::melMidMinHz() const { return profileChannelConfig().aubio.melBanks.mid.minHz; }
double VCAudioTriggers::melMidMaxHz() const { return profileChannelConfig().aubio.melBanks.mid.maxHz; }
int    VCAudioTriggers::melMidBands() const { return profileChannelConfig().aubio.melBanks.mid.bands; }
double VCAudioTriggers::melHighMinHz() const { return profileChannelConfig().aubio.melBanks.high.minHz; }
double VCAudioTriggers::melHighMaxHz() const { return profileChannelConfig().aubio.melBanks.high.maxHz; }
int    VCAudioTriggers::melHighBands() const { return profileChannelConfig().aubio.melBanks.high.bands; }

QVariantList VCAudioTriggers::timelineFramesSnapshot() const
{
    QVariantList list;
    const int count = qMin(m_timeline.size(), kTimelineCapacity);
    if (count == 0)
        return list;

    const int startIdx = (m_timelineWriteIdx >= kTimelineCapacity)
        ? (m_timelineWriteIdx % kTimelineCapacity) : 0;

    list.reserve(count);
    for (int i = 0; i < count; ++i)
    {
        const int idx = (startIdx + i) % kTimelineCapacity;
        const TimelineFrame &tf = m_timeline.at(idx);
        QVariantMap m;
        m[QStringLiteral("pitchHz")] = tf.pitchHz;
        m[QStringLiteral("pitchConf")] = tf.pitchConfidence;
        m[QStringLiteral("onsetMask")] = tf.onsetMask;
        m[QStringLiteral("beat")] = tf.beat;
        m[QStringLiteral("bpm")] = tf.bpm;
        m[QStringLiteral("noteMidi")] = tf.noteMidi;
        m[QStringLiteral("noteVelocity")] = tf.noteVelocity;
        m[QStringLiteral("noteOn")] = tf.noteOn;
        m[QStringLiteral("noteOff")] = tf.noteOff;
        list.append(m);
    }
    return list;
}

namespace
{
    // 3 mel banks (low/mid/high) replace the former 5 perceptual bands.
    // Band power is the mean of each bank's processed mel output.
}

// Mel band crossover indices for QML spectrum coloring. Map the multi-mel
// bank Hz cuts to bin indices in the legacy 40-band aubio mel filterbank.
// Mel-bin spacing is approximately log-frequency, so a coarse linear-Hz
// mapping is fine for visual coloring.
int VCAudioTriggers::melCrossLowMid() const
{
    const auto &cfg = profileChannelConfig().aubio.melBanks.low;
    const double nyquist = m_inputCapture ? double(m_inputCapture->sampleRate()) / 2.0 : 22050.0;
    return qBound(1, int(cfg.maxHz / nyquist * AUBIO_MEL_BANDS + 0.5), AUBIO_MEL_BANDS - 1);
}
int VCAudioTriggers::melCrossMid() const
{
    const auto &cfg = profileChannelConfig().aubio.melBanks.mid;
    const double nyquist = m_inputCapture ? double(m_inputCapture->sampleRate()) / 2.0 : 22050.0;
    return qBound(1, int(cfg.maxHz / nyquist * AUBIO_MEL_BANDS + 0.5), AUBIO_MEL_BANDS - 1);
}

// =====================================================================
// Phase A — power-bar Hz crossover setters.
// Strictly-increasing chain: beat < bass < mids < highs, all within
// [10 Hz, sampleRate/2]. Out-of-order requests are silently rejected
// (with a qDebug for visibility) so the QML Spinbox can clamp itself
// against the readback.
// =====================================================================
namespace
{
    inline double clampHz(double hz, double lo, double hi)
    {
        return std::max(lo, std::min(hi, hz));
    }
}

void VCAudioTriggers::setBeatCutoffHz(double hz)
{
    const double nyquist = m_inputCapture ? double(m_inputCapture->sampleRate()) / 2.0 : 22050.0;
    const double next = clampHz(hz, 10.0, nyquist);
    if (next >= m_bassCutoffHz)
        return;
    if (qFuzzyCompare(m_beatCutoffHz + 1.0, next + 1.0))
        return;
    m_beatCutoffHz = next;
    updateAudioProfileSnapshotPowers();
    emit configChanged();
}

void VCAudioTriggers::setBassCutoffHz(double hz)
{
    const double nyquist = m_inputCapture ? double(m_inputCapture->sampleRate()) / 2.0 : 22050.0;
    const double next = clampHz(hz, 10.0, nyquist);
    if (next <= m_beatCutoffHz || next >= m_midsCutoffHz)
        return;
    if (qFuzzyCompare(m_bassCutoffHz + 1.0, next + 1.0))
        return;
    m_bassCutoffHz = next;
    updateAudioProfileSnapshotPowers();
    emit configChanged();
}

void VCAudioTriggers::setMidsCutoffHz(double hz)
{
    const double nyquist = m_inputCapture ? double(m_inputCapture->sampleRate()) / 2.0 : 22050.0;
    const double next = clampHz(hz, 10.0, nyquist);
    if (next <= m_bassCutoffHz || next >= m_highsCutoffHz)
        return;
    if (qFuzzyCompare(m_midsCutoffHz + 1.0, next + 1.0))
        return;
    m_midsCutoffHz = next;
    updateAudioProfileSnapshotPowers();
    emit configChanged();
}

void VCAudioTriggers::setHighsCutoffHz(double hz)
{
    const double nyquist = m_inputCapture ? double(m_inputCapture->sampleRate()) / 2.0 : 22050.0;
    const double next = clampHz(hz, 10.0, nyquist);
    if (next <= m_midsCutoffHz)
        return;
    if (qFuzzyCompare(m_highsCutoffHz + 1.0, next + 1.0))
        return;
    m_highsCutoffHz = next;
    updateAudioProfileSnapshotPowers();
    emit configChanged();
}

// MFCC display scale — pixels per MFCC unit. Owned by C++ so the QML
// signed-bar widget can plot |mfccCoeffs[i]| * mfccDisplayScale without
// re-deriving anything from mfccPower / mfccScale.
double VCAudioTriggers::mfccDisplayScale() const
{
    const auto &a = profileChannelConfig().aubio;
    // Heuristic: aubio MFCC magnitudes typically land in [0..30] for normal
    // music when (mfccPower=1, mfccScale=1). Inverse-scale by mfccScale and
    // by max(mfccPower, 1) to keep peaks near full deflection across the
    // common settings range. Floor prevents division by zero.
    const double denom = std::max(1.0, std::abs(a.mfccScale)) * std::max(1.0, a.mfccPower);
    return 1.0 / (10.0 * denom);
}


void VCAudioTriggers::setPitchMethod(const QString &method)
{
    AudioChannelConfig config = profileChannelConfig();
    if (config.aubio.pitchMethod == method)
        return;
    config.aubio.pitchMethod = method;
    applyChannelConfig(config);
}

void VCAudioTriggers::setPitchUnit(const QString &unit)
{
    // Validate against aubio_pitch_set_unit's accepted vocabulary; fall back
    // to "Hz" silently if the QML hands us something unexpected, so we never
    // pass a bogus string into aubio.
    static const QStringList kPitchUnits = {
        QStringLiteral("Hz"), QStringLiteral("midi"),
        QStringLiteral("cent"), QStringLiteral("bin")
    };
    QString validated = kPitchUnits.contains(unit) ? unit : QStringLiteral("Hz");
    AudioChannelConfig config = profileChannelConfig();
    if (config.aubio.pitchUnit == validated)
        return;
    config.aubio.pitchUnit = validated;
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

void VCAudioTriggers::setBeatsPerBar(int n)
{
    AudioChannelConfig config = profileChannelConfig();
    n = qBound(1, n, 8);
    if (config.aubio.beatsPerBar == n)
        return;
    config.aubio.beatsPerBar = n;
    applyChannelConfig(config);
}

void VCAudioTriggers::setPreEmphasisEnabled(bool enabled)
{
    AudioChannelConfig config = profileChannelConfig();
    if (config.aubio.preEmphasisEnabled == enabled)
        return;
    config.aubio.preEmphasisEnabled = enabled;
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

void VCAudioTriggers::setWindowType(const QString &type)
{
    static const QStringList kAllowed = {
        QStringLiteral("default"),    QStringLiteral("rectangle"),
        QStringLiteral("hamming"),    QStringLiteral("hanning"),
        QStringLiteral("hanningz"),   QStringLiteral("blackman"),
        QStringLiteral("blackman_harris"), QStringLiteral("gaussian"),
        QStringLiteral("welch"),      QStringLiteral("parzen")
    };
    QString sanitized = kAllowed.contains(type) ? type : QStringLiteral("default");
    AudioChannelConfig config = profileChannelConfig();
    if (config.aubio.windowType == sanitized)
        return;
    config.aubio.windowType = sanitized;
    applyChannelConfig(config);
}

void VCAudioTriggers::setMelScale(const QString &scale)
{
    QString sanitized = scale;
    if (sanitized.compare(QStringLiteral("matt_mel"), Qt::CaseInsensitive) != 0
        && sanitized.compare(QStringLiteral("htk"), Qt::CaseInsensitive) != 0
        && sanitized.compare(QStringLiteral("slaney"), Qt::CaseInsensitive) != 0)
    {
        sanitized = QStringLiteral("matt_mel");
    }
    AudioChannelConfig config = profileChannelConfig();
    if (config.aubio.melScale == sanitized)
        return;
    config.aubio.melScale = sanitized;
    applyChannelConfig(config);
}

void VCAudioTriggers::setTempoDelayMs(double ms)
{
    AudioChannelConfig config = profileChannelConfig();
    ms = qBound(-1000.0, ms, 1000.0);
    if (qFuzzyCompare(config.aubio.tempoDelayMs + 1.0, ms + 1.0))
        return;
    config.aubio.tempoDelayMs = ms;
    applyChannelConfig(config);
}

void VCAudioTriggers::setNoteSilenceDb(double db)
{
    AudioChannelConfig config = profileChannelConfig();
    db = qBound(-120.0, db, 0.0);
    if (qFuzzyCompare(config.aubio.noteSilenceDb + 1.0, db + 1.0))
        return;
    config.aubio.noteSilenceDb = db;
    applyChannelConfig(config);
}

void VCAudioTriggers::setNoteMinIntervalMs(double ms)
{
    AudioChannelConfig config = profileChannelConfig();
    ms = qBound(0.0, ms, 5000.0);
    if (qFuzzyCompare(config.aubio.noteMinIntervalMs + 1.0, ms + 1.0))
        return;
    config.aubio.noteMinIntervalMs = ms;
    applyChannelConfig(config);
}

void VCAudioTriggers::setNoteReleaseDropDb(double db)
{
    AudioChannelConfig config = profileChannelConfig();
    db = qBound(0.0, db, 120.0);
    if (qFuzzyCompare(config.aubio.noteReleaseDropDb + 1.0, db + 1.0))
        return;
    config.aubio.noteReleaseDropDb = db;
    applyChannelConfig(config);
}

void VCAudioTriggers::setMfccPower(double power)
{
    AudioChannelConfig config = profileChannelConfig();
    power = qBound(0.0, power, 8.0);
    if (qFuzzyCompare(config.aubio.mfccPower + 1.0, power + 1.0))
        return;
    config.aubio.mfccPower = power;
    applyChannelConfig(config);
}

void VCAudioTriggers::setMfccScale(double scale)
{
    AudioChannelConfig config = profileChannelConfig();
    scale = qBound(0.0, scale, 1000.0);
    if (qFuzzyCompare(config.aubio.mfccScale + 1.0, scale + 1.0))
        return;
    config.aubio.mfccScale = scale;
    applyChannelConfig(config);
}

void VCAudioTriggers::setOnsetMethodEnabled(int idx, bool enabled)
{
    if (idx < 0 || idx >= 9)
        return;
    AudioChannelConfig config = profileChannelConfig();
    if (config.aubio.onsetMethodEnabled[idx] == enabled)
        return;
    config.aubio.onsetMethodEnabled[idx] = enabled;
    applyChannelConfig(config);
}

void VCAudioTriggers::setOnsetHistorySeconds(int s)
{
    s = qBound(1, s, 120);
    if (s == m_onsetHistorySeconds) return;
    m_onsetHistorySeconds = s;
    emit configChanged();
    setDocModified();
}

void VCAudioTriggers::setOnsetMethodOverrideField(int idx, const QString &fieldName, double value)
{
    if (idx < 0 || idx >= 9)
        return;
    AudioChannelConfig config = profileChannelConfig();
    OnsetMethodOverride &ov = config.aubio.onsetOverrides[idx];

    // qIsNaN encodes "clear this single field back to its sentinel" — that
    // way QML can reset one column without disturbing the others.
    const bool clear = qIsNaN(value);

    if (fieldName == QLatin1String("threshold"))
        ov.threshold = clear ? -1.0 : qBound(0.0, value, 10.0);
    else if (fieldName == QLatin1String("silenceDb"))
        ov.silenceDb = clear ? -999.0 : qBound(-120.0, value, 0.0);
    else if (fieldName == QLatin1String("minioiMs"))
        ov.minioiMs  = clear ? -1.0 : qBound(0.0, value, 5000.0);
    else if (fieldName == QLatin1String("delayMs"))
        ov.delayMs   = clear ? -9999.0 : qBound(-1000.0, value, 5000.0);
    else if (fieldName == QLatin1String("compression"))
        ov.compression = clear ? -1.0 : qBound(0.0, value, 100.0);
    else if (fieldName == QLatin1String("awhitening"))
        ov.awhitening = clear ? -1 : (value > 0.5 ? 1 : 0);
    else
        return;

    applyChannelConfig(config);
}

void VCAudioTriggers::resetOnsetMethodOverride(int idx)
{
    if (idx < 0 || idx >= 9)
        return;
    AudioChannelConfig config = profileChannelConfig();
    config.aubio.onsetOverrides[idx] = OnsetMethodOverride{};
    applyChannelConfig(config);
}

QVariantMap VCAudioTriggers::onsetMethodDefaults(int idx) const
{
    QVariantMap m;
    if (idx < 0 || idx >= 9)
        return m;
    const OnsetMethodOverride def = readAubioOnsetDefaults(idx);
    m[QStringLiteral("threshold")]   = def.threshold;
    m[QStringLiteral("silenceDb")]   = def.silenceDb;
    m[QStringLiteral("minioiMs")]    = def.minioiMs;
    m[QStringLiteral("delayMs")]     = def.delayMs;
    m[QStringLiteral("compression")] = def.compression;
    m[QStringLiteral("awhitening")]  = def.awhitening;
    return m;
}

QVariantMap VCAudioTriggers::triggerState(int band) const
{
    QVariantMap result;
    if (band < 0 || band >= 3)
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

void VCAudioTriggers::updateAudioProfileSnapshotPowers(bool emitVisuals)
{
    AudioProfile *profile = resolvedAudioProfile();
    AudioChannel *channel = profile ? profile->channel() : nullptr;

    if (channel)
    {
        m_cachedSnapshot = channel->snapshot();

        // Sticky kick lamp latch — updated every hop (this function is
        // called per-hop), not just on visual frames. Without this, short
        // kicks (default holdMs=50) can fall entirely between two ~40ms QML
        // refreshes and never light the lamp.
        const auto &kt = m_cachedSnapshot.kickTrigger;
        if (kt.active || kt.firedThisFrame)
            m_kickLampHoldRemainingMs = kKickLampHoldMs;
        else
            m_kickLampHoldRemainingMs = std::max(0.0,
                m_kickLampHoldRemainingMs - m_cachedSnapshot.audioDtMs);
    }
    else
    {
        m_cachedSnapshot = AudioSnapshot{};
        m_kickLampHoldRemainingMs = 0.0;
    }

    if (!emitVisuals)
        return;

    // Rebuild trigger states cache for reactive QML bindings (3 mel banks).
    m_triggerStatesCache.clear();
    m_triggerStatesCache.reserve(3);
    for (int i = 0; i < 3; ++i)
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
    m_melSpectrumProcessedCache.clear();
    m_melSpectrumProcessedCache.reserve(AUBIO_MEL_BANDS);
    m_melSpectrumNoveltyCache.clear();
    m_melSpectrumNoveltyCache.reserve(AUBIO_MEL_BANDS);
    for (int i = 0; i < AUBIO_MEL_BANDS; ++i)
    {
        m_melSpectrumCache.append(m_cachedSnapshot.mel[i]);
        m_melSpectrumProcessedCache.append(m_cachedSnapshot.melProcessed[i]);
        m_melSpectrumNoveltyCache.append(m_cachedSnapshot.melNovelty[i]);
    }

    // Multi-resolution mel bank caches. Populated only when the snapshot
    // carries valid bank data (count > 0) — otherwise left empty so the QML
    // can show a "no signal" placeholder via list.length === 0.
    m_melLowValuesCache.clear();
    m_melMidValuesCache.clear();
    m_melHighValuesCache.clear();
    m_melLowValuesCache.reserve(m_cachedSnapshot.melLow.count);
    m_melMidValuesCache.reserve(m_cachedSnapshot.melMid.count);
    m_melHighValuesCache.reserve(m_cachedSnapshot.melHigh.count);
    for (int i = 0; i < m_cachedSnapshot.melLow.count; ++i)
        m_melLowValuesCache.append(m_cachedSnapshot.melLow.processed[i]);
    for (int i = 0; i < m_cachedSnapshot.melMid.count; ++i)
        m_melMidValuesCache.append(m_cachedSnapshot.melMid.processed[i]);
    for (int i = 0; i < m_cachedSnapshot.melHigh.count; ++i)
        m_melHighValuesCache.append(m_cachedSnapshot.melHigh.processed[i]);

    m_mfccCoeffsCache.clear();
    m_mfccCoeffsCache.reserve(AUBIO_MFCC_COEFFS);
    for (int i = 0; i < AUBIO_MFCC_COEFFS; ++i)
        m_mfccCoeffsCache.append(m_cachedSnapshot.mfcc[i]);

    m_onsetDescriptorCache.clear();
    m_onsetDescriptorCache.reserve(AUBIO_ONSET_METHODS);
    m_onsetThresholdedCache.clear();
    m_onsetThresholdedCache.reserve(AUBIO_ONSET_METHODS);
    for (int i = 0; i < AUBIO_ONSET_METHODS; ++i)
    {
        m_onsetDescriptorCache.append(m_cachedSnapshot.onsets.descriptors[i]);
        m_onsetThresholdedCache.append(m_cachedSnapshot.onsets.thresholdedDescriptors[i]);
    }

    // Append a new TimelineFrame (24s ring buffer)
    {
        TimelineFrame tf;
        tf.pitchHz = float(m_cachedSnapshot.pitch.hz);
        tf.pitchConfidence = float(m_cachedSnapshot.pitch.confidence);
        tf.beat = m_cachedSnapshot.music.beat;
        tf.bpm = float(m_cachedSnapshot.music.bpm);
        tf.noteMidi = float(m_cachedSnapshot.note.midi);
        tf.noteVelocity = float(m_cachedSnapshot.note.velocity);
        tf.noteOn = m_cachedSnapshot.note.noteOn;
        tf.noteOff = m_cachedSnapshot.note.noteOff;

        const auto &o = m_cachedSnapshot.onsets;
        quint16 mask = 0;
        if (o.energy)   mask |= 0x001;
        if (o.hfc)      mask |= 0x002;
        if (o.complex_) mask |= 0x004;
        if (o.phase)    mask |= 0x008;
        if (o.wphase)   mask |= 0x010;
        if (o.specdiff) mask |= 0x020;
        if (o.kl)       mask |= 0x040;
        if (o.mkl)      mask |= 0x080;
        if (o.specflux) mask |= 0x100;
        tf.onsetMask = mask;

        if (m_timeline.size() < kTimelineCapacity)
            m_timeline.append(tf);
        else
            m_timeline[m_timelineWriteIdx % kTimelineCapacity] = tf;
        m_timelineWriteIdx++;
    }

    // ===================================================================
    // Phase A — display-ready scalars (Iron Rule: ALL math lives here).
    // Refer to docs/audio-dsp-plans/plan-widget-impl.md §0.
    // ===================================================================

    // 1. AGC scalar from MelPostProcessor (1.0 when post-processing off).
    m_melAgcGain = m_cachedSnapshot.melAgcGain;

    // 2. Power slices — Fix 2 of plan-dsp-harmonize.md: the canonical
    //    lows/mids/highs are computed in AudioChannel via the LedFx-parity
    //    freq_power_filter (audio.py:1306 get_freq_power(i, filtered=True))
    //    and exposed as m_cachedSnapshot.lows/.mids/.highs. The widget reads
    //    them directly via lowsPower()/midsPower()/highsPower() and the
    //    *Sliced getters; no display-side recompute is performed here.

    // 3. Onset descriptor display — per-method peak normalisation. Decay
    //    each peak per audio hop; bump up to current value on overshoot.
    m_onsetDescriptorDisplayCache.clear();
    m_onsetDescriptorDisplayCache.reserve(AUBIO_ONSET_METHODS);
    for (int i = 0; i < AUBIO_ONSET_METHODS; ++i)
    {
        const double v = std::abs(m_cachedSnapshot.onsets.descriptors[i]);
        m_methodPeak[i] = std::max(kOnsetPeakFloor,
                                   std::max(v, m_methodPeak[i] * kOnsetPeakDecay));
        const double normalised = std::clamp(v / m_methodPeak[i], 0.0, 1.0);
        m_onsetDescriptorDisplayCache.append(normalised);
    }

    // 4. Pitch — log2 mapping over [20 Hz, 20 kHz].
    {
        const double hz = m_cachedSnapshot.pitch.hz;
        if (hz > 20.0)
        {
            constexpr double kLogLo = 20.0;
            constexpr double kLogRange = 9.965784284662087; // log2(20000/20)
            m_pitchDisplay = std::clamp(std::log2(hz / kLogLo) / kLogRange, 0.0, 1.0);
        }
        else
        {
            m_pitchDisplay = 0.0;
        }

        // Note name. MIDI note 69 = A4 = 440 Hz. We render only when there's
        // a usable pitch and the note module produced a confident MIDI value.
        if (hz > 20.0 && m_cachedSnapshot.note.midi > 0.0)
        {
            static const char *kNoteNames[12] = {
                "C", "C#", "D", "D#", "E", "F",
                "F#", "G", "G#", "A", "A#", "B"
            };
            const int midi = int(std::round(m_cachedSnapshot.note.midi));
            const int pitchClass = ((midi % 12) + 12) % 12;
            const int octave = midi / 12 - 1;
            m_pitchNoteText = QStringLiteral("%1%2")
                                  .arg(QLatin1String(kNoteNames[pitchClass]))
                                  .arg(octave);
        }
        else
        {
            m_pitchNoteText = QStringLiteral("--");
        }
    }

    // 5. Spectral centroid — log10 over [20 Hz, 15 kHz].
    {
        const double centroidHz = m_cachedSnapshot.features.centroidHz;
        if (centroidHz > 20.0)
        {
            constexpr double kLogLo = 20.0;
            constexpr double kLogRange = 2.8750612633917; // log10(15000/20)
            m_spectralCentroidDisplay = std::clamp(
                std::log10(centroidHz / kLogLo) / kLogRange, 0.0, 1.0);
        }
        else
        {
            m_spectralCentroidDisplay = 0.0;
        }
    }

    // 6. Flatness passthrough (already 0..1 from the snapshot).
    m_spectralFlatnessDisplay = std::clamp(m_cachedSnapshot.features.flatness, 0.0, 1.0);

    // 7. Flux — peak-tracked (same shape as onset descriptors).
    {
        const double v = std::abs(m_cachedSnapshot.features.flux);
        m_fluxPeak = std::max(kFluxPeakFloor,
                              std::max(v, m_fluxPeak * kFluxPeakDecay));
        m_fluxDisplay = std::clamp(v / m_fluxPeak, 0.0, 1.0);
    }

    // 8. RMS — (rmsDb + 96) / 96.
    {
        const double db = m_cachedSnapshot.features.rmsDb;
        m_rmsDisplay = std::clamp((db + 96.0) / 96.0, 0.0, 1.0);
    }

    // 9. TSS scalars — mean of per-bin transient / steady cvec norms.
    {
        const int n = m_cachedSnapshot.tss.binCount;
        if (n > 0)
        {
            double tsum = 0.0;
            double ssum = 0.0;
            for (int i = 0; i < n; ++i)
            {
                tsum += m_cachedSnapshot.tss.transientNorm[i];
                ssum += m_cachedSnapshot.tss.steadyNorm[i];
            }
            m_tssTransientLevel = std::clamp(tsum / double(n), 0.0, 1.0);
            m_tssSteadyLevel    = std::clamp(ssum / double(n), 0.0, 1.0);
        }
        else
        {
            m_tssTransientLevel = 0.0;
            m_tssSteadyLevel    = 0.0;
        }
    }

    emit audioSnapshotChanged();
}

void VCAudioTriggers::selectBarForEditing(int index)
{
    setSelectedBar(index);
}

QVariantList VCAudioTriggers::barsInfo() const
{
    QVariantList bList;

    for (int idx = 0; idx < m_bandMappings.count(); idx++)
    {
        const BandMapping &bm = m_bandMappings[idx];
        QVariantMap barMap;

        barMap.insert("bLabel", bandSourceLabel(bm.source));
        barMap.insert("source", int(bm.source));
        barMap.insert("sourceKey", bandSourceToKey(bm.source));
        barMap.insert("color", bandSourceColor(bm.source));
        barMap.insert("index", idx);
        barMap.insert("type", bm.type);
        barMap.insert("dmxScale", bm.dmxScale);
        barMap.insert("dmxFloor", int(bm.dmxFloor));
        barMap.insert("beatHoldMs", bm.beatHoldMs);

        if (bm.type == VCAudioTriggers::BarType::DMXBar)
        {
            barMap.insert("intVal", bm.dmxChannels.count());
        }
        else if (bm.type == VCAudioTriggers::BarType::FunctionBar)
        {
            barMap.insert("intVal", bm.functionId == Function::invalidId() ? -1 : int(bm.functionId));
        }
        else if (bm.type == VCAudioTriggers::BarType::VCWidgetBar)
        {
            barMap.insert("intVal", bm.widgetId == VCWidget::invalidId() ? -1 : int(bm.widgetId));
            VCWidget *widget = m_vc ? m_vc->widget(bm.widgetId) : nullptr;
            barMap.insert("strVal", widget ? widget->caption() : tr("No widget assigned"));
            barMap.insert("iconVal", widget ? VCWidget::typeToIcon(widget->type()) : QString());
        }
        else
        {
            barMap.insert("intVal", 0);
        }

        // Legacy compat for MCP / older QML — global trigger thresholds are
        // used now, expose stable defaults so consumers don't break.
        barMap.insert("minThreshold", 0);
        barMap.insert("maxThreshold", 100);

        bList.append(barMap);
    }

    return bList;
}

void VCAudioTriggers::setBarType(BarType type)
{
    if (m_selectedBar < 0 || m_selectedBar >= m_bandMappings.count())
        return;

    QMutexLocker locker(&m_mappingsMutex);
    // reset everything in any case
    BandMapping &bm = m_bandMappings[m_selectedBar];
    bm.absDmxChannels.clear();
    bm.dmxChannels.clear();
    bm.dmxScale = 1.0;
    bm.dmxFloor = 0;
    bm.beatHoldMs = 80;
    bm.functionId = Function::invalidId();
    bm.function = nullptr;
    bm.widgetId = VCWidget::invalidId();
    bm.widget = nullptr;
    bm.tapped = false;
    bm.skippedBeats = 0;

    bm.type = type;

    // Re-evaluate DMX source registration
    if (m_captureEnabled)
    {
        bool hasDmx = false;
        for (const BandMapping &m : m_bandMappings)
            if (m.type == BarType::DMXBar) { hasDmx = true; break; }
        if (hasDmx)
            m_doc->masterTimer()->registerDMXSource(this);
        else
            m_doc->masterTimer()->unregisterDMXSource(this);
    }

    emit barsInfoChanged();
}

void VCAudioTriggers::setBarThresholds(uchar minThr, uchar maxThr)
{
    // No-op: per-mapping thresholds removed. Use global trigger settings
    // (triggerHigh / triggerLow / triggerHold / triggerCooldown).
    Q_UNUSED(minThr)
    Q_UNUSED(maxThr)
}

void VCAudioTriggers::setBarFunction(quint32 functionId)
{
    if (m_selectedBar < 0 || m_selectedBar >= m_bandMappings.count())
        return;

    BandMapping &bm = m_bandMappings[m_selectedBar];
    bm.functionId = functionId;
    bm.function = (functionId != Function::invalidId() && m_doc)
                      ? m_doc->function(functionId)
                      : nullptr;
    emit barsInfoChanged();
}

void VCAudioTriggers::setBarWidget(quint32 widgetId)
{
    if (m_selectedBar < 0 || m_selectedBar >= m_bandMappings.count())
        return;

    BandMapping &bm = m_bandMappings[m_selectedBar];
    bm.widgetId = widgetId;
    bm.tapped = false;
    bm.skippedBeats = 0;
    updateBarWidgetReference(bm);
    emit barsInfoChanged();
}

void VCAudioTriggers::setBarDmxChannels(QList<SceneValue> list)
{
    if (m_selectedBar < 0 || m_selectedBar >= m_bandMappings.count())
        return;

    QMutexLocker locker(&m_mappingsMutex);
    BandMapping &bm = m_bandMappings[m_selectedBar];
    bm.dmxChannels = list;
    rebuildBarAbsDmxChannels(bm);
    locker.unlock();
    emit barsInfoChanged();
}

void VCAudioTriggers::setBarDmxScale(double scale)
{
    if (m_selectedBar < 0 || m_selectedBar >= m_bandMappings.count())
        return;
    QMutexLocker locker(&m_mappingsMutex);
    m_bandMappings[m_selectedBar].dmxScale = qBound(0.0, scale, 16.0);
    locker.unlock();
    emit barsInfoChanged();
}

void VCAudioTriggers::setBarDmxFloor(int floor0to255)
{
    if (m_selectedBar < 0 || m_selectedBar >= m_bandMappings.count())
        return;
    QMutexLocker locker(&m_mappingsMutex);
    m_bandMappings[m_selectedBar].dmxFloor = uchar(qBound(0, floor0to255, 255));
    locker.unlock();
    emit barsInfoChanged();
}

void VCAudioTriggers::setBarBeatHoldMs(int ms)
{
    if (m_selectedBar < 0 || m_selectedBar >= m_bandMappings.count())
        return;
    QMutexLocker locker(&m_mappingsMutex);
    m_bandMappings[m_selectedBar].beatHoldMs = qMax(0, ms);
    locker.unlock();
    emit barsInfoChanged();
}

void VCAudioTriggers::rebuildBarAbsDmxChannels(BandMapping &bm) const
{
    bm.absDmxChannels.clear();

    for (const SceneValue &scv : bm.dmxChannels)
    {
        if (Fixture *fx = m_doc->fixture(scv.fxi))
        {
            const quint32 absAddr = fx->universeAddress() + scv.channel;
            bm.absDmxChannels.append(int(absAddr));
        }
    }
}

void VCAudioTriggers::updateBarWidgetReference(BandMapping &bm) const
{
    if (bm.widgetId == VCWidget::invalidId())
    {
        bm.widget = nullptr;
        return;
    }

    bm.widget = m_vc ? m_vc->widget(bm.widgetId) : nullptr;
}

void VCAudioTriggers::checkWidgetFunctionality(BandMapping &bm, const TriggerState &ts) const
{
    if (bm.widgetId == VCWidget::invalidId())
        return;

    updateBarWidgetReference(bm);
    VCWidget *widget = bm.widget;
    if (widget == nullptr)
        return;

    switch (widget->type())
    {
        case VCWidget::ButtonWidget:
        {
            VCButton *button = qobject_cast<VCButton *>(widget);
            if (button == nullptr)
                return;

            if (ts.firedThisFrame && button->state() == VCButton::Inactive)
                button->requestStateChange(true);
            else if (ts.releasedThisFrame && button->state() != VCButton::Inactive)
                button->requestStateChange(false);
        }
        break;
        case VCWidget::SliderWidget:
        {
            VCSlider *slider = qobject_cast<VCSlider *>(widget);
            if (slider != nullptr)
                slider->setValue(bm.m_value, true, true);
        }
        break;
        case VCWidget::SpeedWidget:
        {
            VCSpeedDial *speedDial = qobject_cast<VCSpeedDial *>(widget);
            if (speedDial == nullptr)
                return;

            int divisor = qMax(1, bm.divisor);
            if (ts.firedThisFrame && !bm.tapped)
            {
                if (bm.skippedBeats == 0)
                    speedDial->tap();

                bm.tapped = true;
                bm.skippedBeats = (bm.skippedBeats + 1) % divisor;
            }
            else if (ts.releasedThisFrame)
            {
                bm.tapped = false;
            }
        }
        break;
        case VCWidget::CueListWidget:
        {
            VCCueList *cueList = qobject_cast<VCCueList *>(widget);
            if (cueList == nullptr)
                return;

            int divisor = qMax(1, bm.divisor);
            if (ts.firedThisFrame && !bm.tapped)
            {
                if (bm.skippedBeats == 0)
                    cueList->nextClicked();

                bm.tapped = true;
                bm.skippedBeats = (bm.skippedBeats + 1) % divisor;
            }
            else if (ts.releasedThisFrame)
            {
                bm.tapped = false;
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
    Q_UNUSED(power)

    // Rate-limit QML visual updates to ~25Hz (every 3rd hop at ~86Hz).
    // DMX/function/widget trigger processing still runs at the full hop rate.
    const bool updateVisuals = (++m_visualFrameCounter >= 3); // ~28 Hz visual updates
    if (updateVisuals) m_visualFrameCounter = 0;

    // Pull the latest snapshot (built on the capture thread by the analyzer
    // immediately before this signal was emitted). m_cachedSnapshot is always
    // refreshed; the QVariantList caches, timeline append, and
    // audioSnapshotChanged emit are gated by updateVisuals to keep QML smooth.
    updateAudioProfileSnapshotPowers(updateVisuals);

        // Three mel banks map straight onto the snapshot's triggers[0..2]; volume
    // and beat live in their own trigger states; kick has its own state too.
    auto sourceNorm = [&](BandSource s) -> double {
        switch (s)
        {
            case BandLow:    return m_cachedSnapshot.triggers[0].value;
            case BandMid:    return m_cachedSnapshot.triggers[1].value;
            case BandHigh:   return m_cachedSnapshot.triggers[2].value;
            case BandVolume: return m_cachedSnapshot.volume.normalized;
            case BandBeat:   return m_cachedSnapshot.music.beat ? 1.0 : 0.0;
            case BandKick:   return m_cachedSnapshot.kickTrigger.value;
            default: return 0.0;
        }
    };

    auto trigState = [&](BandSource s) -> const TriggerState & {
        switch (s)
        {
            case BandLow:                     return m_cachedSnapshot.triggers[0];
            case BandMid:                     return m_cachedSnapshot.triggers[1];
            case BandHigh:                    return m_cachedSnapshot.triggers[2];
            case BandVolume:                  return m_cachedSnapshot.volumeTrigger;
            case BandKick:                    return m_cachedSnapshot.kickTrigger;
            default:                          return m_cachedSnapshot.beatTrigger;
        }
    };

    m_audioLevels.clear();
    m_audioLevels.reserve(BandSourceCount);

    // If a beat fired this frame, latch the strobe-hold deadline so writeDMX
    // can drive Beat-source DMX channels for `beatHoldMs` ms (per-mapping).
    qint64 beatLatchUntil = 0;
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();

    {
    QMutexLocker locker(&m_mappingsMutex);
    for (int idx = 0; idx < BandSourceCount; idx++)
    {
        BandMapping &bm = m_bandMappings[idx];
        bm.lastNorm = sourceNorm(bm.source);
        bm.m_value = uchar(qBound(0.0, double(bm.lastNorm) * 255.0, 255.0));
        m_audioLevels.append(int(bm.m_value));

        const TriggerState &ts = trigState(bm.source);

        switch (bm.type)
        {
            case BarType::FunctionBar:
            {
                if (!bm.function && bm.functionId != Function::invalidId())
                    bm.function = m_doc->function(bm.functionId);
                if (bm.function)
                {
                    if (ts.firedThisFrame)
                        bm.function->start(m_doc->masterTimer(), functionParent());
                    else if (ts.releasedThisFrame)
                        bm.function->stop(functionParent());
                }
                break;
            }
            case BarType::VCWidgetBar:
                checkWidgetFunctionality(bm, ts);
                break;
            case BarType::DMXBar:
                if (bm.source == BandBeat && m_cachedSnapshot.music.beat)
                    beatLatchUntil = qMax(beatLatchUntil, nowMs + bm.beatHoldMs);
                break;
            case BarType::None:
            default:
                break;
        }
    }
    } // QMutexLocker scope

    if (beatLatchUntil > 0)
        m_beatUntilMs.store(beatLatchUntil, std::memory_order_relaxed);

    // Gate QML signal to visual frame rate — DMX mappings above already ran at full hop rate.
    if (updateVisuals)
        emit audioLevelsChanged();
}
/*********************************************************************
 * Fixture tree methods
 *********************************************************************/

void VCAudioTriggers::updateFixtureTree()
{
    if (m_fixtureTree == nullptr || m_selectedBar < 0 || m_selectedBar >= m_bandMappings.count())
        return;

    m_fixtureTree->clear();
    FixtureManager::updateGroupsTree(m_doc, m_fixtureTree, m_searchFilter,
                                     FixtureManager::ShowCheckBoxes | FixtureManager::ShowGroups | FixtureManager::ShowChannels,
                                     m_bandMappings[m_selectedBar].dmxChannels);
}

QVariant VCAudioTriggers::groupsTreeModel()
{
    if (m_selectedBar < 0 || m_selectedBar >= m_bandMappings.count())
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

void VCAudioTriggers::setSearchFilter(const QString &searchFilter)
{
    if (m_searchFilter == searchFilter)
        return;

    int currLen = m_searchFilter.length();

    m_searchFilter = searchFilter;

    if ((searchFilter.length() >= SEARCH_MIN_CHARS ||
        (currLen >= SEARCH_MIN_CHARS && searchFilter.length() < SEARCH_MIN_CHARS))
        && m_selectedBar >= 0 && m_selectedBar < m_bandMappings.count())
    {
        FixtureManager::updateGroupsTree(m_doc, m_fixtureTree, m_searchFilter,
                                         FixtureManager::ShowCheckBoxes | FixtureManager::ShowGroups | FixtureManager::ShowChannels,
                                         m_bandMappings[m_selectedBar].dmxChannels);
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
    if (tree == nullptr || m_selectedBar < 0 || m_selectedBar >= m_bandMappings.count())
        return;

    BandMapping &bm = m_bandMappings[m_selectedBar];

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
                    if (bm.dmxChannels.contains(scv) == false)
                        bm.dmxChannels.append(scv);
                }
                else
                {
                    bm.dmxChannels.removeAll(scv);
                }
            }
        }

        if (item->hasChildren())
            checkFixtureTree(item->children(), sourceFixture, channelIndex, checked);
    }
}

void VCAudioTriggers::slotTreeDataChanged(TreeModelItem *item, int role, const QVariant &value)
{
    if (m_isUpdating || m_selectedBar < 0 || m_selectedBar >= m_bandMappings.count())
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
    BandMapping &bm = m_bandMappings[m_selectedBar];

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
            if (bm.dmxChannels.contains(scv) == false)
                bm.dmxChannels.append(scv);
        }
        else
        {
            bm.dmxChannels.removeAll(scv);
        }
    }

    rebuildBarAbsDmxChannels(bm);
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
        default:
        break;
    }
}

/*********************************************************************
 * DMXSource
 *********************************************************************/

void VCAudioTriggers::writeDMX(MasterTimer *timer, QList<Universe *> universes)
{
    Q_UNUSED(timer);
    QMutexLocker locker(&m_mappingsMutex);

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 beatUntil = m_beatUntilMs.load(std::memory_order_relaxed);

    quint32 lastUniverse = Universe::invalid();
    QSharedPointer<GenericFader> fader;

    for (BandMapping &bm : m_bandMappings)
    {
        if (bm.type != VCAudioTriggers::BarType::DMXBar || bm.absDmxChannels.isEmpty())
            continue;

        // Compute drive level: scaled lastNorm, except Beat sources which
        // strobe to full while the per-mapping hold window is active.
        double norm = double(bm.lastNorm) * bm.dmxScale;
        if (bm.source == BandBeat)
            norm = (now <= beatUntil) ? 1.0 : 0.0;

        uchar dmx = uchar(qBound(0.0, norm * 255.0, 255.0));
        if (dmx < bm.dmxFloor)
            dmx = bm.dmxFloor;

        for (int absAddress : bm.absDmxChannels)
        {
            quint32 universe = quint32(absAddress) >> 9;
            const int universeIdx = int(universe);
            if (universeIdx < 0 || universeIdx >= universes.size() ||
                universes[universeIdx] == nullptr)
            {
                // Bad mapping (saved against a universe that no longer
                // exists). Throttled debug log — not a warning, since this
                // can happen routinely on workspace reload before the user
                // re-patches outputs.
                static QSet<quint32> warned;
                if (!warned.contains(universe))
                {
                    warned.insert(universe);
                    qDebug() << "VCAudioTriggers: skipping mapping for missing universe"
                             << universe;
                }
                continue;
            }
            if (universe != lastUniverse)
            {
                fader = m_fadersMap.value(universe, QSharedPointer<GenericFader>());
                if (fader == nullptr)
                {
                    fader = universes[universeIdx]->requestFader();
                    fader->adjustIntensity(intensity());
                    m_fadersMap[universe] = fader;
                }
                fader->setEnabled(m_captureEnabled);
                lastUniverse = universe;
            }

            FadeChannel *fc = fader->getChannelFader(m_doc, universes[universeIdx], Fixture::invalidId(), absAddress);
            fc->setStart(fc->current());
            fc->setTarget(dmx);
            fc->setReady(false);
            fc->setElapsed(0);
        }
    }
}

/*********************************************************************
 * Load & Save
 *********************************************************************/

bool VCAudioTriggers::loadBarXML(QXmlStreamReader &root)
{
    QXmlStreamAttributes attrs = root.attributes();

    if (!attrs.hasAttribute(KXMLQLCAudioMappingSource))
        return false;

    bool keyOk = false;
    int srcInt = bandSourceFromKey(attrs.value(KXMLQLCAudioMappingSource).toString(), &keyOk);
    if (!keyOk || srcInt < 0 || srcInt >= BandSourceCount)
    {
        qWarning() << Q_FUNC_INFO << "Unknown band source:" << attrs.value(KXMLQLCAudioMappingSource).toString();
        return false;
    }

    BandMapping &bm = m_bandMappings[srcInt];
    bm.source = BandSource(srcInt);
    bm.type = BarType(attrs.value(KXMLQLCAudioBarType).toString().toInt());

    if (attrs.hasAttribute(KXMLQLCAudioMappingDmxScale))
        bm.dmxScale = attrs.value(KXMLQLCAudioMappingDmxScale).toString().toDouble();
    if (attrs.hasAttribute(KXMLQLCAudioMappingDmxFloor))
        bm.dmxFloor = uchar(qBound(0, attrs.value(KXMLQLCAudioMappingDmxFloor).toString().toInt(), 255));
    if (attrs.hasAttribute(KXMLQLCAudioMappingBeatHold))
        bm.beatHoldMs = qMax(0, attrs.value(KXMLQLCAudioMappingBeatHold).toString().toInt());
    if (attrs.hasAttribute(KXMLQLCAudioBarDivisor))
        bm.divisor = qMax(1, attrs.value(KXMLQLCAudioBarDivisor).toString().toInt());

    switch (bm.type)
    {
        case VCAudioTriggers::BarType::FunctionBar:
        {
            if (attrs.hasAttribute(KXMLQLCAudioBarFunction))
            {
                bm.functionId = attrs.value(KXMLQLCAudioBarFunction).toUInt();
                Function *func = m_doc->function(bm.functionId);
                if (func != nullptr)
                    bm.function = func;
            }
        }
        break;
        case VCAudioTriggers::BarType::VCWidgetBar:
        {
            if (attrs.hasAttribute(KXMLQLCAudioBarWidget))
            {
                quint32 wid = attrs.value(KXMLQLCAudioBarWidget).toString().toUInt();
                bm.widgetId = wid;
                bm.widget = nullptr;
                bm.tapped = false;
                bm.skippedBeats = 0;
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
                if (!dmxValues.isEmpty())
                {
                    QList<SceneValue> channels;
                    QStringList varray = dmxValues.split(",");
                    for (int i = 0; i + 1 < varray.count(); i += 2)
                    {
                        channels.append(SceneValue(QString(varray.at(i)).toUInt(),
                                                   QString(varray.at(i + 1)).toUInt(), 0));
                    }
                    selectBarForEditing(srcInt);
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
    Q_ASSERT(doc != nullptr);

    if (index < 0 || index >= m_bandMappings.count())
    {
        qDebug() << "Audio Triggers mapping index out of bounds!" << index;
        return false;
    }

    const BandMapping &bm = m_bandMappings[index];

    doc->writeStartElement(KXMLQLCAudioMapping);
    doc->writeAttribute(KXMLQLCAudioMappingSource, bandSourceToKey(bm.source));
    doc->writeAttribute(KXMLQLCAudioBarType, QString::number(int(bm.type)));
    doc->writeAttribute(KXMLQLCAudioMappingDmxScale, QString::number(bm.dmxScale));
    doc->writeAttribute(KXMLQLCAudioMappingDmxFloor, QString::number(int(bm.dmxFloor)));
    doc->writeAttribute(KXMLQLCAudioMappingBeatHold, QString::number(bm.beatHoldMs));
    if (bm.divisor != 1)
        doc->writeAttribute(KXMLQLCAudioBarDivisor, QString::number(bm.divisor));

    if (bm.type == VCAudioTriggers::BarType::DMXBar && !bm.dmxChannels.isEmpty())
    {
        QString chans;
        for (const SceneValue &scv : bm.dmxChannels)
        {
            if (!chans.isEmpty())
                chans.append(",");
            chans.append(QString("%1,%2").arg(scv.fxi).arg(scv.channel));
        }
        if (!chans.isEmpty())
            doc->writeTextElement(KXMLQLCAudioBarDMXChannels, chans);
    }
    else if (bm.type == VCAudioTriggers::BarType::FunctionBar && bm.functionId != Function::invalidId())
    {
        doc->writeAttribute(KXMLQLCAudioBarFunction, QString::number(bm.functionId));
    }
    else if (bm.type == VCAudioTriggers::BarType::VCWidgetBar && bm.widgetId != VCWidget::invalidId())
    {
        doc->writeAttribute(KXMLQLCAudioBarWidget, QString::number(bm.widgetId));
    }

    /* End <Mapping> tag */
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

    // Legacy BarsNumber attribute is ignored — count is fixed at BandSourceCount.

    /* Widget commons */
    loadXMLCommon(root);

    bool legacyBarsSeen = false;

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
        else if (root.name() == KXMLQLCAudioMapping)
        {
            loadBarXML(root);
            root.skipCurrentElement();
        }
        else if (root.name() == KXMLQLCAudioTriggerBar ||
                 root.name() == KXMLQLCVolumeBar ||
                 root.name() == KXMLQLCSpectrumBar)
        {
            // Legacy spectrum/volume bar mappings — clean start, drop them.
            legacyBarsSeen = true;
            root.skipCurrentElement();
        }
        else
        {
            qWarning() << Q_FUNC_INFO << "Unknown audio trigger tag:" << root.name().toString();
            root.skipCurrentElement();
        }
    }

    if (legacyBarsSeen)
        qWarning() << "Legacy spectrum bar mappings dropped. Please reconfigure audio source mappings.";

    return true;
}

bool VCAudioTriggers::saveXML(QXmlStreamWriter *doc) const
{
    Q_ASSERT(doc != nullptr);

    /* VC object entry */
    doc->writeStartElement(KXMLQLCVCAudioTriggers);
    doc->writeAttribute(KXMLQLCAudioTriggerAudioProfileID, QString::number(m_audioProfileId));

    saveXMLCommon(doc);

    /* Window state */
    saveXMLWindowState(doc);

    /* Appearance */
    saveXMLAppearance(doc);

    /* External control */
    saveXMLInputControl(doc, INPUT_ENABLE_CAPTURE);
    saveXMLInputControl(doc, INPUT_VOLUME_CONTROL);

    /* Save only configured mappings */
    for (int i = 0; i < m_bandMappings.count(); i++)
    {
        if (m_bandMappings[i].type != VCAudioTriggers::BarType::None)
            saveBarXML(doc, i);
    }

    /* Write the <end> tag */
    doc->writeEndElement();

    return true;
}
