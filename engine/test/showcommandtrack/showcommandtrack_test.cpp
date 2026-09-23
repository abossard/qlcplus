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

    QVERIFY(track.retime(7, 4200, &error));
    QVERIFY(error.isEmpty());
    QCOMPARE(idsOf(track), QVector<quint32>({8, 9, 10, 11, 7}));
    QCOMPARE(track.commands().at(4).time, 4200u);
    QCOMPARE(track.commands().at(4).functionId, 12u);
    QVERIFY(track.commands().at(4).action == ShowCommandAction::Start);
    QCOMPARE(track.lastCommandTime(), 4200u);

    QVERIFY(!track.retime(99, 100, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!track.retime(7, ShowCommand::InvalidId, &error));
    QCOMPARE(track.commands().at(4).time, 4200u);
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
    QTest::newRow("future version") << track("Version=\"2\"", start) << false << 5;
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

QTEST_APPLESS_MAIN(ShowCommandTrack_Test)