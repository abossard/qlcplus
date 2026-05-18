/*
  Q Light Controller Plus
  vdjbridge.cpp

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

#include "vdjbridge.h"
#include "vdjdeckmodel.h"
#include "vdjtelemetryclient.h"
#include "vdjbonjour.h"
#include "songloadtracker.h"
#include "showfactory.h"
#include "qlcioplugin.h"

#include "doc.h"

#include <QDebug>
#include <QFileInfo>

VdjBridge::VdjBridge(QObject *parent)
    : QObject(parent)
    , m_tracker(new SongLoadTracker(this))
    , m_showFactory(nullptr)
{
    for (int i = 0; i < 4; ++i)
        m_deckModels[i] = new VdjDeckModel(i + 1, this);
}

void VdjBridge::setDoc(Doc *doc)
{
    m_doc = doc;
    if (m_doc && !m_showFactory)
    {
        m_showFactory = new ShowFactory(m_doc, this);
        connect(m_tracker, &SongLoadTracker::songReady,
                m_showFactory, &ShowFactory::createShowForSong);
    }
}

// ---------- OS2L attachment (unchanged) ----------

void VdjBridge::attachOS2LPlugin(QLCIOPlugin *plugin)
{
    if (m_plugin == plugin)
        return;

    if (!m_plugin.isNull())
        m_plugin->disconnect(this);

    m_plugin = plugin;
    if (m_plugin.isNull())
    {
        if (m_connected)
        {
            m_connected = false;
            emit connectedChanged();
        }
        return;
    }

    connect(m_plugin.data(), SIGNAL(beatReceived()),
            this, SLOT(onBeat()));
    connect(m_plugin.data(), SIGNAL(connectionStatusChanged(quint32,quint32)),
            this, SLOT(refreshConnectionStatus()));

    refreshConnectionStatus();
}

void VdjBridge::refreshConnectionStatus()
{
    bool now = false;
    if (!m_plugin.isNull())
        now = (m_plugin->connectionStatus(0) == QLCIOPlugin::Connected);

    if (now != m_connected)
    {
        m_connected = now;
        emit connectedChanged();
    }
}

void VdjBridge::onBeat()
{
    // When telemetry client is connected, suppress OS2L beats to avoid double-count
    if (m_telemetry && m_telemetry->status() == VdjTelemetryClient::ClientConnected)
        return;

    ++m_beatCount;

    if (!m_connected)
    {
        m_connected = true;
        emit connectedChanged();
    }

    emit beatReceived();
}

// ---------- Telemetry ----------

void VdjBridge::startTelemetry(quint16 port)
{
    if (port == 0)
        return;

    if (!m_telemetry)
    {
        m_telemetry = new VdjTelemetryClient(this);

        connect(m_telemetry, &VdjTelemetryClient::statusChanged,
                this, &VdjBridge::telemetryStatusChanged);
        connect(m_telemetry, &VdjTelemetryClient::beatReceived,
                this, &VdjBridge::onTelemetryBeat);
        connect(m_telemetry, &VdjTelemetryClient::deckTriggerReceived,
                this, &VdjBridge::onDeckTrigger);
        connect(m_telemetry, &VdjTelemetryClient::globalTriggerReceived,
                this, &VdjBridge::onGlobalTrigger);
        connect(m_telemetry, &VdjTelemetryClient::clientConnected,
                this, &VdjBridge::onTelemetryClientConnected);
        connect(m_telemetry, &VdjTelemetryClient::clientDisconnected,
                this, &VdjBridge::onTelemetryClientDisconnected);
    }

    m_telemetry->start(port);
    m_telemetryPort = port;

    // Bonjour registration for VDJ auto-discovery.
    if (!m_bonjour)
        m_bonjour = new VdjBonjour(this);
    registerBonjour();

    // VDJ caches Bonjour services and doesn't re-discover after QLC+ restarts.
    // Cycle the registration every 10s while no client is connected to force
    // mDNS announcements that trigger VDJ's service browser.
    if (!m_discoveryTimer)
    {
        m_discoveryTimer = new QTimer(this);
        m_discoveryTimer->setInterval(10000);
        connect(m_discoveryTimer, &QTimer::timeout, this, &VdjBridge::cycleBonjour);
    }
    m_discoveryTimer->start();
}

void VdjBridge::stopTelemetry()
{
    if (m_discoveryTimer)
        m_discoveryTimer->stop();
    if (m_bonjour)
        m_bonjour->unregisterService();
    if (m_telemetry)
        m_telemetry->stop();
}

void VdjBridge::registerBonjour()
{
    if (m_bonjour && m_telemetryPort > 0)
        m_bonjour->registerService("QLC+ Telemetry", m_telemetryPort);
}

void VdjBridge::cycleBonjour()
{
    // Stop cycling once a client is connected
    if (m_telemetry && m_telemetry->status() == VdjTelemetryClient::ClientConnected)
    {
        m_discoveryTimer->stop();
        return;
    }

    // Unregister and re-register to force mDNS announcement
    if (m_bonjour)
    {
        m_bonjour->unregisterService();
        // Short delay then re-register (use single-shot timer)
        QTimer::singleShot(500, this, &VdjBridge::registerBonjour);
    }
}

QString VdjBridge::telemetryStatus() const
{
    if (!m_telemetry)
        return QStringLiteral("Idle");

    switch (m_telemetry->status())
    {
        case VdjTelemetryClient::Idle:            return QStringLiteral("Idle");
        case VdjTelemetryClient::Listening:       return QStringLiteral("Listening");
        case VdjTelemetryClient::PortInUse:       return QStringLiteral("PortInUse");
        case VdjTelemetryClient::ClientConnected: return QStringLiteral("Connected");
    }
    return QStringLiteral("Unknown");
}

bool VdjBridge::telemetryConnected() const
{
    return m_telemetry &&
           m_telemetry->status() == VdjTelemetryClient::ClientConnected;
}

QList<QObject*> VdjBridge::decks() const
{
    QList<QObject*> list;
    for (int i = 0; i < 4; ++i)
        list.append(m_deckModels[i]);
    return list;
}

// ---------- Telemetry signal handlers ----------

void VdjBridge::onTelemetryBeat(int /*pos*/, qreal /*bpm*/, qreal /*strength*/, bool /*change*/)
{
    ++m_beatCount;
    emit beatReceived();
}

void VdjBridge::onDeckTrigger(int deckIndex, const QString &trigger, const QVariant &value)
{
    if (deckIndex < 0 || deckIndex >= 4)
        return;
    applyDeckTrigger(m_deckModels[deckIndex], deckIndex, trigger, value);
}

void VdjBridge::onGlobalTrigger(const QString &trigger, const QVariant &value)
{
    bool changed = false;

    if (trigger == "master_volume")
    {
        qreal v = value.toDouble();
        if (!qFuzzyCompare(m_masterVolume, v)) { m_masterVolume = v; changed = true; }
    }
    else if (trigger == "crossfader")
    {
        qreal v = value.toDouble();
        if (!qFuzzyCompare(m_crossfader, v)) { m_crossfader = v; changed = true; }
    }
    else if (trigger == "headphone_volume")
    {
        qreal v = value.toDouble();
        if (!qFuzzyCompare(m_headphoneVolume, v)) { m_headphoneVolume = v; changed = true; }
    }
    else if (trigger == "get_vu_meter")
    {
        qreal v = value.toDouble();
        if (!qFuzzyCompare(m_masterVu, v)) { m_masterVu = v; changed = true; }
    }
    else if (trigger == "masterdeck")
    {
        int v = value.toInt() - 1; // VDJ is 1-based, we store 0-based
        if (v >= 0 && v < 4 && v != m_masterDeck)
        {
            m_masterDeck = v;
            m_tracker->onMasterDeck(value.toInt()); // tracker uses 1-based
            emit masterDeckChanged();
        }
    }

    if (changed)
        emit globalMixerChanged();
}

void VdjBridge::onTelemetryClientConnected()
{
    qDebug() << "[VdjBridge] Telemetry client connected — telemetry beats active, OS2L beats suppressed";
    // Stop Bonjour cycling — we have a client
    if (m_discoveryTimer)
        m_discoveryTimer->stop();
}

void VdjBridge::onTelemetryClientDisconnected()
{
    qDebug() << "[VdjBridge] Telemetry client disconnected — OS2L beats resumed";
    // Restart Bonjour cycling to attract VDJ again
    if (m_discoveryTimer)
        m_discoveryTimer->start();
    // Reset deck models
    for (int i = 0; i < 4; ++i)
        m_deckModels[i]->reset();
    // Reset FSM tracker
    m_tracker->onDisconnected();
    // Reset global state
    m_masterDeck = 0;
    m_masterVolume = 0.0;
    m_crossfader = 0.0;
    m_headphoneVolume = 0.0;
    m_masterVu = 0.0;
    emit masterDeckChanged();
    emit globalMixerChanged();
}

void VdjBridge::applyDeckTrigger(VdjDeckModel *deck, int deckIndex,
                                 const QString &trigger, const QVariant &value)
{
    // --- Route to FSM tracker (1-based deck) ---
    m_tracker->onTrigger(deckIndex + 1, trigger, value);

    // --- Route to deck model for UI state ---
    if (trigger == "get_filepath")       { deck->setFilepath(value.toString()); }
    else if (trigger == "get_title")     { deck->setTitle(value.toString()); }
    else if (trigger == "get_artist")         { deck->setArtist(value.toString()); }
    else if (trigger == "get_title_artist")   { deck->setTitleArtist(value.toString()); }
    else if (trigger == "get_album")          { deck->setAlbum(value.toString()); }
    else if (trigger == "get_genre")          { deck->setGenre(value.toString()); }
    else if (trigger == "get_key")            { deck->setKey(value.toString()); }
    else if (trigger == "get_bpm")            { deck->setBpm(value.toDouble()); }
    else if (trigger == "get_firstbeat")      { deck->setFirstBeat(value.toDouble()); }
    else if (trigger == "get_time total")     { deck->setTimeTotal(value.toDouble()); }
    else if (trigger == "loaded")             { deck->setLoaded(value.toString() == "on"); }
    else if (trigger == "play")               { deck->setPlaying(value.toString() == "on"); }
    else if (trigger == "volume")             { deck->setVolume(value.toDouble()); }

    // Continuous
    else if (trigger == "get_position")              { deck->setPosition(value.toDouble()); }
    else if (trigger == "get_time")                  { deck->setTimeRemaining(value.toDouble()); }
    else if (trigger == "get_time elapsed absolute") { deck->setTimeElapsed(value.toDouble()); }
    else if (trigger == "get_beatpos")               { deck->setBeatPos(value.toDouble()); }
    else if (trigger == "get_vu_meter")              { deck->setVu(value.toDouble()); }
    else if (trigger == "level")                     { deck->setLevel(value.toDouble()); }

    // EQ
    else if (trigger == "eq_high") { deck->setEqHigh(value.toDouble()); }
    else if (trigger == "eq_med")  { deck->setEqMed(value.toDouble()); }
    else if (trigger == "eq_low")  { deck->setEqLow(value.toDouble()); }
    else if (trigger == "gain")    { deck->setGain(value.toDouble()); }

    // Loop
    else if (trigger == "loop")     { deck->setLooping(value.toBool()); }
    else if (trigger == "get_loop") { deck->setLoopLength(value.toDouble()); }
}
