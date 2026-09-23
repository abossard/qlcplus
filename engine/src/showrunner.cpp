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
#include <QDebug>

#include "showrunner.h"
#include "audio.h"
#include "chaser.h"
#include "function.h"
#include "track.h"
#include "show.h"
#include "doc.h"
#include "inputoutputmap.h"

#define TIMER_INTERVAL 50

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
    , m_pendingCommandSeek(NoCommandSeek)
{
    Q_ASSERT(m_doc != NULL);
    Q_ASSERT(showID != Show::invalidId());

    m_show = qobject_cast<Show*>(m_doc->function(showID));
    if (m_show == NULL)
        return;
    m_performAudioSuppressed = m_show->performAudioSuppressed();

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
     *  everything before it is history whose values get restored, never
     *  triggers that get replayed */
    refreshCommandTrack();
    m_commandState.playing = true;
    m_commandState = ShowCommandFsm::seek(m_commandTrack, m_commandState, startTime).state;
    m_pendingCommandSeek = startTime;

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
    m_pendingCommandSeek = NoCommandSeek;
    if (m_show != NULL)
        m_show->commandTraversalRestarted();

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

    // --- External sync: update elapsed time from external source ---
    if (m_syncSource == External)
    {
        quint32 newTime = m_externalElapsedTime.load(std::memory_order_relaxed);

        // Backward seek detection: if external time moved backward,
        // stop all running functions and reset scan indices
        if (newTime < m_elapsedTime && m_elapsedTime > 0)
        {
            seekTo(newTime);
        }

        m_elapsedTime = newTime;
        const int songBpm = m_show->timeDivisionBPM() > 0 ? m_show->timeDivisionBPM() : 120;
        m_elapsedBeats = qRound64(double(newTime) * songBpm / 60.0);
        beatSynced = true;
    }

    const quint64 requestedSeekTime =
            m_requestedSeekTime.exchange(NoSeekRequested, std::memory_order_acquire);
    if (requestedSeekTime != NoSeekRequested && m_syncSource == Autonomous)
        seekTo(quint32(requestedSeekTime));

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
            // remove it from the running queue
            m_runningQueue.removeAt(i);
        }
    }

    // Phase 3. Apply the commands this position is due for. They run after the
    // clip phases, so for the same millisecond a command is the newer intent.
    refreshCommandTrack();
    processCommands(seekRestartPending);
    if (m_show != NULL)
        m_show->setCommandPosition(m_elapsedTime);

    // Phase 4. Check if this is the end of the Show. A Show can mix
    // time-based and beat-based tracks, so it is only really over once
    // both the time-based and the beat-based timelines have completed.
    // While there are beat-based Functions but no beat has been detected
    // yet, the beat timeline hasn't even started, so it can't be "done".
    // An authored command extent and an armed recorder extend that end.
    bool timeDone = m_elapsedTime >= m_totalRunTime && m_elapsedTime >= commandExtent();
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

void ShowRunner::setExternalElapsedTime(quint32 ms)
{
    m_externalElapsedTime.store(ms, std::memory_order_relaxed);
}

void ShowRunner::requestSeek(quint32 ms)
{
    m_requestedSeekTime.store(ms, std::memory_order_release);
}

void ShowRunner::seekTo(quint32 newTime)
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

    /** A jump ends this traversal: command-owned playback is released and the
     *  live no-echo suppression of the previous pass is over. The authored
     *  values of the destination are restored once its clips are scheduled. */
    stopCommandOwnedFunctions();
    if (m_show != NULL)
        m_show->commandTraversalRestarted();
    m_pendingCommandSeek = newTime;
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

    QSet<quint32> alreadyApplied;
    m_commandTrack = m_show->commandTrackSnapshot(&alreadyApplied);

    /** The recorder executed these live, so the traversal owes them nothing.
     *  The transition logic drops each one again as soon as the playhead
     *  consumes its time, and a seek clears them for the next traversal. */
    m_commandState.consumedLiveEventIds.unite(alreadyApplied);
    retireLiveStopDeadlines(alreadyApplied);
}

void ShowRunner::processCommands(bool traversalStalled)
{
    if (m_show == NULL)
        return;

    /** A seek that is still waiting for a Function to finish stopping has not
     *  resumed the traversal yet. Consuming commands now would swallow the
     *  ones authored exactly at the destination. */
    if (traversalStalled)
        return;

    m_commandStartedThisTick.clear();

    if (m_pendingCommandSeek != NoCommandSeek)
    {
        const quint32 destination = quint32(m_pendingCommandSeek);
        m_pendingCommandSeek = NoCommandSeek;

        const ShowCommandTransition restored =
                ShowCommandFsm::seek(m_commandTrack, m_commandState, destination);
        m_commandState = restored.state;

        // values only: historical Start/Stop are never replayed by a seek
        for (const ShowCommand &cmd : restored.effects)
            applyCommandEffect(cmd);
    }

    m_commandState.playing = true;
    const ShowCommandTransition played =
            ShowCommandFsm::advance(m_commandTrack, m_commandState, m_elapsedTime);
    m_commandState = played.state;

    for (const ShowCommand &cmd : played.effects)
        applyCommandEffect(cmd);
}

void ShowRunner::applyCommandEffect(const ShowCommand &cmd)
{
    if (m_show == NULL || cmd.functionId == m_show->id())
        return;

    Function *f = m_doc->function(cmd.functionId);
    if (f == NULL)
        return;

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
            m_commandOwnedFunctions.insert(cmd.functionId);
            m_commandStartedThisTick.insert(cmd.functionId);
        }
        break;

        case ShowCommandAction::Stop:
        {
            f->stop(functionParent());
            m_commandOwnedFunctions.remove(cmd.functionId);
            retireClipDeadline(cmd.functionId);
        }
        break;

        case ShowCommandAction::SetIntensity:
            applyCommandIntensity(f, cmd.intensity);
        break;
    }
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

void ShowRunner::retireLiveStopDeadlines(const QSet<quint32> &liveIds)
{
    if (liveIds.isEmpty())
        return;

    for (const ShowCommand &cmd : m_commandTrack.commands())
    {
        if (cmd.action != ShowCommandAction::Stop)
            continue;

        if (liveIds.contains(cmd.id) == false || m_retiredLiveStopIds.contains(cmd.id))
            continue;

        // bookkeeping only: the recorder already performed the Stop itself
        m_retiredLiveStopIds.insert(cmd.id);
        retireClipDeadline(cmd.functionId);
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
