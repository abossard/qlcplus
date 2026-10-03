/*
  Q Light Controller
  showrunner.cpp

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

#include <QMutex>
#include <QScopeGuard>
#include <algorithm>
#include <utility>
#include <QDebug>

#include "showrunner.h"
#include "showeventlog.h"
#include "audio.h"
#include "chaser.h"
#include "function.h"
#include "track.h"
#include "show.h"
#include "doc.h"
#include "inputoutputmap.h"

#define TIMER_INTERVAL 50

/** The Intensity override a command sets: the Single override's own value */
static qreal intensityOf(const Function *function)
{
    const Attribute intensity = function->attributes().at(Function::Intensity);
    return intensity.m_isOverridden && intensity.m_value > 0.0
           ? intensity.m_overrideValue / intensity.m_value : 1.0;
}

static bool compareShowFunctions(const ShowFunction *sf1, const ShowFunction *sf2)
{
    if (sf1->startTime() < sf2->startTime())
        return true;
    return false;
}

ShowRunner::ShowRunner(const Doc* doc, quint32 showID, quint32 startTime)
    : QObject(NULL)
    , m_doc(doc)
    , m_syncSource(Autonomous)
    , m_currentTimeFunctionIndex(0)
    , m_elapsedTime(startTime)
    , m_currentBeatFunctionIndex(0)
    , m_elapsedBeats(0)
    , m_internalBeatClockMs(startTime)
    , beatSynced(false)
    , m_syncElapsedTime(0)
    , m_syncBeatsTime(0)
    , m_totalRunTime(0)
    , m_totalRunBeats(0)
    , m_commandRevision(std::numeric_limits<quint64>::max())
{
    Q_ASSERT(m_doc != NULL);
    Q_ASSERT(showID != Show::invalidId());

    m_show = qobject_cast<Show*>(m_doc->function(showID));
    if (m_show == NULL)
        return;
    m_performAudioSuppressed = m_show->performAudioSuppressed();
    m_commandTraversal = m_show->commandTraversal();
    m_commandRun = m_show->commandRun();

    /* startTime (e.g. coming from the cursor position) is always real
       milliseconds. If playback doesn't start from 0, m_elapsedBeats needs
       the equivalent estimate in "beats as ms" too, otherwise every Play
       would always start counting beats from 0 regardless of where
       playback actually begins - making beat-based Functions that should
       already be running (or long finished) at startTime behave as if
       playback had just begun. This is only a starting estimate: once the
       first real beat lands (see beatSynced in write()), m_elapsedBeats
       keeps advancing in lockstep with the actual beat clock from there */
    if (startTime > 0)
    {
        int bpm = m_doc->masterTimer()->beatSourceType() == MasterTimer::None
                ? (m_show->timeDivisionBPM() > 0 ? m_show->timeDivisionBPM() : 120)
                : m_doc->masterTimer()->bpmNumber();
        if (bpm > 0)
            m_elapsedBeats = qRound64(double(startTime) * bpm / 60.0);
    }

    foreach (Track *track, m_show->tracks())
    {
        // some sanity checks
        if (track == NULL ||
            track->id() == Track::invalidId())
                continue;

        if (track->isMute())
            continue;

        // get all the functions of the track and append them to the runner queue
        foreach (ShowFunction *sfunc, track->showFunctions())
        {
            Function *f = m_doc->function(sfunc->functionID());
            if (f == NULL)
                continue;

            if (f->tempoType() == Function::Time)
            {
                m_timeFunctions.append(sfunc);
                if (sfunc->startTime() + sfunc->duration(m_doc) > m_totalRunTime)
                    m_totalRunTime = sfunc->startTime() + sfunc->duration(m_doc);
            }
            else
            {
                m_beatFunctions.append(sfunc);
                if (sfunc->startTime() + sfunc->duration(m_doc) > m_totalRunBeats)
                    m_totalRunBeats = sfunc->startTime() + sfunc->duration(m_doc);
            }
        }

        // Initialize the intensity map
        m_intensityMap[track->id()] = 1.0;
    }

    std::sort(m_timeFunctions.begin(), m_timeFunctions.end(), compareShowFunctions);
    std::sort(m_beatFunctions.begin(), m_beatFunctions.end(), compareShowFunctions);

#if 1
    qDebug() << "Ordered list of ShowFunctions (time):";
    foreach (ShowFunction *sfunc, m_timeFunctions)
        qDebug() << "[Show] Function ID:" << sfunc->functionID() << "start time:" << sfunc->startTime() << "duration:" << sfunc->duration(m_doc);

    qDebug() << "Ordered list of ShowFunctions (beats):";
    foreach (ShowFunction *sfunc, m_beatFunctions)
        qDebug() << "[Show] Function ID:" << sfunc->functionID() << "start time:" << sfunc->startTime() << "duration:" << sfunc->duration(m_doc);
#endif
    m_runningQueue.clear();

    /** Commands are traversed from the cursor the Show actually starts at:
     *  nothing before it is replayed or restored, commands at it are due at
     *  once. Under External the first fresh sample is that position instead.
     *  The live ids imported here were all published after Play was accepted. */
    m_commandState.playing = true;
    m_commandState = ShowCommandFsm::seek(m_commandState, startTime);
    refreshCommandTrack();
    m_externalBaselinePending = m_show->syncSource() == External;

    qDebug() << "ShowRunner created";
}

ShowRunner::~ShowRunner()
{
}

void ShowRunner::start()
{
    qDebug() << "ShowRunner started";
}

void ShowRunner::setPause(bool enable)
{
    const bool keepAudioPaused = !enable && m_show != NULL &&
                                 m_show->performAudioSuppressed();
    for (int i = 0; i < m_runningQueue.count(); i++)
    {
        Function *f = m_runningQueue.at(i).first;
        if (keepAudioPaused && isAudioFunction(f))
            continue;
        f->setPause(enable);
    }
}

void ShowRunner::stop()
{
    m_elapsedTime = 0;
    m_elapsedBeats = 0;
    m_internalBeatClockMs = 0.0;
    beatSynced = false;
    m_syncElapsedTime = 0;
    m_syncBeatsTime = 0;
    m_currentTimeFunctionIndex = 0;
    m_currentBeatFunctionIndex = 0;

    for (int i = 0; i < m_runningQueue.count(); i++)
    {
        Function *f = m_runningQueue.at(i).first;
        f->stop(functionParent());
    }

    stopCommandOwnedFunctions();
    m_commandState = ShowCommandState();
    m_pendingRollback = NoCommandSeek;
    m_commandJumpPending = false;
    m_legacyJournal.clear();
    m_commandRemainder.clear();
    m_heldControlBatch = 0;
    m_commandCatchUp = false;
    m_settlingFunction = ShowCommand::InvalidId;
    m_engineStarted.clear();
    m_engineStopped.clear();
    if (m_show != NULL)
        m_commandTraversal = m_show->commandTraversalRestarted(m_commandTraversal, false);

    m_runningQueue.clear();
    m_seekRestartFunctions.clear();
    m_deferredTimeAudioFunctionIds.clear();
    m_deferredBeatAudioFunctionIds.clear();
    qDebug() << "ShowRunner stopped";
}

FunctionParent ShowRunner::functionParent() const
{
    return FunctionParent(FunctionParent::Function, m_show->id());
}

void ShowRunner::write(MasterTimer *timer)
{
    //qDebug() << Q_FUNC_INFO << "elapsed:" << m_elapsedTime << ", total:" << m_totalRunTime;

    // effects journaled under another time source are not this traversal's
    if (m_syncSource != m_journalSource)
    {
        m_journalSource = m_syncSource;
        m_legacyJournal.clear();
    }

    // the traversal begins here: nothing before it is due, and the live marks
    // published since Play stay
    const auto beginTraversal = [this](quint32 ms) {
        m_externalBaselinePending = false;
        const ShowLiveMarks marks = m_commandState.consumedLiveEventIds;
        m_commandState = ShowCommandFsm::seek(m_commandState, ms);
        m_commandState.consumedLiveEventIds = marks;
    };
    // an autonomous clock waits for no sample
    if (m_externalBaselinePending && m_syncSource != External)
        beginTraversal(m_elapsedTime);

    // --- External sync: update elapsed time from external source ---
    if (m_syncSource == External)
    {
        // the anchor first: the value read after it is at least that new
        const quint64 anchor = m_externalAnchor.load(std::memory_order_acquire);
        quint32 newTime = m_externalElapsedTime.load(std::memory_order_relaxed);

        // Backward seek detection: if external time moved backward,
        // stop all running functions and reset scan indices
        if (newTime < m_elapsedTime && m_elapsedTime > 0)
        {
            seekTo(newTime, false);
        }

        m_elapsedTime = newTime;
        const int songBpm = m_show->timeDivisionBPM() > 0 ? m_show->timeDivisionBPM() : 120;
        m_elapsedBeats = qRound64(double(newTime) * songBpm / 60.0);
        beatSynced = true;

        // the earliest sample since Play, or where the clock went back to since
        if (m_externalBaselinePending && anchor != NoSample)
            beginTraversal(qMin(quint32(anchor), newTime));
    }

    // the Show forwards a request only under Autonomous, one kind per write
    const quint64 requestedSeekTime =
            m_requestedSeekTime.exchange(NoSeekRequested, std::memory_order_acquire);
    const quint64 requestedForwardTime =
            m_requestedForwardTime.exchange(NoSeekRequested, std::memory_order_acquire);
    if (requestedSeekTime != NoSeekRequested)
        seekTo(quint32(requestedSeekTime), true);
    else if (requestedForwardTime != NoSeekRequested)
        seekTo(quint32(requestedForwardTime), true, true);

    bool seekRestartPending = false;
    const auto waitForSeekRestart = [this, &seekRestartPending](Function *function) {
        if (!m_seekRestartFunctions.contains(function))
            return false;
        if (function->isRunning() && function->stopped())
        {
            seekRestartPending = true;
            return true;
        }
        m_seekRestartFunctions.remove(function);
        return false;
    };

    // Phase 1. Check all the Functions that need to be started
    // m_timeFunctions is ordered by startup time, so when we found an entry
    // with start time greater than m_elapsed, this phase is over
    bool startFunctionsDone = false;

    // A Show can freely mix time-based and beat-based Functions on its
    // tracks (e.g. a beat-synced Chaser next to a Time-based Audio track),
    // regardless of the Show's own timeline display type. So beat tracking
    // must not depend on, nor gate, anything based on the Show's own
    // division: it only needs to run when this Show actually has beat-based
    // Functions to drive, and it must never block m_timeFunctions/m_elapsedTime
    // from progressing while waiting for the first beat to land.
    if (m_syncSource == Autonomous && timer->beatSourceType() == MasterTimer::None &&
        (!m_beatFunctions.isEmpty() || m_show->tempoType() == Function::Beats))
    {
        int songBpm = m_show->timeDivisionBPM();
        if (songBpm <= 0)
            songBpm = 120;
        beatSynced = true;
        m_internalBeatClockMs += double(MasterTimer::tick());
        m_elapsedBeats = quint32((m_internalBeatClockMs * songBpm * 1000.0) / 60000.0);
    }
    else if (m_syncSource == Autonomous && !m_beatFunctions.isEmpty() && timer->isBeat())
    {
        if (beatSynced == false)
        {
            beatSynced = true;
            m_syncElapsedTime = m_elapsedTime;
            int syncBpm = timer->bpmNumber();
            m_syncBeatsTime = syncBpm > 0 ? Function::beatsToTime(m_elapsedBeats, 60000 / syncBpm) : 0;
            qDebug() << "Beat synced";
        }
        else
        {
            m_elapsedBeats += 1000;
        }
    }

    syncPerformAudioSuppression();
    if (!m_performAudioSuppressed)
    {
        startDeferredAudioFunctions(m_timeFunctions, m_deferredTimeAudioFunctionIds, m_elapsedTime);
        if (beatSynced)
            startDeferredAudioFunctions(m_beatFunctions, m_deferredBeatAudioFunctionIds, m_elapsedBeats);
    }

    // check if there are time-based functions to start
    while (startFunctionsDone == false)
    {
        if (m_currentTimeFunctionIndex == m_timeFunctions.count())
            break;

        ShowFunction *sf = m_timeFunctions.at(m_currentTimeFunctionIndex);
        quint32 funcStartTime = sf->startTime();
        quint32 functionTimeOffset = 0;
        Function *f = m_doc->function(sf->functionID());
        const quint32 functionStopTime = sf->startTime() + sf->duration(m_doc);
        if (f == nullptr || functionStopTime <= m_elapsedTime)
        {
            m_deferredTimeAudioFunctionIds.remove(sf->id());
            m_currentTimeFunctionIndex++;
            continue;
        }
        if (m_performAudioSuppressed && isAudioFunction(f))
        {
            m_deferredTimeAudioFunctionIds.insert(sf->id());
            m_currentTimeFunctionIndex++;
            continue;
        }
        // this should happen only when a Show is not started from 0
        if (m_elapsedTime > funcStartTime)
        {
            functionTimeOffset = m_elapsedTime - funcStartTime;
            funcStartTime = m_elapsedTime;
        }
        if (m_elapsedTime >= funcStartTime)
        {
            if (waitForSeekRestart(f))
            {
                startFunctionsDone = true;
                continue;
            }
            applyTrackIntensity(sf, f);

            f->start(m_doc->masterTimer(), functionParent(), functionTimeOffset);
            noteEngineStart(f);
            m_runningQueue.append(QPair<Function *, quint32>(f, functionStopTime));
            m_currentTimeFunctionIndex++;
        }
        else
            startFunctionsDone = true;
    }

    startFunctionsDone = false;

    // check if there are beat-based functions to start
    // (wait for the first real beat to land before considering any of
    // them, so m_elapsedBeats == 0 is not mistaken for "beat zero happened")
    while (startFunctionsDone == false && beatSynced)
    {
        if (m_currentBeatFunctionIndex == m_beatFunctions.count())
            break;

        ShowFunction *sf = m_beatFunctions.at(m_currentBeatFunctionIndex);
        quint32 funcStartTime = sf->startTime();
        quint32 functionTimeOffset = 0;
        Function *f = m_doc->function(sf->functionID());
        const quint32 functionStopTime = sf->startTime() + sf->duration(m_doc);
        if (f == nullptr || functionStopTime <= m_elapsedBeats)
        {
            m_deferredBeatAudioFunctionIds.remove(sf->id());
            m_currentBeatFunctionIndex++;
            continue;
        }
        if (m_performAudioSuppressed && isAudioFunction(f))
        {
            m_deferredBeatAudioFunctionIds.insert(sf->id());
            m_currentBeatFunctionIndex++;
            continue;
        }
        // this should happen only when a Show is not started from 0
        if (m_elapsedBeats > funcStartTime)
        {
            functionTimeOffset = m_elapsedBeats - funcStartTime;
            funcStartTime = m_elapsedBeats;
        }
        if (m_elapsedBeats >= funcStartTime)
        {
            if (waitForSeekRestart(f))
            {
                startFunctionsDone = true;
                continue;
            }
            applyTrackIntensity(sf, f);

            f->start(m_doc->masterTimer(), functionParent(), functionTimeOffset);
            noteEngineStart(f);
            m_runningQueue.append(QPair<Function *, quint32>(f, functionStopTime));
            m_currentBeatFunctionIndex++;
        }
        else
            startFunctionsDone = true;
    }

    // Phase 2. Check if we need to stop some running Functions
    // It is done in reverse order for two reasons:
    // 1- m_runningQueue is not ordered by stop time
    // 2- to avoid messing up with indices when an entry is removed
    for (int i = m_runningQueue.count() - 1; i >= 0; i--)
    {
        Function *func = m_runningQueue.at(i).first;
        quint32 stopTime = m_runningQueue.at(i).second;
        quint32 currTime = func->tempoType() == Function::Time ? m_elapsedTime : m_elapsedBeats;

        // if we passed the function stop time
        if (currTime >= stopTime)
        {
            // stop the function
            func->stop(functionParent());
            noteEngineStop(func);
            // remove it from the running queue
            m_runningQueue.removeAt(i);
        }
    }

    // Phase 3. Apply the commands this position is due for. They run after the
    // clip phases, so for the same millisecond a command is the newer intent.
    refreshCommandTrack();
    processCommands(seekRestartPending);
    publishCommandWork();
    if (m_show != NULL)
        m_show->setCommandPosition(m_elapsedTime);

    // Phase 4. Check if this is the end of the Show. A Show can mix
    // time-based and beat-based tracks, so it is only really over once
    // both the time-based and the beat-based timelines have completed.
    // While there are beat-based Functions but no beat has been detected
    // yet, the beat timeline hasn't even started, so it can't be "done".
    // An authored command extent and an armed recorder extend that end.
    // crossed commands still owed, a VC batch still being applied, or a
    // jump's Start/Stop still settling hold the end
    bool timeDone = m_elapsedTime >= m_totalRunTime && m_elapsedTime >= commandExtent() &&
                    m_commandRemainder.isEmpty() && m_heldControlBatch == 0 &&
                    m_settlingFunction == ShowCommand::InvalidId;
    bool beatsDone = m_beatFunctions.isEmpty() ||
                      (beatSynced && m_elapsedBeats >= m_totalRunBeats);

    // an armed recorder keeps a command-only or very short Show alive
    if (m_show != NULL && m_show->commandRecording())
        timeDone = false;

    if (timeDone && beatsDone)
    {
        if (m_show != NULL)
            m_show->stop(functionParent());
        emit showFinished();
        return;
    }

    // Only auto-increment in Autonomous mode
    if (m_syncSource == Autonomous && !seekRestartPending)
        m_elapsedTime += MasterTimer::tick();

    // Report plain elapsed milliseconds: it advances smoothly on every
    // tick, and the UI scales it to a beat/bar position against the
    // current BPM (see ShowManager and TimeUtils.timeToBeatPosition()) so
    // it reacts immediately to live BPM changes.
    //
    // However, when the Show's own timeline is BPM based, m_elapsedTime is
    // real wall-clock time since the Show started, which includes however
    // long it took to wait for the first beat to sync (beat 0 is defined
    // by that sync moment, not by when the Show started) - reporting it
    // directly would make the cursor jump to an arbitrary, not
    // beat-aligned position the instant sync happens. Instead, report the
    // beat-zeroed resume position (m_syncBeatsTime) plus how much real
    // time has passed since the sync moment: this still advances smoothly
    // every tick (unlike reporting m_elapsedBeats directly, which only
    // moves in whole-beat jumps), while staying exactly beat-aligned at
    // the sync instant itself.
    if (Show::isTimeBasedDivision(m_show->timeDivisionType()) || m_syncSource == External ||
        m_beatFunctions.isEmpty())
    {
        emit timeChanged(m_elapsedTime);
    }
    else if (beatSynced)
    {
        emit timeChanged(m_syncBeatsTime + (m_elapsedTime - m_syncElapsedTime));
    }
}

/************************************************************************
 * Sync source
 ************************************************************************/

void ShowRunner::setSyncSource(SyncSource source)
{
    m_syncSource = source;
    if (source == External)
        qDebug() << "[ShowRunner] Sync source set to External";
    else
        qDebug() << "[ShowRunner] Sync source set to Autonomous";
}

void ShowRunner::setExternalElapsedTime(quint32 ms, quint64 anchor)
{
    m_externalElapsedTime.store(ms, std::memory_order_relaxed);
    m_externalAnchor.store(anchor, std::memory_order_release);
}

void ShowRunner::requestSeek(quint32 ms, bool forward)
{
    (forward ? m_requestedForwardTime : m_requestedSeekTime).store(ms, std::memory_order_release);
}

void ShowRunner::seekTo(quint32 newTime, bool requested, bool forward)
{
    qDebug() << "[ShowRunner] Seeking:" << m_elapsedTime << "->" << newTime;

    // Stop all currently running functions
    m_seekRestartFunctions.clear();
    for (int i = 0; i < m_runningQueue.count(); i++)
    {
        Function *f = m_runningQueue.at(i).first;
        f->stop(functionParent());
        if (f->isRunning() && f->stopped())
            m_seekRestartFunctions.insert(f);
    }
    m_runningQueue.clear();
    m_deferredTimeAudioFunctionIds.clear();
    m_deferredBeatAudioFunctionIds.clear();

    // Reset the scan index so functions can be re-evaluated from the new position
    m_currentTimeFunctionIndex = 0;
    m_currentBeatFunctionIndex = 0;

    m_elapsedTime = newTime;
    m_internalBeatClockMs = newTime;
    const MasterTimer *timer = m_doc->masterTimer();
    const int bpm = timer->beatSourceType() == MasterTimer::None
            ? (m_show->timeDivisionBPM() > 0 ? m_show->timeDivisionBPM() : 120)
            : timer->bpmNumber();
    if (bpm > 0)
        m_elapsedBeats = qRound64(double(newTime) * bpm / 60.0);
    m_syncElapsedTime = newTime;
    m_syncBeatsTime = newTime;

    // Skip time-based functions that have already ended before newTime
    while (m_currentTimeFunctionIndex < m_timeFunctions.count())
    {
        ShowFunction *sf = m_timeFunctions.at(m_currentTimeFunctionIndex);
        if (sf->startTime() + sf->duration(m_doc) <= newTime)
            m_currentTimeFunctionIndex++;
        else
            break;
    }

    /** A forward jump keeps the traversal: command-owned playback, live
     *  marks, the held batch, the remainder and a settling target stay, and
     *  the next advance plays the crossed interval */
    if (forward)
    {
        m_commandJumpPending = true;
        return;
    }

    /** A backward move ends this traversal: the crossed work still owed is
     *  dropped, the live no-echo suppression of the previous pass is over,
     *  and nothing before newTime is replayed. What this traversal changed
     *  after newTime is rolled back once a settling target settled. */
    m_commandRemainder.clear();
    m_heldControlBatch = 0;
    m_commandCatchUp = false;
    m_commandJumpPending = false;
    m_engineStarted.clear();
    m_engineStopped.clear();
    m_retiredLiveStopIds.clear();
    // ids published from here on belong to the next traversal and stay shielded
    m_commandState = ShowCommandFsm::seek(m_commandState, newTime);
    if (m_show != NULL)
        m_commandTraversal = m_show->commandTraversalRestarted(m_commandTraversal, requested);
    if (m_externalBaselinePending == false)
        m_pendingRollback = newTime;
}

/************************************************************************
 * Commands
 ************************************************************************/

void ShowRunner::refreshCommandTrack()
{
    if (m_show == NULL)
        return;

    const quint64 revision = m_show->commandTrackRevision();
    if (revision == m_commandRevision)
        return;

    m_commandRevision = revision;

    ShowLiveMarks alreadyApplied;
    m_commandTrack = m_show->commandTrackSnapshot(&alreadyApplied);

    /** The recorder executed these occurrences live, so the traversal owes
     *  them nothing. Each mark keeps its capture time, so taking them again on
     *  a later revision never suppresses an event moved elsewhere since. The
     *  transition logic drops each one again as soon as the playhead consumes
     *  the mark's time, and a seek clears them for the next traversal. */
    m_commandState.consumedLiveEventIds.insert(alreadyApplied);
    retireLiveStopDeadlines(alreadyApplied);
}

void ShowRunner::processCommands(bool traversalStalled)
{
    if (m_show == NULL)
        return;

    dropDiscardedTraversal();

    // an external traversal begins at its first fresh sample
    if (m_externalBaselinePending)
        return;

    /** A seek that is still waiting for a Function to finish stopping has not
     *  resumed the traversal yet. Consuming commands now would swallow the
     *  ones authored exactly at the destination. */
    if (traversalStalled)
        return;

    m_commandStartedThisTick.clear();

    if (std::exchange(m_commandJumpPending, false) && commandCatchUp())
        m_commandCatchUp = true;

    m_commandState.playing = true;
    const ShowCommandTransition played =
            ShowCommandFsm::advance(m_commandTrack, m_commandState, m_elapsedTime);
    m_commandState = played.state;

    dispatchCommandEffects(played.effects);
}

void ShowRunner::dropDiscardedTraversal()
{
    if (m_show->commandTraversalDiscarded(m_commandRun, &m_commandTraversal) == false)
        return;

    m_commandRemainder.clear();
    m_heldControlBatch = 0;
    m_commandCatchUp = false;
    m_settlingFunction = ShowCommand::InvalidId;
    m_engineStarted.clear();
    m_engineStopped.clear();
}

bool ShowRunner::commandCatchUp() const
{
    return m_show != NULL && m_show->hasControlExecutor() && !m_commandTrack.referencedControls().isEmpty();
}

void ShowRunner::dispatchCommandEffects(const QVector<ShowCommand> &effects)
{
    const auto isControl = [](const ShowCommand &cmd) {
        return cmd.action == ShowCommandAction::SetButtonState ||
               cmd.action == ShowCommandAction::SetSliderPosition;
    };

    // without an executor VC records are dropped, as before there was one
    const bool executor = m_show->hasControlExecutor();
    if (m_heldControlBatch != 0 &&
        (executor == false || m_show->controlBatchAcknowledged(m_heldControlBatch)))
        m_heldControlBatch = 0;

    m_commandRemainder.append(effects);
    // visible before its first operation takes native effect
    if (effects.isEmpty() == false)
        publishCommandWork();
    if (m_settlingFunction != ShowCommand::InvalidId)
    {
        if (commandSettled() == false)
            return;
        m_settlingFunction = ShowCommand::InvalidId;
    }

    if (m_pendingRollback != NoCommandSeek && m_heldControlBatch == 0)
    {
        const quint32 to = quint32(std::exchange(m_pendingRollback, NoCommandSeek));
        if (m_show->commandTraversalCurrent(m_commandRun, m_commandTraversal) == false)
        {
            m_commandRemainder.clear();
            return;
        }
        rollBackLegacyEffects(to);
        // the next pass waits until the executor rolled its controls back
        if (commandCatchUp())
        {
            m_heldControlBatch = m_show->publishControlBatch(m_commandRun, m_commandTraversal, QVector<ShowCommand>(),
                                                             m_commandRemainder,
                                                             std::exchange(m_engineStarted, QVector<quint32>()),
                                                             std::exchange(m_engineStopped, QVector<quint32>()), to);
            if (m_heldControlBatch == 0)
            {
                m_commandRemainder.clear();
                return;
            }
        }
    }

    while (m_commandRemainder.isEmpty() == false)
    {
        if (m_heldControlBatch != 0)
            return;

        int run = 0;
        while (executor && run < m_commandRemainder.count() && isControl(m_commandRemainder.at(run)))
            run++;

        if (run == 0)
        {
            const ShowCommand cmd = m_commandRemainder.takeFirst();
            // a Stop retired this run, or a seek or source switch ended its traversal,
            // while its last operation executed: none of the rest may run
            if (m_show->commandTraversalCurrent(m_commandRun, m_commandTraversal) == false)
            {
                m_commandRemainder.clear();
                return;
            }
            if (m_commandCatchUp == false)
            {
                applyCommandEffect(cmd);
                continue;
            }
            // a seek or stop is taking over: nothing of this traversal may run
            if (m_show->controlWorkCancelled(m_commandRun))
            {
                m_commandRemainder.clear();
                return;
            }
            applyCommandEffect(cmd);
            if (cmd.action == ShowCommandAction::Start || cmd.action == ShowCommandAction::Stop)
            {
                // ponytail: one Start or Stop per tick, settle several targets at once if lag matters
                m_settlingFunction = cmd.functionId;
                m_settlingStart = cmd.action == ShowCommandAction::Start;
                return;
            }
            continue;
        }

        m_heldControlBatch = m_show->publishControlBatch(m_commandRun, m_commandTraversal,
                                                         m_commandRemainder.mid(0, run),
                                                         m_commandRemainder.mid(run),
                                                         std::exchange(m_engineStarted, QVector<quint32>()),
                                                         std::exchange(m_engineStopped, QVector<quint32>()));
        // a seek, stop or source switch ended this traversal: nothing after the run may execute
        if (m_heldControlBatch == 0)
        {
            m_commandRemainder.clear();
            return;
        }
        m_commandRemainder.remove(0, run);
    }
    // the jump drained: later crossings dispatch as ordinary playback
    if (m_heldControlBatch == 0)
        m_commandCatchUp = false;
}

/** Diagnostics from the timer thread: numeric identity only, never a label
 *  the GUI may be changing */
static void traceCommandEffect(quint64 g, const Show *show, quint64 traversal, quint32 elapsed,
                               const ShowCommand &cmd, ShowEventLog::Outcome outcome, const char *reason)
{
    if (g == 0)
        return;
    ShowEventLog::Entry e;
    e.generation = g;
    e.phase = ShowEventLog::Phase::Execute;
    e.outcome = outcome;
    e.origin = int(ShowCommandOrigin::Replay);
    e.showId = show->id();
    e.traversal = traversal;
    e.showTimeMs = elapsed;
    e.commandId = cmd.id;
    e.functionId = cmd.functionId;
    e.control = QStringLiteral("Function %1").arg(cmd.functionId);
    e.action = ShowCommand::actionToString(cmd.action);
    if (cmd.action == ShowCommandAction::SetIntensity)
        e.value = QString::number(cmd.intensity * 100.0, 'f', 1) + QLatin1Char('%');
    e.reason = QLatin1String(reason);
    ShowEventLog::append(e, true);
}

void ShowRunner::applyCommandEffect(const ShowCommand &cmd)
{
    if (m_show == NULL)
        return;
    // diagnostics of this operation belong to the observation it started in
    const quint64 traced = ShowEventLog::generation();
    if (cmd.functionId == m_show->id())
    {
        traceCommandEffect(traced, m_show, m_commandTraversal, m_elapsedTime, cmd, ShowEventLog::Outcome::Skipped,
                           "a Show never commands itself");
        return;
    }

    Function *f = m_doc->function(cmd.functionId);
    if (f == NULL)
    {
        traceCommandEffect(traced, m_show, m_commandTraversal, m_elapsedTime, cmd, ShowEventLog::Outcome::Skipped,
                           "no such function");
        ShowEventLog::failure(QStringLiteral("Replay skipped: function %1 is missing").arg(cmd.functionId),
                              ShowEventLog::View::Events);
        return;
    }

    // what this effect finds and leaves, so a backward move can undo it
    const bool journaled = cmd.action == ShowCommandAction::Start || cmd.action == ShowCommandAction::Stop ||
                           cmd.action == ShowCommandAction::SetIntensity;
    LegacyEffect effect{ cmd.time, cmd.functionId, commandTargetActive(f), false, intensityOf(f), 0.0 };
    const auto journal = qScopeGuard([&]() {
        if (journaled == false)
            return;
        effect.runningAfter = commandTargetActive(f);
        effect.intensityAfter = intensityOf(f);
        m_legacyJournal.append(effect);
    });

    switch (cmd.action)
    {
        case ShowCommandAction::Start:
        {
            /** Native ownership rules decide the rest: a target this Show
             *  already plays gains no second activation, and one somebody else
             *  started keeps its own progress instead of being reset */
            if (commandTargetActive(f) == false)
                prepareCommandStart(f);

            f->start(m_doc->masterTimer(), functionParent());
            noteEngineStart(f);
            m_commandOwnedFunctions.insert(cmd.functionId);
            m_commandStartedThisTick.insert(cmd.functionId);
            traceCommandEffect(traced, m_show, m_commandTraversal, m_elapsedTime, cmd, ShowEventLog::Outcome::Requested,
                               "start requested; native ownership decides the rest");
        }
        break;

        case ShowCommandAction::Stop:
        {
            f->stop(functionParent());
            noteEngineStop(f);
            m_commandOwnedFunctions.remove(cmd.functionId);
            retireClipDeadline(cmd.functionId);
            traceCommandEffect(traced, m_show, m_commandTraversal, m_elapsedTime, cmd, ShowEventLog::Outcome::Requested,
                               "this Show's playback released; other owners keep theirs");
        }
        break;

        case ShowCommandAction::SetIntensity:
        {
            const bool active = traced != 0 && commandTargetActive(f);
            applyCommandIntensity(f, cmd.intensity);
            if (traced != 0)
                traceCommandEffect(traced, m_show, m_commandTraversal, m_elapsedTime, cmd,
                                   active ? ShowEventLog::Outcome::Applied : ShowEventLog::Outcome::Skipped,
                                   active ? "intensity override set" : "target not running");
        }
        break;

        // VC state records target controls, never a Function
        case ShowCommandAction::SetButtonState:
        case ShowCommandAction::SetSliderPosition:
        break;
    }
}

bool ShowRunner::commandSettled() const
{
    // a start queued by anyone, even the restart of a running target, is not run yet
    const Function *f = m_doc->function(m_settlingFunction);
    if (f != NULL && m_doc->masterTimer()->isStartQueued(f))
        return false;

    return m_show->functionSettled(m_settlingFunction, m_settlingStart);
}

void ShowRunner::drainCommands()
{
    if (m_show == NULL)
        return;

    dropDiscardedTraversal();

    if (m_show->controlWorkCancelled(m_commandRun))
    {
        m_commandRemainder.clear();
        m_heldControlBatch = 0;
        m_settlingFunction = ShowCommand::InvalidId;
    }
    else
    {
        m_commandStartedThisTick.clear();
        dispatchCommandEffects(QVector<ShowCommand>());
    }
    publishCommandWork();
}

void ShowRunner::publishCommandWork()
{
    if (m_show == NULL)
        return;

    const bool settling = m_settlingFunction != ShowCommand::InvalidId;
    const bool owed = settling || m_heldControlBatch != 0 || m_commandRemainder.isEmpty() == false;
    if (owed == false && m_commandWorkPublished == false)
        return;

    QVector<ShowCommand> work = m_commandRemainder;
    if (settling)
        work.prepend(m_settlingStart ? ShowCommand::start(ShowCommand::InvalidId, 0, m_settlingFunction)
                                     : ShowCommand::stop(ShowCommand::InvalidId, 0, m_settlingFunction));
    m_show->setCommandWork(m_commandRun, m_commandTraversal, m_heldControlBatch, work);
    m_commandWorkPublished = owed;
}

void ShowRunner::noteEngineStart(const Function *f)
{
    m_engineStopped.removeAll(f->id());
    if (m_engineStarted.contains(f->id()) == false)
        m_engineStarted.append(f->id());
}

void ShowRunner::noteEngineStop(const Function *f)
{
    m_engineStarted.removeAll(f->id());
    if (m_engineStopped.contains(f->id()) == false)
        m_engineStopped.append(f->id());
}

void ShowRunner::prepareCommandStart(Function *f)
{
    if (f->type() != Function::ChaserType && f->type() != Function::SequenceType)
        return;

    Chaser *chaser = qobject_cast<Chaser *>(f);
    if (chaser == NULL)
        return;

    ChaserAction action;
    action.m_action = ChaserSetStepIndex;
    action.m_stepIndex = 0;
    action.m_masterIntensity = 1.0;
    action.m_stepIntensity = 1.0;
    action.m_fadeMode = Chaser::FromFunction;
    chaser->setAction(action);
}

bool ShowRunner::commandTargetActive(const Function *function) const
{
    /** Queued during this very tick: the timer starts it at the end of the
     *  tick, so it is not observable as running yet */
    if (m_commandStartedThisTick.contains(function->id()))
        return true;

    /** Otherwise ask the Function itself, on the thread that owns that state.
     *  Scheduler membership is not liveness: a queued clip end outlives a
     *  target the user shut down in the middle of its clip. */
    return function->isRunning() && function->stopped() == false;
}

void ShowRunner::applyCommandIntensity(Function *f, qreal value)
{
    // a value on a stopped target does nothing, and never starts it
    if (commandTargetActive(f) == false)
        return;

    /** The native Intensity override is Single: requesting it returns the one
     *  shared handle for that attribute and applies the value in the same
     *  call. Caching the identifier would only risk driving whatever attribute
     *  inherits that number after a reset. */
    f->requestAttributeOverride(Function::Intensity, value);
}

void ShowRunner::retireClipDeadline(quint32 functionId)
{
    for (int i = m_runningQueue.count() - 1; i >= 0; i--)
    {
        const Function *queued = m_runningQueue.at(i).first;
        if (queued != NULL && queued->id() == functionId)
            m_runningQueue.removeAt(i);
    }
}

void ShowRunner::retireLiveStopDeadlines(const ShowLiveMarks &liveMarks)
{
    if (liveMarks.isEmpty())
        return;

    // from the captured occurrence, not the current track: an edit that moved,
    // retargeted or deleted the Stop since does not undo what ran live
    for (auto it = liveMarks.cbegin(); it != liveMarks.cend(); ++it)
    {
        if (it.value().stopFunctionId == ShowCommand::InvalidId || m_retiredLiveStopIds.contains(it.key()))
            continue;

        // bookkeeping only: the recorder already performed the Stop itself
        m_retiredLiveStopIds.insert(it.key());
        retireClipDeadline(it.value().stopFunctionId);
    }
}

void ShowRunner::rollBackLegacyEffects(quint32 time)
{
    // per target, the state before the first effect after time, in journal order
    QVector<LegacyEffect> first;
    for (const LegacyEffect &effect : std::as_const(m_legacyJournal))
    {
        if (effect.time <= time)
            continue;
        const bool known = std::any_of(first.cbegin(), first.cend(), [&effect](const LegacyEffect &earlier) {
            return earlier.functionId == effect.functionId;
        });
        if (known == false)
            first.append(effect);
    }
    m_legacyJournal.erase(std::remove_if(m_legacyJournal.begin(), m_legacyJournal.end(),
                                         [time](const LegacyEffect &effect) { return effect.time > time; }),
                          m_legacyJournal.end());

    // stops before starts; a target already in its earlier state is left alone
    for (const bool start : { false, true })
    {
        for (const LegacyEffect &effect : std::as_const(first))
        {
            Function *f = m_doc->function(effect.functionId);
            if (f == NULL || effect.runningBefore != start || commandTargetActive(f) == start)
                continue;
            if (start)
            {
                prepareCommandStart(f);
                f->start(m_doc->masterTimer(), functionParent());
                noteEngineStart(f);
                m_commandOwnedFunctions.insert(effect.functionId);
                m_commandStartedThisTick.insert(effect.functionId);
            }
            else
            {
                f->stop(functionParent());
                noteEngineStop(f);
                m_commandOwnedFunctions.remove(effect.functionId);
            }
        }
    }
    for (const LegacyEffect &effect : std::as_const(first))
    {
        Function *f = m_doc->function(effect.functionId);
        if (f != NULL && effect.runningBefore && intensityOf(f) != effect.intensityBefore)
            applyCommandIntensity(f, effect.intensityBefore);
    }
}

void ShowRunner::stopCommandOwnedFunctions()
{
    const QSet<quint32> owned = m_commandOwnedFunctions;
    m_commandOwnedFunctions.clear();
    m_commandStartedThisTick.clear();
    m_retiredLiveStopIds.clear();

    foreach (quint32 functionId, owned)
    {
        Function *f = m_doc->function(functionId);
        if (f != NULL)
            f->stop(functionParent());
    }
}

quint32 ShowRunner::commandExtent() const
{
    return m_commandTrack.extent();
}

void ShowRunner::syncPerformAudioSuppression()
{
    const bool requested = (m_show != NULL) ? m_show->performAudioSuppressed() : false;
    if (requested == m_performAudioSuppressed)
        return;

    m_performAudioSuppressed = requested;
    if (m_performAudioSuppressed)
        stopRunningAudioFunctions();
}

bool ShowRunner::isAudioFunction(const Function *function) const
{
    return function != NULL && function->type() == Function::AudioType;
}

bool ShowRunner::isFunctionScheduled(Function *function, quint32 stopTime) const
{
    for (int i = 0; i < m_runningQueue.count(); ++i)
    {
        if (m_runningQueue.at(i).first == function && m_runningQueue.at(i).second == stopTime)
            return true;
    }

    return false;
}

void ShowRunner::markRunningAudioFunctionsDeferred(const QList<ShowFunction *> &functions,
                                                   QSet<quint32> &deferred,
                                                   quint32 elapsed)
{
    foreach (ShowFunction *sf, functions)
    {
        if (sf == NULL)
            continue;

        const quint32 startTime = sf->startTime();
        const quint32 stopTime = startTime + sf->duration(m_doc);
        if (elapsed < startTime || elapsed >= stopTime)
            continue;

        Function *f = m_doc->function(sf->functionID());
        if (!isAudioFunction(f))
            continue;

        deferred.insert(sf->id());
    }
}

void ShowRunner::startDeferredAudioFunctions(const QList<ShowFunction *> &functions,
                                             QSet<quint32> &deferred,
                                             quint32 elapsed)
{
    foreach (ShowFunction *sf, functions)
    {
        if (sf == NULL)
            continue;

        if (!deferred.contains(sf->id()))
            continue;

        const quint32 startTime = sf->startTime();
        const quint32 stopTime = startTime + sf->duration(m_doc);
        if (stopTime <= elapsed)
        {
            deferred.remove(sf->id());
            continue;
        }

        Function *f = m_doc->function(sf->functionID());
        if (!isAudioFunction(f))
        {
            deferred.remove(sf->id());
            continue;
        }

        if (elapsed < startTime)
            continue;

        if (!isFunctionScheduled(f, stopTime))
        {
            const quint32 offset = elapsed > startTime ? elapsed - startTime : 0;
            applyTrackIntensity(sf, f);
            f->start(m_doc->masterTimer(), functionParent(), offset);
            noteEngineStart(f);
            m_runningQueue.append(QPair<Function *, quint32>(f, stopTime));
        }

        deferred.remove(sf->id());
    }
}

void ShowRunner::stopRunningAudioFunctions()
{
    markRunningAudioFunctionsDeferred(m_timeFunctions, m_deferredTimeAudioFunctionIds, m_elapsedTime);
    if (beatSynced)
        markRunningAudioFunctionsDeferred(m_beatFunctions, m_deferredBeatAudioFunctionIds, m_elapsedBeats);

    for (int i = m_runningQueue.count() - 1; i >= 0; --i)
    {
        Function *f = m_runningQueue.at(i).first;
        if (!isAudioFunction(f))
            continue;

        f->stop(functionParent());
        if (Audio *audio = qobject_cast<Audio *>(f))
            audio->stopOutput();
        m_runningQueue.removeAt(i);
    }
}

void ShowRunner::applyTrackIntensity(ShowFunction *sf, Function *f)
{
    foreach (Track *track, m_show->tracks())
    {
        if (track->showFunctions().contains(sf))
        {
            int intOverrideId = f->requestAttributeOverride(Function::Intensity, m_intensityMap[track->id()]);
            sf->setIntensityOverrideId(intOverrideId);
            break;
        }
    }
}

/************************************************************************
 * Intensity
 ************************************************************************/

void ShowRunner::adjustIntensity(qreal fraction, const Track *track)
{
    if (track == NULL)
        return;

    qDebug() << Q_FUNC_INFO << "Track ID: " << track->id() << ", val:" << fraction;
    m_intensityMap[track->id()] = fraction;

    foreach (ShowFunction *sf, track->showFunctions())
    {
        Function *f = m_doc->function(sf->functionID());
        if (f == NULL)
            continue;

        for (int i = 0; i < m_runningQueue.count(); i++)
        {
            Function *rf = m_runningQueue.at(i).first;
            if (f == rf)
                f->adjustAttribute(fraction, sf->intensityOverrideId());
        }
    }
}
