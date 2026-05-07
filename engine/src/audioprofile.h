/*
  Q Light Controller Plus
  audioprofile.h

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

#ifndef AUDIOPROFILE_H
#define AUDIOPROFILE_H

#include <QObject>
#include <QString>
#include <climits>

#include "audiochannelconfig.h"

class AudioAnalyzer;
class AudioChannel;
class QXmlStreamReader;
class QXmlStreamWriter;

/** @addtogroup engine Engine
 * @{
 */

#define KXMLQLCAudioProfile                     QStringLiteral("AudioProfile")
#define KXMLQLCAudioProfileID                   QStringLiteral("ID")
#define KXMLQLCAudioProfileName                 QStringLiteral("Name")
#define KXMLQLCAudioProfileIsDefault            QStringLiteral("IsDefault")
#define KXMLQLCAudioProfileVersion              QStringLiteral("Version")

#define KXMLQLCAudioProfileEnvelope             QStringLiteral("Envelope")
#define KXMLQLCAudioProfileEnvelopeAttack       QStringLiteral("Attack")
#define KXMLQLCAudioProfileEnvelopeRelease      QStringLiteral("Release")

#define KXMLQLCAudioProfileAgc                  QStringLiteral("Agc")
#define KXMLQLCAudioProfileAgcMaxGain           QStringLiteral("MaxGain")
#define KXMLQLCAudioProfileAgcRelease           QStringLiteral("Release")
#define KXMLQLCAudioProfileAgcNoiseFloor        QStringLiteral("NoiseFloor")
#define KXMLQLCAudioProfileAgcInputGain         QStringLiteral("InputGain")
#define KXMLQLCAudioProfileAgcEnabled           QStringLiteral("Enabled")

#define KXMLQLCAudioProfileTriggers             QStringLiteral("Triggers")
#define KXMLQLCAudioProfileTriggersHigh         QStringLiteral("High")
#define KXMLQLCAudioProfileTriggersLow          QStringLiteral("Low")
#define KXMLQLCAudioProfileTriggersHold         QStringLiteral("Hold")
#define KXMLQLCAudioProfileTriggersCooldown     QStringLiteral("Cooldown")

#define KXMLQLCAudioProfileBands                QStringLiteral("Bands")
#define KXMLQLCAudioProfileBandsSubMax          QStringLiteral("SubMax")
#define KXMLQLCAudioProfileBandsBassMax         QStringLiteral("BassMax")
#define KXMLQLCAudioProfileBandsLowMidMax       QStringLiteral("LowMidMax")
#define KXMLQLCAudioProfileBandsMidMax          QStringLiteral("MidMax")
#define KXMLQLCAudioProfileBandsHighMax         QStringLiteral("HighMax")

#define KXMLQLCAudioProfileNoiseGate            QStringLiteral("NoiseGate")
#define KXMLQLCAudioProfileNoiseGateThreshold   QStringLiteral("Threshold")
#define KXMLQLCAudioProfileNoiseGateHold        QStringLiteral("Hold")

#define KXMLQLCAudioProfileVolume               QStringLiteral("Volume")
#define KXMLQLCAudioProfileVolumeSmoothing      QStringLiteral("Smoothing")
#define KXMLQLCAudioProfileVolumeBrightnessFloor QStringLiteral("BrightnessFloor")

#define KXMLQLCAudioProfileAubio                QStringLiteral("Aubio")
#define KXMLQLCAudioProfileAubioOnsetThreshold  QStringLiteral("OnsetThreshold")
#define KXMLQLCAudioProfileAubioOnsetMinInterval QStringLiteral("OnsetMinInterval")
#define KXMLQLCAudioProfileAubioPitchMethod     QStringLiteral("PitchMethod")
#define KXMLQLCAudioProfileAubioPitchSilenceDb  QStringLiteral("PitchSilenceDb")
#define KXMLQLCAudioProfileAubioPitchTolerance  QStringLiteral("PitchTolerance")
#define KXMLQLCAudioProfileAubioTempoMinBpm     QStringLiteral("TempoMinBpm")
#define KXMLQLCAudioProfileAubioTempoMaxBpm     QStringLiteral("TempoMaxBpm")
#define KXMLQLCAudioProfileAubioTatumSubdivision QStringLiteral("TatumSubdivision")
#define KXMLQLCAudioProfileAubioTssAlpha        QStringLiteral("TssAlpha")
#define KXMLQLCAudioProfileAubioTssBeta         QStringLiteral("TssBeta")
#define KXMLQLCAudioProfileAubioTssThreshold    QStringLiteral("TssThreshold")
#define KXMLQLCAudioProfileAubioWindowType      QStringLiteral("WindowType")
#define KXMLQLCAudioProfileAubioMelScale        QStringLiteral("MelScale")
#define KXMLQLCAudioProfileAubioOnsetAdaptiveWhitening QStringLiteral("OnsetAdaptiveWhitening")
#define KXMLQLCAudioProfileAubioOnsetCompressionLambda QStringLiteral("OnsetCompressionLambda")
#define KXMLQLCAudioProfileAubioOnsetMethodsEnabled QStringLiteral("OnsetMethodsEnabled")
#define KXMLQLCAudioProfileAubioTempoDelayMs    QStringLiteral("TempoDelayMs")
#define KXMLQLCAudioProfileAubioNoteSilenceDb   QStringLiteral("NoteSilenceDb")
#define KXMLQLCAudioProfileAubioNoteMinIntervalMs QStringLiteral("NoteMinIntervalMs")
#define KXMLQLCAudioProfileAubioNoteReleaseDropDb QStringLiteral("NoteReleaseDropDb")
#define KXMLQLCAudioProfileAubioMfccPower       QStringLiteral("MfccPower")
#define KXMLQLCAudioProfileAubioMfccScale       QStringLiteral("MfccScale")

class AudioProfile : public QObject
{
    Q_OBJECT
    Q_PROPERTY(quint32 id READ id CONSTANT)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(bool isDefault READ isDefault WRITE setIsDefault NOTIFY isDefaultChanged)

public:
    static quint32 invalidId() { return UINT_MAX; }

    AudioProfile(quint32 id, QObject *parent = nullptr);
    ~AudioProfile();

    quint32 id() const;

    QString name() const;
    void setName(const QString &name);

    bool isDefault() const;
    void setIsDefault(bool def);

    AudioChannelConfig channelConfig() const;
    void setChannelConfig(const AudioChannelConfig &config);

    void bindAnalyzer(AudioAnalyzer *analyzer);
    void releaseAnalyzer();
    AudioChannel* channel() const;

    bool loadXML(QXmlStreamReader &root);
    bool saveXML(QXmlStreamWriter *doc) const;

    static AudioChannelConfig configFromLegacySliders(int gain, int reactivity, int floor, int sensitivity);

signals:
    void nameChanged();
    void isDefaultChanged();
    void configChanged();

private:
    quint32 m_id;
    QString m_name;
    bool m_isDefault = false;
    AudioChannelConfig m_config;
    AudioAnalyzer *m_analyzer = nullptr;
    AudioChannel *m_channel = nullptr;
};

/** @} */

#endif // AUDIOPROFILE_H
