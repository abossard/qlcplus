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
#include "djfsm.h"
#include "showfactory.h"

#include <QTimer>
#include "qlcioplugin.h"

#include "doc.h"
#include "show.h"
#include "function.h"
#include "mastertimer.h"
#include "inputoutputmap.h"

#include <QDebug>
#include <QFileInfo>

VdjBridge::VdjBridge(QObject *parent)
    : QObject(parent)
    , m_fsm(new DjFsm(this))
    , m_showFactory(nullptr)
    , m_performFsm(new PerformFsm(this))
{
    // Engine effects run on FSM transitions (unidirectional: the FSM decides,
    // the bridge executes; effects never write back into the FSM).
    connect(m_performFsm, &PerformFsm::stateChanged,
            this, &VdjBridge::applyPerformState);
    connect(m_performFsm, &PerformFsm::activeShowChanged,
            this, &VdjBridge::applyPerformShowChange);

    // status forwarding for QML
    connect(m_performFsm, &PerformFsm::stateChanged,
            this, &VdjBridge::performStatusChanged);
    connect(m_performFsm, &PerformFsm::activeShowChanged,
            this, &VdjBridge::performStatusChanged);
}

void VdjBridge::setDoc(Doc *doc)
{
    // PerformFsm avoids an engine dependency by re-declaring the invalid-id
    // sentinel; pin the two together so they cannot drift
    Q_ASSERT(PerformFsm::InvalidShowId == Function::invalidId());

    m_doc = doc;
    if (m_doc && !m_showFactory)
    {
        m_showFactory = new ShowFactory(m_doc, this);
        // Note: show creation is driven by DJ Manager (which owns the
        // filepath dedup decision), not auto-wired from the FSM here.

        // Any mapping change (deferred creation, assignment, restore-from-Doc)
        // can resolve the Perform show — re-feed the FSM inputs.
        connect(m_showFactory, &ShowFactory::showCreatedForSong,
                this, [this](const QString &, quint32) { refreshPerformInputs(); });
        connect(m_showFactory, &ShowFactory::mappingChanged,
                this, [this](const QString &, quint32) { refreshPerformInputs(); });
    }

    // Route the active deck's authoritative BPM into the engine so the engine
    // BPM follows VirtualDJ's steady value instead of jittery beat timing.
    if (m_doc && m_fsm)
    {
        connect(m_fsm, &DjFsm::deckChanged, this, [this](int deck) {
            if (deck == m_fsm->activeDeck())
            {
                pushActiveBpm();
                // transport of the active deck feeds the Perform FSM
                m_performFsm->setDeckPlaying(m_fsm->deckAt(deck - 1).playing);
            }
        });
        connect(m_fsm, &DjFsm::activeDeckChanged, this, [this](int) {
            pushActiveBpm();
            // active deck changed → re-resolve the Perform show + transport
            refreshPerformInputs();
        });
        connect(m_fsm, &DjFsm::activeSongChanged,
                this, &VdjBridge::refreshPerformInputs);
    }
}

void VdjBridge::pushActiveBpm()
{
    if (!m_doc)
        return;
    InputOutputMap *iom = m_doc->inputOutputMap();
    if (!iom)
        return;

    const int active = m_fsm->activeDeck();
    if (active < 1)
        return;

    const DjFsm::Deck &deck = m_fsm->deckAt(active - 1);
    if (deck.playing)
    {
        // Playing: lock the engine BPM to VirtualDJ's steady value.
        const double bpm = deck.song.bpm;
        if (bpm > 0.0)
            iom->setExternalBpm(qRound(bpm));
    }
    else
    {
        // Paused: drop the engine BPM super low as a visual cue (restores the
        // old pre-lock behavior), while keeping the lock so it stays steady-low
        // instead of jittering off stray beat pulses.
        iom->setExternalBpm(m_pausedBpm);
    }
}

void VdjBridge::setPausedBpm(int bpm)
{
    m_pausedBpm = bpm;
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
    // When the telemetry plugin client is connected, suppress OS2L beats
    // to avoid double-counting (the plugin emits its own beatReceived).
    if (telemetryConnected())
        return;

    ++m_beatCount;

    if (!m_connected)
    {
        m_connected = true;
        emit connectedChanged();
    }

    emit beatReceived();
}

// ---------- VDJ Bridge plugin attachment ----------

void VdjBridge::attachVdjPlugin(QLCIOPlugin *plugin)
{
    if (m_vdjPlugin == plugin)
        return;

    if (!m_vdjPlugin.isNull())
        m_vdjPlugin->disconnect(this);

    m_vdjPlugin = plugin;
    if (m_vdjPlugin.isNull())
        return;

    // String-based connect: VdjBridge only knows the plugin via
    // QLCIOPlugin*; the custom telemetry signals live on the concrete
    // VdjBridgePlugin and are resolved through QMetaObject.
    connect(m_vdjPlugin.data(),
            SIGNAL(telemetryBeatReceived(int,qreal,qreal,bool)),
            this, SLOT(onTelemetryBeat(int,qreal,qreal,bool)));
    connect(m_vdjPlugin.data(),
            SIGNAL(deckTriggerReceived(int,QString,QVariant)),
            this, SLOT(onDeckTrigger(int,QString,QVariant)));
    connect(m_vdjPlugin.data(),
            SIGNAL(globalTriggerReceived(QString,QVariant)),
            this, SLOT(onGlobalTrigger(QString,QVariant)));
    connect(m_vdjPlugin.data(),
            SIGNAL(clientConnected()),
            this, SLOT(onTelemetryClientConnected()));
    connect(m_vdjPlugin.data(),
            SIGNAL(clientDisconnected()),
            this, SLOT(onTelemetryClientDisconnected()));

    // Status changes (Idle/Advertising/Connected) drive the QML
    // telemetryStatus property.
    connect(m_vdjPlugin.data(),
            SIGNAL(connectionStatusChanged(quint32,quint32)),
            this, SIGNAL(telemetryStatusChanged()));
}

QString VdjBridge::telemetryStatus() const
{
    if (m_vdjPlugin.isNull())
        return QStringLiteral("Idle");

    switch (m_vdjPlugin->connectionStatus(0))
    {
        case QLCIOPlugin::Connected:   return QStringLiteral("Connected");
        case QLCIOPlugin::Advertising: return QStringLiteral("Listening");
        case QLCIOPlugin::Idle:        return QStringLiteral("Idle");
    }
    return QStringLiteral("Unknown");
}

bool VdjBridge::telemetryConnected() const
{
    return !m_vdjPlugin.isNull()
        && m_vdjPlugin->connectionStatus(0) == QLCIOPlugin::Connected;
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
    applyDeckTrigger(deckIndex, trigger, value);
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
    else if (trigger == "get_decks")
    {
        // VirtualDJ reports the real number of decks. Decks beyond this are
        // phantom mirrors of the first decks (VDJ streams cloned data for them),
        // so drive the tracked deck count from it — this is how the real
        // DMXDesktop avoids showing cloned decks. Out-of-range values are
        // clamped by DjFsm::setDeckCount; non-numeric values are ignored.
        bool ok = false;
        const int n = value.toInt(&ok);
        if (ok)
            m_fsm->setDeckCount(n);
    }
    else if (trigger == "masterdeck")
    {
        // VirtualDJ reports masterdeck as on/off (not a deck index): on => the
        // master/active deck is deck 1, off => deck 2. (2-deck topology.)
        const QString sv = value.toString().toLower();
        int deck1 = 0;
        if (sv == "on" || sv == "true" || sv == "1")
            deck1 = 1;
        else if (sv == "off" || sv == "false" || sv == "0")
            deck1 = 2;
        else
            deck1 = value.toInt(); // fall back to a numeric deck index if sent

        if (deck1 >= 1 && deck1 <= 4 && (deck1 - 1) != m_masterDeck)
        {
            m_masterDeck = deck1 - 1; // 0-based for the auto-play path
            m_fsm->onMasterDeck(deck1); // FSM uses 1-based
            emit masterDeckChanged();
        }
    }

    if (changed)
        emit globalMixerChanged();
}

void VdjBridge::onTelemetryClientConnected()
{
    qDebug() << "[VdjBridge] Telemetry client connected — telemetry beats active, OS2L beats suppressed";
}

void VdjBridge::onTelemetryClientDisconnected()
{
    qDebug() << "[VdjBridge] Telemetry client disconnected — OS2L beats resumed";
    // The FSM holds all deck state — reset it.
    m_fsm->onDisconnected();
    // Release the authoritative BPM lock so the engine resumes normal behavior.
    if (m_doc && m_doc->inputOutputMap())
        m_doc->inputOutputMap()->clearExternalBpm();
    // Reset global state
    m_masterDeck = -1;
    m_masterVolume = 0.0;
    m_crossfader = 0.0;
    m_headphoneVolume = 0.0;
    m_masterVu = 0.0;
    emit masterDeckChanged();
    emit globalMixerChanged();
}

void VdjBridge::applyDeckTrigger(int deckIndex, const QString &trigger, const QVariant &value)
{
    // The DjFsm is the single source of truth for all per-deck VDJ data.
    m_fsm->onDeckTrigger(deckIndex + 1, trigger, value);

    // Perform mode drives Show playback off the FSM's active deck.
    driveActiveShow(deckIndex, trigger, value);
}

void VdjBridge::driveActiveShow(int deckIndex, const QString &trigger, const QVariant &value)
{
    Q_UNUSED(value)
    if (!m_fsm || deckIndex + 1 != m_fsm->activeDeck())
        return;

    // Data plane only: per-frame position sync while Live. All control
    // decisions (start/pause/handover) flow through the PerformFsm via
    // the DjFsm signal connections in setDoc().
    if (trigger == QLatin1String("get_time elapsed absolute")
        && m_performFsm->state() == PerformFsm::PerformState::Live)
    {
        Show *show = lookupShow(m_adoptedShowId);
        if (show && show->isRunning())
        {
            const int elapsed = m_fsm->deckAt(deckIndex).elapsedMs;
            show->setExternalElapsedTime(static_cast<quint32>(qMax(0, elapsed)));
        }
    }
}

void VdjBridge::setPerformMode(bool on)
{
    if (performMode() == on)
        return;

    // resolve inputs BEFORE enabling so Idle -> Live happens in one
    // transition when VDJ is already playing
    refreshPerformInputs();
    m_performFsm->setPerformEnabled(on);
    emit performModeChanged();
}

QString VdjBridge::performShowName() const
{
    Show *show = lookupShow(m_performFsm->activeShowId());
    return show ? show->name() : QString();
}

void VdjBridge::refreshPerformInputs()
{
    if (!m_fsm)
        return;

    const int active = m_fsm->activeDeck();
    const DjFsm::Deck *deck = (active >= 1) ? &m_fsm->deckAt(active - 1) : nullptr;

    quint32 showId = PerformFsm::InvalidShowId;
    if (deck && !deck->song.filepath.isEmpty() && m_showFactory)
    {
        const quint32 id = m_showFactory->showIdForFilepath(deck->song.filepath);
        if (id != Function::invalidId())
            showId = id;
    }

    // transport first so Armed -> Live happens directly on show resolution
    m_performFsm->setDeckPlaying(deck ? deck->playing : false);
    m_performFsm->setActiveShow(showId);
}

Show *VdjBridge::lookupShow(quint32 showId) const
{
    if (!m_doc || showId == PerformFsm::InvalidShowId)
        return nullptr;
    return qobject_cast<Show*>(m_doc->function(showId));
}

void VdjBridge::applyPerformState(PerformFsm::PerformState state)
{
    switch (state)
    {
        case PerformFsm::PerformState::Idle:
            releaseAdoptedShow();
            // safety net: pause any OTHER external-sync show still running
            // (e.g. left over from an older workspace/session), so leaving
            // Perform always stops VDJ-driven lighting completely
            if (m_doc)
            {
                for (Function *f : m_doc->functionsByType(Function::ShowType))
                {
                    Show *show = qobject_cast<Show*>(f);
                    if (show && show->isRunning() && !show->isPaused()
                        && show->syncSource() == 1)
                    {
                        show->setPause(true);
                    }
                }
            }
            break;
        case PerformFsm::PerformState::Armed:
            releaseAdoptedShow();
            break;
        case PerformFsm::PerformState::Live:
            adoptActiveShow();
            startAdoptedShow();
            break;
        case PerformFsm::PerformState::Suspended:
            // "VDJ stops -> the progression stops": adopted show freezes
            adoptActiveShow();
            pauseAdoptedShow();
            break;
    }
}

void VdjBridge::applyPerformShowChange(quint32 showId)
{
    if (m_adoptedShowId == showId)
        return;

    // deck handover: release the old show (pause + restore sync source),
    // then re-apply the current state to adopt/start the new one
    releaseAdoptedShow();
    applyPerformState(m_performFsm->state());
}

void VdjBridge::adoptActiveShow()
{
    const quint32 id = m_performFsm->activeShowId();
    if (m_adoptedShowId == id)
        return;

    releaseAdoptedShow();

    Show *show = lookupShow(id);
    if (show == nullptr)
        return;

    m_adoptedShowId = id;
    m_adoptedPrevSyncSource = show->syncSource();
    show->setSyncSource(1); // ShowRunner::External — VDJ owns the playhead
    qDebug() << "[VdjBridge] Perform: adopted show" << show->name();
}

void VdjBridge::releaseAdoptedShow()
{
    Show *show = lookupShow(m_adoptedShowId);
    if (show != nullptr)
    {
        if (show->isRunning() && !show->isPaused())
            show->setPause(true);
        show->setSyncSource(m_adoptedPrevSyncSource);
        qDebug() << "[VdjBridge] Perform: released show" << show->name();
    }
    m_adoptedShowId = PerformFsm::InvalidShowId;
    m_adoptedPrevSyncSource = 0;
}

void VdjBridge::startAdoptedShow()
{
    Show *show = lookupShow(m_adoptedShowId);
    if (show == nullptr || m_doc == nullptr || m_fsm == nullptr)
        return;

    const int active = m_fsm->activeDeck();
    const int elapsed = (active >= 1) ? m_fsm->deckAt(active - 1).elapsedMs : 0;

    if (!show->isRunning())
    {
        qDebug() << "[VdjBridge] Perform: starting show" << show->name();
        show->start(m_doc->masterTimer(), FunctionParent::master());
    }
    else if (show->isPaused())
    {
        show->setPause(false);
    }
    show->setExternalElapsedTime(static_cast<quint32>(qMax(0, elapsed)));
}

void VdjBridge::pauseAdoptedShow()
{
    Show *show = lookupShow(m_adoptedShowId);
    if (show && show->isRunning() && !show->isPaused())
        show->setPause(true);
}
