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
#include "qlcfile.h"

#include <QDebug>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

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
        return false;
    }
    m_id = id;

    if (attrs.hasAttribute(KXMLQLCAudioProfileName))
        setName(attrs.value(KXMLQLCAudioProfileName).toString());

    if (attrs.hasAttribute(KXMLQLCAudioProfileIsDefault))
        setIsDefault(boolFromString(attrs.value(KXMLQLCAudioProfileIsDefault).toString()));

    constexpr int kSupportedVersion = 1;
    int version = 0;
    if (attrs.hasAttribute(KXMLQLCAudioProfileVersion))
    {
        bool versionOk = false;
        const int parsed = attrs.value(KXMLQLCAudioProfileVersion).toString().toInt(&versionOk);
        if (!versionOk)
        {
            qWarning() << "AudioProfile" << m_name
                       << "has unparseable Version attribute"
                       << attrs.value(KXMLQLCAudioProfileVersion).toString()
                       << "- treating as legacy (version 0). Defaults will be used for missing settings.";
            version = 0;
        }
        else if (parsed < 0)
        {
            qWarning() << "AudioProfile" << m_name << "has negative Version" << parsed
                       << "- treating as legacy (version 0). Defaults will be used for missing settings.";
            version = 0;
        }
        else
        {
            version = parsed;
            if (version > kSupportedVersion)
            {
                qWarning() << "AudioProfile" << m_name << "has version" << version
                           << "which is newer than supported (" << kSupportedVersion
                           << "). Some settings may be lost.";
            }
        }
    }
    else
    {
        // Missing Version attribute: treat as legacy (version 0).
        version = 0;
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
            config.bandLayout.subMaxHz = doubleAttribute(childAttrs,
                                                         KXMLQLCAudioProfileBandsSubMax,
                                                         config.bandLayout.subMaxHz);
            config.bandLayout.bassMaxHz = doubleAttribute(childAttrs,
                                                          KXMLQLCAudioProfileBandsBassMax,
                                                          config.bandLayout.bassMaxHz);
            config.bandLayout.lowMidMaxHz = doubleAttribute(childAttrs,
                                                            KXMLQLCAudioProfileBandsLowMidMax,
                                                            config.bandLayout.lowMidMaxHz);
            config.bandLayout.midMaxHz = doubleAttribute(childAttrs,
                                                         KXMLQLCAudioProfileBandsMidMax,
                                                         config.bandLayout.midMaxHz);
            config.bandLayout.highMaxHz = doubleAttribute(childAttrs,
                                                          KXMLQLCAudioProfileBandsHighMax,
                                                          config.bandLayout.highMaxHz);
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
            config.aubio.onsetThreshold = doubleAttribute(childAttrs,
                                                          KXMLQLCAudioProfileAubioOnsetThreshold,
                                                          config.aubio.onsetThreshold);
            config.aubio.onsetMinIntervalMs = doubleAttribute(childAttrs,
                                                              KXMLQLCAudioProfileAubioOnsetMinInterval,
                                                              config.aubio.onsetMinIntervalMs);
            config.aubio.pitchMethod = stringAttribute(childAttrs,
                                                       KXMLQLCAudioProfileAubioPitchMethod,
                                                       config.aubio.pitchMethod);
            config.aubio.pitchSilenceDb = doubleAttribute(childAttrs,
                                                          KXMLQLCAudioProfileAubioPitchSilenceDb,
                                                          config.aubio.pitchSilenceDb);
            config.aubio.pitchTolerance = doubleAttribute(childAttrs,
                                                          KXMLQLCAudioProfileAubioPitchTolerance,
                                                          config.aubio.pitchTolerance);
            config.aubio.tatumSubdivision = intAttribute(childAttrs,
                                                         KXMLQLCAudioProfileAubioTatumSubdivision,
                                                         config.aubio.tatumSubdivision);
            config.aubio.tssAlpha = doubleAttribute(childAttrs,
                                                    KXMLQLCAudioProfileAubioTssAlpha,
                                                    config.aubio.tssAlpha);
            config.aubio.tssBeta = doubleAttribute(childAttrs,
                                                   KXMLQLCAudioProfileAubioTssBeta,
                                                   config.aubio.tssBeta);
            config.aubio.tssThreshold = doubleAttribute(childAttrs,
                                                        KXMLQLCAudioProfileAubioTssThreshold,
                                                        config.aubio.tssThreshold);
            root.skipCurrentElement();
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
    doc->writeAttribute(KXMLQLCAudioProfileVersion, QStringLiteral("1"));

    doc->writeEmptyElement(KXMLQLCAudioProfileEnvelope);
    doc->writeAttribute(KXMLQLCAudioProfileEnvelopeAttack, QString::number(m_config.envelope.attackMs));
    doc->writeAttribute(KXMLQLCAudioProfileEnvelopeRelease, QString::number(m_config.envelope.releaseMs));

    doc->writeEmptyElement(KXMLQLCAudioProfileTriggers);
    doc->writeAttribute(KXMLQLCAudioProfileTriggersHigh, QString::number(m_config.triggers.highThreshold));
    doc->writeAttribute(KXMLQLCAudioProfileTriggersLow, QString::number(m_config.triggers.lowThreshold));
    doc->writeAttribute(KXMLQLCAudioProfileTriggersHold, QString::number(m_config.triggers.holdMs));
    doc->writeAttribute(KXMLQLCAudioProfileTriggersCooldown, QString::number(m_config.triggers.cooldownMs));

    doc->writeEmptyElement(KXMLQLCAudioProfileBands);
    doc->writeAttribute(KXMLQLCAudioProfileBandsSubMax, QString::number(m_config.bandLayout.subMaxHz));
    doc->writeAttribute(KXMLQLCAudioProfileBandsBassMax, QString::number(m_config.bandLayout.bassMaxHz));
    doc->writeAttribute(KXMLQLCAudioProfileBandsLowMidMax, QString::number(m_config.bandLayout.lowMidMaxHz));
    doc->writeAttribute(KXMLQLCAudioProfileBandsMidMax, QString::number(m_config.bandLayout.midMaxHz));
    doc->writeAttribute(KXMLQLCAudioProfileBandsHighMax, QString::number(m_config.bandLayout.highMaxHz));

    doc->writeEmptyElement(KXMLQLCAudioProfileNoiseGate);
    doc->writeAttribute(KXMLQLCAudioProfileNoiseGateThreshold, QString::number(m_config.noiseGate.thresholdDb));
    doc->writeAttribute(KXMLQLCAudioProfileNoiseGateHold, QString::number(m_config.noiseGate.holdMs));

    doc->writeEmptyElement(KXMLQLCAudioProfileVolume);
    doc->writeAttribute(KXMLQLCAudioProfileVolumeSmoothing, QString::number(m_config.volumeSmoothingMs));
    doc->writeAttribute(KXMLQLCAudioProfileVolumeBrightnessFloor, QString::number(m_config.brightnessFloor));

    doc->writeEmptyElement(KXMLQLCAudioProfileAubio);
    doc->writeAttribute(KXMLQLCAudioProfileAubioOnsetThreshold, QString::number(m_config.aubio.onsetThreshold));
    doc->writeAttribute(KXMLQLCAudioProfileAubioOnsetMinInterval, QString::number(m_config.aubio.onsetMinIntervalMs));
    doc->writeAttribute(KXMLQLCAudioProfileAubioPitchMethod, m_config.aubio.pitchMethod);
    doc->writeAttribute(KXMLQLCAudioProfileAubioPitchSilenceDb, QString::number(m_config.aubio.pitchSilenceDb));
    doc->writeAttribute(KXMLQLCAudioProfileAubioPitchTolerance, QString::number(m_config.aubio.pitchTolerance));
    doc->writeAttribute(KXMLQLCAudioProfileAubioTatumSubdivision, QString::number(m_config.aubio.tatumSubdivision));
    doc->writeAttribute(KXMLQLCAudioProfileAubioTssAlpha, QString::number(m_config.aubio.tssAlpha));
    doc->writeAttribute(KXMLQLCAudioProfileAubioTssBeta, QString::number(m_config.aubio.tssBeta));
    doc->writeAttribute(KXMLQLCAudioProfileAubioTssThreshold, QString::number(m_config.aubio.tssThreshold));

    doc->writeEndElement();
    return true;
}

AudioChannelConfig AudioProfile::configFromLegacySliders(int gain, int reactivity, int floor, int sensitivity)
{
    return AudioChannelConfig::fromLegacySliders(gain, reactivity, floor, sensitivity);
}
