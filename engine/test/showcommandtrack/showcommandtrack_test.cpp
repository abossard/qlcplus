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
    unknownAction.action = static_cast<ShowCommandAction>(9);
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

    QTest::newRow("button without control")
        << ShowCommand::setButtonState(26, 800, QUuid(), true) << false;
    ShowCommand buttonWithFunction = ShowCommand::setButtonState(27, 800, kControlA, true);
    buttonWithFunction.functionId = 42;
    QTest::newRow("button with function") << buttonWithFunction << false;
    ShowCommand buttonAsSlider = ShowCommand::setButtonState(28, 800, kControlA, true);
    buttonAsSlider.role = ShowControlRole::LevelSlider;
    QTest::newRow("button with slider role") << buttonAsSlider << false;
    ShowCommand buttonWithPosition = ShowCommand::setButtonState(29, 800, kControlA, false);
    buttonWithPosition.position = 0.5;
    QTest::newRow("button carrying a position") << buttonWithPosition << false;
    QTest::newRow("slider with button role")
        << ShowCommand::setSliderPosition(30, 900, kControlB, ShowControlRole::ToggleButton,
                                          QString(), 0.5) << false;
    QTest::newRow("slider without role")
        << ShowCommand::setSliderPosition(31, 900, kControlB, ShowControlRole::None,
                                          QString(), 0.5) << false;
    QTest::newRow("level slider with attribute")
        << ShowCommand::setSliderPosition(32, 900, kControlB, ShowControlRole::LevelSlider,
                                          QStringLiteral("Intensity"), 0.5) << false;
    QTest::newRow("adjust slider without attribute")
        << ShowCommand::setSliderPosition(33, 900, kControlB, ShowControlRole::AdjustSlider,
                                          QString(), 0.5) << false;
    QTest::newRow("slider above one")
        << ShowCommand::setSliderPosition(34, 900, kControlB, ShowControlRole::LevelSlider,
                                          QString(), 1.5) << false;
    QTest::newRow("slider nan")
        << ShowCommand::setSliderPosition(35, 900, kControlB, ShowControlRole::LevelSlider,
                                          QString(), qQNaN()) << false;
    ShowCommand sliderOn = ShowCommand::setSliderPosition(36, 900, kControlB,
                                                          ShowControlRole::LevelSlider, QString(), 0.5);
    sliderOn.on = true;
    QTest::newRow("slider carrying a button state") << sliderOn << false;

    ShowCommand legacyWithControl = ShowCommand::start(37, 700, 42);
    legacyWithControl.controlId = kControlA;
    QTest::newRow("function command with control") << legacyWithControl << false;
    ShowCommand legacyWithRole = ShowCommand::stop(38, 700, 42);
    legacyWithRole.role = ShowControlRole::ToggleButton;
    QTest::newRow("function command with role") << legacyWithRole << false;
    ShowCommand legacyWithAttribute = ShowCommand::setIntensity(39, 700, 42, 0.5);
    legacyWithAttribute.attribute = QStringLiteral("Intensity");
    QTest::newRow("function command with attribute") << legacyWithAttribute << false;
    ShowCommand legacyWithState = ShowCommand::start(40, 700, 42);
    legacyWithState.on = true;
    QTest::newRow("function command with button state") << legacyWithState << false;
}

void ShowCommandTrack_Test::commandValidation()
{
    QFETCH(ShowCommand, command);
    QFETCH(bool, valid);

    const QString error = ShowCommand::validate(command);
    QCOMPARE(error.isEmpty(), valid);

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
    QVERIFY(fromXml(xml, &loaded, &error));
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(idsOf(loaded), QVector<quint32>({0, 1, 2, 3, 4, 5}));
    for (int i = 0; i < saved.count(); i++)
        QVERIFY(loaded.commands().at(i) == saved.commands().at(i));
    QCOMPARE(toXml(loaded), xml);
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
    QTest::newRow("future version") << track("Version=\"3\"", start) << false << 5;

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

void ShowCommandTrack_Test::seekRestoresLatestValues_data()
{
    QTest::addColumn<quint32>("destination");
    QTest::addColumn<QString>("restored");

    QTest::newRow("start of show") << 0u << QString();
    QTest::newRow("before any value") << 1000u << QString();
    QTest::newRow("on a value boundary") << 1500u << QStringLiteral("20");
    QTest::newRow("just past a value") << 1501u << QStringLiteral("20,9");
    QTest::newRow("on the stop") << 4200u << QStringLiteral("9,21");
    QTest::newRow("past a stopped target") << 4201u << QStringLiteral("21,11");
    QTest::newRow("beyond the last command") << 18000u << QStringLiteral("21,11");
}

void ShowCommandTrack_Test::seekRestoresLatestValues()
{
    QFETCH(quint32, destination);
    QFETCH(QString, restored);

    const ShowCommandTrack track = playbackTrack();
    ShowCommandState state = playingState();
    state.recording = true;
    state.boundShowId = 55;
    state.resolvedShowId = 55;

    const ShowCommandTransition step = ShowCommandFsm::seek(track, state, destination);

    QCOMPARE(idsOf(step.effects), restored);
    for (const ShowCommand &effect : step.effects)
        QVERIFY(effect.action == ShowCommandAction::SetIntensity);

    QVERIFY(step.authored.isEmpty());
    QCOMPARE(step.state.position, destination);
    QCOMPARE(step.state.consumedThrough, destination);
    QVERIFY(step.state.recording);
    QCOMPARE(step.state.boundShowId, 55u);
    QVERIFY(step.state.playing);
}

void ShowCommandTrack_Test::seekKeepsIntentAndLoopReplaysCommands()
{
    const ShowCommandTrack track = playbackTrack();

    ShowCommandTransition step = ShowCommandFsm::advance(track, playingState(), 4200);
    QCOMPARE(idsOf(step.effects), QStringLiteral("7,8,20,9,21,10,11"));

    // looping back re-arms the whole traversal
    step = ShowCommandFsm::seek(track, step.state, 0);
    QVERIFY(step.effects.isEmpty());
    QCOMPARE(step.state.consumedThrough, 0u);

    step = ShowCommandFsm::advance(track, step.state, 1000);
    QCOMPARE(idsOf(step.effects), QStringLiteral("7,8,20"));

    // a seek while paused keeps the transport paused and still restores values
    const ShowCommandState paused = ShowCommandFsm::setPlaying(step.state, false);
    const ShowCommandTransition jumped = ShowCommandFsm::seek(track, paused, 3500);
    QCOMPARE(idsOf(jumped.effects), QStringLiteral("9,21"));
    QVERIFY(!jumped.state.playing);
    QCOMPARE(jumped.state.position, 3500u);
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

    state = ShowCommandFsm::seek(track, state, 0).state;
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
    QCOMPARE(state.consumedLiveEventIds, QSet<quint32>({1}));

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
    state = ShowCommandFsm::seek(track, state, 0).state;
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
    QCOMPARE(step.state.consumedLiveEventIds, QSet<quint32>({2}));
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
    state = ShowCommandFsm::seek(track, state, 0).state;
    QCOMPARE(idsOf(ShowCommandFsm::advance(track, state, 4200).effects), QStringLiteral("0,1,2"));

    // seeking away re-arms the whole traversal, marks included
    input.functionId = 30;
    const ShowCommandState marked = ShowCommandFsm::userInput(track, state, input).state;
    QVERIFY(!marked.consumedLiveEventIds.isEmpty());
    QVERIFY(ShowCommandFsm::seek(track, marked, 0).state.consumedLiveEventIds.isEmpty());
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
    QCOMPARE(step.state.consumedLiveEventIds, QSet<quint32>({5}));
    QVERIFY(step.effects.isEmpty());

    // a button state offered with a slider role is explained, not authored
    ShowCommandInput malformed;
    malformed.origin = ShowCommandOrigin::Pointer;
    malformed.action = ShowCommandAction::SetButtonState;
    malformed.controlId = kControlA;
    malformed.role = ShowControlRole::LevelSlider;
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