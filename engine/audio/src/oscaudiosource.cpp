/*
  Q Light Controller Plus
  oscaudiosource.cpp

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

#include "oscaudiosource.h"
#include "audiochannel.h"

#include <QUdpSocket>
#include <QTimer>
#include <QDebug>

#include <algorithm>
#include <cmath>
#include <cstring>

// --------------------------------------------------------------------------
// OscSchmittTrigger
// --------------------------------------------------------------------------

TriggerState OscSchmittTrigger::update(double value, double dtMs)
{
    firedThisFrame = false;
    releasedThisFrame = false;

    if (cooldownRemainingMs > 0.0)
    {
        cooldownRemainingMs = std::max(0.0, cooldownRemainingMs - dtMs);
    }

    if (active)
    {
        heldMs += dtMs;
        if (heldMs >= holdMs && value < lowThreshold)
        {
            active = false;
            releasedThisFrame = true;
            cooldownRemainingMs = cooldownMs;
            heldMs = 0.0;
        }
    }
    else if (cooldownRemainingMs <= 0.0 && value >= highThreshold)
    {
        active = true;
        firedThisFrame = true;
        heldMs = 0.0;
    }

    TriggerState ts;
    ts.value = value;
    ts.active = active;
    ts.firedThisFrame = firedThisFrame;
    ts.releasedThisFrame = releasedThisFrame;
    ts.heldMs = heldMs;
    ts.cooldownRemainingMs = cooldownRemainingMs;
    return ts;
}

// --------------------------------------------------------------------------
// OscAudioSource
// --------------------------------------------------------------------------

OscAudioSource::OscAudioSource(QObject *parent)
    : QObject(parent)
{
}

OscAudioSource::~OscAudioSource()
{
    stop();
}

void OscAudioSource::setTargetChannel(AudioChannel *channel)
{
    m_targetChannel = channel;
}

AudioChannel *OscAudioSource::targetChannel() const
{
    return m_targetChannel;
}

quint16 OscAudioSource::port() const
{
    return m_port;
}

void OscAudioSource::setPort(quint16 port)
{
    if (m_port == port)
        return;

    bool wasRunning = m_running;
    if (wasRunning)
        stop();

    m_port = port;
    emit portChanged();

    if (wasRunning)
        start();
}

bool OscAudioSource::isRunning() const
{
    return m_running;
}

void OscAudioSource::start()
{
    if (m_running)
        return;

    m_socket = new QUdpSocket(this);
    if (!m_socket->bind(QHostAddress::AnyIPv4, m_port,
                        QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint))
    {
        qWarning() << "OscAudioSource: failed to bind UDP port" << m_port
                    << m_socket->errorString();
        delete m_socket;
        m_socket = nullptr;
        return;
    }

    connect(m_socket, &QUdpSocket::readyRead,
            this, &OscAudioSource::slotReadPendingDatagrams);

    // Inject timer at ~60Hz to match Synesthesia's send rate
    m_injectTimer = new QTimer(this);
    m_injectTimer->setTimerType(Qt::PreciseTimer);
    m_injectTimer->setInterval(16); // ~62.5Hz
    connect(m_injectTimer, &QTimer::timeout,
            this, &OscAudioSource::slotInjectSnapshot);
    m_injectTimer->start();

    m_running = true;
    m_raw = SynRawState();
    m_hasNewData.store(false);

    qDebug() << "OscAudioSource: started on port" << m_port;
    emit runningChanged();
}

void OscAudioSource::stop()
{
    if (!m_running)
        return;

    if (m_injectTimer)
    {
        m_injectTimer->stop();
        delete m_injectTimer;
        m_injectTimer = nullptr;
    }

    if (m_socket)
    {
        m_socket->close();
        delete m_socket;
        m_socket = nullptr;
    }

    m_running = false;
    m_raw = SynRawState();
    qDebug() << "OscAudioSource: stopped";
    emit runningChanged();
}

SynRawState OscAudioSource::rawState() const
{
    return m_raw;
}

// --------------------------------------------------------------------------
// UDP packet reception
// --------------------------------------------------------------------------

void OscAudioSource::slotReadPendingDatagrams()
{
    while (m_socket && m_socket->hasPendingDatagrams())
    {
        QByteArray datagram;
        datagram.resize(int(m_socket->pendingDatagramSize()));
        m_socket->readDatagram(datagram.data(), datagram.size());
        parseOscMessage(datagram);
    }
}

// --------------------------------------------------------------------------
// OSC float parser — intentionally separate from OSCPacketizer which
// quantizes to uchar. This preserves full float precision.
// --------------------------------------------------------------------------

static float readBigEndianFloat(const char *data)
{
    float val;
    auto *dst = reinterpret_cast<char *>(&val);
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
    dst[0] = data[3];
    dst[1] = data[2];
    dst[2] = data[1];
    dst[3] = data[0];
#else
    std::memcpy(dst, data, 4);
#endif
    return val;
}

static int oscPadded(int len)
{
    return (len + 3) & ~3;
}

void OscAudioSource::parseOscMessage(const QByteArray &data)
{
    if (data.size() < 8)
        return;

    // Check for bundle
    if (data.startsWith("#bundle"))
    {
        int pos = 16; // skip "#bundle\0" + 8-byte timetag
        while (pos + 4 <= data.size())
        {
            int msgSize = 0;
            auto *sizePtr = reinterpret_cast<const uchar *>(data.constData() + pos);
            msgSize = (sizePtr[0] << 24) | (sizePtr[1] << 16) |
                      (sizePtr[2] << 8)  | sizePtr[3];
            pos += 4;
            if (msgSize <= 0 || pos + msgSize > data.size())
                break;
            parseOscMessage(data.mid(pos, msgSize));
            pos += msgSize;
        }
        return;
    }

    // Single message: extract address
    int addrEnd = data.indexOf('\0');
    if (addrEnd < 1)
        return;
    QString address = QString::fromUtf8(data.constData(), addrEnd);

    // Skip to type tag string
    int pos = oscPadded(addrEnd + 1);
    if (pos >= data.size() || data.at(pos) != ',')
        return;

    int tagEnd = data.indexOf('\0', pos);
    if (tagEnd < 0)
        return;
    QString tags = QString::fromUtf8(data.constData() + pos + 1, tagEnd - pos - 1);
    pos = oscPadded(tagEnd + 1);

    // Extract first float value (we only care about float arguments)
    for (int i = 0; i < tags.length() && pos < data.size(); i++)
    {
        QChar tag = tags.at(i);
        if (tag == 'f' && pos + 4 <= data.size())
        {
            float val = readBigEndianFloat(data.constData() + pos);
            if (i == 0)
                parseOscFloat(address, val);
            pos += 4;
        }
        else if (tag == 'i')
        {
            pos += 4;
        }
        else if (tag == 'd')
        {
            pos += 8;
        }
        else if (tag == 's')
        {
            int end = data.indexOf('\0', pos);
            if (end < 0) break;
            pos = oscPadded(end + 1);
        }
        else if (tag == 'T' || tag == 'F' || tag == 'N' || tag == 'I')
        {
            // No data bytes for these types
        }
        else
        {
            break; // Unknown type, stop
        }
    }
}

void OscAudioSource::parseOscFloat(const QString &address, float value)
{
    double v = double(value);
    m_raw.hasData = true;
    m_hasNewData.store(true, std::memory_order_relaxed);

    // Band levels
    if (address == QLatin1String("/audio/level/bass"))       { m_raw.levelBass = v; return; }
    if (address == QLatin1String("/audio/level/mid"))        { m_raw.levelMid = v; return; }
    if (address == QLatin1String("/audio/level/midhigh"))    { m_raw.levelMidHigh = v; return; }
    if (address == QLatin1String("/audio/level/high"))       { m_raw.levelHigh = v; return; }
    if (address == QLatin1String("/audio/level/all"))        { m_raw.levelAll = v; return; }
    if (address == QLatin1String("/audio/level/raw"))        { m_raw.levelRaw = v; return; }

    // Hits
    if (address == QLatin1String("/audio/hits/bass"))        { m_raw.hitsBass = v; return; }
    if (address == QLatin1String("/audio/hits/mid"))         { m_raw.hitsMid = v; return; }
    if (address == QLatin1String("/audio/hits/midhigh"))     { m_raw.hitsMidHigh = v; return; }
    if (address == QLatin1String("/audio/hits/high"))        { m_raw.hitsHigh = v; return; }
    if (address == QLatin1String("/audio/hits/all"))         { m_raw.hitsAll = v; return; }

    // Presence
    if (address == QLatin1String("/audio/presence/bass"))    { m_raw.presenceBass = v; return; }
    if (address == QLatin1String("/audio/presence/mid"))     { m_raw.presenceMid = v; return; }
    if (address == QLatin1String("/audio/presence/midhigh")) { m_raw.presenceMidHigh = v; return; }
    if (address == QLatin1String("/audio/presence/high"))    { m_raw.presenceHigh = v; return; }
    if (address == QLatin1String("/audio/presence/all"))     { m_raw.presenceAll = v; return; }

    // Beat
    if (address == QLatin1String("/audio/beat/onbeat"))      { m_raw.onbeat = v; return; }
    if (address == QLatin1String("/audio/beat/beattime"))    { m_raw.beattime = v; return; }
    if (address == QLatin1String("/audio/beat/randomonbeat")){ m_raw.randomOnBeat = v; return; }

    // BPM
    if (address == QLatin1String("/audio/bpm"))              { m_raw.bpm = v; return; }
    if (address == QLatin1String("/audio/bpm/bpm"))          { m_raw.bpm = v; return; }
    if (address == QLatin1String("/audio/bpm/bpmconfidence")){ m_raw.bpmConfidence = v; return; }

    // BPM LFOs
    if (address == QLatin1String("/audio/bpm/bpmtri"))       { m_raw.bpmTri = v; return; }
    if (address == QLatin1String("/audio/bpm/bpmtri2"))      { m_raw.bpmTri2 = v; return; }
    if (address == QLatin1String("/audio/bpm/bpmtri4"))      { m_raw.bpmTri4 = v; return; }
    if (address == QLatin1String("/audio/bpm/bpmtri8"))      { m_raw.bpmTri8 = v; return; }
    if (address == QLatin1String("/audio/bpm/bpmsin"))       { m_raw.bpmSin = v; return; }
    if (address == QLatin1String("/audio/bpm/bpmsin2"))      { m_raw.bpmSin2 = v; return; }
    if (address == QLatin1String("/audio/bpm/bpmsin4"))      { m_raw.bpmSin4 = v; return; }
    if (address == QLatin1String("/audio/bpm/bpmsin8"))      { m_raw.bpmSin8 = v; return; }
    if (address == QLatin1String("/audio/bpm/bpmtwitcher"))  { m_raw.bpmTwitcher = v; return; }

    // Energy
    if (address == QLatin1String("/audio/energy/intensity")) { m_raw.energyIntensity = v; return; }

    // Timecode
    if (address == QLatin1String("/audio/timecode"))         { m_raw.timecode = v; return; }
}

// --------------------------------------------------------------------------
// Snapshot injection (timer-driven, ~60Hz)
// --------------------------------------------------------------------------

void OscAudioSource::slotInjectSnapshot()
{
    if (!m_targetChannel)
        return;

    // Build a snapshot every tick even without new data so that Schmitt trigger
    // hold/cooldown timers advance and firedThisFrame resets correctly.
    constexpr double dtMs = 16.0; // ~60Hz timer interval
    AudioSnapshot snap = buildSnapshot(dtMs);
    m_targetChannel->injectSnapshot(snap);
    m_hasNewData.store(false, std::memory_order_relaxed);
    emit snapshotInjected();
}

AudioSnapshot OscAudioSource::buildSnapshot(double dtMs)
{
    AudioSnapshot snap;
    const SynRawState &r = m_raw;

    // --- Frequency band powers ---
    // Synesthesia bass ≈ 20-200Hz, QLC+ lows = 0-250Hz
    snap.lows = std::clamp(r.levelBass, 0.0, 1.0);
    // Split bass for beatPower (0-100Hz) and bassPower (100-250Hz) — approximate
    snap.beatPower = std::clamp(r.levelBass * 0.5, 0.0, 1.0);
    snap.bassPower = std::clamp(r.levelBass * 0.5, 0.0, 1.0);

    // Mids: blend Synesthesia mid (200-2000Hz) + midhigh for QLC+ 250-3000Hz
    snap.mids = std::clamp(0.7 * r.levelMid + 0.3 * r.levelMidHigh, 0.0, 1.0);

    // Highs: blend midhigh (2-6kHz) + high (6-20kHz) for QLC+ 3-10kHz
    snap.highs = std::clamp(0.65 * r.levelMidHigh + 0.35 * r.levelHigh, 0.0, 1.0);

    // --- Triggers (Schmitt state machines) ---
    snap.triggers[0] = m_lowTrigger.update(snap.lows, dtMs);
    snap.triggers[1] = m_midTrigger.update(snap.mids, dtMs);
    snap.triggers[2] = m_highTrigger.update(snap.highs, dtMs);

    // --- Volume ---
    snap.volume.raw = std::clamp(r.levelAll, 0.0, 1.0);

    // Noise gate heuristic
    m_gateSmoothed = 0.99 * m_gateSmoothed + 0.01 * r.levelAll;
    if (m_gateSmoothed < 0.02)
        m_gateHeldMs += dtMs;
    else
        m_gateHeldMs = 0.0;
    m_gateClosed = (m_gateHeldMs >= 120.0);
    snap.noiseGateClosed = m_gateClosed;

    snap.volume.normalized = m_gateClosed ? 0.0 : snap.volume.raw;
    snap.volume.smoothed = std::clamp(r.presenceAll, 0.0, 1.0);
    snap.volume.volumeNorm = std::clamp(r.energyIntensity, 0.0, 1.0);

    snap.volumeTrigger = m_volTrigger.update(snap.volume.normalized, dtMs);

    // --- Beat / BPM ---
    bool onbeatRising = (r.onbeat > 0.5 && m_prevOnbeat <= 0.5);
    m_prevOnbeat = r.onbeat;

    snap.music.beat = onbeatRising;
    snap.music.bpm = r.bpm;
    snap.music.beatConfidence = r.bpmConfidence;

    // Beat phase synthesis: reset on beat, advance at bpm rate
    if (onbeatRising)
        m_beatPhaseSec = 0.0;
    else if (r.bpm > 0.0)
        m_beatPhaseSec += dtMs / 1000.0;

    double beatPeriodSec = (r.bpm > 0.0) ? (60.0 / r.bpm) : 1.0;
    snap.music.beatPhase = std::fmod(m_beatPhaseSec / beatPeriodSec, 1.0);

    // Bar phase from beattime (0-7)
    int beatInt = int(r.beattime);
    snap.music.barPhase = double(beatInt) + snap.music.beatPhase;

    // Downbeat detection
    bool isDownbeat = (beatInt == 0 && m_prevBeattimeInt != 0 && m_prevBeattimeInt >= 0);
    snap.downbeatFired = isDownbeat;
    m_prevBeattimeInt = beatInt;

    // Beat trigger
    TriggerState beatTs;
    beatTs.value = onbeatRising ? 1.0 : 0.0;
    beatTs.active = onbeatRising;
    beatTs.firedThisFrame = onbeatRising;
    beatTs.releasedThisFrame = false;
    snap.beatTrigger = beatTs;

    // --- Kick detection (level/presence ratio) ---
    double kickValue = 0.0;
    if (r.presenceBass > 0.01)
        kickValue = std::clamp(r.levelBass / r.presenceBass - 1.0, 0.0, 1.0);
    snap.kickTrigger = m_kickTrigger.update(kickValue, dtMs);

    // --- Features (approximate where possible, zero otherwise) ---
    snap.features.rmsDb = (r.levelAll > 0.0001) ? 20.0 * std::log10(r.levelAll) : -96.0;
    snap.features.peakDb = snap.features.rmsDb;
    snap.features.crestFactor = 1.0;

    // --- Timing ---
    snap.audioDtMs = dtMs;
    snap.brightnessFloor = 0.0;

    return snap;
}
