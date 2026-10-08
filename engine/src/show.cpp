/*
  Q Light Controller Plus
  show.cpp

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

#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QString>
#include <QDebug>
#include <QFile>
#include <QHash>
#include <QList>

#include "showrunner.h"
#include "function.h"
#include "show.h"
#include "doc.h"

/** Pending function receipts of one Show. Every field is guarded by mutex;
 *  the observers exist only while a request is pending. */
struct ShowReceiptState
{
    struct Request
    {
        quint64 opId = 0;
        QVector<ShowFunctionExpectation> expectations;
        QSet<quint32> started; //!< functionStarted seen since the request
        QSet<quint32> stopped; //!< a cause's Function::stopped seen since the request
        QVector<QMetaObject::Connection> connections;
    };

    QMutex mutex;
    Show *show = nullptr; //!< cleared by the Show destructor
    quint64 traversal = 0;
    QVector<Request> pending;
    QVector<QMetaObject::Connection> timerConnections;

    Request *find(quint64 opId)
    {
        for (Request &request : pending)
        {
            if (request.opId == opId)
                return &request;
        }
        return nullptr;
    }

    void cancelAll()
    {
        for (const Request &request : pending)
        {
            for (const QMetaObject::Connection &c : request.connections)
                QObject::disconnect(c);
        }
        pending.clear();
        for (const QMetaObject::Connection &c : timerConnections)
            QObject::disconnect(c);
        timerConnections.clear();
    }

    /** Timer thread, after the start queue drained. Reads only what the
     *  timer owns, like ShowRunner::commandTargetActive. */
    static bool holds(const Doc *doc, const Request &request, const ShowFunctionExpectation &e)
    {
        if (e.causeFunctionId != ShowCommand::InvalidId)
        {
            if (request.stopped.contains(e.causeFunctionId))
                return true;
            const Function *cause = doc->function(e.causeFunctionId);
            // ended, possibly before this request observed it
            if (cause == nullptr || (cause->stopped() && !cause->isRunning()))
                return true;
            // the drain that ran the cause's preRun also started or refused its members
            if (!request.started.contains(e.causeFunctionId) && !cause->isRunning())
                return false;
        }

        const Function *f = doc->function(e.functionId);
        if (f == nullptr)
            return true;
        // whenever it was armed: a start ran, or already ran and ended; a
        // stop landed, or another owner kept or restarted the function
        if (e.expectLive)
            return f->isRunning() || f->stopped();
        return !f->isRunning() || !f->stopped();
    }

    void evaluate(const Doc *doc)
    {
        QMutexLocker locker(&mutex);
        for (int i = 0; i < pending.count(); )
        {
            const Request &request = pending.at(i);
            bool done = true;
            for (const ShowFunctionExpectation &e : request.expectations)
                done = done && holds(doc, request, e);
            if (!done)
            {
                i++;
                continue;
            }

            for (const QMetaObject::Connection &c : request.connections)
                QObject::disconnect(c);
            if (show != nullptr)
                emit show->functionReceiptReady(traversal, request.opId);
            pending.removeAt(i);
        }

        if (pending.isEmpty())
            cancelAll();
    }
};

#define KXMLQLCShowTimeDivision QStringLiteral("TimeDivision")
#define KXMLQLCShowTimeType     QStringLiteral("Type")
#define KXMLQLCShowTimeBPM      QStringLiteral("BPM")

/*****************************************************************************
 * Initialization
 *****************************************************************************/

Show::Show(Doc* doc) : Function(doc, Function::ShowType)
    , m_timeDivisionType(Time)
    , m_timeDivisionBPM(120)
    , m_syncSource(0) // ShowRunner::Autonomous
    , m_performAudioSuppressed(false)
    , m_latestTrackId(0)
    , m_latestShowFunctionID(0)
    , m_receipts(std::make_shared<ShowReceiptState>())
    , m_runner(NULL)
{
    m_receipts->show = this;
    setName(tr("New Show"));

    // Clear attributes here. I want attributes to be mapped
    // exactly like the Show tracks
    unregisterAttribute(tr("Intensity"));
}

Show::~Show()
{
    {
        QMutexLocker locker(&m_receipts->mutex);
        m_receipts->show = nullptr;
        m_receipts->cancelAll();
    }
    m_tracks.clear();
}

QIcon Show::getIcon() const
{
    return QIcon(":/show.png");
}

quint32 Show::totalDuration()
{
    quint32 totalDuration = 0;

    foreach (Track *track, m_tracks)
    {
        foreach (ShowFunction *sf, track->showFunctions())
        {
            if (sf->startTime() + sf->duration(doc()) > totalDuration)
                totalDuration = sf->startTime() + sf->duration(doc());
        }
    }

    return totalDuration;
}

/*****************************************************************************
 * Copying
 *****************************************************************************/

Function* Show::createCopy(Doc* doc, bool addToDoc)
{
    Q_ASSERT(doc != NULL);

    Function* copy = new Show(doc);
    if (copy->copyFrom(this) == false)
    {
        delete copy;
        copy = NULL;
    }
    if (addToDoc == true && doc->addFunction(copy) == false)
    {
        delete copy;
        copy = NULL;
    }

    return copy;
}

bool Show::copyFrom(const Function* function)
{
    const Show* show = qobject_cast<const Show*> (function);
    if (show == NULL)
        return false;

    m_timeDivisionType = show->m_timeDivisionType;
    m_timeDivisionBPM = show->m_timeDivisionBPM;
    m_latestTrackId = show->m_latestTrackId;
    m_latestShowFunctionID = show->m_latestShowFunctionID;

    /** A commanded target gets one private copy that every one of its clips
     *  shares, so a command reaches each occurrence. An uncommanded target
     *  keeps the existing copy-per-clip behaviour. */
    const QList<quint32> commandedTargets = show->commandTrack().referencedFunctionIds();
    QHash<quint32, quint32> copiedTargets;

    // create a copy of each track
    foreach (Track *track, show->tracks())
    {
        quint32 sceneID = track->getSceneID();
        Track* newTrack = new Track(sceneID, this);
        newTrack->setName(track->name());
        addTrack(newTrack);

        // create a copy of each sequence/audio in a track
        foreach (ShowFunction *sfunc, track->showFunctions())
        {
            Function* function = doc()->function(sfunc->functionID());
            if (function == NULL)
                continue;

            const bool commanded = commandedTargets.contains(function->id());
            Function *copy = NULL;

            if (commanded && copiedTargets.contains(function->id()))
            {
                copy = doc()->function(copiedTargets.value(function->id()));
            }
            else
            {
                /* Attempt to create a copy of the function to Doc */
                copy = function->createCopy(doc());
                if (copy != NULL)
                {
                    copy->setName(tr("Copy of %1").arg(function->name()));
                    if (commanded)
                        copiedTargets.insert(function->id(), copy->id());
                }
            }

            if (copy != NULL)
            {
                ShowFunction *showFunc = newTrack->createShowFunction(copy->id());
                showFunc->setStartTime(sfunc->startTime());
                showFunc->setDuration(sfunc->duration());
                showFunc->setColor(sfunc->color());
                showFunc->setLocked(sfunc->isLocked());
            }
        }
    }

    ShowCommandTrack commands = show->commandTrack();
    for (QHash<quint32, quint32>::const_iterator it = copiedTargets.constBegin();
         it != copiedTargets.constEnd(); ++it)
    {
        commands.remapFunctionId(it.key(), it.value());
    }
    storeCommandTrack(commands);

    return Function::copyFrom(function);
}

/*********************************************************************
 * Time division
 *********************************************************************/

void Show::setTimeDivision(Show::TimeDivision type, int BPM)
{
    qDebug() << "[setTimeDivision] type:" << type << ", BPM:" << BPM;
    m_timeDivisionType = type;
    m_timeDivisionBPM = BPM;
}

Show::TimeDivision Show::timeDivisionType() const
{
    return m_timeDivisionType;
}

int Show::beatsDivision() const
{
    switch(m_timeDivisionType)
    {
        case BPM_2_4: return 2;
        case BPM_3_4: return 3;
        case BPM_4_4: return 4;
        default: return 0;
    }
}

void Show::setTimeDivisionType(TimeDivision type)
{
    m_timeDivisionType = type;
}

int Show::timeDivisionBPM() const
{
    return m_timeDivisionBPM;
}

void Show::setTimeDivisionBPM(int BPM)
{
    m_timeDivisionBPM = BPM;
}

QString Show::tempoToString(Show::TimeDivision type)
{
    switch(type)
    {
        case Time: return QString("Time"); break;
        case VDJBeat: return QString("VDJBeat"); break;
        case BPM_4_4: return QString("BPM_4_4"); break;
        case BPM_3_4: return QString("BPM_3_4"); break;
        case BPM_2_4: return QString("BPM_2_4"); break;
        case Invalid:
        default:
            return QString("Invalid"); break;
    }
    return QString();
}

Show::TimeDivision Show::stringToTempo(const QString& tempo)
{
    if (tempo == "Time")
        return Time;
    else if (tempo == "VDJBeat")
        return VDJBeat;
    else if (tempo == "BPM_4_4")
        return BPM_4_4;
    else if (tempo == "BPM_3_4")
        return BPM_3_4;
    else if (tempo == "BPM_2_4")
        return BPM_2_4;
    else
        return Invalid;
}

/*****************************************************************************
 * External sync
 *****************************************************************************/

void Show::setSyncSource(int source)
{
    bool changed;
    {
        // a seek request lives only under the source it was made under
        QMutexLocker locker(&m_commandTrackMutex);
        changed = source != m_syncSource;
        if (changed)
        {
            m_requestedSeekTime.store(NoSeekRequested, std::memory_order_release);
            m_requestedForwardTime.store(NoSeekRequested, std::memory_order_release);
            m_requestedSeekIds.clear();
        }
        m_syncSource = source;
    }
    if (m_runner != NULL)
        m_runner->setSyncSource(static_cast<ShowRunner::SyncSource>(source));
    if (changed)
        emit syncSourceChanged();
}

void Show::setExternalElapsedTime(quint32 ms)
{
    const bool changed = m_externalElapsedTime.exchange(ms, std::memory_order_relaxed) != ms;
    // after the value: a reader that sees the anchor also sees a value at least as new
    quint64 none = NoSample;
    m_playFirstSample.compare_exchange_strong(none, ms, std::memory_order_acq_rel);
    if (changed)
        emit externalElapsedTimeChanged(ms);
}

void Show::requestSeek(quint32 ms)
{
    {
        QMutexLocker locker(&m_commandTrackMutex);
        // an external clock owns the position: the request is ignored, and
        // so the crossed work it would have ended stays valid
        if (m_syncSource != ShowRunner::Autonomous)
            return;
        const bool backwardPending = m_requestedSeekTime.load(std::memory_order_acquire) != NoSeekRequested;
        if (backwardPending == false && ms > commandPosition())
        {
            // forward: playback goes on through the crossed interval, nothing is cancelled
            m_requestedForwardTime.store(ms, std::memory_order_release);
            return;
        }
        m_requestedForwardTime.store(NoSeekRequested, std::memory_order_release);
        if (backwardPending)
        {
            // already cancelled and announced: only the target changes
            m_requestedSeekTime.store(ms, std::memory_order_release);
            return;
        }
        for (auto it = m_commandAppliedIds.cbegin(); it != m_commandAppliedIds.cend(); ++it)
            m_requestedSeekIds.insert(it.key());
        cancelControlWork();
        // after the cancellation, so the runner consuming it publishes anew
        m_requestedSeekTime.store(ms, std::memory_order_release);
    }
    emit commandTraversalCancelled();
}

std::optional<quint32> Show::pendingBackwardSeek() const
{
    const quint64 target = m_requestedSeekTime.load(std::memory_order_acquire);
    if (target == NoSeekRequested)
        return std::nullopt;
    return quint32(target);
}

void Show::stopRequested()
{
    // whoever stops it, crossed VC work and live exclusions of this traversal
    // end at once, even for a start the timer never ran
    {
        QMutexLocker locker(&m_commandTrackMutex);
        m_commandStops++;
        m_runnerRetired.store(true, std::memory_order_release);
        m_commandAppliedIds.clear();
        m_requestedSeekIds.clear();
        cancelControlWork();
    }
    emit commandTraversalCancelled();
}

void Show::setPerformAudioSuppressed(bool suppress)
{
    m_performAudioSuppressed.store(suppress, std::memory_order_relaxed);
}

/*****************************************************************************
 * Tracks
 *****************************************************************************/

bool Show::addTrack(Track *track, quint32 id)
{
    Q_ASSERT(track != NULL);

    // No ID given, this method can assign one
    if (id == Track::invalidId())
        id = createTrackId();

     track->setId(id);
     track->setShowId(this->id());
     m_tracks[id] = track;

     registerAttribute(QString("%1-%2").arg(track->name()).arg(track->id()));

     return true;
}

bool Show::removeTrack(quint32 id)
{
    if (m_tracks.contains(id) == true)
    {
        Track* track = m_tracks.take(id);
        Q_ASSERT(track != NULL);

        unregisterAttribute(QString("%1-%2").arg(track->name()).arg(track->id()));

        //emit trackRemoved(id);
        delete track;

        return true;
    }
    else
    {
        qWarning() << Q_FUNC_INFO << "No track found with id" << id;
        return false;
    }
}

Track* Show::track(quint32 id) const
{
    return m_tracks.value(id, NULL);
}

Track* Show::getTrackFromSceneID(quint32 id) const
{
    foreach (Track *track, m_tracks)
    {
        if (track->getSceneID() == id)
            return track;
    }
    return NULL;
}

Track *Show::getTrackFromShowFunctionID(quint32 id) const
{
    foreach (Track *track, m_tracks)
        if (track->showFunction(id) != NULL)
            return track;

    return NULL;
}

int Show::getTracksCount() const
{
    return m_tracks.size();
}

void Show::moveTrack(Track *track, int direction)
{
    if (track == NULL)
        return;

    qint32 trkID = track->id();
    if (trkID == 0 && direction == -1)
        return;
    qint32 maxID = -1;
    Track *swapTrack = NULL;
    qint32 swapID = -1;
    if (direction > 0) swapID = INT_MAX;

    foreach (quint32 id, m_tracks.keys())
    {
        qint32 signedID = (qint32)id;
        if (signedID > maxID) maxID = signedID;
        if (direction == -1 && signedID > swapID && signedID < trkID)
            swapID = signedID;
        else if (direction == 1 && signedID < swapID && signedID > trkID)
            swapID = signedID;
    }

    qDebug() << Q_FUNC_INFO << "Direction:" << direction << ", trackID:" << trkID << ", swapID:" << swapID;
    if (swapID == trkID || (direction > 0 && trkID == maxID))
        return;

    swapTrack = m_tracks[swapID];
    m_tracks[swapID] = track;
    m_tracks[trkID] = swapTrack;
    track->setId(swapID);
    swapTrack->setId(trkID);
}

QList <Track*> Show::tracks() const
{
    return m_tracks.values();
}

quint32 Show::createTrackId()
{
    while (m_tracks.contains(m_latestTrackId) == true ||
           m_latestTrackId == Track::invalidId())
    {
        m_latestTrackId++;
    }

    return m_latestTrackId;
}

/*********************************************************************
 * Show Functions
 *********************************************************************/

quint32 Show::getLatestShowFunctionId()
{
    return m_latestShowFunctionID++;
}

ShowFunction *Show::showFunction(quint32 id) const
{
    foreach (Track *track, m_tracks)
    {
        ShowFunction *sf = track->showFunction(id);
        if (sf != NULL)
            return sf;
    }

    return NULL;
}

/*********************************************************************
 * Command track
 *********************************************************************/

ShowCommandTrack Show::commandTrack() const
{
    QMutexLocker locker(&m_commandTrackMutex);
    return m_commandTrack;
}

void Show::armCommandTrackSaveProjection(const ShowCommandTrack &track)
{
    QMutexLocker locker(&m_commandTrackMutex);
    m_commandTrackSaveProjection = track;
    m_commandTrackSaveProjectionArmed = true;
}

QString Show::commandTargetError(const ShowCommandTrack &track) const
{
    Doc *document = doc();

    foreach (quint32 functionId, track.referencedFunctionIds())
    {
        if (id() != Function::invalidId() && functionId == id())
            return tr("A Show cannot command itself");

        if (document != NULL && document->function(functionId) == NULL)
            return tr("Unknown command target %1").arg(functionId);
    }

    return QString();
}

bool Show::setCommandTrack(const ShowCommandTrack &track, const QSet<quint32> &alreadyApplied,
                           QString *error)
{
    return publishCommandTrack(track, alreadyApplied, std::nullopt, error);
}

bool Show::setCommandTrack(const ShowCommandTrack &track, const QSet<quint32> &alreadyApplied,
                           quint64 appliedTraversal, QString *error)
{
    return publishCommandTrack(track, alreadyApplied, appliedTraversal, error);
}

bool Show::commandPlaybackStopped() const
{
    QMutexLocker locker(&m_commandTrackMutex);
    return m_startsEnded == m_startsAccepted;
}

bool Show::setStoppedCommandTrack(const ShowCommandTrack &track, QString *error)
{
    return publishCommandTrack(track, QSet<quint32>(), std::nullopt, error, true);
}

void Show::startRequested()
{
    QMutexLocker locker(&m_commandTrackMutex);
    m_startsAccepted++;
    m_playFirstSample.store(NoSample, std::memory_order_release);
}

bool Show::publishCommandTrack(const ShowCommandTrack &track, const QSet<quint32> &alreadyApplied,
                               std::optional<quint64> appliedTraversal, QString *error, bool onlyStopped)
{
    QString reason = commandTargetError(track);
    // a live mark needs the occurrence the caller executed
    for (auto it = alreadyApplied.cbegin(); reason.isEmpty() && it != alreadyApplied.cend(); ++it)
    {
        if (track.contains(*it) == false)
            reason = tr("Live event %1 is not in the published recording").arg(*it);
    }
    if (reason.isEmpty() == false)
    {
        if (error != NULL)
            *error = reason;
        return false;
    }

    ShowCommandTrack accepted = track;
    /** The extent is authored, but it cannot cut authored commands short.
     *  Nothing is appended past the last command: a trailing Start at the
     *  extent ends the Show right there, which is the user's choice */
    if (accepted.extent() < accepted.lastCommandTime())
        accepted.setExtent(accepted.lastCommandTime());

    if (storeCommandTrack(accepted, alreadyApplied, appliedTraversal, onlyStopped) == false)
    {
        if (error != NULL)
            *error = tr("Recordings can be edited once playback has fully stopped");
        return false;
    }

    if (error != NULL)
        error->clear();

    emit commandTrackChanged();
    return true;
}

bool Show::storeCommandTrack(const ShowCommandTrack &track, const QSet<quint32> &alreadyApplied,
                             std::optional<quint64> appliedTraversal, bool onlyStopped)
{
    {
        QMutexLocker locker(&m_commandTrackMutex);
        if (onlyStopped && m_startsEnded != m_startsAccepted)
            return false;
        m_commandTrack = track;
        m_commandTrackSaveProjectionArmed = false;
        /** Ids published earlier in this traversal are still waiting for the
         *  cursor to reach them, so they must survive a newer publication.
         *  A stopped Show has no traversal: accepting a start begins the next
         *  one, and what was executed before it is history Play replays.
         *  So is what ran in a traversal that has ended since. */
        if (stopped() == false && appliedTraversal.value_or(m_commandTraversal) == m_commandTraversal)
        {
            // the capture publication fixes the occurrence; later ones never retag it
            for (quint32 id : alreadyApplied)
            {
                if (m_commandAppliedIds.contains(id) == false)
                    m_commandAppliedIds.insert(id, liveMarkOf(track.commands().at(track.indexOfId(id))));
            }
        }
    }

    m_commandTrackRevision.fetch_add(1, std::memory_order_release);
    return true;
}

quint64 Show::commandTrackRevision() const
{
    return m_commandTrackRevision.load(std::memory_order_acquire);
}

ShowCommandTrack Show::commandTrackSnapshot(ShowLiveMarks *alreadyApplied) const
{
    QMutexLocker locker(&m_commandTrackMutex);
    if (alreadyApplied != NULL)
        *alreadyApplied = m_commandAppliedIds;

    return m_commandTrack;
}

quint64 Show::commandTraversalRestarted(quint64 traversal, bool requested)
{
    {
        QMutexLocker locker(&m_commandTrackMutex);
        if (m_runnerRetired.load(std::memory_order_acquire))
            return traversal;
        if (requested && traversal != m_commandTraversal)
            return m_commandTraversal;
        m_commandAppliedIds.clear();
        cancelControlWork();
        traversal = m_commandTraversal;
    }
    emit commandTraversalCancelled();
    return traversal;
}

void Show::cancelControlWork()
{
    m_commandTraversal++;
    m_controlBatches.clear();

    QMutexLocker receiptLocker(&m_receipts->mutex);
    m_receipts->traversal = m_commandTraversal;
    m_receipts->cancelAll();
}

quint64 Show::commandTraversal() const
{
    QMutexLocker locker(&m_commandTrackMutex);
    return m_commandTraversal;
}

quint64 Show::publishControlBatch(quint64 run, quint64 traversal, const QVector<ShowCommand> &commands,
                                  const QVector<ShowCommand> &owedAfter,
                                  const QVector<quint32> &engineStarted,
                                  const QVector<quint32> &engineStopped,
                                  std::optional<quint32> rollbackTo)
{
    quint64 seq;
    {
        QMutexLocker locker(&m_commandTrackMutex);
        // crossed while a GUI seek or stop was taking effect, or by a traversal
        // a source switch discarded: never stamped as the current one
        if (controlWorkCancelled(run) || traversal != m_commandTraversal)
            return 0;

        ShowControlBatch batch;
        batch.traversal = m_commandTraversal;
        batch.seq = seq = ++m_lastControlSeq;
        batch.rollbackTo = rollbackTo;
        batch.commands = commands;
        batch.engineStarted = engineStarted;
        batch.engineStopped = engineStopped;
        m_controlBatches.append(batch);
        // the owed work moves into the outbox in the same step
        m_commandWorkTraversal = m_commandTraversal;
        m_commandWorkHeldSeq = seq;
        m_commandWork = owedAfter;
    }
    emit controlBatchesReady();
    return seq;
}

bool Show::runRetired(quint64 run) const
{
    // the flag covers a Stop while preRun installed this runner, the count a
    // newer Play clearing the flag
    return m_runnerRetired.load(std::memory_order_acquire) ||
           m_commandStops.load(std::memory_order_acquire) != run;
}

bool Show::controlWorkCancelled(quint64 run) const
{
    return runRetired(run) || stopped() ||
           m_requestedSeekTime.load(std::memory_order_acquire) != NoSeekRequested;
}

bool Show::commandTraversalCurrent(quint64 run, quint64 traversal) const
{
    QMutexLocker locker(&m_commandTrackMutex);
    return runRetired(run) == false && traversal == m_commandTraversal;
}

bool Show::commandTraversalDiscarded(quint64 run, quint64 *traversal) const
{
    // one lock with requestSeek(), which cancels and stores the request together
    QMutexLocker locker(&m_commandTrackMutex);
    if (*traversal == m_commandTraversal || controlWorkCancelled(run))
        return false;
    *traversal = m_commandTraversal;
    return true;
}

bool Show::functionSettled(quint32 functionId, bool expectLive) const
{
    ShowFunctionExpectation expectation;
    expectation.functionId = functionId;
    expectation.expectLive = expectLive;
    return ShowReceiptState::holds(doc(), ShowReceiptState::Request(), expectation);
}

bool Show::controlBatchAcknowledged(quint64 seq) const
{
    QMutexLocker locker(&m_commandTrackMutex);
    return m_ackedControlSeq >= seq;
}

void Show::attachControlExecutor(bool attach)
{
    m_controlExecutors.fetch_add(attach ? 1 : -1, std::memory_order_acq_rel);
}

QVector<ShowControlBatch> Show::takeControlBatches()
{
    QMutexLocker locker(&m_commandTrackMutex);
    return std::exchange(m_controlBatches, QVector<ShowControlBatch>());
}

void Show::acknowledgeControlBatch(quint64 traversal, quint64 seq)
{
    QMutexLocker locker(&m_commandTrackMutex);
    if (traversal == m_commandTraversal && seq > m_ackedControlSeq)
        m_ackedControlSeq = seq;
}

void Show::requestFunctionReceipt(quint64 traversal, quint64 opId,
                                  const QVector<ShowFunctionExpectation> &expectations)
{
    Doc *document = doc();
    MasterTimer *timer = document->masterTimer();
    const std::shared_ptr<ShowReceiptState> state = m_receipts;

    QMutexLocker locker(&state->mutex);
    if (traversal != state->traversal)
        return;

    // observe first, so nothing between here and the next tick is missed
    if (state->timerConnections.isEmpty())
    {
        state->timerConnections.append(connect(timer, &MasterTimer::functionStarted,
            [state](quint32 id)
            {
                QMutexLocker l(&state->mutex);
                for (ShowReceiptState::Request &request : state->pending)
                    request.started.insert(id);
            }));
        state->timerConnections.append(connect(timer, &MasterTimer::tickReady,
            [state, document]() { state->evaluate(document); }));
    }

    ShowReceiptState::Request request;
    request.opId = opId;
    request.expectations = expectations;
    // the functions themselves are judged by state, only a cause needs its stop seen
    for (const ShowFunctionExpectation &e : expectations)
    {
        Function *cause = document->function(e.causeFunctionId);
        if (cause == nullptr)
            continue;
        request.connections.append(connect(cause, qOverload<quint32>(&Function::stopped), [state, opId](quint32 id)
        {
            QMutexLocker l(&state->mutex);
            if (ShowReceiptState::Request *pending = state->find(opId))
                pending->stopped.insert(id);
        }));
    }
    state->pending.append(request);
}

bool Show::commandWorkPending() const
{
    QMutexLocker locker(&m_commandTrackMutex);
    if (m_commandWorkTraversal != m_commandTraversal)
        return false;
    return m_commandWorkHeldSeq > m_ackedControlSeq || m_commandWork.isEmpty() == false;
}

QVector<ShowCommand> Show::pendingCommandWork() const
{
    QMutexLocker locker(&m_commandTrackMutex);
    QVector<ShowCommand> work;
    for (const ShowControlBatch &batch : m_controlBatches)
        work += batch.commands;
    if (m_commandWorkTraversal == m_commandTraversal)
        work += m_commandWork;
    return work;
}

void Show::setCommandWork(quint64 run, quint64 traversal, quint64 heldSeq, const QVector<ShowCommand> &owed)
{
    bool drained;
    {
        QMutexLocker locker(&m_commandTrackMutex);
        const bool wasOwed = m_commandWorkHeldSeq != 0 || m_commandWork.isEmpty() == false;
        const bool cancelled = controlWorkCancelled(run) || traversal != m_commandTraversal;
        m_commandWorkTraversal = m_commandTraversal;
        m_commandWorkHeldSeq = cancelled ? 0 : heldSeq;
        m_commandWork = cancelled ? QVector<ShowCommand>() : owed;
        drained = wasOwed && m_commandWorkHeldSeq == 0 && m_commandWork.isEmpty();
    }
    if (drained)
        emit commandWorkDrained();
}

void Show::setCommandRecording(bool enabled)
{
    m_commandRecording.store(enabled, std::memory_order_relaxed);
}

bool Show::commandRecording() const
{
    return m_commandRecording.load(std::memory_order_relaxed);
}

void Show::setCommandPosition(quint32 ms)
{
    m_commandPosition.store(ms, std::memory_order_relaxed);
}

quint32 Show::commandPosition() const
{
    /** An externally driven Show has exactly one authoritative clock: the one
     *  the host pushes. It stays readable while the Show is idle or paused,
     *  and it is what the runtime consumes anyway, so the two never disagree.
     *  Function::isRunning() is deliberately not consulted here - it belongs
     *  to the MasterTimer, not to callers on the UI thread. */
    if (m_syncSource != 0)
        return externalElapsedTime();

    return m_commandPosition.load(std::memory_order_relaxed);
}

/*****************************************************************************
 * Load & Save
 *****************************************************************************/

bool Show::saveXML(QXmlStreamWriter *doc) const
{
    Q_ASSERT(doc != NULL);

    /* Function tag */
    doc->writeStartElement(KXMLQLCFunction);

    /* Common attributes */
    saveXMLCommon(doc);

    doc->writeStartElement(KXMLQLCShowTimeDivision);
    doc->writeAttribute(KXMLQLCShowTimeType, tempoToString(m_timeDivisionType));
    doc->writeAttribute(KXMLQLCShowTimeBPM, QString::number(m_timeDivisionBPM));
    doc->writeEndElement();

    foreach (Track *track, m_tracks)
        track->saveXML(doc);

    ShowCommandTrack commands;
    {
        QMutexLocker locker(&m_commandTrackMutex);
        commands = m_commandTrackSaveProjectionArmed ? m_commandTrackSaveProjection : m_commandTrack;
        m_commandTrackSaveProjectionArmed = false;
    }
    if (commands.isEmpty() == false || commands.extent() > 0)
        commands.saveXML(doc);

    /* End the <Function> tag */
    doc->writeEndElement();

    return true;
}

bool Show::loadXML(QXmlStreamReader &root)
{
    if (root.name() != KXMLQLCFunction)
    {
        qWarning() << Q_FUNC_INFO << "Function node not found";
        return false;
    }

    if (root.attributes().value(KXMLQLCFunctionType).toString() != typeToString(Function::ShowType))
    {
        qWarning() << Q_FUNC_INFO << root.attributes().value(KXMLQLCFunctionType).toString()
                   << "is not a show";
        return false;
    }

    while (root.readNextStartElement())
    {
        if (root.name() == KXMLQLCShowTimeDivision)
        {
            QString type = root.attributes().value(KXMLQLCShowTimeType).toString();
            int bpm = root.attributes().value(KXMLQLCShowTimeBPM).toString().toInt();
            setTimeDivision(stringToTempo(type), bpm);
            root.skipCurrentElement();
        }
        else if (root.name() == KXMLQLCTrack)
        {
            Track *trk = new Track(Function::invalidId(), this);
            if (trk->loadXML(root) == true)
                addTrack(trk, trk->id());
        }
        else if (root.name() == KXMLShowCommandTrack)
        {
            ShowCommandTrack parsed;
            QString error;
            if (parsed.loadXML(root, &error) == false)
            {
                qWarning() << Q_FUNC_INFO << "Invalid command track:" << error;
                // leave the reader at the end of this Show, not inside it
                while (root.readNextStartElement())
                    root.skipCurrentElement();
                return false;
            }

            /** Targets are not resolved here: the functions a Show commands may
             *  still be further down the workspace. postLoad() diagnoses them. */
            storeCommandTrack(parsed);
        }
        else
        {
            qWarning() << Q_FUNC_INFO << "Unknown Show tag:" << root.name();
            root.skipCurrentElement();
        }
    }

    return true;
}

void Show::postLoad()
{
    foreach (Track* track, m_tracks)
    {
        if (track->postLoad(doc()))
            doc()->setModified();
    }

    /** An unresolved command target is reported and kept: dropping it would
     *  silently destroy authored data that a later workspace fix could use */
    const ShowCommandTrack commands = commandTrack();
    foreach (quint32 functionId, commands.referencedFunctionIds())
    {
        if (doc()->function(functionId) == NULL)
            qWarning() << Q_FUNC_INFO << "Show" << name()
                       << "commands an unknown function:" << functionId;
    }
}

bool Show::contains(quint32 functionId) const
{
    Doc *doc = this->doc();
    Q_ASSERT(doc != NULL);

    if (functionId == id())
        return true;

    foreach (Track* track, m_tracks)
    {
        if (track->contains(doc, functionId))
            return true;
    }

    return commandTrack().referencedFunctionIds().contains(functionId);
}

QList<quint32> Show::components() const
{
    QList<quint32> ids;

    foreach (Track* track, m_tracks)
        ids.append(track->components());

    foreach (quint32 functionId, commandTrack().referencedFunctionIds())
    {
        if (ids.contains(functionId) == false)
            ids.append(functionId);
    }

    return ids;
}

/*****************************************************************************
 * Running
 *****************************************************************************/

void Show::preRun(MasterTimer* timer)
{
    quint64 stops;
    {
        QMutexLocker locker(&m_commandTrackMutex);
        stops = m_commandStops;
        m_startsRunning = m_startsAccepted;
    }
    m_requestedSeekTime.store(NoSeekRequested, std::memory_order_relaxed);
    m_requestedForwardTime.store(NoSeekRequested, std::memory_order_relaxed);
    m_commandPosition.store(elapsed(), std::memory_order_relaxed);
    Function::preRun(timer);
    m_runningChildren.clear();
    if (m_runner != NULL)
    {
        m_runner->stop();
        delete m_runner;
    }

    {
        // a Stop while installing wins: this runner is retired from the start
        QMutexLocker locker(&m_commandTrackMutex);
        if (m_commandStops == stops)
            m_runnerRetired.store(false, std::memory_order_release);
    }
    m_runner = new ShowRunner(doc(), this->id(), elapsed());
    m_runnerPaused = false;
    m_runner->setSyncSource(static_cast<ShowRunner::SyncSource>(m_syncSource));
    const quint64 anchor = m_playFirstSample.load(std::memory_order_acquire);
    m_runner->setExternalElapsedTime(externalElapsedTime(), anchor);
    int i = 0;
    foreach (Track *track, m_tracks)
        m_runner->adjustIntensity(getAttributeValue(i++), track);

    connect(m_runner, SIGNAL(timeChanged(quint32)), this, SIGNAL(timeChanged(quint32)));
    connect(m_runner, SIGNAL(showFinished()), this, SIGNAL(showFinished()));
    m_runner->start();
}

void Show::setPause(bool enable)
{
    // like Chaser: the runner and its clips follow at the next write, on the
    // timer thread that owns them
    Function::setPause(enable);
}

void Show::write(MasterTimer* timer, QList<Universe *> universes)
{
    Q_UNUSED(universes);

    if (m_runnerRetired.load(std::memory_order_acquire))
        return;

    const bool paused = isPaused();
    if (paused != m_runnerPaused)
    {
        m_runnerPaused = paused;
        m_runner->setPause(paused);
    }

    // paused: finish what was already crossed, without clock, clips or seeks
    if (paused)
    {
        m_runner->drainCommands();
        return;
    }

    // the anchor first, so the value read after it is at least as new
    const quint64 anchor = m_playFirstSample.load(std::memory_order_acquire);
    m_runner->setExternalElapsedTime(externalElapsedTime(), anchor);
    quint64 seekTime;
    quint64 forwardTime;
    {
        QMutexLocker locker(&m_commandTrackMutex);
        seekTime = m_requestedSeekTime.exchange(NoSeekRequested, std::memory_order_acquire);
        forwardTime = m_requestedForwardTime.exchange(NoSeekRequested, std::memory_order_acquire);
        // an external clock ignores it; consumed, what it ends is history
        if (seekTime != NoSeekRequested && m_syncSource == ShowRunner::Autonomous)
        {
            for (quint32 id : std::as_const(m_requestedSeekIds))
                m_commandAppliedIds.remove(id);
        }
        else
            seekTime = NoSeekRequested;
        m_requestedSeekIds.clear();
        // the runner reached it since the request was classified: playback crossed it already
        if (m_syncSource != ShowRunner::Autonomous ||
            forwardTime <= m_commandPosition.load(std::memory_order_relaxed))
            forwardTime = NoSeekRequested;
    }
    if (seekTime != NoSeekRequested)
        m_runner->requestSeek(quint32(seekTime));
    else if (forwardTime != NoSeekRequested)
        m_runner->requestSeek(quint32(forwardTime), true);
    m_runner->write(timer);
}

void Show::postRun(MasterTimer* timer, QList<Universe *> universes)
{
    m_requestedSeekTime.store(NoSeekRequested, std::memory_order_relaxed);
    m_requestedForwardTime.store(NoSeekRequested, std::memory_order_relaxed);
    if (m_runner != NULL)
    {
        m_runner->stop();
        delete m_runner;
        m_runner = NULL;
    }
    {
        // a start accepted after this run began keeps the Show playing
        QMutexLocker locker(&m_commandTrackMutex);
        m_startsEnded = m_startsRunning;
    }
    Function::postRun(timer, universes);
}

void Show::slotChildStopped(quint32 fid)
{
    Q_UNUSED(fid);
}

/*****************************************************************************
 * Attributes
 *****************************************************************************/

int Show::adjustAttribute(qreal fraction, int attributeId)
{
    int attrIndex = Function::adjustAttribute(fraction, attributeId);

    if (m_runner != NULL)
    {
        QList<Track*> trkList = m_tracks.values();
        if (trkList.isEmpty() == false &&
            attrIndex >= 0 && attrIndex < trkList.count())
        {
            Track *track = trkList.at(attrIndex);
            if (track != NULL)
                m_runner->adjustIntensity(getAttributeValue(attrIndex), track);
        }
    }

    return attrIndex;
}
