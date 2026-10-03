/*
  Q Light Controller Plus
  showcommandrecorder.cpp

  Copyright (c) QLC+ contributors

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

#include "showcommandrecorder.h"

#include <QLocale>
#include <QScopedValueRollback>
#include <cmath>

#include "collection.h"
#include "doc.h"
#include "performfsm.h"
#include "show.h"
#include "showrunner.h"
#include "tardis.h"
#include "vcbutton.h"
#include "vcpage.h"
#include "vcslider.h"
#include "vcsoloframe.h"
#include "virtualconsole.h"



ShowCommandRecorder *ShowCommandRecorder::s_instance = nullptr;

using Phase = ShowEventLog::Phase;
using Outcome = ShowEventLog::Outcome;

/** The Function a control's native op acts on, InvalidId for none */
static quint32 controlFunction(VCWidget *control)
{
    if (VCButton *button = qobject_cast<VCButton *>(control))
        return button->functionID();
    VCSlider *slider = qobject_cast<VCSlider *>(control);
    return slider != nullptr && slider->sliderMode() == VCSlider::Adjust ? slider->controlledFunction()
                                                                         : ShowCommand::InvalidId;
}

ShowCommandRecorder::ShowCommandRecorder(Doc *doc, QObject *parent)
    : QObject(parent)
    , m_doc(doc)
{
    s_instance = this;

    connect(m_doc, &Doc::functionAdded, this, &ShowCommandRecorder::slotFunctionAdded);
    connect(m_doc, &Doc::functionRemoved, this, &ShowCommandRecorder::slotFunctionRemoved);
    connect(m_doc, &Doc::clearing, this, &ShowCommandRecorder::dropUserRequests);
    // a new workspace starts a new undo history and an empty clipboard
    connect(m_doc, &Doc::clearing, this, [this]()
    {
        m_eventIdFloors.clear();
        m_clipboardDeletedTargets.clear();
        endEditSession(tr("The workspace was replaced during this edit"));
        if (m_clipboard.isEmpty() == false)
        {
            m_clipboard.clear();
            emit clipboardChanged();
        }
    });
    connect(this, &ShowCommandRecorder::stateChanged, this, &ShowCommandRecorder::editGateChanged);
    connect(this, &ShowCommandRecorder::commandsChanged, this, &ShowCommandRecorder::observeRowSources);
    // the referenced controls follow the tracked Show's track while observed
    connect(this, &ShowCommandRecorder::commandsChanged, this, [this]()
    {
        if (m_referencesObserved)
            emit referencesChanged();
    });
    for (Function *function : m_doc->functionsByType(Function::ShowType))
        attachShow(qobject_cast<Show *>(function));
}

ShowCommandRecorder::~ShowCommandRecorder()
{
    if (s_instance == this)
        s_instance = nullptr;
    for (Show *show : std::as_const(m_attachedShows))
    {
        if (show != nullptr)
            show->attachControlExecutor(false);
    }
}

ShowCommandRecorder *ShowCommandRecorder::instance()
{
    return s_instance;
}

void ShowCommandRecorder::setVirtualConsole(VirtualConsole *vc)
{
    m_vc = vc;
}

/*********************************************************************
 * Transport and binding
 *********************************************************************/

bool ShowCommandRecorder::isRecording() const
{
    return m_state.recording;
}

bool ShowCommandRecorder::setRecording(bool on)
{
    const ShowCommandState next = ShowCommandFsm::setRecording(m_state, on);
    if (next == m_state)
        return true;

    const bool diag = ShowEventLog::generation() != 0;
    if (isAuthoring() && checkpoint() == false)
    {
        if (diag)
            traceTransport(Outcome::Failed, on ? tr("REC on") : tr("REC off"), phaseName(), m_lastError);
        return false;
    }

    enterRecordingState(next);
    if (diag)
        traceTransport(Outcome::Applied, on ? tr("REC on") : tr("REC off"), phaseName());
    return true;
}

void ShowCommandRecorder::discardRecording()
{
    m_unpublished.clear();
    m_hostCursorShowId = ShowCommand::InvalidId;
    // nothing resolved in the old workspace resolves in the next one
    const ShowCommandState next =
        ShowCommandFsm::setResolvedShow(ShowCommandFsm::setRecording(m_state, false), ShowCommand::InvalidId);
    if (next != m_state)
        enterRecordingState(next);
}

void ShowCommandRecorder::enterRecordingState(const ShowCommandState &next)
{
    Show *previous = trackedShow();
    m_state = next;
    m_takeId++;
    reloadTrack();

    Show *bound = trackedShow();
    if (bound != nullptr && isAuthoring())
    {
        m_clockPosition = bound->commandPosition();
        m_state.position = m_clockPosition;
    }
    if (previous != nullptr && previous != bound)
        previous->setCommandRecording(false);
    if (bound != nullptr)
        bound->setCommandRecording(m_state.phase() == ShowRecordPhase::Bound);

    emit stateChanged();
}

bool ShowCommandRecorder::setResolvedShow(quint32 showId)
{
    ShowCommandState next = ShowCommandFsm::setResolvedShow(m_state, showId);
    if (next == m_state)
        return true;

    // Only the automatic source still changes the Show while a take is bound:
    // the take on it closes and Record rearms on the next Show, or waits armed.
    bool handed = true;
    if (m_state.recording && m_state.boundShowId != ShowCommand::InvalidId && showId != m_state.boundShowId)
    {
        const QString previous = targetName();
        QString reason;
        handed = publishCheckpoint(&reason);
        // a take that cannot close disarms: its accepted input stays for a later checkpoint
        next = ShowCommandFsm::setRecording(next, false);
        if (handed)
            next = ShowCommandFsm::setRecording(next, true);
        else
            reportFailure(tr("Recording stopped: %1 could not be saved. %2").arg(previous, reason));
    }

    Show *previous = trackedShow();
    m_state = next;
    m_takeId++;
    reloadTrack();

    Show *bound = trackedShow();
    if (bound != nullptr && isAuthoring())
    {
        m_clockPosition = bound->commandPosition();
        m_state.position = m_clockPosition;
    }
    if (previous != nullptr && previous != bound)
        previous->setCommandRecording(false);
    if (bound != nullptr)
        bound->setCommandRecording(isAuthoring());

    if (ShowEventLog::generation() != 0)
        traceTransport(handed ? Outcome::Applied : Outcome::Failed, tr("Show resolved"), targetName(),
                       handed ? phaseName() : m_lastError);
    emit stateChanged();
    return handed;
}

void ShowCommandRecorder::followPerform(PerformFsm *fsm)
{
    // the same follow as the Show Manager's, on both signals: engaging Perform
    // on an unchanged deck changes only the state
    const auto follow = [this, fsm]()
    {
        if (fsm->state() != PerformFsm::PerformState::Idle)
            setResolvedShow(fsm->activeShowId());
    };
    connect(fsm, &PerformFsm::stateChanged, this, follow);
    connect(fsm, &PerformFsm::activeShowChanged, this, follow);
}

bool ShowCommandRecorder::isAuthoring() const
{
    return m_state.phase() == ShowRecordPhase::Bound;
}

void ShowCommandRecorder::setPlaying(bool playing)
{
    // a transport change of the host may have started or stopped the Show
    emit editGateChanged();
    const ShowCommandState next = ShowCommandFsm::setPlaying(m_state, playing);
    if (next == m_state)
        return;

    m_state = next;
    if (ShowEventLog::generation() != 0)
        traceTransport(Outcome::Applied, playing ? tr("Playing") : tr("Not playing"), targetName());
    emit stateChanged();
}

quint64 ShowCommandRecorder::takeId() const
{
    return m_takeId;
}

quint32 ShowCommandRecorder::boundShowId() const
{
    return m_state.boundShowId;
}

bool ShowCommandRecorder::checkpoint()
{
    QString error;
    if (publishCheckpoint(&error))
        return true;

    reportFailure(error);
    return false;
}

bool ShowCommandRecorder::publishCheckpoint(QString *error)
{
    const ShowTraceCause operation = newCause();
    QScopedValueRollback<const ShowTraceCause *> scoped(m_operation,
                                                        operation.generation != 0 ? &operation : nullptr);
    const bool published = publishAccepted(error);
    if (operation.generation == 0 || operation.generation != ShowEventLog::generation())
        return published;
    ShowEventLog::Entry e = causeEntry(operation, Phase::Transport, published ? Outcome::Applied : Outcome::Failed,
                                       published ? tr("accepted input ends at its content") : *error);
    if (e.generation != 0)
    {
        e.action = tr("Checkpoint");
        e.value = targetName();
        ShowEventLog::append(e);
    }
    return published;
}

bool ShowCommandRecorder::publishAccepted(QString *error)
{
    // the take's Show ends at its content even when it accepted nothing new
    QVector<quint32> shows;
    if (m_state.recording && m_state.boundShowId != ShowCommand::InvalidId)
        shows.append(m_state.boundShowId);
    for (const Unpublished &accepted : std::as_const(m_unpublished))
    {
        if (!shows.contains(accepted.showId))
            shows.append(accepted.showId);
    }

    bool published = true;
    for (quint32 showId : std::as_const(shows))
    {
        Show *show = qobject_cast<Show *>(m_doc->function(showId));
        if (show == nullptr || publishUnpublished(show, error) == false)
            published = false;
    }
    return published;
}

void ShowCommandRecorder::discardUnpublished()
{
    m_unpublished.clear();
}

bool ShowCommandRecorder::publishUnpublished(Show *show, QString *error)
{
    // traversals only grow, so only the latest accepted one can still be
    // current; the Show decides that as it publishes: live ids must not echo,
    // a later traversal replays them
    quint64 latest = 0;
    for (const Unpublished &accepted : std::as_const(m_unpublished))
    {
        if (accepted.showId == show->id())
            latest = qMax(latest, accepted.traversal);
    }

    ShowCommandTrack candidate = show->commandTrack();
    QSet<quint32> executed;
    for (const Unpublished &accepted : std::as_const(m_unpublished))
    {
        if (accepted.showId != show->id())
            continue;
        if (candidate.insert(accepted.command, error) == false)
            return false;
        if (accepted.traversal == latest)
            executed.insert(accepted.command.id);
    }
    // nothing new and already ending at its content: nothing to publish
    if (candidate.count() == show->commandTrack().count() && candidate.extent() == candidate.lastCommandTime())
        return true;
    const quint32 extentBefore = show->commandTrack().extent();
    candidate.setExtent(candidate.lastCommandTime());

    if (publishTrack(candidate, executed, latest, show, error) == false)
        return false;
    if (const quint64 g = ShowEventLog::generation())
    {
        // each command under the input it came from; the extent they derive
        // only when every one of them was observed, never backfilled
        const ShowTraceCause *extentCause = m_operation != nullptr && m_operation->generation == g ? m_operation : nullptr;
        bool unobserved = false;
        for (const Unpublished &accepted : std::as_const(m_unpublished))
        {
            if (accepted.showId != show->id())
                continue;
            if (accepted.cause.generation != g)
            {
                unobserved = true;
                continue;
            }
            traceCommand(accepted.cause, Phase::Commit, Outcome::Published, nullptr, &accepted.command, show->id(),
                         tr("added"));
            extentCause = &accepted.cause;
        }
        if (extentBefore != candidate.extent() && extentCause != nullptr && !unobserved)
            traceExtent(*extentCause, show->id(), extentBefore, candidate.extent(), tr("extent follows the content"));
    }
    m_doc->setModified();

    m_unpublished.erase(std::remove_if(m_unpublished.begin(), m_unpublished.end(),
                                       [show](const Unpublished &accepted) { return accepted.showId == show->id(); }),
                        m_unpublished.end());
    return true;
}

int ShowCommandRecorder::phaseValue() const
{
    return int(m_state.phase());
}

QString ShowCommandRecorder::phaseName() const
{
    switch (m_state.phase())
    {
        case ShowRecordPhase::Armed: return tr("Armed");
        case ShowRecordPhase::Bound: return tr("Recording");
        case ShowRecordPhase::Suspended: return tr("Suspended");
        case ShowRecordPhase::Off: break;
    }
    return tr("Off");
}

QString ShowCommandRecorder::targetName() const
{
    Show *show = trackedShow();
    return show != nullptr ? show->name() : QString();
}

int ShowCommandRecorder::position() const
{
    Show *show = trackedShow();
    return show != nullptr ? int(show->commandPosition()) : 0;
}

quint32 ShowCommandRecorder::acceptancePosition(const Show *show) const
{
    // the traversal is about to restart there: input now belongs to that pass
    if (const std::optional<quint32> seek = show->pendingBackwardSeek())
        return *seek;
    // a cursor only stands for an autonomous clock; an external one is the position
    const bool cursor = m_hostCursorShowId == show->id() && show->syncSource() == ShowRunner::Autonomous;
    return cursor ? m_hostCursor : show->commandPosition();
}

void ShowCommandRecorder::setHostCursor(quint32 showId, quint32 ms)
{
    m_hostCursorShowId = showId;
    m_hostCursor = ms;
    if (ShowEventLog::generation() != 0)
        traceTransport(Outcome::Applied, tr("Cursor moved"), QString::number(ms / 1000.0, 'f', 3),
                       tr("applies nothing until Play"));
}

QString ShowCommandRecorder::lastError() const
{
    return m_lastError;
}

/*********************************************************************
 * Input seam
 *********************************************************************/

bool ShowCommandRecorder::submitUserInput(const ShowCommandInput &input)
{
    return submitUserInput(QVector<ShowCommandInput>{input});
}

bool ShowCommandRecorder::submitUserInput(const QVector<ShowCommandInput> &inputs)
{
    Show *show = trackedShow();
    if (show == nullptr || isAuthoring() == false)
        return false;

    syncPosition(show);
    return authorAt(inputs, m_state.position);
}

bool ShowCommandRecorder::authorAt(const QVector<ShowCommandInput> &inputs, quint32 positionMs)
{
    Show *show = trackedShow();
    if (show == nullptr || isAuthoring() == false)
        return false;

    // An input carrying an older timestamp is late, not a rewind: the clock
    // says when a segment ended, never the order inputs happen to arrive in.
    m_state.position = positionMs;

    bool authored = false;
    for (const ShowCommandInput &input : inputs)
    {
        // ids stay unique across input the Show has not taken yet
        const ShowCommandTrack accepted = withUnpublished(show);
        const ShowCommandTransition transition = ShowCommandFsm::userInput(accepted, m_state, input);
        if (transition.error.isEmpty())
            setError(transition.error);
        else
            reportFailure(transition.error);
        if (transition.authored.isEmpty())
        {
            if (ShowEventLog::generation() != 0)
                traceDecision(transition.error.isEmpty() ? Outcome::Ignored : Outcome::Rejected,
                              transition.error.isEmpty() ? tr("not recordable") : transition.error, &input);
            continue;
        }

        if (ShowEventLog::generation() != 0)
            traceDecision(Outcome::Recorded, QString(), &input);
        if (commit(transition, show))
            authored = true;
    }

    return authored;
}

ShowCommandTrack ShowCommandRecorder::withUnpublished(const Show *show) const
{
    ShowCommandTrack accepted = show->commandTrack();
    for (const Unpublished &pending : std::as_const(m_unpublished))
    {
        if (pending.showId == show->id())
            accepted.insert(pending.command);
    }
    return accepted;
}

void ShowCommandRecorder::reportUnsupported(const QString &what, ShowCommandOrigin origin, VCWidget *control)
{
    const bool reported = isAuthoring() && ShowCommandFsm::isUserOrigin(origin);
    if (const quint64 g = ShowEventLog::generation())
    {
        ShowEventLog::Entry e = traceEntry(g, Phase::Decision, Outcome::Unsupported,
                                           reported ? tr("Not recorded: %1").arg(what)
                                                    : tr("%1 cannot be recorded (%2)").arg(what, phaseName()));
        e.origin = int(origin);
        if (control != nullptr)
        {
            e.control = control->caption();
            e.widgetId = control->id();
            e.controlId = control->recordingId();
        }
        if (Show *show = trackedShow())
            e.showTimeMs = acceptancePosition(show);
        ShowEventLog::append(e);
    }
    if (reported == false)
        return;

    reportFailure(tr("Not recorded: %1").arg(what));
}

void ShowCommandRecorder::requestUserControl(ShowControlRequest request)
{
    if (request.control.isNull())
        return;

    Show *show = trackedShow();
    request.acceptedTimeMs = show != nullptr ? acceptancePosition(show) : 0;
    request.epoch = m_requestEpoch;
    request.accepted = configurationOf(request.control);
    if (ShowEventLog::generation() != 0)
        request.cause = newCause(request.origin, request.control);
    {
        QScopedValueRollback<const ShowControlRequest *> traced(m_tracedRequest, &request);
        captureUserRequest(request);
    }
    m_userRequests.append(request);
    drainUserRequests();
    if (request.cause.trace != 0)
    {
        for (const ShowControlRequest &waiting : std::as_const(m_userRequests))
        {
            if (waiting.cause.trace == request.cause.trace)
            {
                traceRequest(request, Phase::Execute, Outcome::Queued, tr("waits behind earlier work on a coupled control"));
                break;
            }
        }
    }
}

void ShowCommandRecorder::captureUserRequest(ShowControlRequest &request)
{
    const bool diag = request.cause.generation != 0;
    if (isAuthoring() == false)
    {
        if (diag)
            traceRequest(request, Phase::Decision, Outcome::Ignored,
                         m_state.phase() == ShowRecordPhase::Off ? tr("REC is off")
                         : m_state.phase() == ShowRecordPhase::Armed ? tr("REC armed, no Show bound yet")
                                                                     : tr("REC suspended: another Show is resolved"));
        return;
    }
    if (ShowCommandFsm::isUserOrigin(request.origin) == false)
    {
        if (diag)
            traceRequest(request, Phase::Decision, Outcome::Ignored, tr("input of this origin is not recorded"));
        return;
    }

    ShowCommandInput input;
    input.origin = request.origin;
    input.role = request.role;
    if (qobject_cast<VCButton *>(request.control) != nullptr)
    {
        input.action = ShowCommandAction::SetButtonState;
        input.on = request.on;
    }
    else if (VCSlider *slider = qobject_cast<VCSlider *>(request.control))
    {
        // unchanged against the value the control holds once its queued requests ran
        int previous = slider->value();
        for (int i = m_userRequests.count() - 1; i >= 0; i--)
        {
            if (m_userRequests.at(i).control == request.control)
            {
                previous = m_userRequests.at(i).value;
                break;
            }
        }
        if (request.value == previous)
        {
            if (diag)
                traceRequest(request, Phase::Decision, Outcome::Ignored, tr("unchanged value"));
            return;
        }

        const qreal low = slider->rangeLowLimit();
        const qreal high = slider->rangeHighLimit();
        input.action = ShowCommandAction::SetSliderPosition;
        input.attribute = request.accepted.snapshot.attribute;
        // an empty range yields no finite position, which the FSM rejects with a reason
        input.position = (qreal(request.value) - low) / (high - low);
    }
    else
    {
        if (diag)
            traceRequest(request, Phase::Decision, Outcome::Unsupported, tr("not a recordable control"));
        return;
    }

    input.controlId = request.control->ensureRecordingId();
    // the identity this capture issues is the one its entries name
    if (request.cause.generation != 0)
        request.cause.controlId = input.controlId;
    if (authorAt(QVector<ShowCommandInput>{input}, request.acceptedTimeMs))
        request.authoredShowId = trackedShowId();
}

void ShowCommandRecorder::drainUserRequests()
{
    if (m_drainingRequests)
        return;
    QScopedValueRollback<bool> draining(m_drainingRequests, true);

    for (int i = 0; i < m_userRequests.count(); )
    {
        const ShowControlRequest request = m_userRequests.at(i);
        if (request.control.isNull() || request.epoch != m_requestEpoch)
        {
            m_userRequests.removeAt(i);
            if (request.cause.generation != 0)
                traceRequest(request, Phase::Execute, Outcome::Cancelled,
                             request.control.isNull() ? tr("control removed") : tr("requests reset"));
            continue;
        }
        // accepted against what the control no longer is: never retargeted
        const ShowControlConfiguration current = configurationOf(request.control);
        if (current.snapshot.role != request.role || current != request.accepted)
        {
            m_userRequests.removeAt(i);
            const QString error = tr("Not executed: %1 changed while its input waited").arg(request.control->caption());
            traceRequest(request, Phase::Execute, Outcome::Cancelled, error);
            reportFailure(error);
            continue;
        }
        if (userRequestWaits(i))
        {
            i++;
            continue;
        }

        m_userRequests.removeAt(i);
        const TracedCall traced;
        const bool authored = request.authoredShowId != ShowCommand::InvalidId;
        const QVector<TimelineFact> closure = authored ? closureOf(request.control) : QVector<TimelineFact>();
        if (VCButton *button = qobject_cast<VCButton *>(request.control))
        {
            const ShowCommandFsm::ShowButtonOp op = button->applyUserState(request.on);
            if (authored)
                journal(closure, request.authoredShowId, request.acceptedTimeMs,
                        op == ShowCommandFsm::ShowButtonOp::Start);
            if (request.cause.generation != 0)
                traceRequest(request, Phase::Execute, Outcome::Requested, tr("native start/stop through the button"));
        }
        else if (VCSlider *slider = qobject_cast<VCSlider *>(request.control))
        {
            const bool unchanged = slider->value() == request.value;
            slider->setValue(request.value, true, request.updateFeedback);
            if (authored)
                journal(closure, request.authoredShowId, request.acceptedTimeMs, false);
            if (request.cause.generation != 0)
                traceRequest(request, Phase::Execute, unchanged ? Outcome::Skipped : Outcome::Applied,
                             unchanged ? tr("already at this value") : tr("control value set"));
        }
    }
}

bool ShowCommandRecorder::userRequestWaits(int index) const
{
    QVector<ShowCommand> owed;
    for (const ControlRun &run : m_controlRuns)
    {
        if (!run.show.isNull() && run.show->commandTraversal() == run.batch.traversal)
            owed += run.batch.commands;
    }
    for (Show *show : m_attachedShows)
    {
        if (show != nullptr)
            owed += show->pendingCommandWork();
    }
    if (owed.isEmpty() && index == 0)
        return false;

    VCWidget *control = m_userRequests.at(index).control;
    const ShowControlCoupling coupling = couplingOf(control, controlFunction(control));
    const auto dependent = [&](VCWidget *other, quint32 functionId)
    {
        const ShowControlCoupling done = couplingOf(other, functionId);
        return (other != nullptr && other == control) ||
               ShowCommandFsm::controlDependsOn(coupling, done) ||
               ShowCommandFsm::controlDependsOn(done, coupling);
    };

    for (int i = 0; i < index; i++)
    {
        VCWidget *earlier = m_userRequests.at(i).control;
        if (earlier != nullptr && dependent(earlier, controlFunction(earlier)))
            return true;
    }
    for (const ShowCommand &cmd : std::as_const(owed))
    {
        if (cmd.action == ShowCommandAction::SetButtonState || cmd.action == ShowCommandAction::SetSliderPosition)
        {
            VCWidget *replayed = resolveControl(cmd);
            if (replayed != nullptr && dependent(replayed, controlFunction(replayed)))
                return true;
        }
        else if (dependent(nullptr, cmd.functionId))
        {
            return true;
        }
    }
    return false;
}

void ShowCommandRecorder::dropUserRequests()
{
    m_requestEpoch++;
    m_userRequests.clear();
}

void ShowCommandRecorder::slotFunctionRemoved(quint32 id)
{
    // a copied Start, Stop or intensity of it would reach whatever takes the id next
    for (const ShowCommand &copy : std::as_const(m_clipboard))
    {
        if (!copy.controlId.isNull() || copy.functionId != id)
            continue;
        m_clipboardDeletedTargets.insert(id);
        break;
    }
    m_timeline.erase(std::remove_if(m_timeline.begin(), m_timeline.end(),
                                    [id](const TimelineFact &fact) { return fact.showId == id; }),
                     m_timeline.end());
    // accepted input goes with the Show that owned it
    m_unpublished.erase(std::remove_if(m_unpublished.begin(), m_unpublished.end(),
                                       [id](const Unpublished &accepted) { return accepted.showId == id; }),
                        m_unpublished.end());
    for (Show *show : std::as_const(m_attachedShows))
    {
        if (show != nullptr && show->id() == id)
        {
            dropUserRequests();
            return;
        }
    }
}

/*********************************************************************
 * Authored data
 *********************************************************************/

const ShowCommandTrack &ShowCommandRecorder::track() const
{
    return m_track;
}

namespace
{
/** The delta between two track values for the affected ids */
QVector<ShowCommandEditEntry> editDelta(const ShowCommandTrack &before, const ShowCommandTrack &after,
                                        const QVector<quint32> &affected)
{
    QVector<ShowCommandEditEntry> entries;
    for (quint32 id : affected)
    {
        ShowCommandEditEntry entry;
        entry.id = id;
        const int from = before.indexOfId(id);
        const int to = after.indexOfId(id);
        entry.hasBefore = from >= 0;
        entry.hasAfter = to >= 0;
        if (entry.hasBefore)
            entry.before = before.commands().at(from);
        if (entry.hasAfter)
            entry.after = after.commands().at(to);
        if (entry.hasBefore != entry.hasAfter || entry.before != entry.after || entry.before.order != entry.after.order)
            entries.append(entry);
    }
    return entries;
}

/** Replay one side of a delta on the current track. Every event it names must
 *  still be what the other side left, so only that delta changes and any
 *  later capture or edit stays. The extent follows the content. */
bool replayDelta(ShowCommandTrack *track, const QVector<ShowCommandEditEntry> &entries, bool undo,
                 QString *error)
{
    for (const ShowCommandEditEntry &entry : entries)
    {
        const bool expected = undo ? entry.hasAfter : entry.hasBefore;
        const ShowCommand &from = undo ? entry.after : entry.before;
        const int index = track->indexOfId(entry.id);
        if (expected && (index < 0 || track->commands().at(index) != from ||
                         track->commands().at(index).order != from.order))
        {
            *error = ShowCommandRecorder::tr("Event %1 changed since this edit").arg(entry.id);
            return false;
        }
        if (!expected && index >= 0)
        {
            *error = ShowCommandRecorder::tr("Event %1 exists again").arg(entry.id);
            return false;
        }
    }

    ShowCommandTrack result = *track;
    for (const ShowCommandEditEntry &entry : entries)
    {
        if (undo ? entry.hasAfter : entry.hasBefore)
            result.remove(entry.id);
    }
    // each back in its own place among its equal times
    for (const ShowCommandEditEntry &entry : entries)
    {
        if ((undo ? entry.hasBefore : entry.hasAfter) &&
            result.restore(undo ? entry.before : entry.after, error) == false)
            return false;
    }
    result.setExtent(result.lastCommandTime());
    *track = result;
    return true;
}

QString percentText(qreal value)
{
    return QString::number(qRound(value * 1000.0) / 10.0);
}

QVector<quint32> idList(const QVariantList &ids)
{
    QVector<quint32> list;
    for (const QVariant &id : ids)
        list.append(id.toUInt());
    return list;
}
} // namespace

QVariantList ShowCommandRecorder::commands() const
{
    QVariantList list;
    const quint32 showId = trackedShowId();
    for (const ShowCommand &cmd : m_track.commands())
    {
        Function *target = m_doc != nullptr ? m_doc->function(cmd.functionId) : nullptr;
        QVariantMap entry;
        entry["id"] = cmd.id;
        entry["showId"] = showId;
        entry["time"] = cmd.time;
        entry["action"] = ShowCommand::actionToString(cmd.action);
        entry["functionId"] = cmd.functionId;
        entry["functionName"] = target != nullptr ? target->name() : tr("Missing function");
        entry["intensity"] = cmd.intensity;
        entry["isValue"] = cmd.action == ShowCommandAction::SetIntensity;

        QString control = entry["functionName"].toString();
        QString detail = tr("Function %1").arg(cmd.functionId);
        QString value;
        QString field;
        switch (cmd.action)
        {
            case ShowCommandAction::Start: entry["actionLabel"] = tr("Start function"); break;
            case ShowCommandAction::Stop: entry["actionLabel"] = tr("Stop function"); break;
            case ShowCommandAction::SetIntensity:
                entry["actionLabel"] = tr("Set intensity");
                value = percentText(cmd.intensity);
                field = QStringLiteral("value");
            break;
            case ShowCommandAction::SetButtonState:
                entry["actionLabel"] = tr("Button state");
                value = cmd.on ? tr("On") : tr("Off");
                field = QStringLiteral("state");
            break;
            case ShowCommandAction::SetSliderPosition:
                entry["actionLabel"] = tr("Slider position");
                value = percentText(cmd.position);
                field = QStringLiteral("value");
            break;
        }
        if (cmd.action == ShowCommandAction::SetButtonState || cmd.action == ShowCommandAction::SetSliderPosition)
        {
            // the same resolution replay uses: what this record reaches now
            const QList<VCWidget *> widgets = m_vc != nullptr ? m_vc->widgetsByRecordingId(cmd.controlId)
                                                              : QList<VCWidget *>();
            QVector<ShowControlSnapshot> matches;
            for (VCWidget *widget : widgets)
                matches.append(configurationOf(widget).snapshot);
            QString reason;
            const ShowControlStatus status = ShowCommandFsm::resolveControl(cmd, matches, &reason);

            QStringList parts{cmd.controlId.toString(QUuid::WithoutBraces)};
            control = widgets.isEmpty() ? tr("Missing control") : widgets.first()->caption();
            if (!widgets.isEmpty())
            {
                const QString binding = bindingText(widgets.first());
                if (!binding.isEmpty())
                    parts.append(binding);
            }
            switch (status)
            {
                case ShowControlStatus::Ready: break;
                case ShowControlStatus::Missing: parts.append(tr("Missing control")); break;
                case ShowControlStatus::Ambiguous: parts.append(tr("Ambiguous identity")); break;
                case ShowControlStatus::Incompatible: parts.append(tr("Incompatible: %1").arg(reason)); break;
                case ShowControlStatus::Disabled: parts.append(tr("Disabled")); break;
                case ShowControlStatus::Unbound: parts.append(tr("Unbound")); break;
            }
            detail = parts.join(QStringLiteral(" · "));
            entry["controlReady"] = status == ShowControlStatus::Ready;
            entry["controlMissing"] = widgets.isEmpty();
            entry["controlAmbiguous"] = status == ShowControlStatus::Ambiguous;
        }
        entry["control"] = control;
        entry["detail"] = detail;
        entry["valueText"] = field == QStringLiteral("state") || value.isEmpty() ? value : value + QLatin1Char('%');
        entry["valueEdit"] = value;
        entry["valueField"] = field;
        list.append(entry);
    }
    return list;
}

QVariantList ShowCommandRecorder::groups() const
{
    QHash<quint32, QVariantMap> rows;
    for (const QVariant &row : commands())
    {
        const QVariantMap entry = row.toMap();
        rows.insert(entry.value("id").toUInt(), entry);
    }
    const QVector<ShowCommandGroup> groups = m_track.groups();
    // one lane per recorded control, numbered by first appearance in the
    // global groups; captions two lanes share (the legacy "Functions" lane
    // included) name the control's UUID too
    const auto laneKey = [](const ShowCommandGroup &group)
    {
        return group.controlId.isNull() ? QStringLiteral("legacy")
                                        : group.controlId.toString(QUuid::WithoutBraces);
    };
    const auto suffixed = [](const QString &caption, const QString &key)
    {
        return caption + QStringLiteral(" · ") + key.left(8);
    };
    QHash<QString, int> laneIndex;
    QHash<QString, QString> laneCaption;
    QHash<QString, int> captionLanes;
    for (const ShowCommandGroup &group : groups)
    {
        const QString key = laneKey(group);
        if (laneIndex.contains(key))
            continue;
        laneIndex.insert(key, laneIndex.size());
        const QVariantMap first = rows.value(group.eventIds.first());
        QString caption = tr("Functions");
        if (!group.controlId.isNull())
        {
            caption = first.value("control").toString();
            if (first.value("controlMissing").toBool())
                caption = suffixed(caption, key);
            else if (first.value("controlAmbiguous").toBool())
                caption = suffixed(tr("Ambiguous control"), key);
        }
        captionLanes[caption]++;
        laneCaption.insert(key, caption);
    }

    QVariantList result;
    for (const ShowCommandGroup &group : groups)
    {
        QVariantMap entry = rows.value(group.eventIds.first());
        const QString key = laneKey(group);
        const QString caption = laneCaption.value(key);
        entry["laneKey"] = key;
        entry["laneIndex"] = laneIndex.value(key);
        entry["laneLabel"] = !group.controlId.isNull() && captionLanes.value(caption) > 1
            ? suffixed(caption, key) : caption;
        QVariantList ids;
        for (quint32 id : group.eventIds)
            ids.append(id);
        entry["eventIds"] = ids;
        entry["startTime"] = group.startTime;
        entry["endTime"] = group.endTime;
        entry["sampleCount"] = group.eventIds.size();
        entry["endValueText"] = rows.value(group.eventIds.last()).value("valueText");
        entry["slider"] = group.action == ShowCommandAction::SetSliderPosition;
        result.append(entry);
    }
    return result;
}

QVariantMap ShowCommandRecorder::sliderSamples(quint32 showId, const QVariantList &eventIds) const
{
    QVariantList ids, times, positions;
    if (showId == trackedShowId() && showId != ShowCommand::InvalidId)
    {
        const QVector<quint32> list = idList(eventIds);
        const QSet<quint32> wanted(list.begin(), list.end());
        for (const ShowCommand &cmd : m_track.commands())
        {
            if (cmd.action != ShowCommandAction::SetSliderPosition || !wanted.contains(cmd.id))
                continue;
            ids.append(cmd.id);
            times.append(cmd.time);
            positions.append(cmd.position);
            if (ids.size() == wanted.size())
                break;
        }
    }
    return QVariantMap{{"ids", ids}, {"times", times}, {"positions", positions}};
}

int ShowCommandRecorder::extent() const
{
    return int(m_track.extent());
}

bool ShowCommandRecorder::editAllowed(quint32 showId, QString *reason) const
{
    const auto refuse = [reason](const QString &why)
    {
        if (reason != nullptr)
            *reason = why;
        return false;
    };
    // playing, paused and recording Shows take edits too: the runner meets
    // what is published when it gets there, and capture only ever adds
    Show *show = m_doc != nullptr ? qobject_cast<Show *>(m_doc->function(showId)) : nullptr;
    if (show == nullptr)
        return refuse(tr("No Show to edit"));
    if (reason != nullptr)
        reason->clear();
    return true;
}

QString ShowCommandRecorder::editBlockedReason() const
{
    QString reason;
    editAllowed(trackedShowId(), &reason);
    return reason;
}

Show *ShowCommandRecorder::editableShow(quint32 showId)
{
    QString reason;
    if (showId != trackedShowId())
        reason = tr("That row belongs to a Show that is no longer shown");
    else
        editAllowed(showId, &reason);
    if (reason.isEmpty() == false)
    {
        editFailed(reason);
        return nullptr;
    }
    return trackedShow();
}

bool ShowCommandRecorder::retimeCommand(quint32 showId, quint32 id, qreal ms)
{
    Show *show = editableShow(showId);
    if (show == nullptr)
        return false;
    if (!std::isfinite(ms) || ms < 0 || ms > ShowCommand::MaxTime)
        return editFailed(tr("%1 is not a valid time").arg(ms));

    ShowCommandTrack edited = show->commandTrack();
    QString error;
    if (edited.retime(id, quint32(qRound64(ms)), &error) == false)
        return editFailed(error);

    return publishEdit(show, edited, {id});
}

bool ShowCommandRecorder::replaceCommand(quint32 showId, quint32 id,
                                         const std::function<QString(ShowCommand &)> &change)
{
    Show *show = editableShow(showId);
    if (show == nullptr)
        return false;

    ShowCommandTrack edited = show->commandTrack();
    const int index = edited.indexOfId(id);
    if (index < 0)
        return editFailed(tr("no command carries id %1").arg(id));

    ShowCommand cmd = edited.commands().at(index);
    QString error = change(cmd);
    if (error.isEmpty() == false || edited.replace(cmd, &error) == false)
        return editFailed(error);

    return publishEdit(show, edited, {id});
}

bool ShowCommandRecorder::setCommandValue(quint32 showId, quint32 id, qreal value)
{
    return replaceCommand(showId, id, [value](ShowCommand &cmd)
    {
        if (cmd.action == ShowCommandAction::SetIntensity)
            cmd.intensity = value;
        else if (cmd.action == ShowCommandAction::SetSliderPosition)
            cmd.position = value;
        else
            return tr("%1 has no value").arg(ShowCommand::actionToString(cmd.action));
        return QString();
    });
}

bool ShowCommandRecorder::setCommandState(quint32 showId, quint32 id, bool on)
{
    return replaceCommand(showId, id, [on](ShowCommand &cmd)
    {
        if (cmd.action != ShowCommandAction::SetButtonState)
            return tr("%1 has no On/Off state").arg(ShowCommand::actionToString(cmd.action));
        cmd.on = on;
        return QString();
    });
}

bool ShowCommandRecorder::editCommandText(quint32 showId, quint32 id, const QString &field, const QString &text)
{
    const QString trimmed = text.trimmed();
    const auto number = [&trimmed](qreal *value)
    {
        QString digits = trimmed;
        if (digits.endsWith(QLatin1Char('%')))
            digits.chop(1);
        bool ok = false;
        *value = QLocale::c().toDouble(digits.trimmed(), &ok);
        if (!ok)
            *value = QLocale().toDouble(digits.trimmed(), &ok);
        return ok;
    };

    qreal value = 0;
    if (field == QLatin1String("time"))
    {
        if (!number(&value))
            return editFailed(tr("%1 is not a time in seconds").arg(trimmed));
        return retimeCommand(showId, id, value * 1000.0);
    }
    if (field == QLatin1String("value"))
    {
        if (!number(&value))
            return editFailed(tr("%1 is not a percentage").arg(trimmed));
        return setCommandValue(showId, id, value / 100.0);
    }
    if (field == QLatin1String("state"))
    {
        if (trimmed.compare(QLatin1String("on"), Qt::CaseInsensitive) == 0 || trimmed == tr("On"))
            return setCommandState(showId, id, true);
        if (trimmed.compare(QLatin1String("off"), Qt::CaseInsensitive) == 0 || trimmed == tr("Off"))
            return setCommandState(showId, id, false);
        return editFailed(tr("%1 is not On or Off").arg(trimmed));
    }
    return editFailed(tr("%1 cannot be edited").arg(field));
}

bool ShowCommandRecorder::removeCommands(quint32 showId, const QVariantList &ids)
{
    Show *show = editableShow(showId);
    if (show == nullptr)
        return false;

    const QVector<quint32> affected = idList(ids);
    ShowCommandTrack edited = show->commandTrack();
    QString error;
    for (quint32 id : affected)
    {
        if (edited.remove(id, &error) == false)
            return editFailed(error);
    }
    return publishEdit(show, edited, affected);
}

bool ShowCommandRecorder::shiftCommands(Show *show, const QVector<quint32> &ids, qint64 delta)
{
    ShowCommandTrack edited;
    const ShowRetimeRefusal refusal = show->commandTrack().retimeSelection(ids, ShowRetimeKind::Move, delta, &edited);
    if (refusal != ShowRetimeRefusal::None)
        return editFailed(retimeRefusalText(refusal, ShowRetimeKind::Move));
    return publishEdit(show, edited, ids);
}

QString ShowCommandRecorder::retimeRefusalText(ShowRetimeRefusal refusal, ShowRetimeKind kind) const
{
    switch (refusal)
    {
        case ShowRetimeRefusal::None: return QString();
        case ShowRetimeRefusal::UnknownEvent: return tr("Some selected events no longer exist");
        case ShowRetimeRefusal::NoSpan: return tr("Events at one time cannot be stretched");
        case ShowRetimeRefusal::NoTargetSpan: return tr("The selection cannot be stretched onto or past its other edge");
        case ShowRetimeRefusal::Reordered:
            return tr("Compressing this far would change the order of the selected events");
        case ShowRetimeRefusal::OutOfRange:
            return kind == ShowRetimeKind::Move ? tr("Moving the selection would put an event outside the Show")
                                                : tr("Stretching the selection would put an event outside the Show");
    }
    return QString();
}

bool ShowCommandRecorder::beginEditSession(quint32 showId, const QVariantList &ids)
{
    endEditSession(tr("Another recording edit started"));
    Show *show = editableShow(showId);
    if (show == nullptr)
        return false;
    const QVector<quint32> wanted = idList(ids);
    const QSet<quint32> unique(wanted.cbegin(), wanted.cend());
    QVector<ShowCommand> basis;
    for (const ShowCommand &cmd : show->commandTrack().commands())
    {
        if (unique.contains(cmd.id))
            basis.append(cmd);
    }
    if (basis.isEmpty() || basis.count() != unique.count())
        return editFailed(wanted.isEmpty() ? tr("Nothing selected") : tr("Some selected events no longer exist"));

    m_session.show = show;
    m_session.ids = wanted;
    m_session.basis = basis;
    m_sessionEndReason.clear();
    setError(QString());
    emit editSessionChanged();
    return true;
}

void ShowCommandRecorder::endEditSession(const QString &reason)
{
    if (m_session.show == nullptr && m_session.ids.isEmpty())
        return;
    m_session = EditSession();
    m_sessionEndReason = reason;
    if (reason.isEmpty() == false)
        setError(reason);
    emit editSessionChanged();
}

void ShowCommandRecorder::cancelEditSession(const QString &reason)
{
    endEditSession(reason);
}

QString ShowCommandRecorder::sessionConflict() const
{
    if (m_session.show == nullptr)
        return m_session.ids.isEmpty() ? tr("No edit in progress") : tr("The Show of this edit is gone");
    if (m_session.show->id() != trackedShowId())
        return tr("That row belongs to a Show that is no longer shown");
    const ShowCommandTrack current = m_session.show->commandTrack();
    for (const ShowCommand &frozen : m_session.basis)
    {
        const int index = current.indexOfId(frozen.id);
        if (index < 0)
            return tr("Event %1 was deleted during this edit").arg(frozen.id);
        // only the edited events matter: what was recorded or edited around them meanwhile stays
        if (current.commands().at(index) != frozen || current.commands().at(index).order != frozen.order)
            return tr("Event %1 changed during this edit").arg(frozen.id);
    }
    return QString();
}

QVariantMap ShowCommandRecorder::previewRetime(int kind, qreal deltaMs) const
{
    QVariantMap result;
    QString reason = sessionConflict();
    if (reason.isEmpty() && (!std::isfinite(deltaMs) || kind < 0 || kind > int(ShowRetimeKind::StretchStart)))
        reason = tr("%1 is not a valid time").arg(deltaMs);
    ShowCommandTrack edited;
    if (reason.isEmpty())
    {
        const ShowRetimeKind how = ShowRetimeKind(kind);
        reason = retimeRefusalText(m_session.show->commandTrack().retimeSelection(m_session.ids, how,
                                                                                 qRound64(deltaMs), &edited), how);
    }
    if (reason.isEmpty() == false)
    {
        result["reason"] = reason;
        return result;
    }
    QVariantMap times;
    for (quint32 id : m_session.ids)
        times.insert(QString::number(id), edited.commands().at(edited.indexOfId(id)).time);
    result["times"] = times;
    return result;
}

bool ShowCommandRecorder::commitRetime(int kind, qreal deltaMs)
{
    // checked in the same call that publishes, after the Show's own gate
    const QString conflict = sessionConflict();
    const EditSession session = m_session;
    endEditSession();
    if (conflict.isEmpty() == false)
        return editFailed(conflict);
    Show *show = editableShow(session.show->id());
    if (show == nullptr)
        return false;
    if (!std::isfinite(deltaMs) || kind < 0 || kind > int(ShowRetimeKind::StretchStart))
        return editFailed(tr("%1 is not a valid time").arg(deltaMs));

    const ShowRetimeKind how = ShowRetimeKind(kind);
    ShowCommandTrack edited;
    const ShowRetimeRefusal refusal = show->commandTrack().retimeSelection(session.ids, how, qRound64(deltaMs), &edited);
    if (refusal != ShowRetimeRefusal::None)
        return editFailed(retimeRefusalText(refusal, how));
    return publishEdit(show, edited, session.ids);
}

bool ShowCommandRecorder::commitCellText(const QString &field, const QString &text)
{
    const QString conflict = sessionConflict();
    if (conflict.isEmpty() == false || m_session.ids.count() != 1)
    {
        endEditSession();
        return editFailed(conflict.isEmpty() ? tr("A cell edit names one event") : conflict);
    }
    // text that is no valid value keeps the draft open on the same frozen event
    if (editCommandText(m_session.show->id(), m_session.ids.first(), field, text) == false)
        return false;
    endEditSession();
    return true;
}

bool ShowCommandRecorder::moveCommands(quint32 showId, const QVariantList &ids, int direction, qreal stepMs)
{
    Show *show = editableShow(showId);
    if (show == nullptr)
        return false;
    if (!std::isfinite(stepMs) || stepMs <= 0 || stepMs > ShowCommand::MaxTime || (direction != 1 && direction != -1))
        return editFailed(tr("There is no musical grid to move by"));

    // rounded once, so the step back is exactly the step forward
    return shiftCommands(show, idList(ids), direction * qRound64(stepMs));
}

bool ShowCommandRecorder::snapCommands(quint32 showId, const QVariantList &ids, qreal beatMs, qreal anchorMs)
{
    Show *show = editableShow(showId);
    if (show == nullptr)
        return false;
    if (!std::isfinite(beatMs) || beatMs <= 0 || !std::isfinite(anchorMs))
        return editFailed(tr("There is no musical grid to snap to"));

    const QVector<quint32> affected = idList(ids);
    const ShowCommandTrack &track = show->commandTrack();
    qint64 earliest = -1;
    for (quint32 id : affected)
    {
        const int index = track.indexOfId(id);
        if (index < 0)
            return editFailed(tr("Some selected events no longer exist"));
        if (earliest < 0 || track.commands().at(index).time < earliest)
            earliest = track.commands().at(index).time;
    }
    if (earliest < 0)
        return editFailed(tr("Nothing selected"));

    // the nearest beat, rounded half up like the timeline's own grid snap
    const qreal beat = std::floor((earliest - anchorMs) / beatMs + 0.5);
    const qreal target = std::floor(anchorMs + beat * beatMs + 0.5);
    if (target < 0 || target > ShowCommand::MaxTime)
        return editFailed(tr("Snapping the selection would put an event outside the Show"));
    return shiftCommands(show, affected, qint64(target) - earliest);
}

bool ShowCommandRecorder::setCommandAction(quint32 showId, quint32 id, const QString &action)
{
    return replaceCommand(showId, id, [&action](ShowCommand &cmd)
    {
        ShowCommandAction next;
        if (!ShowCommand::actionFromString(action, &next) || next == ShowCommandAction::SetButtonState ||
            next == ShowCommandAction::SetSliderPosition || cmd.controlId.isNull() == false)
            return tr("%1 is not a function command action").arg(action);
        cmd.action = next;
        // a trigger carries no value, so changing one drops it rather than
        // keeping a stale number the validation would reject
        if (cmd.action != ShowCommandAction::SetIntensity)
            cmd.intensity = 0.0;
        return QString();
    });
}

bool ShowCommandRecorder::setCommandTarget(quint32 showId, quint32 id, quint32 functionId)
{
    // target existence and self-recursion are the Show's call, not ours
    return replaceCommand(showId, id, [functionId](ShowCommand &cmd)
    {
        if (cmd.controlId.isNull() == false)
            return tr("A control state has no function target");
        cmd.functionId = functionId;
        return QString();
    });
}

bool ShowCommandRecorder::copyCommands(quint32 showId, const QVariantList &ids)
{
    Show *show = showId == trackedShowId() ? trackedShow() : nullptr;
    if (show == nullptr)
    {
        setError(tr("That row belongs to a Show that is no longer shown"));
        return false;
    }
    const QVector<quint32> wanted = idList(ids);
    QVector<ShowCommand> copied;
    for (const ShowCommand &cmd : show->commandTrack().commands())
    {
        if (wanted.contains(cmd.id))
            copied.append(cmd);
    }
    if (copied.isEmpty() || copied.count() != QSet<quint32>(wanted.begin(), wanted.end()).count())
    {
        setError(wanted.isEmpty() ? tr("Nothing selected") : tr("Some selected events no longer exist"));
        return false;
    }

    m_clipboard = copied;
    m_clipboardDeletedTargets.clear();
    // a target already gone may be taken by another Function before Paste
    for (const ShowCommand &copy : std::as_const(m_clipboard))
    {
        if (copy.controlId.isNull() && m_doc->function(copy.functionId) == nullptr)
            m_clipboardDeletedTargets.insert(copy.functionId);
    }
    emit clipboardChanged();
    setError(QString());
    return true;
}

QVariantList ShowCommandRecorder::pasteCommands(quint32 showId, qreal atMs)
{
    Show *show = editableShow(showId);
    if (show == nullptr)
        return QVariantList();
    if (m_clipboard.isEmpty())
    {
        editFailed(tr("Nothing copied to paste"));
        return QVariantList();
    }
    if (!std::isfinite(atMs) || atMs < 0 || atMs > ShowCommand::MaxTime)
    {
        editFailed(tr("%1 is not a valid time").arg(atMs));
        return QVariantList();
    }
    for (const ShowCommand &copy : std::as_const(m_clipboard))
    {
        // a marked id no longer means what was copied; still absent, the Show's own check names it
        if (copy.controlId.isNull() && m_clipboardDeletedTargets.contains(copy.functionId) &&
            m_doc->function(copy.functionId) != nullptr)
        {
            editFailed(tr("A copied event's Function reference is stale; copy again"));
            return QVariantList();
        }
    }

    // above every id the Show, its input not taken yet and its history hold
    ShowCommandTrack candidate = show->commandTrack();
    candidate.reserve(withUnpublished(show).nextEventId(), 0);
    const qint64 delta = qRound64(atMs) - qint64(m_clipboard.first().time);
    QVector<quint32> added;
    QString error;
    // copied order in, so equal times keep their copied ties after the existing ones
    for (ShowCommand copy : std::as_const(m_clipboard))
    {
        const qint64 time = qint64(copy.time) + delta;
        if (time > qint64(ShowCommand::MaxTime))
        {
            editFailed(tr("Pasting here would put an event outside the Show"));
            return QVariantList();
        }
        copy.id = candidate.nextEventId();
        copy.time = quint32(time);
        if (copy.id == ShowCommand::InvalidId)
        {
            editFailed(tr("This Show has no event id left"));
            return QVariantList();
        }
        if (candidate.insert(copy, &error) == false)
        {
            editFailed(error);
            return QVariantList();
        }
        added.append(copy.id);
    }
    if (publishEdit(show, candidate, added) == false)
        return QVariantList();

    QVariantList ids;
    for (quint32 id : std::as_const(added))
        ids.append(id);
    return ids;
}

bool ShowCommandRecorder::applyHistoryEdit(const QVariant &value, bool undo)
{
    if (!value.canConvert<ShowCommandEdit>())
        return false;
    const ShowCommandEdit edit = value.value<ShowCommandEdit>();

    QString error;
    if (editAllowed(edit.showId, &error) == false)
        return editFailed(error);

    Show *show = qobject_cast<Show *>(m_doc->function(edit.showId));
    const ShowCommandTrack before = show->commandTrack();
    ShowCommandTrack edited = before;
    if (replayDelta(&edited, edit.entries, undo, &error) == false ||
        publishTrack(edited, QSet<quint32>(), show->commandTraversal(), show, &error) == false)
        return editFailed(error);
    if (ShowEventLog::generation() != 0)
        traceTrackDelta(newCause(), before, show->commandTrack(), edit.showId, undo ? tr("undo") : tr("redo"));

    m_doc->setModified();
    setError(QString());
    return true;
}

/*********************************************************************
 * Private
 *********************************************************************/

void ShowCommandRecorder::slotShowClock()
{
    Show *show = trackedShow();
    if (show == nullptr || sender() != show)
        return;

    // Milliseconds, never the editor's ruler units.
    const quint32 now = show->commandPosition();
    // the clock moved: it is the position again, not a cursor set before
    m_hostCursorShowId = ShowCommand::InvalidId;
    if (now < m_clockPosition)
    {
        if (isAuthoring())
            m_takeId++;
        m_state = ShowCommandFsm::seek(m_state, now);
        if (ShowEventLog::generation() != 0)
            traceTransport(Outcome::Applied, tr("Show clock moved back"), QString::number(now / 1000.0, 'f', 3),
                           isAuthoring() ? tr("a new take segment begins") : QString());
    }

    m_clockPosition = now;
    m_state.position = now;
}

void ShowCommandRecorder::slotCommandTrackChanged()
{
    if (m_publishing)
        return;

    Show *show = trackedShow();
    if (show == nullptr || sender() != show)
        return;

    // the last value this recorder saw is the before of the change
    if (ShowEventLog::generation() != 0)
        traceTrackDelta(newCause(), m_track, show->commandTrack(), show->id(), tr("published by another source"));
    m_track = show->commandTrack();
    emit commandsChanged();
}

quint32 ShowCommandRecorder::trackedShowId() const
{
    return m_state.boundShowId != ShowCommand::InvalidId ? m_state.boundShowId
                                                         : m_state.resolvedShowId;
}

Show *ShowCommandRecorder::trackedShow() const
{
    if (m_doc == nullptr || trackedShowId() == ShowCommand::InvalidId)
        return nullptr;

    return qobject_cast<Show *>(m_doc->function(trackedShowId()));
}

void ShowCommandRecorder::reloadTrack()
{
    const quint32 showId = trackedShowId();
    if (showId == m_trackedShowId)
        return;

    if (m_doc != nullptr && m_trackedShowId != ShowCommand::InvalidId)
    {
        Show *previous = qobject_cast<Show *>(m_doc->function(m_trackedShowId));
        // replay stays attached, it does not follow the selection
        if (previous != nullptr)
        {
            disconnect(previous, &Show::commandTrackChanged,
                       this, &ShowCommandRecorder::slotCommandTrackChanged);
            disconnect(previous, &Show::timeChanged, this, &ShowCommandRecorder::slotShowClock);
            disconnect(previous, &Show::externalElapsedTimeChanged,
                       this, &ShowCommandRecorder::slotShowClock);
            disconnect(previous, &Function::nameChanged, this, &ShowCommandRecorder::targetNameChanged);
        }
    }

    m_trackedShowId = showId;
    // a session belongs to the Show it began on
    if (m_session.ids.isEmpty() == false && (m_session.show == nullptr || m_session.show->id() != showId))
        endEditSession(tr("The Show of this edit is no longer shown"));

    Show *show = trackedShow();
    m_track = show != nullptr ? show->commandTrack() : ShowCommandTrack();
    m_clockPosition = show != nullptr ? show->commandPosition() : 0;
    if (show != nullptr)
    {
        connect(show, &Show::commandTrackChanged,
                this, &ShowCommandRecorder::slotCommandTrackChanged);
        connect(show, &Show::timeChanged, this, &ShowCommandRecorder::slotShowClock);
        connect(show, &Show::externalElapsedTimeChanged,
                this, &ShowCommandRecorder::slotShowClock);
        connect(show, &Function::nameChanged, this, &ShowCommandRecorder::targetNameChanged);
    }

    emit commandsChanged();
    emit positionChanged();
    emit targetNameChanged();
}

void ShowCommandRecorder::syncPosition(Show *show)
{
    if (show != nullptr)
        m_state.position = show->commandPosition();
}

bool ShowCommandRecorder::commit(const ShowCommandTransition &transition, Show *show)
{
    // accepted input is kept whether or not the Show takes it now
    m_state = transition.state;
    const ShowTraceCause cause = ShowEventLog::generation() != 0 ? currentCause() : ShowTraceCause();
    for (const ShowCommand &cmd : transition.authored)
        m_unpublished.append({ show->id(), cmd, show->commandTraversal(), cause });

    // published, it is unsaved Show content; kept, it is unsaved input
    QString error;
    if (publishUnpublished(show, &error))
        return true;

    if (ShowEventLog::generation() != 0)
    {
        for (const ShowCommand &cmd : transition.authored)
            traceCommand(cause, Phase::Commit, Outcome::Failed, nullptr, &cmd, show->id(),
                         tr("kept for the next publication: %1").arg(error));
    }

    m_doc->setModified();
    reportFailure(error);
    return false;
}

bool ShowCommandRecorder::publishTrack(const ShowCommandTrack &candidate, const QSet<quint32> &alreadyApplied,
                                       quint64 appliedTraversal, Show *show, QString *error)
{
    QScopedValueRollback<bool> publishing(m_publishing, true);
    const bool stored = show->setCommandTrack(candidate, alreadyApplied, appliedTraversal, error);
    if (stored == false)
        return false;
    rememberEventIds(show);

    if (show == trackedShow())
    {
        m_track = show->commandTrack();
        emit commandsChanged();
    }
    return true;
}

bool ShowCommandRecorder::publishEdit(Show *show, ShowCommandTrack candidate, const QVector<quint32> &affected)
{
    const ShowCommandTrack current = show->commandTrack();
    ShowCommandEdit edit;
    edit.showId = show->id();
    edit.entries = editDelta(current, candidate, affected);
    if (edit.entries.isEmpty())
    {
        setError(QString());
        return true;
    }

    // the extent follows the content, never an edited blank tail
    candidate.setExtent(candidate.lastCommandTime());
    edit.serial = ++m_editSerial;
    QString error;
    if (publishTrack(candidate, QSet<quint32>(), show->commandTraversal(), show, &error) == false)
        return editFailed(error);

    if (Tardis *tardis = Tardis::instance())
        tardis->enqueueAction(Tardis::ShowManagerCommandEdit, edit.showId, QVariant(), QVariant::fromValue(edit));
    if (ShowEventLog::generation() != 0)
        traceTrackDelta(newCause(), current, show->commandTrack(), edit.showId, tr("editor"));
    m_doc->setModified();
    setError(QString());
    return true;
}

bool ShowCommandRecorder::editFailed(const QString &error)
{
    if (ShowEventLog::generation() != 0)
        traceCommand(newCause(), Phase::Commit, Outcome::Rejected, nullptr, nullptr, trackedShowId(), error,
                     tr("Edit"));
    reportFailure(error);
    return false;
}

void ShowCommandRecorder::setError(const QString &error)
{
    if (m_lastError == error)
        return;

    m_lastError = error;
    emit lastErrorChanged();
}


/*********************************************************************
 * Diagnostics
 *********************************************************************/

namespace
{
QString commandActionText(const ShowCommand &cmd)
{
    switch (cmd.action)
    {
        case ShowCommandAction::Start: return ShowCommandRecorder::tr("Start function");
        case ShowCommandAction::Stop: return ShowCommandRecorder::tr("Stop function");
        case ShowCommandAction::SetIntensity: return ShowCommandRecorder::tr("Set intensity");
        case ShowCommandAction::SetButtonState: return ShowCommandRecorder::tr("Button state");
        case ShowCommandAction::SetSliderPosition: return ShowCommandRecorder::tr("Slider position");
    }
    return QString();
}

QString commandValueText(const ShowCommand &cmd)
{
    switch (cmd.action)
    {
        case ShowCommandAction::SetIntensity: return QString::number(cmd.intensity * 100.0, 'f', 1) + QLatin1Char('%');
        case ShowCommandAction::SetSliderPosition: return QString::number(cmd.position * 100.0, 'f', 1) + QLatin1Char('%');
        case ShowCommandAction::SetButtonState: return cmd.on ? ShowCommandRecorder::tr("On") : ShowCommandRecorder::tr("Off");
        default: break;
    }
    return QString();
}

/** Time, action and value of one side of a change */
/** One side of a change: everything that tells two commands apart */
QString commandText(const ShowCommand &cmd)
{
    const QString target = cmd.controlId.isNull()
        ? QStringLiteral("function %1").arg(cmd.functionId)
        : QStringLiteral("%1 %2 %3").arg(cmd.controlId.toString(QUuid::WithoutBraces),
                                         ShowCommand::roleToString(cmd.role), cmd.attribute).trimmed();
    return QStringLiteral("%1 s %2 %3 · %4 · #%5")
        .arg(QString::number(cmd.time / 1000.0, 'f', 3), commandActionText(cmd), commandValueText(cmd), target)
        .arg(cmd.order)
        .simplified();
}

QString requestValueText(const ShowControlRequest &request)
{
    if (request.role == ShowControlRole::ToggleButton)
        return request.on ? ShowCommandRecorder::tr("On") : ShowCommandRecorder::tr("Off");
    return QString::number(request.value);
}
} // namespace

ShowEventLog::Entry ShowCommandRecorder::traceEntry(quint64 generation, Phase phase, Outcome outcome,
                                                    const QString &reason) const
{
    ShowEventLog::Entry e;
    e.generation = generation;
    e.phase = phase;
    e.outcome = outcome;
    e.reason = reason;
    e.showId = trackedShowId();
    e.take = m_takeId;
    return e;
}

void ShowCommandRecorder::traceRequest(const ShowControlRequest &request, Phase phase, Outcome outcome,
                                       const QString &reason) const
{
    ShowEventLog::Entry e = causeEntry(request.cause, phase, outcome, reason);
    if (e.generation == 0)
        return;

    e.showTimeMs = request.acceptedTimeMs;
    e.action = request.role == ShowControlRole::ToggleButton ? tr("Button state") : tr("Slider value");
    e.value = requestValueText(request);
    ShowEventLog::append(e);
}

void ShowCommandRecorder::traceDecision(Outcome outcome, const QString &reason, const ShowCommandInput *input) const
{
    if (m_tracedRequest != nullptr)
    {
        traceRequest(*m_tracedRequest, Phase::Decision, outcome, reason);
        return;
    }
    const quint64 g = ShowEventLog::generation();
    if (g == 0 || input == nullptr)
        return;
    ShowEventLog::Entry e = traceEntry(g, Phase::Decision, outcome, reason);
    e.origin = int(input->origin);
    e.showTimeMs = m_state.position;
    e.functionId = input->functionId;
    e.controlId = input->controlId;
    ShowCommand cmd;
    cmd.action = input->action;
    cmd.intensity = input->intensity;
    cmd.on = input->on;
    cmd.position = input->position;
    e.action = commandActionText(cmd);
    e.value = commandValueText(cmd);
    ShowEventLog::append(e);
}

ShowTraceCause ShowCommandRecorder::newCause(ShowCommandOrigin origin, VCWidget *control) const
{
    ShowTraceCause cause = newCause();
    cause.origin = int(origin);
    if (control != nullptr)
    {
        cause.control = control->caption().left(ShowEventLog::LabelLength);
        cause.widgetId = control->id();
        cause.controlId = control->recordingId();
    }
    return cause;
}

ShowTraceCause ShowCommandRecorder::newCause() const
{
    ShowTraceCause cause;
    cause.generation = ShowEventLog::generation();
    if (cause.generation == 0)
        return cause;
    cause.trace = ShowEventLog::newTrace();
    cause.showId = trackedShowId();
    cause.take = m_takeId;
    return cause;
}

ShowEventLog::Entry ShowCommandRecorder::causeEntry(const ShowTraceCause &cause, Phase phase, Outcome outcome,
                                                    const QString &reason) const
{
    ShowEventLog::Entry e;
    if (cause.generation == 0 || cause.generation != ShowEventLog::generation())
        return e;
    e.generation = cause.generation;
    e.trace = cause.trace;
    e.phase = phase;
    e.outcome = outcome;
    e.reason = reason;
    e.showId = cause.showId;
    e.take = cause.take;
    e.origin = cause.origin;
    e.widgetId = cause.widgetId;
    e.controlId = cause.controlId;
    e.control = cause.control;
    return e;
}

ShowTraceCause ShowCommandRecorder::currentCause() const
{
    if (m_tracedRequest != nullptr)
        return m_tracedRequest->cause;
    if (m_operation != nullptr)
        return *m_operation;
    return newCause();
}

void ShowCommandRecorder::traceCommand(const ShowTraceCause &cause, Phase phase, Outcome outcome,
                                       const ShowCommand *before, const ShowCommand *after, quint32 showId,
                                       const QString &reason, const QString &action) const
{
    ShowEventLog::Entry e = causeEntry(cause, phase, outcome, reason);
    if (e.generation == 0)
        return;

    e.showId = showId;
    const ShowCommand *cmd = after != nullptr ? after : before;
    if (cmd != nullptr)
    {
        e.commandId = cmd->id;
        e.functionId = cmd->functionId;
        e.controlId = cmd->controlId;
        e.showTimeMs = cmd->time;
        e.value = commandValueText(*cmd);
        e.action = action.isEmpty() ? commandActionText(*cmd) : action;
    }
    else
    {
        e.action = action;
    }
    if (before != nullptr)
        e.before = commandText(*before);
    if (after != nullptr)
        e.after = commandText(*after);
    ShowEventLog::append(e);
}

void ShowCommandRecorder::traceExtent(const ShowTraceCause &cause, quint32 showId, quint32 before, quint32 after,
                                      const QString &reason) const
{
    ShowEventLog::Entry e = causeEntry(cause, Phase::Commit, Outcome::Published, reason);
    if (e.generation == 0)
        return;
    e.showId = showId;
    e.action = tr("Extent");
    e.before = QString::number(before / 1000.0, 'f', 3);
    e.after = QString::number(after / 1000.0, 'f', 3);
    ShowEventLog::append(e);
}

void ShowCommandRecorder::traceTrackDelta(const ShowTraceCause &cause, const ShowCommandTrack &before,
                                          const ShowCommandTrack &after, quint32 showId, const QString &reason) const
{
    if (cause.generation == 0 || cause.generation != ShowEventLog::generation())
        return;

    QHash<quint32, int> added;
    for (int i = 0; i < after.commands().count(); i++)
        added.insert(after.commands().at(i).id, i);
    for (const ShowCommand &old : before.commands())
    {
        const auto it = added.constFind(old.id);
        if (it == added.constEnd())
        {
            traceCommand(cause, Phase::Commit, Outcome::Published, &old, nullptr, showId, reason);
            continue;
        }
        const ShowCommand &now = after.commands().at(it.value());
        if (now != old || now.order != old.order)
            traceCommand(cause, Phase::Commit, Outcome::Published, &old, &now, showId, reason);
        added.remove(old.id);
    }
    for (const ShowCommand &now : after.commands())
    {
        if (added.contains(now.id))
            traceCommand(cause, Phase::Commit, Outcome::Published, nullptr, &now, showId, reason);
    }
    if (before.extent() != after.extent())
        traceExtent(cause, showId, before.extent(), after.extent(), reason);
}

void ShowCommandRecorder::traceTransport(Outcome outcome, const QString &action, const QString &value,
                                         const QString &reason) const
{
    const quint64 g = ShowEventLog::generation();
    if (g == 0)
        return;
    ShowEventLog::Entry e = traceEntry(g, Phase::Transport, outcome, reason);
    e.action = action;
    e.value = value;
    if (Show *show = trackedShow())
        e.showTimeMs = acceptancePosition(show);
    ShowEventLog::append(e);
}

void ShowCommandRecorder::reportFailure(const QString &error, ShowEventLog::View view)
{
    ShowEventLog::failure(error, view);
    setError(error);
}

namespace
{
bool s_tracedCall = false;
}

ShowCommandRecorder::TracedCall::TracedCall()
    : m_previous(s_tracedCall)
{
    s_tracedCall = true;
}

ShowCommandRecorder::TracedCall::~TracedCall()
{
    s_tracedCall = m_previous;
}

void ShowCommandRecorder::traceDirectRequest(VCWidget *control, int value)
{
    if (ShowEventLog::generation() == 0 || s_tracedCall)
        return;
    // arrival only: what the control then does is its own business
    traceInput(control, ShowCommandOrigin::Programmatic, Outcome::Received,
               QT_TRANSLATE_NOOP("ShowCommandRecorder", "direct code request, not recorded"), value);
}

void ShowCommandRecorder::traceInput(VCWidget *control, ShowCommandOrigin origin, Outcome outcome,
                                     const char *reason, int value)
{
    const quint64 g = ShowEventLog::generation();
    if (g == 0)
        return;

    ShowEventLog::Entry e;
    e.generation = g;
    e.phase = Phase::Input;
    e.outcome = outcome;
    e.origin = int(origin);
    e.reason = tr(reason);
    if (control != nullptr)
    {
        e.control = control->caption();
        e.widgetId = control->id();
        e.controlId = control->recordingId();
    }
    if (value != INT_MIN)
        e.value = QString::number(value);
    if (ShowCommandRecorder *recorder = instance())
    {
        e.showId = recorder->trackedShowId();
        e.take = recorder->m_takeId;
    }
    ShowEventLog::append(e);
}

/*********************************************************************
 * Replay executor
 *********************************************************************/

void ShowCommandRecorder::slotFunctionAdded(quint32 id)
{
    Show *show = qobject_cast<Show *>(m_doc->function(id));
    const QPair<quint32, quint32> floor = m_eventIdFloors.value(id, qMakePair(0U, 0U));
    if (show != nullptr &&
        (floor.first > show->commandTrack().nextEventId() || floor.second > show->commandTrack().nextOrder()))
    {
        ShowCommandTrack restored = show->commandTrack();
        restored.reserve(floor.first, floor.second);
        show->setCommandTrack(restored);
    }
    attachShow(show);
}

void ShowCommandRecorder::rememberEventIds(const Show *show)
{
    QPair<quint32, quint32> &floor = m_eventIdFloors[show->id()];
    floor.first = qMax(floor.first, show->commandTrack().nextEventId());
    floor.second = qMax(floor.second, show->commandTrack().nextOrder());
}

void ShowCommandRecorder::attachShow(Show *show)
{
    if (show == nullptr || m_attachedShows.contains(show))
        return;

    m_attachedShows.append(show);
    rememberEventIds(show);
    show->attachControlExecutor(true);
    const auto queued = Qt::ConnectionType(Qt::QueuedConnection | Qt::UniqueConnection);
    connect(show, &Show::controlBatchesReady, this,
            &ShowCommandRecorder::slotControlBatchesReady, queued);
    connect(show, &Show::functionReceiptReady, this,
            &ShowCommandRecorder::slotFunctionReceiptReady, queued);
    connect(show, &Show::commandTraversalCancelled, this,
            &ShowCommandRecorder::slotCommandTraversalCancelled, queued);
    connect(show, &Show::commandWorkDrained, this, &ShowCommandRecorder::drainUserRequests, queued);
    connect(show, &Function::running, this, &ShowCommandRecorder::slotShowPlayback);
    connect(show, qOverload<quint32>(&Function::stopped), this, &ShowCommandRecorder::slotShowPlayback);
    connect(show, qOverload<quint32>(&Function::stopped), this, &ShowCommandRecorder::slotTimelineReset);
    connect(show, &Show::syncSourceChanged, this, &ShowCommandRecorder::slotTimelineReset);
}

void ShowCommandRecorder::slotTimelineReset()
{
    Show *show = qobject_cast<Show *>(sender());
    if (show == nullptr)
        return;
    const quint32 showId = show->id();
    m_timeline.erase(std::remove_if(m_timeline.begin(), m_timeline.end(),
                                    [showId](const TimelineFact &fact) { return fact.showId == showId; }),
                     m_timeline.end());
}

void ShowCommandRecorder::slotShowPlayback()
{
    emit editGateChanged();
}

void ShowCommandRecorder::setRowsObserved(bool observed)
{
    if (observed == m_rowsObserved)
        return;
    m_rowsObserved = observed;
    observeRowSources();
}

QString ShowCommandRecorder::bindingText(VCWidget *widget) const
{
    const ShowControlConfiguration current = configurationOf(widget);
    Function *bound = m_doc->function(current.functionId);
    QStringList parts;
    switch (current.snapshot.role)
    {
        case ShowControlRole::ToggleButton:
        case ShowControlRole::AdjustSlider:
            parts.append(bound != nullptr ? bound->name() : tr("no function"));
            if (current.snapshot.role == ShowControlRole::AdjustSlider)
                parts.append(current.snapshot.attribute);
        break;
        case ShowControlRole::LevelSlider:
            parts.append(tr("%n channel(s)", "", current.levelChannels.count()));
        break;
        case ShowControlRole::SubmasterSlider: parts.append(tr("Submaster")); break;
        case ShowControlRole::GrandMasterSlider: parts.append(tr("Grand Master")); break;
        case ShowControlRole::None: break;
    }
    return parts.join(QStringLiteral(" · "));
}

namespace
{
/** Missing first, Ready last */
int statusRank(ShowControlStatus status)
{
    switch (status)
    {
        case ShowControlStatus::Missing: return 0;
        case ShowControlStatus::Ambiguous: return 1;
        case ShowControlStatus::Incompatible: return 2;
        case ShowControlStatus::Disabled: return 3;
        case ShowControlStatus::Unbound: return 4;
        case ShowControlStatus::Ready: break;
    }
    return 5;
}

QString statusKey(ShowControlStatus status)
{
    switch (status)
    {
        case ShowControlStatus::Missing: return QStringLiteral("Missing");
        case ShowControlStatus::Ambiguous: return QStringLiteral("Ambiguous");
        case ShowControlStatus::Incompatible: return QStringLiteral("Incompatible");
        case ShowControlStatus::Disabled: return QStringLiteral("Disabled");
        case ShowControlStatus::Unbound: return QStringLiteral("Unbound");
        case ShowControlStatus::Ready: break;
    }
    return QStringLiteral("Ready");
}

QString statusLabel(ShowControlStatus status)
{
    switch (status)
    {
        case ShowControlStatus::Missing: return ShowCommandRecorder::tr("Missing control");
        case ShowControlStatus::Ambiguous: return ShowCommandRecorder::tr("Ambiguous identity");
        case ShowControlStatus::Incompatible: return ShowCommandRecorder::tr("Incompatible");
        case ShowControlStatus::Disabled: return ShowCommandRecorder::tr("Disabled");
        case ShowControlStatus::Unbound: return ShowCommandRecorder::tr("Unbound");
        case ShowControlStatus::Ready: break;
    }
    return ShowCommandRecorder::tr("Ready");
}

QString roleLabel(ShowControlRole role)
{
    switch (role)
    {
        case ShowControlRole::ToggleButton: return ShowCommandRecorder::tr("Toggle button");
        case ShowControlRole::LevelSlider: return ShowCommandRecorder::tr("Level slider");
        case ShowControlRole::AdjustSlider: return ShowCommandRecorder::tr("Adjust slider");
        case ShowControlRole::SubmasterSlider: return ShowCommandRecorder::tr("Submaster slider");
        case ShowControlRole::GrandMasterSlider: return ShowCommandRecorder::tr("Grand Master slider");
        case ShowControlRole::None: break;
    }
    return QString();
}
} // namespace

QVariantList ShowCommandRecorder::referencedControls() const
{
    struct Row
    {
        QUuid id;
        QList<VCWidget *> widgets;
        QVariantList roles;
        QStringList summary;
        int count = 0;
        ShowControlStatus worst = ShowControlStatus::Ready;
    };
    QVector<Row> rows;
    for (const ShowControlReference &ref : m_track.referencedControls())
    {
        auto it = std::find_if(rows.begin(), rows.end(), [&ref](const Row &row) { return row.id == ref.controlId; });
        if (it == rows.end())
        {
            Row row;
            row.id = ref.controlId;
            if (m_vc != nullptr)
                row.widgets = m_vc->widgetsByRecordingId(ref.controlId);
            rows.append(row);
            it = rows.end() - 1;
        }

        // the replay resolution of what this role expects
        QVector<ShowControlSnapshot> matches;
        for (VCWidget *widget : std::as_const(it->widgets))
            matches.append(configurationOf(widget).snapshot);
        ShowCommand expected;
        expected.controlId = ref.controlId;
        expected.role = ref.role;
        expected.attribute = ref.attribute;
        QString reason;
        const ShowControlStatus status = ShowCommandFsm::resolveControl(expected, matches, &reason);

        QString role = roleLabel(ref.role);
        if (!ref.attribute.isEmpty())
            role += QStringLiteral(" (%1)").arg(ref.attribute);
        QVariantMap entry;
        entry["role"] = ShowCommand::roleToString(ref.role);
        entry["label"] = role;
        entry["count"] = ref.count;
        entry["status"] = statusKey(status);
        entry["reason"] = reason;
        it->roles.append(entry);
        it->summary.append(reason.isEmpty() ? tr("%1 ×%2: %3").arg(role).arg(ref.count).arg(statusLabel(status))
                                            : tr("%1 ×%2: %3, %4").arg(role).arg(ref.count).arg(statusLabel(status), reason));
        it->count += ref.count;
        if (statusRank(status) < statusRank(it->worst))
            it->worst = status;
    }

    std::stable_sort(rows.begin(), rows.end(),
                     [](const Row &a, const Row &b) { return statusRank(a.worst) < statusRank(b.worst); });

    QVariantList list;
    for (const Row &row : std::as_const(rows))
    {
        QVariantMap map;
        map["controlId"] = row.id.toString(QUuid::WithoutBraces);
        map["caption"] = row.widgets.isEmpty() ? tr("Missing control") : row.widgets.first()->caption();
        map["binding"] = row.widgets.isEmpty() ? QString() : bindingText(row.widgets.first());
        map["count"] = row.count;
        map["roles"] = row.roles;
        map["status"] = statusKey(row.worst);
        map["statusText"] = statusLabel(row.worst);
        map["problem"] = row.worst != ShowControlStatus::Ready;
        map["detail"] = row.summary.join(QStringLiteral("; "));
        list.append(map);
    }
    return list;
}

void ShowCommandRecorder::setReferencesObserved(bool observed)
{
    if (observed == m_referencesObserved)
        return;
    m_referencesObserved = observed;
    observeRowSources();
    if (observed)
        emit referencesChanged();
}

void ShowCommandRecorder::observeRowSources()
{
    for (const QMetaObject::Connection &connection : std::as_const(m_rowSources))
        disconnect(connection);
    m_rowSources.clear();
    if ((m_rowsObserved == false && m_referencesObserved == false) || m_doc == nullptr)
        return;

    // queued: a control being destroyed or reconfigured is read once it settled.
    // Whoever still observes when it arrives reads the current configuration.
    const auto refresh = [this]()
    {
        if (m_rowsRefreshPending)
            return;
        m_rowsRefreshPending = true;
        QMetaObject::invokeMethod(this, [this]()
        {
            m_rowsRefreshPending = false;
            if (m_rowsObserved)
            {
                emit commandsChanged();
            }
            else if (m_referencesObserved)
            {
                observeRowSources();
                emit referencesChanged();
            }
        }, Qt::QueuedConnection);
    };
    QSet<QObject *> followed;
    const auto follow = [&](QObject *source)
    {
        if (source == nullptr || followed.contains(source))
            return;
        followed.insert(source);
        m_rowSources.append(connect(source, &QObject::destroyed, this, refresh));
        if (Function *function = qobject_cast<Function *>(source))
            m_rowSources.append(connect(function, &Function::nameChanged, this, refresh));
        if (VCWidget *widget = qobject_cast<VCWidget *>(source))
        {
            m_rowSources.append(connect(widget, &VCWidget::captionChanged, this, refresh));
            m_rowSources.append(connect(widget, &VCWidget::disabledStateChanged, this, refresh));
        }
        if (VCButton *button = qobject_cast<VCButton *>(source))
        {
            m_rowSources.append(connect(button, &VCButton::functionIDChanged, this, refresh));
            m_rowSources.append(connect(button, &VCButton::actionTypeChanged, this, refresh));
        }
        if (VCSlider *slider = qobject_cast<VCSlider *>(source))
        {
            m_rowSources.append(connect(slider, &VCSlider::sliderModeChanged, this, refresh));
            m_rowSources.append(connect(slider, &VCSlider::controlledFunctionChanged, this, refresh));
            m_rowSources.append(connect(slider, &VCSlider::controlledAttributeChanged, this, refresh));
            m_rowSources.append(connect(slider, &VCSlider::channelsCountChanged, this, refresh));
        }
    };

    // a control or function appearing may be one a row names: restored by
    // undo, pasted or loaded, with the identity it had
    if (m_vc != nullptr)
        m_rowSources.append(connect(m_vc, &VirtualConsole::widgetsChanged, this, refresh));
    m_rowSources.append(connect(m_doc, &Doc::functionAdded, this, refresh));
    m_rowSources.append(connect(m_doc, &Doc::functionRemoved, this, refresh));

    for (const ShowCommand &cmd : m_track.commands())
    {
        if (cmd.controlId.isNull())
        {
            follow(m_doc->function(cmd.functionId));
            continue;
        }
        const QList<VCWidget *> widgets = m_vc != nullptr ? m_vc->widgetsByRecordingId(cmd.controlId)
                                                          : QList<VCWidget *>();
        for (VCWidget *widget : widgets)
        {
            follow(widget);
            follow(m_doc->function(configurationOf(widget).functionId));
        }
    }
}

void ShowCommandRecorder::slotControlBatchesReady()
{
    Show *show = qobject_cast<Show *>(sender());
    if (show == nullptr)
        return;

    for (const ShowControlBatch &batch : show->takeControlBatches())
    {
        // a run of an earlier traversal is abandoned unacknowledged
        m_controlRuns.erase(std::remove_if(m_controlRuns.begin(), m_controlRuns.end(),
                                           [show](const ControlRun &run)
                                           { return run.show.isNull() || run.show == show; }),
                            m_controlRuns.end());

        ControlRun run;
        run.show = show;
        run.batch = batch;
        if (const quint64 g = ShowEventLog::generation())
        {
            ShowEventLog::Entry e = traceEntry(g, Phase::Execute, Outcome::Received, tr("crossed recorded VC states"));
            e.origin = int(ShowCommandOrigin::Replay);
            e.showId = show->id();
            e.traversal = batch.traversal;
            e.action = tr("Replay batch");
            e.value = tr("%n command(s)", "", batch.commands.count());
            ShowEventLog::append(e);
        }
        for (quint32 functionId : batch.engineStarted)
            run.engineDone.append(qMakePair(couplingOf(nullptr, functionId), ShowCommandFsm::ShowButtonOp::Start));
        for (quint32 functionId : batch.engineStopped)
            run.engineDone.append(qMakePair(couplingOf(nullptr, functionId), ShowCommandFsm::ShowButtonOp::Stop));
        if (continueControlRun(run) == false)
            m_controlRuns.append(run);
    }
    drainUserRequests();
}

void ShowCommandRecorder::slotFunctionReceiptReady(quint64 traversal, quint64 opId)
{
    for (int i = 0; i < m_controlRuns.count(); i++)
    {
        ControlRun &run = m_controlRuns[i];
        if (run.waitingOp != opId)
            continue;

        run.waitingOp = 0;
        if (run.show.isNull() || run.batch.traversal != traversal || continueControlRun(run))
            m_controlRuns.removeAt(i);
        drainUserRequests();
        return;
    }
}

void ShowCommandRecorder::slotRecordedWriteRetired(quint64 generation, int outcome, quint32 functionId, int effect)
{
    Q_UNUSED(outcome)

    VCSlider *slider = qobject_cast<VCSlider *>(sender());
    if (slider == nullptr)
        return;

    for (int i = 0; i < m_controlRuns.count(); i++)
    {
        ControlRun &run = m_controlRuns[i];
        if (run.waitingWrites.removeAll(qMakePair(slider->id(), generation)) == 0)
            continue;

        if (effect != VCSlider::RecordedWriteNoEffect)
        {
            ShowFunctionExpectation native;
            native.functionId = functionId;
            native.expectLive = effect == VCSlider::RecordedWriteStarted;
            run.barrier.append(native);
        }
        if (!run.waitingWrites.isEmpty())
            return;

        if (run.show.isNull())
        {
            m_controlRuns.removeAt(i);
        }
        else if (!run.barrier.isEmpty())
        {
            // the writes only queued their starts and stops: done once the timer ran them
            run.waitingOp = ++m_lastReceiptOp;
            run.show->requestFunctionReceipt(run.batch.traversal, run.waitingOp,
                                             std::exchange(run.barrier, QVector<ShowFunctionExpectation>()));
        }
        else if (continueControlRun(run))
        {
            m_controlRuns.removeAt(i);
        }
        drainUserRequests();
        return;
    }
}

void ShowCommandRecorder::slotCommandTraversalCancelled()
{
    if (const quint64 g = ShowEventLog::generation())
    {
        for (const ControlRun &run : std::as_const(m_controlRuns))
        {
            if (!run.show.isNull() && run.show->commandTraversal() == run.batch.traversal)
                continue;
            ShowEventLog::Entry e = traceEntry(g, Phase::Execute, Outcome::Cancelled,
                                               tr("a seek, stop or unload ended its traversal"));
            e.origin = int(ShowCommandOrigin::Replay);
            e.showId = run.show.isNull() ? ShowEventLog::NoId : run.show->id();
            e.traversal = run.batch.traversal;
            e.action = tr("Replay batch");
            e.value = tr("%n command(s) not run", "", run.batch.commands.count() - run.next);
            ShowEventLog::append(e);
        }
    }
    // a run parked on a receipt ends here, unacknowledged
    m_controlRuns.erase(std::remove_if(m_controlRuns.begin(), m_controlRuns.end(),
                                       [](const ControlRun &run)
                                       {
                                           return run.show.isNull() ||
                                                  run.show->commandTraversal() != run.batch.traversal;
                                       }),
                        m_controlRuns.end());
    // replay work ended, the accepted requests stay
    drainUserRequests();
}

bool ShowCommandRecorder::continueControlRun(ControlRun &run)
{
    using ShowCommandFsm::ShowButtonOp;

    // the boundary of an applied op, for whatever depends on it next
    const auto expect = [](const QPair<ShowControlCoupling, ShowButtonOp> &done,
                           const ShowControlCoupling *next)
    {
        QVector<ShowFunctionExpectation> expectations;
        ShowFunctionExpectation own;
        own.functionId = done.first.functionId;
        own.expectLive = done.second == ShowButtonOp::Start;
        expectations.append(own);
        const auto member = [&](quint32 functionId)
        {
            ShowFunctionExpectation child;
            child.functionId = functionId;
            child.causeFunctionId = done.first.functionId;
            expectations.append(child);
        };
        if (own.expectLive && next != nullptr)
        {
            if (done.first.startsFunctions.contains(next->functionId))
                member(next->functionId);
            // a started member whose Solo Frame sibling the next op starts
            for (const QPair<quint32, quint32> &solo : done.first.memberSoloGroups)
            {
                // the function itself is already covered by its own expectation
                if (next->soloGroup != ShowCommand::InvalidId && solo.second == next->soloGroup &&
                    solo.first != done.first.functionId)
                    member(solo.first);
            }
        }
        return expectations;
    };

    const auto wait = [this, &run](const QVector<ShowFunctionExpectation> &expectations)
    {
        run.waitingOp = ++m_lastReceiptOp;
        run.show->requestFunctionReceipt(run.batch.traversal, run.waitingOp, expectations);
    };

    const auto cancelled = [&run]() { return run.show->commandTraversal() != run.batch.traversal; };

    if (run.batch.rollbackTo.has_value() && run.rolledBack == false)
    {
        if (cancelled())
            return true;
        run.rolledBack = true;
        rollBackTimeline(run);
        if (!run.waitingWrites.isEmpty())
            return false;
        if (!run.barrier.isEmpty())
        {
            wait(std::exchange(run.barrier, QVector<ShowFunctionExpectation>()));
            return false;
        }
    }

    while (run.next < run.batch.commands.count())
    {
        if (cancelled())
            return true;

        const ShowCommand &cmd = run.batch.commands.at(run.next);
        VCWidget *control = resolveControl(cmd);
        VCButton *button = qobject_cast<VCButton *>(control);
        VCSlider *slider = qobject_cast<VCSlider *>(control);
        // diagnostics of this op belong to the observation it started in
        const quint64 g = ShowEventLog::generation();
        const auto trace = [&](Outcome outcome, const QString &reason)
        {
            if (g == 0)
                return;
            ShowEventLog::Entry e = traceEntry(g, Phase::Execute, outcome, reason);
            e.origin = int(ShowCommandOrigin::Replay);
            e.showId = run.show->id();
            e.traversal = run.batch.traversal;
            e.commandId = cmd.id;
            e.controlId = cmd.controlId;
            e.showTimeMs = cmd.time;
            e.action = commandActionText(cmd);
            e.value = commandValueText(cmd);
            if (control != nullptr)
            {
                e.control = control->caption();
                e.widgetId = control->id();
            }
            ShowEventLog::append(e);
        };
        if (button == nullptr && slider == nullptr)
        {
            // the same resolution, now for its reason
            QVector<ShowControlSnapshot> matches;
            if (m_vc != nullptr)
            {
                for (VCWidget *widget : m_vc->widgetsByRecordingId(cmd.controlId))
                    matches.append(configurationOf(widget).snapshot);
            }
            QString reason;
            ShowCommandFsm::resolveControl(cmd, matches, &reason);
            const QString why = tr("Replay skipped %1: %2").arg(cmd.controlId.toString(QUuid::WithoutBraces), reason);
            trace(Outcome::Skipped, why);
            ShowEventLog::failure(why, ShowEventLog::View::References);
            run.next++;
            continue;
        }

        const ShowControlCoupling coupling = couplingOf(control, controlFunction(control));
        QVector<ShowFunctionExpectation> expectations;
        for (int i = run.unsettled.count() - 1; i >= 0; i--)
        {
            if (!ShowCommandFsm::controlDependsOn(coupling, run.unsettled.at(i).first))
                continue;
            expectations += expect(run.unsettled.at(i), &coupling);
            run.unsettled.removeAt(i);
        }
        for (int i = run.engineDone.count() - 1; i >= 0; i--)
        {
            if (!ShowCommandFsm::controlDependsOn(coupling, run.engineDone.at(i).first))
                continue;
            expectations += expect(run.engineDone.at(i), &coupling);
            run.engineDone.removeAt(i);
        }
        if (!expectations.isEmpty())
        {
            wait(expectations);
            return false;
        }

        run.next++;
        const QVector<TimelineFact> closure = closureOf(control);
        if (button != nullptr)
        {
            const ShowButtonOp op = button->applyRecordedState(cmd.on);
            journal(closure, run.show->id(), cmd.time, op == ShowButtonOp::Start);
            if (op != ShowButtonOp::None)
                run.unsettled.append(qMakePair(coupling, op));
            if (g != 0)
                trace(op == ShowButtonOp::None ? Outcome::Skipped : Outcome::Requested,
                      op == ShowButtonOp::None ? tr("already in the recorded state")
                      : op == ShowButtonOp::Start ? tr("native start through the button")
                                                  : tr("native stop through the button"));
            continue;
        }

        const int valueBefore = slider->value();
        const quint64 generation = slider->applyRecordedPosition(cmd.position);
        journal(closure, run.show->id(), cmd.time, false);
        if (g != 0)
            trace(slider->value() == valueBefore ? Outcome::Skipped
                  : generation != 0 ? Outcome::Requested : Outcome::Applied,
                  slider->value() == valueBefore ? tr("already at the recorded position")
                  : generation != 0 ? tr("value %1 set, its write is pending").arg(slider->value())
                                    : tr("value %1 set").arg(slider->value()));
        awaitRecordedWrite(run, slider, generation);
        if (generation == 0)
            expectSliderStart(run, slider);
        // a Submaster rescales its frame's sliders, which keep the writes they owe
        VCWidget *frame = qobject_cast<VCWidget *>(slider->parent());
        if (slider->sliderMode() == VCSlider::Submaster && frame != nullptr)
        {
            for (VCSlider *child : frame->findChildren<VCSlider *>())
            {
                if (child == slider)
                    continue;
                awaitRecordedWrite(run, child, child->awaitPendingWrite());
                expectSliderStart(run, child);
            }
        }
        if (!run.waitingWrites.isEmpty())
            return false;
        if (!run.barrier.isEmpty())
        {
            wait(std::exchange(run.barrier, QVector<ShowFunctionExpectation>()));
            return false;
        }
    }

    if (cancelled())
        return true;

    // acknowledged only after the last boundary of the batch
    if (!run.unsettled.isEmpty())
    {
        QVector<ShowFunctionExpectation> expectations;
        for (const auto &done : run.unsettled)
            expectations += expect(done, nullptr);
        run.unsettled.clear();
        wait(expectations);
        return false;
    }

    run.show->acknowledgeControlBatch(run.batch.traversal, run.batch.seq);
    return true;
}

void ShowCommandRecorder::awaitRecordedWrite(ControlRun &run, VCSlider *slider, quint64 generation)
{
    // ponytail: every later op of the batch waits for this write (one tick),
    // wait per dependency instead if a batch ever needs the overlap
    if (generation == 0)
        return;
    run.waitingWrites.append(qMakePair(slider->id(), generation));
    connect(slider, &VCSlider::recordedWriteRetired, this,
            &ShowCommandRecorder::slotRecordedWriteRetired, Qt::UniqueConnection);
}

void ShowCommandRecorder::expectSliderStart(ControlRun &run, VCSlider *slider)
{
    // Only an Adjust slider on Intensity starts its Function. A start an
    // earlier write queued, even one that landed before anything waited,
    // must be live first: observed, never acted on.
    if (slider->sliderMode() == VCSlider::Adjust && slider->controlledAttribute() == Function::Intensity &&
        m_doc->function(slider->controlledFunction()) != nullptr)
    {
        ShowFunctionExpectation start;
        start.functionId = slider->controlledFunction();
        run.barrier.append(start);
    }
}

namespace
{
int timelineState(VCWidget *control)
{
    if (VCButton *button = qobject_cast<VCButton *>(control))
        return button->state() == VCButton::Active ? 1 : 0;
    if (VCSlider *slider = qobject_cast<VCSlider *>(control))
        return slider->value();
    return 0;
}
}

QVector<ShowCommandRecorder::TimelineFact> ShowCommandRecorder::closureOf(VCWidget *control) const
{
    const int state = timelineState(control);
    QVector<TimelineFact> closure{ { ShowCommand::InvalidId, 0, control, state, state } };
    const auto add = [&closure](VCWidget *member, bool stopped) {
        for (const TimelineFact &known : std::as_const(closure))
        {
            if (known.control == member)
                return;
        }
        const int before = timelineState(member);
        closure.append({ ShowCommand::InvalidId, 0, member, before, stopped ? 0 : before });
    };

    if (VCButton *button = qobject_cast<VCButton *>(control))
    {
        // the Solo Frames a start reaches: the button's own, and those of the
        // buttons of what it starts, each told which Function started
        const ShowControlCoupling coupling = couplingOf(control, controlFunction(control));
        QVector<QPair<quint32, quint32>> starts = coupling.memberSoloGroups;
        if (coupling.soloGroup != ShowCommand::InvalidId)
            starts.prepend(qMakePair(coupling.functionId, coupling.soloGroup));
        for (const QPair<quint32, quint32> &start : std::as_const(starts))
        {
            VCSoloFrame *frame = m_vc != nullptr ? qobject_cast<VCSoloFrame *>(m_vc->widget(start.second)) : nullptr;
            if (frame == nullptr)
                continue;
            for (VCWidget *child : frame->children(true))
            {
                VCButton *sibling = qobject_cast<VCButton *>(child);
                if (sibling != nullptr && sibling != button)
                    add(sibling, sibling->soloStartStops(start.first, frame->excludeMonitoredFunctions()));
            }
        }
    }
    else if (VCSlider *slider = qobject_cast<VCSlider *>(control))
    {
        VCWidget *frame = qobject_cast<VCWidget *>(slider->parent());
        if (slider->sliderMode() == VCSlider::Submaster && frame != nullptr)
        {
            for (VCSlider *child : frame->findChildren<VCSlider *>())
                add(child, false);
        }
    }
    return closure;
}

void ShowCommandRecorder::journal(QVector<TimelineFact> closure, quint32 showId, quint32 time, bool started)
{
    const bool button = qobject_cast<VCButton *>(closure.first().control) != nullptr;
    for (int i = 0; i < closure.count(); i++)
    {
        TimelineFact &fact = closure[i];
        if (fact.control.isNull())
            continue;
        fact.showId = showId;
        fact.time = time;
        // a sibling's stop lands later, in slotFunctionStopped: its after is the predicted Solo effect
        if (i == 0 || button == false)
            fact.after = timelineState(fact.control);
        else if (started == false)
            fact.after = fact.before;
        m_timeline.append(fact);
    }
}

void ShowCommandRecorder::rollBackTimeline(ControlRun &run)
{
    using ShowCommandFsm::ShowButtonOp;
    const quint32 showId = run.show->id();
    const quint32 to = *run.batch.rollbackTo;

    // per control: the state before its first fact after `to`, in the order
    // they were journaled, and the state its latest fact left it in
    QVector<QPair<QPointer<VCWidget>, int>> targets;
    QHash<VCWidget *, int> latest;
    for (const TimelineFact &fact : std::as_const(m_timeline))
    {
        if (fact.showId != showId || fact.control.isNull())
            continue;
        latest.insert(fact.control, fact.after);
        const bool known = std::any_of(targets.cbegin(), targets.cend(),
                                       [&fact](const QPair<QPointer<VCWidget>, int> &target)
                                       { return target.first == fact.control; });
        if (fact.time > to && known == false)
            targets.append(qMakePair(fact.control, fact.before));
    }
    m_timeline.erase(std::remove_if(m_timeline.begin(), m_timeline.end(),
                                    [showId, to](const TimelineFact &fact)
                                    { return fact.showId == showId && (fact.time > to || fact.control.isNull()); }),
                     m_timeline.end());

    // something else changed it since the timeline did: that change wins
    const auto changedElsewhere = [&latest](VCWidget *control) {
        const auto it = latest.constFind(control);
        return it == latest.cend() ? timelineState(control) != 0 : timelineState(control) != it.value();
    };
    // the Solo Frames a button's start reaches: its own, and those of the
    // buttons of what it starts
    const auto reachedFrames = [this](VCButton *button) {
        QSet<quint32> frames;
        const ShowControlCoupling coupling = couplingOf(button, controlFunction(button));
        if (coupling.soloGroup != ShowCommand::InvalidId)
            frames.insert(coupling.soloGroup);
        for (const QPair<quint32, quint32> &member : coupling.memberSoloGroups)
            frames.insert(member.second);
        return frames;
    };
    QSet<quint32> guardedFrames;
    for (const auto &target : std::as_const(targets))
    {
        VCButton *button = qobject_cast<VCButton *>(target.first);
        if (button == nullptr)
            continue;
        for (const quint32 frameId : reachedFrames(button))
        {
            VCSoloFrame *frame = m_vc != nullptr ? qobject_cast<VCSoloFrame *>(m_vc->widget(frameId)) : nullptr;
            if (frame == nullptr || guardedFrames.contains(frameId))
                continue;
            for (VCWidget *child : frame->children(true))
            {
                if (qobject_cast<VCButton *>(child) != nullptr && changedElsewhere(child))
                    guardedFrames.insert(frameId);
            }
        }
    }
    // another Active Toggle button holds the same Function: stopping would end it for that one too
    const auto heldElsewhere = [this](VCButton *button) {
        for (int page = 0; m_vc != nullptr && page < m_vc->pagesCount(); page++)
        {
            for (VCWidget *widget : m_vc->page(page)->children(true))
            {
                VCButton *other = qobject_cast<VCButton *>(widget);
                if (other != nullptr && other != button && other->functionID() == button->functionID() &&
                    other->actionType() == VCButton::Toggle && other->state() == VCButton::Active)
                    return true;
            }
        }
        return false;
    };

    // one delta: buttons Off, slider values, then buttons On
    for (int pass = 0; pass < 3; pass++)
    {
        for (const auto &target : std::as_const(targets))
        {
            VCWidget *control = target.first;
            if (control == nullptr || timelineState(control) == target.second || changedElsewhere(control))
                continue;
            VCButton *button = qobject_cast<VCButton *>(control);
            VCSlider *slider = qobject_cast<VCSlider *>(control);
            if (slider != nullptr && pass == 1)
            {
                const quint64 generation = slider->applyRecordedValue(target.second);
                awaitRecordedWrite(run, slider, generation);
                if (generation == 0)
                    expectSliderStart(run, slider);
            }
            else if (button != nullptr && pass == (target.second != 0 ? 2 : 0))
            {
                // switching a member On is the Solo operation that stops others
                if (target.second != 0 && reachedFrames(button).intersects(guardedFrames))
                    continue;
                if (target.second == 0 && heldElsewhere(button) && button->releaseToMonitoring())
                    continue;
                const ShowButtonOp op = button->applyRecordedState(target.second != 0);
                if (op != ShowButtonOp::None)
                    run.unsettled.append(qMakePair(couplingOf(button, controlFunction(button)), op));
            }
        }
    }
}

VCWidget *ShowCommandRecorder::resolveControl(const ShowCommand &cmd) const
{
    if (m_vc == nullptr)
        return nullptr;

    const QList<VCWidget *> widgets = m_vc->widgetsByRecordingId(cmd.controlId);
    QVector<ShowControlSnapshot> matches;
    for (VCWidget *widget : widgets)
        matches.append(configurationOf(widget).snapshot);

    if (ShowCommandFsm::resolveControl(cmd, matches, nullptr) != ShowControlStatus::Ready)
        return nullptr;

    return widgets.first();
}

ShowControlConfiguration ShowCommandRecorder::configurationOf(VCWidget *widget) const
{
    ShowControlConfiguration configuration;
    ShowControlSnapshot &snapshot = configuration.snapshot;
    snapshot.enabled = !widget->isDisabled();
    configuration.functionId = controlFunction(widget);
    VCButton *button = qobject_cast<VCButton *>(widget);
    if (button != nullptr && button->actionType() == VCButton::Toggle)
    {
        snapshot.role = ShowControlRole::ToggleButton;
        snapshot.bound = m_doc->function(button->functionID()) != nullptr;
    }
    VCSlider *slider = qobject_cast<VCSlider *>(widget);
    if (slider != nullptr)
    {
        switch (slider->sliderMode())
        {
            case VCSlider::Level:
                snapshot.role = ShowControlRole::LevelSlider;
                configuration.levelChannels = slider->levelChannels();
            break;
            case VCSlider::Submaster: snapshot.role = ShowControlRole::SubmasterSlider; break;
            case VCSlider::GrandMaster: snapshot.role = ShowControlRole::GrandMasterSlider; break;
            case VCSlider::Adjust:
            {
                Function *function = m_doc->function(slider->controlledFunction());
                snapshot.role = ShowControlRole::AdjustSlider;
                snapshot.bound = function != nullptr;
                if (slider->controlledAttribute() == Function::Intensity)
                    snapshot.attribute = QStringLiteral("Intensity");
                else if (function != nullptr)
                    snapshot.attribute = Function::typeToString(function->type()) + QLatin1Char(':') +
                                         QString::number(slider->controlledAttribute());
            }
            break;
        }
    }
    return configuration;
}

ShowControlCoupling ShowCommandRecorder::couplingOf(VCWidget *control, quint32 functionId) const
{
    const auto soloGroupOf = [](VCWidget *widget)
    {
        for (VCWidget *parent = qobject_cast<VCWidget *>(widget->parent()); parent != nullptr;
             parent = qobject_cast<VCWidget *>(parent->parent()))
        {
            if (parent->type() == VCWidget::SoloFrameWidget)
                return parent->id();
            if (parent->type() != VCWidget::FrameWidget)
                break;
        }
        return ShowCommand::InvalidId;
    };

    ShowControlCoupling coupling;
    coupling.functionId = functionId;
    if (control != nullptr)
    {
        coupling.controlId = control->recordingId();
        coupling.soloGroup = soloGroupOf(control);
    }

    // what Collection::preRun starts, transitively through member Collections
    QList<quint32> pending{ coupling.functionId };
    while (!pending.isEmpty())
    {
        Collection *collection = qobject_cast<Collection *>(m_doc->function(pending.takeFirst()));
        if (collection == nullptr)
            continue;
        for (quint32 member : collection->functions())
        {
            if (member == coupling.functionId || coupling.startsFunctions.contains(member) ||
                m_doc->function(member) == nullptr)
                continue;
            coupling.startsFunctions.append(member);
            pending.append(member);
        }
    }

    // a started function, or a member it starts, stops the Solo Frame
    // siblings of its buttons once their running() lands
    const bool engineStart = control == nullptr;
    for (int page = 0; m_vc != nullptr && (engineStart || !coupling.startsFunctions.isEmpty()) &&
                       page < m_vc->pagesCount(); page++)
    {
        for (VCWidget *widget : m_vc->page(page)->children(true))
        {
            VCButton *button = qobject_cast<VCButton *>(widget);
            if (button == nullptr || (button->functionID() != functionId &&
                                      !coupling.startsFunctions.contains(button->functionID())))
                continue;
            const quint32 solo = soloGroupOf(button);
            if (solo != ShowCommand::InvalidId)
                coupling.memberSoloGroups.append(qMakePair(button->functionID(), solo));
        }
    }
    return coupling;
}
