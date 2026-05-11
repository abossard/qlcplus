/*
  Q Light Controller Plus
  audioprofile.cpp

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

#include "audioprofile.h"

#include "audioanalyzer.h"
#include "audiochannel.h"
#include "aubioresults.h"
#include "qlcfile.h"

#include <QDebug>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <algorithm>
#include <cmath>

namespace
{
    bool boolFromString(const QString &value, bool defaultValue = false)
    {
        if (value.compare(KXMLQLCTrue, Qt::CaseInsensitive) == 0 ||
            value.compare(QStringLiteral("1")) == 0)
        {
            return true;
        }
        if (value.compare(KXMLQLCFalse, Qt::CaseInsensitive) == 0 ||
            value.compare(QStringLiteral("0")) == 0)
        {
            return false;
        }
        return defaultValue;
    }

    double doubleAttribute(const QXmlStreamAttributes &attrs,
                           const QString &name,
                           double defaultValue)
    {
        if (attrs.hasAttribute(name) == false)
            return defaultValue;

        bool ok = false;
        const double value = attrs.value(name).toString().toDouble(&ok);
        return ok ? value : defaultValue;
    }

    int intAttribute(const QXmlStreamAttributes &attrs,
                     const QString &name,
                     int defaultValue)
    {
        if (attrs.hasAttribute(name) == false)
            return defaultValue;

        bool ok = false;
        const int value = attrs.value(name).toString().toInt(&ok);
        return ok ? value : defaultValue;
    }

    QString stringAttribute(const QXmlStreamAttributes &attrs,
                            const QString &name,
                            const QString &defaultValue)
    {
        if (attrs.hasAttribute(name) == false)
            return defaultValue;
        return attrs.value(name).toString();
    }
}

AudioProfile::AudioProfile(quint32 id, QObject *parent)
    : QObject(parent)
    , m_id(id)
    , m_config(AudioChannelConfig::defaults())
{
}

AudioProfile::~AudioProfile()
{
    releaseAnalyzer();
}

quint32 AudioProfile::id() const
{
    return m_id;
}

QString AudioProfile::name() const
{
    return m_name;
}

void AudioProfile::setName(const QString &name)
{
    if (m_name == name)
        return;

    m_name = name;
    emit nameChanged();
}

bool AudioProfile::isDefault() const
{
    return m_isDefault;
}

void AudioProfile::setIsDefault(bool def)
{
    if (m_isDefault == def)
        return;

    m_isDefault = def;
    emit isDefaultChanged();
}

AudioChannelConfig AudioProfile::channelConfig() const
{
    return m_config;
}

void AudioProfile::setChannelConfig(const AudioChannelConfig &config)
{
    m_config = config;
    if (m_channel != nullptr)
        m_channel->updateConfig(m_config);
    emit configChanged();
}

void AudioProfile::bindAnalyzer(AudioAnalyzer *analyzer)
{
    if (m_analyzer == analyzer)
        return;

    releaseAnalyzer();
    m_analyzer = analyzer;
    if (m_analyzer != nullptr)
        m_channel = m_analyzer->createChannel(m_config);
}

void AudioProfile::releaseAnalyzer()
{
    if (m_analyzer != nullptr && m_channel != nullptr)
        m_analyzer->destroyChannel(m_channel);

    m_channel = nullptr;
    m_analyzer = nullptr;
}

AudioChannel* AudioProfile::channel() const
{
    return m_channel;
}

bool AudioProfile::loadXML(QXmlStreamReader &root)
{
    if (root.name() != KXMLQLCAudioProfile)
    {
        qWarning() << Q_FUNC_INFO << "AudioProfile node not found";
        return false;
    }

    const QXmlStreamAttributes attrs = root.attributes();

    bool ok = false;
    const quint32 id = attrs.value(KXMLQLCAudioProfileID).toString().toUInt(&ok);
    if (ok == false || id == invalidId())
    {
        qWarning() << Q_FUNC_INFO << "Invalid AudioProfile ID:"
                   << attrs.value(KXMLQLCAudioProfileID).toString();
        root.skipCurrentElement();
        return false;
    }
    m_id = id;

    if (attrs.hasAttribute(KXMLQLCAudioProfileName))
        setName(attrs.value(KXMLQLCAudioProfileName).toString());

    if (attrs.hasAttribute(KXMLQLCAudioProfileIsDefault))
        setIsDefault(boolFromString(attrs.value(KXMLQLCAudioProfileIsDefault).toString()));

    constexpr int kSupportedVersion = 2;
    int version = 0;
    if (attrs.hasAttribute(KXMLQLCAudioProfileVersion))
    {
        bool versionOk = false;
        const int parsed = attrs.value(KXMLQLCAudioProfileVersion).toString().toInt(&versionOk);
        if (!versionOk || parsed < kSupportedVersion)
        {
            qWarning() << "AudioProfile" << m_name
                       << "has unsupported Version"
                       << attrs.value(KXMLQLCAudioProfileVersion).toString()
                       << "- minimum supported is" << kSupportedVersion
                       << ". Recreate the profile from current defaults.";
            root.skipCurrentElement();
            return false;
        }
        version = parsed;
        if (version > kSupportedVersion)
        {
            qWarning() << "AudioProfile" << m_name << "has version" << version
                       << "which is newer than supported (" << kSupportedVersion
                       << "). Some settings may be lost.";
        }
    }
    else
    {
        // No backward compat: pre-Version-2 profiles are dead.
        qWarning() << "AudioProfile" << m_name
                   << "is missing the Version attribute (legacy v1 or older)."
                   << " Recreate from current defaults.";
        root.skipCurrentElement();
        return false;
    }
    Q_UNUSED(version)

    AudioChannelConfig config = m_config;

    while (root.readNextStartElement())
    {
        const QXmlStreamAttributes childAttrs = root.attributes();

        if (root.name() == KXMLQLCAudioProfileEnvelope)
        {
            config.envelope.attackMs = doubleAttribute(childAttrs,
                                                       KXMLQLCAudioProfileEnvelopeAttack,
                                                       config.envelope.attackMs);
            config.envelope.releaseMs = doubleAttribute(childAttrs,
                                                        KXMLQLCAudioProfileEnvelopeRelease,
                                                        config.envelope.releaseMs);
            root.skipCurrentElement();
        }
        else if (root.name() == KXMLQLCAudioProfileAgc)
        {
            // Legacy AGC element: input gain is now an OS/hardware concern and
            // is no longer part of the audio profile. Skip the element.
            root.skipCurrentElement();
        }
        else if (root.name() == KXMLQLCAudioProfileTriggers)
        {
            config.triggers.highThreshold = doubleAttribute(childAttrs,
                                                            KXMLQLCAudioProfileTriggersHigh,
                                                            config.triggers.highThreshold);
            config.triggers.lowThreshold = doubleAttribute(childAttrs,
                                                           KXMLQLCAudioProfileTriggersLow,
                                                           config.triggers.lowThreshold);
            config.triggers.holdMs = doubleAttribute(childAttrs,
                                                     KXMLQLCAudioProfileTriggersHold,
                                                     config.triggers.holdMs);
            config.triggers.cooldownMs = doubleAttribute(childAttrs,
                                                         KXMLQLCAudioProfileTriggersCooldown,
                                                         config.triggers.cooldownMs);
            root.skipCurrentElement();
        }
        else if (root.name() == KXMLQLCAudioProfileBands)
        {
            // Legacy 5-perceptual-band layout has been removed. The element
            // is silently skipped to avoid blocking older profile loads.
            root.skipCurrentElement();
        }
        else if (root.name() == KXMLQLCAudioProfileNoiseGate)
        {
            config.noiseGate.thresholdDb = doubleAttribute(childAttrs,
                                                           KXMLQLCAudioProfileNoiseGateThreshold,
                                                           config.noiseGate.thresholdDb);
            config.noiseGate.holdMs = doubleAttribute(childAttrs,
                                                      KXMLQLCAudioProfileNoiseGateHold,
                                                      config.noiseGate.holdMs);
            root.skipCurrentElement();
        }
        else if (root.name() == KXMLQLCAudioProfileKick)
        {
            config.kick.enabled = intAttribute(childAttrs,
                                                KXMLQLCAudioProfileKickEnabled,
                                                config.kick.enabled ? 1 : 0) != 0;
            config.kick.beatMaxHz = doubleAttribute(childAttrs,
                                                     KXMLQLCAudioProfileKickBeatMaxHz,
                                                     config.kick.beatMaxHz);
            config.kick.beatMinPercentDiff = doubleAttribute(childAttrs,
                                                              KXMLQLCAudioProfileKickBeatMinPercentDiff,
                                                              config.kick.beatMinPercentDiff);
            config.kick.beatMinAmplitude = doubleAttribute(childAttrs,
                                                            KXMLQLCAudioProfileKickBeatMinAmplitude,
                                                            config.kick.beatMinAmplitude);
            config.kick.beatRefractorySec = doubleAttribute(childAttrs,
                                                             KXMLQLCAudioProfileKickBeatRefractorySec,
                                                             config.kick.beatRefractorySec);
            config.kick.beatHistoryLen = intAttribute(childAttrs,
                                                       KXMLQLCAudioProfileKickBeatHistoryLen,
                                                       config.kick.beatHistoryLen);
            // Legacy QLC+ Schmitt-detector fields (SpikeThreshold/ReleaseFactor/
            // Cooldown/Hold/MelBandStart/MelBandEnd) were removed in the LedFx
            // port. We silently drop them on load.
            root.skipCurrentElement();
        }
        else if (root.name() == KXMLQLCAudioProfileMelPost)
        {
            // Master 40-band post-processor (snap.melProcessed[] / flatness).
            config.melPost.enabled = intAttribute(childAttrs,
                                                  KXMLQLCAudioProfileMelPostEnabled,
                                                  config.melPost.enabled ? 1 : 0) != 0;
            config.melPost.powerFactor   = doubleAttribute(childAttrs, KXMLQLCAudioProfileMelBankPowerFactor,   config.melPost.powerFactor);
            config.melPost.gaussianSigma = doubleAttribute(childAttrs, KXMLQLCAudioProfileMelBankGaussianSigma, config.melPost.gaussianSigma);
            config.melPost.smoothDecay   = doubleAttribute(childAttrs, KXMLQLCAudioProfileMelBankSmoothDecay,   config.melPost.smoothDecay);
            config.melPost.smoothRise    = doubleAttribute(childAttrs, KXMLQLCAudioProfileMelBankSmoothRise,    config.melPost.smoothRise);
            config.melPost.commonDecay   = doubleAttribute(childAttrs, KXMLQLCAudioProfileMelBankCommonDecay,   config.melPost.commonDecay);
            config.melPost.commonRise    = doubleAttribute(childAttrs, KXMLQLCAudioProfileMelBankCommonRise,    config.melPost.commonRise);
            config.melPost.diffDecay     = doubleAttribute(childAttrs, KXMLQLCAudioProfileMelBankDiffDecay,     config.melPost.diffDecay);
            config.melPost.diffRise      = doubleAttribute(childAttrs, KXMLQLCAudioProfileMelBankDiffRise,      config.melPost.diffRise);
            config.melPost.agcDecay      = doubleAttribute(childAttrs, KXMLQLCAudioProfileMelBankAgcDecay,      config.melPost.agcDecay);
            config.melPost.agcRise       = doubleAttribute(childAttrs, KXMLQLCAudioProfileMelBankAgcRise,       config.melPost.agcRise);
            root.skipCurrentElement();
        }
        else if (root.name() == KXMLQLCAudioProfileFreqPower)
        {
            // <FreqPower>
            //   <Band Name="beat" MaxHz="100" Decay="0.1" Rise="0.99"/>
            //   ...
            // </FreqPower>
            while (root.readNextStartElement())
            {
                if (root.name() == KXMLQLCAudioProfileFreqPowerBand)
                {
                    const QXmlStreamAttributes bAttrs = root.attributes();
                    const QString name = stringAttribute(bAttrs,
                                                         KXMLQLCAudioProfileFreqPowerBandName,
                                                         QString()).toLower();
                    FreqPowerBandConfig *band = nullptr;
                    if      (name == QStringLiteral("beat")) band = &config.freqPower.beat;
                    else if (name == QStringLiteral("bass")) band = &config.freqPower.bass;
                    else if (name == QStringLiteral("mids")) band = &config.freqPower.mids;
                    else if (name == QStringLiteral("high")) band = &config.freqPower.high;
                    if (band != nullptr)
                    {
                        band->maxHz = doubleAttribute(bAttrs, KXMLQLCAudioProfileFreqPowerBandMaxHz, band->maxHz);
                        band->decay = doubleAttribute(bAttrs, KXMLQLCAudioProfileFreqPowerBandDecay, band->decay);
                        band->rise  = doubleAttribute(bAttrs, KXMLQLCAudioProfileFreqPowerBandRise,  band->rise);
                    }
                }
                root.skipCurrentElement();
            }
        }
        else if (root.name() == KXMLQLCAudioProfileVolume)
        {
            config.volumeSmoothingMs = doubleAttribute(childAttrs,
                                                       KXMLQLCAudioProfileVolumeSmoothing,
                                                       config.volumeSmoothingMs);
            config.brightnessFloor = doubleAttribute(childAttrs,
                                                     KXMLQLCAudioProfileVolumeBrightnessFloor,
                                                     config.brightnessFloor);
            root.skipCurrentElement();
        }
        else if (root.name() == KXMLQLCAudioProfileAubio)
        {
            // OnsetThreshold / OnsetMinInterval are silently ignored on load
            // (legacy attributes — global onset overrides have been removed
            // so aubio's per-method tuned defaults stay authoritative).
            config.aubio.pitchMethod = stringAttribute(childAttrs,
                                                       KXMLQLCAudioProfileAubioPitchMethod,
                                                       config.aubio.pitchMethod);
            // Validate pitchMethod against aubio's accepted set; fall back
            // to the default ("yinfft") if the stored XML is corrupt.
            {
                static const QStringList kPitchMethods = {
                    QStringLiteral("yinfft"), QStringLiteral("yin"),
                    QStringLiteral("yinfast"), QStringLiteral("schmitt"),
                    QStringLiteral("fcomb"), QStringLiteral("mcomb"),
                    QStringLiteral("specacf"), QStringLiteral("default")
                };
                if (!kPitchMethods.contains(config.aubio.pitchMethod))
                    config.aubio.pitchMethod = QStringLiteral("yinfft");
            }
            config.aubio.pitchSilenceDb = doubleAttribute(childAttrs,
                                                          KXMLQLCAudioProfileAubioPitchSilenceDb,
                                                          config.aubio.pitchSilenceDb);
            config.aubio.pitchTolerance = doubleAttribute(childAttrs,
                                                          KXMLQLCAudioProfileAubioPitchTolerance,
                                                          config.aubio.pitchTolerance);
            config.aubio.filterbankNorm = doubleAttribute(childAttrs,
                                                          KXMLQLCAudioProfileAubioFilterbankNorm,
                                                          config.aubio.filterbankNorm);
            config.aubio.filterbankPower = doubleAttribute(childAttrs,
                                                           KXMLQLCAudioProfileAubioFilterbankPower,
                                                           config.aubio.filterbankPower);
            config.aubio.tempoSilenceDb = doubleAttribute(childAttrs,
                                                          KXMLQLCAudioProfileAubioTempoSilenceDb,
                                                          config.aubio.tempoSilenceDb);
            config.aubio.tempoThreshold = doubleAttribute(childAttrs,
                                                          KXMLQLCAudioProfileAubioTempoThreshold,
                                                          config.aubio.tempoThreshold);
            config.aubio.tatumSubdivision = intAttribute(childAttrs,
                                                         KXMLQLCAudioProfileAubioTatumSubdivision,
                                                         config.aubio.tatumSubdivision);
            config.aubio.beatsPerBar = std::clamp(intAttribute(childAttrs,
                                                               KXMLQLCAudioProfileAubioBeatsPerBar,
                                                               config.aubio.beatsPerBar),
                                                  1, 8);
            config.aubio.preEmphasisEnabled = intAttribute(childAttrs,
                                                           KXMLQLCAudioProfileAubioPreEmphasisEnabled,
                                                           config.aubio.preEmphasisEnabled ? 1 : 0) != 0;
            config.aubio.tssAlpha = doubleAttribute(childAttrs,
                                                    KXMLQLCAudioProfileAubioTssAlpha,
                                                    config.aubio.tssAlpha);
            config.aubio.tssBeta = doubleAttribute(childAttrs,
                                                   KXMLQLCAudioProfileAubioTssBeta,
                                                   config.aubio.tssBeta);
            config.aubio.tssThreshold = doubleAttribute(childAttrs,
                                                        KXMLQLCAudioProfileAubioTssThreshold,
                                                        config.aubio.tssThreshold);

            // New aubio params (added in Version 1+; missing attributes use
            // in-class struct defaults for backward compat with older XML).
            config.aubio.windowType = stringAttribute(childAttrs,
                                                      KXMLQLCAudioProfileAubioWindowType,
                                                      config.aubio.windowType);
            {
                static const QStringList kWindowTypes = {
                    QStringLiteral("default"),    QStringLiteral("rectangle"),
                    QStringLiteral("hamming"),    QStringLiteral("hanning"),
                    QStringLiteral("hanningz"),   QStringLiteral("blackman"),
                    QStringLiteral("blackman_harris"), QStringLiteral("gaussian"),
                    QStringLiteral("welch"),      QStringLiteral("parzen")
                };
                if (!kWindowTypes.contains(config.aubio.windowType))
                    config.aubio.windowType = QStringLiteral("default");
            }
            config.aubio.melScale = stringAttribute(childAttrs,
                                                    KXMLQLCAudioProfileAubioMelScale,
                                                    config.aubio.melScale);
            if (config.aubio.melScale.compare(QStringLiteral("matt_mel"), Qt::CaseInsensitive) != 0
                && config.aubio.melScale.compare(QStringLiteral("htk"), Qt::CaseInsensitive) != 0
                && config.aubio.melScale.compare(QStringLiteral("slaney"), Qt::CaseInsensitive) != 0)
            {
                config.aubio.melScale = QStringLiteral("matt_mel");
            }
            // Older profile XML may include OnsetAdaptiveWhitening /
            // OnsetCompressionLambda — they are now ignored because aubio's
            // per-method defaults are used instead.
            // Per-method enable bitmask: stored as a 9-character string of
            // '1'/'0' (energy first ... specflux last). Missing or wrong-length
            // attribute -> keep struct defaults (all enabled).
            {
                const QString mask = stringAttribute(childAttrs,
                                                     KXMLQLCAudioProfileAubioOnsetMethodsEnabled,
                                                     QString());
                if (mask.length() == 9)
                {
                    for (int i = 0; i < 9; i++)
                        config.aubio.onsetMethodEnabled[i] = (mask.at(i) != QLatin1Char('0'));
                }
            }
            config.aubio.onsetMethodIndex = std::clamp(intAttribute(childAttrs,
                                                                    KXMLQLCAudioProfileAubioOnsetMethodIndex,
                                                                    config.aubio.onsetMethodIndex),
                                                       0, AUBIO_ONSET_METHODS - 1);
            config.aubio.tempoDelayMs = doubleAttribute(childAttrs,
                                                         KXMLQLCAudioProfileAubioTempoDelayMs,
                                                         config.aubio.tempoDelayMs);
            config.aubio.noteSilenceDb = doubleAttribute(childAttrs,
                                                         KXMLQLCAudioProfileAubioNoteSilenceDb,
                                                         config.aubio.noteSilenceDb);
            config.aubio.noteMinIntervalMs = doubleAttribute(childAttrs,
                                                             KXMLQLCAudioProfileAubioNoteMinIntervalMs,
                                                             config.aubio.noteMinIntervalMs);
            config.aubio.noteReleaseDropDb = doubleAttribute(childAttrs,
                                                             KXMLQLCAudioProfileAubioNoteReleaseDropDb,
                                                             config.aubio.noteReleaseDropDb);
            config.aubio.mfccPower = doubleAttribute(childAttrs,
                                                     KXMLQLCAudioProfileAubioMfccPower,
                                                     config.aubio.mfccPower);
            config.aubio.mfccScale = doubleAttribute(childAttrs,
                                                     KXMLQLCAudioProfileAubioMfccScale,
                                                     config.aubio.mfccScale);

            // Pitch unit — passed straight to aubio_pitch_set_unit. Validate
            // against aubio's accepted values; anything else falls back to Hz.
            config.aubio.pitchUnit = stringAttribute(childAttrs,
                                                     KXMLQLCAudioProfileAubioPitchUnit,
                                                     config.aubio.pitchUnit);
            {
                static const QStringList kPitchUnits = {
                    QStringLiteral("Hz"), QStringLiteral("midi"),
                    QStringLiteral("cent"), QStringLiteral("bin")
                };
                if (!kPitchUnits.contains(config.aubio.pitchUnit))
                    config.aubio.pitchUnit = QStringLiteral("Hz");
            }

            // Per-method onset overrides arrive as <OnsetOverride/> children.
            // Sentinel-encoded fields (see audiochannelconfig.h) stay at "use
            // aubio default" when an attribute is absent, so a partial
            // override is allowed.
            static const QHash<QString, int> kMethodIndex = {
                { QStringLiteral("energy"),   0 }, { QStringLiteral("hfc"),      1 },
                { QStringLiteral("complex"),  2 }, { QStringLiteral("phase"),    3 },
                { QStringLiteral("wphase"),   4 }, { QStringLiteral("specdiff"), 5 },
                { QStringLiteral("kl"),       6 }, { QStringLiteral("mkl"),      7 },
                { QStringLiteral("specflux"), 8 }
            };
            while (root.readNextStartElement())
            {
                if (root.name() == KXMLQLCAudioProfileAubioOnsetOverride)
                {
                    const QXmlStreamAttributes ovAttrs = root.attributes();
                    const QString method = stringAttribute(ovAttrs,
                                                           KXMLQLCAudioProfileAubioOnsetOverrideMethod,
                                                           QString());
                    const int idx = kMethodIndex.value(method, -1);
                    if (idx >= 0 && idx < 9)
                    {
                        OnsetMethodOverride &ov = config.aubio.onsetOverrides[idx];
                        if (ovAttrs.hasAttribute(KXMLQLCAudioProfileAubioOnsetOverrideThreshold))
                            ov.threshold = doubleAttribute(ovAttrs, KXMLQLCAudioProfileAubioOnsetOverrideThreshold, ov.threshold);
                        if (ovAttrs.hasAttribute(KXMLQLCAudioProfileAubioOnsetOverrideSilence))
                            ov.silenceDb = doubleAttribute(ovAttrs, KXMLQLCAudioProfileAubioOnsetOverrideSilence, ov.silenceDb);
                        if (ovAttrs.hasAttribute(KXMLQLCAudioProfileAubioOnsetOverrideMinioi))
                            ov.minioiMs = doubleAttribute(ovAttrs, KXMLQLCAudioProfileAubioOnsetOverrideMinioi, ov.minioiMs);
                        if (ovAttrs.hasAttribute(KXMLQLCAudioProfileAubioOnsetOverrideDelay))
                            ov.delayMs = doubleAttribute(ovAttrs, KXMLQLCAudioProfileAubioOnsetOverrideDelay, ov.delayMs);
                        if (ovAttrs.hasAttribute(KXMLQLCAudioProfileAubioOnsetOverrideCompression))
                            ov.compression = doubleAttribute(ovAttrs, KXMLQLCAudioProfileAubioOnsetOverrideCompression, ov.compression);
                        if (ovAttrs.hasAttribute(KXMLQLCAudioProfileAubioOnsetOverrideAwhitening))
                        {
                            const QString aw = stringAttribute(ovAttrs, KXMLQLCAudioProfileAubioOnsetOverrideAwhitening, QString());
                            ov.awhitening = (aw.compare(KXMLQLCTrue, Qt::CaseInsensitive) == 0
                                             || aw == QStringLiteral("1")) ? 1 : 0;
                        }
                    }
                    root.skipCurrentElement();
                }
                else if (root.name() == KXMLQLCAudioProfileAubioMelBank)
                {
                    // Per-bank multi-mel range. Role selects which of low/mid/high
                    // is being configured; missing or unknown roles are ignored.
                    const QXmlStreamAttributes mbAttrs = root.attributes();
                    const QString role = stringAttribute(mbAttrs,
                                                         KXMLQLCAudioProfileAubioMelBankRole,
                                                         QString()).toLower();
                    MelBankConfig::Bank *bank = nullptr;
                    if (role == QStringLiteral("low"))       bank = &config.aubio.melBanks.low;
                    else if (role == QStringLiteral("mid"))  bank = &config.aubio.melBanks.mid;
                    else if (role == QStringLiteral("high")) bank = &config.aubio.melBanks.high;
                    if (bank != nullptr)
                    {
                        bank->minHz = doubleAttribute(mbAttrs, KXMLQLCAudioProfileAubioMelBankMinHz, bank->minHz);
                        bank->maxHz = doubleAttribute(mbAttrs, KXMLQLCAudioProfileAubioMelBankMaxHz, bank->maxHz);
                        bank->bands = intAttribute(mbAttrs, KXMLQLCAudioProfileAubioMelBankBands, bank->bands);
                        if (bank->bands < 1) bank->bands = 1;
                        if (bank->bands > MelBankConfig::kMaxBandsPerBank)
                            bank->bands = MelBankConfig::kMaxBandsPerBank;

                        // Per-bank MelPostConfig (LedFx melbank.py:374-378).
                        bank->post.powerFactor   = doubleAttribute(mbAttrs, KXMLQLCAudioProfileMelBankPowerFactor,   bank->post.powerFactor);
                        bank->post.gaussianSigma = doubleAttribute(mbAttrs, KXMLQLCAudioProfileMelBankGaussianSigma, bank->post.gaussianSigma);
                        bank->post.smoothDecay   = doubleAttribute(mbAttrs, KXMLQLCAudioProfileMelBankSmoothDecay,   bank->post.smoothDecay);
                        bank->post.smoothRise    = doubleAttribute(mbAttrs, KXMLQLCAudioProfileMelBankSmoothRise,    bank->post.smoothRise);
                        bank->post.commonDecay   = doubleAttribute(mbAttrs, KXMLQLCAudioProfileMelBankCommonDecay,   bank->post.commonDecay);
                        bank->post.commonRise    = doubleAttribute(mbAttrs, KXMLQLCAudioProfileMelBankCommonRise,    bank->post.commonRise);
                        bank->post.diffDecay     = doubleAttribute(mbAttrs, KXMLQLCAudioProfileMelBankDiffDecay,     bank->post.diffDecay);
                        bank->post.diffRise      = doubleAttribute(mbAttrs, KXMLQLCAudioProfileMelBankDiffRise,      bank->post.diffRise);
                        bank->post.agcDecay      = doubleAttribute(mbAttrs, KXMLQLCAudioProfileMelBankAgcDecay,      bank->post.agcDecay);
                        bank->post.agcRise       = doubleAttribute(mbAttrs, KXMLQLCAudioProfileMelBankAgcRise,       bank->post.agcRise);
                    }
                    if (mbAttrs.hasAttribute(KXMLQLCAudioProfileAubioMelBankPreset))
                    {
                        config.aubio.melBanks.preset = stringAttribute(mbAttrs,
                                                                       KXMLQLCAudioProfileAubioMelBankPreset,
                                                                       config.aubio.melBanks.preset);
                    }
                    root.skipCurrentElement();
                }
                else
                {
                    root.skipCurrentElement();
                }
            }
        }
        else
        {
            qWarning() << Q_FUNC_INFO << "Unknown AudioProfile tag:" << root.name();
            root.skipCurrentElement();
        }
    }

    setChannelConfig(config);
    return true;
}

bool AudioProfile::saveXML(QXmlStreamWriter *doc) const
{
    Q_ASSERT(doc != nullptr);

    doc->writeStartElement(KXMLQLCAudioProfile);
    doc->writeAttribute(KXMLQLCAudioProfileID, QString::number(id()));
    doc->writeAttribute(KXMLQLCAudioProfileName, name());
    doc->writeAttribute(KXMLQLCAudioProfileIsDefault, isDefault() ? KXMLQLCTrue : KXMLQLCFalse);
    doc->writeAttribute(KXMLQLCAudioProfileVersion, QStringLiteral("2"));

    doc->writeEmptyElement(KXMLQLCAudioProfileEnvelope);
    doc->writeAttribute(KXMLQLCAudioProfileEnvelopeAttack, QString::number(m_config.envelope.attackMs));
    doc->writeAttribute(KXMLQLCAudioProfileEnvelopeRelease, QString::number(m_config.envelope.releaseMs));

    doc->writeEmptyElement(KXMLQLCAudioProfileTriggers);
    doc->writeAttribute(KXMLQLCAudioProfileTriggersHigh, QString::number(m_config.triggers.highThreshold));
    doc->writeAttribute(KXMLQLCAudioProfileTriggersLow, QString::number(m_config.triggers.lowThreshold));
    doc->writeAttribute(KXMLQLCAudioProfileTriggersHold, QString::number(m_config.triggers.holdMs));
    doc->writeAttribute(KXMLQLCAudioProfileTriggersCooldown, QString::number(m_config.triggers.cooldownMs));

    doc->writeEmptyElement(KXMLQLCAudioProfileNoiseGate);
    doc->writeAttribute(KXMLQLCAudioProfileNoiseGateThreshold, QString::number(m_config.noiseGate.thresholdDb));
    doc->writeAttribute(KXMLQLCAudioProfileNoiseGateHold, QString::number(m_config.noiseGate.holdMs));

    doc->writeEmptyElement(KXMLQLCAudioProfileKick);
    doc->writeAttribute(KXMLQLCAudioProfileKickEnabled, m_config.kick.enabled ? "1" : "0");
    doc->writeAttribute(KXMLQLCAudioProfileKickBeatMaxHz, QString::number(m_config.kick.beatMaxHz));
    doc->writeAttribute(KXMLQLCAudioProfileKickBeatMinPercentDiff, QString::number(m_config.kick.beatMinPercentDiff));
    doc->writeAttribute(KXMLQLCAudioProfileKickBeatMinAmplitude, QString::number(m_config.kick.beatMinAmplitude));
    doc->writeAttribute(KXMLQLCAudioProfileKickBeatRefractorySec, QString::number(m_config.kick.beatRefractorySec));
    doc->writeAttribute(KXMLQLCAudioProfileKickBeatHistoryLen, QString::number(m_config.kick.beatHistoryLen));

    doc->writeEmptyElement(KXMLQLCAudioProfileMelPost);
    doc->writeAttribute(KXMLQLCAudioProfileMelPostEnabled, m_config.melPost.enabled ? "1" : "0");
    doc->writeAttribute(KXMLQLCAudioProfileMelBankPowerFactor,   QString::number(m_config.melPost.powerFactor));
    doc->writeAttribute(KXMLQLCAudioProfileMelBankGaussianSigma, QString::number(m_config.melPost.gaussianSigma));
    doc->writeAttribute(KXMLQLCAudioProfileMelBankSmoothDecay,   QString::number(m_config.melPost.smoothDecay));
    doc->writeAttribute(KXMLQLCAudioProfileMelBankSmoothRise,    QString::number(m_config.melPost.smoothRise));
    doc->writeAttribute(KXMLQLCAudioProfileMelBankCommonDecay,   QString::number(m_config.melPost.commonDecay));
    doc->writeAttribute(KXMLQLCAudioProfileMelBankCommonRise,    QString::number(m_config.melPost.commonRise));
    doc->writeAttribute(KXMLQLCAudioProfileMelBankDiffDecay,     QString::number(m_config.melPost.diffDecay));
    doc->writeAttribute(KXMLQLCAudioProfileMelBankDiffRise,      QString::number(m_config.melPost.diffRise));
    doc->writeAttribute(KXMLQLCAudioProfileMelBankAgcDecay,      QString::number(m_config.melPost.agcDecay));
    doc->writeAttribute(KXMLQLCAudioProfileMelBankAgcRise,       QString::number(m_config.melPost.agcRise));

    // Per-band freq_power: <FreqPower><Band Name="..." MaxHz=".." Decay=".." Rise=".."/>...</FreqPower>
    doc->writeStartElement(KXMLQLCAudioProfileFreqPower);
    {
        struct { const char *name; const FreqPowerBandConfig *band; } bands[] = {
            { "beat", &m_config.freqPower.beat },
            { "bass", &m_config.freqPower.bass },
            { "mids", &m_config.freqPower.mids },
            { "high", &m_config.freqPower.high }
        };
        for (size_t i = 0; i < sizeof(bands) / sizeof(bands[0]); ++i)
        {
            doc->writeEmptyElement(KXMLQLCAudioProfileFreqPowerBand);
            doc->writeAttribute(KXMLQLCAudioProfileFreqPowerBandName,  QString::fromLatin1(bands[i].name));
            doc->writeAttribute(KXMLQLCAudioProfileFreqPowerBandMaxHz, QString::number(bands[i].band->maxHz));
            doc->writeAttribute(KXMLQLCAudioProfileFreqPowerBandDecay, QString::number(bands[i].band->decay));
            doc->writeAttribute(KXMLQLCAudioProfileFreqPowerBandRise,  QString::number(bands[i].band->rise));
        }
    }
    doc->writeEndElement(); // FreqPower

    doc->writeEmptyElement(KXMLQLCAudioProfileVolume);
    doc->writeAttribute(KXMLQLCAudioProfileVolumeSmoothing, QString::number(m_config.volumeSmoothingMs));
    doc->writeAttribute(KXMLQLCAudioProfileVolumeBrightnessFloor, QString::number(m_config.brightnessFloor));

    doc->writeStartElement(KXMLQLCAudioProfileAubio);
    doc->writeAttribute(KXMLQLCAudioProfileAubioPitchMethod, m_config.aubio.pitchMethod);
    doc->writeAttribute(KXMLQLCAudioProfileAubioPitchUnit, m_config.aubio.pitchUnit);
    doc->writeAttribute(KXMLQLCAudioProfileAubioPitchSilenceDb, QString::number(m_config.aubio.pitchSilenceDb));
    doc->writeAttribute(KXMLQLCAudioProfileAubioPitchTolerance, QString::number(m_config.aubio.pitchTolerance));
    doc->writeAttribute(KXMLQLCAudioProfileAubioFilterbankNorm, QString::number(m_config.aubio.filterbankNorm));
    doc->writeAttribute(KXMLQLCAudioProfileAubioFilterbankPower, QString::number(m_config.aubio.filterbankPower));
    doc->writeAttribute(KXMLQLCAudioProfileAubioTempoSilenceDb, QString::number(m_config.aubio.tempoSilenceDb));
    doc->writeAttribute(KXMLQLCAudioProfileAubioTempoThreshold, QString::number(m_config.aubio.tempoThreshold));
    doc->writeAttribute(KXMLQLCAudioProfileAubioTatumSubdivision, QString::number(m_config.aubio.tatumSubdivision));
    doc->writeAttribute(KXMLQLCAudioProfileAubioBeatsPerBar, QString::number(m_config.aubio.beatsPerBar));
    doc->writeAttribute(KXMLQLCAudioProfileAubioPreEmphasisEnabled, m_config.aubio.preEmphasisEnabled ? "1" : "0");
    doc->writeAttribute(KXMLQLCAudioProfileAubioTssAlpha, QString::number(m_config.aubio.tssAlpha));
    doc->writeAttribute(KXMLQLCAudioProfileAubioTssBeta, QString::number(m_config.aubio.tssBeta));
    doc->writeAttribute(KXMLQLCAudioProfileAubioTssThreshold, QString::number(m_config.aubio.tssThreshold));

    doc->writeAttribute(KXMLQLCAudioProfileAubioWindowType, m_config.aubio.windowType);
    doc->writeAttribute(KXMLQLCAudioProfileAubioMelScale, m_config.aubio.melScale);
    {
        QString mask;
        mask.reserve(9);
        for (int i = 0; i < 9; i++)
            mask.append(m_config.aubio.onsetMethodEnabled[i] ? QLatin1Char('1') : QLatin1Char('0'));
        doc->writeAttribute(KXMLQLCAudioProfileAubioOnsetMethodsEnabled, mask);
    }
    doc->writeAttribute(KXMLQLCAudioProfileAubioOnsetMethodIndex,
                        QString::number(std::clamp(m_config.aubio.onsetMethodIndex,
                                                   0, AUBIO_ONSET_METHODS - 1)));
    doc->writeAttribute(KXMLQLCAudioProfileAubioTempoDelayMs, QString::number(m_config.aubio.tempoDelayMs));
    doc->writeAttribute(KXMLQLCAudioProfileAubioNoteSilenceDb, QString::number(m_config.aubio.noteSilenceDb));
    doc->writeAttribute(KXMLQLCAudioProfileAubioNoteMinIntervalMs, QString::number(m_config.aubio.noteMinIntervalMs));
    doc->writeAttribute(KXMLQLCAudioProfileAubioNoteReleaseDropDb, QString::number(m_config.aubio.noteReleaseDropDb));
    doc->writeAttribute(KXMLQLCAudioProfileAubioMfccPower, QString::number(m_config.aubio.mfccPower));
    doc->writeAttribute(KXMLQLCAudioProfileAubioMfccScale, QString::number(m_config.aubio.mfccScale));

    // Per-method onset overrides. Sentinel values (see audiochannelconfig.h)
    // mean "use aubio default" and are NOT serialized — only real overrides
    // produce <OnsetOverride/> child elements, keeping XML diffs minimal.
    static const char *kMethodNames[9] = {
        "energy", "hfc", "complex", "phase", "wphase",
        "specdiff", "kl", "mkl", "specflux"
    };
    for (int i = 0; i < 9; i++)
    {
        const OnsetMethodOverride &ov = m_config.aubio.onsetOverrides[i];
        const bool hasAny =
            ov.threshold >= 0.0 || ov.silenceDb > -900.0 ||
            ov.minioiMs >= 0.0 || ov.delayMs > -9000.0 ||
            ov.compression >= 0.0 || ov.awhitening >= 0;
        if (!hasAny)
            continue;
        doc->writeEmptyElement(KXMLQLCAudioProfileAubioOnsetOverride);
        doc->writeAttribute(KXMLQLCAudioProfileAubioOnsetOverrideMethod, QString::fromLatin1(kMethodNames[i]));
        if (ov.threshold >= 0.0)
            doc->writeAttribute(KXMLQLCAudioProfileAubioOnsetOverrideThreshold, QString::number(ov.threshold));
        if (ov.silenceDb > -900.0)
            doc->writeAttribute(KXMLQLCAudioProfileAubioOnsetOverrideSilence, QString::number(ov.silenceDb));
        if (ov.minioiMs >= 0.0)
            doc->writeAttribute(KXMLQLCAudioProfileAubioOnsetOverrideMinioi, QString::number(ov.minioiMs));
        if (ov.delayMs > -9000.0)
            doc->writeAttribute(KXMLQLCAudioProfileAubioOnsetOverrideDelay, QString::number(ov.delayMs));
        if (ov.compression >= 0.0)
            doc->writeAttribute(KXMLQLCAudioProfileAubioOnsetOverrideCompression, QString::number(ov.compression));
        if (ov.awhitening >= 0)
            doc->writeAttribute(KXMLQLCAudioProfileAubioOnsetOverrideAwhitening, ov.awhitening ? KXMLQLCTrue : KXMLQLCFalse);
    }
    // Multi-mel bank ranges (low/mid/high). One <MelBank/> child per role; the
    // shared preset label is written on the `low` element so it round-trips.
    {
        const MelBankConfig &mb = m_config.aubio.melBanks;
        struct { const char *role; const MelBankConfig::Bank *bank; } banks[] = {
            { "low",  &mb.low  },
            { "mid",  &mb.mid  },
            { "high", &mb.high }
        };
        for (size_t i = 0; i < sizeof(banks) / sizeof(banks[0]); i++)
        {
            doc->writeEmptyElement(KXMLQLCAudioProfileAubioMelBank);
            doc->writeAttribute(KXMLQLCAudioProfileAubioMelBankRole,
                                QString::fromLatin1(banks[i].role));
            doc->writeAttribute(KXMLQLCAudioProfileAubioMelBankMinHz,
                                QString::number(banks[i].bank->minHz));
            doc->writeAttribute(KXMLQLCAudioProfileAubioMelBankMaxHz,
                                QString::number(banks[i].bank->maxHz));
            doc->writeAttribute(KXMLQLCAudioProfileAubioMelBankBands,
                                QString::number(banks[i].bank->bands));
            const MelPostConfig &p = banks[i].bank->post;
            doc->writeAttribute(KXMLQLCAudioProfileMelBankPowerFactor,   QString::number(p.powerFactor));
            doc->writeAttribute(KXMLQLCAudioProfileMelBankGaussianSigma, QString::number(p.gaussianSigma));
            doc->writeAttribute(KXMLQLCAudioProfileMelBankSmoothDecay,   QString::number(p.smoothDecay));
            doc->writeAttribute(KXMLQLCAudioProfileMelBankSmoothRise,    QString::number(p.smoothRise));
            doc->writeAttribute(KXMLQLCAudioProfileMelBankCommonDecay,   QString::number(p.commonDecay));
            doc->writeAttribute(KXMLQLCAudioProfileMelBankCommonRise,    QString::number(p.commonRise));
            doc->writeAttribute(KXMLQLCAudioProfileMelBankDiffDecay,     QString::number(p.diffDecay));
            doc->writeAttribute(KXMLQLCAudioProfileMelBankDiffRise,      QString::number(p.diffRise));
            doc->writeAttribute(KXMLQLCAudioProfileMelBankAgcDecay,      QString::number(p.agcDecay));
            doc->writeAttribute(KXMLQLCAudioProfileMelBankAgcRise,       QString::number(p.agcRise));
            if (i == 0)
                doc->writeAttribute(KXMLQLCAudioProfileAubioMelBankPreset, mb.preset);
        }
    }
    doc->writeEndElement(); // Aubio

    doc->writeEndElement();
    return true;
}

AudioChannelConfig AudioProfile::configFromLegacySliders(int gain, int reactivity, int floor, int sensitivity)
{
    return AudioChannelConfig::fromLegacySliders(gain, reactivity, floor, sensitivity);
}
