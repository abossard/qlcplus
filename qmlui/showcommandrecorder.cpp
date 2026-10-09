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
#include "showcontrolaction.h"
#include <QJsonDocument>
#include <QJsonObject>

#include <QLocale>
#include <QScopedValueRollback>
#include <QColor>
#include <QStringList>
#include <algorithm>
#include <cmath>
#include <limits>

#include "doc.h"
#include "performfsm.h"
#include "show.h"
#include "showrunner.h"
#include "tardis.h"
#include "vcwidget.h"
#include "virtualconsole.h"



ShowCommandRecorder *ShowCommandRecorder::s_instance = nullptr;

using Phase = ShowEventLog::Phase;
using Outcome = ShowEventLog::Outcome;

static bool isHoldRole(ShowControlRole role)
{
    return role == ShowControlRole::FlashButton || role == ShowControlRole::FreezeHoldButton ||
           role == ShowControlRole::LevelSlider || role == ShowControlRole::AdjustSlider ||
           role == ShowControlRole::SubmasterSlider || role == ShowControlRole::GrandMasterSlider;
}

namespace
{
QString commandActionText(const ShowCommand &cmd);

QVariantMap nativeDraft(const ShowCommand &command)
{
    if (ShowCommand::hasTypedPayload(command.action))
        return QJsonDocument::fromJson(command.payload.encode(command.action).toUtf8()).object().toVariantMap();
    if (command.action == ShowCommandAction::SetXYPadPosition)
    {
        ShowCommandPanTilt point;
        ShowCommandPanTilt::decode(command.nativeArgument(), &point);
        return {{"pan", point.pan}, {"tilt", point.tilt}};
    }
    if (command.action == ShowCommandAction::SetXYPadFloor)
    {
        ShowCommandFloor point;
        ShowCommandFloor::decode(command.nativeArgument(), &point);
        return {{"x", point.x}, {"y", point.y}, {"z", point.z}};
    }
    if (command.action == ShowCommandAction::SetSliderColors)
    {
        ShowCommandColors colors;
        ShowCommandColors::decode(command.nativeArgument(), &colors);
        return {{"rgb", QVariantList{colors.red, colors.green, colors.blue}},
                {"wauv", QVariantList{colors.white, colors.amber, colors.ultraviolet}},
                {"brightness", command.position}};
    }
    return {};
}

QString applyNativeDraft(ShowCommand *command, const QVariantMap &draft)
{
    if (ShowCommand::hasTypedPayload(command->action))
    {
        QString reason;
        if (!ShowCommandPayload::decode(command->action,
            QString::fromUtf8(QJsonDocument::fromVariant(draft).toJson(QJsonDocument::Compact)),
            &command->payload, &reason))
            return reason;
    }
    else
    {
        const QVariantMap expected = nativeDraft(*command);
        if (expected.isEmpty() || expected.keys() != draft.keys())
            return QStringLiteral("typed draft has missing or unrelated arguments");
        const auto number = [&](const QString &key, qreal *value) {
            bool valid = false;
            *value = draft.value(key).toDouble(&valid);
            return valid && std::isfinite(*value);
        };
        if (command->action == ShowCommandAction::SetXYPadFloor)
        {
            ShowCommandFloor point;
            if (!number("x", &point.x) || !number("y", &point.y) || !number("z", &point.z))
                return QStringLiteral("floor coordinates must be finite metres");
            command->payload.value = point;
        }
        else if (command->action == ShowCommandAction::SetXYPadPosition)
        {
            ShowCommandPanTilt point;
            if (!number("pan", &point.pan) || !number("tilt", &point.tilt))
                return QStringLiteral("pan and tilt must be finite native coordinates");
            command->payload.value = point;
        }
        else if (command->action == ShowCommandAction::SetSliderColors)
        {
            const QVariantList rgb = draft.value("rgb").toList(), wauv = draft.value("wauv").toList();
            if (rgb.count() != 3 || wauv.count() != 3 || !number("brightness", &command->position))
                return QStringLiteral("color draft needs RGB, white/amber/UV and brightness");
            QStringList parts;
            for (const QVariant &entry : rgb + wauv)
            {
                bool valid = false;
                const qreal value = entry.toDouble(&valid);
                if (!valid || !std::isfinite(value) || value < 0 || value > 255 ||
                    value != std::floor(value))
                    return QStringLiteral("color components must be integers in 0..255");
                parts.append(QString::number(int(value)));
            }
            ShowCommandColors color;
            ShowCommandColors::decode(parts.mid(0, 3).join(',') + ';' + parts.mid(3).join(','), &color);
            command->payload.value = color;
        }
    }
    return ShowCommand::validate(*command);
}
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
    if (isAuthoring() && checkpoint(!on) == false)
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
        handed = publishCheckpoint(true, &reason);
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

bool ShowCommandRecorder::checkpoint(bool disarming)
{
    QString error;
    if (publishCheckpoint(disarming, &error))
        return true;

    reportFailure(error);
    return false;
}

bool ShowCommandRecorder::publishCheckpoint(bool disarming, QString *error)
{
    const ShowTraceCause operation = newCause();
    QScopedValueRollback<const ShowTraceCause *> scoped(m_operation,
                                                        operation.generation != 0 ? &operation : nullptr);
    const bool published = publishAccepted(disarming, error);
    if (published && disarming == false)
        armCheckpointSaveProjection();
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

bool ShowCommandRecorder::publishAccepted(bool disarming, QString *error)
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
        if (show == nullptr || publishUnpublished(show, disarming, error) == false)
            published = false;
    }
    return published;
}

void ShowCommandRecorder::discardUnpublished()
{
    m_unpublished.clear();
}

bool ShowCommandRecorder::publishUnpublished(Show *show, bool disarming, QString *error)
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
    if (disarming && show->id() == m_state.boundShowId)
    {
        QHash<QUuid, ShowCommand> openHolds;
        for (const ShowCommand &cmd : candidate.commands())
        {
            if (cmd.action != ShowCommandAction::SetButtonState || !isHoldRole(cmd.role))
                continue;
            if (cmd.on)
                openHolds.insert(cmd.controlId, cmd);
            else
                openHolds.remove(cmd.controlId);
        }

        const quint32 closeTime = qMin(acceptancePosition(show), ShowCommand::MaxTime);
        for (const ShowCommand &open : std::as_const(openHolds))
        {
            ShowCommand close = ShowCommand::setButtonState(candidate.nextEventId(), closeTime,
                                                            open.controlId, false);
            close.role = open.role;
            close.pairId = open.pairId;
            if (candidate.insert(close, error) == false)
                return false;
        }
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

ShowCommandTrack ShowCommandRecorder::projectedCheckpointTrack(Show *show, bool *projected) const
{
    ShowCommandTrack candidate = show->commandTrack();
    bool hasProjection = false;
    if (show->id() == m_state.boundShowId && m_state.recording)
    {
        QHash<QUuid, ShowCommand> openHolds;
        for (const ShowCommand &cmd : candidate.commands())
        {
            if (cmd.action != ShowCommandAction::SetButtonState || !isHoldRole(cmd.role))
                continue;
            if (cmd.on)
                openHolds.insert(cmd.controlId, cmd);
            else
                openHolds.remove(cmd.controlId);
        }

        const quint32 closeTime = qMin(acceptancePosition(show), ShowCommand::MaxTime);
        for (const ShowCommand &open : std::as_const(openHolds))
        {
            ShowCommand close = ShowCommand::setButtonState(candidate.nextEventId(), closeTime,
                                                            open.controlId, false);
            close.role = open.role;
            close.pairId = open.pairId;
            QString error;
            if (!candidate.insert(close, &error))
                continue;
            hasProjection = true;
        }
        if (hasProjection)
            candidate.setExtent(candidate.lastCommandTime());
    }
    if (projected != nullptr)
        *projected = hasProjection;
    return candidate;
}

void ShowCommandRecorder::armCheckpointSaveProjection()
{
    Show *show = trackedShow();
    if (show == nullptr)
        return;

    bool projected = false;
    const ShowCommandTrack candidate = projectedCheckpointTrack(show, &projected);
    if (projected)
        show->armCommandTrackSaveProjection(candidate);
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
    request.accepted = configurationOf(request.control, &request.input);
    request.input = ShowControlAction::acceptedInput(request);
    if (ShowCommand::hasTypedPayload(request.input.action))
    {
        const QString reason = ShowControlAction::preflight(request.control, request.input);
        if (!reason.isEmpty())
        {
            reportFailure(reason);
            return;
        }
    }
    if (ShowEventLog::generation() != 0)
        request.cause = newCause(request.input.origin, request.control);
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
    if (ShowCommandFsm::isUserOrigin(request.input.origin) == false)
    {
        if (diag)
            traceRequest(request, Phase::Decision, Outcome::Ignored, tr("input of this origin is not recorded"));
        return;
    }

    const ShowControlRequest *previous = nullptr;
    const ShowCommandAction action = request.input.action;
    for (int i = m_userRequests.count() - 1; i >= 0; --i)
        if (m_userRequests.at(i).control == request.control &&
            m_userRequests.at(i).input.action == action)
        {
            previous = &m_userRequests.at(i);
            break;
        }
    const auto plan = ShowControlAction::planCapture(request, ShowControlAction::observe(request.control), previous);
    if (!plan.supported)
    {
        if (diag)
            traceRequest(request, Phase::Decision, Outcome::Unsupported, tr("not a recordable control"));
        return;
    }
    if (plan.unchanged)
    {
        if (diag)
            traceRequest(request, Phase::Decision, Outcome::Ignored, tr("unchanged value"));
        return;
    }
    ShowCommandInput input = plan.input;

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
        const ShowControlConfiguration current = configurationOf(request.control, &request.input);
        if (current.snapshot.role != request.input.role || current != request.accepted)
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
        const FunctionParent requestOwner(FunctionParent::ManualVCWidget, request.control->id());
        const QVector<TimelineFact> closure = authored ? closureOf(request.control, requestOwner) : QVector<TimelineFact>();
        const ShowCommandInput &input = request.input;
        const auto receipt = ShowControlAction::apply(m_doc, request.control, input, requestOwner,
                                                      false, false, &request);
        if (!receipt.refusal.isEmpty())
        {
            reportFailure(receipt.refusal);
            traceRequest(request, Phase::Execute,
                         receipt.executionFailed ? Outcome::Failed : Outcome::Cancelled, receipt.refusal);
            continue;
        }
        if (request.authoredShowId == ShowCommand::InvalidId &&
            ShowCommandFsm::isUserOrigin(input.origin) && request.accepted.snapshot.enabled)
            for (TimelineFact &fact : m_timeline)
                if (fact.control == request.control && fact.owner.type() == FunctionParent::Function)
                    fact.superseded = true;
        if (input.action == ShowCommandAction::SetButtonState)
        {
            const quint32 widgetId = request.control->id();
            const quint32 intentShowId = request.authoredShowId != ShowCommand::InvalidId
                                           ? request.authoredShowId : trackedShowId();
            if (input.role == ShowControlRole::FreezeHoldButton)
            {
                if (input.on)
                    m_userFreezeHolds.insert(widgetId);
                else
                    m_userFreezeHolds.remove(widgetId);
            }
            else if (input.role == ShowControlRole::FlashButton)
            {
                if (input.on)
                    m_userFlashHolds.insert(widgetId);
                else
                    m_userFlashHolds.remove(widgetId);
            }
            else if (input.role == ShowControlRole::FreezeButton && intentShowId != ShowCommand::InvalidId)
            {
                m_userFreezeIntent.insert(intentShowId, input.on);
            }
            else if (input.role == ShowControlRole::BlackoutButton && intentShowId != ShowCommand::InvalidId)
            {
                m_userBlackoutIntent.insert(intentShowId, input.on);
            }
            else
            {
                if (input.on)
                    m_userSliderFlashHolds.insert(widgetId);
                else
                    m_userSliderFlashHolds.remove(widgetId);
            }
        }
        if (authored)
            journal(closure, request.authoredShowId, request.acceptedTimeMs,
                    receipt.buttonOp == ShowCommandFsm::ShowButtonOp::Start, requestOwner, receipt);
        if (request.cause.generation != 0)
            traceRequest(request, Phase::Execute, receipt.unchanged ? Outcome::Skipped
                         : receipt.requested || !receipt.functions.isEmpty() ||
                           receipt.buttonOp != ShowCommandFsm::ShowButtonOp::None
                             ? Outcome::Requested : Outcome::Applied,
                         receipt.unchanged ? tr("native value unchanged") : tr("native action applied"));
    }
}

bool ShowCommandRecorder::userRequestWaits(int index) const
{
    if (index >= 0 && index < m_userRequests.count())
    {
        const ShowControlRole role = m_userRequests.at(index).input.role;
        if (role == ShowControlRole::FreezeHoldButton || role == ShowControlRole::FreezeButton ||
            role == ShowControlRole::BlackoutButton)
            return false;
    }

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
    const ShowCommandInput &input = m_userRequests.at(index).input;
    const ShowControlCoupling coupling = couplingOf(control, ShowControlAction::functionId(control, &input));
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
        const ShowCommandInput &earlierInput = m_userRequests.at(i).input;
        if (earlier != nullptr && dependent(earlier, ShowControlAction::functionId(earlier, &earlierInput)))
            return true;
    }
    for (const ShowCommand &cmd : std::as_const(owed))
    {
        if (ShowCommand::isControlAction(cmd.action))
        {
            VCWidget *replayed = resolveControl(cmd);
            ShowCommandInput replayInput;
            replayInput.action = cmd.action;
            replayInput.payload = cmd.payload;
            if (replayed != nullptr && dependent(replayed, ShowControlAction::functionId(replayed, &replayInput)))
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
    m_replayFreezeHolds.clear();
    m_replayFlashButtons.clear();
    m_replaySliderFlashes.clear();
    m_userFreezeHolds.clear();
    m_userFlashHolds.clear();
    m_userSliderFlashHolds.clear();
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
    m_replayFreezeHolds.remove(id);
    m_replayFlashButtons.remove(id);
    m_replaySliderFlashes.remove(id);
    m_userFreezeIntent.remove(id);
    m_userBlackoutIntent.remove(id);
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

bool isPairedHoldCommand(const ShowCommand &cmd)
{
    return cmd.action == ShowCommandAction::SetButtonState && isHoldRole(cmd.role) &&
           cmd.pairId != ShowCommand::InvalidId;
}

QString validateWholeHoldSelection(const ShowCommandTrack &track, const QVector<quint32> &selected)
{
    if (selected.isEmpty())
        return QString();

    QHash<quint32, QVector<const ShowCommand *>> pairEdges;
    for (const ShowCommand &cmd : track.commands())
    {
        if (isPairedHoldCommand(cmd))
            pairEdges[cmd.pairId].append(&cmd);
    }

    const QSet<quint32> selectedIds(selected.cbegin(), selected.cend());
    for (quint32 id : selected)
    {
        const int index = track.indexOfId(id);
        if (index < 0)
            return ShowCommandRecorder::tr("Some selected events no longer exist");
        const ShowCommand &cmd = track.commands().at(index);
        if (!isPairedHoldCommand(cmd))
            continue;

        const QVector<const ShowCommand *> edges = pairEdges.value(cmd.pairId);
        if (edges.count() != 2 || edges.at(0)->controlId != edges.at(1)->controlId ||
            edges.at(0)->role != edges.at(1)->role || edges.at(0)->on == edges.at(1)->on)
        {
            return ShowCommandRecorder::tr("Hold pair %1 is malformed").arg(cmd.pairId);
        }
        if (!selectedIds.contains(edges.at(0)->id) || !selectedIds.contains(edges.at(1)->id))
            return ShowCommandRecorder::tr("Select both edges of each hold pair");
    }
    return QString();
}

QString validateClipboardHoldPairs(const QVector<ShowCommand> &copied)
{
    QHash<quint32, QVector<const ShowCommand *>> pairEdges;
    for (const ShowCommand &cmd : copied)
    {
        if (isPairedHoldCommand(cmd))
            pairEdges[cmd.pairId].append(&cmd);
    }
    for (auto it = pairEdges.cbegin(); it != pairEdges.cend(); ++it)
    {
        const QVector<const ShowCommand *> edges = it.value();
        if (edges.count() != 2 || edges.at(0)->controlId != edges.at(1)->controlId ||
            edges.at(0)->role != edges.at(1)->role || edges.at(0)->on == edges.at(1)->on)
        {
            return ShowCommandRecorder::tr("A copied hold pair is incomplete; copy again");
        }
    }
    return QString();
}

quint32 nextHoldPairId(const ShowCommandTrack &track)
{
    quint32 top = 0;
    for (const ShowCommand &cmd : track.commands())
    {
        if (cmd.pairId != ShowCommand::InvalidId)
            top = qMax(top, cmd.pairId);
    }
    if (top == std::numeric_limits<quint32>::max())
        return ShowCommand::InvalidId;
    return top + 1;
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
            case ShowCommandAction::SetSliderColors:
                entry["actionLabel"] = tr("Slider colors");
                value = tr("RGB/WAUV %1, brightness %2").arg(cmd.nativeArgument(), percentText(cmd.position));
                field = QStringLiteral("value");
            break;
            case ShowCommandAction::SetSliderReset:
                entry["actionLabel"] = tr("Slider reset");
                value = tr("Release override");
                field = QStringLiteral("state");
            break;
            case ShowCommandAction::SetXYPadPosition:
                entry["actionLabel"] = tr("XY position");
                value = tr("Pan/Tilt %1 (0..255)").arg(cmd.nativeArgument());
                field = QStringLiteral("value");
            break;
            case ShowCommandAction::SetXYPadFloor:
                entry["actionLabel"] = tr("Floor target (m)");
                value = tr("X/Y/Z %1 m").arg(cmd.nativeArgument());
                field = QStringLiteral("value");
            break;
            case ShowCommandAction::SetAnimationFader:
                entry["actionLabel"] = tr("Animation fader");
                value = percentText(cmd.position);
                field = QStringLiteral("value");
            break;
            case ShowCommandAction::SetXYPadRanges:
            case ShowCommandAction::SetXYPadPositionPreset:
            case ShowCommandAction::SetXYPadFunctionPreset:
            case ShowCommandAction::SetXYPadGroupPreset:
            case ShowCommandAction::SetAnimationColor:
            case ShowCommandAction::SetAnimationContent:
            case ShowCommandAction::SetSliderChannel:
                entry["actionLabel"] = commandActionText(cmd);
                value = cmd.payload.encode(cmd.action);
                field = QStringLiteral("arguments");
            break;
        }
        if (ShowCommand::isControlAction(cmd.action))
        {
            // the same resolution replay uses: what this record reaches now
            const QList<VCWidget *> widgets = m_vc != nullptr ? m_vc->widgetsByRecordingId(cmd.controlId)
                                                              : QList<VCWidget *>();
            QVector<ShowControlSnapshot> matches;
            for (VCWidget *widget : widgets)
                matches.append(configurationOf(widget).snapshot);
            QString reason;
            ShowControlStatus status = ShowCommandFsm::resolveControl(cmd, matches, &reason);
            if (status == ShowControlStatus::Ready)
            {
                reason = ShowControlAction::readiness(widgets.first(), cmd);
                if (!reason.isEmpty())
                    status = ShowControlStatus::Incompatible;
            }

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
        const bool coordinates = cmd.action == ShowCommandAction::SetXYPadPosition ||
                                 cmd.action == ShowCommandAction::SetXYPadFloor ||
                                 ShowCommand::hasTypedPayload(cmd.action);
        entry["valueText"] = field == QStringLiteral("state") || value.isEmpty() || coordinates
            ? value : value + QLatin1Char('%');
        entry["valueEdit"] = value;
        entry["valueField"] = field;
        entry["typed"] = !nativeDraft(cmd).isEmpty();
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
        entry["slider"] = group.action == ShowCommandAction::SetSliderPosition ||
                          group.action == ShowCommandAction::SetSliderColors;
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
            if ((cmd.action != ShowCommandAction::SetSliderPosition &&
                 cmd.action != ShowCommandAction::SetSliderColors) || !wanted.contains(cmd.id))
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
    const QString pairError = validateWholeHoldSelection(show->commandTrack(), {id});
    if (pairError.isEmpty() == false)
        return editFailed(pairError);

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
        else if (cmd.action == ShowCommandAction::SetSliderPosition ||
                 cmd.action == ShowCommandAction::SetSliderColors ||
                 cmd.action == ShowCommandAction::SetAnimationFader)
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
    const QString pairError = validateWholeHoldSelection(show->commandTrack(), affected);
    if (pairError.isEmpty() == false)
        return editFailed(pairError);
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
    const QString pairError = validateWholeHoldSelection(show->commandTrack(), ids);
    if (pairError.isEmpty() == false)
        return editFailed(pairError);

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
    const QVector<ShowCommand> commands = show->commandTrack().commands();
    for (const ShowCommand &cmd : commands)
    {
        if (unique.contains(cmd.id))
            basis.append(cmd);
    }
    if (basis.isEmpty() || basis.count() != unique.count())
        return editFailed(wanted.isEmpty() ? tr("Nothing selected") : tr("Some selected events no longer exist"));
    const QString pairError = validateWholeHoldSelection(show->commandTrack(), wanted);
    if (pairError.isEmpty() == false)
        return editFailed(pairError);

    m_session.show = show;
    m_session.ids = wanted;
    m_session.basis = basis;
    for (const ShowCommand &command : basis)
    {
        if (nativeDraft(command).isEmpty() || m_vc == nullptr)
            continue;
        const auto controls = m_vc->widgetsByRecordingId(command.controlId);
        if (controls.count() == 1 &&
            configurationOf(controls.first()).snapshot.role == command.role)
        {
            if (command.action == ShowCommandAction::SetXYPadFloor)
            {
                QVariantMap info = ShowControlAction::editorInfo(controls.first(), QString());
                info.remove("choices");
                m_session.floorMetadata.insert(command.id, info);
            }
            ShowCommandInput input;
            input.action = command.action;
            input.payload = command.payload;
            auto metadata = configurationOf(controls.first(), &input);
            // Editor compatibility compares copied binding values, not object continuity.
            metadata.functionLifetime.clear();
            metadata.nativeLifetimes.clear();
            m_session.nativeMetadata.insert(command.id, metadata);
        }
    }
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

QVariantMap ShowCommandRecorder::typedDraft(quint32 showId, quint32 id) const
{
    Show *show = trackedShow();
    if (show == nullptr || show->id() != showId)
        return {};
    const int index = show->commandTrack().indexOfId(id);
    return index < 0 ? QVariantMap() : nativeDraft(show->commandTrack().commands().at(index));
}

QVariantMap ShowCommandRecorder::typedEditorInfo(quint32 id, const QString &algorithm) const
{
    const int index = m_track.indexOfId(id);
    if (index < 0)
        return {};
    const auto &command = m_track.commands().at(index);
    const QList<VCWidget *> matches = m_vc != nullptr ? m_vc->widgetsByRecordingId(command.controlId)
                                                     : QList<VCWidget *>();
    return matches.count() == 1 ? ShowControlAction::editorInfo(matches.first(), algorithm) : QVariantMap();
}

QVariantMap ShowCommandRecorder::previewTypedEdit(const QVariantMap &drafts) const
{
    const QString conflict = sessionConflict();
    if (!conflict.isEmpty())
        return {{"reason", conflict}};
    if (drafts.count() != m_session.basis.count())
        return {{"reason", tr("A typed commit must name every frozen selected event")}};
    QVariantMap candidates;
    for (const ShowCommand &frozen : m_session.basis)
    {
        const QString key = QString::number(frozen.id);
        if (!drafts.contains(key) || drafts.value(key).metaType().id() != QMetaType::QVariantMap)
            return {{"reason", tr("A typed commit must name every frozen selected event")}};
        ShowCommand candidate = frozen;
        const QString reason = applyNativeDraft(&candidate, drafts.value(key).toMap());
        if (!reason.isEmpty())
            return {{"reason", reason}};
        if (!nativeDraft(candidate).isEmpty() && m_vc != nullptr)
        {
            const auto controls = m_vc->widgetsByRecordingId(candidate.controlId);
            if (m_session.floorMetadata.contains(candidate.id))
            {
                QVariantMap info = controls.count() == 1
                    ? ShowControlAction::editorInfo(controls.first(), QString()) : QVariantMap();
                info.remove("choices");
                if (info != m_session.floorMetadata.value(candidate.id))
                    return {{"reason", tr("Native floor bounds changed during this edit")}};
            }
            if (m_session.nativeMetadata.contains(candidate.id))
            {
                if (controls.count() != 1)
                    return {{"reason", tr("Native edit target is missing or ambiguous")}};
                ShowCommandInput input;
                input.action = frozen.action;
                input.payload = frozen.payload;
                auto metadata = configurationOf(controls.first(), &input);
                metadata.functionLifetime.clear();
                metadata.nativeLifetimes.clear();
                if (metadata != m_session.nativeMetadata.value(candidate.id))
                    return {{"reason", tr("Native binding or metadata changed during this edit")}};
            }
            if (controls.count() == 1 &&
                ShowControlAction::configuration(m_doc, controls.first()).snapshot.role == candidate.role)
            {
                const QString invalid = ShowControlAction::readiness(controls.first(), candidate);
                if (!invalid.isEmpty())
                    return {{"reason", invalid}};
            }
        }
        candidates.insert(key, nativeDraft(candidate));
    }
    return {{"arguments", candidates}};
}

bool ShowCommandRecorder::commitTypedEdit(const QVariantMap &drafts)
{
    const QString conflict = sessionConflict();
    if (!conflict.isEmpty())
    {
        endEditSession();
        return editFailed(conflict);
    }
    const QVariantMap preview = previewTypedEdit(drafts);
    if (preview.contains("reason"))
        return editFailed(preview.value("reason").toString());
    Show *show = editableShow(m_session.show->id());
    if (show == nullptr)
        return false;
    ShowCommandTrack candidate = show->commandTrack();
    QString reason;
    for (const ShowCommand &frozen : m_session.basis)
    {
        ShowCommand command = frozen;
        reason = applyNativeDraft(&command, drafts.value(QString::number(frozen.id)).toMap());
        if (!reason.isEmpty() || !candidate.replace(command, &reason))
            return editFailed(reason);
    }
    const QVector<quint32> ids = m_session.ids;
    const bool published = publishEdit(show, candidate, ids);
    if (published)
        endEditSession();
    return published;
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
            next == ShowCommandAction::SetSliderPosition || next == ShowCommandAction::SetSliderColors ||
            next == ShowCommandAction::SetSliderReset || next == ShowCommandAction::SetXYPadPosition ||
            next == ShowCommandAction::SetAnimationFader ||
            cmd.controlId.isNull() == false)
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
    const QVector<ShowCommand> commands = show->commandTrack().commands();
    for (const ShowCommand &cmd : commands)
    {
        if (wanted.contains(cmd.id))
            copied.append(cmd);
    }
    if (copied.isEmpty() || copied.count() != QSet<quint32>(wanted.begin(), wanted.end()).count())
    {
        setError(wanted.isEmpty() ? tr("Nothing selected") : tr("Some selected events no longer exist"));
        return false;
    }
    const QString pairError = validateWholeHoldSelection(show->commandTrack(), wanted);
    if (pairError.isEmpty() == false)
    {
        setError(pairError);
        return false;
    }
    const QString clipboardError = validateClipboardHoldPairs(copied);
    if (clipboardError.isEmpty() == false)
    {
        setError(clipboardError);
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
    const QString clipboardError = validateClipboardHoldPairs(m_clipboard);
    if (clipboardError.isEmpty() == false)
    {
        editFailed(clipboardError);
        return QVariantList();
    }

    // above every id the Show, its input not taken yet and its history hold
    ShowCommandTrack candidate = show->commandTrack();
    candidate.reserve(withUnpublished(show).nextEventId(), 0);
    const qint64 delta = qRound64(atMs) - qint64(m_clipboard.first().time);
    quint32 nextPair = nextHoldPairId(candidate);
    QHash<quint32, quint32> pairRemap;
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
        if (isPairedHoldCommand(copy))
        {
            if (!pairRemap.contains(copy.pairId))
            {
                if (nextPair == ShowCommand::InvalidId)
                {
                    editFailed(tr("This Show has no hold pair id left"));
                    return QVariantList();
                }
                pairRemap.insert(copy.pairId, nextPair);
                nextPair = (nextPair == std::numeric_limits<quint32>::max() - 1)
                    ? ShowCommand::InvalidId
                    : nextPair + 1;
            }
            copy.pairId = pairRemap.value(copy.pairId);
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
    if (publishUnpublished(show, false, &error))
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
    reconcileReplayOwnedHoldsAtCursor(show);

    if (Tardis *tardis = Tardis::instance())
        tardis->enqueueAction(Tardis::ShowManagerCommandEdit, edit.showId, QVariant(), QVariant::fromValue(edit));
    if (ShowEventLog::generation() != 0)
        traceTrackDelta(newCause(), current, show->commandTrack(), edit.showId, tr("editor"));
    m_doc->setModified();
    setError(QString());
    return true;
}

void ShowCommandRecorder::reconcileReplayOwnedHoldsAtCursor(Show *show)
{
    if (show == nullptr || m_vc == nullptr)
        return;

    const quint32 showId = show->id();
    const FunctionParent replayOwner(FunctionParent::Function, showId);
    const quint32 cursor = show->elapsed();
    const QVector<ShowCommand> commands = show->commandTrack().commands();
    const auto holdStateAt = [&commands, cursor](const QUuid &controlId,
                                                 const std::function<bool(ShowControlRole)> &acceptRole) {
        bool seen = false;
        bool on = false;
        for (const ShowCommand &cmd : commands)
        {
            if (cmd.action != ShowCommandAction::SetButtonState || cmd.controlId != controlId || cmd.time > cursor ||
                !acceptRole(cmd.role))
            {
                continue;
            }
            seen = true;
            on = cmd.on;
        }
        return seen && on;
    };
    const ShowControlAction::Holds user{m_userFreezeHolds, m_userFlashHolds, m_userSliderFlashHolds};

    const auto isFreezeHoldRole = [](ShowControlRole role) { return role == ShowControlRole::FreezeHoldButton; };
    QSet<QUuid> replayFreeze = m_replayFreezeHolds.value(showId);
    for (const QUuid &controlId : replayFreeze)
    {
        if (holdStateAt(controlId, isFreezeHoldRole))
            continue;
        const QList<VCWidget *> widgets = m_vc->widgetsByRecordingId(controlId);
        for (VCWidget *widget : widgets)
            ShowControlAction::releaseHold(m_vc, widget, ShowControlRole::FreezeHoldButton, replayOwner, user, false);
        m_replayFreezeHolds[showId].remove(controlId);
    }

    const auto isFlashRole = [](ShowControlRole role) { return role == ShowControlRole::FlashButton; };
    QSet<QUuid> replayFlash = m_replayFlashButtons.value(showId);
    for (const QUuid &controlId : replayFlash)
    {
        if (holdStateAt(controlId, isFlashRole))
            continue;
        const QList<VCWidget *> widgets = m_vc->widgetsByRecordingId(controlId);
        for (VCWidget *widget : widgets)
            ShowControlAction::releaseHold(m_vc, widget, ShowControlRole::FlashButton, replayOwner, user, false);
        m_replayFlashButtons[showId].remove(controlId);
    }

    const auto isSliderRole = [](ShowControlRole role) {
        return role == ShowControlRole::LevelSlider || role == ShowControlRole::AdjustSlider ||
               role == ShowControlRole::SubmasterSlider || role == ShowControlRole::GrandMasterSlider;
    };
    QSet<QUuid> replaySliderFlash = m_replaySliderFlashes.value(showId);
    for (const QUuid &controlId : replaySliderFlash)
    {
        if (holdStateAt(controlId, isSliderRole))
            continue;
        const QList<VCWidget *> widgets = m_vc->widgetsByRecordingId(controlId);
        for (VCWidget *widget : widgets)
            ShowControlAction::releaseHold(m_vc, widget, ShowControlRole::AdjustSlider, replayOwner, user, false);
        m_replaySliderFlashes[showId].remove(controlId);
    }
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
        case ShowCommandAction::SetSliderColors: return ShowCommandRecorder::tr("Slider colors");
        case ShowCommandAction::SetSliderReset: return ShowCommandRecorder::tr("Slider reset");
        case ShowCommandAction::SetXYPadPosition: return ShowCommandRecorder::tr("XY position");
        case ShowCommandAction::SetXYPadFloor: return ShowCommandRecorder::tr("Floor target (m)");
        case ShowCommandAction::SetAnimationFader: return ShowCommandRecorder::tr("Animation fader");
        case ShowCommandAction::SetXYPadRanges: return ShowCommandRecorder::tr("XY range endpoints");
        case ShowCommandAction::SetXYPadPositionPreset: return ShowCommandRecorder::tr("XY position choice");
        case ShowCommandAction::SetXYPadFunctionPreset: return ShowCommandRecorder::tr("XY function choice");
        case ShowCommandAction::SetXYPadGroupPreset: return ShowCommandRecorder::tr("XY group choice");
        case ShowCommandAction::SetAnimationColor: return ShowCommandRecorder::tr("Matrix color");
        case ShowCommandAction::SetAnimationContent: return ShowCommandRecorder::tr("Matrix content");
        case ShowCommandAction::SetSliderChannel: return ShowCommandRecorder::tr("Channel value");
    }
    return QString();
}

QString commandValueText(const ShowCommand &cmd)
{
    switch (cmd.action)
    {
        case ShowCommandAction::SetIntensity: return QString::number(cmd.intensity * 100.0, 'f', 1) + QLatin1Char('%');
        case ShowCommandAction::SetSliderPosition: return QString::number(cmd.position * 100.0, 'f', 1) + QLatin1Char('%');
        case ShowCommandAction::SetSliderColors: return QString::number(cmd.position * 100.0, 'f', 1) + QLatin1Char('%');
        case ShowCommandAction::SetButtonState: return cmd.on ? ShowCommandRecorder::tr("On") : ShowCommandRecorder::tr("Off");
        case ShowCommandAction::SetSliderReset: return ShowCommandRecorder::tr("Release override");
        case ShowCommandAction::SetXYPadPosition: return cmd.nativeArgument();
        case ShowCommandAction::SetAnimationFader: return QString::number(cmd.position * 100.0, 'f', 1) + QLatin1Char('%');
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
    if (request.input.action == ShowCommandAction::SetButtonState)
        return request.input.on ? ShowCommandRecorder::tr("On") : ShowCommandRecorder::tr("Off");
    if (request.input.action == ShowCommandAction::SetSliderColors)
        return QString::number(request.rawValue * 100.0 / 255.0, 'f', 1) + QLatin1Char('%');
    if (request.input.action == ShowCommandAction::SetSliderReset)
        return ShowCommandRecorder::tr("Release override");
    return QString::number(request.rawValue);
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
    e.action = request.input.action == ShowCommandAction::SetButtonState ? tr("Button state")
             : request.input.action == ShowCommandAction::SetSliderColors ? tr("Slider colors")
             : request.input.action == ShowCommandAction::SetSliderReset ? tr("Slider reset")
                                   : tr("Slider value");
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
    connect(show, qOverload<quint32>(&Function::stopped), this, &ShowCommandRecorder::slotShowStopped);
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
    m_userFreezeIntent.remove(showId);
    m_userBlackoutIntent.remove(showId);
    m_timeline.erase(std::remove_if(m_timeline.begin(), m_timeline.end(),
                                    [showId](const TimelineFact &fact) { return fact.showId == showId; }),
                     m_timeline.end());
}

void ShowCommandRecorder::slotShowPlayback()
{
    if (auto *show = qobject_cast<Show *>(sender()); show != nullptr && show->isRunning())
    {
        const quint32 showId = show->id();
        m_timeline.erase(std::remove_if(m_timeline.begin(), m_timeline.end(),
                                       [showId](const TimelineFact &fact) { return fact.showId == showId; }),
                         m_timeline.end());
    }
    emit editGateChanged();
}

void ShowCommandRecorder::slotShowStopped(quint32)
{
    Show *show = qobject_cast<Show *>(sender());
    if (show == nullptr)
        return;
    drainUserRequests();

    ShowControlAction::Stop state;
    state.owner = FunctionParent(FunctionParent::Function, show->id());
    state.freeze = m_replayFreezeHolds.value(show->id());
    state.flash = m_replayFlashButtons.value(show->id());
    state.sliderFlash = m_replaySliderFlashes.value(show->id());
    state.user = {m_userFreezeHolds, m_userFlashHolds, m_userSliderFlashHolds};
    if (m_userFreezeIntent.contains(show->id()))
        state.freezeIntent = m_userFreezeIntent.value(show->id());
    if (m_userBlackoutIntent.contains(show->id()))
        state.blackoutIntent = m_userBlackoutIntent.value(show->id());
    for (const TimelineFact &fact : std::as_const(m_timeline))
    {
        if (fact.showId != show->id() || !(fact.owner == state.owner))
            continue;
        ShowControlAction::Restoration effect;
        effect.control = fact.control;
        effect.owner = fact.owner;
        effect.after = fact.nativeAfter;
        effect.releaseOwner = !fact.ownerBefore;
        state.effects.append(effect);
    }
    const auto retained = ShowControlAction::stop(m_doc, m_vc, state);
    m_userFreezeHolds = retained.freeze;
    m_userFlashHolds = retained.flash;
    m_userSliderFlashHolds = retained.sliderFlash;
    m_replayFreezeHolds.remove(show->id());
    m_replayFlashButtons.remove(show->id());
    m_replaySliderFlashes.remove(show->id());
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
        case ShowControlRole::FlashButton:
        case ShowControlRole::AdjustSlider:
            parts.append(bound != nullptr ? bound->name() : tr("no function"));
            if (current.snapshot.role == ShowControlRole::AdjustSlider)
                parts.append(current.snapshot.attribute);
        break;
        case ShowControlRole::BlackoutButton: parts.append(tr("Blackout")); break;
        case ShowControlRole::FreezeButton: parts.append(tr("Freeze")); break;
        case ShowControlRole::FreezeHoldButton: parts.append(tr("Freeze hold")); break;
        case ShowControlRole::LevelSlider:
            parts.append(tr("%n channel(s)", "", current.levelChannels.count()));
        break;
        case ShowControlRole::SubmasterSlider: parts.append(tr("Submaster")); break;
        case ShowControlRole::GrandMasterSlider: parts.append(tr("Grand Master")); break;
        case ShowControlRole::XYPad: parts.append(tr("XY Pad")); break;
        case ShowControlRole::AnimationFader:
            parts.append(bound != nullptr ? bound->name() : tr("no function"));
            parts.append(tr("Animation fader"));
        break;
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
        case ShowControlRole::FlashButton: return ShowCommandRecorder::tr("Flash button");
        case ShowControlRole::BlackoutButton: return ShowCommandRecorder::tr("Blackout button");
        case ShowControlRole::FreezeButton: return ShowCommandRecorder::tr("Freeze button");
        case ShowControlRole::FreezeHoldButton: return ShowCommandRecorder::tr("Freeze hold button");
        case ShowControlRole::LevelSlider: return ShowCommandRecorder::tr("Level slider");
        case ShowControlRole::AdjustSlider: return ShowCommandRecorder::tr("Adjust slider");
        case ShowControlRole::SubmasterSlider: return ShowCommandRecorder::tr("Submaster slider");
        case ShowControlRole::GrandMasterSlider: return ShowCommandRecorder::tr("Grand Master slider");
        case ShowControlRole::XYPad: return ShowCommandRecorder::tr("XY pad");
        case ShowControlRole::AnimationFader: return ShowCommandRecorder::tr("Animation fader");
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
        ShowControlStatus status = ShowCommandFsm::resolveControl(expected, matches, &reason);
        if (status == ShowControlStatus::Ready)
        {
            for (const ShowCommand &command : m_track.commands())
            {
                if (command.controlId != ref.controlId || command.role != ref.role ||
                    command.attribute != ref.attribute)
                    continue;
                QString refusal;
                ShowControlStatus candidate = ShowCommandFsm::resolveControl(command, matches, &refusal);
                if (candidate == ShowControlStatus::Ready)
                {
                    refusal = ShowControlAction::readiness(it->widgets.first(), command);
                    if (!refusal.isEmpty())
                        candidate = ShowControlStatus::Incompatible;
                }
                if (statusRank(candidate) < statusRank(status))
                {
                    status = candidate;
                    reason = refusal;
                }
            }
        }

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
            m_rowSources += ShowControlAction::observeConfiguration(widget, this, refresh);
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

void ShowCommandRecorder::slotRecordedWriteRetired(quint64 generation, int outcome, quint32 functionId, int effect,
                                                  const QString &reason)
{
    VCWidget *control = qobject_cast<VCWidget *>(sender());
    if (control == nullptr)
        return;

    for (int i = 0; i < m_controlRuns.count(); i++)
    {
        ControlRun &run = m_controlRuns[i];
        if (run.waitingWrites.removeAll(qMakePair(control->id(), generation)) == 0)
            continue;

        if (!reason.isEmpty())
        {
            if (const quint64 observation = ShowEventLog::generation())
            {
                ShowEventLog::Entry entry = traceEntry(observation, Phase::Execute, Outcome::Cancelled, reason);
                entry.origin = int(ShowCommandOrigin::Replay);
                entry.showId = run.show.isNull() ? Function::invalidId() : run.show->id();
                entry.traversal = run.batch.traversal;
                entry.control = control->caption();
                entry.widgetId = control->id();
                entry.value = tr("Native generation %1 retired with outcome %2").arg(generation).arg(outcome);
                ShowEventLog::append(entry);
            }
            ShowEventLog::failure(reason, ShowEventLog::View::References);
        }
        if (const auto native = ShowControlAction::writerExpectation(functionId, effect))
            run.barrier.append(*native);
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
    if (m_vc != nullptr)
    {
        for (const ControlRun &run : std::as_const(m_controlRuns))
        {
            if (!run.show.isNull() && run.show->commandTraversal() == run.batch.traversal)
                continue;
            for (const auto &pending : run.waitingWrites)
                ShowControlAction::cancelWriter(m_vc->widget(pending.first), pending.second);
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
        const auto resolution = ShowControlAction::resolve(m_doc, m_vc, cmd);
        VCWidget *control = resolution.control;
        ShowCommandInput input;
        input.action = cmd.action;
        input.role = cmd.role;
        input.attribute = cmd.attribute;
        input.on = cmd.on;
        input.position = cmd.position;
        input.payload = cmd.payload;
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
        if (run.claimedCommand == cmd.id &&
            (run.claimedControl != control || control == nullptr ||
             run.claimedConfiguration != configurationOf(control, &input)))
        {
            const QString why = tr("Replay cancelled %1: required native binding changed while waiting")
                                    .arg(cmd.controlId.toString(QUuid::WithoutBraces));
            trace(Outcome::Cancelled, why);
            ShowEventLog::failure(why, ShowEventLog::View::References);
            run.next++;
            continue;
        }
        if (control == nullptr)
        {
            const QString why = tr("Replay skipped %1: %2").arg(cmd.controlId.toString(QUuid::WithoutBraces),
                                                               resolution.reason);
            trace(Outcome::Skipped, why);
            ShowEventLog::failure(why, ShowEventLog::View::References);
            run.next++;
            continue;
        }
        if (run.claimedCommand != cmd.id)
        {
            run.claimedCommand = cmd.id;
            run.claimedControl = control;
            run.claimedConfiguration = configurationOf(control, &input);
        }

        const ShowControlCoupling coupling = couplingOf(control, ShowControlAction::functionId(control, &input));
        QVector<ShowFunctionExpectation> expectations;
        for (int i = run.unsettled.count() - 1; i >= 0; i--)
        {
            const auto plan = ShowControlAction::planReceipt(run.unsettled.at(i).first,
                                                             run.unsettled.at(i).second, &coupling);
            if (!plan.depends)
                continue;
            expectations += plan.functions;
            run.unsettled.removeAt(i);
        }
        for (int i = run.engineDone.count() - 1; i >= 0; i--)
        {
            const auto plan = ShowControlAction::planReceipt(run.engineDone.at(i).first,
                                                             run.engineDone.at(i).second, &coupling);
            if (!plan.depends)
                continue;
            expectations += plan.functions;
            run.engineDone.removeAt(i);
        }
        if (!expectations.isEmpty())
        {
            wait(expectations);
            return false;
        }

        run.next++;
        const FunctionParent replayOwner(FunctionParent::Function, run.show->id());
        const QVector<TimelineFact> closure = closureOf(control, replayOwner);
        const auto receipt = ShowControlAction::apply(m_doc, control, input, replayOwner, true);
        if (!receipt.refusal.isEmpty())
        {
            trace(receipt.executionFailed ? Outcome::Failed : Outcome::Skipped, receipt.refusal);
            ShowEventLog::failure(receipt.refusal, ShowEventLog::View::References);
            if (!receipt.functions.isEmpty())
            {
                wait(receipt.functions);
                return false;
            }
            continue;
        }
        if (cmd.action == ShowCommandAction::SetButtonState)
        {
            if (cmd.role == ShowControlRole::FreezeButton)
                m_userFreezeIntent.remove(run.show->id());
            else if (cmd.role == ShowControlRole::BlackoutButton)
                m_userBlackoutIntent.remove(run.show->id());
            if (cmd.role == ShowControlRole::FreezeHoldButton)
            {
                QSet<QUuid> &holds = m_replayFreezeHolds[run.show->id()];
                if (cmd.on)
                    holds.insert(cmd.controlId);
                else
                    holds.remove(cmd.controlId);
            }
            else if (cmd.role == ShowControlRole::FlashButton)
            {
                QSet<QUuid> &holds = m_replayFlashButtons[run.show->id()];
                if (cmd.on)
                    holds.insert(cmd.controlId);
                else
                    holds.remove(cmd.controlId);
            }
            else if (cmd.role == ShowControlRole::LevelSlider || cmd.role == ShowControlRole::AdjustSlider ||
                     cmd.role == ShowControlRole::SubmasterSlider || cmd.role == ShowControlRole::GrandMasterSlider)
            {
                QSet<QUuid> &holds = m_replaySliderFlashes[run.show->id()];
                if (cmd.on)
                    holds.insert(cmd.controlId);
                else
                    holds.remove(cmd.controlId);
            }
        }
        journal(closure, run.show->id(), cmd.time, receipt.buttonOp == ShowButtonOp::Start, replayOwner, receipt);
        if (receipt.buttonOp != ShowButtonOp::None)
            run.unsettled.append(qMakePair(coupling, receipt.buttonOp));
        trace(receipt.requested || !receipt.writes.isEmpty() || !receipt.functions.isEmpty() ||
              receipt.buttonOp != ShowButtonOp::None
              ? Outcome::Requested : Outcome::Applied, tr("native action applied"));
        for (const auto &write : receipt.writes)
            awaitRecordedWrite(run, write.first, write.second);
        run.barrier += receipt.functions;
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
            expectations += ShowControlAction::planReceipt(done.first, done.second).functions;
        run.unsettled.clear();
        wait(expectations);
        return false;
    }

    run.show->acknowledgeControlBatch(run.batch.traversal, run.batch.seq);
    return true;
}

void ShowCommandRecorder::awaitRecordedWrite(ControlRun &run, VCWidget *control, quint64 generation)
{
    // ponytail: every later op of the batch waits for this write (one tick),
    // wait per dependency instead if a batch ever needs the overlap
    if (control == nullptr)
        return;
    if (generation == 0)
        return;
    run.waitingWrites.append(qMakePair(control->id(), generation));
    connect(control, &QObject::destroyed, this, &ShowCommandRecorder::slotObservedControlDestroyed,
            Qt::UniqueConnection);
    ShowControlAction::connectWriter(control, this);
}

void ShowCommandRecorder::slotObservedControlDestroyed(QObject *object)
{
    VCWidget *control = qobject_cast<VCWidget *>(object);
    if (control == nullptr)
        return;

    for (int i = 0; i < m_controlRuns.count();)
    {
        ControlRun &run = m_controlRuns[i];
        const int removed = run.waitingWrites.removeIf(
                [control](const QPair<quint32, quint64> &waited) { return waited.first == control->id(); });
        if (removed == 0)
        {
            i++;
            continue;
        }

        if (!run.waitingWrites.isEmpty())
        {
            i++;
            continue;
        }

        if (run.show.isNull())
        {
            m_controlRuns.removeAt(i);
        }
        else if (!run.barrier.isEmpty())
        {
            run.waitingOp = ++m_lastReceiptOp;
            run.show->requestFunctionReceipt(run.batch.traversal, run.waitingOp,
                                             std::exchange(run.barrier, QVector<ShowFunctionExpectation>()));
            i++;
        }
        else if (continueControlRun(run))
        {
            m_controlRuns.removeAt(i);
        }
        else
        {
            i++;
        }
    }
    drainUserRequests();
}

QVector<ShowCommandRecorder::TimelineFact> ShowCommandRecorder::closureOf(VCWidget *control, const FunctionParent &owner) const
{
    QVector<TimelineFact> closure;
    for (const auto &native : ShowControlAction::closure(m_doc, m_vc, control, owner))
    {
        TimelineFact fact;
        fact.showId = ShowCommand::InvalidId;
        fact.time = 0;
        fact.control = native.control;
        fact.nativeBefore = native.before;
        fact.nativeAfter = native.after;
        fact.owner = owner;
        fact.ownerBefore = native.ownerBefore;
        fact.conditionalOnStart = native.conditionalOnStart;
        closure.append(fact);
    }
    return closure;
}

void ShowCommandRecorder::journal(QVector<TimelineFact> closure, quint32 showId, quint32 time, bool started,
                                  const FunctionParent &owner, const ShowControlAction::Receipt &receipt)
{
    for (int i = 0; i < closure.count(); i++)
    {
        TimelineFact &fact = closure[i];
        if (fact.control.isNull())
            continue;
        fact.showId = showId;
        fact.time = time;
        if (receipt.atomicNativeState && fact.control == receipt.control && !fact.conditionalOnStart)
        {
            fact.nativeBefore = receipt.before;
            fact.nativeAfter = receipt.after;
        }
        else if (!fact.conditionalOnStart)
            fact.nativeAfter = ShowControlAction::observe(fact.control);
        else if (!started)
            fact.nativeAfter = fact.nativeBefore;
        fact.owner = owner;
        m_timeline.append(fact);
    }
}

void ShowCommandRecorder::rollBackTimeline(ControlRun &run)
{
    const quint32 showId = run.show->id();
    const quint32 to = *run.batch.rollbackTo;

    QVector<ShowControlAction::Restoration> targets;
    QHash<VCWidget *, ShowControlAction::State> latestNative;
    for (const TimelineFact &fact : std::as_const(m_timeline))
    {
        if (fact.showId != showId || fact.control.isNull())
            continue;
        latestNative.insert(fact.control, fact.nativeAfter);
        const bool known = std::any_of(targets.cbegin(), targets.cend(),
                                       [&fact](const ShowControlAction::Restoration &target)
                                       { return target.control == fact.control; });
        if (fact.time > to && known == false)
        {
            ShowControlAction::Restoration target;
            target.control = fact.control;
            target.owner = fact.owner;
            target.releaseOwner = !fact.ownerBefore;
            target.before = fact.nativeBefore;
            target.superseded = fact.superseded;
            targets.append(target);
        }
    }
    for (auto &target : targets)
        target.after = latestNative.value(target.control);
    m_timeline.erase(std::remove_if(m_timeline.begin(), m_timeline.end(),
                                    [showId, to](const TimelineFact &fact)
                                    { return fact.showId == showId && (fact.time > to || fact.control.isNull()); }),
                     m_timeline.end());

    const auto receipts = ShowControlAction::restoreBatch(m_doc, m_vc, targets, latestNative,
                                                         m_userFlashHolds, m_userSliderFlashHolds);
    for (const auto &receipt : receipts)
    {
        if (!receipt.refusal.isEmpty())
            ShowEventLog::failure(receipt.refusal, ShowEventLog::View::References);
        awaitRecordedWrite(run, receipt.control, receipt.writerGeneration);
        run.barrier += receipt.functions;
        if (receipt.buttonOp != ShowCommandFsm::ShowButtonOp::None)
            run.unsettled.append(qMakePair(couplingOf(receipt.control, ShowControlAction::functionId(receipt.control)),
                                          receipt.buttonOp));
    }
}

VCWidget *ShowCommandRecorder::resolveControl(const ShowCommand &cmd) const
{
    return ShowControlAction::resolve(m_doc, m_vc, cmd).control;
}

ShowControlConfiguration ShowCommandRecorder::configurationOf(VCWidget *widget, const ShowCommandInput *input) const
{
    return ShowControlAction::configuration(m_doc, widget, input);
}

ShowControlCoupling ShowCommandRecorder::couplingOf(VCWidget *control, quint32 functionId) const
{
    return ShowControlAction::coupling(m_doc, m_vc, control, functionId);
}
