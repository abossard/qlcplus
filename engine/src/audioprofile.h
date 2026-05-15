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
#define KXMLQLCAudioProfileTriggersBand         QStringLiteral("Band")
#define KXMLQLCAudioProfileTriggersBandName     QStringLiteral("Name")
#define KXMLQLCAudioProfileTriggersHigh         QStringLiteral("High")
#define KXMLQLCAudioProfileTriggersLow          QStringLiteral("Low")
#define KXMLQLCAudioProfileTriggersHold         QStringLiteral("Hold")
#define KXMLQLCAudioProfileTriggersCooldown     QStringLiteral("Cooldown")

// Legacy 5-perceptual-band layout (BandLayout struct removed). The element
// name is still recognised by the loader so old profiles parse cleanly; the
// per-band attributes are silently dropped.
#define KXMLQLCAudioProfileBands                QStringLiteral("Bands")

#define KXMLQLCAudioProfileNoiseGate            QStringLiteral("NoiseGate")
#define KXMLQLCAudioProfileNoiseGateThreshold   QStringLiteral("Threshold")
#define KXMLQLCAudioProfileNoiseGateHold        QStringLiteral("Hold")

#define KXMLQLCAudioProfileKick                 QStringLiteral("Kick")
#define KXMLQLCAudioProfileKickEnabled          QStringLiteral("Enabled")
#define KXMLQLCAudioProfileKickBeatMaxHz        QStringLiteral("BeatMaxHz")
#define KXMLQLCAudioProfileKickBeatMinPercentDiff QStringLiteral("BeatMinPercentDiff")
#define KXMLQLCAudioProfileKickBeatMinAmplitude QStringLiteral("BeatMinAmplitude")
#define KXMLQLCAudioProfileKickBeatRefractorySec QStringLiteral("BeatRefractorySec")
#define KXMLQLCAudioProfileKickBeatHistoryLen   QStringLiteral("BeatHistoryLen")

// Per-bank mel post-processing attributes — written flat on each
// <MelBank/> element. Mirrors LedFx melbank.py:374-378 (every bank owns
// its own ExpFilter chain).
#define KXMLQLCAudioProfileMelBankPowerFactor   QStringLiteral("PowerFactor")
#define KXMLQLCAudioProfileMelBankGaussianSigma QStringLiteral("GaussianSigma")
#define KXMLQLCAudioProfileMelBankSmoothDecay   QStringLiteral("SmoothDecay")
#define KXMLQLCAudioProfileMelBankSmoothRise    QStringLiteral("SmoothRise")
#define KXMLQLCAudioProfileMelBankCommonDecay   QStringLiteral("CommonDecay")
#define KXMLQLCAudioProfileMelBankCommonRise    QStringLiteral("CommonRise")
#define KXMLQLCAudioProfileMelBankDiffDecay     QStringLiteral("DiffDecay")
#define KXMLQLCAudioProfileMelBankDiffRise      QStringLiteral("DiffRise")
#define KXMLQLCAudioProfileMelBankAgcDecay      QStringLiteral("AgcDecay")
#define KXMLQLCAudioProfileMelBankAgcRise       QStringLiteral("AgcRise")
#define KXMLQLCAudioProfileMelBankEnabled       QStringLiteral("Enabled")

// Master 40-band post-processor (snap.melProcessed[] / spectral flatness).
// Same attribute names as <MelBank/>; lives on its own element so the master
// pipeline can be tuned independently of the visualization banks.
#define KXMLQLCAudioProfileMelPost              QStringLiteral("MelPost")
#define KXMLQLCAudioProfileMelPostEnabled       QStringLiteral("Enabled")

#define KXMLQLCAudioProfileFreqPower            QStringLiteral("FreqPower")
#define KXMLQLCAudioProfileFreqPowerBand        QStringLiteral("Band")
#define KXMLQLCAudioProfileFreqPowerBandName    QStringLiteral("Name")
#define KXMLQLCAudioProfileFreqPowerBandMaxHz   QStringLiteral("MaxHz")
#define KXMLQLCAudioProfileFreqPowerBandDecay   QStringLiteral("Decay")
#define KXMLQLCAudioProfileFreqPowerBandRise    QStringLiteral("Rise")

#define KXMLQLCAudioProfileVolume               QStringLiteral("Volume")
#define KXMLQLCAudioProfileVolumeSmoothing      QStringLiteral("Smoothing")
#define KXMLQLCAudioProfileVolumeBrightnessFloor QStringLiteral("BrightnessFloor")

#define KXMLQLCAudioProfileAubio                QStringLiteral("Aubio")
#define KXMLQLCAudioProfileAubioOnsetThreshold  QStringLiteral("OnsetThreshold")
#define KXMLQLCAudioProfileAubioOnsetMinInterval QStringLiteral("OnsetMinInterval")
#define KXMLQLCAudioProfileAubioPitchMethod     QStringLiteral("PitchMethod")
#define KXMLQLCAudioProfileAubioPitchSilenceDb  QStringLiteral("PitchSilenceDb")
#define KXMLQLCAudioProfileAubioPitchTolerance  QStringLiteral("PitchTolerance")
#define KXMLQLCAudioProfileAubioFilterbankNorm  QStringLiteral("FilterbankNorm")
#define KXMLQLCAudioProfileAubioFilterbankPower QStringLiteral("FilterbankPower")
#define KXMLQLCAudioProfileAubioTempoSilenceDb  QStringLiteral("TempoSilenceDb")
#define KXMLQLCAudioProfileAubioTempoThreshold  QStringLiteral("TempoThreshold")
#define KXMLQLCAudioProfileAubioTempoMethod     QStringLiteral("TempoMethod")
#define KXMLQLCAudioProfileAubioCoastBeats      QStringLiteral("CoastBeats")
#define KXMLQLCAudioProfileAubioTempoDecayHalfLifeBeats QStringLiteral("TempoDecayHalfLifeBeats")
#define KXMLQLCAudioProfileAubioTempoDecayTargetBpm     QStringLiteral("TempoDecayTargetBpm")
#define KXMLQLCAudioProfileAubioTempoMinBpm     QStringLiteral("TempoMinBpm")
#define KXMLQLCAudioProfileAubioTempoMaxBpm     QStringLiteral("TempoMaxBpm")
#define KXMLQLCAudioProfileAubioTatumSubdivision QStringLiteral("TatumSubdivision")
#define KXMLQLCAudioProfileAubioBeatsPerBar     QStringLiteral("BeatsPerBar")
#define KXMLQLCAudioProfileAubioPreEmphasisEnabled QStringLiteral("PreEmphasisEnabled")
#define KXMLQLCAudioProfileAubioTssAlpha        QStringLiteral("TssAlpha")
#define KXMLQLCAudioProfileAubioTssBeta         QStringLiteral("TssBeta")
#define KXMLQLCAudioProfileAubioTssThreshold    QStringLiteral("TssThreshold")
#define KXMLQLCAudioProfileAubioWindowType      QStringLiteral("WindowType")
#define KXMLQLCAudioProfileAubioMelScale        QStringLiteral("MelScale")
#define KXMLQLCAudioProfileAubioOnsetMethodsEnabled QStringLiteral("OnsetMethodsEnabled")
#define KXMLQLCAudioProfileAubioOnsetMethodIndex QStringLiteral("OnsetMethodIndex")
#define KXMLQLCAudioProfileAubioTempoDelayMs    QStringLiteral("TempoDelayMs")
#define KXMLQLCAudioProfileAubioNoteSilenceDb   QStringLiteral("NoteSilenceDb")
#define KXMLQLCAudioProfileAubioNoteMinIntervalMs QStringLiteral("NoteMinIntervalMs")
#define KXMLQLCAudioProfileAubioNoteReleaseDropDb QStringLiteral("NoteReleaseDropDb")
#define KXMLQLCAudioProfileAubioMfccPower       QStringLiteral("MfccPower")
#define KXMLQLCAudioProfileAubioMfccScale       QStringLiteral("MfccScale")
#define KXMLQLCAudioProfileAubioPitchUnit       QStringLiteral("PitchUnit")
#define KXMLQLCAudioProfileAubioOnsetOverride   QStringLiteral("OnsetOverride")
#define KXMLQLCAudioProfileAubioOnsetOverrideMethod      QStringLiteral("Method")
#define KXMLQLCAudioProfileAubioOnsetOverrideThreshold   QStringLiteral("Threshold")
#define KXMLQLCAudioProfileAubioOnsetOverrideSilence     QStringLiteral("Silence")
#define KXMLQLCAudioProfileAubioOnsetOverrideMinioi      QStringLiteral("Minioi")
#define KXMLQLCAudioProfileAubioOnsetOverrideDelay       QStringLiteral("Delay")
#define KXMLQLCAudioProfileAubioOnsetOverrideCompression QStringLiteral("Compression")
#define KXMLQLCAudioProfileAubioOnsetOverrideAwhitening  QStringLiteral("Awhitening")

#define KXMLQLCAudioProfileAubioMelBank         QStringLiteral("MelBank")
#define KXMLQLCAudioProfileAubioMelBankRole     QStringLiteral("Role")
#define KXMLQLCAudioProfileAubioMelBankMinHz    QStringLiteral("MinHz")
#define KXMLQLCAudioProfileAubioMelBankMaxHz    QStringLiteral("MaxHz")
#define KXMLQLCAudioProfileAubioMelBankBands    QStringLiteral("Bands")
#define KXMLQLCAudioProfileAubioMelBankPreset   QStringLiteral("Preset")

#define KXMLQLCAudioProfileAudioSource          QStringLiteral("AudioSource")
#define KXMLQLCAudioProfileOscPort              QStringLiteral("OscPort")

class AudioProfile : public QObject
{
    Q_OBJECT
    Q_PROPERTY(quint32 id READ id CONSTANT)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(bool isDefault READ isDefault WRITE setIsDefault NOTIFY isDefaultChanged)
    Q_PROPERTY(int audioSource READ audioSource WRITE setAudioSource NOTIFY audioSourceChanged)
    Q_PROPERTY(quint16 oscPort READ oscPort WRITE setOscPort NOTIFY oscPortChanged)

public:
    enum AudioSourceType
    {
        Microphone = 0,
        OscSynesthesia = 1
    };
    Q_ENUM(AudioSourceType)

    static quint32 invalidId() { return UINT_MAX; }

    AudioProfile(quint32 id, QObject *parent = nullptr);
    ~AudioProfile();

    quint32 id() const;

    QString name() const;
    void setName(const QString &name);

    bool isDefault() const;
    void setIsDefault(bool def);

    int audioSource() const;
    void setAudioSource(int source);

    quint16 oscPort() const;
    void setOscPort(quint16 port);

    AudioChannelConfig channelConfig() const;
    void setChannelConfig(const AudioChannelConfig &config);

    void bindAnalyzer(AudioAnalyzer *analyzer);
    void releaseAnalyzer();
    AudioChannel* channel() const;

    bool loadXML(QXmlStreamReader &root);
    bool saveXML(QXmlStreamWriter *doc) const;

signals:
    void nameChanged();
    void isDefaultChanged();
    void configChanged();
    void audioSourceChanged(int source);
    void oscPortChanged();
    /** Emitted when a new snapshot is available (from any source). */
    void snapshotUpdated();

private:
    quint32 m_id;
    QString m_name;
    bool m_isDefault = false;
    AudioChannelConfig m_config;
    AudioAnalyzer *m_analyzer = nullptr;
    AudioChannel *m_channel = nullptr;
    AudioSourceType m_audioSource = Microphone;
    quint16 m_oscPort = 9999;
};

/** @} */

#endif // AUDIOPROFILE_H
