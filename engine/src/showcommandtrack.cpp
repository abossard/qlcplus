/*
  Q Light Controller Plus
  showcommandtrack.cpp

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

#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QLocale>
#include <QHash>
#include <algorithm>
#include <cmath>

#include "showcommandtrack.h"

#define KXMLShowCommand QStringLiteral("Command")
#define KXMLShowCommandTrackVersion QStringLiteral("Version")
#define KXMLShowCommandTrackExtent QStringLiteral("Extent")
#define KXMLShowCommandId QStringLiteral("ID")
#define KXMLShowCommandTime QStringLiteral("Time")
#define KXMLShowCommandAction QStringLiteral("Action")
#define KXMLShowCommandFunction QStringLiteral("Function")
#define KXMLShowCommandValue QStringLiteral("Value")

namespace
{
    QString actionName(ShowCommandAction action)
    {
        switch (action)
        {
            case ShowCommandAction::Start: return QStringLiteral("Start");
            case ShowCommandAction::Stop: return QStringLiteral("Stop");
            case ShowCommandAction::SetIntensity: return QStringLiteral("SetIntensity");
        }
        return QString();
    }

    bool fail(QString *error, const QString &reason)
    {
        if (error != nullptr)
            *error = reason;
        return false;
    }

    bool succeed(QString *error)
    {
        if (error != nullptr)
            error->clear();
        return true;
    }

    /** Shortest text that reads back as the very same double */
    QString intensityToString(qreal intensity)
    {
        return QLocale::c().toString(intensity, 'g', QLocale::FloatingPointShortest);
    }

    bool readUInt(const QXmlStreamAttributes &attrs, const QString &name, quint32 *out)
    {
        if (!attrs.hasAttribute(name))
            return false;

        bool ok = false;
        const uint value = attrs.value(name).toUInt(&ok);
        if (!ok)
            return false;

        *out = value;
        return true;
    }

    /** Called with the reader on a child of the command track. Skips that child,
     *  nested command tracks included, and leaves the reader on the end element of
     *  the track itself. */
    void skipRestOfTrack(QXmlStreamReader &root)
    {
        root.skipCurrentElement();
        while (root.readNextStartElement())
            root.skipCurrentElement();
    }

    /** A live mark guards its own event only until the playhead consumes that time */
    QSet<quint32> pendingLiveIds(const ShowCommandTrack &track, const QSet<quint32> &marked,
                                 quint32 position)
    {
        if (marked.isEmpty())
            return marked;

        QSet<quint32> pending;
        for (const ShowCommand &cmd : track.commands())
        {
            if (cmd.time > position && marked.contains(cmd.id))
                pending.insert(cmd.id);
        }
        return pending;
    }
}

/************************************************************************
 * ShowCommand
 ***********************************************************************/

ShowCommand ShowCommand::start(quint32 id, quint32 time, quint32 functionId)
{
    ShowCommand cmd;
    cmd.id = id;
    cmd.time = time;
    cmd.functionId = functionId;
    cmd.action = ShowCommandAction::Start;
    return cmd;
}

ShowCommand ShowCommand::stop(quint32 id, quint32 time, quint32 functionId)
{
    ShowCommand cmd = start(id, time, functionId);
    cmd.action = ShowCommandAction::Stop;
    return cmd;
}

ShowCommand ShowCommand::setIntensity(quint32 id, quint32 time, quint32 functionId, qreal intensity)
{
    ShowCommand cmd = start(id, time, functionId);
    cmd.action = ShowCommandAction::SetIntensity;
    cmd.intensity = intensity;
    return cmd;
}

QString ShowCommand::validate(const ShowCommand &cmd)
{
    if (actionName(cmd.action).isEmpty())
        return QStringLiteral("unknown action %1").arg(int(cmd.action));

    if (cmd.id == InvalidId)
        return QStringLiteral("invalid event id");

    if (cmd.functionId == InvalidId)
        return QStringLiteral("invalid function target");

    if (cmd.time > MaxTime)
        return QStringLiteral("time out of range");

    if (cmd.action == ShowCommandAction::SetIntensity)
    {
        if (!std::isfinite(cmd.intensity))
            return QStringLiteral("intensity is not a finite number");
        if (cmd.intensity < 0.0 || cmd.intensity > 1.0)
            return QStringLiteral("intensity outside 0..1");
    }
    else if (cmd.intensity != 0.0)
    {
        return QStringLiteral("%1 carries no value").arg(actionName(cmd.action));
    }

    return QString();
}

QString ShowCommand::actionToString(ShowCommandAction action)
{
    return actionName(action);
}

bool ShowCommand::actionFromString(const QString &name, ShowCommandAction *action)
{
    for (ShowCommandAction candidate : {ShowCommandAction::Start, ShowCommandAction::Stop,
                                        ShowCommandAction::SetIntensity})
    {
        if (actionName(candidate) != name)
            continue;

        if (action != nullptr)
            *action = candidate;
        return true;
    }
    return false;
}

bool ShowCommand::operator==(const ShowCommand &other) const
{
    return id == other.id && time == other.time && functionId == other.functionId
        && action == other.action && intensity == other.intensity;
}

/************************************************************************
 * ShowCommandTrack
 ***********************************************************************/

bool ShowCommandTrack::isEmpty() const
{
    return m_commands.isEmpty();
}

int ShowCommandTrack::count() const
{
    return m_commands.count();
}

const QVector<ShowCommand> &ShowCommandTrack::commands() const
{
    return m_commands;
}

int ShowCommandTrack::indexOfId(quint32 id) const
{
    for (int i = 0; i < m_commands.count(); i++)
    {
        if (m_commands.at(i).id == id)
            return i;
    }
    return -1;
}

bool ShowCommandTrack::contains(quint32 id) const
{
    return indexOfId(id) >= 0;
}

quint32 ShowCommandTrack::extent() const
{
    return m_extent;
}

bool ShowCommandTrack::setExtent(quint32 ms, QString *error)
{
    if (ms > ShowCommand::MaxTime)
        return fail(error, QStringLiteral("extent out of range"));

    m_extent = ms;
    return succeed(error);
}

quint32 ShowCommandTrack::lastCommandTime() const
{
    return m_commands.isEmpty() ? 0 : m_commands.last().time;
}

quint32 ShowCommandTrack::nextEventId() const
{
    quint32 highest = 0;
    bool any = false;

    for (const ShowCommand &cmd : m_commands)
    {
        if (!any || cmd.id > highest)
            highest = cmd.id;
        any = true;
    }

    if (!any)
        return 0;

    if (highest >= ShowCommand::MaxTime)
        return ShowCommand::InvalidId;

    return highest + 1;
}

bool ShowCommandTrack::insert(const ShowCommand &cmd, QString *error)
{
    const QString reason = ShowCommand::validate(cmd);
    if (!reason.isEmpty())
        return fail(error, reason);

    if (contains(cmd.id))
        return fail(error, QStringLiteral("duplicate event id %1").arg(cmd.id));

    // after every command sharing this time, so insertion order is the tie break
    const auto at = std::upper_bound(m_commands.begin(), m_commands.end(), cmd.time,
                                     [](quint32 time, const ShowCommand &other)
                                     { return time < other.time; });
    m_commands.insert(at, cmd);
    return succeed(error);
}

bool ShowCommandTrack::replace(const ShowCommand &cmd, QString *error)
{
    const int index = indexOfId(cmd.id);
    if (index < 0)
        return fail(error, QStringLiteral("unknown event id %1").arg(cmd.id));

    const QString reason = ShowCommand::validate(cmd);
    if (!reason.isEmpty())
        return fail(error, reason);

    if (m_commands.at(index).time == cmd.time)
    {
        m_commands[index] = cmd;
        return succeed(error);
    }

    m_commands.remove(index);
    return insert(cmd, error);
}

bool ShowCommandTrack::retime(quint32 id, quint32 time, QString *error)
{
    const int index = indexOfId(id);
    if (index < 0)
        return fail(error, QStringLiteral("unknown event id %1").arg(id));

    ShowCommand moved = m_commands.at(index);
    moved.time = time;
    return replace(moved, error);
}

bool ShowCommandTrack::remove(quint32 id, QString *error)
{
    const int index = indexOfId(id);
    if (index < 0)
        return fail(error, QStringLiteral("unknown event id %1").arg(id));

    m_commands.remove(index);
    return succeed(error);
}

QList<quint32> ShowCommandTrack::referencedFunctionIds() const
{
    QList<quint32> ids;
    for (const ShowCommand &cmd : m_commands)
    {
        if (!ids.contains(cmd.functionId))
            ids.append(cmd.functionId);
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

int ShowCommandTrack::remapFunctionId(quint32 from, quint32 to)
{
    if (from == ShowCommand::InvalidId || to == ShowCommand::InvalidId || from == to)
        return 0;

    int changed = 0;
    for (ShowCommand &cmd : m_commands)
    {
        if (cmd.functionId != from)
            continue;

        cmd.functionId = to;
        changed++;
    }
    return changed;
}

bool ShowCommandTrack::loadXML(QXmlStreamReader &root, QString *error)
{
    if (root.name() != KXMLShowCommandTrack)
        return fail(error, QStringLiteral("command track node not found"));

    const QXmlStreamAttributes attrs = root.attributes();
    quint32 version = 0;
    if (!readUInt(attrs, KXMLShowCommandTrackVersion, &version) || int(version) != Version)
    {
        // still on the track element itself, so skipping it lands on its end element
        root.skipCurrentElement();
        return fail(error, QStringLiteral("unsupported command track version '%1'")
                               .arg(attrs.value(KXMLShowCommandTrackVersion).toString()));
    }

    // parse into a separate track, so a rejected file leaves this one alone
    ShowCommandTrack parsed;

    if (attrs.hasAttribute(KXMLShowCommandTrackExtent))
    {
        quint32 extent = 0;
        if (!readUInt(attrs, KXMLShowCommandTrackExtent, &extent) || !parsed.setExtent(extent))
        {
            root.skipCurrentElement();
            return fail(error, QStringLiteral("invalid extent '%1'")
                                   .arg(attrs.value(KXMLShowCommandTrackExtent).toString()));
        }
    }

    while (root.readNextStartElement())
    {
        if (root.name() != KXMLShowCommand)
        {
            const QString unknown = root.name().toString();
            skipRestOfTrack(root);
            return fail(error, QStringLiteral("unknown command track element '%1'").arg(unknown));
        }

        const QXmlStreamAttributes cmdAttrs = root.attributes();
        ShowCommand cmd;
        QString reason;

        if (!readUInt(cmdAttrs, KXMLShowCommandId, &cmd.id))
            reason = QStringLiteral("invalid event id");
        else if (!readUInt(cmdAttrs, KXMLShowCommandTime, &cmd.time))
            reason = QStringLiteral("invalid time");
        else if (!readUInt(cmdAttrs, KXMLShowCommandFunction, &cmd.functionId))
            reason = QStringLiteral("invalid function target");
        else if (!ShowCommand::actionFromString(cmdAttrs.value(KXMLShowCommandAction).toString(),
                                                &cmd.action))
            reason = QStringLiteral("unknown action '%1'")
                         .arg(cmdAttrs.value(KXMLShowCommandAction).toString());

        if (reason.isEmpty() && cmdAttrs.hasAttribute(KXMLShowCommandValue))
        {
            bool ok = false;
            cmd.intensity = cmdAttrs.value(KXMLShowCommandValue).toDouble(&ok);
            if (!ok)
                reason = QStringLiteral("invalid value '%1'")
                             .arg(cmdAttrs.value(KXMLShowCommandValue).toString());
        }
        else if (reason.isEmpty() && cmd.action == ShowCommandAction::SetIntensity)
        {
            reason = QStringLiteral("SetIntensity without a value");
        }

        if (reason.isEmpty())
            parsed.insert(cmd, &reason);

        if (!reason.isEmpty())
        {
            skipRestOfTrack(root);
            return fail(error, reason);
        }

        root.skipCurrentElement();
    }

    if (root.hasError())
        return fail(error, root.errorString());

    *this = parsed;
    return succeed(error);
}

bool ShowCommandTrack::saveXML(QXmlStreamWriter *doc) const
{
    Q_ASSERT(doc != nullptr);

    doc->writeStartElement(KXMLShowCommandTrack);
    doc->writeAttribute(KXMLShowCommandTrackVersion, QString::number(Version));
    doc->writeAttribute(KXMLShowCommandTrackExtent, QString::number(m_extent));

    for (const ShowCommand &cmd : m_commands)
    {
        doc->writeStartElement(KXMLShowCommand);
        doc->writeAttribute(KXMLShowCommandId, QString::number(cmd.id));
        doc->writeAttribute(KXMLShowCommandTime, QString::number(cmd.time));
        doc->writeAttribute(KXMLShowCommandAction, ShowCommand::actionToString(cmd.action));
        doc->writeAttribute(KXMLShowCommandFunction, QString::number(cmd.functionId));
        if (cmd.action == ShowCommandAction::SetIntensity)
            doc->writeAttribute(KXMLShowCommandValue, intensityToString(cmd.intensity));
        doc->writeEndElement();
    }

    doc->writeEndElement();
    return true;
}

/************************************************************************
 * ShowCommandState
 ***********************************************************************/

ShowRecordPhase ShowCommandState::phase() const
{
    if (!recording)
        return ShowRecordPhase::Off;

    if (boundShowId == ShowCommand::InvalidId)
        return ShowRecordPhase::Armed;

    return boundShowId == resolvedShowId ? ShowRecordPhase::Bound : ShowRecordPhase::Suspended;
}

bool ShowCommandState::operator==(const ShowCommandState &other) const
{
    return recording == other.recording && playing == other.playing
        && boundShowId == other.boundShowId && resolvedShowId == other.resolvedShowId
        && position == other.position && consumedThrough == other.consumedThrough
        && consumedLiveEventIds == other.consumedLiveEventIds;
}

/************************************************************************
 * ShowCommandFsm
 ***********************************************************************/

ShowCommandState ShowCommandFsm::setRecording(const ShowCommandState &state, bool on)
{
    ShowCommandState next = state;
    next.recording = on;

    if (!on)
        next.boundShowId = ShowCommand::InvalidId;
    else if (!state.recording)
        next.boundShowId = state.resolvedShowId; // arming with a show already resolved binds it

    return next;
}

ShowCommandState ShowCommandFsm::setResolvedShow(const ShowCommandState &state, quint32 showId)
{
    ShowCommandState next = state;
    next.resolvedShowId = showId;

    // an armed recorder binds the first Show it sees and keeps it afterwards
    if (state.recording && state.boundShowId == ShowCommand::InvalidId)
        next.boundShowId = showId;

    return next;
}

ShowCommandState ShowCommandFsm::setPlaying(const ShowCommandState &state, bool playing)
{
    ShowCommandState next = state;
    next.playing = playing;
    return next;
}

ShowCommandTransition ShowCommandFsm::advance(const ShowCommandTrack &track,
                                              const ShowCommandState &state, quint32 positionMs)
{
    ShowCommandTransition result;
    result.state = state;

    // a frozen transport is due for nothing, and going backwards is a seek
    if (!state.playing || positionMs < state.position)
        return result;

    const quint32 position = qMin(positionMs, ShowCommand::MaxTime);

    for (const ShowCommand &cmd : track.commands())
    {
        if (cmd.time > position)
            break;
        // the host already executed a live event, so it never echoes back at us
        if (cmd.time >= state.consumedThrough && !state.consumedLiveEventIds.contains(cmd.id))
            result.effects.append(cmd);
    }

    result.state.position = position;
    result.state.consumedThrough = position + 1;
    result.state.consumedLiveEventIds = pendingLiveIds(track, state.consumedLiveEventIds, position);
    return result;
}

ShowCommandTransition ShowCommandFsm::seek(const ShowCommandTrack &track,
                                           const ShowCommandState &state, quint32 positionMs)
{
    ShowCommandTransition result;
    result.state = state;

    const quint32 destination = qMin(positionMs, ShowCommand::MaxTime);
    result.state.position = destination;
    result.state.consumedThrough = destination;
    // a new traversal owes nothing to what was executed live during the previous one
    result.state.consumedLiveEventIds.clear();

    // the latest authored value per target, from before the destination only:
    // history is restored as values, never replayed as triggers
    QHash<quint32, int> latest;
    const QVector<ShowCommand> &commands = track.commands();

    for (int i = 0; i < commands.count(); i++)
    {
        const ShowCommand &cmd = commands.at(i);
        if (cmd.time >= destination)
            break;
        if (cmd.action == ShowCommandAction::SetIntensity)
            latest.insert(cmd.functionId, i);
    }

    QList<int> indexes = latest.values();
    std::sort(indexes.begin(), indexes.end());
    for (int index : indexes)
        result.effects.append(commands.at(index));

    return result;
}

ShowCommandTransition ShowCommandFsm::userInput(const ShowCommandTrack &track,
                                                const ShowCommandState &state,
                                                const ShowCommandInput &input)
{
    ShowCommandTransition result;
    result.state = state;

    // only a bound recorder authors anything; a frozen transport records at its
    // own position, a different resolved Show does not reach this track at all
    if (state.phase() != ShowRecordPhase::Bound)
        return result;

    if (input.origin != ShowCommandOrigin::Pointer &&
        input.origin != ShowCommandOrigin::Midi &&
        input.origin != ShowCommandOrigin::Keyboard)
        return result;

    ShowCommand cmd;
    cmd.id = track.nextEventId();
    cmd.time = state.position;
    cmd.functionId = input.functionId;
    cmd.action = input.action;
    // offered as it came in: a value on a trigger is rejected, not quietly dropped
    cmd.intensity = input.intensity;

    if (cmd.id == ShowCommand::InvalidId)
    {
        result.error = QStringLiteral("no free event id left in this track");
        return result;
    }

    result.error = ShowCommand::validate(cmd);
    if (!result.error.isEmpty())
        return result;

    result.authored.append(cmd);
    // the host already executed this input: mark that one event, leave the cursor alone
    result.state.consumedLiveEventIds.insert(cmd.id);
    return result;
}
