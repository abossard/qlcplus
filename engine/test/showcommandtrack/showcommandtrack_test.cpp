/*
  Q Light Controller Plus - Unit test
  showcommandtrack_test.cpp

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

#include <QtTest>

#include "showcommandtrack_test.h"
#include "showcommandtrack.h"

Q_DECLARE_METATYPE(ShowCommand)
Q_DECLARE_METATYPE(ShowCommandOrigin)
Q_DECLARE_METATYPE(ShowCommandAction)
Q_DECLARE_METATYPE(ShowCommandPayload)
Q_DECLARE_METATYPE(ShowCommandFsm::ShowButtonNative)
Q_DECLARE_METATYPE(ShowCommandFsm::ShowButtonOp)
Q_DECLARE_METATYPE(ShowControlStatus)
Q_DECLARE_METATYPE(QVector<ShowControlSnapshot>)

static const QUuid kControlA(QStringLiteral("{6f1c2d3e-4a5b-4c6d-8e7f-001122334455}"));
static const QUuid kControlB(QStringLiteral("{0a9b8c7d-6e5f-4a3b-9c1d-5566778899aa}"));
/** Output sentinel no valid mapping row produces: a rejected mapping must leave it as is */
static const int kUntouched = 7777;

/** Scrambled insertion order on purpose: two equal-time pairs and three targets */
static ShowCommandTrack fixtureTrack()
{
    ShowCommandTrack track;
    track.insert(ShowCommand::setIntensity(9, 1500, 12, 0.375));
    track.insert(ShowCommand::start(7, 0, 12));
    track.insert(ShowCommand::stop(10, 4200, 30));
    track.insert(ShowCommand::start(8, 0, 30));
    track.insert(ShowCommand::setIntensity(11, 4200, 12, 0.8));
    track.setExtent(9000);
    return track;
}

static QVector<quint32> idsOf(const ShowCommandTrack &track)
{
    QVector<quint32> ids;
    for (const ShowCommand &cmd : track.commands())
        ids.append(cmd.id);
    return ids;
}

static QString toXml(const ShowCommandTrack &track)
{
    QString xml;
    QXmlStreamWriter writer(&xml);
    if (!track.saveXML(&writer))
        return QString();
    return xml;
}

static bool fromXml(const QString &xml, ShowCommandTrack *track, QString *error = nullptr)
{
    QXmlStreamReader reader(xml);
    if (!reader.readNextStartElement())
        return false;
    return track->loadXML(reader, error);
}

void ShowCommandTrack_Test::incompatiblePairIsTransactional_data()
{
    QTest::addColumn<ShowCommand>("command");
    QTest::addColumn<QString>("boundary");
    QTest::addColumn<QString>("pair");
    QVector<ShowCommand> commands = {
        ShowCommand::setSliderColors(24, 1500, kControlA, ShowControlRole::LevelSlider,
                                     "10,20,30;40,50,60", .5),
        ShowCommand::setXYPadPosition(24, 1500, kControlA, "64.1,192.2"),
        ShowCommand::setAnimationFader(24, 1500, kControlA, .75),
        ShowCommand::setSliderReset(24, 1500, kControlA, ShowControlRole::AdjustSlider, "Intensity"),
        ShowCommand::setSliderPosition(24, 1500, kControlA, ShowControlRole::LevelSlider, {}, .3),
        ShowCommand::start(24, 1500, 42),
        ShowCommand::setIntensity(24, 1500, 42, .5)
    };
    ShowCommand floor = ShowCommand::setXYPadPosition(24, 1500, kControlA, {});
    floor.action = ShowCommandAction::SetXYPadFloor;
    floor.payload.value = ShowCommandFloor{1.1, 2.2, 3.3};
    commands.append(floor);
    for (const auto &command : commands)
    {
        for (const QString &boundary : {QStringLiteral("insert"), QStringLiteral("replace"),
                                        QStringLiteral("load")})
        {
            const QString name = ShowCommand::actionToString(command.action) + '-' + boundary;
            QTest::newRow(qPrintable(name)) << command << boundary << QStringLiteral("7");
        }
        const QString name = ShowCommand::actionToString(command.action) + "-load-malformed";
        QTest::newRow(qPrintable(name)) << command << QStringLiteral("load") << QStringLiteral("invalid");
    }
}

void ShowCommandTrack_Test::incompatiblePairIsTransactional()
{
    QFETCH(ShowCommand, command);
    QFETCH(QString, boundary);
    QFETCH(QString, pair);
    ShowCommandTrack track = fixtureTrack();
    QVERIFY(track.retime(7, 1500));
    track.reserve(100, 200);
    const ShowCommandTrack before = track;
    const QString xmlBefore = toXml(track);
    QString reason;
    bool accepted;
    if (boundary == "load")
    {
        ShowCommandTrack candidate;
        QVERIFY(candidate.insert(command));
        QString xml = toXml(candidate);
        xml.replace(" Action=", " Pair=\"" + pair + "\" Action=");
        accepted = fromXml(xml, &track, &reason);
    }
    else
    {
        command.pairId = 7;
        if (boundary == "replace")
            command.id = 9;
        accepted = boundary == "insert" ? track.insert(command, &reason) : track.replace(command, &reason);
    }
    QVERIFY(!accepted);
    QVERIFY2(reason.contains("pair", Qt::CaseInsensitive), qPrintable(reason));
    QCOMPARE(track.commands(), before.commands());
    QCOMPARE(toXml(track), xmlBefore);
    QCOMPARE(track.extent(), before.extent());
    QCOMPARE(track.nextEventId(), before.nextEventId());
    QCOMPARE(track.nextOrder(), before.nextOrder());
    for (int i = 0; i < track.count(); ++i)
        QCOMPARE(track.commands()[i].order, before.commands()[i].order);
}

void ShowCommandTrack_Test::explicitButtonPairIsTransactional_data()
{
    QTest::addColumn<QString>("pair");
    QTest::newRow("explicit unset sentinel") << QStringLiteral("4294967295");
    QTest::newRow("negative") << QStringLiteral("-1");
    QTest::newRow("malformed") << QStringLiteral("invalid");
}

void ShowCommandTrack_Test::explicitButtonPairIsTransactional()
{
    QFETCH(QString, pair);
    ShowCommandTrack track = fixtureTrack();
    QVERIFY(track.retime(7, 1500));
    track.reserve(100, 200);
    const ShowCommandTrack before = track;
    const QString xmlBefore = toXml(track);
    const QString xml = QStringLiteral(
        "<CommandTrack Version=\"3\" Extent=\"4000\"><Command ID=\"24\" Time=\"1500\" "
        "Action=\"SetButtonState\" Control=\"%1\" Role=\"FlashButton\" State=\"On\" "
        "Pair=\"%2\"/></CommandTrack>").arg(kControlA.toString(), pair);
    QString reason;
    QVERIFY(!fromXml(xml, &track, &reason));
    QCOMPARE(reason, QStringLiteral("invalid pair id '%1'").arg(pair));
    QCOMPARE(track.commands(), before.commands());
    QCOMPARE(toXml(track), xmlBefore);
    QCOMPARE(track.extent(), before.extent());
    QCOMPARE(track.nextEventId(), before.nextEventId());
    QCOMPARE(track.nextOrder(), before.nextOrder());
    for (int i = 0; i < track.count(); ++i)
        QCOMPARE(track.commands()[i].order, before.commands()[i].order);
}

void ShowCommandTrack_Test::typedPayloadEquality_data()
{
    QTest::addColumn<ShowCommandPayload>("left");
    QTest::addColumn<ShowCommandPayload>("right");
    const auto row = [](const char *name, ShowCommandPayload::Value left,
                        ShowCommandPayload::Value right) {
        QTest::newRow(name) << ShowCommandPayload{left} << ShowCommandPayload{right};
    };
    row("variant", ShowCommandColors{}, ShowCommandPanTilt{});
    row("pan precision", ShowCommandPanTilt{64.12345678901234, 192.2},
        ShowCommandPanTilt{64.12345678901235, 192.2});
    row("tilt", ShowCommandPanTilt{64.1, 192.2}, ShowCommandPanTilt{64.1, 192.3});
    row("floor x", ShowCommandFloor{1.123456789012345, 2.2, 3.3},
        ShowCommandFloor{1.123456789012346, 2.2, 3.3});
    row("floor y", ShowCommandFloor{1.1, 2.2, 3.3}, ShowCommandFloor{1.1, 2.3, 3.3});
    row("floor z", ShowCommandFloor{1.1, 2.2, 3.3}, ShowCommandFloor{1.1, 2.2, 3.4});
    for (int i = 0; i < 6; ++i)
    {
        ShowCommandColors changed;
        switch (i)
        {
            case 0: changed.red = 1; break;
            case 1: changed.green = 1; break;
            case 2: changed.blue = 1; break;
            case 3: changed.white = 1; break;
            case 4: changed.amber = 1; break;
            case 5: changed.ultraviolet = 1; break;
        }
        row(qPrintable(QString("color-%1").arg(i)), ShowCommandColors{}, changed);
    }
    for (int i = 0; i < 4; ++i)
    {
        ShowCommandRanges changed;
        switch (i)
        {
            case 0: changed.horizontal.pan = 1; break;
            case 1: changed.horizontal.tilt = 1; break;
            case 2: changed.vertical.pan = 1; break;
            case 3: changed.vertical.tilt = 1; break;
        }
        row(qPrintable(QString("range-%1").arg(i)), ShowCommandRanges{}, changed);
    }
    row("choice locator", ShowCommandChoice{2, true, {40.1, 180.2}},
        ShowCommandChoice{3, true, {40.1, 180.2}});
    row("choice active", ShowCommandChoice{2, true, {40.1, 180.2}},
        ShowCommandChoice{2, false, {40.1, 180.2}});
    row("choice point", ShowCommandChoice{2, true, {40.1, 180.2}},
        ShowCommandChoice{2, true, {40.1, 180.3}});
    const ShowCommandMatrixColor black{2, ShowCommandMatrixColor::Operation::Replace, {}, -1, 0, -1};
    for (int i = 0; i < 8; ++i)
    {
        auto changed = black;
        switch (i)
        {
            case 0: changed.index = 3; break;
            case 1: changed.operation = ShowCommandMatrixColor::Operation::Reset; break;
            case 2: changed.color.red = 1; break;
            case 3: changed.color.white = 1; break;
            case 4: changed.component = 0; break;
            case 5: changed.value = 1; break;
            case 6: changed.choice = 3; break;
            case 7: changed.choice = -2; break;
        }
        row(qPrintable(QString("matrix-all-fields-%1").arg(i)), black, changed);
    }
    ShowCommandContent content{"RGBText", "line <one>\nline two", {
        {"list", {ShowCommandProperty::Type::List, "Left", 0}},
        {"range", {ShowCommandProperty::Type::Range, {}, 3}},
        {"float", {ShowCommandProperty::Type::Float, {}, .123456789012345}},
        {"string", {ShowCommandProperty::Type::String, "literal", 0}}
    }, 2};
    for (int i = 0; i < 9; ++i)
    {
        auto changed = content;
        switch (i)
        {
            case 0: changed.algorithm = "Other"; break;
            case 1: changed.text += '!'; break;
            case 2: changed.choice = 3; break;
            case 3: changed.properties["list"].type = ShowCommandProperty::Type::String; break;
            case 4: changed.properties["list"].text = "Right"; break;
            case 5: changed.properties["range"].number = 4; break;
            case 6: changed.properties["float"].number = .123456789012346; break;
            case 7: changed.properties["string"].number = 1; break;
            case 8: changed.properties["float"].text = "unused"; break;
        }
        row(qPrintable(QString("content-all-fields-%1").arg(i)), content, changed);
    }
    row("channel byte", ShowCommandChannel{64, "Intensity"}, ShowCommandChannel{65, "Intensity"});
    row("channel binding", ShowCommandChannel{64, "Intensity"}, ShowCommandChannel{64, "Width"});
}

void ShowCommandTrack_Test::typedPayloadEquality()
{
    QFETCH(ShowCommandPayload, left);
    QFETCH(ShowCommandPayload, right);
    QVERIFY(left == left);
    QVERIFY(right == right);
    QVERIFY(!(left == right));
    QVERIFY(!(right == left));
    ShowCommand command = ShowCommand::setXYPadPosition(24, 1500, kControlA, "64,192");
    command.attribute.clear();
    command.payload = left;
    ShowCommand changed = command;
    changed.payload = right;
    QVERIFY(command != changed);
    changed = command;
    changed.order = 42;
    QCOMPARE(changed, command);
}

void ShowCommandTrack_Test::typedPayloadRejection_data()
{
    QTest::addColumn<ShowCommand>("command");
    QTest::addColumn<QString>("reasonPart");
    const auto row = [](const char *name, ShowCommandAction action, ShowCommandPayload::Value value,
                        const QString &reasonPart = QString()) {
        ShowCommand command;
        command.id = 24;
        command.time = 1500;
        command.controlId = kControlA;
        command.action = action;
        command.role = action == ShowCommandAction::SetSliderChannel || action == ShowCommandAction::SetSliderColors
            ? ShowControlRole::LevelSlider
            : action == ShowCommandAction::SetAnimationColor || action == ShowCommandAction::SetAnimationContent
                ? ShowControlRole::AnimationFader : ShowControlRole::XYPad;
        command.payload.value = value;
        QTest::newRow(name) << command << reasonPart;
    };
    for (const auto action : {ShowCommandAction::SetSliderColors, ShowCommandAction::SetXYPadPosition,
                              ShowCommandAction::SetXYPadFloor, ShowCommandAction::SetXYPadRanges,
                              ShowCommandAction::SetXYPadPositionPreset, ShowCommandAction::SetXYPadFunctionPreset,
                              ShowCommandAction::SetXYPadGroupPreset, ShowCommandAction::SetAnimationColor,
                              ShowCommandAction::SetAnimationContent, ShowCommandAction::SetSliderChannel})
        row(qPrintable(ShowCommand::actionToString(action) + "-wrong-variant"), action, std::monostate{});
    row("position nan", ShowCommandAction::SetXYPadPosition, ShowCommandPanTilt{qQNaN(), 1});
    row("position infinite", ShowCommandAction::SetXYPadPosition, ShowCommandPanTilt{1, qInf()});
    row("position negative", ShowCommandAction::SetXYPadPosition, ShowCommandPanTilt{-1, 1});
    row("position overflow", ShowCommandAction::SetXYPadPosition, ShowCommandPanTilt{1, 255.0001});
    row("floor nan", ShowCommandAction::SetXYPadFloor, ShowCommandFloor{1, qQNaN(), 1});
    row("floor infinity", ShowCommandAction::SetXYPadFloor, ShowCommandFloor{1, 1, qInf()});
    row("floor negative", ShowCommandAction::SetXYPadFloor, ShowCommandFloor{-1, 1, 1});
    for (int i = 0; i < 4; ++i)
    {
        ShowCommandRanges ranges{{10, 200}, {20, 240}};
        switch (i)
        {
            case 0: ranges.horizontal.pan = qQNaN(); break;
            case 1: ranges.horizontal.tilt = -1; break;
            case 2: ranges.vertical.pan = qInf(); break;
            case 3: ranges.vertical.tilt = 256; break;
        }
        row(qPrintable(QString("range invalid endpoint-%1").arg(i)),
            ShowCommandAction::SetXYPadRanges, ranges);
    }
    row("choice negative", ShowCommandAction::SetXYPadPositionPreset, ShowCommandChoice{-1, true, {1, 2}});
    row("choice overflow", ShowCommandAction::SetXYPadGroupPreset, ShowCommandChoice{256, true, {}});
    row("choice invalid point", ShowCommandAction::SetXYPadPositionPreset, ShowCommandChoice{1, true, {qInf(), 2}});
    row("function unused point", ShowCommandAction::SetXYPadFunctionPreset, ShowCommandChoice{1, false, {1, 0}});
    row("group unused point", ShowCommandAction::SetXYPadGroupPreset, ShowCommandChoice{1, false, {0, 1}});
    const ShowCommandMatrixColor black{2, ShowCommandMatrixColor::Operation::Replace, {}, -1, 0, -1};
    for (int i = 0; i < 12; ++i)
    {
        auto color = black;
        switch (i)
        {
            case 0: color.index = -1; break;
            case 1: color.index = 5; break;
            case 2: color.operation = static_cast<ShowCommandMatrixColor::Operation>(255); break;
            case 3: color.component = 0; break;
            case 4: color.value = 1; break;
            case 5: color.choice = -2; break;
            case 6: color.choice = 256; break;
            case 7: color.color.white = 1; break;
            case 8: color.operation = ShowCommandMatrixColor::Operation::Reset; color.color.red = 1; break;
            case 9: color.operation = ShowCommandMatrixColor::Operation::Reset; color.choice = 1; break;
            case 10: color.operation = ShowCommandMatrixColor::Operation::Component; color.component = 3; break;
            case 11: color.operation = ShowCommandMatrixColor::Operation::Component;
                     color.component = 1; color.value = 256; break;
        }
        row(qPrintable(QString("matrix-invalid-%1").arg(i)), ShowCommandAction::SetAnimationColor, color);
    }
    ShowCommandContent content{"RGBText", "literal", {}, -1};
    for (int i = 0; i < 9; ++i)
    {
        auto changed = content;
        switch (i)
        {
            case 0: changed.algorithm.clear(); break;
            case 1: changed.choice = 256; break;
            case 2: changed.properties[""] = {}; break;
            case 3: changed.properties["bad"].type = static_cast<ShowCommandProperty::Type>(255); break;
            case 4: changed.properties["bad"] = {ShowCommandProperty::Type::Range, {}, 1.5}; break;
            case 5: changed.properties["bad"] = {ShowCommandProperty::Type::Float, {}, qQNaN()}; break;
            case 6: changed.properties["bad"] = {ShowCommandProperty::Type::Float, {}, qInf()}; break;
            case 7: changed.properties["bad"] = {ShowCommandProperty::Type::String, "literal", 1}; break;
            case 8: changed.properties["bad"] = {ShowCommandProperty::Type::Float, "unused", 1}; break;
        }
        row(qPrintable(QString("content-invalid-%1").arg(i)), ShowCommandAction::SetAnimationContent, changed);
    }
    row("channel negative", ShowCommandAction::SetSliderChannel, ShowCommandChannel{-1, {}});
    row("channel overflow", ShowCommandAction::SetSliderChannel, ShowCommandChannel{256, {}});
    for (const qreal position : {qQNaN(), qInf(), qreal(-.1), qreal(1.1)})
    {
        ShowCommand fader = ShowCommand::setAnimationFader(24, 1500, kControlA, position);
        QTest::newRow(qPrintable("fader invalid " + QString::number(position))) << fader << QStringLiteral("position");
    }
    ShowCommand fader = ShowCommand::setAnimationFader(24, 1500, kControlA, .5);
    fader.role = ShowControlRole::XYPad;
    QTest::newRow("fader wrong role") << fader << QStringLiteral("role");
    fader = ShowCommand::setAnimationFader(24, 1500, kControlA, .5);
    fader.payload.value = ShowCommandColors{};
    QTest::newRow("fader unrelated payload") << fader << QStringLiteral("unrelated");
    ShowCommand reset = ShowCommand::setSliderReset(24, 1500, kControlA, ShowControlRole::LevelSlider, {});
    reset.position = .5;
    QTest::newRow("reset unused position") << reset << QStringLiteral("carries no");
    reset.position = 0;
    reset.role = ShowControlRole::AnimationFader;
    QTest::newRow("reset wrong role") << reset << QStringLiteral("role");
    ShowCommand invalid = ShowCommand::setXYPadPosition(24, 1500, kControlA, "64,192");
    invalid.payload.value = ShowCommandPanTilt{64, 192};
    invalid.attribute = "64,192";
    QTest::newRow("duplicate native state") << invalid << QStringLiteral("duplicate");
    invalid.attribute.clear();
    invalid.role = ShowControlRole::LevelSlider;
    QTest::newRow("wrong legacy native role") << invalid << QStringLiteral("role");
    invalid.action = ShowCommandAction::SetXYPadRanges;
    invalid.payload.value = ShowCommandRanges{{10, 200}, {20, 240}};
    QTest::newRow("wrong schema9 role") << invalid << QStringLiteral("role");
    invalid.role = ShowControlRole::XYPad;
    invalid.position = .5;
    QTest::newRow("schema9 unrelated position") << invalid << QStringLiteral("unrelated");
    invalid.position = 0;
    invalid.action = static_cast<ShowCommandAction>(255);
    QTest::newRow("true unknown action") << invalid << QStringLiteral("unknown action 255");
}

void ShowCommandTrack_Test::typedPayloadRejection()
{
    QFETCH(ShowCommand, command);
    QFETCH(QString, reasonPart);
    ShowCommandTrack track = fixtureTrack();
    QVERIFY(track.retime(7, 1500));
    track.reserve(100, 200);
    const ShowCommandTrack before = track;
    const QString xmlBefore = toXml(track);
    for (const bool replace : {false, true})
    {
        command.id = replace ? 9 : 24;
        QString reason;
        QVERIFY(!(replace ? track.replace(command, &reason) : track.insert(command, &reason)));
        QVERIFY(!reason.isEmpty());
        QVERIFY2(reason.contains(reasonPart), qPrintable(reason));
        QCOMPARE(track.commands(), before.commands());
        QCOMPARE(toXml(track), xmlBefore);
        QCOMPARE(track.extent(), before.extent());
        QCOMPARE(track.nextEventId(), before.nextEventId());
        QCOMPARE(track.nextOrder(), before.nextOrder());
        for (int i = 0; i < track.count(); ++i)
            QCOMPARE(track.commands()[i].order, before.commands()[i].order);
    }
}

void ShowCommandTrack_Test::nativeSchemaRejection_data()
{
    QTest::addColumn<QString>("action");
    QTest::addColumn<QString>("role");
    QTest::addColumn<QString>("arguments");
    QTest::addColumn<int>("version");
    const auto row = [](const char *name, const char *action, const char *role,
                        const char *arguments, int version = 9) {
        QTest::newRow(name) << QString(action) << QString(role) << QString(arguments) << version;
    };
    row("ranges missing", "SetXYPadRanges", "XYPad", R"({"horizontal":[1,2]})");
    row("ranges extra", "SetXYPadRanges", "XYPad", R"({"horizontal":[1,2],"vertical":[3,4],"extra":0})");
    row("ranges cardinality", "SetXYPadRanges", "XYPad", R"({"horizontal":[1,2,3],"vertical":[3,4]})");
    row("ranges string", "SetXYPadRanges", "XYPad", R"({"horizontal":["1",2],"vertical":[3,4]})");
    row("ranges nonfinite", "SetXYPadRanges", "XYPad", R"({"horizontal":[1e999,2],"vertical":[3,4]})");
    row("ranges negative", "SetXYPadRanges", "XYPad", R"({"horizontal":[-1,2],"vertical":[3,4]})");
    row("ranges overflow", "SetXYPadRanges", "XYPad", R"({"horizontal":[1,2],"vertical":[3,256]})");
    row("ranges wrong role", "SetXYPadRanges", "AnimationFader", R"({"horizontal":[1,2],"vertical":[3,4]})");
    row("choice active type", "SetXYPadPositionPreset", "XYPad", R"({"choice":1,"active":1,"point":[1,2]})");
    row("choice fractional", "SetXYPadFunctionPreset", "XYPad", R"({"choice":1.5,"active":true})");
    row("choice negative", "SetXYPadGroupPreset", "XYPad", R"({"choice":-1,"active":true})");
    row("choice overflow", "SetXYPadGroupPreset", "XYPad", R"({"choice":256,"active":true})");
    row("choice missing point", "SetXYPadPositionPreset", "XYPad", R"({"choice":1,"active":true})");
    row("choice point overflow", "SetXYPadPositionPreset", "XYPad", R"({"choice":1,"active":true,"point":[1,256]})");
    row("choice unrelated point", "SetXYPadFunctionPreset", "XYPad", R"({"choice":1,"active":false,"point":[0,0]})");
    row("color slot negative", "SetAnimationColor", "AnimationFader", R"({"index":-1,"operation":"reset"})");
    row("color slot overflow", "SetAnimationColor", "AnimationFader", R"({"index":5,"operation":"reset"})");
    row("color unknown operation", "SetAnimationColor", "AnimationFader", R"({"index":1,"operation":"unknown"})");
    row("color missing rgb", "SetAnimationColor", "AnimationFader", R"({"index":1,"operation":"replace"})");
    row("color rgb cardinality", "SetAnimationColor", "AnimationFader", R"({"index":1,"operation":"replace","rgb":[1,2]})");
    row("color rgb fractional", "SetAnimationColor", "AnimationFader", R"({"index":1,"operation":"replace","rgb":[1,2.5,3]})");
    row("color rgb overflow", "SetAnimationColor", "AnimationFader", R"({"index":1,"operation":"replace","rgb":[256,2,3]})");
    row("color extra wauv", "SetAnimationColor", "AnimationFader", R"({"index":1,"operation":"replace","rgb":[1,2,3],"wauv":[0,0,0]})");
    row("reset extra rgb", "SetAnimationColor", "AnimationFader", R"({"index":1,"operation":"reset","rgb":[0,0,0]})");
    row("reset extra choice", "SetAnimationColor", "AnimationFader", R"({"index":1,"operation":"reset","choice":1})");
    row("component missing value", "SetAnimationColor", "AnimationFader", R"({"index":1,"operation":"component","component":1})");
    row("component overflow", "SetAnimationColor", "AnimationFader", R"({"index":1,"operation":"component","component":3,"value":1})");
    row("component value overflow", "SetAnimationColor", "AnimationFader", R"({"index":1,"operation":"component","component":1,"value":256})");
    row("component bool value", "SetAnimationColor", "AnimationFader", R"({"index":1,"operation":"component","component":1,"value":true})");
    row("content blank algorithm", "SetAnimationContent", "AnimationFader", R"({"algorithm":"","text":"","properties":[]})");
    row("content malformed properties", "SetAnimationContent", "AnimationFader", R"({"algorithm":"RGBText","text":"","properties":{}})");
    row("content unnamed property", "SetAnimationContent", "AnimationFader", R"({"algorithm":"RGBText","text":"","properties":[{"name":"","type":"String","value":"x"}]})");
    row("content duplicate property", "SetAnimationContent", "AnimationFader", R"({"algorithm":"RGBText","text":"","properties":[{"name":"a","type":"String","value":"x"},{"name":"a","type":"String","value":"y"}]})");
    row("content unknown type", "SetAnimationContent", "AnimationFader", R"({"algorithm":"RGBText","text":"","properties":[{"name":"a","type":"Boolean","value":true}]})");
    row("content List number", "SetAnimationContent", "AnimationFader", R"({"algorithm":"RGBText","text":"","properties":[{"name":"a","type":"List","value":1}]})");
    row("content String bool", "SetAnimationContent", "AnimationFader", R"({"algorithm":"RGBText","text":"","properties":[{"name":"a","type":"String","value":false}]})");
    row("content Range fractional", "SetAnimationContent", "AnimationFader", R"({"algorithm":"RGBText","text":"","properties":[{"name":"a","type":"Range","value":1.5}]})");
    row("content Float nonfinite", "SetAnimationContent", "AnimationFader", R"({"algorithm":"RGBText","text":"","properties":[{"name":"a","type":"Float","value":1e999}]})");
    row("content property extra", "SetAnimationContent", "AnimationFader", R"({"algorithm":"RGBText","text":"","properties":[{"name":"a","type":"Float","value":1,"extra":0}]})");
    row("content property missing", "SetAnimationContent", "AnimationFader", R"({"algorithm":"RGBText","text":"","properties":[{"name":"a","type":"String"}]})");
    row("channel string byte", "SetSliderChannel", "LevelSlider", R"({"binding":"","value":"64"})");
    row("channel negative", "SetSliderChannel", "LevelSlider", R"({"binding":"","value":-1})");
    row("channel overflow", "SetSliderChannel", "LevelSlider", R"({"binding":"","value":256})");
    row("channel binding type", "SetSliderChannel", "LevelSlider", R"({"binding":1,"value":64})");
    row("unknown true action", "FutureAction255", "XYPad", "{}");
    row("future version", "SetXYPadRanges", "XYPad", R"({"horizontal":[1,2],"vertical":[3,4]})", 10);
    row("below literal floor8", "SetXYPadFloor", "XYPad", "1.1,2.2,3.3", 7);
    row("below literal hold3", "SetButtonState", "FlashButton", "", 2);
}

void ShowCommandTrack_Test::nativeSchemaRejection()
{
    QFETCH(QString, action);
    QFETCH(QString, role);
    QFETCH(QString, arguments);
    QFETCH(int, version);
    ShowCommandTrack track = fixtureTrack();
    QVERIFY(track.retime(7, 1500));
    track.reserve(100, 200);
    const ShowCommandTrack before = track;
    const QString xmlBefore = toXml(track);
    QString xml;
    QXmlStreamWriter writer(&xml);
    writer.writeStartElement("CommandTrack");
    writer.writeAttribute("Version", QString::number(version));
    writer.writeStartElement("Command");
    writer.writeAttribute("ID", "24");
    writer.writeAttribute("Time", "1500");
    writer.writeAttribute("Action", action);
    writer.writeAttribute("Control", kControlA.toString());
    writer.writeAttribute("Role", role);
    writer.writeAttribute(action == "SetXYPadFloor" ? "Attribute" : "Arguments", arguments);
    if (action == "SetButtonState")
    {
        writer.writeAttribute("State", "On");
        writer.writeAttribute("Pair", "7");
    }
    writer.writeEndElement();
    writer.writeEndElement();
    QString reason;
    QVERIFY(!fromXml(xml, &track, &reason));
    QVERIFY(!reason.isEmpty());
    if (action == "FutureAction255")
        QVERIFY2(reason.startsWith("unknown action"), qPrintable(reason));
    if (version == 7 || version == 2)
        QVERIFY2(reason.contains(version == 7 ? "version 8" : "version 3"), qPrintable(reason));
    QCOMPARE(track.commands(), before.commands());
    QCOMPARE(toXml(track), xmlBefore);
    QCOMPARE(track.extent(), before.extent());
    QCOMPARE(track.nextEventId(), before.nextEventId());
    QCOMPARE(track.nextOrder(), before.nextOrder());
    for (int i = 0; i < track.count(); ++i)
        QCOMPARE(track.commands()[i].order, before.commands()[i].order);
}

void ShowCommandTrack_Test::allSelectedValuesRoundTrip_data()
{
    QTest::addColumn<bool>("reversePlacement");
    QTest::newRow("forward restore") << false;
    QTest::newRow("reverse restore") << true;
}

void ShowCommandTrack_Test::allSelectedValuesRoundTrip()
{
    QFETCH(bool, reversePlacement);
    QVector<ShowCommand> commands = {
        ShowCommand::setSliderReset(10, 1500, kControlA, ShowControlRole::LevelSlider, {}),
        ShowCommand::setSliderColors(11, 1500, kControlA, ShowControlRole::LevelSlider,
                                     "0,255,64;128,192,1", .123456789012345),
        ShowCommand::setXYPadPosition(12, 1500, kControlB, "64.12345678901234,192.1234567890123"),
        ShowCommand::setAnimationFader(13, 1500, kControlA, .876543210987654),
        ShowCommand::setSliderPosition(14, 1500, kControlA, ShowControlRole::LevelSlider, {}, .75)
    };
    const QVector<QPair<ShowCommandAction, ShowCommandPayload::Value>> values = {
        {ShowCommandAction::SetXYPadFloor, ShowCommandFloor{25.12345678901234, 2.123456789012345, 3.987654321098765}},
        {ShowCommandAction::SetSliderChannel, ShowCommandChannel{0, {}}},
        {ShowCommandAction::SetSliderChannel, ShowCommandChannel{255, "Intensity"}},
        {ShowCommandAction::SetSliderChannel, ShowCommandChannel{64, "Intensity"}},
        {ShowCommandAction::SetXYPadRanges, ShowCommandRanges{{200.1234567890123, 12.12345678901234},
                                                            {40.12345678901234, 240.1234567890123}}},
        {ShowCommandAction::SetXYPadPositionPreset, ShowCommandChoice{255, true, {40.12345678901234, 180.1234567890123}}},
        {ShowCommandAction::SetXYPadPositionPreset, ShowCommandChoice{0, false, {0, 255}}},
        {ShowCommandAction::SetXYPadFunctionPreset, ShowCommandChoice{3, true, {}}},
        {ShowCommandAction::SetXYPadFunctionPreset, ShowCommandChoice{3, false, {}}},
        {ShowCommandAction::SetXYPadGroupPreset, ShowCommandChoice{4, true, {}}},
        {ShowCommandAction::SetXYPadGroupPreset, ShowCommandChoice{4, false, {}}},
        {ShowCommandAction::SetAnimationColor, ShowCommandMatrixColor{4, ShowCommandMatrixColor::Operation::Replace,
                                                                     {0, 0, 0, 0, 0, 0}, -1, 0, 2}},
        {ShowCommandAction::SetAnimationColor, ShowCommandMatrixColor{0, ShowCommandMatrixColor::Operation::Reset,
                                                                     {}, -1, 0, -1}},
        {ShowCommandAction::SetAnimationColor, ShowCommandMatrixColor{2, ShowCommandMatrixColor::Operation::Component,
                                                                     {}, 2, 255, 3}},
        {ShowCommandAction::SetAnimationContent, ShowCommandContent{"RGBText", "line <one>\nline &two", {
            {"list", {ShowCommandProperty::Type::List, "Left", 0}},
            {"range", {ShowCommandProperty::Type::Range, {}, -3}},
            {"float", {ShowCommandProperty::Type::Float, {}, .123456789012345}},
            {"string", {ShowCommandProperty::Type::String, "literal \"text\"", 0}}
        }, 2}}
    };
    for (const auto &value : values)
    {
        ShowCommand command;
        command.id = quint32(10 + commands.count());
        command.time = 1500;
        command.controlId = kControlB;
        command.action = value.first;
        command.role = value.first == ShowCommandAction::SetSliderChannel ? ShowControlRole::LevelSlider
            : value.first == ShowCommandAction::SetAnimationColor || value.first == ShowCommandAction::SetAnimationContent
                ? ShowControlRole::AnimationFader : ShowControlRole::XYPad;
        command.payload.value = value.second;
        commands.append(command);
    }
    ShowCommand hold = ShowCommand::setButtonState(90, 1000, kControlA, true);
    hold.role = ShowControlRole::FlashButton;
    hold.pairId = 71;
    commands.append(hold);
    hold.id = 91;
    hold.time = 2000;
    hold.on = false;
    commands.append(hold);
    for (int i = 0; i < commands.count(); ++i)
        commands[i].order = quint32(100 - 3 * i);
    if (reversePlacement)
        std::reverse(commands.begin(), commands.end());
    ShowCommandTrack track;
    QString reason;
    for (const auto &command : commands)
        QVERIFY2(track.restore(command, &reason), qPrintable(reason));
    QVERIFY(track.setExtent(9000));
    const QString xml = toXml(track);
    QVERIFY(xml.startsWith("<CommandTrack Version=\"9\""));
    QVERIFY(xml.contains("Order="));
    ShowCommandTrack loaded;
    QVERIFY2(fromXml(xml, &loaded, &reason), qPrintable(reason));
    QCOMPARE(loaded.commands(), track.commands());
    QCOMPARE(toXml(loaded), xml);
    QCOMPARE(loaded.extent(), 9000u);
    QCOMPARE(loaded.nextEventId(), track.nextEventId());
    QCOMPARE(loaded.nextOrder(), track.nextOrder());
    for (const auto &expected : commands)
    {
        const auto &actual = loaded.commands()[loaded.indexOfId(expected.id)];
        QCOMPARE(actual, expected);
        QCOMPARE(actual.order, expected.order);
        QCOMPARE(actual.pairId, expected.pairId);
        QCOMPARE(actual.payload, expected.payload);
    }
    const auto &point = std::get<ShowCommandPanTilt>(loaded.commands()[loaded.indexOfId(12)].payload.value);
    QCOMPARE(point.pan, qreal(64.12345678901234));
    QCOMPARE(point.tilt, qreal(192.1234567890123));
    const auto &floor = std::get<ShowCommandFloor>(loaded.commands()[loaded.indexOfId(15)].payload.value);
    QCOMPARE(floor.x, qreal(25.12345678901234));
    QCOMPARE(floor.y, qreal(2.123456789012345));
    QCOMPARE(floor.z, qreal(3.987654321098765));
}

void ShowCommandTrack_Test::selectedNativePayloads_data()
{
    QTest::addColumn<QString>("action");
    QTest::addColumn<QString>("role");
    QTest::addColumn<QString>("payload");
    QTest::newRow("range endpoints") << QStringLiteral("SetXYPadRanges") << QStringLiteral("XYPad")
        << QStringLiteral("{\"horizontal\":[12.25,200.5],\"vertical\":[40.75,240.125]}");
    QTest::newRow("reversed native endpoints") << QStringLiteral("SetXYPadRanges") << QStringLiteral("XYPad")
        << QStringLiteral("{\"horizontal\":[200.5,12.25],\"vertical\":[240.125,40.75]}");
    QTest::newRow("static choice") << QStringLiteral("SetXYPadPositionPreset") << QStringLiteral("XYPad")
        << QStringLiteral("{\"choice\":2,\"active\":true,\"point\":[40.25,180.75]}");
    QTest::newRow("function choice") << QStringLiteral("SetXYPadFunctionPreset") << QStringLiteral("XYPad")
        << QStringLiteral("{\"choice\":3,\"active\":false}");
    QTest::newRow("group choice") << QStringLiteral("SetXYPadGroupPreset") << QStringLiteral("XYPad")
        << QStringLiteral("{\"choice\":4,\"active\":true}");
    QTest::newRow("matrix black") << QStringLiteral("SetAnimationColor") << QStringLiteral("AnimationFader")
        << QStringLiteral("{\"index\":2,\"operation\":\"replace\",\"rgb\":[0,0,0]}");
    QTest::newRow("matrix reset") << QStringLiteral("SetAnimationColor") << QStringLiteral("AnimationFader")
        << QStringLiteral("{\"index\":1,\"operation\":\"reset\"}");
    QTest::newRow("matrix component") << QStringLiteral("SetAnimationColor") << QStringLiteral("AnimationFader")
        << QStringLiteral("{\"index\":0,\"operation\":\"component\",\"component\":1,\"value\":128,\"choice\":3}");
    QTest::newRow("named content") << QStringLiteral("SetAnimationContent") << QStringLiteral("AnimationFader")
        << QStringLiteral("{\"algorithm\":\"RGBText\",\"text\":\"line <one>\\nline two\",\"properties\":[]}");
    QTest::newRow("explicit channel value") << QStringLiteral("SetSliderChannel") << QStringLiteral("AdjustSlider")
        << QStringLiteral("{\"binding\":\"Intensity\",\"value\":64}");
}

void ShowCommandTrack_Test::selectedNativePayloads()
{
    QFETCH(QString, action);
    QFETCH(QString, role);
    QFETCH(QString, payload);
    QString source;
    QXmlStreamWriter writer(&source);
    writer.writeStartElement("CommandTrack");
    writer.writeAttribute("Version", "9");
    writer.writeAttribute("Extent", "3000");
    writer.writeStartElement("Command");
    writer.writeAttribute("ID", "7");
    writer.writeAttribute("Time", "1200");
    writer.writeAttribute("Action", action);
    writer.writeAttribute("Control", "{01234567-89ab-cdef-0123-456789abcdef}");
    writer.writeAttribute("Role", role);
    writer.writeAttribute("Arguments", payload);
    writer.writeEndElement();
    writer.writeEndElement();
    ShowCommandTrack track;
    QXmlStreamReader reader(source);
    QVERIFY(reader.readNextStartElement());
    QString reason;
    QVERIFY2(track.loadXML(reader, &reason), qPrintable(reason));
    QCOMPARE(track.count(), 1);
    QCOMPARE(track.commands().first().id, quint32(7));
    QString persisted;
    QXmlStreamWriter output(&persisted);
    QVERIFY(track.saveXML(&output));
    ShowCommandTrack loaded;
    QXmlStreamReader reload(persisted);
    QVERIFY(reload.readNextStartElement());
    QVERIFY2(loaded.loadXML(reload, &reason), qPrintable(reason));
    QCOMPARE(loaded.commands().first(), track.commands().first());
    source.replace("Version=\"9\"", "Version=\"8\"");
    QXmlStreamReader old(source);
    QVERIFY(old.readNextStartElement());
    QVERIFY(!loaded.loadXML(old, &reason));
    QVERIFY(!reason.isEmpty());
    QCOMPARE(loaded.commands().first(), track.commands().first());
}

void ShowCommandTrack_Test::floorPayloadRoundTrip_data()
{
    QTest::addColumn<QString>("payload");
    QTest::addColumn<bool>("valid");
    QTest::newRow("metres fractional height") << QStringLiteral("1.25,0.75,3.125") << true;
    QTest::newRow("zero") << QStringLiteral("0,0,0") << true;
    QTest::newRow("missing axis") << QStringLiteral("1,2") << false;
    QTest::newRow("nonfinite") << QStringLiteral("1,nan,2") << false;
    QTest::newRow("negative height") << QStringLiteral("1,-1,2") << false;
}

void ShowCommandTrack_Test::floorPayloadRoundTrip()
{
    QFETCH(QString, payload);
    QFETCH(bool, valid);
    ShowCommandTrack track = fixtureTrack();
    const QString before = toXml(track);
    QString xml;
    QXmlStreamWriter writer(&xml);
    writer.writeStartElement(QStringLiteral("CommandTrack"));
    writer.writeAttribute(QStringLiteral("Version"), QStringLiteral("8"));
    writer.writeStartElement(QStringLiteral("Command"));
    writer.writeAttribute(QStringLiteral("ID"), QStringLiteral("24"));
    writer.writeAttribute(QStringLiteral("Time"), QStringLiteral("250"));
    writer.writeAttribute(QStringLiteral("Action"), QStringLiteral("SetXYPadFloor"));
    writer.writeAttribute(QStringLiteral("Control"), kControlA.toString());
    writer.writeAttribute(QStringLiteral("Role"), QStringLiteral("XYPad"));
    writer.writeAttribute(QStringLiteral("Attribute"), payload);
    writer.writeEndElement();
    writer.writeEndElement();
    QString error;
    QCOMPARE(fromXml(xml, &track, &error), valid);
    if (!valid)
    {
        QVERIFY(!error.isEmpty());
        QCOMPARE(toXml(track), before);
        return;
    }
    QCOMPARE(track.count(), 1);
    QVERIFY(track.commands().first().attribute.isEmpty());
    QCOMPARE(track.commands().first().nativeArgument(), payload);
    ShowCommandTrack loaded;
    QVERIFY2(fromXml(toXml(track), &loaded, &error), qPrintable(error));
    QCOMPARE(loaded.commands(), track.commands());
}

/** Two targets, interleaved values and a Stop that is not the last event */
static ShowCommandTrack playbackTrack()
{
    ShowCommandTrack track;
    track.insert(ShowCommand::start(7, 0, 12));
    track.insert(ShowCommand::start(8, 0, 30));
    track.insert(ShowCommand::setIntensity(20, 1000, 30, 0.6));
    track.insert(ShowCommand::setIntensity(9, 1500, 12, 0.375));
    track.insert(ShowCommand::setIntensity(21, 3000, 30, 0.25));
    track.insert(ShowCommand::stop(10, 4200, 30));
    track.insert(ShowCommand::setIntensity(11, 4200, 12, 0.8));
    track.setExtent(20000);
    return track;
}

static ShowCommandState playingState()
{
    ShowCommandState state;
    state.playing = true;
    return state;
}

static QString idsOf(const QVector<ShowCommand> &commands)
{
    QStringList ids;
    for (const ShowCommand &cmd : commands)
        ids.append(QString::number(cmd.id));
    return ids.join(QLatin1Char(','));
}

void ShowCommandTrack_Test::groupsPartitionAndRegroup_data()
{
    QTest::addColumn<bool>("roundTrip");
    QTest::newRow("authored") << false;
    QTest::newRow("xml") << true;
}

void ShowCommandTrack_Test::groupsPartitionAndRegroup()
{
    QFETCH(bool, roundTrip);
    const QVector<ShowCommand> samples = {
        ShowCommand::setSliderPosition(90, 1000, kControlA, ShowControlRole::LevelSlider, {}, .2),
        ShowCommand::setSliderPosition(3, 1100, kControlA, ShowControlRole::LevelSlider, {}, .8),
        ShowCommand::setSliderPosition(70, 1300, kControlA, ShowControlRole::LevelSlider, {}, .4),
        ShowCommand::setButtonState(8, 1300, kControlB, true),
        ShowCommand::setSliderPosition(2, 1400, kControlA, ShowControlRole::LevelSlider, {}, .3),
        ShowCommand::setSliderPosition(51, 1600, kControlA, ShowControlRole::LevelSlider, {}, .5),
        ShowCommand::setSliderPosition(19, 1600, kControlB, ShowControlRole::LevelSlider, {}, .7),
        ShowCommand::start(1, 1700, 42),
        ShowCommand::setSliderPosition(4, 1800, kControlA, ShowControlRole::AdjustSlider, "Intensity", .6),
        ShowCommand::setSliderPosition(5, 1900, kControlA, ShowControlRole::AdjustSlider, "Width", .9),
        ShowCommand::setSliderPosition(6, 999000, kControlA, ShowControlRole::AdjustSlider, "Width", .1)
    };
    ShowCommandTrack track;
    for (const auto &sample : samples)
        QVERIFY(track.insert(sample));
    if (roundTrip)
        QVERIFY(fromXml(toXml(track), &track));
    const QString original = toXml(track);
    auto groups = track.groups();
    QCOMPARE(groups.size(), 7);
    QCOMPARE(groups[0].eventIds, (QVector<quint32>{90, 3, 70}));
    QCOMPARE(groups[0].startTime, 1000U);
    QCOMPARE(groups[0].endTime, 1300U);
    QCOMPARE(groups[0].action, ShowCommandAction::SetSliderPosition);
    QCOMPARE(groups[0].controlId, kControlA);
    QCOMPARE(groups[1].eventIds, (QVector<quint32>{8}));
    QCOMPARE(groups[2].eventIds, (QVector<quint32>{2, 51}));
    QCOMPARE(groups[3].eventIds, (QVector<quint32>{19}));
    QCOMPARE(groups[4].eventIds, (QVector<quint32>{1}));
    QCOMPARE(groups[5].eventIds, (QVector<quint32>{4}));
    QCOMPARE(groups[6].eventIds, (QVector<quint32>{5, 6}));
    QCOMPARE(toXml(track), original);

    QVERIFY(track.retime(8, 1200));
    groups = track.groups();
    QCOMPARE(groups[0].eventIds, (QVector<quint32>{90, 3}));
    QCOMPARE(groups[2].eventIds, (QVector<quint32>{70, 2, 51}));
    QVERIFY(track.retime(8, 1300));
    QVERIFY(track.remove(8));
    QCOMPARE(track.groups()[0].eventIds, (QVector<quint32>{90, 3, 70, 2, 51}));
    auto barrier = samples[3];
    barrier.order = 3;
    QVERIFY(track.restore(barrier));
    QVERIFY(track.retime(70, 2000));
    QVERIFY(track.retime(70, 1300));
    QCOMPARE(track.groups()[0].eventIds, (QVector<quint32>{90, 3, 70}));
    QCOMPARE(toXml(track), original);
}

void ShowCommandTrack_Test::groupsKeepInterleavedHoldPairsDistinct()
{
    ShowCommandTrack track;
    ShowCommand aOn = ShowCommand::setButtonState(1, 10, kControlA, true);
    aOn.role = ShowControlRole::FlashButton;
    aOn.pairId = 100;
    ShowCommand bOn = ShowCommand::setButtonState(2, 11, kControlB, true);
    bOn.role = ShowControlRole::FlashButton;
    bOn.pairId = 101;
    ShowCommand aOff = ShowCommand::setButtonState(3, 20, kControlA, false);
    aOff.role = ShowControlRole::FlashButton;
    aOff.pairId = 100;
    ShowCommand bOff = ShowCommand::setButtonState(4, 21, kControlB, false);
    bOff.role = ShowControlRole::FlashButton;
    bOff.pairId = 101;

    QVERIFY(track.insert(aOn));
    QVERIFY(track.insert(bOn));
    QVERIFY(track.insert(aOff));
    QVERIFY(track.insert(bOff));

    const QVector<ShowCommandGroup> groups = track.groups();
    QCOMPARE(groups.count(), 2);
    QCOMPARE(groups.at(0).eventIds, (QVector<quint32>{1, 3}));
    QCOMPARE(groups.at(0).startTime, 10u);
    QCOMPARE(groups.at(0).endTime, 20u);
    QCOMPARE(groups.at(1).eventIds, (QVector<quint32>{2, 4}));
    QCOMPARE(groups.at(1).startTime, 11u);
    QCOMPARE(groups.at(1).endTime, 21u);
}

void ShowCommandTrack_Test::commandValidation_data()
{
    QTest::addColumn<ShowCommand>("command");
    QTest::addColumn<bool>("valid");

    QTest::newRow("start") << ShowCommand::start(3, 12000, 42) << true;
    QTest::newRow("stop") << ShowCommand::stop(4, 12001, 42) << true;
    QTest::newRow("intensity mid") << ShowCommand::setIntensity(5, 700, 42, 0.375) << true;
    QTest::newRow("intensity zero") << ShowCommand::setIntensity(6, 700, 42, 0.0) << true;
    QTest::newRow("intensity full") << ShowCommand::setIntensity(7, 700, 42, 1.0) << true;
    QTest::newRow("time at limit") << ShowCommand::start(8, ShowCommand::MaxTime, 42) << true;

    QTest::newRow("invalid event id")
        << ShowCommand::start(ShowCommand::InvalidId, 700, 42) << false;
    QTest::newRow("invalid target")
        << ShowCommand::start(9, 700, ShowCommand::InvalidId) << false;
    QTest::newRow("time sentinel")
        << ShowCommand::start(10, ShowCommand::InvalidId, 42) << false;
    QTest::newRow("intensity above one")
        << ShowCommand::setIntensity(11, 700, 42, 1.5) << false;
    QTest::newRow("intensity below zero")
        << ShowCommand::setIntensity(12, 700, 42, -0.25) << false;
    QTest::newRow("intensity nan")
        << ShowCommand::setIntensity(13, 700, 42, qQNaN()) << false;
    QTest::newRow("intensity infinite")
        << ShowCommand::setIntensity(14, 700, 42, qInf()) << false;

    ShowCommand loadedTrigger = ShowCommand::start(15, 700, 42);
    loadedTrigger.intensity = 0.5;
    QTest::newRow("trigger carrying a value") << loadedTrigger << false;

    ShowCommand unknownAction = ShowCommand::start(16, 700, 42);
    unknownAction.action = static_cast<ShowCommandAction>(255);
    QTest::newRow("action outside the enum") << unknownAction << false;

    // VC state records name a control, never a Function
    QTest::newRow("button on") << ShowCommand::setButtonState(20, 800, kControlA, true) << true;
    QTest::newRow("button off") << ShowCommand::setButtonState(21, 800, kControlA, false) << true;
    QTest::newRow("level slider")
        << ShowCommand::setSliderPosition(22, 900, kControlB, ShowControlRole::LevelSlider,
                                          QString(), 0.25) << true;
    QTest::newRow("adjust slider")
        << ShowCommand::setSliderPosition(23, 900, kControlB, ShowControlRole::AdjustSlider,
                                          QStringLiteral("Intensity"), 0.75) << true;
    QTest::newRow("submaster at zero")
        << ShowCommand::setSliderPosition(24, 900, kControlB, ShowControlRole::SubmasterSlider,
                                          QString(), 0.0) << true;
    QTest::newRow("grand master at full")
        << ShowCommand::setSliderPosition(25, 900, kControlB, ShowControlRole::GrandMasterSlider,
                                          QString(), 1.0) << true;
    QTest::newRow("slider reset")
        << ShowCommand::setSliderReset(26, 900, kControlB, ShowControlRole::LevelSlider,
                                       QString()) << true;
    QTest::newRow("adjust slider reset")
        << ShowCommand::setSliderReset(27, 900, kControlB, ShowControlRole::AdjustSlider,
                                       QStringLiteral("Intensity")) << true;
    QTest::newRow("slider colors")
        << ShowCommand::setSliderColors(28, 900, kControlB, ShowControlRole::LevelSlider,
                                        QStringLiteral("10,20,30;40,50,60"), 0.5) << true;
    QTest::newRow("xy pad position")
        << ShowCommand::setXYPadPosition(29, 900, kControlB, QStringLiteral("64.5,192.25")) << true;

    QTest::newRow("button without control")
        << ShowCommand::setButtonState(30, 800, QUuid(), true) << false;
    ShowCommand buttonWithFunction = ShowCommand::setButtonState(31, 800, kControlA, true);
    buttonWithFunction.functionId = 42;
    QTest::newRow("button with function") << buttonWithFunction << false;
    ShowCommand buttonAsSlider = ShowCommand::setButtonState(32, 800, kControlA, true);
    buttonAsSlider.role = ShowControlRole::LevelSlider;
    QTest::newRow("button with slider role") << buttonAsSlider << true;
    ShowCommand buttonWithPosition = ShowCommand::setButtonState(33, 800, kControlA, false);
    buttonWithPosition.position = 0.5;
    QTest::newRow("button carrying a position") << buttonWithPosition << false;
    QTest::newRow("slider with button role")
        << ShowCommand::setSliderPosition(34, 900, kControlB, ShowControlRole::ToggleButton,
                                          QString(), 0.5) << false;
    QTest::newRow("slider without role")
        << ShowCommand::setSliderPosition(35, 900, kControlB, ShowControlRole::None,
                                          QString(), 0.5) << false;
    QTest::newRow("level slider with attribute")
        << ShowCommand::setSliderPosition(36, 900, kControlB, ShowControlRole::LevelSlider,
                                          QStringLiteral("Intensity"), 0.5) << false;
    QTest::newRow("adjust slider without attribute")
        << ShowCommand::setSliderPosition(37, 900, kControlB, ShowControlRole::AdjustSlider,
                                          QString(), 0.5) << false;
    QTest::newRow("slider above one")
        << ShowCommand::setSliderPosition(38, 900, kControlB, ShowControlRole::LevelSlider,
                                          QString(), 1.5) << false;
    QTest::newRow("slider nan")
        << ShowCommand::setSliderPosition(39, 900, kControlB, ShowControlRole::LevelSlider,
                                          QString(), qQNaN()) << false;
    ShowCommand sliderOn = ShowCommand::setSliderPosition(40, 900, kControlB,
                                                          ShowControlRole::LevelSlider, QString(), 0.5);
    sliderOn.on = true;
    QTest::newRow("slider carrying a button state") << sliderOn << false;
    QTest::newRow("slider reset with button role")
        << ShowCommand::setSliderReset(41, 900, kControlB, ShowControlRole::ToggleButton,
                                       QString()) << false;
    QTest::newRow("slider reset missing adjust attribute")
        << ShowCommand::setSliderReset(42, 900, kControlB, ShowControlRole::AdjustSlider,
                                       QString()) << false;
    ShowCommand sliderResetWithValue = ShowCommand::setSliderReset(43, 900, kControlB,
                                                                    ShowControlRole::LevelSlider, QString());
    sliderResetWithValue.position = 0.5;
    QTest::newRow("slider reset carrying value") << sliderResetWithValue << false;
    ShowCommand sliderColorsNoPayload = ShowCommand::setSliderColors(44, 900, kControlB,
                                                                      ShowControlRole::LevelSlider,
                                                                      QString(), 0.5);
    QTest::newRow("slider colors missing payload") << sliderColorsNoPayload << false;
    ShowCommand xyPadWithSliderRole = ShowCommand::setXYPadPosition(45, 900, kControlB,
                                                                     QStringLiteral("10,20"));
    xyPadWithSliderRole.role = ShowControlRole::LevelSlider;
    QTest::newRow("xy pad position with slider role") << xyPadWithSliderRole << false;
    ShowCommand xyPadNoPayload = ShowCommand::setXYPadPosition(46, 900, kControlB, QString());
    QTest::newRow("xy pad position missing payload") << xyPadNoPayload << false;

    ShowCommand legacyWithControl = ShowCommand::start(50, 700, 42);
    legacyWithControl.controlId = kControlA;
    QTest::newRow("function command with control") << legacyWithControl << false;
    ShowCommand legacyWithRole = ShowCommand::stop(51, 700, 42);
    legacyWithRole.role = ShowControlRole::ToggleButton;
    QTest::newRow("function command with role") << legacyWithRole << false;
    ShowCommand legacyWithAttribute = ShowCommand::setIntensity(52, 700, 42, 0.5);
    legacyWithAttribute.attribute = QStringLiteral("Intensity");
    QTest::newRow("function command with attribute") << legacyWithAttribute << false;
    ShowCommand legacyWithState = ShowCommand::start(53, 700, 42);
    legacyWithState.on = true;
    QTest::newRow("function command with button state") << legacyWithState << false;
}

void ShowCommandTrack_Test::commandValidation()
{
    QFETCH(ShowCommand, command);
    QFETCH(bool, valid);

    const QString error = ShowCommand::validate(command);
    QCOMPARE(error.isEmpty(), valid);
    if (command.action == static_cast<ShowCommandAction>(255))
        QCOMPARE(error, QStringLiteral("unknown action 255"));

    ShowCommandTrack track;
    QCOMPARE(track.insert(command), valid);
    QCOMPARE(track.count(), valid ? 1 : 0);
}

void ShowCommandTrack_Test::insertKeepsTimeAndInsertionOrder()
{
    const ShowCommandTrack track = fixtureTrack();

    QCOMPARE(track.count(), 5);
    QCOMPARE(idsOf(track), QVector<quint32>({7, 8, 9, 10, 11}));

    const QVector<ShowCommand> &cmds = track.commands();
    QCOMPARE(cmds.at(0).time, 0u);
    QCOMPARE(cmds.at(1).time, 0u);
    QCOMPARE(cmds.at(1).functionId, 30u);
    QVERIFY(cmds.at(2).action == ShowCommandAction::SetIntensity);
    QCOMPARE(cmds.at(2).intensity, 0.375);
    QVERIFY(cmds.at(3).action == ShowCommandAction::Stop);
    QCOMPARE(cmds.at(4).intensity, 0.8);

    QCOMPARE(track.indexOfId(10), 3);
    QCOMPARE(track.indexOfId(99), -1);
    QVERIFY(track.contains(9));
    QVERIFY(!track.contains(99));
    QCOMPARE(track.nextEventId(), 12u);
}

void ShowCommandTrack_Test::insertRejectsDuplicateId()
{
    ShowCommandTrack track = fixtureTrack();
    QString error;

    QVERIFY(!track.insert(ShowCommand::stop(9, 6000, 30), &error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(track.count(), 5);
    QCOMPARE(idsOf(track), QVector<quint32>({7, 8, 9, 10, 11}));
    QVERIFY(track.commands().at(2).action == ShowCommandAction::SetIntensity);
}

void ShowCommandTrack_Test::replaceEditsOnlyTheAddressedCommand()
{
    ShowCommandTrack track = fixtureTrack();
    QString error;

    QVERIFY(track.replace(ShowCommand::setIntensity(11, 4200, 12, 0.42), &error));
    QVERIFY(error.isEmpty());
    QCOMPARE(idsOf(track), QVector<quint32>({7, 8, 9, 10, 11}));
    QCOMPARE(track.commands().at(4).intensity, 0.42);
    QCOMPARE(track.commands().at(2).intensity, 0.375);

    QVERIFY(!track.replace(ShowCommand::start(99, 100, 12), &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!track.replace(ShowCommand::setIntensity(11, 4200, 12, 2.0), &error));
    QCOMPARE(track.commands().at(4).intensity, 0.42);
    QCOMPARE(track.count(), 5);
}

void ShowCommandTrack_Test::retimeMovesOneCommand()
{
    ShowCommandTrack track = fixtureTrack();
    QString error;

    // an event keeps its equal-time place: 7 was inserted before 10 and 11
    QVERIFY(track.retime(7, 4200, &error));
    QVERIFY(error.isEmpty());
    QCOMPARE(idsOf(track), QVector<quint32>({8, 9, 7, 10, 11}));
    QCOMPARE(track.commands().at(2).time, 4200u);
    QCOMPARE(track.commands().at(2).functionId, 12u);
    QVERIFY(track.commands().at(2).action == ShowCommandAction::Start);
    QCOMPARE(track.lastCommandTime(), 4200u);

    QVERIFY(!track.retime(99, 100, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!track.retime(7, ShowCommand::InvalidId, &error));
    QCOMPARE(track.commands().at(2).time, 4200u);
}

void ShowCommandTrack_Test::retimeSelection_data()
{
    // track: "id@time" inserted in this order, so a later entry has the higher Order;
    // result: every command "id@time" in stored order, or the refusal
    QTest::addColumn<QString>("track");
    QTest::addColumn<QString>("kind");
    QTest::addColumn<qint64>("delta");
    QTest::addColumn<QString>("ids");
    QTest::addColumn<QString>("result");

    const QString three = QStringLiteral("1@1000 2@1500 3@2000 9@1600");
    QTest::newRow("move by one delta, unselected stays")
        << three << "move" << qint64(100) << "1 3" << "1@1100 2@1500 9@1600 3@2100";
    QTest::newRow("move may cross an unselected event")
        << three << "move" << qint64(-700) << "3" << "1@1000 3@1300 2@1500 9@1600";
    QTest::newRow("right edge doubles, earliest pinned")
        << three << "end" << qint64(1000) << "1 2 3" << "1@1000 9@1600 2@2000 3@3000";
    QTest::newRow("left edge doubles, latest pinned")
        << three << "start" << qint64(-1000) << "1 2 3" << "1@0 2@1000 9@1600 3@2000";
    QTest::newRow("right edge halves")
        << three << "end" << qint64(-500) << "1 2 3" << "1@1000 2@1250 3@1500 9@1600";
    QTest::newRow("left edge halves")
        << three << "start" << qint64(500) << "1 2 3" << "1@1500 9@1600 2@1750 3@2000";
    QTest::newRow("partial selection inside an unselected range")
        << three << "end" << qint64(1000) << "1 2" << "1@1000 9@1600 3@2000 2@2500";
    QTest::newRow("rounding half up, ties keep ascending Order")
        << "1@1000 2@1001 3@1003" << "end" << qint64(-2) << "1 2 3" << "1@1000 2@1000 3@1001";
    QTest::newRow("rounding tie that would reverse the selected sequence")
        << "2@1001 1@1000 3@1003" << "end" << qint64(-2) << "1 2 3" << "reordered";
    QTest::newRow("existing equal times keep their sequence when scaled")
        << "1@1000 2@1000 3@2000" << "start" << qint64(-1000) << "1 2 3" << "1@0 2@0 3@2000";
    QTest::newRow("no time span cannot stretch")
        << "1@1000 2@1000" << "end" << qint64(500) << "1 2" << "no span";
    QTest::newRow("no time span still moves")
        << "1@1000 2@1000" << "move" << qint64(500) << "1 2" << "1@1500 2@1500";
    QTest::newRow("a single event cannot stretch")
        << three << "start" << qint64(-10) << "2" << "no span";
    QTest::newRow("right edge onto the pinned edge")
        << three << "end" << qint64(-1000) << "1 2 3" << "no target span";
    QTest::newRow("left edge past the pinned edge")
        << three << "start" << qint64(1500) << "1 2 3" << "no target span";
    QTest::newRow("move before zero")
        << three << "move" << qint64(-1001) << "1 3" << "out of range";
    QTest::newRow("left edge before zero")
        << three << "start" << qint64(-1001) << "1 2 3" << "out of range";
    QTest::newRow("right edge past the last time")
        << "1@0 2@4294967000" << "end" << qint64(295) << "1 2" << "out of range";
    QTest::newRow("right edge onto the last time, full range exact")
        << "1@0 2@2147483647 3@4294967000" << "end" << qint64(294) << "1 2 3" << "1@0 2@2147483794 3@4294967294";
    QTest::newRow("full range compressed by one, products past 63 bits")
        << "1@0 2@2147483647 4@4294967000 3@4294967294" << "end" << qint64(-1) << "1 2 4 3"
        << "1@0 2@2147483647 4@4294966999 3@4294967293";
    QTest::newRow("full range from the left, negative offsets")
        << "1@0 2@2147483647 4@4294967000 3@4294967294" << "start" << qint64(1) << "1 2 4 3"
        << "1@1 2@2147483647 4@4294967000 3@4294967294";
    QTest::newRow("unknown id refuses the whole edit")
        << three << "move" << qint64(100) << "1 7" << "unknown event";
    QTest::newRow("nothing selected")
        << three << "move" << qint64(100) << "" << "unknown event";
}

void ShowCommandTrack_Test::retimeSelection()
{
    QFETCH(QString, track);
    QFETCH(QString, kind);
    QFETCH(qint64, delta);
    QFETCH(QString, ids);
    QFETCH(QString, result);

    ShowCommandTrack source;
    for (const QString &entry : track.split(QLatin1Char(' '), Qt::SkipEmptyParts))
        QVERIFY(source.insert(ShowCommand::start(entry.section(QLatin1Char('@'), 0, 0).toUInt(),
                                                 entry.section(QLatin1Char('@'), 1, 1).toUInt(), 30)));
    QVector<quint32> selected;
    for (const QString &id : ids.split(QLatin1Char(' '), Qt::SkipEmptyParts))
        selected.append(id.toUInt());
    const ShowRetimeKind how = kind == QLatin1String("move") ? ShowRetimeKind::Move
                             : kind == QLatin1String("end") ? ShowRetimeKind::StretchEnd
                                                            : ShowRetimeKind::StretchStart;

    ShowCommandTrack edited = source;
    const ShowRetimeRefusal refusal = source.retimeSelection(selected, how, delta, &edited);

    static const QHash<QString, ShowRetimeRefusal> refusals = {
        { "unknown event", ShowRetimeRefusal::UnknownEvent }, { "no span", ShowRetimeRefusal::NoSpan },
        { "no target span", ShowRetimeRefusal::NoTargetSpan }, { "out of range", ShowRetimeRefusal::OutOfRange },
        { "reordered", ShowRetimeRefusal::Reordered } };
    if (refusals.contains(result))
    {
        QCOMPARE(int(refusal), int(refusals.value(result)));
        // refused whole: the result is untouched
        QCOMPARE(edited.commands(), source.commands());
        return;
    }
    QCOMPARE(int(refusal), int(ShowRetimeRefusal::None));
    QStringList got;
    for (const ShowCommand &cmd : edited.commands())
        got.append(QStringLiteral("%1@%2").arg(cmd.id).arg(cmd.time));
    QCOMPARE(got.join(QLatin1Char(' ')), result);
    // only times change: every other field and the equal-time Order stay
    for (const ShowCommand &cmd : source.commands())
    {
        ShowCommand after = edited.commands().at(edited.indexOfId(cmd.id));
        QCOMPARE(after.order, cmd.order);
        after.time = cmd.time;
        QCOMPARE(after, cmd);
    }
    QCOMPARE(edited.nextEventId(), source.nextEventId());
    QCOMPARE(edited.nextOrder(), source.nextOrder());
}

void ShowCommandTrack_Test::removeLeavesTheRestUntouched()
{
    ShowCommandTrack track = fixtureTrack();
    QString error;

    QVERIFY(track.remove(9, &error));
    QVERIFY(error.isEmpty());
    QCOMPARE(idsOf(track), QVector<quint32>({7, 8, 10, 11}));
    QCOMPARE(track.nextEventId(), 12u);

    QVERIFY(!track.remove(9, &error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(track.count(), 4);
}

void ShowCommandTrack_Test::removedHighestIdIsNotReused()
{
    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::start(0, 0, 12)));
    QVERIFY(track.insert(ShowCommand::setButtonState(1, 300, kControlA, true)));
    QVERIFY(track.insert(ShowCommand::stop(2, 600, 12)));

    QVERIFY(track.remove(2));
    QCOMPARE(track.nextEventId(), 3u);

    // the high water mark travels with the value
    const ShowCommandTrack copy = track;
    QCOMPARE(copy.nextEventId(), 3u);
}

void ShowCommandTrack_Test::extentIsAuthoredIndependently()
{
    ShowCommandTrack track = fixtureTrack();

    QCOMPARE(track.extent(), 9000u);
    QCOMPARE(track.lastCommandTime(), 4200u);

    // editing commands never rewrites the authored extent
    QVERIFY(track.remove(11));
    QVERIFY(track.insert(ShowCommand::start(20, 12500, 30)));
    QCOMPARE(track.extent(), 9000u);
    QCOMPARE(track.lastCommandTime(), 12500u);

    // an extent shorter than the last command is the host's decision, not an error
    QString error = QStringLiteral("untouched");
    QVERIFY(track.setExtent(600, &error));
    QVERIFY(error.isEmpty());
    QCOMPARE(track.extent(), 600u);
    QCOMPARE(track.count(), 5);

    // the unset-time sentinel is not an extent, and a rejected edit keeps the old one
    QVERIFY(!track.setExtent(ShowCommand::InvalidId, &error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(track.extent(), 600u);

    // the highest supported extent survives a round trip
    QVERIFY(track.setExtent(ShowCommand::MaxTime, &error));
    QVERIFY(error.isEmpty());
    ShowCommandTrack reloaded;
    QVERIFY(fromXml(toXml(track), &reloaded, &error));
    QCOMPARE(reloaded.extent(), ShowCommand::MaxTime);

    const ShowCommandTrack empty;
    QVERIFY(empty.isEmpty());
    QCOMPARE(empty.extent(), 0u);
    QCOMPARE(empty.lastCommandTime(), 0u);
    QCOMPARE(empty.nextEventId(), 0u);
}

void ShowCommandTrack_Test::referencedTargetsAndRemap()
{
    ShowCommandTrack track = fixtureTrack();

    QCOMPARE(track.referencedFunctionIds(), QList<quint32>({12, 30}));

    QCOMPARE(track.remapFunctionId(12, 77), 3);
    QCOMPARE(track.referencedFunctionIds(), QList<quint32>({30, 77}));
    QCOMPARE(track.commands().at(0).functionId, 77u);
    QCOMPARE(track.commands().at(1).functionId, 30u);

    QCOMPARE(track.remapFunctionId(55, 88), 0);
    QCOMPARE(track.remapFunctionId(77, ShowCommand::InvalidId), 0);
    QCOMPARE(track.referencedFunctionIds(), QList<quint32>({30, 77}));
}

void ShowCommandTrack_Test::referencedControlsGroupRepeatedRecords()
{
    const QUuid level(QStringLiteral("{c3d4e5f6-0718-4293-a4b5-c6d7e8f90a1b}"));
    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::setButtonState(0, 900, kControlA, false)));
    QVERIFY(track.insert(ShowCommand::start(1, 100, 12)));
    QVERIFY(track.insert(ShowCommand::setSliderPosition(2, 200, level, ShowControlRole::LevelSlider,
                                                        QString(), 0.3)));
    QVERIFY(track.insert(ShowCommand::setButtonState(3, 100, kControlA, true)));
    QVERIFY(track.insert(ShowCommand::setSliderPosition(4, 400, kControlB, ShowControlRole::AdjustSlider,
                                                        QStringLiteral("Intensity"), 0.6)));
    QVERIFY(track.insert(ShowCommand::setButtonState(5, 1200, kControlA, true)));

    const QVector<ShowControlReference> refs = track.referencedControls();

    // authored order of first reference, legacy commands imply none
    QCOMPARE(refs.count(), 3);
    QCOMPARE(refs.at(0).controlId, kControlA);
    QCOMPARE(int(refs.at(0).role), int(ShowControlRole::ToggleButton));
    QCOMPARE(refs.at(0).count, 3);
    QCOMPARE(refs.at(1).controlId, level);
    QCOMPARE(int(refs.at(1).role), int(ShowControlRole::LevelSlider));
    QCOMPARE(refs.at(1).count, 1);
    QCOMPARE(refs.at(2).controlId, kControlB);
    QCOMPARE(int(refs.at(2).role), int(ShowControlRole::AdjustSlider));
    QCOMPARE(refs.at(2).attribute, QStringLiteral("Intensity"));
    QCOMPARE(refs.at(2).count, 1);
    QCOMPARE(track.referencedFunctionIds(), QList<quint32>({12}));
}

void ShowCommandTrack_Test::saveWritesNamedVersionedFields()
{
    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::start(0, 0, 12)));
    QVERIFY(track.insert(ShowCommand::setIntensity(1, 1500, 12, 0.375)));
    QVERIFY(track.insert(ShowCommand::stop(2, 4200, 12)));
    track.setExtent(9000);

    QCOMPARE(toXml(track),
             QStringLiteral("<CommandTrack Version=\"1\" Extent=\"9000\">"
                            "<Command ID=\"0\" Time=\"0\" Action=\"Start\" Function=\"12\"/>"
                            "<Command ID=\"1\" Time=\"1500\" Action=\"SetIntensity\" Function=\"12\" Value=\"0.375\"/>"
                            "<Command ID=\"2\" Time=\"4200\" Action=\"Stop\" Function=\"12\"/>"
                            "</CommandTrack>"));
}

void ShowCommandTrack_Test::saveLoadRoundTripPreservesOrderAndValues()
{
    ShowCommandTrack saved = fixtureTrack();
    // a value that needs more than the default six digits to survive a round trip
    QVERIFY(saved.insert(ShowCommand::setIntensity(12, 1500, 30, 0.123456789012345)));

    ShowCommandTrack loaded;
    QString error = QStringLiteral("untouched");
    QVERIFY(fromXml(toXml(saved), &loaded, &error));
    QVERIFY(error.isEmpty());

    QCOMPARE(loaded.count(), saved.count());
    QCOMPARE(idsOf(loaded), QVector<quint32>({7, 8, 9, 12, 10, 11}));
    QCOMPARE(loaded.extent(), 9000u);
    QCOMPARE(loaded.lastCommandTime(), 4200u);
    QCOMPARE(loaded.nextEventId(), 13u);

    for (int i = 0; i < saved.count(); i++)
        QVERIFY(loaded.commands().at(i) == saved.commands().at(i));

    QCOMPARE(loaded.commands().at(3).intensity, 0.123456789012345);
    QCOMPARE(toXml(loaded), toXml(saved));
}

void ShowCommandTrack_Test::controlStatesSaveAsVersionTwoAndRoundTrip()
{
    const QUuid level(QStringLiteral("{c3d4e5f6-0718-4293-a4b5-c6d7e8f90a1b}"));
    ShowCommandTrack saved;
    // equal times mixing both kinds: insertion order is the saved order
    QVERIFY(saved.insert(ShowCommand::start(0, 0, 12)));
    QVERIFY(saved.insert(ShowCommand::setButtonState(1, 500, kControlA, true)));
    QVERIFY(saved.insert(ShowCommand::setSliderPosition(2, 500, kControlB, ShowControlRole::AdjustSlider,
                                                        QStringLiteral("Intensity"), 0.123456789012345)));
    QVERIFY(saved.insert(ShowCommand::setIntensity(3, 500, 12, 0.4)));
    QVERIFY(saved.insert(ShowCommand::setButtonState(4, 500, kControlA, false)));
    QVERIFY(saved.insert(ShowCommand::setSliderPosition(5, 800, level, ShowControlRole::LevelSlider,
                                                        QString(), 1.0)));
    saved.setExtent(9000);

    const QString xml = toXml(saved);
    QCOMPARE(xml,
             QStringLiteral("<CommandTrack Version=\"2\" Extent=\"9000\">"
                            "<Command ID=\"0\" Time=\"0\" Action=\"Start\" Function=\"12\"/>"
                            "<Command ID=\"1\" Time=\"500\" Action=\"SetButtonState\" "
                            "Control=\"{6f1c2d3e-4a5b-4c6d-8e7f-001122334455}\" Role=\"ToggleButton\" State=\"On\"/>"
                            "<Command ID=\"2\" Time=\"500\" Action=\"SetSliderPosition\" "
                            "Control=\"{0a9b8c7d-6e5f-4a3b-9c1d-5566778899aa}\" Role=\"AdjustSlider\" "
                            "Value=\"0.123456789012345\" Attribute=\"Intensity\"/>"
                            "<Command ID=\"3\" Time=\"500\" Action=\"SetIntensity\" Function=\"12\" Value=\"0.4\"/>"
                            "<Command ID=\"4\" Time=\"500\" Action=\"SetButtonState\" "
                            "Control=\"{6f1c2d3e-4a5b-4c6d-8e7f-001122334455}\" Role=\"ToggleButton\" State=\"Off\"/>"
                            "<Command ID=\"5\" Time=\"800\" Action=\"SetSliderPosition\" "
                            "Control=\"{c3d4e5f6-0718-4293-a4b5-c6d7e8f90a1b}\" Role=\"LevelSlider\" Value=\"1\"/>"
                            "</CommandTrack>"));

    ShowCommandTrack loaded;
    QString error = QStringLiteral("untouched");
    QVERIFY2(fromXml(xml, &loaded, &error), qPrintable(error));
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(idsOf(loaded), QVector<quint32>({0, 1, 2, 3, 4, 5}));
    for (int i = 0; i < saved.count(); i++)
        QVERIFY(loaded.commands().at(i) == saved.commands().at(i));
    QCOMPARE(toXml(loaded), xml);
}

void ShowCommandTrack_Test::controlStatesWithSliderColorsSaveAsVersionFiveAndRoundTrip()
{
    ShowCommandTrack saved;
    QVERIFY(saved.insert(ShowCommand::setSliderColors(0, 500, kControlB, ShowControlRole::LevelSlider,
                                                      QStringLiteral("10,20,30;40,50,60"), 0.5019607843137255)));
    saved.setExtent(9000);

    const QString xml = toXml(saved);
    QCOMPARE(xml,
             QStringLiteral("<CommandTrack Version=\"5\" Extent=\"9000\">"
                            "<Command ID=\"0\" Time=\"500\" Action=\"SetSliderColors\" "
                            "Control=\"{0a9b8c7d-6e5f-4a3b-9c1d-5566778899aa}\" Role=\"LevelSlider\" "
                            "Value=\"0.5019607843137255\" Attribute=\"10,20,30;40,50,60\"/>"
                            "</CommandTrack>"));

    ShowCommandTrack loaded;
    QString error = QStringLiteral("untouched");
    QVERIFY(fromXml(xml, &loaded, &error));
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(loaded.count(), 1);
    QCOMPARE(loaded.commands().first(), saved.commands().first());
}

void ShowCommandTrack_Test::controlStatesWithAnimationFaderSaveAsVersionSevenAndRoundTrip()
{
    ShowCommandTrack saved;
    QVERIFY(saved.insert(ShowCommand::setAnimationFader(0, 500, kControlB, 0.5)));
    saved.setExtent(9000);

    const QString xml = toXml(saved);
    QCOMPARE(xml,
             QStringLiteral("<CommandTrack Version=\"7\" Extent=\"9000\">"
                            "<Command ID=\"0\" Time=\"500\" Action=\"SetAnimationFader\" "
                            "Control=\"{0a9b8c7d-6e5f-4a3b-9c1d-5566778899aa}\" Role=\"AnimationFader\" "
                            "Value=\"0.5\"/>"
                            "</CommandTrack>"));

    ShowCommandTrack loaded;
    QString error = QStringLiteral("untouched");
    QVERIFY(fromXml(xml, &loaded, &error));
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(loaded.count(), 1);
    QCOMPARE(loaded.commands().first(), saved.commands().first());
}

void ShowCommandTrack_Test::loadReplacesPreviousContents()
{
    const ShowCommandTrack empty;
    const QString emptyXml = toXml(empty);
    QCOMPARE(emptyXml, QStringLiteral("<CommandTrack Version=\"1\" Extent=\"0\"/>"));

    ShowCommandTrack track = fixtureTrack();
    QVERIFY(fromXml(emptyXml, &track));
    QVERIFY(track.isEmpty());
    QCOMPARE(track.extent(), 0u);
    QCOMPARE(track.nextEventId(), 0u);

    QVERIFY(fromXml(toXml(fixtureTrack()), &track));
    QCOMPARE(track.count(), 5);
    QCOMPARE(track.extent(), 9000u);
}

void ShowCommandTrack_Test::complexPayloadRejection_data()
{
    QTest::addColumn<ShowCommandAction>("action");
    QTest::addColumn<QString>("payload");
    QTest::newRow("color garbage") << ShowCommandAction::SetSliderColors << QString("invalid");
    QTest::newRow("color missing component") << ShowCommandAction::SetSliderColors << QString("1,2;3,4,5");
    QTest::newRow("color overflow") << ShowCommandAction::SetSliderColors << QString("256,2,3;4,5,6");
    QTest::newRow("color fractional") << ShowCommandAction::SetSliderColors << QString("1.5,2,3;4,5,6");
    QTest::newRow("color negative") << ShowCommandAction::SetSliderColors << QString("1,2,3;-1,5,6");
    QTest::newRow("xy garbage") << ShowCommandAction::SetXYPadPosition << QString("invalid");
    QTest::newRow("xy missing axis") << ShowCommandAction::SetXYPadPosition << QString("64.5");
    QTest::newRow("xy extra axis") << ShowCommandAction::SetXYPadPosition << QString("1,2,3");
    QTest::newRow("xy nonfinite") << ShowCommandAction::SetXYPadPosition << QString("nan,192");
    QTest::newRow("xy negative") << ShowCommandAction::SetXYPadPosition << QString("-1,192");
    QTest::newRow("xy overflow") << ShowCommandAction::SetXYPadPosition << QString("64,256");
}

void ShowCommandTrack_Test::complexPayloadRejection()
{
    QFETCH(ShowCommandAction, action);
    QFETCH(QString, payload);
    ShowCommandTrack track = fixtureTrack();
    const QString before = toXml(track);
    ShowCommand command = action == ShowCommandAction::SetSliderColors
        ? ShowCommand::setSliderColors(50, 1200, kControlA, ShowControlRole::LevelSlider, payload, .5)
        : ShowCommand::setXYPadPosition(50, 1200, kControlA, payload);
    QString reason;
    QVERIFY(!track.insert(command, &reason));
    QVERIFY(!reason.isEmpty());
    QCOMPARE(toXml(track), before);
    command.id = 9;
    QVERIFY(!track.replace(command, &reason));
    QVERIFY(!reason.isEmpty());
    QCOMPARE(toXml(track), before);

    const QString xml = QString(
        "<CommandTrack Version=\"%1\"><Command ID=\"50\" Time=\"1200\" Action=\"%2\" "
        "Control=\"%3\" Role=\"%4\" Attribute=\"%5\" %6/></CommandTrack>")
        .arg(action == ShowCommandAction::SetSliderColors ? 5 : 6)
        .arg(ShowCommand::actionToString(action), kControlA.toString(),
             ShowCommand::roleToString(command.role), payload,
             action == ShowCommandAction::SetSliderColors ? "Value=\"0.5\"" : "");
    QVERIFY(!fromXml(xml, &track, &reason));
    QVERIFY(!reason.isEmpty());
    QCOMPARE(toXml(track), before);
}

void ShowCommandTrack_Test::loadIsTransactional_data()
{
    QTest::addColumn<QString>("xml");
    QTest::addColumn<bool>("loadable");
    QTest::addColumn<int>("count");

    const QString start = QStringLiteral("<Command ID=\"1\" Time=\"1200\" Action=\"Start\" Function=\"12\"/>");

    auto track = [](const QString &attrs, const QString &body)
    { return QStringLiteral("<CommandTrack %1>%2</CommandTrack>").arg(attrs, body); };

    QTest::newRow("version only") << track("Version=\"1\"", QString()) << true << 0;
    QTest::newRow("commands and extent")
        << track("Version=\"1\" Extent=\"30000\"", start + QStringLiteral(
               "<Command ID=\"2\" Time=\"1200\" Action=\"SetIntensity\" Function=\"12\" Value=\"0.64\"/>"))
        << true << 2;
    QTest::newRow("unknown attribute is ignored")
        << track("Version=\"1\" Comment=\"hand edited\"", start) << true << 1;

    QTest::newRow("missing version") << track("Extent=\"30000\"", start) << false << 5;
    QTest::newRow("future version")
        << track(QStringLiteral("Version=\"%1\"").arg(ShowCommandTrack::Version + 1), start) << false << 5;

    const QString button = QStringLiteral(
        "<Command ID=\"2\" Time=\"1200\" Action=\"SetButtonState\" "
        "Control=\"{6f1c2d3e-4a5b-4c6d-8e7f-001122334455}\" Role=\"ToggleButton\" State=\"On\"/>");
    auto vc = [](const QString &attrs)
    { return QStringLiteral("<Command ID=\"3\" Time=\"1300\" %1/>").arg(attrs); };
    const QString control = QStringLiteral("Control=\"{0a9b8c7d-6e5f-4a3b-9c1d-5566778899aa}\"");

    QTest::newRow("v2 function commands only") << track("Version=\"2\"", start) << true << 1;
    QTest::newRow("v2 mixed")
        << track("Version=\"2\"", start + button + vc(QStringLiteral(
               "Action=\"SetSliderPosition\" %1 Role=\"AdjustSlider\" Value=\"0.25\" Attribute=\"Intensity\"").arg(control)))
        << true << 3;
    QTest::newRow("v3 flash hold pair")
        << track("Version=\"3\"", QStringLiteral(
               "<Command ID=\"10\" Time=\"1200\" Action=\"SetButtonState\" "
               "Control=\"{6f1c2d3e-4a5b-4c6d-8e7f-001122334455}\" Role=\"FlashButton\" State=\"On\" Pair=\"7\"/>"
               "<Command ID=\"11\" Time=\"1800\" Action=\"SetButtonState\" "
               "Control=\"{6f1c2d3e-4a5b-4c6d-8e7f-001122334455}\" Role=\"FlashButton\" State=\"Off\" Pair=\"7\"/>"))
        << true << 2;
    QTest::newRow("v3 orphan hold pair")
        << track("Version=\"3\"", QStringLiteral(
               "<Command ID=\"10\" Time=\"1200\" Action=\"SetButtonState\" "
               "Control=\"{6f1c2d3e-4a5b-4c6d-8e7f-001122334455}\" Role=\"FlashButton\" State=\"On\" Pair=\"7\"/>"))
        << false << 5;
    QTest::newRow("v3 pair with same edge state")
        << track("Version=\"3\"", QStringLiteral(
               "<Command ID=\"10\" Time=\"1200\" Action=\"SetButtonState\" "
               "Control=\"{6f1c2d3e-4a5b-4c6d-8e7f-001122334455}\" Role=\"FlashButton\" State=\"On\" Pair=\"7\"/>"
               "<Command ID=\"11\" Time=\"1800\" Action=\"SetButtonState\" "
               "Control=\"{6f1c2d3e-4a5b-4c6d-8e7f-001122334455}\" Role=\"FlashButton\" State=\"On\" Pair=\"7\"/>"))
        << false << 5;
    QTest::newRow("v3 pair with mixed controls")
        << track("Version=\"3\"", QStringLiteral(
               "<Command ID=\"10\" Time=\"1200\" Action=\"SetButtonState\" "
               "Control=\"{6f1c2d3e-4a5b-4c6d-8e7f-001122334455}\" Role=\"FlashButton\" State=\"On\" Pair=\"7\"/>"
               "<Command ID=\"11\" Time=\"1800\" Action=\"SetButtonState\" "
               "Control=\"{0a9b8c7d-6e5f-4a3b-9c1d-5566778899aa}\" Role=\"FlashButton\" State=\"Off\" Pair=\"7\"/>"))
        << false << 5;
    QTest::newRow("v4 slider reset")
        << track("Version=\"4\"", QStringLiteral(
               "<Command ID=\"10\" Time=\"1200\" Action=\"SetSliderReset\" "
               "Control=\"{0a9b8c7d-6e5f-4a3b-9c1d-5566778899aa}\" Role=\"LevelSlider\"/>"))
        << true << 1;
    QTest::newRow("v3 slider reset rejected")
        << track("Version=\"3\"", QStringLiteral(
               "<Command ID=\"10\" Time=\"1200\" Action=\"SetSliderReset\" "
               "Control=\"{0a9b8c7d-6e5f-4a3b-9c1d-5566778899aa}\" Role=\"LevelSlider\"/>"))
        << false << 5;
    QTest::newRow("v5 slider colors")
        << track("Version=\"5\"", QStringLiteral(
               "<Command ID=\"10\" Time=\"1200\" Action=\"SetSliderColors\" "
               "Control=\"{0a9b8c7d-6e5f-4a3b-9c1d-5566778899aa}\" Role=\"LevelSlider\" "
               "Value=\"0.5\" Attribute=\"10,20,30;40,50,60\"/>"))
        << true << 1;
    QTest::newRow("v6 xy pad position")
        << track("Version=\"6\"", QStringLiteral(
               "<Command ID=\"11\" Time=\"1300\" Action=\"SetXYPadPosition\" "
               "Control=\"{0a9b8c7d-6e5f-4a3b-9c1d-5566778899aa}\" Role=\"XYPad\" "
               "Attribute=\"64.5,192.25\"/>"))
        << true << 1;
    QTest::newRow("v5 xy pad position rejected")
        << track("Version=\"5\"", QStringLiteral(
               "<Command ID=\"11\" Time=\"1300\" Action=\"SetXYPadPosition\" "
               "Control=\"{0a9b8c7d-6e5f-4a3b-9c1d-5566778899aa}\" Role=\"XYPad\" "
               "Attribute=\"64.5,192.25\"/>"))
        << false << 5;
    QTest::newRow("v6 xy pad position missing payload")
        << track("Version=\"6\"", QStringLiteral(
               "<Command ID=\"11\" Time=\"1300\" Action=\"SetXYPadPosition\" "
               "Control=\"{0a9b8c7d-6e5f-4a3b-9c1d-5566778899aa}\" Role=\"XYPad\"/>"))
        << false << 5;
    QTest::newRow("v6 xy pad position carrying value")
        << track("Version=\"6\"", QStringLiteral(
               "<Command ID=\"11\" Time=\"1300\" Action=\"SetXYPadPosition\" "
               "Control=\"{0a9b8c7d-6e5f-4a3b-9c1d-5566778899aa}\" Role=\"XYPad\" "
               "Value=\"0.5\" Attribute=\"64.5,192.25\"/>"))
        << false << 5;
    QTest::newRow("v7 animation fader")
        << track("Version=\"7\"", QStringLiteral(
               "<Command ID=\"12\" Time=\"1300\" Action=\"SetAnimationFader\" "
               "Control=\"{0a9b8c7d-6e5f-4a3b-9c1d-5566778899aa}\" Role=\"AnimationFader\" "
               "Value=\"0.5\"/>"))
        << true << 1;
    QTest::newRow("v6 animation fader rejected")
        << track("Version=\"6\"", QStringLiteral(
               "<Command ID=\"12\" Time=\"1300\" Action=\"SetAnimationFader\" "
               "Control=\"{0a9b8c7d-6e5f-4a3b-9c1d-5566778899aa}\" Role=\"AnimationFader\" "
               "Value=\"0.5\"/>"))
        << false << 5;
    QTest::newRow("v4 slider colors rejected")
        << track("Version=\"4\"", QStringLiteral(
               "<Command ID=\"10\" Time=\"1200\" Action=\"SetSliderColors\" "
               "Control=\"{0a9b8c7d-6e5f-4a3b-9c1d-5566778899aa}\" Role=\"LevelSlider\" "
               "Value=\"0.5\" Attribute=\"10,20,30;40,50,60\"/>"))
        << false << 5;
    QTest::newRow("v1 with control state") << track("Version=\"1\"", start + button) << false << 5;
    QTest::newRow("unknown role")
        << track("Version=\"2\"", vc(QStringLiteral(
               "Action=\"SetSliderPosition\" %1 Role=\"Crossfader\" Value=\"0.5\"").arg(control))) << false << 5;
    QTest::newRow("malformed control")
        << track("Version=\"2\"", vc(QStringLiteral(
               "Action=\"SetButtonState\" Control=\"not-a-uuid\" Role=\"ToggleButton\" State=\"On\""))) << false << 5;
    QTest::newRow("control with trailing garbage")
        << track("Version=\"2\"", vc(QStringLiteral(
               "Action=\"SetButtonState\" Control=\"{0a9b8c7d-6e5f-4a3b-9c1d-5566778899aa}junk\" "
               "Role=\"ToggleButton\" State=\"On\""))) << false << 5;
    QTest::newRow("control with unclosed brace")
        << track("Version=\"2\"", vc(QStringLiteral(
               "Action=\"SetButtonState\" Control=\"{0a9b8c7d-6e5f-4a3b-9c1d-5566778899aa\" "
               "Role=\"ToggleButton\" State=\"On\""))) << false << 5;
    QTest::newRow("null control")
        << track("Version=\"2\"", vc(QStringLiteral(
               "Action=\"SetButtonState\" Control=\"{00000000-0000-0000-0000-000000000000}\" "
               "Role=\"ToggleButton\" State=\"On\""))) << false << 5;
    QTest::newRow("unbraced control")
        << track("Version=\"2\"", start + vc(QStringLiteral(
               "Action=\"SetButtonState\" Control=\"0A9B8C7D-6E5F-4A3B-9C1D-5566778899AA\" "
               "Role=\"ToggleButton\" State=\"On\""))) << true << 2;
    QTest::newRow("missing control")
        << track("Version=\"2\"", vc(QStringLiteral(
               "Action=\"SetButtonState\" Role=\"ToggleButton\" State=\"On\""))) << false << 5;
    QTest::newRow("missing role")
        << track("Version=\"2\"", vc(QStringLiteral(
               "Action=\"SetButtonState\" %1 State=\"On\"").arg(control))) << false << 5;
    QTest::newRow("malformed state")
        << track("Version=\"2\"", vc(QStringLiteral(
               "Action=\"SetButtonState\" %1 Role=\"ToggleButton\" State=\"Maybe\"").arg(control))) << false << 5;
    QTest::newRow("missing state")
        << track("Version=\"2\"", vc(QStringLiteral(
               "Action=\"SetButtonState\" %1 Role=\"ToggleButton\"").arg(control))) << false << 5;
    QTest::newRow("malformed position")
        << track("Version=\"2\"", vc(QStringLiteral(
               "Action=\"SetSliderPosition\" %1 Role=\"LevelSlider\" Value=\"half\"").arg(control))) << false << 5;
    QTest::newRow("missing position")
        << track("Version=\"2\"", vc(QStringLiteral(
               "Action=\"SetSliderPosition\" %1 Role=\"LevelSlider\"").arg(control))) << false << 5;
    QTest::newRow("position above one")
        << track("Version=\"2\"", vc(QStringLiteral(
               "Action=\"SetSliderPosition\" %1 Role=\"LevelSlider\" Value=\"1.01\"").arg(control))) << false << 5;
    QTest::newRow("control state with function")
        << track("Version=\"2\"", vc(QStringLiteral(
               "Action=\"SetButtonState\" Function=\"12\" %1 Role=\"ToggleButton\" State=\"On\"").arg(control)))
        << false << 5;
    QTest::newRow("button carrying a position")
        << track("Version=\"2\"", vc(QStringLiteral(
               "Action=\"SetButtonState\" %1 Role=\"ToggleButton\" State=\"On\" Value=\"0.5\"").arg(control)))
        << false << 5;
    QTest::newRow("slider carrying a state")
        << track("Version=\"2\"", vc(QStringLiteral(
               "Action=\"SetSliderPosition\" %1 Role=\"LevelSlider\" Value=\"0.5\" State=\"On\"").arg(control)))
        << false << 5;
    QTest::newRow("function command with control")
        << track("Version=\"2\"", QStringLiteral(
               "<Command ID=\"1\" Time=\"1200\" Action=\"Start\" Function=\"12\" %1/>").arg(control))
        << false << 5;
    QTest::newRow("function command with state")
        << track("Version=\"2\"", QStringLiteral(
               "<Command ID=\"1\" Time=\"1200\" Action=\"Start\" Function=\"12\" State=\"On\"/>"))
        << false << 5;
    QTest::newRow("non numeric version") << track("Version=\"one\"", start) << false << 5;
    QTest::newRow("extent overflow") << track("Version=\"1\" Extent=\"4294967296\"", start) << false << 5;
    QTest::newRow("extent sentinel") << track("Version=\"1\" Extent=\"4294967295\"", start) << false << 5;

    QTest::newRow("unknown action")
        << track("Version=\"1\"", QStringLiteral(
               "<Command ID=\"1\" Time=\"1200\" Action=\"Flash\" Function=\"12\"/>")) << false << 5;
    QTest::newRow("missing action")
        << track("Version=\"1\"", QStringLiteral(
               "<Command ID=\"1\" Time=\"1200\" Function=\"12\"/>")) << false << 5;
    QTest::newRow("duplicate id")
        << track("Version=\"1\"", start + start) << false << 5;
    QTest::newRow("time overflow")
        << track("Version=\"1\"", QStringLiteral(
               "<Command ID=\"1\" Time=\"4294967296\" Action=\"Start\" Function=\"12\"/>")) << false << 5;
    QTest::newRow("negative time")
        << track("Version=\"1\"", QStringLiteral(
               "<Command ID=\"1\" Time=\"-1\" Action=\"Start\" Function=\"12\"/>")) << false << 5;
    QTest::newRow("time sentinel")
        << track("Version=\"1\"", QStringLiteral(
               "<Command ID=\"1\" Time=\"4294967295\" Action=\"Start\" Function=\"12\"/>")) << false << 5;
    QTest::newRow("missing target")
        << track("Version=\"1\"", QStringLiteral(
               "<Command ID=\"1\" Time=\"1200\" Action=\"Start\"/>")) << false << 5;
    QTest::newRow("target sentinel")
        << track("Version=\"1\"", QStringLiteral(
               "<Command ID=\"1\" Time=\"1200\" Action=\"Start\" Function=\"4294967295\"/>")) << false << 5;
    QTest::newRow("id sentinel")
        << track("Version=\"1\"", QStringLiteral(
               "<Command ID=\"4294967295\" Time=\"1200\" Action=\"Start\" Function=\"12\"/>")) << false << 5;
    QTest::newRow("intensity not a number")
        << track("Version=\"1\"", QStringLiteral(
               "<Command ID=\"1\" Time=\"1200\" Action=\"SetIntensity\" Function=\"12\" Value=\"nan\"/>"))
        << false << 5;
    QTest::newRow("intensity infinite")
        << track("Version=\"1\"", QStringLiteral(
               "<Command ID=\"1\" Time=\"1200\" Action=\"SetIntensity\" Function=\"12\" Value=\"inf\"/>"))
        << false << 5;
    QTest::newRow("intensity above one")
        << track("Version=\"1\"", QStringLiteral(
               "<Command ID=\"1\" Time=\"1200\" Action=\"SetIntensity\" Function=\"12\" Value=\"1.4\"/>"))
        << false << 5;
    QTest::newRow("intensity below zero")
        << track("Version=\"1\"", QStringLiteral(
               "<Command ID=\"1\" Time=\"1200\" Action=\"SetIntensity\" Function=\"12\" Value=\"-0.2\"/>"))
        << false << 5;
    QTest::newRow("intensity not numeric")
        << track("Version=\"1\"", QStringLiteral(
               "<Command ID=\"1\" Time=\"1200\" Action=\"SetIntensity\" Function=\"12\" Value=\"loud\"/>"))
        << false << 5;
    QTest::newRow("intensity missing")
        << track("Version=\"1\"", QStringLiteral(
               "<Command ID=\"1\" Time=\"1200\" Action=\"SetIntensity\" Function=\"12\"/>")) << false << 5;
    QTest::newRow("trigger carrying a value")
        << track("Version=\"1\"", QStringLiteral(
               "<Command ID=\"1\" Time=\"1200\" Action=\"Stop\" Function=\"12\" Value=\"0.5\"/>")) << false << 5;
    QTest::newRow("unknown element")
        << track("Version=\"1\"", start + QStringLiteral("<Clip ID=\"2\"/>")) << false << 5;
    QTest::newRow("wrong root")
        << QStringLiteral("<Track Version=\"1\"/>") << false << 5;
    QTest::newRow("equal-time order on some commands only")
        << track("Version=\"1\"", QStringLiteral(
               "<Command ID=\"1\" Time=\"1200\" Action=\"Start\" Function=\"12\" Order=\"4\"/>"
               "<Command ID=\"2\" Time=\"1200\" Action=\"Stop\" Function=\"12\"/>")) << false << 5;
    QTest::newRow("equal-time order used twice")
        << track("Version=\"1\"", QStringLiteral(
               "<Command ID=\"1\" Time=\"1200\" Action=\"Start\" Function=\"12\" Order=\"4\"/>"
               "<Command ID=\"2\" Time=\"1300\" Action=\"Stop\" Function=\"12\" Order=\"4\"/>")) << false << 5;
    QTest::newRow("equal-time order not a number")
        << track("Version=\"1\"", QStringLiteral(
               "<Command ID=\"1\" Time=\"1200\" Action=\"Start\" Function=\"12\" Order=\"first\"/>")) << false << 5;
}

namespace
{
/** Ties at 1.7 s whose ids run against their order (A = 9 before B = 2),
 *  inserted out of time order */
ShowCommandTrack tieTrack()
{
    ShowCommandTrack track;
    track.insert(ShowCommand::setIntensity(9, 1700, 12, 0.25));
    track.insert(ShowCommand::setIntensity(2, 1700, 12, 0.75));
    track.insert(ShowCommand::stop(5, 2500, 12));
    track.insert(ShowCommand::start(0, 1200, 12));
    return track;
}
} // namespace

void ShowCommandTrack_Test::equalTimeOrderSurvivesMovingAwayAndBack_data()
{
    QTest::addColumn<quint32>("moved");
    QTest::addColumn<quint32>("away");
    QTest::addColumn<bool>("reload");

    QTest::newRow("A to an empty later time") << 9u << 5000u << false;
    QTest::newRow("B to an empty later time") << 2u << 5000u << false;
    QTest::newRow("A onto a later tie") << 9u << 2500u << false;
    QTest::newRow("B onto a later tie") << 2u << 2500u << false;
    QTest::newRow("B earlier, saved and loaded in between") << 2u << 1000u << true;
    QTest::newRow("A later, saved and loaded in between") << 9u << 5000u << true;
}

void ShowCommandTrack_Test::equalTimeOrderSurvivesMovingAwayAndBack()
{
    QFETCH(quint32, moved);
    QFETCH(quint32, away);
    QFETCH(bool, reload);

    ShowCommandTrack track = tieTrack();
    QCOMPARE(idsOf(track), QVector<quint32>({0, 9, 2, 5}));

    QVERIFY(track.retime(moved, away));
    if (away == 2500)
        QCOMPARE(idsOf(track), moved == 9 ? QVector<quint32>({0, 2, 9, 5}) : QVector<quint32>({0, 9, 2, 5}));
    if (reload)
    {
        const QString xml = toXml(track);
        QVERIFY(xml.contains(QLatin1String("Order=")));
        ShowCommandTrack loaded;
        QVERIFY(fromXml(xml, &loaded));
        QCOMPARE(idsOf(loaded), idsOf(track));
        QCOMPARE(toXml(loaded), xml);
        track = loaded;
    }

    // back where it was: in its old place among its ties, not by id or last
    QVERIFY(track.retime(moved, 1700));
    QCOMPARE(idsOf(track), QVector<quint32>({0, 9, 2, 5}));

}

void ShowCommandTrack_Test::equalTimeOrderSavesOnlyWhatFileOrderCannotTell()
{
    // file order says it all: the document stays exactly the legacy one
    ShowCommandTrack plain;
    QVERIFY(plain.insert(ShowCommand::start(0, 1200, 12)));
    QVERIFY(plain.insert(ShowCommand::setIntensity(9, 1700, 12, 0.25)));
    QVERIFY(plain.insert(ShowCommand::setIntensity(2, 1700, 12, 0.75)));
    QVERIFY(plain.retime(9, 1700));
    QVERIFY(!toXml(plain).contains(QLatin1String("Order=")));
    // it cannot once a later event holds an earlier order
    QVERIFY(plain.retime(0, 2000));
    QVERIFY(toXml(plain).contains(QLatin1String("Order=")));

    // an older document has no order: its file order is the equal-time order
    const QString legacy = QStringLiteral(
        "<CommandTrack Version=\"1\" Extent=\"2500\">"
        "<Command ID=\"9\" Time=\"1700\" Action=\"SetIntensity\" Function=\"12\" Value=\"0.25\"/>"
        "<Command ID=\"2\" Time=\"1700\" Action=\"SetIntensity\" Function=\"12\" Value=\"0.75\"/>"
        "<Command ID=\"5\" Time=\"2500\" Action=\"Stop\" Function=\"12\"/>"
        "</CommandTrack>");
    ShowCommandTrack loaded;
    QVERIFY(fromXml(legacy, &loaded));
    QCOMPARE(idsOf(loaded), QVector<quint32>({9, 2, 5}));
    QCOMPARE(toXml(loaded), legacy);
    QVERIFY(loaded.retime(9, 3000));
    QVERIFY(loaded.retime(9, 1700));
    QCOMPARE(idsOf(loaded), QVector<quint32>({9, 2, 5}));
    // and the next new event still goes last among its ties
    QVERIFY(loaded.insert(ShowCommand::start(1, 1700, 12)));
    QCOMPARE(idsOf(loaded), QVector<quint32>({9, 2, 1, 5}));
}

void ShowCommandTrack_Test::loadIsTransactional()
{
    QFETCH(QString, xml);
    QFETCH(bool, loadable);
    QFETCH(int, count);

    ShowCommandTrack track = fixtureTrack();
    const QString before = toXml(track);
    QString error = QStringLiteral("untouched");

    QCOMPARE(fromXml(xml, &track, &error), loadable);
    QCOMPARE(error.isEmpty(), loadable);
    QCOMPARE(track.count(), count);

    if (!loadable)
        QCOMPARE(toXml(track), before);
}

void ShowCommandTrack_Test::loadLeavesReaderUsableAfterFailure()
{
    // the host keeps parsing its own Show element after a rejected command track
    QXmlStreamReader reader(QStringLiteral(
        "<Show><CommandTrack Version=\"1\">"
        "<Command ID=\"1\" Time=\"1200\" Action=\"Flash\" Function=\"12\"/>"
        "</CommandTrack><Track ID=\"4\"/></Show>"));

    QVERIFY(reader.readNextStartElement());
    QCOMPARE(reader.name().toString(), QStringLiteral("Show"));
    QVERIFY(reader.readNextStartElement());

    ShowCommandTrack track;
    QString error;
    QVERIFY(!track.loadXML(reader, &error));
    QVERIFY(!error.isEmpty());

    QVERIFY(reader.readNextStartElement());
    QCOMPARE(reader.name().toString(), QStringLiteral("Track"));
    QCOMPARE(reader.attributes().value(QStringLiteral("ID")).toString(), QStringLiteral("4"));
    QVERIFY(!reader.hasError());
}

void ShowCommandTrack_Test::loadRecoversFromNestedTrackNames()
{
    // a hand edited file may nest the very element name the recovery scans for
    const QString nested = QStringLiteral("<CommandTrack Version=\"1\">"
                                          "<Command ID=\"2\" Time=\"1300\" Action=\"Stop\" Function=\"12\"/>"
                                          "</CommandTrack>");
    const QStringList shows = {
        QStringLiteral("<Show><CommandTrack Version=\"1\">"
                       "<Command ID=\"1\" Time=\"1200\" Action=\"Start\" Function=\"12\"/>")
            + nested + QStringLiteral("</CommandTrack><Track ID=\"4\"/></Show>"),
        QStringLiteral("<Show><CommandTrack Version=\"2\">") + nested
            + QStringLiteral("</CommandTrack><Track ID=\"4\"/></Show>")};

    for (const QString &show : shows)
    {
        QXmlStreamReader reader(show);
        QVERIFY(reader.readNextStartElement());
        QVERIFY(reader.readNextStartElement());

        ShowCommandTrack track = fixtureTrack();
        const QString before = toXml(track);
        QString error;

        QVERIFY(!track.loadXML(reader, &error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(toXml(track), before);

        // recovery must stop on the outer end element, so the host reads its sibling
        QVERIFY(reader.readNextStartElement());
        QCOMPARE(reader.name().toString(), QStringLiteral("Track"));
        QCOMPARE(reader.attributes().value(QStringLiteral("ID")).toString(), QStringLiteral("4"));
        QVERIFY(!reader.hasError());
    }
}

void ShowCommandTrack_Test::eventIdsRunOutExplicitly(){
    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::start(ShowCommand::MaxTime - 1, 500, 12)));
    QCOMPARE(track.nextEventId(), ShowCommand::MaxTime);

    QVERIFY(track.insert(ShowCommand::start(ShowCommand::MaxTime, 900, 12)));
    QCOMPARE(track.nextEventId(), ShowCommand::InvalidId);

    // the recorder explains the exhaustion instead of dropping the input silently
    ShowCommandState state = ShowCommandFsm::setResolvedShow(
        ShowCommandFsm::setRecording(ShowCommandState(), true), 55);
    state = ShowCommandFsm::setPlaying(state, true);

    ShowCommandInput input;
    input.origin = ShowCommandOrigin::Pointer;
    input.action = ShowCommandAction::Start;
    input.functionId = 12;

    const ShowCommandTransition step = ShowCommandFsm::userInput(track, state, input);
    QVERIFY(step.authored.isEmpty());
    QVERIFY(!step.error.isEmpty());
    QVERIFY(step.state == state);
}

void ShowCommandTrack_Test::advanceAppliesDueCommandsOnce()
{
    const ShowCommandTrack track = playbackTrack();

    ShowCommandTransition step = ShowCommandFsm::advance(track, playingState(), 0);
    QCOMPARE(idsOf(step.effects), QStringLiteral("7,8"));
    QVERIFY(step.authored.isEmpty());
    QCOMPARE(step.state.position, 0u);
    QCOMPARE(step.state.consumedThrough, 1u);

    step = ShowCommandFsm::advance(track, step.state, 1500);
    QCOMPARE(idsOf(step.effects), QStringLiteral("20,9"));
    QCOMPARE(step.effects.at(1).intensity, 0.375);

    // the same position again is due for nothing
    step = ShowCommandFsm::advance(track, step.state, 1500);
    QVERIFY(step.effects.isEmpty());
    QCOMPARE(step.state.consumedThrough, 1501u);

    // equal timestamps keep their authored order
    step = ShowCommandFsm::advance(track, step.state, 4200);
    QCOMPARE(idsOf(step.effects), QStringLiteral("21,10,11"));
    QVERIFY(step.effects.at(1).action == ShowCommandAction::Stop);

    step = ShowCommandFsm::advance(track, step.state, 19000);
    QVERIFY(step.effects.isEmpty());
    QCOMPARE(step.state.position, 19000u);
}

void ShowCommandTrack_Test::advanceIgnoresPausedAndBackwardTransport()
{
    const ShowCommandTrack track = playbackTrack();

    ShowCommandTransition step = ShowCommandFsm::advance(track, playingState(), 1500);
    QCOMPARE(idsOf(step.effects), QStringLiteral("7,8,20,9"));

    const ShowCommandState paused = ShowCommandFsm::setPlaying(step.state, false);
    const ShowCommandTransition frozen = ShowCommandFsm::advance(track, paused, 4200);
    QVERIFY(frozen.effects.isEmpty());
    QCOMPARE(frozen.state.position, 1500u);
    QCOMPARE(frozen.state.consumedThrough, 1501u);

    // a jump backwards is a seek, never an advance
    const ShowCommandTransition backwards = ShowCommandFsm::advance(track, step.state, 500);
    QVERIFY(backwards.effects.isEmpty());
    QCOMPARE(backwards.state.position, 1500u);
    QCOMPARE(backwards.state.consumedThrough, 1501u);
}

/** C1: a seek only repositions. The old value-restore rows keep their
 *  destinations and assert what the next advance plays from there. */
static ShowCommandTransition repositioned(const ShowCommandTrack &track, const ShowCommandState &state,
                                          quint32 destination)
{
    Q_UNUSED(track)
    ShowCommandTransition step;
    step.state = ShowCommandFsm::seek(state, destination);
    return step;
}

void ShowCommandTrack_Test::seekRepositionsOnly_data()
{
    QTest::addColumn<int>("from");        // -1: Play of a fresh traversal, else advanced to here first
    QTest::addColumn<quint32>("destination");
    QTest::addColumn<quint32>("next");    // the advance after the seek
    QTest::addColumn<QString>("played");

    QTest::newRow("backward to the start of show") << 4200 << 0u << 0u << QStringLiteral("7,8");
    QTest::newRow("backward before any value, tie at T") << 4200 << 1000u << 1000u << QStringLiteral("20");
    QTest::newRow("backward on a value boundary") << 4200 << 1500u << 1500u << QStringLiteral("9");
    QTest::newRow("backward just past a value restores nothing") << 4200 << 1501u << 3000u << QStringLiteral("21");
    QTest::newRow("backward onto the stop it consumed") << 4200 << 4200u << 4200u << QStringLiteral("10,11");
    QTest::newRow("Play from T on a value") << -1 << 3000u << 3000u << QStringLiteral("21");
    QTest::newRow("Play from T past a stopped target") << -1 << 4201u << 18000u << QString();
    QTest::newRow("Play from T beyond the last command") << -1 << 18000u << 20000u << QString();
    QTest::newRow("Play from 0") << -1 << 0u << 1000u << QStringLiteral("7,8,20");
}

void ShowCommandTrack_Test::seekRepositionsOnly()
{
    QFETCH(int, from);
    QFETCH(quint32, destination);
    QFETCH(quint32, next);
    QFETCH(QString, played);

    // a legacy-only track: Start, Stop and SetIntensity
    const ShowCommandTrack track = playbackTrack();
    ShowCommandState state = playingState();
    state.recording = true;
    state.boundShowId = 55;
    state.resolvedShowId = 55;
    if (from >= 0)
        state = ShowCommandFsm::advance(track, state, quint32(from)).state;
    // a live mark of the previous pass at 3000
    state.consumedLiveEventIds.insert(21, { 3000 });

    const ShowCommandTransition seek = repositioned(track, state, destination);
    QCOMPARE(idsOf(seek.effects), QString());
    const ShowCommandState sought = seek.state;

    QCOMPARE(sought.position, destination);
    QCOMPARE(sought.consumedThrough, destination);
    QVERIFY(sought.consumedLiveEventIds.isEmpty());
    QVERIFY(sought.recording);
    QCOMPARE(sought.boundShowId, 55u);
    QVERIFY(sought.playing);

    const ShowCommandTransition step = ShowCommandFsm::advance(track, sought, next);
    QCOMPARE(idsOf(step.effects), played);
    // once per traversal: the same position again plays nothing
    QVERIFY(ShowCommandFsm::advance(track, step.state, next).effects.isEmpty());
}

void ShowCommandTrack_Test::forwardJumpPlaysCrossedInterval_data()
{
    QTest::addColumn<quint32>("from");
    QTest::addColumn<quint32>("to");
    QTest::addColumn<bool>("marked"); // a live mark on 21 at 3000, captured before the jump
    QTest::addColumn<QString>("played");

    QTest::newRow("whole interval in authored order") << 1000u << 4200u << false << QStringLiteral("9,21,10,11");
    QTest::newRow("live mark inside the interval does not echo") << 1000u << 4200u << true << QStringLiteral("9,10,11");
    QTest::newRow("tie at T included, nothing after") << 1000u << 3000u << false << QStringLiteral("9,21");
    QTest::newRow("nothing before consumedThrough") << 1500u << 4199u << false << QStringLiteral("21");
}

void ShowCommandTrack_Test::forwardJumpPlaysCrossedInterval()
{
    QFETCH(quint32, from);
    QFETCH(quint32, to);
    QFETCH(bool, marked);
    QFETCH(QString, played);

    // a forward jump is no seek: the next advance covers [consumedThrough at P, T]
    const ShowCommandTrack track = playbackTrack();
    ShowCommandState state = ShowCommandFsm::advance(track, playingState(), from).state;
    if (marked)
        state.consumedLiveEventIds.insert(21, { 3000 });

    const ShowCommandTransition step = ShowCommandFsm::advance(track, state, to);
    QCOMPARE(idsOf(step.effects), played);
    QCOMPARE(step.state.consumedThrough, to + 1);
}

void ShowCommandTrack_Test::seekKeepsIntentAndLoopReplaysCommands()
{
    const ShowCommandTrack track = playbackTrack();

    ShowCommandTransition step = ShowCommandFsm::advance(track, playingState(), 4200);
    QCOMPARE(idsOf(step.effects), QStringLiteral("7,8,20,9,21,10,11"));

    // looping back re-arms the traversal from the destination on
    const ShowCommandTransition looped = repositioned(track, step.state, 0);
    QCOMPARE(idsOf(looped.effects), QString());
    QCOMPARE(looped.state.consumedThrough, 0u);

    step = ShowCommandFsm::advance(track, looped.state, 1000);
    QCOMPARE(idsOf(step.effects), QStringLiteral("7,8,20"));

    // a seek while paused keeps the transport paused and restores nothing
    const ShowCommandState paused = ShowCommandFsm::setPlaying(step.state, false);
    const ShowCommandTransition seekWhilePaused = repositioned(track, paused, 3500);
    QCOMPARE(idsOf(seekWhilePaused.effects), QString());
    const ShowCommandState jumped = seekWhilePaused.state;
    QVERIFY(!jumped.playing);
    QCOMPARE(jumped.position, 3500u);
    QCOMPARE(jumped.consumedThrough, 3500u);
    // resumed: what follows 3500, not the values before it
    QCOMPARE(idsOf(ShowCommandFsm::advance(track, ShowCommandFsm::setPlaying(jumped, true), 4200).effects),
             QStringLiteral("10,11"));
}

void ShowCommandTrack_Test::extentNeverSynthesizesEffects()
{
    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::start(1, 0, 12)));
    QVERIFY(track.insert(ShowCommand::start(2, 30000, 30)));
    // an extent equal to the last command would end the show on that Start
    track.setExtent(track.lastCommandTime());

    const ShowCommandTransition step = ShowCommandFsm::advance(track, playingState(), 30000);
    QCOMPARE(idsOf(step.effects), QStringLiteral("1,2"));
    for (const ShowCommand &effect : step.effects)
        QVERIFY(effect.action == ShowCommandAction::Start);

    track.setExtent(45000);
    const ShowCommandTransition longer = ShowCommandFsm::advance(track, playingState(), 44000);
    QCOMPARE(idsOf(longer.effects), QStringLiteral("1,2"));
}

void ShowCommandTrack_Test::armingBindsTheFirstResolvedShow()
{
    ShowCommandState state;
    QVERIFY(state.phase() == ShowRecordPhase::Off);

    // armed before any show resolves: the recorder waits
    state = ShowCommandFsm::setRecording(state, true);
    QVERIFY(state.phase() == ShowRecordPhase::Armed);
    QCOMPARE(state.boundShowId, ShowCommand::InvalidId);

    state = ShowCommandFsm::setResolvedShow(state, 55);
    QVERIFY(state.phase() == ShowRecordPhase::Bound);
    QCOMPARE(state.boundShowId, 55u);

    // another song/editor selection suspends instead of retargeting
    state = ShowCommandFsm::setResolvedShow(state, 77);
    QVERIFY(state.phase() == ShowRecordPhase::Suspended);
    QCOMPARE(state.boundShowId, 55u);

    state = ShowCommandFsm::setResolvedShow(state, 55);
    QVERIFY(state.phase() == ShowRecordPhase::Bound);

    state = ShowCommandFsm::setRecording(state, false);
    QVERIFY(state.phase() == ShowRecordPhase::Off);
    QCOMPARE(state.boundShowId, ShowCommand::InvalidId);
    QCOMPARE(state.resolvedShowId, 55u);

    // arming with a show already resolved binds at once
    state = ShowCommandFsm::setRecording(state, true);
    QVERIFY(state.phase() == ShowRecordPhase::Bound);
    QCOMPARE(state.boundShowId, 55u);
}

void ShowCommandTrack_Test::bindingSurvivesPauseSeekAndLoop()
{
    const ShowCommandTrack track = playbackTrack();
    ShowCommandState state = ShowCommandFsm::setResolvedShow(
        ShowCommandFsm::setRecording(ShowCommandState(), true), 55);
    state = ShowCommandFsm::setPlaying(state, true);

    state = ShowCommandFsm::advance(track, state, 2000).state;
    state = ShowCommandFsm::setPlaying(state, false);
    QVERIFY(state.phase() == ShowRecordPhase::Bound);
    QCOMPARE(state.boundShowId, 55u);

    state = ShowCommandFsm::seek(state, 0);
    state = ShowCommandFsm::setPlaying(state, true);
    QVERIFY(state.phase() == ShowRecordPhase::Bound);
    QVERIFY(state.recording);
    QCOMPARE(state.boundShowId, 55u);
    QCOMPARE(state.position, 0u);
}

void ShowCommandTrack_Test::recordsOnlyAcceptedUserInput_data()
{
    QTest::addColumn<ShowCommandOrigin>("origin");
    QTest::addColumn<ShowCommandAction>("action");
    QTest::addColumn<qreal>("intensity");
    QTest::addColumn<quint32>("functionId");
    QTest::addColumn<bool>("recorded");
    QTest::addColumn<bool>("explained");

    QTest::newRow("pointer start")
        << ShowCommandOrigin::Pointer << ShowCommandAction::Start << 0.0 << 12u << true << false;
    QTest::newRow("pointer stop")
        << ShowCommandOrigin::Pointer << ShowCommandAction::Stop << 0.0 << 30u << true << false;
    QTest::newRow("pointer intensity")
        << ShowCommandOrigin::Pointer << ShowCommandAction::SetIntensity << 0.64 << 12u << true << false;
    QTest::newRow("midi intensity")
        << ShowCommandOrigin::Midi << ShowCommandAction::SetIntensity << 0.125 << 30u << true << false;
    QTest::newRow("midi start")
        << ShowCommandOrigin::Midi << ShowCommandAction::Start << 0.0 << 30u << true << false;
    QTest::newRow("keyboard start")
        << ShowCommandOrigin::Keyboard << ShowCommandAction::Start << 0.0 << 12u << true << false;
    QTest::newRow("keyboard stop")
        << ShowCommandOrigin::Keyboard << ShowCommandAction::Stop << 0.0 << 30u << true << false;
    QTest::newRow("osc intensity")
        << ShowCommandOrigin::Osc << ShowCommandAction::SetIntensity << 0.375 << 12u << true << false;

    // input the recorder is not meant to author is ignored, not reported
    QTest::newRow("audio mapping")
        << ShowCommandOrigin::Audio << ShowCommandAction::SetIntensity << 0.64 << 12u << false << false;
    QTest::newRow("generic setter")
        << ShowCommandOrigin::Programmatic << ShowCommandAction::Start << 0.0 << 12u << false << false;
    QTest::newRow("playback feedback")
        << ShowCommandOrigin::Replay << ShowCommandAction::Stop << 0.0 << 12u << false << false;

    QTest::newRow("intensity out of range")
        << ShowCommandOrigin::Pointer << ShowCommandAction::SetIntensity << 1.4 << 12u << false << true;
    QTest::newRow("intensity not a number")
        << ShowCommandOrigin::Pointer << ShowCommandAction::SetIntensity << qQNaN() << 12u << false << true;
    QTest::newRow("unresolved target")
        << ShowCommandOrigin::Pointer << ShowCommandAction::Start << 0.0
        << ShowCommand::InvalidId << false << true;
    QTest::newRow("start carrying a value")
        << ShowCommandOrigin::Pointer << ShowCommandAction::Start << 0.5 << 12u << false << true;
    QTest::newRow("action outside the enum")
        << ShowCommandOrigin::Pointer << static_cast<ShowCommandAction>(9) << 0.0 << 12u << false << true;
}

void ShowCommandTrack_Test::recordsOnlyAcceptedUserInput()
{
    QFETCH(ShowCommandOrigin, origin);
    QFETCH(ShowCommandAction, action);
    QFETCH(qreal, intensity);
    QFETCH(quint32, functionId);
    QFETCH(bool, recorded);
    QFETCH(bool, explained);

    ShowCommandTrack track = playbackTrack();
    ShowCommandState state = ShowCommandFsm::setResolvedShow(
        ShowCommandFsm::setRecording(ShowCommandState(), true), 55);
    state = ShowCommandFsm::setPlaying(state, true);
    state = ShowCommandFsm::advance(track, state, 2600).state;

    ShowCommandInput input;
    input.origin = origin;
    input.action = action;
    input.functionId = functionId;
    input.intensity = intensity;

    const ShowCommandTransition step = ShowCommandFsm::userInput(track, state, input);

    QVERIFY(step.effects.isEmpty());
    QCOMPARE(step.authored.count(), recorded ? 1 : 0);
    QCOMPARE(!step.error.isEmpty(), explained);

    if (!recorded)
    {
        QVERIFY(step.state == state);
        return;
    }

    const ShowCommand &authored = step.authored.first();
    QCOMPARE(authored.id, 22u);
    QCOMPARE(authored.time, 2600u);
    QCOMPARE(authored.functionId, functionId);
    QVERIFY(authored.action == action);
    QCOMPARE(authored.intensity, action == ShowCommandAction::SetIntensity ? intensity : 0.0);
    QVERIFY(ShowCommand::validate(authored).isEmpty());
    QVERIFY(track.insert(authored));
}

void ShowCommandTrack_Test::recordingIsGatedByPhaseAndTransport_data()
{
    QTest::addColumn<bool>("recording");
    QTest::addColumn<quint32>("boundShowId");
    QTest::addColumn<quint32>("resolvedShowId");
    QTest::addColumn<bool>("playing");
    QTest::addColumn<bool>("authors");

    QTest::newRow("bound and playing") << true << 55u << 55u << true << true;
    QTest::newRow("bound and paused") << true << 55u << 55u << false << true;
    QTest::newRow("record off") << false << ShowCommand::InvalidId << 55u << true << false;
    QTest::newRow("armed without a show")
        << true << ShowCommand::InvalidId << ShowCommand::InvalidId << true << false;
    QTest::newRow("suspended by another show") << true << 55u << 77u << true << false;
    QTest::newRow("suspended while paused") << true << 55u << 77u << false << false;
}

void ShowCommandTrack_Test::recordingIsGatedByPhaseAndTransport()
{
    QFETCH(bool, recording);
    QFETCH(quint32, boundShowId);
    QFETCH(quint32, resolvedShowId);
    QFETCH(bool, playing);
    QFETCH(bool, authors);

    const ShowCommandTrack track = playbackTrack();
    ShowCommandState state;
    state.recording = recording;
    state.boundShowId = boundShowId;
    state.resolvedShowId = resolvedShowId;
    state.playing = playing;
    state.position = 2600;
    state.consumedThrough = 2601;

    ShowCommandInput input;
    input.origin = ShowCommandOrigin::Pointer;
    input.action = ShowCommandAction::Start;
    input.functionId = 12;

    const ShowCommandTransition step = ShowCommandFsm::userInput(track, state, input);
    QCOMPARE(step.authored.count(), authors ? 1 : 0);
    QVERIFY(step.effects.isEmpty());

    if (!authors)
    {
        QVERIFY(step.state == state);
        return;
    }

    // a frozen transport records at the position it froze on
    QCOMPARE(step.authored.first().time, 2600u);
    QCOMPARE(step.state.playing, playing);
}

void ShowCommandTrack_Test::liveCommandDoesNotEchoUntilTheNextTraversal()
{
    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::start(0, 2000, 12)));

    ShowCommandState state = ShowCommandFsm::setResolvedShow(
        ShowCommandFsm::setRecording(ShowCommandState(), true), 55);
    state = ShowCommandFsm::setPlaying(state, true);

    // an authored event at the very first position must not echo either
    ShowCommandInput input;
    input.origin = ShowCommandOrigin::Pointer;
    input.action = ShowCommandAction::SetIntensity;
    input.functionId = 30;
    input.intensity = 0.42;

    ShowCommandTransition step = ShowCommandFsm::userInput(track, state, input);
    QCOMPARE(step.authored.count(), 1);
    QCOMPARE(step.authored.first().time, 0u);
    QVERIFY(track.insert(step.authored.first()));
    state = step.state;
    QCOMPARE(state.consumedThrough, 0u);
    QCOMPARE(state.consumedLiveEventIds, ShowLiveMarks({ { 1, { 0 } } }));

    step = ShowCommandFsm::advance(track, state, 2000);
    QCOMPARE(idsOf(step.effects), QStringLiteral("0"));
    state = step.state;

    // live input at a position the cursor already passed, sharing a timestamp
    input.action = ShowCommandAction::Stop;
    input.functionId = 12;
    input.intensity = 0.0;
    step = ShowCommandFsm::userInput(track, state, input);
    QCOMPARE(step.authored.count(), 1);
    QCOMPARE(step.authored.first().time, 2000u);
    QCOMPARE(step.authored.first().id, 2u);
    QVERIFY(track.insert(step.authored.first()));
    state = step.state;

    step = ShowCommandFsm::advance(track, state, 6000);
    QVERIFY(step.effects.isEmpty());
    state = step.state;

    // the next traversal plays everything back, in authored order
    state = ShowCommandFsm::seek(state, 0);
    step = ShowCommandFsm::advance(track, state, 6000);
    QCOMPARE(idsOf(step.effects), QStringLiteral("1,0,2"));
    QVERIFY(state.recording);
    QCOMPARE(state.boundShowId, 55u);
}

void ShowCommandTrack_Test::liveInputMarksOnlyItsOwnEvent()
{
    // the cursor lags the authoritative position: commands before and at D are still due
    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::start(0, 3500, 30)));
    QVERIFY(track.insert(ShowCommand::start(1, 4200, 12)));

    ShowCommandState state = ShowCommandFsm::setResolvedShow(
        ShowCommandFsm::setRecording(ShowCommandState(), true), 55);
    state = ShowCommandFsm::setPlaying(state, true);
    state = ShowCommandFsm::advance(track, state, 3000).state;
    state.position = 4200;

    ShowCommandInput input;
    input.origin = ShowCommandOrigin::Pointer;
    input.action = ShowCommandAction::SetIntensity;
    input.functionId = 12;
    input.intensity = 0.42;

    ShowCommandTransition step = ShowCommandFsm::userInput(track, state, input);
    QCOMPARE(step.authored.count(), 1);
    QCOMPARE(step.authored.first().id, 2u);
    QCOMPARE(step.authored.first().time, 4200u);
    // marking the live event must leave the playback cursor where it was
    QCOMPARE(step.state.consumedThrough, 3001u);
    QCOMPARE(step.state.consumedLiveEventIds, ShowLiveMarks({ { 2, { 4200 } } }));
    QVERIFY(track.insert(step.authored.first()));
    state = step.state;

    // the lagging and same-time events still execute, the live one does not echo
    step = ShowCommandFsm::advance(track, state, 4200);
    QCOMPARE(idsOf(step.effects), QStringLiteral("0,1"));
    QCOMPARE(step.state.consumedThrough, 4201u);
    // its time is consumed now, so the mark is retired
    QVERIFY(step.state.consumedLiveEventIds.isEmpty());
    state = step.state;

    // a later traversal replays it like any other authored command
    state = ShowCommandFsm::seek(state, 0);
    QCOMPARE(idsOf(ShowCommandFsm::advance(track, state, 4200).effects), QStringLiteral("0,1,2"));

    // seeking away re-arms the whole traversal, marks included
    input.functionId = 30;
    const ShowCommandState marked = ShowCommandFsm::userInput(track, state, input).state;
    QVERIFY(!marked.consumedLiveEventIds.isEmpty());
    QVERIFY(ShowCommandFsm::seek(marked, 0).consumedLiveEventIds.isEmpty());
}

void ShowCommandTrack_Test::liveOccurrence_data()
{
    // live input X (id 1) captured at 800 while the cursor lags at 500;
    // U (id 0) at 600 is unrelated. One entry per advance in "effects".
    QTest::addColumn<QStringList>("script");
    QTest::addColumn<QStringList>("effects");

    QTest::newRow("moved ahead before expiry plays at its new time")
        << QStringList({ "time:1500", "advance:900", "advance:2000" })
        << QStringList({ "0", "1" });
    QTest::newRow("moved ahead after expiry and a re-import plays")
        << QStringList({ "advance:900", "reimport", "time:1500", "advance:2000" })
        << QStringList({ "0", "1" });
    QTest::newRow("value edit at the capture time is suppressed once")
        << QStringList({ "value", "advance:900", "time:1500", "advance:2000" })
        << QStringList({ "0", "1" });
    QTest::newRow("deleted mark still guards an undone delete, nothing else")
        << QStringList({ "delete", "paste", "advance:700", "restore", "advance:900" })
        << QStringList({ "0", "2" });
    QTest::newRow("undo back to the capture time before expiry is the same occurrence")
        << QStringList({ "time:1500", "time:800", "advance:2000" })
        << QStringList({ "0" });
    QTest::newRow("undo back after expiry and a re-import plays")
        << QStringList({ "time:1500", "advance:900", "reimport", "time:1700", "time:1500", "advance:2000" })
        << QStringList({ "0", "1" });
    QTest::newRow("edit behind the cursor gives nothing this pass")
        << QStringList({ "time:300", "advance:2000" })
        << QStringList({ "0" });
    QTest::newRow("seek clears the marks")
        << QStringList({ "seek:0", "advance:2000" })
        << QStringList({ "0,1" });
}

void ShowCommandTrack_Test::liveOccurrence()
{
    QFETCH(QStringList, script);
    QFETCH(QStringList, effects);

    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::start(0, 600, 30)));

    ShowCommandState runner = ShowCommandFsm::advance(track, playingState(), 500).state;

    // the recorder follows the authoritative clock, which is ahead of the cursor
    ShowCommandState recorder = ShowCommandFsm::setResolvedShow(
        ShowCommandFsm::setRecording(ShowCommandState(), true), 55);
    recorder = ShowCommandFsm::setPlaying(recorder, true);
    recorder.position = 800;

    ShowCommandInput input;
    input.origin = ShowCommandOrigin::Pointer;
    input.action = ShowCommandAction::SetIntensity;
    input.functionId = 12;
    input.intensity = 0.42;

    const ShowCommandTransition captured = ShowCommandFsm::userInput(track, recorder, input);
    QCOMPARE(captured.authored.count(), 1);
    QCOMPARE(captured.authored.first().id, 1u);
    QVERIFY(track.insert(captured.authored.first()));
    const ShowCommand original = track.commands().at(track.indexOfId(1));
    // what the runner takes from the capture publication, on every later revision too
    const auto reimport = [&]() { runner.consumedLiveEventIds.insert(captured.state.consumedLiveEventIds); };
    reimport();

    QStringList seen;
    for (const QString &step : std::as_const(script))
    {
        const QString verb = step.section(QLatin1Char(':'), 0, 0);
        const quint32 ms = step.section(QLatin1Char(':'), 1, 1).toUInt();
        if (verb == QLatin1String("advance"))
        {
            const ShowCommandTransition played = ShowCommandFsm::advance(track, runner, ms);
            seen.append(idsOf(played.effects));
            runner = played.state;
        }
        else if (verb == QLatin1String("seek"))
            runner = ShowCommandFsm::seek(runner, ms);
        else if (verb == QLatin1String("time"))
            QVERIFY(track.retime(1, ms));
        else if (verb == QLatin1String("value"))
            QVERIFY(track.replace(ShowCommand::setIntensity(1, original.time, 12, 0.9)));
        else if (verb == QLatin1String("delete"))
            QVERIFY(track.remove(1));
        else if (verb == QLatin1String("restore"))
            QVERIFY(track.restore(original));
        else if (verb == QLatin1String("paste"))
            QVERIFY(track.insert(ShowCommand::start(2, original.time, 31)));
        else if (verb == QLatin1String("reimport"))
            reimport();
        else
            QFAIL(qPrintable(step));
    }

    QCOMPARE(seen, effects);

    // every mark is gone once the cursor passed its time, nothing echoes later
    const ShowCommandTransition rest = ShowCommandFsm::advance(track, runner, 5000);
    QVERIFY(rest.effects.isEmpty());
    QVERIFY(rest.state.consumedLiveEventIds.isEmpty());
}

void ShowCommandTrack_Test::controlStateInputAuthorsVcRecord()
{
    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::start(4, 100, 12)));

    ShowCommandState state = ShowCommandFsm::setResolvedShow(
        ShowCommandFsm::setRecording(ShowCommandState(), true), 55);
    state = ShowCommandFsm::setPlaying(state, true);
    state = ShowCommandFsm::advance(track, state, 2750).state;

    ShowCommandInput input;
    input.origin = ShowCommandOrigin::Midi;
    input.action = ShowCommandAction::SetSliderPosition;
    input.controlId = kControlB;
    input.role = ShowControlRole::AdjustSlider;
    input.attribute = QStringLiteral("Intensity");
    input.position = 0.625;

    const ShowCommandTransition step = ShowCommandFsm::userInput(track, state, input);
    QVERIFY2(step.error.isEmpty(), qPrintable(step.error));
    QCOMPARE(step.authored.count(), 1);
    QVERIFY(step.authored.first() == ShowCommand::setSliderPosition(
                5, 2750, kControlB, ShowControlRole::AdjustSlider, QStringLiteral("Intensity"), 0.625));
    QCOMPARE(step.state.consumedLiveEventIds, ShowLiveMarks({ { 5, { 2750 } } }));
    QVERIFY(step.effects.isEmpty());

    // a button state offered without a control role is explained, not authored
    ShowCommandInput malformed;
    malformed.origin = ShowCommandOrigin::Pointer;
    malformed.action = ShowCommandAction::SetButtonState;
    malformed.controlId = kControlA;
    malformed.role = ShowControlRole::None;
    malformed.on = true;

    const ShowCommandTransition rejected = ShowCommandFsm::userInput(track, state, malformed);
    QVERIFY(rejected.authored.isEmpty());
    QVERIFY(!rejected.error.isEmpty());
    QVERIFY(rejected.state == state);
}

void ShowCommandTrack_Test::buttonOpReachesDesiredState_data()
{
    using Native = ShowCommandFsm::ShowButtonNative;
    using Op = ShowCommandFsm::ShowButtonOp;

    QTest::addColumn<Native>("native");
    QTest::addColumn<bool>("on");
    QTest::addColumn<Op>("op");

    QTest::newRow("inactive on")    << Native::Inactive   << true  << Op::Start;
    QTest::newRow("inactive off")   << Native::Inactive   << false << Op::None;
    QTest::newRow("active on")      << Native::Active     << true  << Op::None;
    QTest::newRow("active off")     << Native::Active     << false << Op::Stop;
    // monitoring is neither satisfied on nor satisfied off
    QTest::newRow("monitoring on")  << Native::Monitoring << true  << Op::Start;
    QTest::newRow("monitoring off") << Native::Monitoring << false << Op::Stop;
}

void ShowCommandTrack_Test::buttonOpReachesDesiredState()
{
    QFETCH(ShowCommandFsm::ShowButtonNative, native);
    QFETCH(bool, on);
    QFETCH(ShowCommandFsm::ShowButtonOp, op);

    QCOMPARE(ShowCommandFsm::buttonOp(native, on), op);
}

void ShowCommandTrack_Test::sliderTargetMapsIntoCurrentRange_data()
{
    const qreal nan = std::numeric_limits<qreal>::quiet_NaN();
    const qreal inf = std::numeric_limits<qreal>::infinity();
    const qreal dblMax = std::numeric_limits<qreal>::max();
    const qreal intMax = qreal(INT_MAX);
    const qreal intMin = qreal(INT_MIN);

    QTest::addColumn<qreal>("position");
    QTest::addColumn<qreal>("low");
    QTest::addColumn<qreal>("high");
    QTest::addColumn<bool>("ok");
    QTest::addColumn<int>("value");

    QTest::newRow("half of 0..100")       << 0.5   << 0.0  << 100.0 << true  << 50;
    // 25 captured in 0..50 is 0.5, so a widened range keeps the relative place
    QTest::newRow("25 of 0..50 widened")  << 25.0 / 50.0 << 0.0 << 100.0 << true << 50;
    QTest::newRow("third of 0..255")      << 0.333 << 0.0  << 255.0 << true  << 85;
    QTest::newRow("offset range top")     << 1.0   << 20.0 << 80.0  << true  << 80;
    QTest::newRow("int max top")          << 1.0   << 0.0  << intMax << true  << INT_MAX;
    QTest::newRow("int min bottom")       << 0.0   << intMin << 0.0 << true  << INT_MIN;
    QTest::newRow("reversed range")       << 0.5   << 100.0 << 0.0  << false << kUntouched;
    QTest::newRow("empty range")          << 0.5   << 40.0 << 40.0  << false << kUntouched;
    QTest::newRow("nan position")         << nan   << 0.0  << 100.0 << false << kUntouched;
    QTest::newRow("infinite bound")       << 0.5   << 0.0  << inf   << false << kUntouched;
    QTest::newRow("position above one")   << 1.25  << 0.0  << 100.0 << false << kUntouched;
    QTest::newRow("negative position")    << -0.1  << 0.0  << 100.0 << false << kUntouched;
    QTest::newRow("high above int")       << 1.0   << 0.0  << intMax + 1.0 << false << kUntouched;
    QTest::newRow("span overflows")       << 0.5   << -dblMax << dblMax << false << kUntouched;
    QTest::newRow("low below int")        << 0.0   << intMin - 1.0 << 0.0 << false << kUntouched;
}

void ShowCommandTrack_Test::sliderTargetMapsIntoCurrentRange()
{
    QFETCH(qreal, position);
    QFETCH(qreal, low);
    QFETCH(qreal, high);
    QFETCH(bool, ok);
    QFETCH(int, value);

    int target = kUntouched;
    QCOMPARE(ShowCommandFsm::sliderTarget(position, low, high, &target), ok);
    QCOMPARE(target, value);
}

void ShowCommandTrack_Test::resolveControlReportsSuitability_data()
{
    using Role = ShowControlRole;
    const QString intensity = QStringLiteral("Intensity");
    const ShowCommand button = ShowCommand::setButtonState(1, 100, kControlA, true);
    ShowCommand sliderFlash = ShowCommand::setButtonState(10, 150, kControlB, true);
    sliderFlash.role = Role::AdjustSlider;
    const ShowCommand adjust = ShowCommand::setSliderPosition(2, 200, kControlB, Role::AdjustSlider, intensity, 0.5);
    const ShowCommand level = ShowCommand::setSliderPosition(3, 300, kControlB, Role::LevelSlider, QString(), 0.5);
    const ShowCommand master = ShowCommand::setSliderPosition(4, 400, kControlB, Role::GrandMasterSlider,
                                                              QString(), 0.5);
    const ShowCommand submaster = ShowCommand::setSliderPosition(5, 500, kControlB, Role::SubmasterSlider,
                                                                 QString(), 0.5);

    QTest::addColumn<ShowCommand>("expected");
    QTest::addColumn<QVector<ShowControlSnapshot>>("matches");
    QTest::addColumn<ShowControlStatus>("status");

    QTest::newRow("button ready") << button
        << QVector<ShowControlSnapshot>({{Role::ToggleButton, QString(), true, true}}) << ShowControlStatus::Ready;
    QTest::newRow("adjust ready") << adjust
        << QVector<ShowControlSnapshot>({{Role::AdjustSlider, intensity, true, true}}) << ShowControlStatus::Ready;
    QTest::newRow("slider flash ready") << sliderFlash
        << QVector<ShowControlSnapshot>({{Role::AdjustSlider, intensity, true, true}})
        << ShowControlStatus::Ready;
    QTest::newRow("level ready without function") << level
        << QVector<ShowControlSnapshot>({{Role::LevelSlider, QString(), true, false}}) << ShowControlStatus::Ready;
    QTest::newRow("grand master ready without function") << master
        << QVector<ShowControlSnapshot>({{Role::GrandMasterSlider, QString(), true, false}})
        << ShowControlStatus::Ready;
    QTest::newRow("submaster ready without function") << submaster
        << QVector<ShowControlSnapshot>({{Role::SubmasterSlider, QString(), true, false}})
        << ShowControlStatus::Ready;
    QTest::newRow("missing") << button << QVector<ShowControlSnapshot>() << ShowControlStatus::Missing;
    QTest::newRow("ambiguous") << level
        << QVector<ShowControlSnapshot>({{Role::LevelSlider, QString(), true, false},
                                         {Role::LevelSlider, QString(), true, false}})
        << ShowControlStatus::Ambiguous;
    QTest::newRow("role changed") << button
        << QVector<ShowControlSnapshot>({{Role::LevelSlider, QString(), true, true}})
        << ShowControlStatus::Incompatible;
    QTest::newRow("attribute changed") << adjust
        << QVector<ShowControlSnapshot>({{Role::AdjustSlider, QStringLiteral("Speed"), true, true}})
        << ShowControlStatus::Incompatible;
    QTest::newRow("disabled") << level
        << QVector<ShowControlSnapshot>({{Role::LevelSlider, QString(), false, false}})
        << ShowControlStatus::Disabled;
    QTest::newRow("button unbound") << button
        << QVector<ShowControlSnapshot>({{Role::ToggleButton, QString(), true, false}})
        << ShowControlStatus::Unbound;
    QTest::newRow("adjust unbound") << adjust
        << QVector<ShowControlSnapshot>({{Role::AdjustSlider, intensity, true, false}})
        << ShowControlStatus::Unbound;
}

void ShowCommandTrack_Test::resolveControlReportsSuitability()
{
    QFETCH(ShowCommand, expected);
    QFETCH(QVector<ShowControlSnapshot>, matches);
    QFETCH(ShowControlStatus, status);

    QString reason = QStringLiteral("untouched");
    QCOMPARE(ShowCommandFsm::resolveControl(expected, matches, &reason), status);
    QCOMPARE(reason.isEmpty(), status == ShowControlStatus::Ready);
}

QTEST_APPLESS_MAIN(ShowCommandTrack_Test)
void ShowCommandTrack_Test::legacyComplexValues_areClosedInTrack_data()
{
    QTest::addColumn<QString>("action");
    QTest::addColumn<QString>("arguments");
    QTest::newRow("six component color") << QStringLiteral("SetSliderColors") << QStringLiteral("12,34,56;78,90,123");
    QTest::newRow("fractional Pan/Tilt") << QStringLiteral("SetXYPadPosition") << QStringLiteral("64.5,192.25");
    QTest::newRow("metre floor target") << QStringLiteral("SetXYPadFloor") << QStringLiteral("7.25,1.75,4.125");
}

void ShowCommandTrack_Test::legacyComplexValues_areClosedInTrack()
{
    QFETCH(QString, action);
    QFETCH(QString, arguments);
    ShowCommand command;
    command.id = 1;
    command.controlId = QUuid::createUuid();
    QVERIFY(ShowCommand::actionFromString(action, &command.action));
    command.role = action == QStringLiteral("SetSliderColors") ? ShowControlRole::LevelSlider : ShowControlRole::XYPad;
    command.attribute = arguments;
    ShowCommandTrack track;
    QVERIFY(track.insert(command));
    const ShowCommand stored = track.commands().first();
    QVERIFY(stored.attribute.isEmpty());
    QVERIFY(stored.payload.value.index() != 0);
}
