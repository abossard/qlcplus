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

#include <QScopedValueRollback>

#include "doc.h"
#include "show.h"



ShowCommandRecorder *ShowCommandRecorder::s_instance = nullptr;

ShowCommandRecorder::ShowCommandRecorder(Doc *doc, QObject *parent)
    : QObject(parent)
    , m_doc(doc)
{
    s_instance = this;
}

ShowCommandRecorder::~ShowCommandRecorder()
{
    if (s_instance == this)
        s_instance = nullptr;
}

ShowCommandRecorder *ShowCommandRecorder::instance()
{
    return s_instance;
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

    if (isAuthoring() && finalizeTake() == false)
        return false;

    Show *previous = trackedShow();
    m_state = next;
    m_takeId++;
    reloadTrack();

    Show *bound = trackedShow();
    if (on && bound != nullptr && isAuthoring())
    {
        m_authoredExtentFloor = m_track.extent();
        m_clockPosition = bound->commandPosition();
        m_state.position = m_clockPosition;
    }
    if (previous != nullptr && previous != bound)
        previous->setCommandRecording(false);
    if (bound != nullptr)
        bound->setCommandRecording(m_state.phase() == ShowRecordPhase::Bound);

    emit stateChanged();
    return true;
}

bool ShowCommandRecorder::setResolvedShow(quint32 showId)
{
    const ShowCommandState next = ShowCommandFsm::setResolvedShow(m_state, showId);
    if (next == m_state)
        return true;

    if (isAuthoring() && next.phase() != ShowRecordPhase::Bound && finalizeTake() == false)
        return false;

    m_state = next;
    m_takeId++;
    reloadTrack();

    Show *bound = trackedShow();
    if (bound != nullptr)
        bound->setCommandRecording(m_state.phase() == ShowRecordPhase::Bound);

    emit stateChanged();
    return true;
}

bool ShowCommandRecorder::isAuthoring() const
{
    return m_state.phase() == ShowRecordPhase::Bound;
}

void ShowCommandRecorder::setPlaying(bool playing)
{
    const ShowCommandState next = ShowCommandFsm::setPlaying(m_state, playing);
    if (next == m_state)
        return;

    m_state = next;
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

quint32 ShowCommandRecorder::extentFloor() const
{
    return qMax(m_authoredExtentFloor, m_track.lastCommandTime());
}

bool ShowCommandRecorder::finalizeTake()
{
    Show *show = trackedShow();
    if (show == nullptr)
        return true;

    // Controls holding an accepted input the engine has not executed yet write
    // it into this take now, while it is still the current one.
    emit takeAboutToFinalize(m_takeId);

    // The Show keeps whatever it already played to: a short take never shortens
    // a longer authored duration, and no fixed tail is invented either.
    ShowCommandTrack finalized = m_track;
    const quint32 captureEnd = qMax(extentFloor(), show->commandPosition());
    if (finalized.extent() == captureEnd)
        return true;

    QString error;
    if (finalized.setExtent(captureEnd, &error) == false)
    {
        setError(error);
        return false;
    }

    return publishTrack(finalized, QSet<quint32>(), show);
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

void ShowCommandRecorder::slotResolvedUserCommand(int action, quint32 functionId,
                                                  qreal intensity, quint32 timeMs,
                                                  int origin, quint64 takeId)
{
    // A gesture accepted during an earlier take never lands in a later one.
    if (takeId != m_takeId)
        return;

    ShowCommandInput input;
    input.origin = ShowCommandOrigin(origin);
    input.action = ShowCommandAction(action);
    input.functionId = functionId;
    input.intensity = intensity;

    authorAt(QVector<ShowCommandInput>{input}, timeMs);
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
        const ShowCommandTransition transition = ShowCommandFsm::userInput(m_track, m_state, input);
        setError(transition.error);
        if (transition.authored.isEmpty())
            continue;

        if (commit(transition, show))
            authored = true;
    }

    return authored;
}

void ShowCommandRecorder::reportUnsupported(const QString &what)
{
    if (isAuthoring() == false)
        return;

    setError(tr("Not recorded: %1").arg(what));
}

/*********************************************************************
 * Authored data
 *********************************************************************/

const ShowCommandTrack &ShowCommandRecorder::track() const
{
    return m_track;
}

QVariantList ShowCommandRecorder::commands() const
{
    QVariantList list;
    for (const ShowCommand &cmd : m_track.commands())
    {
        Function *target = m_doc != nullptr ? m_doc->function(cmd.functionId) : nullptr;
        QVariantMap entry;
        entry["id"] = cmd.id;
        entry["time"] = cmd.time;
        entry["action"] = ShowCommand::actionToString(cmd.action);
        entry["functionId"] = cmd.functionId;
        entry["functionName"] = target != nullptr ? target->name() : tr("Missing function");
        entry["intensity"] = cmd.intensity;
        entry["isValue"] = cmd.action == ShowCommandAction::SetIntensity;
        list.append(entry);
    }
    return list;
}

int ShowCommandRecorder::extent() const
{
    return int(m_track.extent());
}

bool ShowCommandRecorder::setExtent(int ms)
{
    if (ms < 0)
        return editFailed(tr("%1 is not a valid time").arg(ms));

    ShowCommandTrack edited = m_track;
    QString error;
    if (edited.setExtent(quint32(ms), &error) == false)
        return editFailed(error);

    return publishEdit(edited);
}

bool ShowCommandRecorder::setCommandAction(quint32 id, const QString &action)
{
    const int index = m_track.indexOfId(id);
    if (index < 0)
        return editFailed(tr("no command carries id %1").arg(id));

    ShowCommand cmd = m_track.commands().at(index);
    if (ShowCommand::actionFromString(action, &cmd.action) == false)
        return editFailed(tr("unknown action %1").arg(action));

    // A trigger carries no value, so changing one drops it rather than keeping
    // a stale number the validation would reject.
    if (cmd.action != ShowCommandAction::SetIntensity)
        cmd.intensity = 0.0;

    ShowCommandTrack edited = m_track;
    QString error;
    if (edited.replace(cmd, &error) == false)
        return editFailed(error);

    return publishEdit(edited);
}

bool ShowCommandRecorder::setCommandTarget(quint32 id, quint32 functionId)
{
    const int index = m_track.indexOfId(id);
    if (index < 0)
        return editFailed(tr("no command carries id %1").arg(id));

    ShowCommand cmd = m_track.commands().at(index);
    cmd.functionId = functionId;

    ShowCommandTrack edited = m_track;
    QString error;
    if (edited.replace(cmd, &error) == false)
        return editFailed(error);

    // Target existence and self-recursion are the Show's call, not ours.
    return publishEdit(edited);
}

QVariantList ShowCommandRecorder::availableTargets() const
{
    QVariantList list;
    if (m_doc == nullptr)
        return list;

    for (Function *function : m_doc->functions())
    {
        if (function == nullptr || function->id() == trackedShowId())
            continue;

        // mLabel/mValue is what the shared combo box expects
        QVariantMap entry;
        entry["mLabel"] = function->name();
        entry["mValue"] = function->id();
        list.append(entry);
    }
    return list;
}

bool ShowCommandRecorder::retimeCommand(quint32 id, int ms)
{
    if (ms < 0)
        return editFailed(tr("%1 is not a valid time").arg(ms));

    ShowCommandTrack edited = m_track;
    QString error;
    if (edited.retime(id, quint32(ms), &error) == false)
        return editFailed(error);

    return publishEdit(edited);
}

bool ShowCommandRecorder::setCommandIntensity(quint32 id, qreal value)
{
    const int index = m_track.indexOfId(id);
    if (index < 0)
        return editFailed(tr("no command carries id %1").arg(id));

    ShowCommand cmd = m_track.commands().at(index);
    cmd.intensity = value;

    ShowCommandTrack edited = m_track;
    QString error;
    if (edited.replace(cmd, &error) == false)
        return editFailed(error);

    return publishEdit(edited);
}

bool ShowCommandRecorder::removeCommand(quint32 id)
{
    ShowCommandTrack edited = m_track;
    QString error;
    if (edited.remove(id, &error) == false)
        return editFailed(error);

    return publishEdit(edited);
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
    if (now < m_clockPosition)
    {
        if (isAuthoring())
        {
            emit takeAboutToFinalize(m_takeId);
            m_authoredExtentFloor = qMax(extentFloor(), m_clockPosition);
            m_takeId++;
        }
        m_state = ShowCommandFsm::seek(m_track, m_state, now).state;
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
        if (previous != nullptr)
            disconnect(previous, nullptr, this, nullptr);
    }

    m_trackedShowId = showId;

    Show *show = trackedShow();
    m_track = show != nullptr ? show->commandTrack() : ShowCommandTrack();
    // Whatever this Show already played to is a floor the take cannot shorten.
    m_authoredExtentFloor = m_track.extent();
    m_clockPosition = show != nullptr ? show->commandPosition() : 0;
    if (show != nullptr)
    {
        connect(show, &Show::commandTrackChanged,
                this, &ShowCommandRecorder::slotCommandTrackChanged);
        connect(show, &Show::timeChanged, this, &ShowCommandRecorder::slotShowClock);
        connect(show, &Show::externalElapsedTimeChanged,
                this, &ShowCommandRecorder::slotShowClock);
    }

    emit commandsChanged();
    emit positionChanged();
}

void ShowCommandRecorder::syncPosition(Show *show)
{
    if (show != nullptr)
        m_state.position = show->commandPosition();
}

bool ShowCommandRecorder::commit(const ShowCommandTransition &transition, Show *show)
{
    ShowCommandTrack candidate = m_track;
    QString error;

    for (const ShowCommand &cmd : transition.authored)
    {
        if (candidate.insert(cmd, &error) == false)
        {
            setError(error);
            return false;
        }

        // The take grows to the position actually reached. Nothing is padded:
        // Show::commandRecording() is what keeps a short Show playing.
        const quint32 reached = qMax(qMax(cmd.time, m_state.position), extentFloor());
        if (candidate.extent() < reached)
            candidate.setExtent(reached, nullptr);
    }

    if (publishTrack(candidate, transition.state.consumedLiveEventIds, show) == false)
        return false;

    m_state = transition.state;
    return true;
}

bool ShowCommandRecorder::publishTrack(const ShowCommandTrack &candidate,
                                       const QSet<quint32> &alreadyApplied, Show *show)
{
    QString error;
    QScopedValueRollback<bool> publishing(m_publishing, true);
    if (show->setCommandTrack(candidate, alreadyApplied, &error) == false)
    {
        setError(error);
        return false;
    }

    m_track = show->commandTrack();
    emit commandsChanged();
    return true;
}

bool ShowCommandRecorder::publishEdit(const ShowCommandTrack &candidate)
{
    Show *show = trackedShow();
    if (show == nullptr)
        return editFailed(tr("no Show selected"));

    if (publishTrack(candidate, QSet<quint32>(), show) == false)
        return false;

    setError(QString());
    return true;
}

bool ShowCommandRecorder::editFailed(const QString &error)
{
    setError(error);
    return false;
}

void ShowCommandRecorder::setError(const QString &error)
{
    if (m_lastError == error)
        return;

    m_lastError = error;
    emit lastErrorChanged();
}
