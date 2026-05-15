/*
  Q Light Controller Plus
  oscaudiosource.h

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

#pragma once

#include "audiosnapshot.h"

#include <QHostAddress>
#include <QObject>

#include <atomic>

class QUdpSocket;
class QTimer;
class AudioChannel;

/**
 * Decoded OSC values from Synesthesia. All floats, no quantization.
 */
struct SynRawState
{
    // Band levels (0..1)
    double levelBass = 0.0;
    double levelMid = 0.0;
    double levelMidHigh = 0.0;
    double levelHigh = 0.0;
    double levelAll = 0.0;
    double levelRaw = 0.0;

    // Hits / transients (0..1)
    double hitsBass = 0.0;
    double hitsMid = 0.0;
    double hitsMidHigh = 0.0;
    double hitsHigh = 0.0;
    double hitsAll = 0.0;

    // Presence / slow envelopes (0..1)
    double presenceBass = 0.0;
    double presenceMid = 0.0;
    double presenceMidHigh = 0.0;
    double presenceHigh = 0.0;
    double presenceAll = 0.0;

    // Beat detection
    double onbeat = 0.0;
    double beattime = 0.0;
    double randomOnBeat = 0.0;

    // BPM
    double bpm = 0.0;
    double bpmConfidence = 0.0;

    // Beat-synced LFOs
    double bpmTri = 0.0;
    double bpmTri2 = 0.0;
    double bpmTri4 = 0.0;
    double bpmTri8 = 0.0;
    double bpmSin = 0.0;
    double bpmSin2 = 0.0;
    double bpmSin4 = 0.0;
    double bpmSin8 = 0.0;
    double bpmTwitcher = 0.0;

    // Energy
    double energyIntensity = 0.0;

    // Timecode
    double timecode = 0.0;

    bool hasData = false;
};

/**
 * Schmitt trigger with hold and cooldown, matching QLC+ AudioChannel behavior.
 */
struct OscSchmittTrigger
{
    double highThreshold;
    double lowThreshold;
    double holdMs;
    double cooldownMs;

    bool active = false;
    double heldMs = 0.0;
    double cooldownRemainingMs = 0.0;
    bool firedThisFrame = false;
    bool releasedThisFrame = false;

    TriggerState update(double value, double dtMs);
};

/**
 * Receives OSC audio data from Synesthesia (or similar) on a UDP port,
 * maps it to AudioSnapshot fields, and injects it into an AudioChannel.
 *
 * This is a parallel implementation to OSCPacketizer that preserves float
 * precision — the existing OSC plugin quantizes to uchar (0-255) which
 * destroys audio data fidelity.
 */
class OscAudioSource : public QObject
{
    Q_OBJECT
    Q_PROPERTY(quint16 port READ port WRITE setPort NOTIFY portChanged)
    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)

public:
    explicit OscAudioSource(QObject *parent = nullptr);
    ~OscAudioSource();

    void setTargetChannel(AudioChannel *channel);
    AudioChannel *targetChannel() const;

    quint16 port() const;
    void setPort(quint16 port);

    bool isRunning() const;

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();

    SynRawState rawState() const;

signals:
    void portChanged();
    void runningChanged();
    /** Emitted each time a new snapshot is injected into the target channel. */
    void snapshotInjected();

private slots:
    void slotReadPendingDatagrams();
    void slotInjectSnapshot();

private:
    void parseOscMessage(const QByteArray &data);
    void parseOscFloat(const QString &address, float value);
    AudioSnapshot buildSnapshot(double dtMs);

    QUdpSocket *m_socket = nullptr;
    QTimer *m_injectTimer = nullptr;
    AudioChannel *m_targetChannel = nullptr;
    quint16 m_port = 9999;
    bool m_running = false;

    SynRawState m_raw;

    // Schmitt triggers matching QLC+ AudioChannelConfig defaults
    OscSchmittTrigger m_lowTrigger   { 0.45, 0.25, 150.0, 200.0 };
    OscSchmittTrigger m_midTrigger   { 0.65, 0.45,  80.0, 120.0 };
    OscSchmittTrigger m_highTrigger  { 0.70, 0.50,  60.0, 100.0 };
    OscSchmittTrigger m_volTrigger   { 0.65, 0.45,  80.0, 120.0 };

    // Beat phase synthesis
    double m_beatPhaseSec = 0.0;
    double m_prevOnbeat = 0.0;
    int m_prevBeattimeInt = -1;

    // Noise gate heuristic
    double m_gateSmoothed = 0.0;
    double m_gateHeldMs = 0.0;
    bool m_gateClosed = true;

    // Kick approximation
    double m_prevBassPresence = 0.0;
    OscSchmittTrigger m_kickTrigger { 0.45, 0.25, 60.0, 100.0 };

    std::atomic<bool> m_hasNewData { false };
};
