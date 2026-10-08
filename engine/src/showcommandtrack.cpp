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
#include <tuple>
#include <type_traits>

#include "showcommandtrack.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

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
#define KXMLShowCommandPair QStringLiteral("Pair")
#define KXMLShowCommandStateOn QStringLiteral("On")
#define KXMLShowCommandStateOff QStringLiteral("Off")

namespace
{
    constexpr int kControlStateVersion = 2;
    constexpr int kButtonPairVersion = 3;
    constexpr int kSliderResetVersion = 4;
    constexpr int kSliderColorsVersion = 5;
    constexpr int kXYPadPositionVersion = 6;
    constexpr int kAnimationFaderVersion = 7;
    constexpr int kXYPadFloorVersion = 8;
    constexpr int kNativeArgumentsVersion = 9;

    QString actionName(ShowCommandAction action)
    {
        switch (action)
        {
            case ShowCommandAction::Start: return QStringLiteral("Start");
            case ShowCommandAction::Stop: return QStringLiteral("Stop");
            case ShowCommandAction::SetIntensity: return QStringLiteral("SetIntensity");
            case ShowCommandAction::SetButtonState: return QStringLiteral("SetButtonState");
            case ShowCommandAction::SetSliderPosition: return QStringLiteral("SetSliderPosition");
            case ShowCommandAction::SetSliderColors: return QStringLiteral("SetSliderColors");
            case ShowCommandAction::SetSliderReset: return QStringLiteral("SetSliderReset");
            case ShowCommandAction::SetXYPadPosition: return QStringLiteral("SetXYPadPosition");
            case ShowCommandAction::SetAnimationFader: return QStringLiteral("SetAnimationFader");
            case ShowCommandAction::SetXYPadFloor: return QStringLiteral("SetXYPadFloor");
            case ShowCommandAction::SetXYPadRanges: return QStringLiteral("SetXYPadRanges");
            case ShowCommandAction::SetXYPadPositionPreset: return QStringLiteral("SetXYPadPositionPreset");
            case ShowCommandAction::SetXYPadFunctionPreset: return QStringLiteral("SetXYPadFunctionPreset");
            case ShowCommandAction::SetXYPadGroupPreset: return QStringLiteral("SetXYPadGroupPreset");
            case ShowCommandAction::SetAnimationColor: return QStringLiteral("SetAnimationColor");
            case ShowCommandAction::SetAnimationContent: return QStringLiteral("SetAnimationContent");
            case ShowCommandAction::SetSliderChannel: return QStringLiteral("SetSliderChannel");
        }
        return QString();
    }

    QString roleName(ShowControlRole role)
    {
        switch (role)
        {
            case ShowControlRole::None: return QStringLiteral("None");
            case ShowControlRole::ToggleButton: return QStringLiteral("ToggleButton");
            case ShowControlRole::FlashButton: return QStringLiteral("FlashButton");
            case ShowControlRole::BlackoutButton: return QStringLiteral("BlackoutButton");
            case ShowControlRole::FreezeButton: return QStringLiteral("FreezeButton");
            case ShowControlRole::FreezeHoldButton: return QStringLiteral("FreezeHoldButton");
            case ShowControlRole::LevelSlider: return QStringLiteral("LevelSlider");
            case ShowControlRole::AdjustSlider: return QStringLiteral("AdjustSlider");
            case ShowControlRole::SubmasterSlider: return QStringLiteral("SubmasterSlider");
            case ShowControlRole::GrandMasterSlider: return QStringLiteral("GrandMasterSlider");
            case ShowControlRole::XYPad: return QStringLiteral("XYPad");
            case ShowControlRole::AnimationFader: return QStringLiteral("AnimationFader");
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
        return ShowCommand::isControlAction(action);
    }

    bool isSliderRole(ShowControlRole role)
    {
        return role == ShowControlRole::LevelSlider || role == ShowControlRole::AdjustSlider ||
               role == ShowControlRole::SubmasterSlider || role == ShowControlRole::GrandMasterSlider;
    }

    bool isButtonRole(ShowControlRole role)
    {
        return role == ShowControlRole::ToggleButton || role == ShowControlRole::FlashButton ||
               role == ShowControlRole::BlackoutButton || role == ShowControlRole::FreezeButton ||
               role == ShowControlRole::FreezeHoldButton;
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

        if (ShowCommand::hasTypedPayload(cmd.action))
        {
            const bool matrix = cmd.action == ShowCommandAction::SetAnimationColor ||
                                cmd.action == ShowCommandAction::SetAnimationContent;
            const bool channel = cmd.action == ShowCommandAction::SetSliderChannel;
            if (channel ? !isSliderRole(cmd.role)
                        : cmd.role != (matrix ? ShowControlRole::AnimationFader : ShowControlRole::XYPad))
                return QStringLiteral("%1 has an incompatible role").arg(action);
            if (cmd.on || cmd.position != 0.0 || !cmd.attribute.isEmpty() ||
                cmd.pairId != ShowCommand::InvalidId)
                return QStringLiteral("%1 carries unrelated state").arg(action);
            return cmd.payload.validate(cmd.action);
        }
        const bool legacyNative = cmd.action == ShowCommandAction::SetSliderColors ||
                                  cmd.action == ShowCommandAction::SetXYPadPosition ||
                                  cmd.action == ShowCommandAction::SetXYPadFloor;
        if (!legacyNative && cmd.payload.value.index() != 0)
            return QStringLiteral("%1 carries unrelated native arguments").arg(action);
        if (legacyNative && !cmd.attribute.isEmpty())
            return QStringLiteral("%1 carries duplicate or invalid native arguments").arg(action);
        if (cmd.action == ShowCommandAction::SetButtonState)
        {
            if (!isButtonRole(cmd.role) && !isSliderRole(cmd.role))
                return QStringLiteral("%1 needs a toggle button role").arg(action);
            if (!cmd.attribute.isEmpty() || cmd.position != 0.0)
                return QStringLiteral("%1 carries no slider value").arg(action);
            return QString();
        }

        if (cmd.action == ShowCommandAction::SetSliderReset)
        {
            if (!isSliderRole(cmd.role))
                return QStringLiteral("%1 needs a slider role").arg(action);
            if (cmd.on || cmd.position != 0.0)
                return QStringLiteral("%1 carries no state or slider value").arg(action);
            if (cmd.attribute.isEmpty() != (cmd.role != ShowControlRole::AdjustSlider))
                return QStringLiteral("only an adjust slider names a function attribute");
            return QString();
        }

        if (cmd.action == ShowCommandAction::SetXYPadFloor)
        {
            if (cmd.role != ShowControlRole::XYPad)
                return QStringLiteral("%1 needs an XYPad role").arg(action);
            if (cmd.on || cmd.position != 0.0 || cmd.pairId != ShowCommand::InvalidId)
                return QStringLiteral("%1 carries no button state, slider value or pair").arg(action);
            return cmd.payload.validate(cmd.action);
        }

        if (cmd.action == ShowCommandAction::SetXYPadPosition)
        {
            if (cmd.role != ShowControlRole::XYPad)
                return QStringLiteral("%1 needs an XYPad role").arg(action);
            if (cmd.on || cmd.position != 0.0)
                return QStringLiteral("%1 carries no button state or slider value").arg(action);
            return cmd.payload.validate(cmd.action);
        }

        if (cmd.action == ShowCommandAction::SetAnimationFader)
        {
            if (cmd.role != ShowControlRole::AnimationFader)
                return QStringLiteral("%1 needs an AnimationFader role").arg(action);
            if (cmd.on || !cmd.attribute.isEmpty())
                return QStringLiteral("%1 carries no button state or attribute payload").arg(action);
            if (!std::isfinite(cmd.position))
                return QStringLiteral("position is not a finite number");
            if (cmd.position < 0.0 || cmd.position > 1.0)
                return QStringLiteral("position outside 0..1");
            return QString();
        }

        if (cmd.action == ShowCommandAction::SetSliderColors)
        {
            if (!isSliderRole(cmd.role))
                return QStringLiteral("%1 needs a slider role").arg(action);
            if (cmd.on)
                return QStringLiteral("%1 carries no button state").arg(action);
            const QString invalid = cmd.payload.validate(cmd.action);
            if (!invalid.isEmpty())
                return invalid;
            if (!std::isfinite(cmd.position))
                return QStringLiteral("position is not a finite number");
            if (cmd.position < 0.0 || cmd.position > 1.0)
                return QStringLiteral("position outside 0..1");
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
        if (attrs.hasAttribute(KXMLShowCommandPair))
            return QStringLiteral("%1 carries no pair id").arg(actionName(cmd->action));

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
    QString readControlState(const QXmlStreamAttributes &attrs, int version, ShowCommand *cmd)
    {
        const QString action = ShowCommand::actionToString(cmd->action);

        if (cmd->action != ShowCommandAction::SetButtonState && attrs.hasAttribute(KXMLShowCommandPair))
            return QStringLiteral("%1 carries no pair id").arg(action);

        if (attrs.hasAttribute(KXMLShowCommandFunction))
            return QStringLiteral("%1 carries a function target").arg(action);

        cmd->controlId = ShowCommand::controlIdFromString(attrs.value(KXMLShowCommandControl).toString());
        if (cmd->controlId.isNull())
            return QStringLiteral("invalid control target '%1'")
                .arg(attrs.value(KXMLShowCommandControl).toString());

        if (!ShowCommand::roleFromString(attrs.value(KXMLShowCommandRole).toString(), &cmd->role))
            return QStringLiteral("unknown role '%1'").arg(attrs.value(KXMLShowCommandRole).toString());
        if (version < kButtonPairVersion && cmd->action == ShowCommandAction::SetButtonState &&
            cmd->role != ShowControlRole::ToggleButton)
        {
            return QStringLiteral("%1 needs command track version 3")
                .arg(ShowCommand::roleToString(cmd->role));
        }
        if (version < kSliderResetVersion && cmd->action == ShowCommandAction::SetSliderReset)
            return QStringLiteral("%1 needs command track version %2")
                .arg(action).arg(kSliderResetVersion);
        if (version < kSliderColorsVersion && cmd->action == ShowCommandAction::SetSliderColors)
            return QStringLiteral("%1 needs command track version %2")
                .arg(action).arg(kSliderColorsVersion);
        if (version < kXYPadPositionVersion && cmd->action == ShowCommandAction::SetXYPadPosition)
            return QStringLiteral("%1 needs command track version %2")
                .arg(action).arg(kXYPadPositionVersion);
        if (version < kAnimationFaderVersion && cmd->action == ShowCommandAction::SetAnimationFader)
            return QStringLiteral("%1 needs command track version %2")
                .arg(action).arg(kAnimationFaderVersion);
        if (version < kXYPadFloorVersion && cmd->action == ShowCommandAction::SetXYPadFloor)
            return QStringLiteral("%1 needs command track version %2")
                .arg(action).arg(kXYPadFloorVersion);

        if (ShowCommand::hasTypedPayload(cmd->action))
        {
            if (version < kNativeArgumentsVersion)
                return QStringLiteral("%1 needs command track version 9").arg(action);
            if (attrs.hasAttribute(KXMLShowCommandState) || attrs.hasAttribute(KXMLShowCommandValue) ||
                attrs.hasAttribute(KXMLShowCommandAttribute) || attrs.hasAttribute(KXMLShowCommandPair))
                return QStringLiteral("%1 carries unrelated state").arg(action);
            QString reason;
            if (!ShowCommandPayload::decode(cmd->action, attrs.value("Arguments").toString(),
                                            &cmd->payload, &reason))
                return reason;
            return QString();
        }
        if (attrs.hasAttribute("Arguments"))
            return QStringLiteral("%1 carries unrelated native arguments").arg(action);
        if (cmd->action == ShowCommandAction::SetButtonState)
        {
            if (attrs.hasAttribute(KXMLShowCommandValue) || attrs.hasAttribute(KXMLShowCommandAttribute))
                return QStringLiteral("%1 carries no slider value").arg(action);

            const QString state = attrs.value(KXMLShowCommandState).toString();
            if (state != KXMLShowCommandStateOn && state != KXMLShowCommandStateOff)
                return QStringLiteral("invalid state '%1'").arg(state);
            cmd->on = state == KXMLShowCommandStateOn;
            if (attrs.hasAttribute(KXMLShowCommandPair))
            {
                if (version < kButtonPairVersion)
                    return QStringLiteral("pair id needs command track version 3");
                if (!readUInt(attrs, KXMLShowCommandPair, &cmd->pairId) ||
                    cmd->pairId == ShowCommand::InvalidId)
                    return QStringLiteral("invalid pair id '%1'")
                        .arg(attrs.value(KXMLShowCommandPair).toString());
            }
            return QString();
        }

        if (cmd->action == ShowCommandAction::SetSliderReset)
        {
            if (attrs.hasAttribute(KXMLShowCommandValue) || attrs.hasAttribute(KXMLShowCommandState))
                return QStringLiteral("%1 carries no slider value or button state").arg(action);
            cmd->attribute = attrs.value(KXMLShowCommandAttribute).toString();
            return QString();
        }

        if (cmd->action == ShowCommandAction::SetSliderColors)
        {
            if (attrs.hasAttribute(KXMLShowCommandState))
                return QStringLiteral("%1 carries no button state").arg(action);

            bool ok = false;
            cmd->position = attrs.value(KXMLShowCommandValue).toDouble(&ok);
            if (!ok)
                return QStringLiteral("invalid value '%1'").arg(attrs.value(KXMLShowCommandValue).toString());
            cmd->attribute = attrs.value(KXMLShowCommandAttribute).toString();
            if (cmd->attribute.isEmpty())
                return QStringLiteral("missing color payload");
            return QString();
        }

        if (cmd->action == ShowCommandAction::SetXYPadPosition ||
            cmd->action == ShowCommandAction::SetXYPadFloor)
        {
            if (attrs.hasAttribute(KXMLShowCommandState) || attrs.hasAttribute(KXMLShowCommandValue))
                return QStringLiteral("%1 carries no state or slider value").arg(action);
            cmd->attribute = attrs.value(KXMLShowCommandAttribute).toString();
            if (cmd->attribute.isEmpty())
                return QStringLiteral("missing XY pad payload");
            return QString();
        }

        if (cmd->action == ShowCommandAction::SetAnimationFader)
        {
            if (attrs.hasAttribute(KXMLShowCommandState) || attrs.hasAttribute(KXMLShowCommandAttribute))
                return QStringLiteral("%1 carries no state or attribute payload").arg(action);
            bool ok = false;
            cmd->position = attrs.value(KXMLShowCommandValue).toDouble(&ok);
            if (!ok)
                return QStringLiteral("invalid value '%1'").arg(attrs.value(KXMLShowCommandValue).toString());
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

    bool isMomentaryHoldRole(ShowControlRole role)
    {
        return role == ShowControlRole::FlashButton || role == ShowControlRole::FreezeHoldButton ||
               role == ShowControlRole::LevelSlider || role == ShowControlRole::AdjustSlider ||
               role == ShowControlRole::SubmasterSlider || role == ShowControlRole::GrandMasterSlider;
    }

    quint32 nextPairId(const ShowCommandTrack &track)
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

    quint32 activePairId(const ShowCommandTrack &track, const QUuid &controlId, ShowControlRole role)
    {
        quint32 open = ShowCommand::InvalidId;
        for (const ShowCommand &cmd : track.commands())
        {
            if (cmd.action != ShowCommandAction::SetButtonState || cmd.role != role || cmd.controlId != controlId ||
                cmd.pairId == ShowCommand::InvalidId)
            {
                continue;
            }
            if (cmd.on)
                open = cmd.pairId;
            else if (open == cmd.pairId)
                open = ShowCommand::InvalidId;
        }
        return open;
    }

    QString validatePairedHoldStructure(const ShowCommandTrack &track)
    {
        QHash<quint32, QVector<const ShowCommand *>> edgesByPair;
        for (const ShowCommand &cmd : track.commands())
        {
            if (cmd.action == ShowCommandAction::SetButtonState && cmd.pairId != ShowCommand::InvalidId)
                edgesByPair[cmd.pairId].append(&cmd);
        }

        for (auto it = edgesByPair.cbegin(); it != edgesByPair.cend(); ++it)
        {
            const QVector<const ShowCommand *> edges = it.value();
            if (edges.count() != 2 || edges.at(0)->controlId != edges.at(1)->controlId ||
                edges.at(0)->role != edges.at(1)->role || edges.at(0)->on == edges.at(1)->on)
            {
                return QStringLiteral("malformed hold pair '%1'").arg(it.key());
            }
        }
        return QString();
    }
}

/************************************************************************
 * ShowCommand
 ***********************************************************************/

bool ShowCommandColors::decode(const QString &payload, ShowCommandColors *value)
{
    const auto groups = payload.split(QLatin1Char(';'));
    if (groups.size() != 2)
        return false;
    const auto rgb = groups.at(0).split(QLatin1Char(','));
    const auto secondary = groups.at(1).split(QLatin1Char(','));
    if (rgb.size() != 3 || secondary.size() != 3)
        return false;
    ShowCommandColors parsed;
    quint8 *components[] = {&parsed.red, &parsed.green, &parsed.blue,
                            &parsed.white, &parsed.amber, &parsed.ultraviolet};
    for (int i = 0; i < 6; ++i)
    {
        bool ok = false;
        const int component = (i < 3 ? rgb.at(i) : secondary.at(i - 3)).toInt(&ok);
        if (!ok || component < 0 || component > 255)
            return false;
        *components[i] = quint8(component);
    }
    if (value != nullptr)
        *value = parsed;
    return true;
}

QString ShowCommandColors::encode() const
{
    return QStringLiteral("%1,%2,%3;%4,%5,%6")
        .arg(red).arg(green).arg(blue).arg(white).arg(amber).arg(ultraviolet);
}

bool ShowCommandPanTilt::decode(const QString &payload, ShowCommandPanTilt *value)
{
    const auto axes = payload.split(QLatin1Char(','));
    if (axes.size() != 2)
        return false;
    bool panOk = false, tiltOk = false;
    ShowCommandPanTilt parsed;
    parsed.pan = QLocale::c().toDouble(axes.at(0), &panOk);
    parsed.tilt = QLocale::c().toDouble(axes.at(1), &tiltOk);
    if (!panOk || !tiltOk || !std::isfinite(parsed.pan) || !std::isfinite(parsed.tilt) ||
        parsed.pan < 0.0 || parsed.pan > 255.0 || parsed.tilt < 0.0 || parsed.tilt > 255.0)
        return false;
    if (value != nullptr)
        *value = parsed;
    return true;
}

QString ShowCommandPanTilt::encode() const
{
    return intensityToString(pan) + QLatin1Char(',') + intensityToString(tilt);
}

bool ShowCommandFloor::decode(const QString &payload, ShowCommandFloor *value)
{
    const auto axes = payload.split(QLatin1Char(','));
    if (axes.size() != 3)
        return false;
    ShowCommandFloor parsed;
    qreal *coordinates[] = {&parsed.x, &parsed.y, &parsed.z};
    for (int i = 0; i < 3; ++i)
    {
        bool ok = false;
        const qreal coordinate = QLocale::c().toDouble(axes.at(i), &ok);
        if (!ok || !std::isfinite(coordinate) || coordinate < 0.0)
            return false;
        *coordinates[i] = coordinate;
    }
    if (value != nullptr)
        *value = parsed;
    return true;
}

QString ShowCommandFloor::encode() const
{
    return intensityToString(x) + QLatin1Char(',') + intensityToString(y) +
           QLatin1Char(',') + intensityToString(z);
}

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

ShowCommand ShowCommand::setSliderColors(quint32 id, quint32 time, const QUuid &controlId,
                                         ShowControlRole role, const QString &payload,
                                         qreal position)
{
    ShowCommand cmd;
    cmd.id = id;
    cmd.time = time;
    cmd.action = ShowCommandAction::SetSliderColors;
    cmd.controlId = controlId;
    cmd.role = role;
    cmd.attribute = payload;
    cmd.position = position;
    return cmd.canonicalized();
}

ShowCommand ShowCommand::setSliderReset(quint32 id, quint32 time, const QUuid &controlId,
                                        ShowControlRole role, const QString &attribute)
{
    ShowCommand cmd = start(id, time, InvalidId);
    cmd.action = ShowCommandAction::SetSliderReset;
    cmd.controlId = controlId;
    cmd.role = role;
    cmd.attribute = attribute;
    return cmd;
}

ShowCommand ShowCommand::setXYPadPosition(quint32 id, quint32 time, const QUuid &controlId,
                                          const QString &payload)
{
    ShowCommand cmd = start(id, time, InvalidId);
    cmd.action = ShowCommandAction::SetXYPadPosition;
    cmd.controlId = controlId;
    cmd.role = ShowControlRole::XYPad;
    cmd.attribute = payload;
    return cmd.canonicalized();
}

ShowCommand ShowCommand::setAnimationFader(quint32 id, quint32 time, const QUuid &controlId,
                                           qreal position)
{
    ShowCommand cmd;
    cmd.id = id;
    cmd.time = time;
    cmd.action = ShowCommandAction::SetAnimationFader;
    cmd.controlId = controlId;
    cmd.role = ShowControlRole::AnimationFader;
    cmd.position = position;
    return cmd;
}

ShowCommand ShowCommand::canonicalized() const
{
    ShowCommand result = *this;
    if (payload.value.index() == 0 && (action == ShowCommandAction::SetSliderColors ||
        action == ShowCommandAction::SetXYPadPosition || action == ShowCommandAction::SetXYPadFloor))
    {
        if (ShowCommandPayload::decode(action, attribute, &result.payload))
            result.attribute.clear();
    }
    return result;
}

QString ShowCommand::nativeArgument() const
{
    return payload.value.index() == 0 ? attribute : payload.encode(action);
}

QString ShowCommand::validate(const ShowCommand &original)
{
    const ShowCommand cmd = original.canonicalized();
    if (actionName(cmd.action).isEmpty())
        return QStringLiteral("unknown action %1").arg(int(cmd.action));

    if (cmd.id == InvalidId)
        return QStringLiteral("invalid event id");

    if (cmd.action != ShowCommandAction::SetButtonState && cmd.pairId != InvalidId)
        return QStringLiteral("%1 carries no pair id").arg(actionName(cmd.action));

    if (isControlAction(cmd.action))
        return validateControlState(cmd);

    if (cmd.functionId == InvalidId)
        return QStringLiteral("invalid function target");

    if (!cmd.controlId.isNull() || cmd.role != ShowControlRole::None || !cmd.attribute.isEmpty()
        || cmd.on || cmd.position != 0.0 || cmd.payload.value.index() != 0)
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

bool ShowCommandPayload::decode(ShowCommandAction action, const QString &text,
                                ShowCommandPayload *out, QString *reason)
{
    ShowCommandPayload legacy;
    bool isLegacy = true;
    bool validLegacy = false;
    if (action == ShowCommandAction::SetSliderColors)
    {
        ShowCommandColors colors;
        validLegacy = ShowCommandColors::decode(text, &colors);
        legacy.value = colors;
    }
    else if (action == ShowCommandAction::SetXYPadPosition)
    {
        ShowCommandPanTilt point;
        validLegacy = ShowCommandPanTilt::decode(text, &point);
        legacy.value = point;
    }
    else if (action == ShowCommandAction::SetXYPadFloor)
    {
        ShowCommandFloor point;
        validLegacy = ShowCommandFloor::decode(text, &point);
        legacy.value = point;
    }
    else
        isLegacy = false;
    if (isLegacy)
    {
        if (!validLegacy)
            return fail(reason, QStringLiteral("invalid native value"));
        if (out != nullptr)
            *out = legacy;
        return succeed(reason);
    }
    const QJsonDocument document = QJsonDocument::fromJson(text.toUtf8());
    if (!document.isObject())
        return fail(reason, QStringLiteral("native arguments must be an object"));
    const QJsonObject object = document.object();
    const auto keys = [](const QJsonObject &value, const QStringList &required,
                         const QStringList &optional = QStringList()) {
        for (const QString &key : required)
            if (!value.contains(key))
                return false;
        for (const QString &key : value.keys())
            if (!required.contains(key) && !optional.contains(key))
                return false;
        return true;
    };
    const auto integer = [](const QJsonValue &value, int *target) {
        if (!value.isDouble() || !std::isfinite(value.toDouble()) ||
            value.toDouble() != std::floor(value.toDouble()) ||
            value.toDouble() < INT_MIN || value.toDouble() > INT_MAX)
            return false;
        *target = int(value.toDouble());
        return true;
    };
    const auto point = [](const QJsonValue &value, ShowCommandPanTilt *target) {
        if (!value.isArray())
            return false;
        const auto array = value.toArray();
        if (array.count() != 2 || !array[0].isDouble() || !array[1].isDouble())
            return false;
        target->pan = array[0].toDouble();
        target->tilt = array[1].toDouble();
        return true;
    };
    ShowCommandPayload candidate;
    bool valid = false;
    if (action == ShowCommandAction::SetSliderChannel)
    {
        ShowCommandChannel channel;
        valid = keys(object, {"value", "binding"}) && integer(object["value"], &channel.value) &&
                object["binding"].isString();
        channel.binding = object["binding"].toString();
        candidate.value = channel;
    }
    else if (action == ShowCommandAction::SetXYPadRanges)
    {
        ShowCommandRanges ranges;
        valid = keys(object, {"horizontal", "vertical"}) &&
                point(object["horizontal"], &ranges.horizontal) &&
                point(object["vertical"], &ranges.vertical);
        candidate.value = ranges;
    }
    else if (action == ShowCommandAction::SetXYPadPositionPreset ||
             action == ShowCommandAction::SetXYPadFunctionPreset ||
             action == ShowCommandAction::SetXYPadGroupPreset)
    {
        ShowCommandChoice choice;
        const bool position = action == ShowCommandAction::SetXYPadPositionPreset;
        valid = keys(object, position ? QStringList{"choice", "active", "point"}
                                     : QStringList{"choice", "active"}) &&
                integer(object["choice"], &choice.choice) && object["active"].isBool();
        choice.active = object["active"].toBool();
        if (position)
            valid = valid && point(object["point"], &choice.point);
        candidate.value = choice;
    }
    else if (action == ShowCommandAction::SetAnimationColor)
    {
        ShowCommandMatrixColor color;
        const QString operation = object["operation"].toString();
        valid = object["operation"].isString() && integer(object["index"], &color.index);
        if (operation == "replace")
        {
            color.operation = ShowCommandMatrixColor::Operation::Replace;
            valid = valid && keys(object, {"index", "operation", "rgb"}, {"choice"}) &&
                    object["rgb"].isArray();
            const QJsonArray rgb = object["rgb"].toArray();
            int red = 0, green = 0, blue = 0;
            valid = valid && rgb.count() == 3 && integer(rgb[0], &red) &&
                    integer(rgb[1], &green) && integer(rgb[2], &blue) &&
                    red >= 0 && red <= 255 && green >= 0 && green <= 255 && blue >= 0 && blue <= 255;
            color.color = {quint8(red), quint8(green), quint8(blue), 0, 0, 0};
        }
        else if (operation == "reset")
        {
            color.operation = ShowCommandMatrixColor::Operation::Reset;
            valid = valid && keys(object, {"index", "operation"});
        }
        else if (operation == "component")
        {
            color.operation = ShowCommandMatrixColor::Operation::Component;
            valid = valid && keys(object, {"index", "operation", "component", "value"}, {"choice"}) &&
                    integer(object["component"], &color.component) &&
                    integer(object["value"], &color.value);
        }
        else
            valid = false;
        if (object.contains("choice"))
            valid = valid && integer(object["choice"], &color.choice);
        candidate.value = color;
    }
    else if (action == ShowCommandAction::SetAnimationContent)
    {
        ShowCommandContent content;
        valid = keys(object, {"algorithm", "text", "properties"}, {"choice"}) &&
                object["algorithm"].isString() && object["text"].isString() &&
                object["properties"].isArray();
        content.algorithm = object["algorithm"].toString();
        content.text = object["text"].toString();
        if (object.contains("choice"))
            valid = valid && integer(object["choice"], &content.choice);
        for (const QJsonValue &entry : object["properties"].toArray())
        {
            const QJsonObject property = entry.toObject();
            const QString name = property["name"].toString(), type = property["type"].toString();
            ShowCommandProperty value;
            valid = valid && entry.isObject() && keys(property, {"name", "type", "value"}) &&
                    property["name"].isString() && !name.isEmpty() && !content.properties.contains(name);
            if (type == "List" || type == "String")
            {
                value.type = type == "List" ? ShowCommandProperty::Type::List : ShowCommandProperty::Type::String;
                valid = valid && property["value"].isString();
                value.text = property["value"].toString();
            }
            else if (type == "Range" || type == "Float")
            {
                value.type = type == "Range" ? ShowCommandProperty::Type::Range : ShowCommandProperty::Type::Float;
                valid = valid && property["value"].isDouble();
                value.number = property["value"].toDouble();
            }
            else
                valid = false;
            content.properties.insert(name, value);
        }
        candidate.value = content;
    }
    const QString error = candidate.validate(action);
    if (!valid || !error.isEmpty())
        return fail(reason, error.isEmpty() ? QStringLiteral("invalid native argument fields") : error);
    if (out != nullptr)
        *out = candidate;
    return succeed(reason);
}

QString ShowCommandPayload::validate(ShowCommandAction action) const
{
    const auto point = [](const ShowCommandPanTilt &value) {
        return std::isfinite(value.pan) && std::isfinite(value.tilt) &&
               value.pan >= 0 && value.pan <= 255 && value.tilt >= 0 && value.tilt <= 255;
    };
    if (action == ShowCommandAction::SetSliderColors)
        return std::holds_alternative<ShowCommandColors>(value)
            ? QString() : QStringLiteral("color needs six integer components in 0..255");
    if (action == ShowCommandAction::SetXYPadPosition)
    {
        const auto *position = std::get_if<ShowCommandPanTilt>(&value);
        return position != nullptr && point(*position)
            ? QString() : QStringLiteral("position needs finite Pan/Tilt coordinates in 0..255");
    }
    if (action == ShowCommandAction::SetXYPadFloor)
    {
        const auto *point = std::get_if<ShowCommandFloor>(&value);
        return point != nullptr && std::isfinite(point->x) && std::isfinite(point->y) &&
               std::isfinite(point->z) && point->x >= 0 && point->y >= 0 && point->z >= 0
            ? QString() : QStringLiteral("floor needs finite nonnegative metre X/Y/Z coordinates");
    }
    if (action == ShowCommandAction::SetSliderChannel)
    {
        const auto *channel = std::get_if<ShowCommandChannel>(&value);
        return channel != nullptr && channel->value >= 0 && channel->value <= 255
            ? QString() : QStringLiteral("channel value needs an integer in 0..255");
    }
    if (action == ShowCommandAction::SetXYPadRanges)
    {
        const auto *ranges = std::get_if<ShowCommandRanges>(&value);
        if (ranges != nullptr && point(ranges->horizontal) && point(ranges->vertical))
            return QString();
        return QStringLiteral("range endpoints must be finite native values in 0..255");
    }
    if (action == ShowCommandAction::SetXYPadPositionPreset ||
        action == ShowCommandAction::SetXYPadFunctionPreset ||
        action == ShowCommandAction::SetXYPadGroupPreset)
    {
        const auto *choice = std::get_if<ShowCommandChoice>(&value);
        if (choice != nullptr && choice->choice >= 0 && choice->choice <= 255 &&
            (action == ShowCommandAction::SetXYPadPositionPreset ? point(choice->point)
                : choice->point.pan == 0 && choice->point.tilt == 0))
            return QString();
        return QStringLiteral("invalid native choice or position");
    }
    if (action == ShowCommandAction::SetAnimationColor)
    {
        const auto *color = std::get_if<ShowCommandMatrixColor>(&value);
        if (color == nullptr || color->index < 0 || color->index > 4 ||
            color->choice < -1 || color->choice > 255 || color->color.white != 0 ||
            color->color.amber != 0 || color->color.ultraviolet != 0)
            return QStringLiteral("invalid Matrix color slot or choice");
        const bool black = color->color.red == 0 && color->color.green == 0 && color->color.blue == 0;
        if ((color->operation == ShowCommandMatrixColor::Operation::Replace &&
             color->component == -1 && color->value == 0) ||
            (color->operation == ShowCommandMatrixColor::Operation::Reset && black &&
             color->component == -1 && color->value == 0 && color->choice == -1) ||
            (color->operation == ShowCommandMatrixColor::Operation::Component && black &&
             color->component >= 0 && color->component <= 2 && color->value >= 0 && color->value <= 255))
            return QString();
        return QStringLiteral("invalid Matrix color operation arguments");
    }
    if (action == ShowCommandAction::SetAnimationContent)
    {
        const auto *content = std::get_if<ShowCommandContent>(&value);
        if (content == nullptr || content->algorithm.isEmpty() || content->choice < -1 || content->choice > 255)
            return QStringLiteral("content needs a stable algorithm name");
        for (auto it = content->properties.cbegin(); it != content->properties.cend(); ++it)
        {
            const auto &property = it.value();
            if (it.key().isEmpty())
                return QStringLiteral("content needs named properties");
            switch (property.type)
            {
                case ShowCommandProperty::Type::List:
                case ShowCommandProperty::Type::String:
                    if (property.number != 0)
                        return QStringLiteral("string property carries a numeric argument");
                    break;
                case ShowCommandProperty::Type::Range:
                case ShowCommandProperty::Type::Float:
                    if (!property.text.isEmpty() || !std::isfinite(property.number) ||
                        (property.type == ShowCommandProperty::Type::Range &&
                         property.number != std::floor(property.number)))
                        return QStringLiteral("invalid numeric property argument");
                    break;
                default: return QStringLiteral("unknown property type");
            }
        }
        return QString();
    }
    return value.index() == 0 ? QString() : QStringLiteral("unrelated native arguments");
}

QString ShowCommandPayload::encode(ShowCommandAction action) const
{
    if (const auto *colors = std::get_if<ShowCommandColors>(&value))
        return colors->encode();
    if (const auto *position = std::get_if<ShowCommandPanTilt>(&value))
        return position->encode();
    if (const auto *floor = std::get_if<ShowCommandFloor>(&value))
        return floor->encode();
    QJsonObject object;
    const auto point = [](const ShowCommandPanTilt &value) { return QJsonArray{value.pan, value.tilt}; };
    if (const auto *channel = std::get_if<ShowCommandChannel>(&value))
        object = {{"value", channel->value}, {"binding", channel->binding}};
    else if (const auto *ranges = std::get_if<ShowCommandRanges>(&value))
        object = {{"horizontal", point(ranges->horizontal)}, {"vertical", point(ranges->vertical)}};
    else if (const auto *choice = std::get_if<ShowCommandChoice>(&value))
    {
        object = {{"choice", choice->choice}, {"active", choice->active}};
        if (action == ShowCommandAction::SetXYPadPositionPreset)
            object["point"] = point(choice->point);
    }
    else if (const auto *color = std::get_if<ShowCommandMatrixColor>(&value))
    {
        object["index"] = color->index;
        if (color->operation == ShowCommandMatrixColor::Operation::Replace)
        {
            object["operation"] = "replace";
            object["rgb"] = QJsonArray{color->color.red, color->color.green, color->color.blue};
        }
        else if (color->operation == ShowCommandMatrixColor::Operation::Reset)
            object["operation"] = "reset";
        else
        {
            object["operation"] = "component";
            object["component"] = color->component;
            object["value"] = color->value;
        }
        if (color->choice >= 0)
            object["choice"] = color->choice;
    }
    else if (const auto *content = std::get_if<ShowCommandContent>(&value))
    {
        object = {{"algorithm", content->algorithm}, {"text", content->text}};
        if (content->choice >= 0)
            object["choice"] = content->choice;
        QJsonArray properties;
        for (auto it = content->properties.cbegin(); it != content->properties.cend(); ++it)
        {
            const auto &property = it.value();
            const bool numeric = property.type == ShowCommandProperty::Type::Range ||
                                 property.type == ShowCommandProperty::Type::Float;
            const QString type = property.type == ShowCommandProperty::Type::List ? "List"
                : property.type == ShowCommandProperty::Type::Range ? "Range"
                : property.type == ShowCommandProperty::Type::Float ? "Float" : "String";
            properties.append(QJsonObject{{"name", it.key()}, {"type", type},
                {"value", numeric ? QJsonValue(property.number) : QJsonValue(property.text)}});
        }
        object["properties"] = properties;
    }
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}

bool ShowCommandPayload::operator==(const ShowCommandPayload &other) const
{
    if (value.index() != other.value.index())
        return false;
    return std::visit([&](const auto &own) {
        using T = std::decay_t<decltype(own)>;
        const auto &compared = std::get<T>(other.value);
        if constexpr (std::is_same_v<T, std::monostate>)
            return true;
        else if constexpr (std::is_same_v<T, ShowCommandColors>)
            return std::tie(own.red, own.green, own.blue, own.white, own.amber, own.ultraviolet) ==
                   std::tie(compared.red, compared.green, compared.blue, compared.white,
                            compared.amber, compared.ultraviolet);
        else if constexpr (std::is_same_v<T, ShowCommandPanTilt>)
            return std::tie(own.pan, own.tilt) == std::tie(compared.pan, compared.tilt);
        else if constexpr (std::is_same_v<T, ShowCommandFloor>)
            return std::tie(own.x, own.y, own.z) == std::tie(compared.x, compared.y, compared.z);
        else if constexpr (std::is_same_v<T, ShowCommandRanges>)
            return std::tie(own.horizontal.pan, own.horizontal.tilt, own.vertical.pan, own.vertical.tilt) ==
                   std::tie(compared.horizontal.pan, compared.horizontal.tilt,
                            compared.vertical.pan, compared.vertical.tilt);
        else if constexpr (std::is_same_v<T, ShowCommandChoice>)
            return std::tie(own.choice, own.active, own.point.pan, own.point.tilt) ==
                   std::tie(compared.choice, compared.active, compared.point.pan, compared.point.tilt);
        else if constexpr (std::is_same_v<T, ShowCommandMatrixColor>)
            return std::tie(own.index, own.operation, own.color.red, own.color.green, own.color.blue,
                            own.color.white, own.color.amber, own.color.ultraviolet,
                            own.component, own.value, own.choice) ==
                   std::tie(compared.index, compared.operation, compared.color.red, compared.color.green,
                            compared.color.blue, compared.color.white, compared.color.amber,
                            compared.color.ultraviolet, compared.component, compared.value, compared.choice);
        else if constexpr (std::is_same_v<T, ShowCommandChannel>)
            return std::tie(own.value, own.binding) == std::tie(compared.value, compared.binding);
        else
        {
            if (std::tie(own.algorithm, own.text, own.choice) !=
                std::tie(compared.algorithm, compared.text, compared.choice) ||
                own.properties.keys() != compared.properties.keys())
                return false;
            for (auto it = own.properties.cbegin(); it != own.properties.cend(); ++it)
            {
                const auto &property = it.value(), &otherProperty = compared.properties[it.key()];
                if (std::tie(property.type, property.text, property.number) !=
                    std::tie(otherProperty.type, otherProperty.text, otherProperty.number))
                    return false;
            }
            return true;
        }
    }, value);
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
                                        ShowCommandAction::SetSliderPosition,
                                        ShowCommandAction::SetSliderColors,
                                        ShowCommandAction::SetSliderReset,
                                        ShowCommandAction::SetXYPadPosition,
                                        ShowCommandAction::SetAnimationFader,
                                        ShowCommandAction::SetXYPadFloor,
                                        ShowCommandAction::SetXYPadRanges,
                                        ShowCommandAction::SetXYPadPositionPreset,
                                        ShowCommandAction::SetXYPadFunctionPreset,
                                        ShowCommandAction::SetXYPadGroupPreset,
                                        ShowCommandAction::SetAnimationColor,
                                        ShowCommandAction::SetAnimationContent,
                                        ShowCommandAction::SetSliderChannel})
    {
        if (actionName(candidate) != name)
            continue;

        if (action != nullptr)
            *action = candidate;
        return true;
    }
    return false;
}

bool ShowCommand::hasTypedPayload(ShowCommandAction action)
{
    return action >= ShowCommandAction::SetXYPadRanges &&
           action <= ShowCommandAction::SetSliderChannel;
}

bool ShowCommand::isControlAction(ShowCommandAction action)
{
    return action >= ShowCommandAction::SetButtonState &&
           action <= ShowCommandAction::SetSliderChannel;
}

QString ShowCommand::roleToString(ShowControlRole role)
{
    return roleName(role);
}

bool ShowCommand::roleFromString(const QString &name, ShowControlRole *role)
{
    for (ShowControlRole candidate : {ShowControlRole::None, ShowControlRole::ToggleButton,
                                      ShowControlRole::FlashButton, ShowControlRole::BlackoutButton,
                                      ShowControlRole::FreezeButton, ShowControlRole::FreezeHoldButton,
                                      ShowControlRole::LevelSlider, ShowControlRole::AdjustSlider,
                                      ShowControlRole::SubmasterSlider,
                                      ShowControlRole::GrandMasterSlider,
                                      ShowControlRole::XYPad,
                                      ShowControlRole::AnimationFader})
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
    const auto own = canonicalized(), compared = other.canonicalized();
    return id == other.id && time == other.time && functionId == other.functionId
        && action == other.action && intensity == other.intensity
        && controlId == other.controlId && role == other.role && own.attribute == compared.attribute
        && on == other.on && position == other.position && pairId == other.pairId
        && own.payload == compared.payload;
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
    QHash<quint32, int> holdPairGroups;
    for (int i = 0; i < m_commands.count(); ++i)
    {
        const ShowCommand &command = m_commands.at(i);
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
        if (command.action == ShowCommandAction::SetButtonState && command.pairId != ShowCommand::InvalidId)
        {
            const auto it = holdPairGroups.constFind(command.pairId);
            if (it != holdPairGroups.constEnd())
            {
                ShowCommandGroup &group = result[it.value()];
                if (group.action == ShowCommandAction::SetButtonState &&
                    group.controlId == command.controlId && group.role == command.role)
                {
                    group.eventIds.append(command.id);
                    group.startTime = qMin(group.startTime, command.time);
                    group.endTime = qMax(group.endTime, command.time);
                    continue;
                }
            }
        }
        result.append({ { command.id }, command.action, command.controlId, command.role,
                        command.attribute, command.time, command.time });
        if (command.action == ShowCommandAction::SetButtonState && command.pairId != ShowCommand::InvalidId)
            holdPairGroups.insert(command.pairId, result.size() - 1);
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

bool ShowCommandTrack::place(const ShowCommand &original, QString *error)
{
    const ShowCommand cmd = original.canonicalized();
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

bool ShowCommandTrack::replace(const ShowCommand &original, QString *error)
{
    const ShowCommand cmd = original.canonicalized();
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
        (int(version) != LegacyVersion && int(version) != kControlStateVersion &&
         int(version) != kButtonPairVersion && int(version) != kSliderResetVersion &&
         int(version) != kSliderColorsVersion && int(version) != kXYPadPositionVersion &&
         int(version) != kAnimationFaderVersion && int(version) != kXYPadFloorVersion &&
         int(version) != kNativeArgumentsVersion))
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
            reason = readControlState(cmdAttrs, int(version), &cmd);

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
    const QString pairError = validatePairedHoldStructure(parsed);
    if (!pairError.isEmpty())
        return fail(error, pairError);

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
    const bool requiresV4 = std::any_of(m_commands.cbegin(), m_commands.cend(),
                                        [](const ShowCommand &cmd)
                                        { return cmd.action == ShowCommandAction::SetSliderReset; });
    const bool requiresV5 = std::any_of(m_commands.cbegin(), m_commands.cend(),
                                        [](const ShowCommand &cmd)
                                        { return cmd.action == ShowCommandAction::SetSliderColors; });
    const bool requiresV6 = std::any_of(m_commands.cbegin(), m_commands.cend(),
                                        [](const ShowCommand &cmd)
                                        { return cmd.action == ShowCommandAction::SetXYPadPosition; });
    const bool requiresV7 = std::any_of(m_commands.cbegin(), m_commands.cend(),
                                        [](const ShowCommand &cmd)
                                        { return cmd.action == ShowCommandAction::SetAnimationFader; });
    const bool requiresV8 = std::any_of(m_commands.cbegin(), m_commands.cend(),
                                        [](const ShowCommand &cmd)
                                        { return cmd.action == ShowCommandAction::SetXYPadFloor; });
    const bool requiresV9 = std::any_of(m_commands.cbegin(), m_commands.cend(),
                                        [](const ShowCommand &cmd)
                                        { return ShowCommand::hasTypedPayload(cmd.action); });
    const bool requiresV3 = std::any_of(m_commands.cbegin(), m_commands.cend(),
                                        [](const ShowCommand &cmd)
                                        {
                                            return cmd.pairId != ShowCommand::InvalidId ||
                                                   (cmd.action == ShowCommandAction::SetButtonState &&
                                                    cmd.role != ShowControlRole::ToggleButton);
                                        });

    doc->writeStartElement(KXMLShowCommandTrack);
    doc->writeAttribute(KXMLShowCommandTrackVersion,
                        QString::number(requiresV9 ? kNativeArgumentsVersion
                                                   : requiresV8 ? kXYPadFloorVersion
                                                   : requiresV7 ? kAnimationFaderVersion
                                                   : requiresV6 ? kXYPadPositionVersion
                                                   : requiresV5 ? kSliderColorsVersion
                                                   : requiresV4 ? kSliderResetVersion
                                                   : requiresV3 ? kButtonPairVersion
                                                   : controlStates ? kControlStateVersion
                                                                   : LegacyVersion));
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
            if (ShowCommand::hasTypedPayload(cmd.action))
                doc->writeAttribute("Arguments", cmd.payload.encode(cmd.action));
            else if (cmd.action == ShowCommandAction::SetButtonState)
            {
                doc->writeAttribute(KXMLShowCommandState, cmd.on ? KXMLShowCommandStateOn
                                                                 : KXMLShowCommandStateOff);
                if (cmd.pairId != ShowCommand::InvalidId)
                    doc->writeAttribute(KXMLShowCommandPair, QString::number(cmd.pairId));
            }
            else if (cmd.action == ShowCommandAction::SetSliderPosition ||
                     cmd.action == ShowCommandAction::SetSliderColors)
            {
                doc->writeAttribute(KXMLShowCommandValue, intensityToString(cmd.position));
                if (!cmd.nativeArgument().isEmpty())
                    doc->writeAttribute(KXMLShowCommandAttribute, cmd.nativeArgument());
            }
            else if (cmd.action == ShowCommandAction::SetXYPadPosition ||
                     cmd.action == ShowCommandAction::SetXYPadFloor)
            {
                doc->writeAttribute(KXMLShowCommandAttribute, cmd.nativeArgument());
            }
            else if (cmd.action == ShowCommandAction::SetAnimationFader)
            {
                doc->writeAttribute(KXMLShowCommandValue, intensityToString(cmd.position));
            }
            else if (!cmd.attribute.isEmpty())
            {
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
    cmd.payload = input.payload;
    cmd.on = input.on;
    cmd.position = input.position;

    if (cmd.action == ShowCommandAction::SetButtonState && isMomentaryHoldRole(cmd.role))
    {
        const quint32 openPair = activePairId(track, cmd.controlId, cmd.role);
        if (cmd.on)
        {
            if (openPair != ShowCommand::InvalidId)
                return result;
            cmd.pairId = nextPairId(track);
        }
        else
        {
            if (openPair == ShowCommand::InvalidId)
                return result;
            cmd.pairId = openPair;
        }
    }

    if (cmd.id == ShowCommand::InvalidId)
    {
        result.error = QStringLiteral("no free event id left in this track");
        return result;
    }

    cmd = cmd.canonicalized();
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
    if (expected.action == ShowCommandAction::SetSliderPosition && control.attribute != expected.attribute)
        return status(ShowControlStatus::Incompatible,
                      QStringLiteral("expected attribute '%1', found '%2'")
                          .arg(expected.attribute, control.attribute));
    if (expected.action == ShowCommandAction::SetSliderReset && control.attribute != expected.attribute)
        return status(ShowControlStatus::Incompatible,
                      QStringLiteral("expected attribute '%1', found '%2'")
                          .arg(expected.attribute, control.attribute));
    if (expected.action == ShowCommandAction::SetSliderChannel)
    {
        const auto *channel = std::get_if<ShowCommandChannel>(&expected.payload.value);
        if (channel == nullptr || control.attribute != channel->binding)
            return status(ShowControlStatus::Incompatible, QStringLiteral("channel binding is incompatible"));
    }
    if (!control.enabled)
        return status(ShowControlStatus::Disabled, QStringLiteral("control is disabled"));

    const bool needsFunction = expected.role == ShowControlRole::ToggleButton ||
                               expected.role == ShowControlRole::FlashButton ||
                               expected.role == ShowControlRole::AdjustSlider ||
                               expected.role == ShowControlRole::AnimationFader;
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
