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

/** Engine tempo while the active deck is paused and no deck plays: a visible crawl */
static const int kPausedBpm = 1;

static_assert(PerformFsm::MaxDecks == DjFsm::MaxDecks, "PerformFsm and DjFsm track the same decks");

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
        // queued: App reads the song choices after the Engine, before the event loop runs
        connect(m_doc, &Doc::loaded, this, &VdjBridge::refreshPerformInputs, Qt::QueuedConnection);
    }

    if (m_doc && m_fsm)
    {
        connect(m_fsm, &DjFsm::deckChanged, this, [this]() {
            pushActiveBpm();
            refreshPerformInputs();
        });
        connect(m_fsm, &DjFsm::activeDeckChanged, this, [this]() {
            pushActiveBpm();
            refreshPerformInputs();
        });
        // data plane: a driven Show's master intensity is its deck's mix weight
        connect(m_fsm, &DjFsm::weightChanged, this, [this](int deck) {
            for (const PerformFsm::Drive &drive : m_performFsm->snapshot().drives)
                if (drive.deck == deck)
                    if (Show *show = lookupShow(drive.showId))
                        pushWeight(show, deck);
        });
    }
}

void VdjBridge::pushActiveBpm()
{
    InputOutputMap *iom = m_doc ? m_doc->inputOutputMap() : nullptr;
    // VirtualDJ's steady tempo replaces only the plugin's own jittery beat estimate
    if (!iom || iom->beatGeneratorType() != InputOutputMap::Plugin)
        return;

    // a disconnect leaves VirtualDJ's tempo behind: the decks it empties lead no more
    if (m_holdInputs)
        return;

    // a known tempo leads: the active deck's song while it plays, else a playing
    // deck's, whose Show plays on; the crawl only while nothing plays
    const int active = m_fsm->activeDeck();
    const DjFsm::Deck *activeDeck = active >= 1 ? &m_fsm->deckAt(active - 1) : nullptr;
    const auto leads = [](const DjFsm::Deck &deck) {
        return deck.playing && deck.song.isValid() && deck.song.bpm > 0;
    };
    const DjFsm::Deck *lead = activeDeck && leads(*activeDeck) ? activeDeck : nullptr;
    bool playing = false;
    for (int deck = 1; deck <= m_fsm->deckCount(); deck++)
    {
        const DjFsm::Deck &other = m_fsm->deckAt(deck - 1);
        playing = playing || (other.playing && other.song.isValid());
        if (!lead && leads(other))
            lead = &other;
    }
    if (lead)
    {
        m_lastSongBpm = qRound(lead->song.bpm);
        iom->setExternalBpm(m_lastSongBpm);
        return;
    }
    // a song of unknown tempo plays, or an emptied deck: the engine stays where it was
    if (playing || !activeDeck || !activeDeck->song.isValid())
        return;
    if (activeDeck->song.bpm > 0)
        m_lastSongBpm = qRound(activeDeck->song.bpm);
    iom->setExternalBpm(kPausedBpm);
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
            SIGNAL(tickEnded()),
            this, SLOT(onTickEnded()));
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
    m_fsm->onDeckTrigger(deckIndex + 1, trigger, value);
}

void VdjBridge::onTickEnded()
{
    m_fsm->onTickEnded();
    // data plane: every driven Show follows its deck's clock, one sample
    // per write, after the write's song change
    for (const PerformFsm::Drive &drive : m_performFsm->snapshot().drives)
        if (Show *show = lookupShow(drive.showId))
            pushPosition(show, drive.deck);
    refreshPerformInputs();
}

void VdjBridge::onGlobalTrigger(const QString &trigger, const QVariant &value)
{
    // every global trigger belongs to the write under way, get_decks too
    m_fsm->onGlobalTrigger(trigger, value);
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
    m_lastSongBpm = 0;
    {
        // the FSM empties the active deck first, then deck by deck: one fact for Perform
        QScopedValueRollback<bool> hold(m_holdInputs, true);
        m_fsm->onDisconnected();
    }
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
    // a half-read workspace resolves no Show: the load's end refreshes once
    if (m_doc && m_doc->loadStatus() == Doc::Loading)
        return;
    if (m_holdInputs)
        return;
    // a Doc tearing down deletes its timer before its Shows: they go with it
    if (m_doc && !m_doc->masterTimer())
        return;
    // every deck's song and its Show as the FSM's one fact
    PerformFsm::Inputs inputs;
    inputs.activeDeck = m_fsm->activeDeck();
    for (int deck = 1; deck <= PerformFsm::MaxDecks; deck++)
    {
        std::optional<Ended> &ended = m_ended[deck - 1];
        if (ended && deckPosition(deck).value_or(0) < ended->positionMs)
            ended.reset();
        const DjFsm::Deck &state = m_fsm->deckAt(deck - 1);
        Show *show = m_showFactory && state.song.isValid()
                         ? lookupShow(m_showFactory->showIdForFilepath(state.song.filepath))
                         : nullptr;
        if (!show)
            continue;
        PerformFsm::DeckInputs &in = inputs.decks[deck - 1];
        in.showId = show->id();
        in.playing = state.playing;
        in.ended = ended && ended->showId == in.showId;
    }
    m_performFsm->setInputs(inputs);
}

std::optional<quint32> VdjBridge::deckPosition(int deck) const
{
    if (deck < 1)
        return std::nullopt;
    const DjFsm::Deck &state = m_fsm->deckAt(deck - 1);
    // a new song's first sample is still ahead: the runner waits for it
    if (!state.song.isValid() || state.positionStale)
        return std::nullopt;
    return quint32(qMax(0, state.elapsedMs));
}

void VdjBridge::pushPosition(Show *show, int deck) const
{
    if (const std::optional<quint32> position = deckPosition(deck))
        show->setExternalElapsedTime(*position);
}

void VdjBridge::pushWeight(Show *show, int deck) const
{
    show->setMasterIntensity(m_fsm->deckAt(deck - 1).weight);
}

const PerformFsm::Drive *VdjBridge::appliedDrive(quint32 showId) const
{
    for (const PerformFsm::Drive &drive : m_applied.drives)
        if (drive.showId == showId)
            return &drive;
    return nullptr;
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
        const PerformFsm::Snapshot next = m_performFsm->snapshot();
        QHash<quint32, PerformFsm::ShowFacts> facts;
        for (const PerformFsm::Drive &drive : next.drives)
        {
            if (Show *show = lookupShow(drive.showId))
                facts.insert(drive.showId, { show->isRunning(), show->isPaused(),
                                             m_doc->masterTimer()->isStartQueued(show) && !show->stopped() });
        }
        const QList<PerformFsm::Step> steps = PerformFsm::plan(m_applied, next, facts);
        m_applied = next;
        for (const PerformFsm::Step &step : steps)
            perform(step);
    } while (m_replanPending);
}

void VdjBridge::perform(const PerformFsm::Step &step)
{
    using Effect = PerformFsm::Effect;
    Show *show = lookupShow(step.showId);
    // the deck that drives the Show from here: the plan's target
    const PerformFsm::Drive *drive = appliedDrive(step.showId);
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
                show->setMasterIntensity(1.0);
                show->setDominant(true);
                qDebug() << "[VdjBridge] Perform: released show" << show->name();
            }
            for (std::optional<Ended> &ended : m_ended)
                if (ended && ended->showId == step.showId)
                    ended.reset();
            m_finishedElsewhere.remove(step.showId);
        break;
        case Effect::DemoteShow:
            if (show)
                show->setDominant(false);
        break;
        case Effect::PromoteShow:
            if (show)
                show->setDominant(true);
        break;
        case Effect::AdoptShow:
            if (show && drive)
            {
                // held: a set's clip playing it lets it go
                if (show->isRunning() && show->stopped() == false)
                    show->start(m_doc->masterTimer(), FunctionParent::master());
                show->setPerformAudioSuppressed(true);
                show->setSyncSource(ShowRunner::External);
                show->setDominant(drive->dominant);
                pushWeight(show, drive->deck);
                pushPosition(show, drive->deck);
                const quint32 id = show->id();
                // a run its runner ended: the finish came first, so the deck reads Ended here
                connect(show, QOverload<quint32>::of(&Function::stopped), this, [this, id]() {
                    if (!appliedDrive(id))
                        return;
                    refreshPerformInputs();
                    applyPerformPlan();
                }, Qt::QueuedConnection);
                // the runner is the one owner of "the Show ended": from here its deck's
                // position decides, a move below it plays the Show again; a finish
                // before the song's first sample is the stale clock's: the queued
                // stop re-plans Live and the start waits for the sample
                connect(show, QOverload<quint32>::of(&Function::running), this, [this, id]() {
                    m_finishedElsewhere.remove(id);
                }, Qt::QueuedConnection);
                // the Show forwards its runner's finish to the GUI thread itself: taken
                // there at once, it comes in the timer thread's order, before the stop
                // it caused and before a next run starts; the plan waits for the loop
                connect(show, &Show::showFinished, this, [this, id]() {
                    if (m_finishedElsewhere.contains(id))
                        return;
                    const PerformFsm::Drive *finished = appliedDrive(id);
                    const std::optional<quint32> position = finished ? deckPosition(finished->deck) : std::nullopt;
                    if (!position)
                        return;
                    m_ended[finished->deck - 1] = Ended{ id, *position };
                    QMetaObject::invokeMethod(this, [this]() { refreshPerformInputs(); }, Qt::QueuedConnection);
                });
                qDebug() << "[VdjBridge] Perform: adopted show" << show->name() << "on deck" << drive->deck;
            }
            if (drive)
                m_ended[drive->deck - 1].reset();
        break;
        case Effect::FollowDeck:
            if (show && drive)
            {
                // a run its runner ended on the old deck's clock: its queued
                // finish says nothing about the new deck
                if (show->stopped())
                    m_finishedElsewhere.insert(show->id());
                pushWeight(show, drive->deck);
                pushPosition(show, drive->deck);
            }
        break;
        case Effect::StartShow:
            if (show && drive)
            {
                // the position before the start, so its first tick runs there,
                // and after it, as the first sample of this Play
                pushPosition(show, drive->deck);
                show->start(m_doc->masterTimer(), FunctionParent::master());
                pushPosition(show, drive->deck);
            }
        break;
        case Effect::ResumeShow:
            if (show && drive)
            {
                pushPosition(show, drive->deck);
                show->setPause(false);
            }
        break;
        case Effect::PauseShow:
            if (show)
                show->setPause(true);
        break;
    }
}
