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
#include <limits>

#include "showcommandtrack.h"

#define KXMLShowCommand QStringLiteral("Command")
#define KXMLShowCommandTrackVersion QStringLiteral("Version")
#define KXMLShowCommandTrackExtent QStringLiteral("Extent")
#define KXMLShowCommandId QStringLiteral("ID")
#define KXMLShowCommandOrder QStringLiteral("Order")
#define KXMLShowCommandTime QStringLiteral("Time")
#define KXMLShowCommandAction QStringLiteral("Action")
#define KXMLShowCommandFunction QStringLiteral("Function")
#define KXMLShowCommandValue QStringLiteral("Value")
#define KXMLShowCommandControl QStringLiteral("Control")
#define KXMLShowCommandRole QStringLiteral("Role")
#define KXMLShowCommandState QStringLiteral("State")
#define KXMLShowCommandAttribute QStringLiteral("Attribute")
#define KXMLShowCommandStateOn QStringLiteral("On")
#define KXMLShowCommandStateOff QStringLiteral("Off")

namespace
{
    QString actionName(ShowCommandAction action)
    {
        switch (action)
        {
            case ShowCommandAction::Start: return QStringLiteral("Start");
            case ShowCommandAction::Stop: return QStringLiteral("Stop");
            case ShowCommandAction::SetIntensity: return QStringLiteral("SetIntensity");
            case ShowCommandAction::SetButtonState: return QStringLiteral("SetButtonState");
            case ShowCommandAction::SetSliderPosition: return QStringLiteral("SetSliderPosition");
        }
        return QString();
    }

    QString roleName(ShowControlRole role)
    {
        switch (role)
        {
            case ShowControlRole::None: return QStringLiteral("None");
            case ShowControlRole::ToggleButton: return QStringLiteral("ToggleButton");
            case ShowControlRole::LevelSlider: return QStringLiteral("LevelSlider");
            case ShowControlRole::AdjustSlider: return QStringLiteral("AdjustSlider");
            case ShowControlRole::SubmasterSlider: return QStringLiteral("SubmasterSlider");
            case ShowControlRole::GrandMasterSlider: return QStringLiteral("GrandMasterSlider");
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

    bool isControlAction(ShowCommandAction action)
    {
        return action == ShowCommandAction::SetButtonState ||
               action == ShowCommandAction::SetSliderPosition;
    }

    bool isSliderRole(ShowControlRole role)
    {
        return role == ShowControlRole::LevelSlider || role == ShowControlRole::AdjustSlider ||
               role == ShowControlRole::SubmasterSlider || role == ShowControlRole::GrandMasterSlider;
    }

    /** A VC state record names a control and carries exactly its own value */
    QString validateControlState(const ShowCommand &cmd)
    {
        const QString action = actionName(cmd.action);

        if (cmd.functionId != ShowCommand::InvalidId)
            return QStringLiteral("%1 carries a function target").arg(action);

        if (cmd.controlId.isNull())
            return QStringLiteral("invalid control target");

        if (cmd.time > ShowCommand::MaxTime)
            return QStringLiteral("time out of range");

        if (cmd.intensity != 0.0)
            return QStringLiteral("%1 carries no intensity").arg(action);

        if (cmd.action == ShowCommandAction::SetButtonState)
        {
            if (cmd.role != ShowControlRole::ToggleButton)
                return QStringLiteral("%1 needs a toggle button role").arg(action);
            if (!cmd.attribute.isEmpty() || cmd.position != 0.0)
                return QStringLiteral("%1 carries no slider value").arg(action);
            return QString();
        }

        if (!isSliderRole(cmd.role))
            return QStringLiteral("%1 needs a slider role").arg(action);
        if (cmd.on)
            return QStringLiteral("%1 carries no button state").arg(action);
        if (cmd.attribute.isEmpty() != (cmd.role != ShowControlRole::AdjustSlider))
            return QStringLiteral("only an adjust slider names a function attribute");
        if (!std::isfinite(cmd.position))
            return QStringLiteral("position is not a finite number");
        if (cmd.position < 0.0 || cmd.position > 1.0)
            return QStringLiteral("position outside 0..1");
        return QString();
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

    /** Target and value of a function command. Empty when read, otherwise the reason. */
    QString readFunctionCommand(const QXmlStreamAttributes &attrs, ShowCommand *cmd)
    {
        if (!readUInt(attrs, KXMLShowCommandFunction, &cmd->functionId))
            return QStringLiteral("invalid function target");

        for (const QString &name : {KXMLShowCommandControl, KXMLShowCommandRole,
                                    KXMLShowCommandState, KXMLShowCommandAttribute})
        {
            if (attrs.hasAttribute(name))
                return QStringLiteral("%1 carries VC control state")
                    .arg(ShowCommand::actionToString(cmd->action));
        }

        if (attrs.hasAttribute(KXMLShowCommandValue))
        {
            bool ok = false;
            cmd->intensity = attrs.value(KXMLShowCommandValue).toDouble(&ok);
            if (!ok)
                return QStringLiteral("invalid value '%1'")
                    .arg(attrs.value(KXMLShowCommandValue).toString());
        }
        else if (cmd->action == ShowCommandAction::SetIntensity)
        {
            return QStringLiteral("SetIntensity without a value");
        }
        return QString();
    }

    /** Control target and state of a VC state record. Empty when read, otherwise the reason. */
    QString readControlState(const QXmlStreamAttributes &attrs, ShowCommand *cmd)
    {
        const QString action = ShowCommand::actionToString(cmd->action);

        if (attrs.hasAttribute(KXMLShowCommandFunction))
            return QStringLiteral("%1 carries a function target").arg(action);

        cmd->controlId = ShowCommand::controlIdFromString(attrs.value(KXMLShowCommandControl).toString());
        if (cmd->controlId.isNull())
            return QStringLiteral("invalid control target '%1'")
                .arg(attrs.value(KXMLShowCommandControl).toString());

        if (!ShowCommand::roleFromString(attrs.value(KXMLShowCommandRole).toString(), &cmd->role))
            return QStringLiteral("unknown role '%1'").arg(attrs.value(KXMLShowCommandRole).toString());

        if (cmd->action == ShowCommandAction::SetButtonState)
        {
            if (attrs.hasAttribute(KXMLShowCommandValue) || attrs.hasAttribute(KXMLShowCommandAttribute))
                return QStringLiteral("%1 carries no slider value").arg(action);

            const QString state = attrs.value(KXMLShowCommandState).toString();
            if (state != KXMLShowCommandStateOn && state != KXMLShowCommandStateOff)
                return QStringLiteral("invalid state '%1'").arg(state);
            cmd->on = state == KXMLShowCommandStateOn;
            return QString();
        }

        if (attrs.hasAttribute(KXMLShowCommandState))
            return QStringLiteral("%1 carries no button state").arg(action);

        bool ok = false;
        cmd->position = attrs.value(KXMLShowCommandValue).toDouble(&ok);
        if (!ok)
            return QStringLiteral("invalid value '%1'").arg(attrs.value(KXMLShowCommandValue).toString());

        cmd->attribute = attrs.value(KXMLShowCommandAttribute).toString();
        return QString();
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

    /** A live mark guards its own occurrence only until the playhead consumes the
     *  mark's time, wherever its event was moved or whether it still exists */
    ShowLiveMarks pendingLiveMarks(const ShowLiveMarks &marked, quint32 position)
    {
        ShowLiveMarks pending;
        for (auto it = marked.cbegin(); it != marked.cend(); ++it)
        {
            if (it.value().time > position)
                pending.insert(it.key(), it.value());
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

ShowCommand ShowCommand::setButtonState(quint32 id, quint32 time, const QUuid &controlId, bool on)
{
    ShowCommand cmd = start(id, time, InvalidId);
    cmd.action = ShowCommandAction::SetButtonState;
    cmd.controlId = controlId;
    cmd.role = ShowControlRole::ToggleButton;
    cmd.on = on;
    return cmd;
}

ShowCommand ShowCommand::setSliderPosition(quint32 id, quint32 time, const QUuid &controlId,
                                           ShowControlRole role, const QString &attribute,
                                           qreal position)
{
    ShowCommand cmd = start(id, time, InvalidId);
    cmd.action = ShowCommandAction::SetSliderPosition;
    cmd.controlId = controlId;
    cmd.role = role;
    cmd.attribute = attribute;
    cmd.position = position;
    return cmd;
}

QString ShowCommand::validate(const ShowCommand &cmd)
{
    if (actionName(cmd.action).isEmpty())
        return QStringLiteral("unknown action %1").arg(int(cmd.action));

    if (cmd.id == InvalidId)
        return QStringLiteral("invalid event id");

    if (isControlAction(cmd.action))
        return validateControlState(cmd);

    if (cmd.functionId == InvalidId)
        return QStringLiteral("invalid function target");

    if (!cmd.controlId.isNull() || cmd.role != ShowControlRole::None || !cmd.attribute.isEmpty()
        || cmd.on || cmd.position != 0.0)
        return QStringLiteral("%1 carries VC control state").arg(actionName(cmd.action));

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
                                        ShowCommandAction::SetIntensity,
                                        ShowCommandAction::SetButtonState,
                                        ShowCommandAction::SetSliderPosition})
    {
        if (actionName(candidate) != name)
            continue;

        if (action != nullptr)
            *action = candidate;
        return true;
    }
    return false;
}

QString ShowCommand::roleToString(ShowControlRole role)
{
    return roleName(role);
}

bool ShowCommand::roleFromString(const QString &name, ShowControlRole *role)
{
    for (ShowControlRole candidate : {ShowControlRole::None, ShowControlRole::ToggleButton,
                                      ShowControlRole::LevelSlider, ShowControlRole::AdjustSlider,
                                      ShowControlRole::SubmasterSlider,
                                      ShowControlRole::GrandMasterSlider})
    {
        if (roleName(candidate) != name)
            continue;

        if (role != nullptr)
            *role = candidate;
        return true;
    }
    return false;
}

QUuid ShowCommand::controlIdFromString(const QString &text)
{
    // QUuid ignores trailing text and a missing closing brace; only its own
    // braced or unbraced spelling is accepted, so no malformed id resolves
    const QUuid id(text);
    if (text.compare(id.toString(QUuid::WithBraces), Qt::CaseInsensitive) != 0 &&
        text.compare(id.toString(QUuid::WithoutBraces), Qt::CaseInsensitive) != 0)
        return QUuid();
    return id;
}

bool ShowCommand::operator==(const ShowCommand &other) const
{
    return id == other.id && time == other.time && functionId == other.functionId
        && action == other.action && intensity == other.intensity
        && controlId == other.controlId && role == other.role && attribute == other.attribute
        && on == other.on && position == other.position;
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

QVector<ShowCommandGroup> ShowCommandTrack::groups() const
{
    QVector<ShowCommandGroup> result;
    for (const ShowCommand &command : m_commands)
    {
        if (!result.isEmpty() && command.action == ShowCommandAction::SetSliderPosition)
        {
            ShowCommandGroup &last = result.last();
            if (last.action == command.action && last.controlId == command.controlId
                && last.role == command.role && last.attribute == command.attribute)
            {
                last.eventIds.append(command.id);
                last.endTime = command.time;
                continue;
            }
        }
        result.append({ { command.id }, command.action, command.controlId, command.role,
                        command.attribute, command.time, command.time });
    }
    return result;
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
    return m_nextEventId;
}

quint32 ShowCommandTrack::nextOrder() const
{
    return m_nextOrder;
}

void ShowCommandTrack::reserve(quint32 nextEventId, quint32 nextOrder)
{
    if (m_nextEventId != ShowCommand::InvalidId && nextEventId > m_nextEventId)
        m_nextEventId = nextEventId;
    if (m_nextOrder != ShowCommand::InvalidId && nextOrder > m_nextOrder)
        m_nextOrder = nextOrder;
}

bool ShowCommandTrack::place(const ShowCommand &cmd, QString *error)
{
    const QString reason = ShowCommand::validate(cmd);
    if (!reason.isEmpty())
        return fail(error, reason);

    if (contains(cmd.id))
        return fail(error, QStringLiteral("duplicate event id %1").arg(cmd.id));

    const auto at = std::upper_bound(m_commands.begin(), m_commands.end(), cmd,
                                     [](const ShowCommand &placed, const ShowCommand &other)
                                     { return placed.time < other.time ||
                                              (placed.time == other.time && placed.order < other.order); });
    m_commands.insert(at, cmd);

    if (cmd.id >= ShowCommand::MaxTime)
        m_nextEventId = ShowCommand::InvalidId;
    else if (m_nextEventId != ShowCommand::InvalidId && cmd.id >= m_nextEventId)
        m_nextEventId = cmd.id + 1;
    if (cmd.order >= ShowCommand::MaxTime)
        m_nextOrder = ShowCommand::InvalidId;
    else if (m_nextOrder != ShowCommand::InvalidId && cmd.order >= m_nextOrder)
        m_nextOrder = cmd.order + 1;
    return succeed(error);
}

bool ShowCommandTrack::insert(const ShowCommand &cmd, QString *error)
{
    if (m_nextOrder == ShowCommand::InvalidId)
        return fail(error, QStringLiteral("no equal-time order left"));

    // after every command sharing this time
    ShowCommand inserted = cmd;
    inserted.order = m_nextOrder;
    return place(inserted, error);
}

bool ShowCommandTrack::restore(const ShowCommand &cmd, QString *error)
{
    for (const ShowCommand &other : std::as_const(m_commands))
    {
        if (other.order == cmd.order && other.id != cmd.id)
            return fail(error, QStringLiteral("equal-time order %1 is taken").arg(cmd.order));
    }
    return place(cmd, error);
}

bool ShowCommandTrack::replace(const ShowCommand &cmd, QString *error)
{
    const int index = indexOfId(cmd.id);
    if (index < 0)
        return fail(error, QStringLiteral("unknown event id %1").arg(cmd.id));

    const QString reason = ShowCommand::validate(cmd);
    if (!reason.isEmpty())
        return fail(error, reason);

    ShowCommand replaced = cmd;
    replaced.order = m_commands.at(index).order;
    if (m_commands.at(index).time == cmd.time)
    {
        m_commands[index] = replaced;
        return succeed(error);
    }

    const ShowCommand previous = m_commands.takeAt(index);
    if (place(replaced, error))
        return true;
    m_commands.insert(index, previous);
    return false;
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

ShowRetimeRefusal ShowCommandTrack::retimeSelection(const QVector<quint32> &ids, ShowRetimeKind kind,
                                                    qint64 deltaMs, ShowCommandTrack *result) const
{
    const QSet<quint32> wanted(ids.cbegin(), ids.cend());
    // the selected events in their stored, (time, Order), sequence
    QVector<ShowCommand> selected;
    for (const ShowCommand &cmd : m_commands)
    {
        if (wanted.contains(cmd.id))
            selected.append(cmd);
    }
    if (selected.isEmpty() || selected.count() != wanted.count())
        return ShowRetimeRefusal::UnknownEvent;

    const qint64 first = selected.first().time;
    const qint64 last = selected.last().time;
    const quint64 oldSpan = quint64(last - first);
    qint64 pin = 0;
    qint64 newSpan = 0;
    if (kind != ShowRetimeKind::Move)
    {
        if (oldSpan == 0)
            return ShowRetimeRefusal::NoSpan;
        // the delta moves the dragged edge; the other one stays
        newSpan = kind == ShowRetimeKind::StretchEnd ? qint64(oldSpan) + deltaMs : qint64(oldSpan) - deltaMs;
        if (newSpan <= 0)
            return ShowRetimeRefusal::NoTargetSpan;
        pin = kind == ShowRetimeKind::StretchEnd ? first : last;
        const qint64 moved = kind == ShowRetimeKind::StretchEnd ? first + newSpan : last - newSpan;
        if (moved < 0 || moved > qint64(ShowCommand::MaxTime))
            return ShowRetimeRefusal::OutOfRange;
    }

    // both factors below 2^32, so the product and the half fit 64 unsigned bits
    const auto scaled = [&](qint64 time) -> qint64 {
        const quint64 offset = quint64(time > pin ? time - pin : pin - time);
        const qint64 rounded = qint64((offset * quint64(newSpan) + oldSpan / 2) / oldSpan);
        return time > pin ? pin + rounded : pin - rounded;
    };

    QVector<qint64> times;
    for (const ShowCommand &cmd : std::as_const(selected))
    {
        const qint64 time = kind == ShowRetimeKind::Move ? qint64(cmd.time) + deltaMs : scaled(cmd.time);
        if (time < 0 || time > qint64(ShowCommand::MaxTime))
            return ShowRetimeRefusal::OutOfRange;
        if (times.isEmpty() == false &&
            (time < times.last() || (time == times.last() && cmd.order < selected.at(times.count() - 1).order)))
            return ShowRetimeRefusal::Reordered;
        times.append(time);
    }

    ShowCommandTrack edited = *this;
    for (int i = 0; i < selected.count(); ++i)
    {
        if (edited.retime(selected.at(i).id, quint32(times.at(i))) == false)
            return ShowRetimeRefusal::OutOfRange;
    }
    *result = edited;
    return ShowRetimeRefusal::None;
}

QList<quint32> ShowCommandTrack::referencedFunctionIds() const
{
    QList<quint32> ids;
    for (const ShowCommand &cmd : m_commands)
    {
        if (!isControlAction(cmd.action) && !ids.contains(cmd.functionId))
            ids.append(cmd.functionId);
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

QVector<ShowControlReference> ShowCommandTrack::referencedControls() const
{
    QVector<ShowControlReference> refs;
    for (const ShowCommand &cmd : m_commands)
    {
        if (!isControlAction(cmd.action))
            continue;

        auto same = [&cmd](const ShowControlReference &ref)
        { return ref.controlId == cmd.controlId && ref.role == cmd.role && ref.attribute == cmd.attribute; };
        auto it = std::find_if(refs.begin(), refs.end(), same);
        if (it == refs.end())
            refs.append({cmd.controlId, cmd.role, cmd.attribute, 1});
        else
            it->count++;
    }
    return refs;
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
    if (!readUInt(attrs, KXMLShowCommandTrackVersion, &version) ||
        (int(version) != LegacyVersion && int(version) != Version))
    {
        // still on the track element itself, so skipping it lands on its end element
        root.skipCurrentElement();
        return fail(error, QStringLiteral("unsupported command track version '%1'")
                               .arg(attrs.value(KXMLShowCommandTrackVersion).toString()));
    }

    // parse into a separate track, so a rejected file leaves this one alone
    ShowCommandTrack parsed;
    // an older document has no orders: its file order is the equal-time order
    bool ordered = false;

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
        else if (!ShowCommand::actionFromString(cmdAttrs.value(KXMLShowCommandAction).toString(),
                                                &cmd.action))
            reason = QStringLiteral("unknown action '%1'")
                         .arg(cmdAttrs.value(KXMLShowCommandAction).toString());
        else if (!isControlAction(cmd.action))
            reason = readFunctionCommand(cmdAttrs, &cmd);
        else if (int(version) == LegacyVersion)
            reason = QStringLiteral("%1 needs command track version %2")
                         .arg(ShowCommand::actionToString(cmd.action)).arg(Version);
        else
            reason = readControlState(cmdAttrs, &cmd);

        const bool hasOrder = cmdAttrs.hasAttribute(KXMLShowCommandOrder);
        if (parsed.isEmpty())
            ordered = hasOrder;
        if (reason.isEmpty() && hasOrder != ordered)
            reason = QStringLiteral("equal-time order on some commands only");
        else if (reason.isEmpty() && ordered && !readUInt(cmdAttrs, KXMLShowCommandOrder, &cmd.order))
            reason = QStringLiteral("invalid equal-time order");

        if (reason.isEmpty() && ordered)
            parsed.restore(cmd, &reason);
        else if (reason.isEmpty())
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

    // a track of function commands only keeps writing exactly the legacy document
    const bool controlStates = std::any_of(m_commands.cbegin(), m_commands.cend(),
                                           [](const ShowCommand &cmd)
                                           { return isControlAction(cmd.action); });

    doc->writeStartElement(KXMLShowCommandTrack);
    doc->writeAttribute(KXMLShowCommandTrackVersion,
                        QString::number(controlStates ? Version : LegacyVersion));
    doc->writeAttribute(KXMLShowCommandTrackExtent, QString::number(m_extent));

    // orders the file order gives back on loading are not written, so such a
    // track keeps writing exactly the document it did before orders existed
    bool fileOrder = true;
    for (int i = 0; i < m_commands.count(); i++)
        fileOrder = fileOrder && m_commands.at(i).order == quint32(i);

    for (const ShowCommand &cmd : m_commands)
    {
        doc->writeStartElement(KXMLShowCommand);
        doc->writeAttribute(KXMLShowCommandId, QString::number(cmd.id));
        if (!fileOrder)
            doc->writeAttribute(KXMLShowCommandOrder, QString::number(cmd.order));
        doc->writeAttribute(KXMLShowCommandTime, QString::number(cmd.time));
        doc->writeAttribute(KXMLShowCommandAction, ShowCommand::actionToString(cmd.action));
        if (isControlAction(cmd.action))
        {
            doc->writeAttribute(KXMLShowCommandControl, cmd.controlId.toString());
            doc->writeAttribute(KXMLShowCommandRole, ShowCommand::roleToString(cmd.role));
            if (cmd.action == ShowCommandAction::SetButtonState)
            {
                doc->writeAttribute(KXMLShowCommandState, cmd.on ? KXMLShowCommandStateOn
                                                                 : KXMLShowCommandStateOff);
            }
            else
            {
                doc->writeAttribute(KXMLShowCommandValue, intensityToString(cmd.position));
                if (!cmd.attribute.isEmpty())
                    doc->writeAttribute(KXMLShowCommandAttribute, cmd.attribute);
            }
        }
        else
        {
            doc->writeAttribute(KXMLShowCommandFunction, QString::number(cmd.functionId));
            if (cmd.action == ShowCommandAction::SetIntensity)
                doc->writeAttribute(KXMLShowCommandValue, intensityToString(cmd.intensity));
        }
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
        // the host already executed a live occurrence, so it never echoes back at us;
        // the same event at another time is a different occurrence and plays
        const auto mark = state.consumedLiveEventIds.constFind(cmd.id);
        const bool live = mark != state.consumedLiveEventIds.cend() && mark.value().time == cmd.time;
        if (cmd.time >= state.consumedThrough && !live)
            result.effects.append(cmd);
    }

    result.state.position = position;
    result.state.consumedThrough = position + 1;
    result.state.consumedLiveEventIds = pendingLiveMarks(state.consumedLiveEventIds, position);
    return result;
}

ShowCommandState ShowCommandFsm::seek(const ShowCommandState &state, quint32 positionMs)
{
    ShowCommandState next = state;
    next.position = qMin(positionMs, ShowCommand::MaxTime);
    next.consumedThrough = next.position;
    // a new pass owes nothing to what was executed live during the previous one
    next.consumedLiveEventIds.clear();
    return next;
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

    if (!isUserOrigin(input.origin))
        return result;

    ShowCommand cmd;
    cmd.id = track.nextEventId();
    cmd.time = state.position;
    cmd.functionId = input.functionId;
    cmd.action = input.action;
    // offered as it came in: a value on a trigger is rejected, not quietly dropped
    cmd.intensity = input.intensity;
    cmd.controlId = input.controlId;
    cmd.role = input.role;
    cmd.attribute = input.attribute;
    cmd.on = input.on;
    cmd.position = input.position;

    if (cmd.id == ShowCommand::InvalidId)
    {
        result.error = QStringLiteral("no free event id left in this track");
        return result;
    }

    result.error = ShowCommand::validate(cmd);
    if (!result.error.isEmpty())
        return result;

    result.authored.append(cmd);
    // the host already executed this input: mark that occurrence, leave the cursor alone
    result.state.consumedLiveEventIds.insert(cmd.id, liveMarkOf(cmd));
    return result;
}

bool ShowCommandFsm::isUserOrigin(ShowCommandOrigin origin)
{
    return origin == ShowCommandOrigin::Pointer || origin == ShowCommandOrigin::Midi ||
           origin == ShowCommandOrigin::Keyboard || origin == ShowCommandOrigin::Osc;
}

ShowCommandFsm::ShowButtonOp ShowCommandFsm::buttonOp(ShowButtonNative native, bool desiredOn)
{
    // Monitoring satisfies neither: On acquires the button's own activation
    if (desiredOn)
        return native == ShowButtonNative::Active ? ShowButtonOp::None : ShowButtonOp::Start;

    return native == ShowButtonNative::Inactive ? ShowButtonOp::None : ShowButtonOp::Stop;
}

bool ShowCommandFsm::sliderTarget(qreal position, qreal low, qreal high, int *value)
{
    if (!std::isfinite(position) || !std::isfinite(low) || !std::isfinite(high))
        return false;
    if (position < 0.0 || position > 1.0 || high <= low)
        return false;

    const qreal intMin = qreal(std::numeric_limits<int>::min());
    const qreal intMax = qreal(std::numeric_limits<int>::max());
    // a bound no int can hold is an unrepresentable range, not something to clamp
    if (low < intMin || high > intMax)
        return false;

    const qreal target = low + position * (high - low);
    if (!std::isfinite(target))
        return false;

    const long long rounded = std::llround(target);
    if (rounded < std::numeric_limits<int>::min() || rounded > std::numeric_limits<int>::max())
        return false;

    *value = int(rounded);
    return true;
}

ShowControlStatus ShowCommandFsm::resolveControl(const ShowCommand &expected,
                                                 const QVector<ShowControlSnapshot> &matches,
                                                 QString *reason)
{
    auto status = [reason](ShowControlStatus result, const QString &why)
    {
        if (reason != nullptr)
            *reason = why;
        return result;
    };

    if (matches.isEmpty())
        return status(ShowControlStatus::Missing, QStringLiteral("no control carries this identity"));
    if (matches.count() > 1)
        return status(ShowControlStatus::Ambiguous,
                      QStringLiteral("%1 controls carry this identity").arg(matches.count()));

    const ShowControlSnapshot &control = matches.first();
    if (control.role != expected.role)
        return status(ShowControlStatus::Incompatible,
                      QStringLiteral("expected %1, found %2")
                          .arg(ShowCommand::roleToString(expected.role),
                               ShowCommand::roleToString(control.role)));
    if (control.attribute != expected.attribute)
        return status(ShowControlStatus::Incompatible,
                      QStringLiteral("expected attribute '%1', found '%2'")
                          .arg(expected.attribute, control.attribute));
    if (!control.enabled)
        return status(ShowControlStatus::Disabled, QStringLiteral("control is disabled"));

    const bool needsFunction = expected.role == ShowControlRole::ToggleButton ||
                               expected.role == ShowControlRole::AdjustSlider;
    if (needsFunction && !control.bound)
        return status(ShowControlStatus::Unbound, QStringLiteral("control has no function"));

    return status(ShowControlStatus::Ready, QString());
}

bool ShowCommandFsm::controlDependsOn(const ShowControlCoupling &next, const ShowControlCoupling &done)
{
    // controls without a Function, like Level sliders, couple only to themselves
    if (next.controlId.isNull() == false && next.controlId == done.controlId)
        return true;
    if (next.functionId != ShowCommand::InvalidId && next.functionId == done.functionId)
        return true;
    if (next.soloGroup != ShowCommand::InvalidId && next.soloGroup == done.soloGroup)
        return true;
    for (const QPair<quint32, quint32> &member : done.memberSoloGroups)
    {
        if (next.soloGroup != ShowCommand::InvalidId && member.second == next.soloGroup)
            return true;
    }
    return done.startsFunctions.contains(next.functionId);
}
