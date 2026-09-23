/*
  Q Light Controller Plus - Unit test
  show_test.cpp

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

#include <QtTest>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "showcommandtrack.h"
#include "show_test.h"
#include "show.h"

void Show_Test::initTestCase()
{
    m_doc = new Doc(this);
}

void Show_Test::cleanupTestCase()
{
    delete m_doc;
}

void Show_Test::defaults()
{
    Show s(m_doc);

    // check defaults
    QCOMPARE(s.type(), Function::ShowType);
    QCOMPARE(s.id(), Function::invalidId());
    QCOMPARE(s.name(), "New Show");
    QCOMPARE(s.attributes().count(), 0);
}

void Show_Test::copy()
{
    Show show(m_doc);
    show.setID(123);
    show.setTimeDivision(Show::BPM_3_4, 123);

    Scene *scene = new Scene(m_doc);
    m_doc->addFunction(scene);

    Track *t = new Track(123, &show);
    t->setSceneID(456);
    t->setName("Original track");

    ShowFunction *sf = new ShowFunction(show.getLatestShowFunctionId());
    sf->setFunctionID(scene->id());
    sf->setStartTime(1000);
    sf->setDuration(2000);
    sf->setLocked(true);

    t->addShowFunction(sf);
    show.addTrack(t);

    QVERIFY(show.showFunction(666) == NULL);
    QVERIFY(show.showFunction(0) == sf);

    Show showCopy(m_doc);
    showCopy.copyFrom(&show);

    QVERIFY(show.timeDivisionType() == Show::BPM_3_4);
    QVERIFY(show.timeDivisionBPM() == 123);
    QVERIFY(show.totalDuration() == 3000);

    QVERIFY(showCopy.getTracksCount() == show.getTracksCount());

    Track *copyTrack = showCopy.tracks().first();

    QVERIFY(copyTrack->getSceneID() == 456);
    QVERIFY(copyTrack->name() == "Original track");
    QVERIFY(copyTrack->showFunctions().count() == 1);

    ShowFunction *copySF = copyTrack->showFunctions().first();
    QVERIFY(copySF->functionID() != scene->id());
    QVERIFY(copySF->startTime() == 1000);
    QVERIFY(copySF->duration() == 2000);
    QVERIFY(copySF->isLocked() == true);

    QVERIFY(show.contains(123) == true);
    QVERIFY(show.contains(124) == false);
    QVERIFY(show.contains(0) == true);

    QVERIFY(show.components().count() == 1);
}

void Show_Test::timeDivision()
{
    Show s(m_doc);
    QCOMPARE(s.timeDivisionType(), Show::Time);
    QCOMPARE(s.timeDivisionBPM(), 120);

    s.setTimeDivision(Show::BPM_4_4, 111);
    QCOMPARE(s.timeDivisionType(), Show::BPM_4_4);
    QCOMPARE(s.timeDivisionBPM(), 111);

    QCOMPARE(s.beatsDivision(), 4);
    s.setTimeDivisionType(Show::BPM_2_4);
    QCOMPARE(s.beatsDivision(), 2);
    s.setTimeDivisionType(Show::BPM_3_4);
    QCOMPARE(s.beatsDivision(), 3);
    s.setTimeDivisionType(Show::Time);
    QCOMPARE(s.beatsDivision(), 0);
    s.setTimeDivisionType(Show::VDJBeat);
    QCOMPARE(s.beatsDivision(), 0);

    QCOMPARE(s.stringToTempo("Time"), Show::Time);
    QCOMPARE(s.stringToTempo("VDJBeat"), Show::VDJBeat);
    QCOMPARE(s.stringToTempo("BPM_4_4"), Show::BPM_4_4);
    QCOMPARE(s.stringToTempo("BPM_3_4"), Show::BPM_3_4);
    QCOMPARE(s.stringToTempo("BPM_2_4"), Show::BPM_2_4);

    QCOMPARE(s.tempoToString(Show::Time), "Time");
    QCOMPARE(s.tempoToString(Show::VDJBeat), "VDJBeat");
    QCOMPARE(s.tempoToString(Show::BPM_4_4), "BPM_4_4");
    QCOMPARE(s.tempoToString(Show::BPM_3_4), "BPM_3_4");
    QCOMPARE(s.tempoToString(Show::BPM_2_4), "BPM_2_4");

    QVERIFY(Show::isTimeBasedDivision(Show::Time) == true);
    QVERIFY(Show::isTimeBasedDivision(Show::VDJBeat) == true);
    QVERIFY(Show::isTimeBasedDivision(Show::BPM_4_4) == false);
    QVERIFY(Show::isTimeBasedDivision(Show::BPM_3_4) == false);
    QVERIFY(Show::isTimeBasedDivision(Show::BPM_2_4) == false);
}

void Show_Test::tracks()
{
    Show s(m_doc);
    s.setID(123);

    QCOMPARE(s.tracks().count(), 0);

    Track *t = new Track(123, &s);
    t->setName("First track");

    Track *t2 = new Track(321, &s);
    t2->setName("Second track");

    QVERIFY(s.addTrack(t) == true);
    QCOMPARE(s.getTracksCount(), 1);
    QCOMPARE(s.tracks().count(), 1);

    QVERIFY(s.track(456) == NULL);
    QVERIFY(s.getTrackFromSceneID(456) == NULL);

    QVERIFY(s.track(0) == t);
    QVERIFY(s.getTrackFromSceneID(123) == t);

    // check sutomatic ID assignment
    QVERIFY(t->id() == 0);
    QVERIFY(t->showId() == 123);

    // check automatic attribute registration
    QCOMPARE(s.attributes().count(), 1);
    QVERIFY(s.attributes().at(0).m_name == "First track-0");

    // add a second track and move it up
    QVERIFY(s.addTrack(t2) == true);
    QCOMPARE(s.getTracksCount(), 2);

    s.moveTrack(t2, -1);
    QVERIFY(s.tracks().at(0)->name() == "Second track");
    QVERIFY(s.tracks().at(1)->name() == "First track");

    // no change
    s.moveTrack(t2, -1);
    QVERIFY(s.tracks().at(0)->name() == "Second track");
    QVERIFY(s.tracks().at(1)->name() == "First track");

    // move back as original
    s.moveTrack(t, -1);
    QVERIFY(s.tracks().at(0)->name() == "First track");
    QVERIFY(s.tracks().at(1)->name() == "Second track");

    // check invalid track removal
    QVERIFY(s.removeTrack(456) == false);
    QCOMPARE(s.tracks().count(), 2);

    // check valid track removal
    QVERIFY(s.removeTrack(0) == true);
    QCOMPARE(s.tracks().count(), 1);
    QCOMPARE(s.attributes().count(), 1);

    QVERIFY(s.removeTrack(1) == true);
    QCOMPARE(s.tracks().count(), 0);
    QCOMPARE(s.attributes().count(), 0);
}

void Show_Test::duration()
{
    Show show(m_doc);
    show.setID(123);

    Scene *scene = new Scene(m_doc);
    m_doc->addFunction(scene);

    Track *t = new Track(123, &show);
    ShowFunction *sf = new ShowFunction(show.getLatestShowFunctionId());
    sf->setFunctionID(scene->id());
    sf->setStartTime(1000);
    sf->setDuration(2000);

    t->addShowFunction(sf);
    show.addTrack(t);

    QVERIFY(show.totalDuration() == 3000);

}

void Show_Test::load()
{
    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly | QIODevice::Text);
    QXmlStreamWriter xmlWriter(&buffer);

    xmlWriter.writeStartElement("Function");
    xmlWriter.writeAttribute("Type", "Show");

    xmlWriter.writeStartElement("TimeDivision");
    xmlWriter.writeAttribute("Type", "BPM_2_4");
    xmlWriter.writeAttribute("BPM", "222");
    xmlWriter.writeEndElement();

    xmlWriter.writeStartElement("Track");
    xmlWriter.writeAttribute("ID", "0");
    xmlWriter.writeAttribute("SceneID", "111");
    xmlWriter.writeAttribute("Name", "Read track 1");
    xmlWriter.writeAttribute("isMute", "0");
    xmlWriter.writeEndElement();

    xmlWriter.writeStartElement("Track");
    xmlWriter.writeAttribute("ID", "1");
    xmlWriter.writeAttribute("SceneID", "222");
    xmlWriter.writeAttribute("Name", "Read track 2");
    xmlWriter.writeAttribute("isMute", "1");
    xmlWriter.writeEndElement();

    xmlWriter.writeEndElement();

    xmlWriter.writeEndDocument();
    xmlWriter.setDevice(NULL);
    buffer.close();

    buffer.open(QIODevice::ReadOnly | QIODevice::Text);
    QXmlStreamReader xmlReader(&buffer);
    xmlReader.readNextStartElement();

    Show s(m_doc);
    QVERIFY(s.loadXML(xmlReader) == true);

    QCOMPARE(s.timeDivisionType(), Show::BPM_2_4);
    QCOMPARE(s.timeDivisionBPM(), 222);

    QCOMPARE(s.getTracksCount(), 2);

    Track *t, *t2;
    t = s.track(111);
    QVERIFY(t == NULL);

    t = s.track(0);
    t2 = s.track(1);

    QCOMPARE(t->name(), "Read track 1");
    QVERIFY(t->getSceneID() == 111);
    QCOMPARE(t->isMute(), false);

    QCOMPARE(t2->name(), "Read track 2");
    QVERIFY(t2->getSceneID() == 222);
    QCOMPARE(t2->isMute(), true);
}

void Show_Test::save()
{
    Show s(m_doc);
    s.setID(123);
    s.setName("Test Show");
    s.setTimeDivision(Show::BPM_3_4, 111);

    Track *t = new Track(456, &s);
    t->setName("First track");

    Track *t2 = new Track(789, &s);
    t2->setName("Second track");
    t2->setMute(true);

    s.addTrack(t);
    s.addTrack(t2);

    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly | QIODevice::Text);
    QXmlStreamWriter xmlWriter(&buffer);

    QVERIFY(s.saveXML(&xmlWriter) == true);
    xmlWriter.setDevice(NULL);
    buffer.close();

    buffer.open(QIODevice::ReadOnly | QIODevice::Text);
    QXmlStreamReader xmlReader(&buffer);


    xmlReader.readNextStartElement();
    QVERIFY(xmlReader.name().toString() == "Function");
    QVERIFY(xmlReader.attributes().value("Type").toString() == "Show");
    QVERIFY(xmlReader.attributes().value("ID").toString() == "123");
    QVERIFY(xmlReader.attributes().value("Name").toString() == "Test Show");

    xmlReader.readNextStartElement();
    QVERIFY(xmlReader.name().toString() == "TimeDivision");
    QVERIFY(xmlReader.attributes().value("Type").toString() == "BPM_3_4");
    QVERIFY(xmlReader.attributes().value("BPM").toString() == "111");
    xmlReader.skipCurrentElement();

    xmlReader.readNextStartElement();
    QVERIFY(xmlReader.name().toString() == "Track");
    QVERIFY(xmlReader.attributes().value("ID").toString() == "0");
    QVERIFY(xmlReader.attributes().value("SceneID").toString() == "456");
    QVERIFY(xmlReader.attributes().value("Name").toString() == "First track");
    QVERIFY(xmlReader.attributes().value("isMute").toString() == "0");
    xmlReader.skipCurrentElement();

    xmlReader.readNextStartElement();
    QVERIFY(xmlReader.name().toString() == "Track");
    QVERIFY(xmlReader.attributes().value("ID").toString() == "1");
    QVERIFY(xmlReader.attributes().value("SceneID").toString() == "789");
    QVERIFY(xmlReader.attributes().value("Name").toString() == "Second track");
    QVERIFY(xmlReader.attributes().value("isMute").toString() == "1");
}

void Show_Test::syncSource()
{
    Show *show = new Show(m_doc);
    m_doc->addFunction(show);

    // Default sync source is Autonomous (0)
    QCOMPARE(show->syncSource(), 0);

    // Set to External (1)
    show->setSyncSource(1);
    QCOMPARE(show->syncSource(), 1);

    // setExternalElapsedTime should not crash when runner is not active
    show->setExternalElapsedTime(5000);

    // Set back to Autonomous
    show->setSyncSource(0);
    QCOMPARE(show->syncSource(), 0);

    m_doc->deleteFunction(show->id());
}


/*****************************************************************************
 * Command track
 *****************************************************************************/

static QString showXML(const QString &body)
{
    return QStringLiteral("<Function Type=\"Show\" ID=\"700\" Name=\"Commanded\">%1</Function>").arg(body);
}

static bool loadShow(Show &show, const QString &xml)
{
    QXmlStreamReader reader(xml);
    reader.readNextStartElement();
    return show.loadXML(reader);
}

static QString saveShow(const Show &show)
{
    QString xml;
    QXmlStreamWriter writer(&xml);
    show.saveXML(&writer);
    return xml;
}

void Show_Test::commandTrackRoundtrip()
{
    Scene *first = new Scene(m_doc);
    m_doc->addFunction(first);
    Scene *second = new Scene(m_doc);
    m_doc->addFunction(second);

    Show show(m_doc);
    show.setID(700);

    ShowCommandTrack track;
    QString error;
    QVERIFY2(track.insert(ShowCommand::start(1, 0, first->id()), &error), qPrintable(error));
    // equal time: authored order must survive the roundtrip
    QVERIFY2(track.insert(ShowCommand::setIntensity(2, 0, first->id(), 0.25), &error), qPrintable(error));
    QVERIFY2(track.insert(ShowCommand::start(3, 0, second->id()), &error), qPrintable(error));
    QVERIFY2(track.insert(ShowCommand::setIntensity(4, 4500, second->id(), 1.0), &error), qPrintable(error));
    QVERIFY2(track.insert(ShowCommand::stop(5, 900, first->id()), &error), qPrintable(error));
    track.setExtent(6000);
    QVERIFY2(show.setCommandTrack(track, {}, &error), qPrintable(error));

    Show reloaded(m_doc);
    QVERIFY(loadShow(reloaded, saveShow(show)));

    const ShowCommandTrack savedTrack = show.commandTrack();
    const ShowCommandTrack readTrack = reloaded.commandTrack();
    const QVector<ShowCommand> &saved = savedTrack.commands();
    const QVector<ShowCommand> &read = readTrack.commands();
    QCOMPARE(read.count(), saved.count());
    for (int i = 0; i < saved.count(); i++)
        QCOMPARE(read.at(i), saved.at(i));

    QCOMPARE(read.at(0).id, quint32(1));
    QCOMPARE(read.at(1).id, quint32(2));
    QCOMPARE(read.at(2).id, quint32(3));
    QCOMPARE(read.at(1).intensity, 0.25);
    // later authored, but earlier in time: ordering is (time, authored order)
    QCOMPARE(read.at(3).id, quint32(5));
    QCOMPARE(read.at(3).time, quint32(900));
    QVERIFY(read.at(3).action == ShowCommandAction::Stop);
    QCOMPARE(read.at(4).id, quint32(4));
    QCOMPARE(read.at(4).time, quint32(4500));
    QCOMPARE(read.at(4).intensity, 1.0);
    QCOMPARE(readTrack.extent(), quint32(6000));
}

void Show_Test::commandTrackLegacyShow()
{
    Show legacy(m_doc);
    QVERIFY(loadShow(legacy, showXML("<TimeDivision Type=\"Time\" BPM=\"120\"/>")));
    QVERIFY(legacy.commandTrack().isEmpty());
    QCOMPARE(legacy.commandTrack().extent(), quint32(0));

    // an untouched Show must keep writing exactly the legacy document
    QVERIFY(saveShow(legacy).contains("CommandTrack") == false);
}

void Show_Test::commandTrackInvalidLoad_data()
{
    QTest::addColumn<QString>("commandTrack");

    QTest::newRow("unsupported version")
        << "<CommandTrack Version=\"2\" Extent=\"100\"/>";
    QTest::newRow("duplicate id")
        << "<CommandTrack Version=\"1\" Extent=\"100\">"
           "<Command ID=\"1\" Time=\"0\" Action=\"Start\" Function=\"5\"/>"
           "<Command ID=\"1\" Time=\"50\" Action=\"Stop\" Function=\"5\"/></CommandTrack>";
    QTest::newRow("unknown action")
        << "<CommandTrack Version=\"1\" Extent=\"100\">"
           "<Command ID=\"1\" Time=\"0\" Action=\"Blink\" Function=\"5\"/></CommandTrack>";
    QTest::newRow("infinite intensity")
        << "<CommandTrack Version=\"1\" Extent=\"100\">"
           "<Command ID=\"1\" Time=\"0\" Action=\"SetIntensity\" Function=\"5\" Value=\"inf\"/></CommandTrack>";
    QTest::newRow("nan intensity")
        << "<CommandTrack Version=\"1\" Extent=\"100\">"
           "<Command ID=\"1\" Time=\"0\" Action=\"SetIntensity\" Function=\"5\" Value=\"nan\"/></CommandTrack>";
    QTest::newRow("intensity out of range")
        << "<CommandTrack Version=\"1\" Extent=\"100\">"
           "<Command ID=\"1\" Time=\"0\" Action=\"SetIntensity\" Function=\"5\" Value=\"1.5\"/></CommandTrack>";
    QTest::newRow("time out of range")
        << "<CommandTrack Version=\"1\" Extent=\"100\">"
           "<Command ID=\"1\" Time=\"4294967295\" Action=\"Start\" Function=\"5\"/></CommandTrack>";
    QTest::newRow("unknown element")
        << "<CommandTrack Version=\"1\" Extent=\"100\"><Lane ID=\"1\"/></CommandTrack>";
}

void Show_Test::commandTrackInvalidLoad()
{
    QFETCH(QString, commandTrack);

    Scene *target = new Scene(m_doc);
    m_doc->addFunction(target);

    Show show(m_doc);
    show.setID(700);

    ShowCommandTrack valid;
    QVERIFY(valid.insert(ShowCommand::start(9, 120, target->id())));
    valid.setExtent(3000);
    QVERIFY(show.setCommandTrack(valid));

    QVERIFY(loadShow(show, showXML(commandTrack)) == false);

    // the previously valid track must be untouched
    QCOMPARE(show.commandTrack().count(), 1);
    QCOMPARE(show.commandTrack().commands().first().id, quint32(9));
    QCOMPARE(show.commandTrack().extent(), quint32(3000));
}

void Show_Test::commandTrackUnresolvedTargetSurvivesLoad()
{
    Show show(m_doc);
    QVERIFY(loadShow(show, showXML("<CommandTrack Version=\"1\" Extent=\"800\">"
                                   "<Command ID=\"1\" Time=\"100\" Action=\"Start\" Function=\"424242\"/>"
                                   "</CommandTrack>")));
    QCOMPARE(show.commandTrack().count(), 1);

    // a forward reference is diagnosed, never silently dropped
    show.postLoad();
    QCOMPARE(show.commandTrack().count(), 1);
    QCOMPARE(show.commandTrack().commands().first().functionId, quint32(424242));
}

void Show_Test::commandTrackRejectedEdit_data()
{
    QTest::addColumn<bool>("selfTarget");

    QTest::newRow("self recursive target") << true;
    QTest::newRow("missing target") << false;
}

void Show_Test::commandTrackRejectedEdit()
{
    QFETCH(bool, selfTarget);

    Scene *target = new Scene(m_doc);
    m_doc->addFunction(target);

    Show show(m_doc);
    show.setID(700);

    ShowCommandTrack valid;
    QVERIFY(valid.insert(ShowCommand::start(1, 0, target->id())));
    QVERIFY(show.setCommandTrack(valid));

    ShowCommandTrack broken;
    QVERIFY(broken.insert(ShowCommand::start(1, 0, selfTarget ? show.id() : quint32(424243))));

    QString error;
    QSignalSpy changed(&show, &Show::commandTrackChanged);
    QVERIFY(show.setCommandTrack(broken, {}, &error) == false);
    QVERIFY(error.isEmpty() == false);
    QCOMPARE(changed.count(), 0);
    QCOMPARE(show.commandTrack().commands().first().functionId, target->id());
}

void Show_Test::commandTrackExtent_data()
{
    QTest::addColumn<quint32>("authored");
    QTest::addColumn<quint32>("expected");

    QTest::newRow("raised to last event") << quint32(100) << quint32(900);
    QTest::newRow("trailing start not extended") << quint32(900) << quint32(900);
    QTest::newRow("explicit longer extent") << quint32(5000) << quint32(5000);
}

void Show_Test::commandTrackExtent()
{
    QFETCH(quint32, authored);
    QFETCH(quint32, expected);

    Scene *target = new Scene(m_doc);
    m_doc->addFunction(target);

    Show show(m_doc);
    show.setID(700);

    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::start(1, 0, target->id())));
    QVERIFY(track.insert(ShowCommand::start(2, 900, target->id())));
    track.setExtent(authored);

    QString error;
    QVERIFY2(show.setCommandTrack(track, {}, &error), qPrintable(error));
    QCOMPARE(show.commandTrack().extent(), expected);
}

void Show_Test::commandTrackCopyAndReferences()
{
    Scene *clipped = new Scene(m_doc);
    clipped->setName("Clipped");
    m_doc->addFunction(clipped);
    Scene *external = new Scene(m_doc);
    external->setName("External");
    m_doc->addFunction(external);

    Show show(m_doc);
    show.setID(700);
    Track *t = new Track(clipped->id(), &show);
    ShowFunction *sf = new ShowFunction(show.getLatestShowFunctionId());
    sf->setFunctionID(clipped->id());
    sf->setStartTime(0);
    sf->setDuration(1000);
    t->addShowFunction(sf);
    show.addTrack(t);

    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::start(1, 100, clipped->id())));
    QVERIFY(track.insert(ShowCommand::setIntensity(2, 200, external->id(), 0.5)));
    track.setExtent(2000);
    QVERIFY(show.setCommandTrack(track));

    QVERIFY(show.contains(clipped->id()));
    QVERIFY(show.contains(external->id()));
    QVERIFY(show.components().contains(external->id()));

    Show copy(m_doc);
    QVERIFY(copy.copyFrom(&show));

    const quint32 copiedClip = copy.tracks().first()->showFunctions().first()->functionID();
    QVERIFY(copiedClip != clipped->id());
    QCOMPARE(copy.commandTrack().count(), 2);
    // a command target that was deep copied with its clip follows the copy
    QCOMPARE(copy.commandTrack().commands().at(0).functionId, copiedClip);
    // a target owned by nobody else stays shared
    QCOMPARE(copy.commandTrack().commands().at(1).functionId, external->id());
    QCOMPARE(copy.commandTrack().extent(), quint32(2000));
    // the source Show is untouched
    QCOMPARE(show.commandTrack().commands().at(0).functionId, clipped->id());
}

void Show_Test::commandTrackCopyReusesOneClonePerTarget()
{
    Scene *commanded = new Scene(m_doc);
    commanded->setName("Commanded");
    m_doc->addFunction(commanded);
    Scene *plain = new Scene(m_doc);
    plain->setName("Plain");
    m_doc->addFunction(plain);

    Show show(m_doc);
    show.setID(701);
    Track *t = new Track(commanded->id(), &show);
    // the same two functions appear in two clips each
    for (quint32 start : {quint32(100), quint32(500)})
    {
        ShowFunction *commandedClip = new ShowFunction(show.getLatestShowFunctionId());
        commandedClip->setFunctionID(commanded->id());
        commandedClip->setStartTime(start);
        commandedClip->setDuration(200);
        t->addShowFunction(commandedClip);

        ShowFunction *plainClip = new ShowFunction(show.getLatestShowFunctionId());
        plainClip->setFunctionID(plain->id());
        plainClip->setStartTime(start);
        plainClip->setDuration(200);
        t->addShowFunction(plainClip);
    }
    show.addTrack(t);

    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::stop(1, 180, commanded->id())));
    track.setExtent(1000);
    QVERIFY(show.setCommandTrack(track));

    Show copy(m_doc);
    QVERIFY(copy.copyFrom(&show));

    QList<quint32> commandedClones;
    QList<quint32> plainClones;
    foreach (ShowFunction *sf, copy.tracks().first()->showFunctions())
    {
        Function *copied = m_doc->function(sf->functionID());
        QVERIFY2(copied != NULL, "every copied clip must resolve in the Doc");
        if (copied->name().contains("Commanded"))
            commandedClones.append(sf->functionID());
        else
            plainClones.append(sf->functionID());
    }

    const quint32 commandTarget = copy.commandTrack().commands().first().functionId;

    QCOMPARE(commandedClones.count(), 2);
    QCOMPARE(plainClones.count(), 2);
    QVERIFY(commandedClones.first() != commanded->id());
    // one clone serves every clip of a commanded target, so the command
    // reaches the first occurrence as well as the last
    QVERIFY2(commandedClones.at(0) == commandedClones.at(1),
             "every clip of a commanded target must share one copy");
    QCOMPARE(commandTarget, commandedClones.first());
    QVERIFY(m_doc->function(commandTarget) != NULL);
    // an uncommanded target keeps its existing per-clip copy behaviour
    QVERIFY(plainClones.at(0) != plainClones.at(1));
}

void Show_Test::commandRecordingAndPosition()
{
    Show *show = new Show(m_doc);
    m_doc->addFunction(show);

    QCOMPARE(show->commandRecording(), false);
    show->setCommandRecording(true);
    QCOMPARE(show->commandRecording(), true);

    // an externally driven Show reports the external clock while it is not running
    show->setSyncSource(1);
    show->setExternalElapsedTime(7200);
    QCOMPARE(show->commandPosition(), quint32(7200));

    show->setCommandRecording(false);
    QCOMPARE(show->commandRecording(), false);

    Scene *target = new Scene(m_doc);
    m_doc->addFunction(target);
    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::start(1, 0, target->id())));
    QSignalSpy changed(show, &Show::commandTrackChanged);
    QVERIFY(show->setCommandTrack(track));
    QCOMPARE(changed.count(), 1);

    m_doc->deleteFunction(show->id());
}

QTEST_APPLESS_MAIN(Show_Test)
