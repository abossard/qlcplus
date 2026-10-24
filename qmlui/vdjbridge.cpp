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

#include "qlcioplugin.h"

#include "doc.h"
#include "show.h"
#include "showrunner.h"
#include "function.h"
#include "mastertimer.h"
#include "inputoutputmap.h"

#include <QDebug>
#include <QScopedValueRollback>

/** Engine tempo while the active deck is paused: a visible crawl */
static const int kPausedBpm = 1;

VdjBridge::VdjBridge(QObject *parent)
    : QObject(parent)
    , m_fsm(new DjFsm(this))
    , m_showFactory(nullptr)
    , m_performFsm(new PerformFsm(this))
{
    // the FSM decides, the bridge executes; effects never write back into the FSM
    connect(m_performFsm, &PerformFsm::stateChanged, this, &VdjBridge::applyPerformPlan);
    connect(m_performFsm, &PerformFsm::stateChanged, this, &VdjBridge::performStatusChanged);
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

        // any mapping change can resolve the Perform show
        connect(m_showFactory, &ShowFactory::showCreatedForSong, this, &VdjBridge::refreshPerformInputs);
        connect(m_showFactory, &ShowFactory::mappingChanged, this, &VdjBridge::refreshPerformInputs);
    }

    if (m_doc && m_fsm)
    {
        connect(m_fsm, &DjFsm::deckChanged, this, [this](int deck) {
            if (deck != m_fsm->activeDeck())
                return;
            pushActiveBpm();
            refreshPerformInputs();
        });
        connect(m_fsm, &DjFsm::activeDeckChanged, this, [this]() {
            pushActiveBpm();
            refreshPerformInputs();
        });
    }
}

void VdjBridge::pushActiveBpm()
{
    InputOutputMap *iom = m_doc ? m_doc->inputOutputMap() : nullptr;
    // VirtualDJ's steady tempo replaces only the plugin's own jittery beat estimate
    if (!iom || iom->beatGeneratorType() != InputOutputMap::Plugin)
        return;

    const int active = m_fsm->activeDeck();
    if (active < 1)
        return;

    const DjFsm::Deck &deck = m_fsm->deckAt(active - 1);
    // an emptied deck keeps the engine where it was
    if (!deck.song.isValid())
        return;
    if (deck.song.bpm > 0)
        m_lastSongBpm = qRound(deck.song.bpm);
    const int bpm = deck.playing ? qRound(deck.song.bpm) : kPausedBpm;
    if (bpm > 0)
        iom->setExternalBpm(bpm);
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
            SIGNAL(deckTriggerReceived(int,QString,QVariant)),
            this, SLOT(onDeckTrigger(int,QString,QVariant)));
    connect(m_vdjPlugin.data(),
            SIGNAL(globalTriggerReceived(QString,QVariant)),
            this, SLOT(onGlobalTrigger(QString,QVariant)));
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

void VdjBridge::onDeckTrigger(int deckIndex, const QString &trigger, const QVariant &value)
{
    if (deckIndex < 0 || deckIndex >= DjFsm::MaxDecks)
        return;
    m_fsm->onDeckTrigger(deckIndex + 1, trigger, value);

    if (trigger != QLatin1String("get_time elapsed absolute") || deckIndex + 1 != m_fsm->activeDeck())
        return;
    // data plane: the adopted Show follows the deck's clock
    if (Show *show = lookupShow(m_performFsm->activeShowId()))
        pushPosition(show);
    refreshPerformInputs();
}

void VdjBridge::onGlobalTrigger(const QString &trigger, const QVariant &value)
{
    if (trigger != QLatin1String("get_decks"))
        return;
    // VirtualDJ reports the real number of decks. Decks beyond this are
    // phantom mirrors of its default deck, so drive the tracked deck count
    // from it; DjFsm::setDeckCount clamps, non-numeric values are ignored.
    bool ok = false;
    const int n = value.toInt(&ok);
    if (ok)
        m_fsm->setDeckCount(n);
}

void VdjBridge::onTelemetryClientDisconnected()
{
    qDebug() << "[VdjBridge] Telemetry client disconnected";
    InputOutputMap *iom = m_doc ? m_doc->inputOutputMap() : nullptr;
    if (iom && iom->beatGeneratorType() == InputOutputMap::Plugin)
    {
        // the last song tempo outlives the lock, the paused crawl does not
        if (m_lastSongBpm > 0)
            iom->setExternalBpm(m_lastSongBpm);
        iom->clearExternalBpm();
    }
    m_fsm->onDisconnected();
}

void VdjBridge::onAdoptedShowStopped(quint32 showId)
{
    if (showId == m_appliedShowId)
        applyPerformPlan();
}

void VdjBridge::onAdoptedShowFinished(quint32 showId)
{
    // the runner is the one owner of "the Show ended": from here the deck
    // position decides, a move below it plays the Show again
    if (showId != m_appliedShowId)
        return;
    m_endedAt = deckPosition().value_or(0);
    refreshPerformInputs();
}

// ---------- Perform ----------

void VdjBridge::setPerformMode(bool on)
{
    if (performMode() == on)
        return;

    // resolve inputs BEFORE enabling so Idle -> Live happens in one transition
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
    if (m_endedAt && deckPosition().value_or(0) < *m_endedAt)
        m_endedAt.reset();
    m_performFsm->setInputs(performInputs());
}

PerformFsm::Inputs VdjBridge::performInputs() const
{
    PerformFsm::Inputs inputs;
    const int active = m_fsm->activeDeck();
    if (active < 1 || !m_showFactory)
        return inputs;
    const DjFsm::Deck &deck = m_fsm->deckAt(active - 1);
    if (!deck.song.isValid())
        return inputs;
    Show *show = lookupShow(m_showFactory->showIdForFilepath(deck.song.filepath));
    if (!show)
        return inputs;

    inputs.showId = show->id();
    inputs.playing = deck.playing;
    inputs.ended = m_endedAt.has_value();
    return inputs;
}

PerformFsm::ShowFacts VdjBridge::showFacts(quint32 showId) const
{
    Show *show = lookupShow(showId);
    if (!show)
        return {};
    PerformFsm::ShowFacts facts;
    facts.running = show->isRunning();
    facts.paused = show->isPaused();
    facts.startQueued = m_doc->masterTimer()->isStartQueued(show) && !show->stopped();
    return facts;
}

std::optional<quint32> VdjBridge::deckPosition() const
{
    const int active = m_fsm->activeDeck();
    if (active < 1)
        return std::nullopt;
    const DjFsm::Deck &deck = m_fsm->deckAt(active - 1);
    // a new song's first sample is still ahead: the runner waits for it
    if (!deck.song.isValid() || deck.positionStale)
        return std::nullopt;
    return quint32(qMax(0, deck.elapsedMs));
}

void VdjBridge::pushPosition(Show *show) const
{
    if (const std::optional<quint32> position = deckPosition())
        show->setExternalElapsedTime(*position);
}

Show *VdjBridge::lookupShow(quint32 showId) const
{
    if (!m_doc || showId == PerformFsm::InvalidShowId)
        return nullptr;
    return qobject_cast<Show*>(m_doc->function(showId));
}

void VdjBridge::applyPerformPlan()
{
    // a step that re-enters (a listener toggling Perform) plans after this
    // plan completed, against the state it reached
    if (m_planning)
    {
        m_replanPending = true;
        return;
    }
    QScopedValueRollback<bool> planning(m_planning, true);
    do
    {
        m_replanPending = false;
        const PerformFsm::PerformState state = m_performFsm->state();
        const quint32 showId = m_performFsm->activeShowId();
        const QList<PerformFsm::Step> steps =
            PerformFsm::plan(m_appliedState, m_appliedShowId, state, showId, showFacts(showId));
        m_appliedState = state;
        m_appliedShowId = showId;
        for (const PerformFsm::Step &step : steps)
            perform(step);
    } while (m_replanPending);
}

void VdjBridge::perform(const PerformFsm::Step &step)
{
    using Effect = PerformFsm::Effect;
    Show *show = lookupShow(step.showId);
    switch (step.effect)
    {
        case Effect::ResolveRecorder:
            emit performShowResolved(step.showId);
        break;
        case Effect::ReleaseRecorder:
            emit performReleased();
        break;
        case Effect::SelectShow:
            emit performShowSelected(int(step.showId));
        break;
        case Effect::StopShow:
            // idempotent, and the only way to reach a start the timer is taking off its queue
            if (show)
                show->stop(FunctionParent::master());
        break;
        case Effect::ReleaseShow:
            if (show)
            {
                disconnect(show, nullptr, this, nullptr);
                show->setPerformAudioSuppressed(false);
                show->setSyncSource(ShowRunner::Autonomous);
                qDebug() << "[VdjBridge] Perform: released show" << show->name();
            }
            m_endedAt.reset();
        break;
        case Effect::AdoptShow:
            if (show)
            {
                show->setPerformAudioSuppressed(true);
                show->setSyncSource(ShowRunner::External);
                pushPosition(show);
                connect(show, QOverload<quint32>::of(&Function::stopped), this, &VdjBridge::onAdoptedShowStopped,
                        Qt::QueuedConnection);
                const quint32 id = show->id();
                connect(show, &Show::showFinished, this, [this, id]() { onAdoptedShowFinished(id); },
                        Qt::QueuedConnection);
                qDebug() << "[VdjBridge] Perform: adopted show" << show->name();
            }
            m_endedAt.reset();
        break;
        case Effect::StartShow:
            if (show && m_doc)
            {
                // the position before the start, so its first tick runs there,
                // and after it, as the first sample of this Play
                pushPosition(show);
                show->start(m_doc->masterTimer(), FunctionParent::master());
                pushPosition(show);
            }
        break;
        case Effect::ResumeShow:
            if (show)
            {
                pushPosition(show);
                show->setPause(false);
            }
        break;
        case Effect::PauseShow:
            if (show)
                show->setPause(true);
        break;
    }
}
