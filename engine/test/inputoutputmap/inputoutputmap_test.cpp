/*
  Q Light Controller Plus - Unit test
  inputoutputmap_test.cpp

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
#include <QSignalSpy>
#include <QXmlStreamReader>
#include <QtTest>
#include <algorithm>
#include <atomic>
#include <thread>
#include <array>
#include <cmath>
#include <limits>
#include "../doc/audio_test_capture.h"
#include "audioanalyzer.h"
#include "audiochannel.h"
#include "audioframe.h"

#define private public
#define protected public
#include "iopluginstub.h"
#include "inputoutputmap_test.h"
#include "inputoutputmap.h"
#include "channelmodifier.h"
#include "qlcinputsource.h"
#include "chaserstep.h"
#include "grandmaster.h"
#include "outputpatch.h"
#include "inputpatch.h"
#include "qlcconfig.h"
#include "universe.h"
#include "fixture.h"
#include "chaser.h"
#include "scene.h"
#include "qlcfile.h"
#include "doc.h"
#undef protected
#undef private

namespace
{
class AudioClockChaser : public Chaser
{
public:
    explicit AudioClockChaser(Doc *doc) : Chaser(doc) {}
    QList<bool> consumedBeats;
    void write(MasterTimer *timer, QList<Universe *> universes) override
    {
        consumedBeats.append(timer->isBeat());
        Chaser::write(timer, universes);
    }
};

struct AudioClockTest
{
    Doc doc{nullptr, 0};
    InputOutputMap *iom = doc.inputOutputMap();
    AudioProfile *profile = new AudioProfile(7, &doc);
    AudioSnapshot snapshot;
    const int64_t origin = AudioRenderView::nowNs() + 100000000000LL;

    AudioClockTest()
    {
        auto capture = QSharedPointer<AudioTestCapture>::create();
        doc.m_inputCapture = capture;
        capture->setAnalyzer(doc.audioAnalyzer());
        doc.addAudioProfile(profile);
        iom->setBeatGeneratorType(InputOutputMap::Audio);
        iom->m_audioPollTimer.stop();
        snapshot.sourceId = "stability-test";
        snapshot.sourceEpoch = 1;
        snapshot.available = true;
        snapshot.music.valid = true;
    }

    bool poll(int repeats = 1)
    {
        for (int i = 0; i < repeats; ++i)
            if (!QMetaObject::invokeMethod(iom, "slotPollAudio", Qt::DirectConnection))
                return false;
        return true;
    }

    bool publish(double bpm, int milliseconds, int repeats = 1)
    {
        snapshot.music.bpm = bpm;
        snapshot.publishTimeNs = origin + int64_t(milliseconds) * 1000000;
        ++snapshot.frameSequence;
        profile->channel()->injectSnapshot(snapshot);
        return poll(repeats);
    }

    AudioClockChaser *chaser()
    {
        auto *chaser = new AudioClockChaser(&doc);
        chaser->setTempoType(Function::Beats);
        chaser->setDurationMode(Chaser::PerStep);
        chaser->setFadeInMode(Chaser::PerStep);
        chaser->setFadeOutMode(Chaser::PerStep);
        for (int i = 0; i < 2; ++i)
        {
            auto *scene = new Scene(&doc);
            doc.addFunction(scene);
            ChaserStep step(scene->id());
            step.fadeIn = 0;
            step.fadeOut = 0;
            step.duration = 1000;
            step.hold = 1000;
            chaser->addStep(step);
        }
        doc.addFunction(chaser);
        chaser->start(doc.masterTimer(), FunctionParent::master());
        return chaser;
    }
};

double jitterBpm(int observation, int trace, int milliseconds)
{
    static const double traces[][8] = {
        {119, 120, 121, 120, 121, 119, 120, 120},
        {119.49, 119.51, 120.49, 120.51, 120.49, 119.51, 120.51, 119.49},
        {120, 120, 119, 120, 120, 120, 121, 120}
    };
    if (trace < 3)
        return traces[trace][observation % 8];
    const int time = (milliseconds - 100) % 3000;
    const int direction = trace == 4 || trace == 6 ? -1 : 1;
    if (time < 900 || (trace >= 5 && time >= 1000 && time < 1900))
        return 120 + direction * (1 + (time / 100) % 2);
    if (trace >= 5 && time == 900)
        return 120 - direction;
    return 120;
}
}

#define TESTPLUGINDIR "../iopluginstub"
#define ENGINEDIR "../../src"
#include "../common/resource_paths.h"

static QDir testPluginDir()
{
    QDir dir(TESTPLUGINDIR);
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
    return dir;
}

void InputOutputMap_Test::initTestCase()
{
    m_doc = new Doc(this);
    m_doc->ioPluginCache()->load(testPluginDir());
    QVERIFY(m_doc->ioPluginCache()->plugins().size() != 0);
}

void InputOutputMap_Test::cleanupTestCase()
{
    delete m_doc;
    m_doc = NULL;
}

void InputOutputMap_Test::initial()
{
    InputOutputMap im(m_doc, 4);
    QVERIFY(im.universesCount() == 4);
    QVERIFY(im.m_universeArray.count() == 4);
    QVERIFY(im.universeNames().count() == 4);
    QVERIFY(im.m_profiles.size() == 0);
    QVERIFY(im.profileNames().size() == 0);
}

void InputOutputMap_Test::pluginNames()
{
    InputOutputMap im(m_doc, 4);
    QCOMPARE(im.outputPluginNames().size(), 1);
    QCOMPARE(im.outputPluginNames().at(0), QString("I/O Plugin Stub"));
}

void InputOutputMap_Test::pluginInputs()
{
    InputOutputMap im(m_doc, 4);

    QVERIFY(im.pluginInputs("Foo").size() == 0);

    IOPluginStub* stub = static_cast<IOPluginStub*>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);

    QVERIFY(im.pluginInputs(stub->name()).size() == 4);
    QVERIFY(im.pluginInputs(stub->name()) == stub->inputs());
    QVERIFY(im.inputPluginNames().count() == 1);
    QVERIFY(im.inputPluginNames().at(0) == stub->name());
    QVERIFY(im.pluginSupportsFeedback(stub->name()) == false);
}

void InputOutputMap_Test::pluginOutputs()
{
    InputOutputMap om(m_doc, 4);

    IOPluginStub* stub = static_cast<IOPluginStub*>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);

    QStringList ls(om.pluginOutputs(stub->name()));
    QVERIFY(ls == stub->outputs());

    QVERIFY(om.pluginOutputs("Foobar").isEmpty() == true);
}

void InputOutputMap_Test::configurePlugin()
{
    InputOutputMap im(m_doc, 4);

    QCOMPARE(im.canConfigurePlugin("Foo"), false);

    IOPluginStub* stub = static_cast<IOPluginStub*>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);

    QCOMPARE(im.canConfigurePlugin("Foo"), false);
    QCOMPARE(im.canConfigurePlugin(stub->name()), false);
    stub->m_canConfigure = true;
    QCOMPARE(im.canConfigurePlugin(stub->name()), true);

    /* Must be able to call multiple times */
    im.configurePlugin(stub->name());
    QVERIFY(stub->m_configureCalled == 1);
    im.configurePlugin(stub->name());
    QVERIFY(stub->m_configureCalled == 2);
    im.configurePlugin(stub->name());
    QVERIFY(stub->m_configureCalled == 3);
}

void InputOutputMap_Test::inputPluginStatus()
{
    InputOutputMap im(m_doc, 4);

    QVERIFY(im.inputPluginStatus("Foo", QLCIOPlugin::invalidLine()).contains("Nothing selected"));
    QVERIFY(im.inputPluginStatus("Bar", 0).contains("Nothing selected"));
    QVERIFY(im.inputPluginStatus("Baz", 1).contains("Nothing selected"));
    QVERIFY(im.inputPluginStatus("Xyzzy", 2).contains("Nothing selected"));
    QVERIFY(im.inputPluginStatus("AYBABTU", 3).contains("Nothing selected"));

    IOPluginStub* stub = static_cast<IOPluginStub*>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);

    QVERIFY(im.inputPluginStatus(stub->name(), QLCIOPlugin::invalidLine()) == stub->inputInfo(QLCIOPlugin::invalidLine()));
    QVERIFY(im.inputPluginStatus(stub->name(), 0) == stub->inputInfo(0));
    QVERIFY(im.inputPluginStatus(stub->name(), 1) == stub->inputInfo(1));
    QVERIFY(im.inputPluginStatus(stub->name(), 2) == stub->inputInfo(2));

    QVERIFY(im.pluginDescription("Foo") == "");
    QVERIFY(im.pluginDescription(stub->name()) == stub->pluginInfo());
}

void InputOutputMap_Test::outputPluginStatus()
{
    InputOutputMap om(m_doc, 4);

    QVERIFY(om.outputPluginStatus("Foo", QLCIOPlugin::invalidLine()).contains("Nothing selected"));
    QVERIFY(om.outputPluginStatus("Bar", 0).contains("Nothing selected"));
    QVERIFY(om.outputPluginStatus("Baz", 1).contains("Nothing selected"));
    QVERIFY(om.outputPluginStatus("Xyzzy", 2).contains("Nothing selected"));
    QVERIFY(om.outputPluginStatus("AYBABTU", 3).contains("Nothing selected"));

    IOPluginStub* stub = static_cast<IOPluginStub*>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);

    QVERIFY(om.outputPluginStatus(stub->name(), 4) == stub->outputInfo(QLCIOPlugin::invalidLine()));
    QVERIFY(om.outputPluginStatus(stub->name(), 0) == stub->outputInfo(0));
    QVERIFY(om.outputPluginStatus(stub->name(), 1) == stub->outputInfo(1));
    QVERIFY(om.outputPluginStatus(stub->name(), 2) == stub->outputInfo(2));
}

void InputOutputMap_Test::universeNames()
{
    InputOutputMap iom(m_doc, 4);

    QCOMPARE(quint32(iom.universeNames().size()), iom.universesCount());
    QVERIFY(iom.universeNames().at(0).contains("Universe"));
    QVERIFY(iom.universeNames().at(1).contains("Universe"));
    QVERIFY(iom.universeNames().at(2).contains("Universe"));
    QVERIFY(iom.universeNames().at(3).contains("Universe"));

    IOPluginStub* stub = static_cast<IOPluginStub*>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);

    iom.setOutputPatch(0, stub->name(), "", "", 3);
    QCOMPARE(quint32(iom.universeNames().size()), iom.universesCount());
    QCOMPARE(iom.universeNames().at(0), QString("Universe 1"));
    QCOMPARE(iom.universeNames().at(1), QString("Universe 2"));
    QCOMPARE(iom.universeNames().at(2), QString("Universe 3"));
    QCOMPARE(iom.universeNames().at(3), QString("Universe 4"));

    iom.setOutputPatch(3, stub->name(), "", "", 2);
    QCOMPARE(quint32(iom.universeNames().size()), iom.universesCount());
    QCOMPARE(iom.universeNames().at(0), QString("Universe 1"));
    QCOMPARE(iom.universeNames().at(1), QString("Universe 2"));
    QCOMPARE(iom.universeNames().at(2), QString("Universe 3"));
    QCOMPARE(iom.universeNames().at(3), QString("Universe 4"));

    iom.setUniverseName(1, "Name Changed");
    iom.setUniverseName(42, "This is not the Universe you're looking for");

    QCOMPARE(iom.getUniverseNameByIndex(1), QString("Name Changed"));
    QCOMPARE(iom.getUniverseNameByIndex(2), QString("Universe 3"));
    QCOMPARE(iom.getUniverseNameByIndex(42), QString());
    QCOMPARE(iom.getUniverseNameByID(3), QString("Universe 4"));
}

void InputOutputMap_Test::addUniverse()
{
    InputOutputMap im(m_doc, 4);
    QVERIFY(im.universesCount() == 4);
    QVERIFY(im.addUniverse() == true);
    QVERIFY(im.universesCount() == 5);
    QVERIFY(im.getUniverseID(4) == 4);
    QVERIFY(im.getUniverseID(42) == Universe::invalid());

    /* try to add an existing universe */
    QVERIFY(im.addUniverse(3) == false);
    QVERIFY(im.universesCount() == 5);

    /* add a universe with high id and check that
     * there's no gaps */
    QVERIFY(im.addUniverse(8) == true);
    QVERIFY(im.universesCount() == 9);
}

void InputOutputMap_Test::removeUniverse()
{
    InputOutputMap im(m_doc, 4);
    QVERIFY(im.universesCount() == 4);

    // Creating a gap in the universe list is forbidden
    QVERIFY(im.removeUniverse(1) == false);
    QVERIFY(im.universesCount() == 4);

    // Removing the last universe is OK
    QVERIFY(im.removeUniverse(3) == true);
    QVERIFY(im.universesCount() == 3);

    QVERIFY(im.removeUniverse(7) == false);
    im.removeAllUniverses();
    QVERIFY(im.universesCount() == 0);
}

void InputOutputMap_Test::universe()
{
    InputOutputMap im(m_doc, 4);
    QVERIFY(im.universes().count() == 4);

    im.setUniversePassthrough(1, true);
    QVERIFY(im.getUniversePassthrough(1) == true);
    im.setUniversePassthrough(42, true);
    QVERIFY(im.getUniversePassthrough(42) == false);

    im.setUniverseMonitor(2, true);
    QVERIFY(im.getUniverseMonitor(2) == true);
    im.setUniverseMonitor(42, true);
    QVERIFY(im.getUniverseMonitor(42) == false);
}

void InputOutputMap_Test::profiles()
{
    InputOutputMap im(m_doc, 4);
    QVERIFY(im.m_profiles.size() == 0);

    QLCInputProfile* prof = new QLCInputProfile();
    prof->setManufacturer("Foo");
    prof->setModel("Bar");

    QVERIFY(im.addProfile(prof) == true);
    QVERIFY(im.m_profiles.size() == 1);
    QVERIFY(im.addProfile(prof) == false);
    QVERIFY(im.m_profiles.size() == 1);

    QVERIFY(im.profileNames().size() == 1);
    QVERIFY(im.profileNames().at(0) == prof->name());
    QVERIFY(im.profile(prof->name()) == prof);
    QVERIFY(im.profile("Foobar") == NULL);

    QVERIFY(im.removeProfile("Foobar") == false);
    QVERIFY(im.m_profiles.size() == 1);
    QVERIFY(im.removeProfile(prof->name()) == true);
    QVERIFY(im.m_profiles.size() == 0);
}

void InputOutputMap_Test::setInputPatch()
{
    InputOutputMap im(m_doc, 4);

    IOPluginStub* stub = static_cast<IOPluginStub*>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);

    QLCInputProfile* prof = new QLCInputProfile();
    prof->setManufacturer("Foo");
    prof->setModel("Bar");
    im.addProfile(prof);

    QVERIFY(im.inputPatch(0) == NULL);
    QVERIFY(im.inputPatch(1) == NULL);
    QVERIFY(im.inputPatch(2) == NULL);
    QVERIFY(im.inputPatch(3) == NULL);
    QVERIFY(im.inputMapping(stub->name(), 0) == InputOutputMap::invalidUniverse());
    QVERIFY(im.inputMapping(stub->name(), 1) == InputOutputMap::invalidUniverse());
    QVERIFY(im.inputMapping(stub->name(), 2) == InputOutputMap::invalidUniverse());
    QVERIFY(im.inputMapping(stub->name(), 3) == InputOutputMap::invalidUniverse());
    QVERIFY(im.isUniversePatched(0) == false);
    QVERIFY(im.isUniversePatched(42) == false);

    QVERIFY(im.setInputPatch(0, "Foobar", "", "", 0, prof->name()) == true);
    QVERIFY(im.inputPatch(0) == NULL);
    QVERIFY(im.inputMapping(stub->name(), 0) == InputOutputMap::invalidUniverse());

    QVERIFY(im.inputPatch(1) == NULL);
    QVERIFY(im.inputMapping(stub->name(), 1) == InputOutputMap::invalidUniverse());

    QVERIFY(im.inputPatch(2) == NULL);
    QVERIFY(im.inputMapping(stub->name(), 2) == InputOutputMap::invalidUniverse());

    QVERIFY(im.inputPatch(3) == NULL);
    QVERIFY(im.inputMapping(stub->name(), 3) == InputOutputMap::invalidUniverse());

    QVERIFY(im.setInputPatch(0, stub->name(), "", stub->inputs().at(0), 0) == true);
    QVERIFY(im.inputPatch(0)->plugin() == stub);
    QVERIFY(im.inputPatch(0)->input() == 0);
    QVERIFY(im.inputPatch(0)->profile() == NULL);
    QVERIFY(im.inputMapping(stub->name(), 0) == 0);
    QVERIFY(im.isUniversePatched(0) == true);

    QVERIFY(im.inputPatch(1) == NULL);
    QVERIFY(im.inputMapping(stub->name(), 1) == InputOutputMap::invalidUniverse());

    QVERIFY(im.inputPatch(2) == NULL);
    QVERIFY(im.inputMapping(stub->name(), 2) == InputOutputMap::invalidUniverse());

    QVERIFY(im.inputPatch(3) == NULL);
    QVERIFY(im.inputMapping(stub->name(), 3) == InputOutputMap::invalidUniverse());

    QVERIFY(im.setInputPatch(2, stub->name(), "", stub->inputs().at(3), 3, prof->name()) == true);
    QVERIFY(im.inputPatch(0)->plugin() == stub);
    QVERIFY(im.inputPatch(0)->input() == 0);
    QVERIFY(im.inputPatch(0)->profile() == NULL);
    QVERIFY(im.inputMapping(stub->name(), 0) == 0);

    QVERIFY(im.inputPatch(1) == NULL);
    QVERIFY(im.inputMapping(stub->name(), 1) == InputOutputMap::invalidUniverse());

    QVERIFY(im.inputPatch(2)->plugin() == stub);
    QVERIFY(im.inputPatch(2)->input() == 3);
    QVERIFY(im.inputPatch(2)->profile() == prof);
    QVERIFY(im.inputMapping(stub->name(), 2) == InputOutputMap::invalidUniverse());

    QVERIFY(im.inputPatch(3) == NULL);
    QVERIFY(im.inputMapping(stub->name(), 3) == 2);

    // Universe out of bounds
    QVERIFY(im.setInputPatch(im.universesCount(), stub->name(), "", stub->inputs().at(0), 0) == false);
}


void InputOutputMap_Test::removeInputPatch_data()
{
    QTest::addColumn<QString>("via");
    QTest::addColumn<bool>("startPatched");
    QTest::addColumn<QString>("pluginName");
    QTest::addColumn<quint32>("line");
    QTest::addColumn<bool>("expectedResult");
    QTest::addColumn<quint32>("expectedLine");
    QTest::addColumn<int>("expectedCloses");
    QTest::addColumn<bool>("expectWarning");

    const quint32 none = QLCIOPlugin::invalidLine();

    QTest::newRow("None line 0 removes")           << "map" << true  << KInputNone      << quint32(0) << true  << none        << 1 << false;
    QTest::newRow("None invalidLine removes")      << "map" << true  << KInputNone      << none       << true  << none        << 1 << false;
    QTest::newRow("empty name line 0 removes")     << "map" << true  << QString()       << quint32(0) << true  << none        << 1 << false;
    QTest::newRow("unknown invalidLine removes")   << "map" << true  << "NoSuchPlugin"  << none       << true  << none        << 1 << false;
    QTest::newRow("unknown line 0 keeps patch")    << "map" << true  << "NoSuchPlugin"  << quint32(0) << false << quint32(0)  << 0 << true;
    QTest::newRow("valid repatch moves line")      << "map" << true  << "<stub>"        << quint32(3) << true  << quint32(3)  << 1 << false;
    QTest::newRow("None line 0 when unpatched")    << "map" << false << KInputNone      << quint32(0) << true  << none        << 0 << false;
    QTest::newRow("unknown line 0 when unpatched") << "map" << false << "NoSuchPlugin"  << quint32(0) << true  << none        << 0 << true;
    QTest::newRow("Universe NULL line 0 removes")  << "universe" << true << QString()   << quint32(0) << true  << none        << 1 << false;
    QTest::newRow("Universe NULL invalidLine removes") << "universe" << true << QString() << none     << true  << none        << 1 << false;
    QTest::newRow("Universe NULL when unpatched")  << "universe" << false << QString()  << quint32(0) << true  << none        << 0 << false;
}

void InputOutputMap_Test::removeInputPatch()
{
    QFETCH(QString, via);
    QFETCH(bool, startPatched);
    QFETCH(QString, pluginName);
    QFETCH(quint32, line);
    QFETCH(bool, expectedResult);
    QFETCH(quint32, expectedLine);
    QFETCH(int, expectedCloses);
    QFETCH(bool, expectWarning);

    IOPluginStub* stub = static_cast<IOPluginStub*>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);
    if (pluginName == "<stub>")
        pluginName = stub->name();

    InputOutputMap im(m_doc, 4);
    stub->m_openInputs.clear();
    QVERIFY(im.setInputPatch(1, stub->name(), "", "", 1) == true);
    QVERIFY(im.setInputPatch(2, stub->name(), "", "", 2) == true);
    QVERIFY(im.setOutputPatch(0, stub->name(), "", "", 0) == true);
    if (startPatched)
        QVERIFY(im.setInputPatch(0, stub->name(), "", "", 0) == true);
    InputPatch *other1 = im.inputPatch(1);
    InputPatch *other2 = im.inputPatch(2);
    OutputPatch *output0 = im.outputPatch(0);
    stub->m_closedInputs.clear();

    if (expectWarning)
        QTest::ignoreMessage(QtWarningMsg,
                             QRegularExpression(QString("Unknown input plugin.*%1").arg(pluginName)));

    bool result;
    if (via == "universe")
        result = im.universe(0)->setInputPatch(NULL, line, NULL);
    else
        result = im.setInputPatch(0, pluginName, "", "", line);

    QCOMPARE(result, expectedResult);
    for (quint32 i = 0; i < im.universesCount(); i++)
        QVERIFY2(im.inputPatch(i) == NULL || im.inputPatch(i)->plugin() != NULL,
                 qPrintable(QString("universe %1 kept an input patch without a plugin").arg(i)));

    if (expectedLine == QLCIOPlugin::invalidLine())
    {
        QVERIFY(im.inputPatch(0) == NULL);
        QVERIFY(stub->m_openInputs.contains(0) == false);
    }
    else
    {
        QVERIFY(im.inputPatch(0) != NULL);
        QCOMPARE(im.inputPatch(0)->plugin(), stub);
        QCOMPARE(im.inputPatch(0)->input(), expectedLine);
        QVERIFY(stub->m_openInputs.contains(expectedLine));

        QSignalSpy spy(&im, SIGNAL(inputValueChanged(quint32, quint32, uchar, const QString&)));
        stub->emitValueChanged(UINT_MAX, expectedLine, 7, 200);
        im.flushInputs();
        QCOMPARE(spy.size(), 1);
        QCOMPARE(spy.at(0).at(0).toUInt(), quint32(0));
    }
    QCOMPARE(stub->m_closedInputs.count(), expectedCloses);
    if (expectedCloses > 0)
        QCOMPARE(stub->m_closedInputs.first(), quint32(0));

    QCOMPARE(im.inputPatch(1), other1);
    QCOMPARE(im.inputPatch(1)->plugin(), stub);
    QCOMPARE(im.inputPatch(1)->input(), quint32(1));
    QCOMPARE(im.inputPatch(2), other2);
    QCOMPARE(im.inputPatch(2)->input(), quint32(2));
    QVERIFY(stub->m_openInputs.contains(1) && stub->m_openInputs.contains(2));
    QCOMPARE(im.outputPatch(0), output0);
    QCOMPARE(im.outputPatch(0)->plugin(), stub);

    // Re-patching the same universe afterwards must work
    QVERIFY(im.setInputPatch(0, stub->name(), "", "", 0) == true);
    QCOMPARE(im.inputPatch(0)->plugin(), stub);
    QCOMPARE(im.inputPatch(0)->input(), quint32(0));
}

void InputOutputMap_Test::loadXMLInputPatch_data()
{
    QTest::addColumn<QString>("inputElement");
    QTest::addColumn<quint32>("expectedLine");
    QTest::addColumn<bool>("expectWarning");

    const quint32 none = QLCIOPlugin::invalidLine();

    QTest::newRow("None line 0")        << R"(<Input Plugin="None" Line="0"/>)"          << none       << false;
    QTest::newRow("no plugin attribute") << R"(<Input Line="0"/>)"                        << none       << false;
    QTest::newRow("missing plugin")     << R"(<Input Plugin="NoSuchPlugin" Line="0"/>)"  << none       << true;
    QTest::newRow("stub line 2")        << R"(<Input Plugin="<stub>" Line="2"/>)"        << quint32(2) << false;
    QTest::newRow("no input")           << QString()                                     << none       << false;
}

void InputOutputMap_Test::loadXMLInputPatch()
{
    QFETCH(QString, inputElement);
    QFETCH(quint32, expectedLine);
    QFETCH(bool, expectWarning);

    IOPluginStub* stub = static_cast<IOPluginStub*>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);
    inputElement.replace("<stub>", stub->name());

    QString xml = QString(R"(<InputOutputMap><Universe Name="U1" ID="0">%1</Universe>)"
                          R"(<Universe Name="U2" ID="1"><Input Plugin="%2" Line="1"/></Universe>)"
                          R"(</InputOutputMap>)").arg(inputElement, stub->name());
    QXmlStreamReader root(xml);
    root.readNextStartElement();

    if (expectWarning)
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Unknown input plugin.*NoSuchPlugin"));

    InputOutputMap im(m_doc, 4);
    QVERIFY(im.loadXML(root) == true);
    QCOMPARE(im.universesCount(), quint32(2));

    if (expectedLine == QLCIOPlugin::invalidLine())
        QVERIFY(im.inputPatch(0) == NULL);
    else
    {
        QVERIFY(im.inputPatch(0) != NULL);
        QCOMPARE(im.inputPatch(0)->plugin(), stub);
        QCOMPARE(im.inputPatch(0)->input(), expectedLine);
    }
    QVERIFY(im.inputPatch(1) != NULL);
    QCOMPARE(im.inputPatch(1)->plugin(), stub);
    QCOMPARE(im.inputPatch(1)->input(), quint32(1));
}

void InputOutputMap_Test::setOutputPatch()
{
    InputOutputMap iom(m_doc, 4);

    IOPluginStub* stub = static_cast<IOPluginStub*>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);

    QVERIFY(iom.setOutputPatch(0, "Foobar", "", "", 0) == false);
    QVERIFY(iom.outputPatch(0) == NULL);
    QVERIFY(iom.outputPatch(1) == NULL);
    QVERIFY(iom.outputPatch(2) == NULL);
    QVERIFY(iom.outputPatch(3) == NULL);

    QVERIFY(iom.setOutputPatch(4, stub->name(), "", "", 0) == false);
    QVERIFY(iom.outputPatch(0) == NULL);
    QVERIFY(iom.outputPatch(1) == NULL);
    QVERIFY(iom.outputPatch(2) == NULL);
    QVERIFY(iom.outputPatch(3) == NULL);

    QVERIFY(iom.setOutputPatch(4, stub->name(), "", "", 4) == false);
    QVERIFY(iom.outputPatch(0) == NULL);
    QVERIFY(iom.outputPatch(1) == NULL);
    QVERIFY(iom.outputPatch(2) == NULL);
    QVERIFY(iom.outputPatch(3) == NULL);

    QVERIFY(iom.setOutputPatch(3, stub->name(), "", stub->outputs().at(0), 0) == true);
    QVERIFY(iom.outputPatch(3)->plugin() == stub);
    QVERIFY(iom.outputPatch(3)->output() == 0);

    QVERIFY(iom.setOutputPatch(2, stub->name(), "", stub->outputs().at(1), 1) == true);
    QVERIFY(iom.outputPatch(2)->plugin() == stub);
    QVERIFY(iom.outputPatch(2)->output() == 1);

    QVERIFY(iom.setOutputPatch(1, stub->name(), "", stub->outputs().at(2), 2) == true);
    QVERIFY(iom.outputPatch(1)->plugin() == stub);
    QVERIFY(iom.outputPatch(1)->output() == 2);

    QVERIFY(iom.setOutputPatch(0, stub->name(), "", stub->outputs().at(3), 3) == true);
    QVERIFY(iom.outputPatch(0)->plugin() == stub);
    QVERIFY(iom.outputPatch(0)->output() == 3);

    QVERIFY(iom.outputMapping("Foo", 42) == QLCIOPlugin::invalidLine());
    QVERIFY(iom.outputMapping(stub->name(), 0) == 3);

    QVERIFY(iom.feedbackPatch(42) == NULL);
    QVERIFY(iom.feedbackPatch(0) == NULL);
}

void InputOutputMap_Test::setMultipleOutputPatches()
{
    InputOutputMap iom(m_doc, 4);

    IOPluginStub* stub = static_cast<IOPluginStub*>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);

    // add an output patch
    QVERIFY(iom.setOutputPatch(1, stub->name(), "", "", 0, false, 0) == true);
    QVERIFY(iom.outputPatchesCount(1) == 1);
    QVERIFY(iom.outputPatch(1, 0)->plugin() == stub);
    QVERIFY(iom.outputPatch(1, 0)->output() == 0);

    // add another output patch
    QVERIFY(iom.setOutputPatch(1, stub->name(), "", "", 0, false, 1) == true);
    QVERIFY(iom.outputPatchesCount(1) == 2);
    QVERIFY(iom.outputPatch(1, 1)->plugin() == stub);
    QVERIFY(iom.outputPatch(1, 1)->output() == 0);

    // remove the first output patch
    QVERIFY(iom.setOutputPatch(1, stub->name(), "", "", QLCIOPlugin::invalidLine(), false, 0) == true);
    QVERIFY(iom.outputPatchesCount(1) == 1);
    QVERIFY(iom.outputPatch(1, 0)->plugin() == stub);
    QVERIFY(iom.outputPatch(1, 0)->output() == 0);
    QVERIFY(iom.outputPatch(1, 1) == NULL);

    // remove the first output patch again
    QVERIFY(iom.setOutputPatch(1, stub->name(), "", "", QLCIOPlugin::invalidLine(), false, 0) == true);
    QVERIFY(iom.outputPatchesCount(1) == 0);
}

void InputOutputMap_Test::slotValueChanged()
{
    InputOutputMap im(m_doc, 4);

    IOPluginStub* stub = static_cast<IOPluginStub*>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);

    QVERIFY(im.setInputPatch(0, stub->name(), "", stub->inputs().at(0), 0) == true);
    QVERIFY(im.inputPatch(0)->plugin() == stub);
    QVERIFY(im.inputPatch(0)->input() == 0);

    QSignalSpy spy(&im, SIGNAL(inputValueChanged(quint32, quint32, uchar, const QString&)));
    stub->emitValueChanged(UINT_MAX, 0, 15, UCHAR_MAX);
    QVERIFY(spy.size() == 0);
    im.flushInputs();
    QVERIFY(spy.size() == 1);
    QVERIFY(spy.at(0).at(0) == 0);
    QVERIFY(spy.at(0).at(1) == 15);
    QVERIFY(spy.at(0).at(2) == UCHAR_MAX);

    /* Invalid mapping for this plugin -> no signal */
    stub->emitValueChanged(UINT_MAX, 3, 15, UCHAR_MAX);
    QVERIFY(spy.size() == 1);
    im.flushInputs();
    QVERIFY(spy.size() == 1);
    QVERIFY(spy.at(0).at(0) == 0);
    QVERIFY(spy.at(0).at(1) == 15);
    QVERIFY(spy.at(0).at(2) == UCHAR_MAX);

    /* Invalid mapping for this plugin -> no signal */
    stub->emitValueChanged(UINT_MAX, 1, 15, UCHAR_MAX);
    QVERIFY(spy.size() == 1);
    im.flushInputs();
    QVERIFY(spy.size() == 1);
    QVERIFY(spy.at(0).at(0) == 0);
    QVERIFY(spy.at(0).at(1) == 15);
    QVERIFY(spy.at(0).at(2) == UCHAR_MAX);

    stub->emitValueChanged(UINT_MAX, 0, 5, 127);
    QVERIFY(spy.size() == 1);
    im.flushInputs();
    QVERIFY(spy.size() == 2);
    QVERIFY(spy.at(0).at(0) == 0);
    QVERIFY(spy.at(0).at(1) == 15);
    QVERIFY(spy.at(0).at(2) == UCHAR_MAX);
    QVERIFY(spy.at(1).at(0) == 0);
    QVERIFY(spy.at(1).at(1) == 5);
    QVERIFY(spy.at(1).at(2) == 127);

    stub->emitValueChanged(UINT_MAX, 0, 2, 0);
    QVERIFY(spy.size() == 2);
    stub->emitValueChanged(UINT_MAX, 0, 2, UCHAR_MAX);
    QVERIFY(spy.size() == 3);
    QVERIFY(spy.at(0).at(0) == 0);
    QVERIFY(spy.at(0).at(1) == 15);
    QVERIFY(spy.at(0).at(2) == UCHAR_MAX);
    QVERIFY(spy.at(1).at(0) == 0);
    QVERIFY(spy.at(1).at(1) == 5);
    QVERIFY(spy.at(1).at(2) == 127);
    QVERIFY(spy.at(2).at(0) == 0);
    QVERIFY(spy.at(2).at(1) == 2);
    QVERIFY(spy.at(2).at(2) == 0);
    im.flushInputs();
    QVERIFY(spy.size() == 4);
    QVERIFY(spy.at(0).at(0) == 0);
    QVERIFY(spy.at(0).at(1) == 15);
    QVERIFY(spy.at(0).at(2) == UCHAR_MAX);
    QVERIFY(spy.at(1).at(0) == 0);
    QVERIFY(spy.at(1).at(1) == 5);
    QVERIFY(spy.at(1).at(2) == 127);
    QVERIFY(spy.at(2).at(0) == 0);
    QVERIFY(spy.at(2).at(1) == 2);
    QVERIFY(spy.at(2).at(2) == 0);
    QVERIFY(spy.at(3).at(0) == 0);
    QVERIFY(spy.at(3).at(1) == 2);
    QVERIFY(spy.at(3).at(2) == UCHAR_MAX);
}

void InputOutputMap_Test::slotConfigurationChanged()
{
    InputOutputMap im(m_doc, 4);

    IOPluginStub* stub = static_cast<IOPluginStub*>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);

    QSignalSpy spy(&im, SIGNAL(pluginConfigurationChanged(QString, bool)));
    stub->configure();
    QCOMPARE(spy.size(), 1);
    QCOMPARE(spy.at(0).size(), 2);
    QCOMPARE(spy.at(0).at(0).toString(), QString(stub->name()));
}

void InputOutputMap_Test::loadInputProfiles()
{
    InputOutputMap im(m_doc, 4);

    // No profiles in a nonexistent directory
    QDir dir("/path/to/a/nonexistent/place/beyond/this/universe");
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtInputProfile));
    im.loadProfiles(dir);
    QVERIFY(im.profileNames().isEmpty() == true);

    // No profiles in an existing directory
    dir = testPluginDir();
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtInputProfile));
    im.loadProfiles(dir);
    QVERIFY(im.profileNames().isEmpty() == true);

    // Should be able to load profiles
    dir.setPath(INTERNAL_PROFILEDIR);
    im.loadProfiles(dir);
    QStringList names(im.profileNames());
    QVERIFY(names.size() > 0);

    // Shouldn't load duplicates
    im.loadProfiles(dir);
    QCOMPARE(names, im.profileNames());
}

void InputOutputMap_Test::inputSourceNames()
{
    InputOutputMap im(m_doc, 4);

    IOPluginStub* stub = static_cast<IOPluginStub*> (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);

    QDir dir(INTERNAL_PROFILEDIR);
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtInputProfile));
    im.loadProfiles(dir);

    // Allow unpatched universe
    QString uni, ch;
    QVERIFY(im.inputSourceNames(new QLCInputSource(0, 0), uni, ch) == true);
    QCOMPARE(uni, QString("%1 -UNPATCHED-").arg(1));
    QCOMPARE(ch, QString("%1: ?").arg(1));

    // Don't allow unexisting universe
    QVERIFY(im.inputSourceNames(new QLCInputSource(100, 0), uni, ch) == false);

    QVERIFY(im.setInputPatch(0, stub->name(), "", stub->inputs().at(0), 0, QString("Generic MIDI")) == true);
    QVERIFY(im.inputSourceNames(new QLCInputSource(0, 0), uni, ch) == true);
    QCOMPARE(uni, tr("%1: Generic MIDI").arg(1));
    QCOMPARE(ch, tr("%1: Bank select MSB").arg(1));

    uni.clear();
    ch.clear();
    QVERIFY(im.inputSourceNames(new QLCInputSource(0, 50000), uni, ch) == true);
    QCOMPARE(uni, tr("%1: Generic MIDI").arg(1));
    QCOMPARE(ch, tr("%1: ?").arg(50001));

    QVERIFY(im.setInputPatch(0, stub->name(), "", stub->inputs().at(0), 0, QString()) == true);

    uni.clear();
    ch.clear();
    QVERIFY(im.inputSourceNames(new QLCInputSource(0, 0), uni, ch) == true);
    QCOMPARE(uni, tr("%1: %2").arg(1).arg(stub->name()));
    QCOMPARE(ch, tr("%1: ?").arg(1));

    QVERIFY(im.inputSourceNames(new QLCInputSource(0, QLCInputSource::invalidChannel), uni, ch) == false);
    QVERIFY(im.inputSourceNames(new QLCInputSource(InputOutputMap::invalidUniverse(), 0), uni, ch) == false);
    QVERIFY(im.inputSourceNames(new QLCInputSource(), uni, ch) == false);
}

void InputOutputMap_Test::profileDirectories()
{
    QDir dir = InputOutputMap::systemProfileDirectory();
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    QDir ipDir(INPUTPROFILEDIR);
#else
    QDir ipDir(QCoreApplication::applicationDirPath() + "/../" INPUTPROFILEDIR);
#endif
    QVERIFY(dir.filter() & QDir::Files);
    QVERIFY(dir.nameFilters().contains(QString("*%1").arg(KExtInputProfile)));
    QCOMPARE(dir.absolutePath(), ipDir.absolutePath());

    dir = InputOutputMap::userProfileDirectory();
#ifndef SKIP_TEST
    QVERIFY(dir.exists() == true);
#endif
    QVERIFY(dir.filter() & QDir::Files);
    QVERIFY(dir.nameFilters().contains(QString("*%1").arg(KExtInputProfile)));
    QVERIFY(dir.absolutePath().contains(USERINPUTPROFILEDIR));
}

void InputOutputMap_Test::claimReleaseDumpReset()
{
    InputOutputMap iom(m_doc, 4);

    IOPluginStub* stub = static_cast<IOPluginStub*>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);

    iom.setOutputPatch(0, stub->name(), "", stub->outputs().at(0), 0);
    iom.setOutputPatch(1, stub->name(), "", stub->outputs().at(1), 1);
    iom.setOutputPatch(2, stub->name(), "", stub->outputs().at(2), 2);
    iom.setOutputPatch(3, stub->name(), "", stub->outputs().at(3), 3);

    QList<Universe*> unis = iom.claimUniverses();
    for (int i = 0; i < 512; i++)
        unis[0]->write(i, 'a');
    for (int i = 0; i < 512; i++)
        unis[1]->write(i, 'b');
    for (int i = 0; i < 512; i++)
        unis[2]->write(i, 'c');
    for (int i = 0; i < 512; i++)
        unis[3]->write(i, 'd');
    iom.releaseUniverses();

    foreach (Universe *universe, unis)
    {
        const QByteArray postGM = universe->postGMValues()->mid(0, universe->usedChannels());
        universe->dumpOutput(postGM, true);
    }

    for (int i = 0; i < 512; i++)
        QCOMPARE(stub->m_universe.data()[i], 'a');

    for (int i = 512; i < 1024; i++)
        QCOMPARE(stub->m_universe.data()[i], 'b');

    for (int i = 1024; i < 1536; i++)
        QCOMPARE(stub->m_universe.data()[i], 'c');

    for (int i = 1536; i < 2048; i++)
        QCOMPARE(stub->m_universe.data()[i], 'd');

    iom.resetUniverses();
    for (int u = 0; u < iom.m_universeArray.size(); u++)
    {
        for (quint32 i = 0; i < 512; i++)
            QVERIFY(iom.m_universeArray.at(u)->preGMValues().data()[i] == 0);
    }
}

void InputOutputMap_Test::blackout()
{
    InputOutputMap iom(m_doc, 4);

    IOPluginStub* stub = static_cast<IOPluginStub*>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);

    iom.setOutputPatch(0, stub->name(), "", stub->outputs().at(0), 0);
    iom.setOutputPatch(1, stub->name(), "", stub->outputs().at(1), 1);
    iom.setOutputPatch(2, stub->name(), "", stub->outputs().at(2), 2);
    iom.setOutputPatch(3, stub->name(), "", stub->outputs().at(3), 3);

    QList<Universe*> unis = iom.claimUniverses();
    unis[0]->setChannelCapability(42, QLCChannel::Intensity);
    unis[1]->setChannelCapability(42, QLCChannel::Intensity);
    unis[2]->setChannelCapability(42, QLCChannel::Intensity);
    unis[3]->setChannelCapability(42, QLCChannel::Intensity);

    for (int i = 0; i < 512; i++)
        unis[0]->write(i, 'a');
    for (int i = 0; i < 512; i++)
        unis[1]->write(i, 'b');
    for (int i = 0; i < 512; i++)
        unis[2]->write(i, 'c');
    for (int i = 0; i < 512; i++)
        unis[3]->write(i, 'd');
    iom.releaseUniverses();

    foreach (Universe *universe, unis)
    {
        const QByteArray postGM = universe->postGMValues()->mid(0, universe->usedChannels());
        universe->dumpOutput(postGM, true);
    }

    iom.setBlackout(true);
    QVERIFY(iom.blackout() == true);

    foreach (Universe *universe, unis)
    {
        const QByteArray postGM = universe->postGMValues()->mid(0, universe->usedChannels());
        universe->dumpOutput(postGM, true);
    }

    int offset = 0;

    for (int u = 0; u < 4; u++)
    {
        for (int i = 0; i < 512; i++)
        {
            if (i == 42)
                QVERIFY(stub->m_universe[offset + i] == (char) 0);
            else if (u == 0)
                QVERIFY(stub->m_universe[offset + i] == (char) 'a');
            else if (u == 1)
                QVERIFY(stub->m_universe[offset + i] == (char) 'b');
            else if (u == 2)
                QVERIFY(stub->m_universe[offset + i] == (char) 'c');
            else if (u == 3)
                QVERIFY(stub->m_universe[offset + i] == (char) 'd');
        }

        offset += 512;
    }

    iom.setBlackout(true);
    QVERIFY(iom.blackout() == true);

    foreach (Universe *universe, unis)
    {
        const QByteArray postGM = universe->postGMValues()->mid(0, universe->usedChannels());
        universe->dumpOutput(postGM, true);
    }

    offset = 0;

    for (int u = 0; u < 4; u++)
    {
        for (int i = 0; i < 512; i++)
        {
            if (i == 42)
                QVERIFY(stub->m_universe[offset + i] == (char) 0);
            else if (u == 0)
                QVERIFY(stub->m_universe[offset + i] == (char) 'a');
            else if (u == 1)
                QVERIFY(stub->m_universe[offset + i] == (char) 'b');
            else if (u == 2)
                QVERIFY(stub->m_universe[offset + i] == (char) 'c');
            else if (u == 3)
                QVERIFY(stub->m_universe[offset + i] == (char) 'd');
        }

        offset += 512;
    }

    iom.toggleBlackout();
    QVERIFY(iom.blackout() == false);

    foreach (Universe *universe, unis)
    {
        const QByteArray postGM = universe->postGMValues()->mid(0, universe->usedChannels());
        universe->dumpOutput(postGM, true);
    }

    for (int i = 0; i < 512; i++)
        QVERIFY(stub->m_universe[i] == 'a');
    for (int i = 512; i < 1024; i++)
        QVERIFY(stub->m_universe[i] == 'b');
    for (int i = 1024; i < 1536; i++)
        QVERIFY(stub->m_universe[i] == 'c');
    for (int i = 1536; i < 2048; i++)
        QVERIFY(stub->m_universe[i] == 'd');

    iom.setBlackout(false);
    QVERIFY(iom.blackout() == false);

    foreach (Universe *universe, unis)
    {
        const QByteArray postGM = universe->postGMValues()->mid(0, universe->usedChannels());
        universe->dumpOutput(postGM, true);
    }

    for (int i = 0; i < 512; i++)
        QVERIFY(stub->m_universe[i] == 'a');
    for (int i = 512; i < 1024; i++)
        QVERIFY(stub->m_universe[i] == 'b');
    for (int i = 1024; i < 1536; i++)
        QVERIFY(stub->m_universe[i] == 'c');
    for (int i = 1536; i < 2048; i++)
        QVERIFY(stub->m_universe[i] == 'd');

    iom.toggleBlackout();
    QVERIFY(iom.blackout() == true);

    foreach (Universe *universe, unis)
    {
        const QByteArray postGM = universe->postGMValues()->mid(0, universe->usedChannels());
        universe->dumpOutput(postGM, true);
    }

    offset = 0;

    for (int u = 0; u < 4; u++)
    {
        for (int i = 0; i < 512; i++)
        {
            if (i == 42)
                QVERIFY(stub->m_universe[offset + i] == (char) 0);
            else if (u == 0)
                QVERIFY(stub->m_universe[offset + i] == (char) 'a');
            else if (u == 1)
                QVERIFY(stub->m_universe[offset + i] == (char) 'b');
            else if (u == 2)
                QVERIFY(stub->m_universe[offset + i] == (char) 'c');
            else if (u == 3)
                QVERIFY(stub->m_universe[offset + i] == (char) 'd');
        }

        offset += 512;
    }
}

/****************************************************************************
 * Freeze: hold the lighting output of every universe
 ****************************************************************************/

/** Publish one universe exactly the way the worker thread does after a tick */
static void dumpUniverse(Universe *universe, bool dataChanged = true)
{
    const QByteArray postGM = universe->postGMValues()->mid(0, universe->usedChannels());
    universe->dumpOutput(postGM, dataChanged);
}

/** Patch two universes to two stub output lines and give them a nonuniform
 *  mix of HTP intensity and LTP channels, so held values cannot be confused
 *  with a uniformly filled buffer. */
static void patchTwoUniverses(InputOutputMap &iom, IOPluginStub *stub)
{
    iom.setOutputPatch(0, stub->name(), "", stub->outputs().at(0), 0);
    iom.setOutputPatch(1, stub->name(), "", stub->outputs().at(1), 1);

    QList<Universe *> unis = iom.claimUniverses();
    unis[0]->setChannelCapability(10, QLCChannel::Intensity);
    unis[0]->setChannelCapability(11, QLCChannel::Pan);
    unis[0]->setChannelCapability(12, QLCChannel::Intensity);
    unis[1]->setChannelCapability(20, QLCChannel::Intensity);
    unis[1]->setChannelCapability(21, QLCChannel::Colour);
    iom.releaseUniverses();
}

/** Write a complete look and publish it. HTP channels are cleared first,
 *  the way processFaders() does at the start of every tick. */
static void writeLook(QList<Universe *> unis, uchar u0ch10, uchar u0ch11,
                      uchar u0ch12, uchar u1ch20, uchar u1ch21)
{
    unis[0]->zeroIntensityChannels();
    unis[1]->zeroIntensityChannels();
    unis[0]->write(10, u0ch10);
    unis[0]->write(11, u0ch11);
    unis[0]->write(12, u0ch12);
    unis[1]->write(20, u1ch20);
    unis[1]->write(21, u1ch21);
}

void InputOutputMap_Test::freezeHoldsLastSubmittedLook()
{
    InputOutputMap iom(m_doc, 2);

    IOPluginStub *stub = static_cast<IOPluginStub *>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);
    stub->m_universe.fill(char(0));

    patchTwoUniverses(iom, stub);
    QList<Universe *> unis = iom.universes();

    QVERIFY(iom.isFrozen() == false);

    /* Look A reaches the plugin */
    writeLook(unis, 200, 77, 13, 90, 33);
    dumpUniverse(unis[0]);
    dumpUniverse(unis[1]);

    QCOMPARE(uchar(stub->m_universe[10]), uchar(200));
    QCOMPARE(uchar(stub->m_universe[11]), uchar(77));
    QCOMPARE(uchar(stub->m_universe[12]), uchar(13));
    QCOMPARE(uchar(stub->m_universe[512 + 20]), uchar(90));
    QCOMPARE(uchar(stub->m_universe[512 + 21]), uchar(33));

    QSignalSpy frozenSpy(&iom, SIGNAL(frozenChanged(bool)));
    iom.setFrozen(true);
    QVERIFY(iom.isFrozen() == true);
    QCOMPARE(frozenSpy.count(), 1);
    QCOMPARE(frozenSpy.at(0).at(0).toBool(), true);

    /* Look B: the producers keep running, the output must not follow */
    writeLook(unis, 5, 1, 255, 250, 240);
    dumpUniverse(unis[0]);
    dumpUniverse(unis[1]);

    QCOMPARE(uchar(stub->m_universe[10]), uchar(200));
    QCOMPARE(uchar(stub->m_universe[11]), uchar(77));
    QCOMPARE(uchar(stub->m_universe[12]), uchar(13));
    QCOMPARE(uchar(stub->m_universe[512 + 20]), uchar(90));
    QCOMPARE(uchar(stub->m_universe[512 + 21]), uchar(33));

    /* A repeated enable request must not re-capture the newer look */
    iom.setFrozen(true);
    QCOMPARE(frozenSpy.count(), 1);
    dumpUniverse(unis[0]);
    dumpUniverse(unis[1]);

    QCOMPARE(uchar(stub->m_universe[10]), uchar(200));
    QCOMPARE(uchar(stub->m_universe[12]), uchar(13));
    QCOMPARE(uchar(stub->m_universe[512 + 20]), uchar(90));
}

/* Expected bytes below are worked out by hand from the Grand Master rules,
   not recomputed the way the engine does:
   Reduce scales by value/255 and rounds with floor(v * f + 0.5),
   Limit clamps to the Grand Master value, and the Intensity channel mode
   leaves LTP channels alone. */
/* Rubber-duck trap 2: the snapshot has to be the frame the receivers really
   saw, not whatever the producers composed after it. */
/* C1-1: a while-pressed control holds the look for as long as it is held */
void InputOutputMap_Test::freezeMomentaryHoldsAndReleases()
{
    InputOutputMap iom(m_doc, 2);

    IOPluginStub *stub = static_cast<IOPluginStub *>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);
    stub->m_universe.fill(char(0));

    patchTwoUniverses(iom, stub);
    QList<Universe *> unis = iom.universes();

    writeLook(unis, 200, 77, 13, 90, 33);
    dumpUniverse(unis[0]);
    QCOMPARE(uchar(stub->m_universe[10]), uchar(200));

    QSignalSpy aggregateSpy(&iom, SIGNAL(frozenChanged(bool)));
    QSignalSpy momentarySpy(&iom, SIGNAL(frozenMomentaryChanged(bool)));
    QSignalSpy latchSpy(&iom, SIGNAL(frozenLatchChanged(bool)));

    iom.setFrozenMomentary(true);
    QVERIFY(iom.isFrozen() == true);
    QVERIFY(iom.frozenMomentary() == true);
    QVERIFY(iom.frozenLatch() == false);
    QCOMPARE(aggregateSpy.count(), 1);
    QCOMPARE(momentarySpy.count(), 1);
    QCOMPARE(latchSpy.count(), 0);

    writeLook(unis, 5, 1, 255, 250, 240);
    dumpUniverse(unis[0]);
    QCOMPARE(uchar(stub->m_universe[10]), uchar(200));

    iom.setFrozenMomentary(false);
    QVERIFY(iom.isFrozen() == false);
    QVERIFY(iom.frozenMomentary() == false);
    QCOMPARE(aggregateSpy.count(), 2);
    QCOMPARE(momentarySpy.count(), 2);
    QCOMPARE(latchSpy.count(), 0);

    dumpUniverse(unis[0]);
    QCOMPARE(uchar(stub->m_universe[10]), uchar(5));
}

/* C1-2: any release clears the shared momentary flag, never the latch, and
   the latch keeps the original snapshot rather than capturing a new one */
void InputOutputMap_Test::freezeMomentaryReleaseKeepsLatch()
{
    InputOutputMap iom(m_doc, 2);

    IOPluginStub *stub = static_cast<IOPluginStub *>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);
    stub->m_universe.fill(char(0));

    patchTwoUniverses(iom, stub);
    QList<Universe *> unis = iom.universes();

    writeLook(unis, 200, 77, 13, 90, 33);
    dumpUniverse(unis[0]);

    iom.setFrozen(true);
    QVERIFY(iom.frozenLatch() == true);
    QVERIFY(iom.frozenMomentary() == false);

    writeLook(unis, 150, 60, 20, 80, 30);
    dumpUniverse(unis[0]);
    QCOMPARE(uchar(stub->m_universe[10]), uchar(200));

    QSignalSpy aggregateSpy(&iom, SIGNAL(frozenChanged(bool)));

    /* A while-pressed control is used on top of the latch */
    iom.setFrozenMomentary(true);
    QCOMPARE(aggregateSpy.count(), 0);

    writeLook(unis, 100, 40, 30, 70, 20);
    dumpUniverse(unis[0]);
    QCOMPARE(uchar(stub->m_universe[10]), uchar(200));

    /* Releasing it must leave the latch, and the original snapshot, alone */
    iom.setFrozenMomentary(false);
    QVERIFY(iom.isFrozen() == true);
    QVERIFY(iom.frozenLatch() == true);
    QVERIFY(iom.frozenMomentary() == false);
    QCOMPARE(aggregateSpy.count(), 0);

    dumpUniverse(unis[0]);
    QCOMPARE(uchar(stub->m_universe[10]), uchar(200));
    QCOMPARE(uchar(stub->m_universe[11]), uchar(77));
    QCOMPARE(uchar(stub->m_universe[12]), uchar(13));

    iom.setFrozen(false);
    QCOMPARE(aggregateSpy.count(), 1);
    dumpUniverse(unis[0]);
    QCOMPARE(uchar(stub->m_universe[10]), uchar(100));
}

/* C0-4: pressing the latch while a momentary hold is active flips the latch
   only. The effective state does not change, so nothing is re-captured. */
void InputOutputMap_Test::freezeLatchPressWhileMomentaryDoesNotRecapture()
{
    InputOutputMap iom(m_doc, 2);

    IOPluginStub *stub = static_cast<IOPluginStub *>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);
    stub->m_universe.fill(char(0));

    patchTwoUniverses(iom, stub);
    QList<Universe *> unis = iom.universes();

    writeLook(unis, 200, 77, 13, 90, 33);
    dumpUniverse(unis[0]);

    iom.setFrozenMomentary(true);

    writeLook(unis, 150, 60, 20, 80, 30);
    dumpUniverse(unis[0]);
    QCOMPARE(uchar(stub->m_universe[10]), uchar(200));

    QSignalSpy aggregateSpy(&iom, SIGNAL(frozenChanged(bool)));
    QSignalSpy latchSpy(&iom, SIGNAL(frozenLatchChanged(bool)));

    iom.setFrozen(true);
    QVERIFY(iom.frozenLatch() == true);
    QVERIFY(iom.frozenMomentary() == true);
    QCOMPARE(latchSpy.count(), 1);
    QCOMPARE(latchSpy.at(0).at(0).toBool(), true);
    QCOMPARE(aggregateSpy.count(), 0);

    dumpUniverse(unis[0]);
    QCOMPARE(uchar(stub->m_universe[10]), uchar(200));

    iom.setFrozenMomentary(false);
    QVERIFY(iom.isFrozen() == true);
    QCOMPARE(aggregateSpy.count(), 0);

    dumpUniverse(unis[0]);
    QCOMPARE(uchar(stub->m_universe[10]), uchar(200));
}

/* Both components are runtime state and are cleared with the universes */
void InputOutputMap_Test::freezeResetClearsBothComponents()
{
    InputOutputMap iom(m_doc, 2);

    IOPluginStub *stub = static_cast<IOPluginStub *>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);
    stub->m_universe.fill(char(0));

    patchTwoUniverses(iom, stub);
    QList<Universe *> unis = iom.universes();

    writeLook(unis, 200, 77, 13, 90, 33);
    dumpUniverse(unis[0]);

    iom.setFrozen(true);
    iom.setFrozenMomentary(true);
    QVERIFY(iom.frozenLatch() == true);
    QVERIFY(iom.frozenMomentary() == true);

    stub->m_writeLog.clear();
    iom.resetUniverses();

    QVERIFY(iom.frozenLatch() == false);
    QVERIFY(iom.frozenMomentary() == false);
    QVERIFY(iom.isFrozen() == false);
    QCOMPARE(stub->m_writeLog.count(), 0);
}

void InputOutputMap_Test::freezeHoldsLastSubmittedNotUnsentFrame()
{
    InputOutputMap iom(m_doc, 2);

    IOPluginStub *stub = static_cast<IOPluginStub *>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);
    stub->m_universe.fill(char(0));

    patchTwoUniverses(iom, stub);
    QList<Universe *> unis = iom.universes();

    /* Look A is submitted to the plugin */
    writeLook(unis, 200, 77, 13, 90, 33);
    dumpUniverse(unis[0]);
    dumpUniverse(unis[1]);

    /* Look B is composed but never submitted */
    writeLook(unis, 5, 1, 255, 250, 240);

    iom.setFrozen(true);
    dumpUniverse(unis[0]);
    dumpUniverse(unis[1]);

    QCOMPARE(uchar(stub->m_universe[10]), uchar(200));
    QCOMPARE(uchar(stub->m_universe[11]), uchar(77));
    QCOMPARE(uchar(stub->m_universe[12]), uchar(13));
    QCOMPARE(uchar(stub->m_universe[512 + 20]), uchar(90));
    QCOMPARE(uchar(stub->m_universe[512 + 21]), uchar(33));
}

/* Rubber-duck trap 1: a patch paused while the Grand Master was fully down
   must still be able to reveal its look when the Grand Master comes back,
   which its published bytes alone could never do. */
void InputOutputMap_Test::freezeHoldsPausedSourceAcrossGrandMaster()
{
    InputOutputMap iom(m_doc, 2);

    IOPluginStub *stub = static_cast<IOPluginStub *>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);
    stub->m_universe.fill(char(0));

    patchTwoUniverses(iom, stub);
    iom.setOutputPatch(0, stub->name(), "", stub->outputs().at(2), 2, false, 1);

    QList<Universe *> unis = iom.universes();
    OutputPatch *pausedPatch = unis[0]->outputPatch(1);
    QVERIFY(pausedPatch != NULL);

    iom.setGrandMasterValueMode(GrandMaster::Reduce);
    iom.setGrandMasterChannelMode(GrandMaster::Intensity);
    iom.setGrandMasterValue(0);

    writeLook(unis, 200, 77, 13, 90, 33);
    dumpUniverse(unis[0]);
    QCOMPARE(uchar(stub->m_universe[10]), uchar(0));

    /* The pause buffer and its provenance are both captured on this dump */
    pausedPatch->setPaused(true);
    writeLook(unis, 150, 60, 20, 90, 33);
    dumpUniverse(unis[0]);

    /* The live patch moves on, the paused one must not follow */
    writeLook(unis, 100, 40, 30, 90, 33);
    dumpUniverse(unis[0]);

    iom.setFrozen(true);
    iom.setGrandMasterValue(255);
    dumpUniverse(unis[0]);

    QCOMPARE(uchar(stub->m_universe[10]), uchar(100));
    QCOMPARE(uchar(stub->m_universe[11]), uchar(40));
    QCOMPARE(uchar(stub->m_universe[1024 + 10]), uchar(150));
    QCOMPARE(uchar(stub->m_universe[1024 + 11]), uchar(60));

    /* Thaw returns the paused patch to its legacy buffer, captured at zero
       Grand Master. The level step is the documented consequence. */
    iom.setFrozen(false);
    dumpUniverse(unis[0]);

    QVERIFY(pausedPatch->paused() == true);
    QCOMPARE(uchar(stub->m_universe[10]), uchar(100));
    QCOMPARE(uchar(stub->m_universe[1024 + 10]), uchar(0));
    QCOMPARE(uchar(stub->m_universe[1024 + 11]), uchar(60));
}

/* Rubber-duck trap 1, second half: pausing while blacked out buffers
   blackout bytes, but the source behind them is still the look. */
void InputOutputMap_Test::freezePausedDuringBlackoutHoldsItsSource()
{
    InputOutputMap iom(m_doc, 2);

    IOPluginStub *stub = static_cast<IOPluginStub *>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);
    stub->m_universe.fill(char(0));

    patchTwoUniverses(iom, stub);
    iom.setOutputPatch(0, stub->name(), "", stub->outputs().at(2), 2, false, 1);

    QList<Universe *> unis = iom.universes();
    OutputPatch *pausedPatch = unis[0]->outputPatch(1);
    QVERIFY(pausedPatch != NULL);

    writeLook(unis, 200, 77, 13, 90, 33);
    dumpUniverse(unis[0]);

    iom.setBlackout(true);
    pausedPatch->setPaused(true);
    dumpUniverse(unis[0]);

    QCOMPARE(uchar(stub->m_universe[1024 + 10]), uchar(0));
    QCOMPARE(uchar(stub->m_universe[1024 + 11]), uchar(77));

    iom.setFrozen(true);

    /* Producers move on while hidden, then blackout is cleared */
    writeLook(unis, 5, 1, 255, 250, 240);
    dumpUniverse(unis[0]);
    iom.setBlackout(false);

    QCOMPARE(uchar(stub->m_universe[1024 + 10]), uchar(200));
    QCOMPARE(uchar(stub->m_universe[1024 + 11]), uchar(77));
    QCOMPARE(uchar(stub->m_universe[10]), uchar(200));
    QCOMPARE(uchar(stub->m_universe[11]), uchar(77));
}

/* Rubber-duck trap 3: switching AllChannels -> Intensity leaves the live
   post-GM buffer holding scaled non-intensity bytes. Freeze makes that
   visible, so thaw must recompose rather than step back to the stale value. */
void InputOutputMap_Test::freezeThawRecomposesAfterChannelModeChange()
{
    InputOutputMap iom(m_doc, 2);

    IOPluginStub *stub = static_cast<IOPluginStub *>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);
    stub->m_universe.fill(char(0));

    patchTwoUniverses(iom, stub);
    QList<Universe *> unis = iom.universes();

    iom.setGrandMasterValueMode(GrandMaster::Reduce);
    iom.setGrandMasterChannelMode(GrandMaster::AllChannels);
    iom.setGrandMasterValue(128);

    writeLook(unis, 200, 77, 13, 90, 33);
    dumpUniverse(unis[0]);

    QCOMPARE(uchar(stub->m_universe[10]), uchar(100));
    QCOMPARE(uchar(stub->m_universe[11]), uchar(39));

    iom.setFrozen(true);

    /* Intensity only: the held LTP channel must stop being scaled */
    iom.setGrandMasterChannelMode(GrandMaster::Intensity);
    dumpUniverse(unis[0]);

    QCOMPARE(uchar(stub->m_universe[10]), uchar(100));
    QCOMPARE(uchar(stub->m_universe[11]), uchar(77));

    /* Thaw must land on the same value, not on the stale scaled one */
    iom.setFrozen(false);
    dumpUniverse(unis[0]);

    QCOMPARE(uchar(stub->m_universe[10]), uchar(100));
    QCOMPARE(uchar(stub->m_universe[11]), uchar(77));
}

void InputOutputMap_Test::freezeAppliesLiveGrandMaster_data()
{
    QTest::addColumn<int>("captureGM");
    QTest::addColumn<int>("liveGM");
    QTest::addColumn<int>("valueMode");
    QTest::addColumn<int>("channelMode");
    QTest::addColumn<int>("expectedIntensity");   // captured 200
    QTest::addColumn<int>("expectedLTP");         // captured 77
    QTest::addColumn<int>("expectedLowIntensity");// captured 13

    QTest::newRow("reduce intensity, dimmed after capture")
            << 255 << 128 << int(GrandMaster::Reduce) << int(GrandMaster::Intensity)
            << 100 << 77 << 7;
    QTest::newRow("reduce all channels, dimmed after capture")
            << 255 << 128 << int(GrandMaster::Reduce) << int(GrandMaster::AllChannels)
            << 100 << 39 << 7;
    QTest::newRow("limit intensity")
            << 255 << 100 << int(GrandMaster::Limit) << int(GrandMaster::Intensity)
            << 100 << 77 << 13;
    QTest::newRow("limit all channels")
            << 255 << 50 << int(GrandMaster::Limit) << int(GrandMaster::AllChannels)
            << 50 << 50 << 13;
    QTest::newRow("captured at zero, raised to full")
            << 0 << 255 << int(GrandMaster::Reduce) << int(GrandMaster::Intensity)
            << 200 << 77 << 13;
    QTest::newRow("captured dimmed, raised to full")
            << 64 << 255 << int(GrandMaster::Reduce) << int(GrandMaster::Intensity)
            << 200 << 77 << 13;
    QTest::newRow("reduced to zero after capture")
            << 255 << 0 << int(GrandMaster::Reduce) << int(GrandMaster::Intensity)
            << 0 << 77 << 0;
}

void InputOutputMap_Test::freezeAppliesLiveGrandMaster()
{
    QFETCH(int, captureGM);
    QFETCH(int, liveGM);
    QFETCH(int, valueMode);
    QFETCH(int, channelMode);
    QFETCH(int, expectedIntensity);
    QFETCH(int, expectedLTP);
    QFETCH(int, expectedLowIntensity);

    InputOutputMap iom(m_doc, 2);

    IOPluginStub *stub = static_cast<IOPluginStub *>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);
    stub->m_universe.fill(char(0));

    patchTwoUniverses(iom, stub);
    QList<Universe *> unis = iom.universes();

    /* Capture the look under the Grand Master that was set at that time */
    iom.setGrandMasterValueMode(GrandMaster::Reduce);
    iom.setGrandMasterChannelMode(GrandMaster::Intensity);
    iom.setGrandMasterValue(uchar(captureGM));

    writeLook(unis, 200, 77, 13, 90, 33);
    dumpUniverse(unis[0]);
    dumpUniverse(unis[1]);

    if (captureGM == 0)
    {
        /* Proves the held source cannot be the published bytes: everything
           the Grand Master scaled away has to come back later */
        QCOMPARE(uchar(stub->m_universe[10]), uchar(0));
        QCOMPARE(uchar(stub->m_universe[12]), uchar(0));
    }

    iom.setFrozen(true);

    /* Producers keep running and the Grand Master keeps being operated */
    writeLook(unis, 5, 1, 255, 250, 240);
    iom.setGrandMasterValueMode(GrandMaster::ValueMode(valueMode));
    iom.setGrandMasterChannelMode(GrandMaster::ChannelMode(channelMode));
    iom.setGrandMasterValue(uchar(liveGM));

    dumpUniverse(unis[0]);
    dumpUniverse(unis[1]);

    QCOMPARE(uchar(stub->m_universe[10]), uchar(expectedIntensity));
    QCOMPARE(uchar(stub->m_universe[11]), uchar(expectedLTP));
    QCOMPARE(uchar(stub->m_universe[12]), uchar(expectedLowIntensity));

    /* A second universe holds its own look through the same Grand Master */
    QVERIFY(uchar(stub->m_universe[512 + 20]) != uchar(250));
    QVERIFY(uchar(stub->m_universe[512 + 21]) != uchar(240));
}

void InputOutputMap_Test::freezeBlackoutOverridesHeldLook()
{
    InputOutputMap iom(m_doc, 2);

    IOPluginStub *stub = static_cast<IOPluginStub *>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);
    stub->m_universe.fill(char(0));

    patchTwoUniverses(iom, stub);
    QList<Universe *> unis = iom.universes();

    writeLook(unis, 200, 77, 13, 90, 33);
    dumpUniverse(unis[0]);
    dumpUniverse(unis[1]);

    iom.setFrozen(true);

    /* Producers move on to a look that must never be published */
    writeLook(unis, 5, 1, 255, 250, 240);
    dumpUniverse(unis[0]);
    dumpUniverse(unis[1]);

    /* Blackout wins over the held look, using the existing channel classes:
       intensity goes dark, the held LTP value stays */
    iom.setBlackout(true);
    QVERIFY(iom.blackout() == true);
    QVERIFY(iom.isFrozen() == true);

    QCOMPARE(uchar(stub->m_universe[10]), uchar(0));
    QCOMPARE(uchar(stub->m_universe[12]), uchar(0));
    QCOMPARE(uchar(stub->m_universe[11]), uchar(77));
    QCOMPARE(uchar(stub->m_universe[512 + 20]), uchar(0));
    QCOMPARE(uchar(stub->m_universe[512 + 21]), uchar(33));

    /* Producers keep writing while blacked out and frozen */
    writeLook(unis, 7, 2, 9, 11, 13);
    dumpUniverse(unis[0]);
    dumpUniverse(unis[1]);

    QCOMPARE(uchar(stub->m_universe[10]), uchar(0));
    QCOMPARE(uchar(stub->m_universe[11]), uchar(77));

    /* Clearing blackout restores the held look, not the producers' look */
    iom.setBlackout(false);

    QCOMPARE(uchar(stub->m_universe[10]), uchar(200));
    QCOMPARE(uchar(stub->m_universe[11]), uchar(77));
    QCOMPARE(uchar(stub->m_universe[12]), uchar(13));
    QCOMPARE(uchar(stub->m_universe[512 + 20]), uchar(90));
    QCOMPARE(uchar(stub->m_universe[512 + 21]), uchar(33));

    /* Clearing blackout restores it under the live Grand Master */
    iom.setGrandMasterValue(128);
    dumpUniverse(unis[0]);
    QCOMPARE(uchar(stub->m_universe[10]), uchar(100));
    QCOMPARE(uchar(stub->m_universe[11]), uchar(77));
    QCOMPARE(uchar(stub->m_universe[12]), uchar(7));

    iom.setFrozen(false);
    iom.setGrandMasterValue(255);

    /* The other order: blackout first, then freeze. The hidden look is
       captured, so clearing blackout reveals it rather than zeros. */
    writeLook(unis, 111, 55, 22, 44, 66);
    dumpUniverse(unis[0]);
    dumpUniverse(unis[1]);

    iom.setBlackout(true);
    iom.setFrozen(true);

    QCOMPARE(uchar(stub->m_universe[10]), uchar(0));
    QCOMPARE(uchar(stub->m_universe[11]), uchar(55));

    writeLook(unis, 3, 4, 5, 6, 7);
    dumpUniverse(unis[0]);
    dumpUniverse(unis[1]);

    iom.setBlackout(false);

    QCOMPARE(uchar(stub->m_universe[10]), uchar(111));
    QCOMPARE(uchar(stub->m_universe[11]), uchar(55));
    QCOMPARE(uchar(stub->m_universe[12]), uchar(22));
}

/* Owner decision 1: frozen Blackout must suppress the RENDERED held look,
   retaining rendered non-HTP values, not the raw pre-GM blackout buffer. */
void InputOutputMap_Test::freezeBlackoutSuppressesRenderedHeldLook()
{
    ChannelModifier invert;
    QList<QPair<uchar, uchar> > map;
    map << qMakePair(uchar(0), uchar(255)) << qMakePair(uchar(255), uchar(0));
    invert.setModifierMap(map);

    InputOutputMap iom(m_doc, 2);

    IOPluginStub *stub = static_cast<IOPluginStub *>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);
    stub->m_universe.fill(char(0));

    patchTwoUniverses(iom, stub);
    QList<Universe *> unis = iom.universes();

    QVERIFY(iom.setInputPatch(0, stub->name(), "", stub->inputs().at(0), 0));
    iom.setUniversePassthrough(0, true);
    stub->emitValueChanged(UINT_MAX, 0, 11, 200);
    iom.flushInputs();

    unis[0]->setChannelModifier(11, &invert);

    iom.setGrandMasterValueMode(GrandMaster::Reduce);
    iom.setGrandMasterChannelMode(GrandMaster::AllChannels);
    iom.setGrandMasterValue(128);

    writeLook(unis, 200, 77, 13, 90, 33);
    dumpUniverse(unis[0]);

    /* 77 -> reduce at 128/255 -> 39 -> inverted -> 216 -> passthrough floor 200 */
    QCOMPARE(uchar(stub->m_universe[11]), uchar(216));
    QCOMPARE(uchar(stub->m_universe[10]), uchar(100));
    QCOMPARE(uchar(stub->m_universe[12]), uchar(7));

    iom.setFrozen(true);
    writeLook(unis, 5, 1, 255, 250, 240);
    dumpUniverse(unis[0]);
    QCOMPARE(uchar(stub->m_universe[11]), uchar(216));

    /* Blackout suppresses the HTP classes of the rendered held look and
       retains the rendered non-HTP value, not the raw captured 77 */
    iom.setBlackout(true);

    QCOMPARE(uchar(stub->m_universe[10]), uchar(0));
    QCOMPARE(uchar(stub->m_universe[12]), uchar(0));
    QCOMPARE(uchar(stub->m_universe[11]), uchar(216));

    iom.setBlackout(false);
    QCOMPARE(uchar(stub->m_universe[11]), uchar(216));
    QCOMPARE(uchar(stub->m_universe[10]), uchar(100));
}

/* Owner decision 2, non-frozen half: a locally paused output may not defeat
   Blackout. Its unblackened pause buffer is preserved and suppressed on the
   way out, so clearing Blackout restores exactly what it was holding. */
void InputOutputMap_Test::pausedPatchRespectsBlackout()
{
    InputOutputMap iom(m_doc, 2);

    IOPluginStub *stub = static_cast<IOPluginStub *>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);
    stub->m_universe.fill(char(0));

    patchTwoUniverses(iom, stub);
    iom.setOutputPatch(0, stub->name(), "", stub->outputs().at(2), 2, false, 1);

    QList<Universe *> unis = iom.universes();
    OutputPatch *pausedPatch = unis[0]->outputPatch(1);
    QVERIFY(pausedPatch != NULL);

    writeLook(unis, 200, 77, 13, 90, 33);
    dumpUniverse(unis[0]);

    pausedPatch->setPaused(true);
    dumpUniverse(unis[0]);
    QCOMPARE(uchar(stub->m_universe[1024 + 10]), uchar(200));

    iom.setBlackout(true);
    QVERIFY(iom.isFrozen() == false);

    QCOMPARE(uchar(stub->m_universe[10]), uchar(0));
    QCOMPARE(uchar(stub->m_universe[1024 + 10]), uchar(0));
    QCOMPARE(uchar(stub->m_universe[1024 + 11]), uchar(77));

    /* The pause buffer itself is untouched, so it comes back intact */
    iom.setBlackout(false);
    dumpUniverse(unis[0]);
    QCOMPARE(uchar(stub->m_universe[1024 + 10]), uchar(200));
    QCOMPARE(uchar(stub->m_universe[1024 + 11]), uchar(77));
    QVERIFY(pausedPatch->paused() == true);
}

/* Owner decision 2, thaw half: no bright paused buffer may leak on thaw
   while Blackout is still active. */
void InputOutputMap_Test::freezePausedThawUnderBlackoutStaysDark()
{
    InputOutputMap iom(m_doc, 2);

    IOPluginStub *stub = static_cast<IOPluginStub *>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);
    stub->m_universe.fill(char(0));

    patchTwoUniverses(iom, stub);
    iom.setOutputPatch(0, stub->name(), "", stub->outputs().at(2), 2, false, 1);

    QList<Universe *> unis = iom.universes();
    OutputPatch *pausedPatch = unis[0]->outputPatch(1);
    QVERIFY(pausedPatch != NULL);

    /* Bright look A is pinned by a local pause before Freeze */
    writeLook(unis, 200, 77, 13, 90, 33);
    dumpUniverse(unis[0]);
    pausedPatch->setPaused(true);
    dumpUniverse(unis[0]);
    QCOMPARE(uchar(stub->m_universe[1024 + 10]), uchar(200));

    iom.setFrozen(true);
    iom.setBlackout(true);
    QCOMPARE(uchar(stub->m_universe[1024 + 10]), uchar(0));

    writeLook(unis, 5, 1, 255, 250, 240);
    dumpUniverse(unis[0]);

    /* Thaw while Blackout remains on: the rig must stay dark */
    iom.setFrozen(false);
    dumpUniverse(unis[0]);

    QCOMPARE(uchar(stub->m_universe[10]), uchar(0));
    QCOMPARE(uchar(stub->m_universe[1024 + 10]), uchar(0));

    /* Clearing Blackout restores the pinned look A, unblackened */
    iom.setBlackout(false);
    dumpUniverse(unis[0]);
    QCOMPARE(uchar(stub->m_universe[1024 + 10]), uchar(200));
    QCOMPARE(uchar(stub->m_universe[1024 + 11]), uchar(77));
}

void InputOutputMap_Test::freezeThawPublishesLatestLook()
{
    InputOutputMap iom(m_doc, 2);

    IOPluginStub *stub = static_cast<IOPluginStub *>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);
    stub->m_universe.fill(char(0));

    patchTwoUniverses(iom, stub);
    /* A second output patch on universe 0, paused before Freeze */
    iom.setOutputPatch(0, stub->name(), "", stub->outputs().at(2), 2, false, 1);
    QCOMPARE(iom.outputPatchesCount(0), 2);

    QList<Universe *> unis = iom.universes();
    OutputPatch *pausedPatch = unis[0]->outputPatch(1);
    QVERIFY(pausedPatch != NULL);

    /* Look A, then look B which the paused patch keeps */
    writeLook(unis, 200, 77, 13, 90, 33);
    dumpUniverse(unis[0]);
    dumpUniverse(unis[1]);

    pausedPatch->setPaused(true);
    writeLook(unis, 160, 60, 20, 80, 30);
    dumpUniverse(unis[0]);
    dumpUniverse(unis[1]);

    QCOMPARE(uchar(stub->m_universe[10]), uchar(160));
    QCOMPARE(uchar(stub->m_universe[1024 + 10]), uchar(160));

    /* Look C reaches the live patch only */
    writeLook(unis, 100, 40, 30, 70, 20);
    dumpUniverse(unis[0]);
    dumpUniverse(unis[1]);

    QCOMPARE(uchar(stub->m_universe[10]), uchar(100));
    QCOMPARE(uchar(stub->m_universe[1024 + 10]), uchar(160));

    iom.setFrozen(true);

    /* Look D: each patch holds its own source, the paused one holds B */
    writeLook(unis, 50, 20, 40, 60, 10);
    dumpUniverse(unis[0]);
    dumpUniverse(unis[1]);

    QCOMPARE(uchar(stub->m_universe[10]), uchar(100));
    QCOMPARE(uchar(stub->m_universe[12]), uchar(30));
    QCOMPARE(uchar(stub->m_universe[1024 + 10]), uchar(160));
    QCOMPARE(uchar(stub->m_universe[1024 + 12]), uchar(20));

    /* The Grand Master dims both held sources, including the paused one */
    iom.setGrandMasterValue(128);
    dumpUniverse(unis[0]);

    QCOMPARE(uchar(stub->m_universe[10]), uchar(50));
    QCOMPARE(uchar(stub->m_universe[12]), uchar(15));
    QCOMPARE(uchar(stub->m_universe[11]), uchar(40));
    QCOMPARE(uchar(stub->m_universe[1024 + 10]), uchar(80));
    QCOMPARE(uchar(stub->m_universe[1024 + 12]), uchar(10));
    QCOMPARE(uchar(stub->m_universe[1024 + 11]), uchar(60));

    iom.setGrandMasterValue(255);

    /* Thaw jumps to the look the producers reached, with no restart.
       The independent pause survives and resumes its own buffer. */
    iom.setFrozen(false);
    QVERIFY(iom.isFrozen() == false);
    QVERIFY(pausedPatch->paused() == true);

    dumpUniverse(unis[0]);
    dumpUniverse(unis[1]);

    QCOMPARE(uchar(stub->m_universe[10]), uchar(50));
    QCOMPARE(uchar(stub->m_universe[11]), uchar(20));
    QCOMPARE(uchar(stub->m_universe[12]), uchar(40));
    QCOMPARE(uchar(stub->m_universe[512 + 20]), uchar(60));
    QCOMPARE(uchar(stub->m_universe[512 + 21]), uchar(10));
    QCOMPARE(uchar(stub->m_universe[1024 + 10]), uchar(160));
    QCOMPARE(uchar(stub->m_universe[1024 + 12]), uchar(20));
}

void InputOutputMap_Test::freezeResetClearsLatchAndHeldLook()
{
    InputOutputMap iom(m_doc, 2);

    IOPluginStub *stub = static_cast<IOPluginStub *>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);
    stub->m_universe.fill(char(0));

    patchTwoUniverses(iom, stub);
    QList<Universe *> unis = iom.universes();

    writeLook(unis, 200, 77, 13, 90, 33);
    dumpUniverse(unis[0]);
    dumpUniverse(unis[1]);

    QSignalSpy frozenSpy(&iom, SIGNAL(frozenChanged(bool)));
    iom.setFrozen(true);
    QCOMPARE(frozenSpy.count(), 1);

    /* A replaced workspace releases the latch and forgets the held look,
       without first publishing anything from the workspace being discarded */
    stub->m_writeLog.clear();
    iom.resetUniverses();
    QVERIFY(iom.isFrozen() == false);
    QCOMPARE(stub->m_writeLog.count(), 0);
    QCOMPARE(frozenSpy.count(), 2);
    QCOMPARE(frozenSpy.at(1).at(0).toBool(), false);

    /* Freezing again before any new frame was submitted must not resurrect
       the look that belonged to the discarded workspace */
    iom.setFrozen(true);
    dumpUniverse(unis[0]);
    dumpUniverse(unis[1]);

    QCOMPARE(uchar(stub->m_universe[10]), uchar(0));
    QCOMPARE(uchar(stub->m_universe[11]), uchar(0));
    QCOMPARE(uchar(stub->m_universe[12]), uchar(0));
    QCOMPARE(uchar(stub->m_universe[512 + 20]), uchar(0));
    QCOMPARE(uchar(stub->m_universe[512 + 21]), uchar(0));
}

void InputOutputMap_Test::freezeCoversUniverseAddedWhileFrozen()
{
    InputOutputMap iom(m_doc, 1);

    IOPluginStub *stub = static_cast<IOPluginStub *>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);
    stub->m_universe.fill(char(0));

    iom.setOutputPatch(0, stub->name(), "", stub->outputs().at(0), 0);
    QList<Universe *> unis = iom.universes();
    unis[0]->setChannelCapability(10, QLCChannel::Intensity);
    unis[0]->write(10, 200);
    dumpUniverse(unis[0]);
    QCOMPARE(uchar(stub->m_universe[10]), uchar(200));

    iom.setFrozen(true);

    /* A universe added while the workspace is frozen must not leak live
       output. It has no publication history, so it holds zero until thaw. */
    QVERIFY(iom.addUniverse());
    iom.setOutputPatch(1, stub->name(), "", stub->outputs().at(1), 1);

    Universe *added = iom.universe(1);
    QVERIFY(added != NULL);
    added->setChannelCapability(20, QLCChannel::Intensity);
    added->write(20, 222);
    dumpUniverse(added);

    QCOMPARE(uchar(stub->m_universe[512 + 20]), uchar(0));

    /* Thaw releases it together with the rest of the workspace */
    iom.setFrozen(false);
    dumpUniverse(added);
    QCOMPARE(uchar(stub->m_universe[512 + 20]), uchar(222));
}

/** The dataChanged flags the plugin received for one output line */
static QList<bool> changeFlagsFor(IOPluginStub *stub, quint32 output)
{
    QList<bool> flags;
    for (int i = 0; i < stub->m_writeLog.count(); i++)
    {
        if (stub->m_writeLog.at(i).first == output)
            flags << stub->m_writeLog.at(i).second;
    }
    return flags;
}

void InputOutputMap_Test::freezeKeepsPublishingRefreshes()
{
    InputOutputMap iom(m_doc, 2);

    IOPluginStub *stub = static_cast<IOPluginStub *>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);
    stub->m_universe.fill(char(0));
    stub->m_writeLog.clear();

    patchTwoUniverses(iom, stub);
    QList<Universe *> unis = iom.universes();

    writeLook(unis, 200, 77, 13, 90, 33);
    dumpUniverse(unis[0], unis[0]->hasChanged());
    dumpUniverse(unis[1], unis[1]->hasChanged());

    iom.setFrozen(true);

    /* The producers keep moving. Refresh callbacks must keep coming, but
       after the freeze edge the held bytes never change, so the frames
       must be reported clean even though the live values are changing. */
    stub->m_writeLog.clear();
    for (int tick = 0; tick < 3; tick++)
    {
        writeLook(unis, uchar(200 - tick * 10), uchar(77 - tick), 13, uchar(90 - tick), 33);
        dumpUniverse(unis[0], unis[0]->hasChanged());
        dumpUniverse(unis[1], unis[1]->hasChanged());
    }

    QCOMPARE(changeFlagsFor(stub, 0), QList<bool>() << true << false << false);
    QCOMPARE(changeFlagsFor(stub, 1), QList<bool>() << true << false << false);

    /* A Grand Master move changes the held bytes, so it must be reported */
    stub->m_writeLog.clear();
    iom.setGrandMasterValue(128);
    dumpUniverse(unis[0], unis[0]->hasChanged());
    dumpUniverse(unis[0], unis[0]->hasChanged());
    QCOMPARE(changeFlagsFor(stub, 0), QList<bool>() << true << false);

    /* Blackout and its release both change the effective bytes */
    stub->m_writeLog.clear();
    iom.setBlackout(true);
    QCOMPARE(changeFlagsFor(stub, 0), QList<bool>() << true);

    stub->m_writeLog.clear();
    iom.setBlackout(false);
    QCOMPARE(changeFlagsFor(stub, 0), QList<bool>() << true);

    /* Thaw must report a change even though the generator went static
       and the live bytes happen to match the held ones */
    stub->m_writeLog.clear();
    iom.setFrozen(false);
    dumpUniverse(unis[0], unis[0]->hasChanged());
    dumpUniverse(unis[0], unis[0]->hasChanged());
    QCOMPARE(changeFlagsFor(stub, 0), QList<bool>() << true << false);
}

void InputOutputMap_Test::freezeHoldsCapturedModifierAndPassthrough()
{
    /* Inverting modifier: getValue(v) == 255 - v */
    ChannelModifier invert;
    QList<QPair<uchar, uchar> > map;
    map << qMakePair(uchar(0), uchar(255)) << qMakePair(uchar(255), uchar(0));
    invert.setModifierMap(map);
    QCOMPARE(invert.getValue(13), uchar(242));
    QCOMPARE(invert.getValue(7), uchar(248));

    InputOutputMap iom(m_doc, 2);

    IOPluginStub *stub = static_cast<IOPluginStub *>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);
    stub->m_universe.fill(char(0));

    patchTwoUniverses(iom, stub);
    QList<Universe *> unis = iom.universes();

    /* Passthrough feeds an HTP floor onto the LTP channel 11 */
    QVERIFY(iom.setInputPatch(0, stub->name(), "", stub->inputs().at(0), 0));
    iom.setUniversePassthrough(0, true);
    stub->emitValueChanged(UINT_MAX, 0, 11, 90);
    iom.flushInputs();

    unis[0]->setChannelModifier(12, &invert);

    writeLook(unis, 200, 77, 13, 90, 33);
    dumpUniverse(unis[0]);
    dumpUniverse(unis[1]);

    QCOMPARE(uchar(stub->m_universe[11]), uchar(90));   // passthrough floor wins over 77
    QCOMPARE(uchar(stub->m_universe[12]), uchar(242));  // 13 inverted

    iom.setFrozen(true);

    /* Configuration changes after the capture must not reach the held look */
    unis[0]->setChannelModifier(12, NULL);
    stub->emitValueChanged(UINT_MAX, 0, 11, 200);
    iom.flushInputs();
    writeLook(unis, 5, 1, 255, 250, 240);
    dumpUniverse(unis[0]);

    QCOMPARE(uchar(stub->m_universe[11]), uchar(90));
    QCOMPARE(uchar(stub->m_universe[12]), uchar(242));

    /* The live Grand Master still runs upstream of the captured modifier:
       13 -> reduce at 128/255 -> 7 -> inverted -> 248 */
    iom.setGrandMasterValue(128);
    dumpUniverse(unis[0]);

    QCOMPARE(uchar(stub->m_universe[12]), uchar(248));
    QCOMPARE(uchar(stub->m_universe[11]), uchar(90));
    QCOMPARE(uchar(stub->m_universe[10]), uchar(100));
}

/* Rubber-duck trap 4: qmlui/fixturemanager.cpp:2693 edits an existing
   ChannelModifier in place, so holding a pointer to one is not a snapshot. */
void InputOutputMap_Test::freezeHoldsModifierValuesNotPointer()
{
    ChannelModifier invert;
    QList<QPair<uchar, uchar> > map;
    map << qMakePair(uchar(0), uchar(255)) << qMakePair(uchar(255), uchar(0));
    invert.setModifierMap(map);
    QCOMPARE(invert.getValue(13), uchar(242));

    InputOutputMap iom(m_doc, 2);

    IOPluginStub *stub = static_cast<IOPluginStub *>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);
    stub->m_universe.fill(char(0));

    patchTwoUniverses(iom, stub);
    QList<Universe *> unis = iom.universes();

    unis[0]->setChannelModifier(12, &invert);
    writeLook(unis, 200, 77, 13, 90, 33);
    dumpUniverse(unis[0]);
    QCOMPARE(uchar(stub->m_universe[12]), uchar(242));

    iom.setFrozen(true);

    /* The operator edits the modifier template itself while the look is held */
    QList<QPair<uchar, uchar> > identity;
    identity << qMakePair(uchar(0), uchar(0)) << qMakePair(uchar(255), uchar(255));
    invert.setModifierMap(identity);
    QCOMPARE(invert.getValue(13), uchar(13));

    dumpUniverse(unis[0]);

    /* The held look must keep the mapping that was captured */
    QCOMPARE(uchar(stub->m_universe[12]), uchar(242));
}

/* Rubber-duck trap 3: reopening a line gives a receiver with no cache, so the
   next held frame has to be reported as a change even if the bytes match. */
void InputOutputMap_Test::freezeReconnectForcesFullRefresh()
{
    InputOutputMap iom(m_doc, 2);

    IOPluginStub *stub = static_cast<IOPluginStub *>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);
    stub->m_universe.fill(char(0));

    patchTwoUniverses(iom, stub);
    QList<Universe *> unis = iom.universes();

    writeLook(unis, 200, 77, 13, 90, 33);
    dumpUniverse(unis[0]);

    iom.setFrozen(true);
    dumpUniverse(unis[0]);   // freeze edge, reported as a change

    /* The held look has settled: further frames are clean */
    stub->m_writeLog.clear();
    dumpUniverse(unis[0]);
    QCOMPARE(changeFlagsFor(stub, 0), QList<bool>() << false);

    QVERIFY(unis[0]->outputPatch(0)->reconnect());

    stub->m_writeLog.clear();
    dumpUniverse(unis[0]);
    QCOMPARE(changeFlagsFor(stub, 0), QList<bool>() << true);

    /* and it settles again afterwards */
    stub->m_writeLog.clear();
    dumpUniverse(unis[0]);
    QCOMPARE(changeFlagsFor(stub, 0), QList<bool>() << false);
}

void InputOutputMap_Test::freezeRepatchedOutputHoldsZero()
{
    InputOutputMap iom(m_doc, 1);

    IOPluginStub *stub = static_cast<IOPluginStub *>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);
    stub->m_universe.fill(char(0));

    iom.setOutputPatch(0, stub->name(), "", stub->outputs().at(0), 0);
    QList<Universe *> unis = iom.universes();
    unis[0]->setChannelCapability(10, QLCChannel::Intensity);
    unis[0]->write(10, 200);
    dumpUniverse(unis[0]);
    QCOMPARE(uchar(stub->m_universe[10]), uchar(200));

    iom.setFrozen(true);

    /* Moving the universe to another output line gives that line no history,
       so it must not inherit the look the previous line was holding */
    iom.setOutputPatch(0, stub->name(), "", stub->outputs().at(3), 3);
    dumpUniverse(unis[0]);

    QCOMPARE(uchar(stub->m_universe[1536 + 10]), uchar(0));

    iom.setFrozen(false);
    unis[0]->zeroIntensityChannels();
    unis[0]->write(10, 111);
    dumpUniverse(unis[0]);

    QCOMPARE(uchar(stub->m_universe[1536 + 10]), uchar(111));
}

/* Owner decision 3: a pause requested while the look is held must pin the
   frame the operator can actually see, not a later live one. */
void InputOutputMap_Test::freezePauseDuringFreezePinsHeldFrame()
{
    InputOutputMap iom(m_doc, 2);

    IOPluginStub *stub = static_cast<IOPluginStub *>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);
    stub->m_universe.fill(char(0));

    patchTwoUniverses(iom, stub);
    iom.setOutputPatch(0, stub->name(), "", stub->outputs().at(2), 2, false, 1);

    QList<Universe *> unis = iom.universes();
    OutputPatch *pausedPatch = unis[0]->outputPatch(1);
    QVERIFY(pausedPatch != NULL);

    writeLook(unis, 200, 77, 13, 90, 33);
    dumpUniverse(unis[0]);
    QCOMPARE(uchar(stub->m_universe[1024 + 10]), uchar(200));

    iom.setFrozen(true);

    /* Producers move on, the held look A stays on stage */
    writeLook(unis, 150, 60, 20, 80, 30);
    dumpUniverse(unis[0]);
    QCOMPARE(uchar(stub->m_universe[1024 + 10]), uchar(200));

    /* The operator pauses this output while looking at A */
    pausedPatch->setPaused(true);
    dumpUniverse(unis[0]);

    writeLook(unis, 100, 40, 30, 70, 20);
    dumpUniverse(unis[0]);
    QCOMPARE(uchar(stub->m_universe[1024 + 10]), uchar(200));

    /* Thaw: the live patch jumps to C, the paused one keeps the pinned A */
    iom.setFrozen(false);
    dumpUniverse(unis[0]);

    QCOMPARE(uchar(stub->m_universe[10]), uchar(100));
    QCOMPARE(uchar(stub->m_universe[12]), uchar(30));
    QCOMPARE(uchar(stub->m_universe[1024 + 10]), uchar(200));
    QCOMPARE(uchar(stub->m_universe[1024 + 11]), uchar(77));
    QCOMPARE(uchar(stub->m_universe[1024 + 12]), uchar(13));
}

/* Owner decision 6: when the live output extent grows during Freeze the held
   frame is zero-extended, so the advertised and published lengths agree and
   channels patched while frozen stay dark instead of undefined. */
void InputOutputMap_Test::freezeExtendsHeldFrameWhenExtentGrows()
{
    InputOutputMap iom(m_doc, 1);

    IOPluginStub *stub = static_cast<IOPluginStub *>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);
    stub->m_universe.fill(char(0));

    iom.setOutputPatch(0, stub->name(), "", stub->outputs().at(0), 0);
    QList<Universe *> unis = iom.universes();
    unis[0]->setChannelCapability(10, QLCChannel::Intensity);
    unis[0]->write(10, 200);
    dumpUniverse(unis[0]);
    QCOMPARE(uchar(stub->m_universe[10]), uchar(200));

    iom.setFrozen(true);

    /* A sentinel beyond the captured extent proves the frame really grew */
    stub->m_universe[40] = char(0xAA);

    unis[0]->setChannelCapability(40, QLCChannel::Intensity);
    unis[0]->write(40, 255);
    dumpUniverse(unis[0]);

    QCOMPARE(uchar(stub->m_universe[10]), uchar(200));
    QCOMPARE(uchar(stub->m_universe[40]), uchar(0));
}

/* Owner decision 5: a workspace reset must discard ALL per-publication
   history, including a paused patch's legacy bytes, so nothing from the old
   workspace can be republished. */
void InputOutputMap_Test::freezeResetClearsPausedPatchHistory()
{
    InputOutputMap iom(m_doc, 2);

    IOPluginStub *stub = static_cast<IOPluginStub *>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);
    stub->m_universe.fill(char(0));

    patchTwoUniverses(iom, stub);
    iom.setOutputPatch(0, stub->name(), "", stub->outputs().at(2), 2, false, 1);

    QList<Universe *> unis = iom.universes();
    OutputPatch *pausedPatch = unis[0]->outputPatch(1);
    QVERIFY(pausedPatch != NULL);

    writeLook(unis, 200, 77, 13, 90, 33);
    dumpUniverse(unis[0]);
    pausedPatch->setPaused(true);
    dumpUniverse(unis[0]);
    QCOMPARE(uchar(stub->m_universe[1024 + 10]), uchar(200));

    iom.setFrozen(true);
    iom.resetUniverses();
    QVERIFY(iom.isFrozen() == false);

    /* Nothing from the discarded workspace may reach the plugin again */
    stub->m_writeFrames.clear();
    dumpUniverse(unis[0]);

    QCOMPARE(uchar(stub->m_universe[1024 + 10]), uchar(0));
    QCOMPARE(uchar(stub->m_universe[1024 + 11]), uchar(0));
    QCOMPARE(uchar(stub->m_universe[10]), uchar(0));

    foreach (const QByteArray &frame, stub->m_writeFrames)
        QCOMPARE(frame.count(char(200)), 0);
}

/* Owner decision 4: a main-thread Blackout request may not interleave with a
   publication that is already in progress. The barrier holds the publishing
   thread inside writeUniverse while Blackout is requested from this thread. */
void InputOutputMap_Test::blackoutIsSerializedAgainstPublication()
{
    InputOutputMap iom(m_doc, 1);

    IOPluginStub *stub = static_cast<IOPluginStub *>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);
    stub->m_universe.fill(char(0));
    stub->m_writeFrames.clear();

    /* Two patches, so a flag flipped mid-publication would tear the frame
       across them: one patch blacked out and the other not. */
    iom.setOutputPatch(0, stub->name(), "", stub->outputs().at(0), 0);
    iom.setOutputPatch(0, stub->name(), "", stub->outputs().at(2), 2, false, 1);
    QCOMPARE(iom.outputPatchesCount(0), 2);

    QList<Universe *> unis = iom.universes();
    unis[0]->setChannelCapability(10, QLCChannel::Intensity);
    unis[0]->setChannelCapability(11, QLCChannel::Pan);
    unis[0]->write(10, 200);
    unis[0]->write(11, 77);

    Universe *universe = unis[0];
    stub->m_gatedOutput = 0;

    /* A publishing thread enters writeUniverse for the first patch and is
       held there, before the second patch has been published */
    std::thread publisher([universe]() { universe->dumpOutput(); });
    QVERIFY(stub->m_gateEntered.tryAcquire(1, 5000));

    /* Blackout is requested while that publication is still in progress */
    std::atomic<bool> blackoutDone(false);
    std::thread blackoutRequest([&iom, &blackoutDone]() {
        iom.setBlackout(true);
        blackoutDone = true;
    });
    QTest::qWait(150);
    QVERIFY(blackoutDone.load() == false);
    QCOMPARE(stub->m_writeFrames.count(), 0);

    stub->m_gateRelease.release(1);     // first patch of the live frame
    publisher.join();

    QVERIFY(stub->m_gateEntered.tryAcquire(1, 5000));
    stub->m_gateRelease.release(1);     // first patch of the blacked out frame
    blackoutRequest.join();
    stub->m_gatedOutput = -1;

    /* Four coherent writes: a whole live frame, then a whole dark one.
       No frame may be a mixture of the two states. */
    QCOMPARE(stub->m_writeFrames.count(), 4);
    QCOMPARE(uchar(stub->m_writeFrames.at(0).at(10)), uchar(200));
    QCOMPARE(uchar(stub->m_writeFrames.at(1).at(10)), uchar(200));
    QCOMPARE(uchar(stub->m_writeFrames.at(2).at(10)), uchar(0));
    QCOMPARE(uchar(stub->m_writeFrames.at(3).at(10)), uchar(0));
    QCOMPARE(uchar(stub->m_writeFrames.at(1).at(11)), uchar(77));
    QCOMPARE(uchar(stub->m_writeFrames.at(3).at(11)), uchar(77));
}

/* C0-3 through the real playback seam: an actual Chaser keeps stepping
   through MasterTimer while the look is held, and the frames are composed
   and published by Universe::processFaders, not by the test. */
void InputOutputMap_Test::freezeHoldsWhileChaserKeepsRunning()
{
    /* Uses the fixture Doc: a second IOPluginCache would load the same plugin
       instance and unload it from under this one on destruction. */
    Doc &doc = *m_doc;

    IOPluginStub *stub = static_cast<IOPluginStub *>
                                (doc.ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);
    stub->m_universe.fill(char(0));

    InputOutputMap *iom = doc.inputOutputMap();
    QVERIFY(iom->setOutputPatch(0, stub->name(), "", stub->outputs().at(0), 0));

    Fixture *fxi = new Fixture(&doc);
    fxi->setAddress(0);
    fxi->setUniverse(0);
    fxi->setChannels(1);
    doc.addFixture(fxi);

    Scene *sceneA = new Scene(&doc);
    sceneA->setValue(fxi->id(), 0, 200);
    doc.addFunction(sceneA);
    Scene *sceneB = new Scene(&doc);
    sceneB->setValue(fxi->id(), 0, 120);
    doc.addFunction(sceneB);
    Scene *sceneC = new Scene(&doc);
    sceneC->setValue(fxi->id(), 0, 60);
    doc.addFunction(sceneC);

    Chaser *chaser = new Chaser(&doc);
    chaser->setDurationMode(Chaser::PerStep);
    chaser->setFadeInMode(Chaser::PerStep);
    chaser->setFadeOutMode(Chaser::PerStep);
    foreach (Scene *scene, QList<Scene *>() << sceneA << sceneB << sceneC)
    {
        ChaserStep step(scene->id());
        step.fadeIn = 0;
        step.fadeOut = 0;
        step.hold = 100;
        step.duration = 100;
        chaser->addStep(step);
    }
    doc.addFunction(chaser);

    Universe *universe = iom->universes().at(0);

    chaser->start(doc.masterTimer(), FunctionParent::master());

    /* Let the chaser settle on its first look */
    for (int tick = 0; tick < 3; tick++)
    {
        doc.masterTimer()->timerTick();
        universe->processFaders(MasterTimer::tick());
    }
    QCOMPARE(uchar(stub->m_universe[0]), uchar(200));

    iom->setFrozen(true);

    /* The chaser keeps running for several steps while the look is held */
    for (int tick = 0; tick < 20; tick++)
    {
        doc.masterTimer()->timerTick();
        universe->processFaders(MasterTimer::tick());
        QCOMPARE(uchar(stub->m_universe[0]), uchar(200));
    }

    /* It really progressed: the live composition moved off the held value */
    QVERIFY(chaser->isRunning());
    const uchar liveValue = universe->preGMValue(0);
    QVERIFY2(liveValue != 200,
             qPrintable(QString("chaser did not progress, live value %1").arg(liveValue)));

    /* Thaw lands on whatever the chaser reached, with no restart */
    iom->setFrozen(false);
    doc.masterTimer()->timerTick();
    universe->processFaders(MasterTimer::tick());

    QCOMPARE(uchar(stub->m_universe[0]), universe->preGMValue(0));
    QVERIFY(uchar(stub->m_universe[0]) != uchar(200));
    QVERIFY(chaser->isRunning());

    chaser->stop(FunctionParent::master());
    doc.masterTimer()->timerTick();
    universe->processFaders(MasterTimer::tick());

    iom->setOutputPatch(0, KOutputNone, "", KOutputNone, QLCIOPlugin::invalidLine());
    doc.clearContents();
}

/* The submitted bytes and the retained provenance must describe the same
   frame. A producer that advances while the frame is inside the plugin must
   not change what was submitted, nor what a later Freeze holds. */
void InputOutputMap_Test::publishedFrameAndHeldSourceAgree()
{
    InputOutputMap iom(m_doc, 1);

    IOPluginStub *stub = static_cast<IOPluginStub *>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);
    stub->m_universe.fill(char(0));
    stub->m_writeFrames.clear();

    iom.setOutputPatch(0, stub->name(), "", stub->outputs().at(0), 0);
    Universe *universe = iom.universes().at(0);
    universe->setChannelCapability(10, QLCChannel::Intensity);
    universe->setChannelCapability(11, QLCChannel::Pan);
    universe->write(10, 200);
    universe->write(11, 77);

    stub->m_gatedOutput = 0;
    std::thread publisher([universe]() { universe->dumpOutput(); });
    QVERIFY(stub->m_gateEntered.tryAcquire(1, 5000));

    /* Producers reach B while the frame for A is still inside the plugin */
    universe->zeroIntensityChannels();
    universe->write(10, 99);
    universe->write(11, 42);

    stub->m_gateRelease.release(1);
    publisher.join();
    stub->m_gatedOutput = -1;

    /* What the plugin received is A, not a frame mutated under it */
    QCOMPARE(stub->m_writeFrames.count(), 1);
    QCOMPARE(uchar(stub->m_writeFrames.at(0).at(10)), uchar(200));
    QCOMPARE(uchar(stub->m_writeFrames.at(0).at(11)), uchar(77));

    /* ...and Freeze holds exactly that submitted frame */
    iom.setFrozen(true);
    dumpUniverse(universe);
    QCOMPARE(uchar(stub->m_universe[10]), uchar(200));
    QCOMPARE(uchar(stub->m_universe[11]), uchar(77));

    iom.setFrozen(false);
    dumpUniverse(universe);
    QCOMPARE(uchar(stub->m_universe[10]), uchar(99));
    QCOMPARE(uchar(stub->m_universe[11]), uchar(42));
}

void InputOutputMap_Test::grandMaster()
{
    InputOutputMap iom(m_doc, 4);

    QVERIFY(iom.grandMasterChannelMode() == GrandMaster::Intensity);
    QVERIFY(iom.grandMasterValueMode() == GrandMaster::Reduce);
    QVERIFY(iom.grandMasterValue() == 255);

    iom.setGrandMasterValue(100);
    QVERIFY(iom.grandMasterValue() == 100);

    iom.setGrandMasterChannelMode(GrandMaster::AllChannels);
    QVERIFY(iom.grandMasterChannelMode() == GrandMaster::AllChannels);

    iom.setGrandMasterValueMode(GrandMaster::Limit);
    QVERIFY(iom.grandMasterValueMode() == GrandMaster::Limit);
}

void InputOutputMap_Test::beatSourceBpmAndExternalLock()
{
    InputOutputMap iom(m_doc, 4);
    iom.setBeatGeneratorType(InputOutputMap::Plugin);
    QSignalSpy beatSpy(&iom, &InputOutputMap::beat);
    QVERIFY(beatSpy.isValid());

    QVERIFY(QMetaObject::invokeMethod(&iom, "slotProcessBeat",
                                      Qt::DirectConnection, Q_ARG(int, 140)));
    QCOMPARE(iom.bpmNumber(), 140);
    QCOMPARE(beatSpy.count(), 1);

    QTest::qWait(600);
    QVERIFY(QMetaObject::invokeMethod(&iom, "slotProcessBeat",
                                      Qt::DirectConnection, Q_ARG(int, 0)));
    QVERIFY(iom.bpmNumber() > 0);
    QVERIFY(iom.bpmNumber() != 140);
    QCOMPARE(beatSpy.count(), 2);

    iom.setExternalBpm(128);
    QCOMPARE(iom.bpmNumber(), 128);

    QVERIFY(QMetaObject::invokeMethod(&iom, "slotProcessBeat",
                                      Qt::DirectConnection, Q_ARG(int, 140)));
    QCOMPARE(iom.bpmNumber(), 128);
    QCOMPARE(beatSpy.count(), 3);

    QTest::qWait(600);
    QVERIFY(QMetaObject::invokeMethod(&iom, "slotProcessBeat",
                                      Qt::DirectConnection, Q_ARG(int, 0)));
    QCOMPARE(iom.bpmNumber(), 128);
    QCOMPARE(beatSpy.count(), 4);

    iom.clearExternalBpm();
    QVERIFY(QMetaObject::invokeMethod(&iom, "slotProcessBeat",
                                      Qt::DirectConnection, Q_ARG(int, 140)));
    QCOMPARE(iom.bpmNumber(), 140);
    QCOMPARE(beatSpy.count(), 5);
}

void InputOutputMap_Test::canonicalAudioClock()
{
    Doc doc(nullptr, 0);
    auto capture = QSharedPointer<AudioTestCapture>::create();
    doc.m_inputCapture = capture;
    capture->setAnalyzer(doc.audioAnalyzer());
    auto *first = new AudioProfile(7, &doc);
    auto *second = new AudioProfile(29, &doc);
    QVERIFY(doc.addAudioProfile(first));
    QVERIFY(doc.addAudioProfile(second));
    auto *iom = doc.inputOutputMap();
    iom->setBeatGeneratorType(InputOutputMap::Audio);
    iom->m_audioPollTimer.stop();
    QSignalSpy beats(iom, &InputOutputMap::beat);
    auto publish = [&](AudioProfile *profile, uint64_t count, double bpm,
                       uint64_t epoch = 1, bool valid = true) {
        AudioSnapshot snapshot;
        snapshot.sourceId = "clock-test";
        snapshot.sourceEpoch = epoch;
        snapshot.frameSequence = count + 1;
        snapshot.available = true;
        snapshot.status = "available";
        snapshot.publishTimeNs = AudioRenderView::nowNs();
        snapshot.music.bpm = bpm;
        snapshot.music.valid = valid;
        snapshot.events.beat = count;
        profile->channel()->injectSnapshot(snapshot);
    };
    const auto poll = [&]() {
        if (!QMetaObject::invokeMethod(iom, "slotPollAudio", Qt::DirectConnection))
            return false;
        while (iom->m_pendingAudioBeats)
            doc.masterTimer()->timerTick();
        return true;
    };
    publish(first, 11, 110.4);
    QVERIFY(poll());
    QCOMPARE(iom->bpmNumber(), 110);
    QCOMPARE(beats.count(), 0);
    publish(first, 13, 110.4);
    publish(first, 16, 128.2);
    QTest::qWait(65);
    QCOMPARE(beats.count(), 0);
    QVERIFY(QMetaObject::invokeMethod(iom, "slotPollAudio", Qt::DirectConnection));
    QCOMPARE(iom->m_pendingAudioBeats, uint64_t(5));
    QCOMPARE(beats.count(), 0);
    for (int i = 1; i <= 5; ++i)
    {
        doc.masterTimer()->timerTick();
        QCOMPARE(beats.count(), i);
        QVERIFY(doc.masterTimer()->isBeat());
    }
    QCOMPARE(iom->bpmNumber(), 128);
    QCOMPARE(beats.count(), 5);
    QVERIFY(poll());
    QCOMPARE(beats.count(), 5);
    iom->setExternalBpm(200);
    QCOMPARE(iom->bpmNumber(), 128);
    publish(first, 17, 145, 1, false);
    QVERIFY(poll());
    QCOMPARE(iom->bpmNumber(), 0);
    QCOMPARE(beats.count(), 5);

    publish(second, 100, 90);
    doc.setActiveAudioProfileId(29);
    QVERIFY(!doc.audioSnapshot().available);
    publish(second, 102, 90);
    QVERIFY(poll());
    QCOMPARE(beats.count(), 5);
    publish(second, 105, 90);
    QVERIFY(poll());
    QCOMPARE(beats.count(), 8);
    QCOMPARE(iom->bpmNumber(), 90);

    doc.setActiveAudioProfileId(7);
    doc.setActiveAudioProfileId(29);
    publish(second, 120, 91);
    QVERIFY(poll());
    QCOMPARE(beats.count(), 8);
    publish(second, 1, 91, 2);
    QVERIFY(poll());
    QCOMPARE(beats.count(), 8);
    publish(second, 2, 91, 2);
    QVERIFY(poll());
    QCOMPARE(beats.count(), 9);
    QVERIFY(doc.removeAudioProfile(29));
    QCOMPARE(doc.activeAudioProfileId(), quint32(7));
    QVERIFY(poll());
    QCOMPARE(beats.count(), 9);
}

void InputOutputMap_Test::audioRestartDropsPendingBeats_data()
{
    QTest::addColumn<QString>("source");
    QTest::addColumn<quint64>("epoch");
    QTest::newRow("restarted-source") << QString("clock-test") << quint64(2);
    QTest::newRow("replaced-source") << QString("other-clock") << quint64(1);
}

void InputOutputMap_Test::audioRestartDropsPendingBeats()
{
    QFETCH(QString, source);
    QFETCH(quint64, epoch);
    Doc doc(nullptr, 0);
    auto capture = QSharedPointer<AudioTestCapture>::create();
    doc.m_inputCapture = capture;
    capture->setAnalyzer(doc.audioAnalyzer());
    auto *profile = new AudioProfile(7, &doc);
    QVERIFY(doc.addAudioProfile(profile));
    auto *iom = doc.inputOutputMap();
    iom->setBeatGeneratorType(InputOutputMap::Audio);
    iom->m_audioPollTimer.stop();
    QSignalSpy beats(iom, &InputOutputMap::beat);
    AudioSnapshot snapshot;
    snapshot.sourceId = "clock-test";
    snapshot.sourceEpoch = 1;
    snapshot.available = true;
    snapshot.music.bpm = 110;
    snapshot.music.valid = true;
    const auto publish = [&]() {
        snapshot.publishTimeNs = AudioRenderView::nowNs();
        ++snapshot.frameSequence;
        profile->channel()->injectSnapshot(snapshot);
        return QMetaObject::invokeMethod(iom, "slotPollAudio", Qt::DirectConnection);
    };
    QVERIFY(publish());
    snapshot.events.beat = 5;
    QVERIFY(publish());
    QCOMPARE(iom->m_pendingAudioBeats, uint64_t(5));
    doc.masterTimer()->timerTick();
    QCOMPARE(beats.count(), 1);

    snapshot.sourceId = source;
    snapshot.sourceEpoch = epoch;
    snapshot.events.beat = 0;
    QVERIFY(publish());
    QCOMPARE(iom->m_pendingAudioBeats, uint64_t(0));
    doc.masterTimer()->timerTick();
    QCOMPARE(beats.count(), 1);
    snapshot.events.beat = 1;
    QVERIFY(publish());
    doc.masterTimer()->timerTick();
    QCOMPARE(beats.count(), 2);
}

void InputOutputMap_Test::canonicalAudioOwnership_data()
{
    QTest::addColumn<int>("source");
    QTest::newRow("internal") << int(InputOutputMap::Internal);
    QTest::newRow("plugin") << int(InputOutputMap::Plugin);
}

void InputOutputMap_Test::canonicalAudioOwnership()
{
    QFETCH(int, source);
    Doc doc(nullptr, 0);
    auto *iom = doc.inputOutputMap();
    iom->setBeatGeneratorType(InputOutputMap::BeatGeneratorType(source));
    iom->setExternalBpm(127);
    QSignalSpy beats(iom, &InputOutputMap::beat);
    QVERIFY(QMetaObject::invokeMethod(iom, "slotPollAudio", Qt::DirectConnection));
    QCOMPARE(iom->bpmNumber(), 127);
    QCOMPARE(beats.count(), 0);
    QVERIFY(iom->m_externalBpmLock);
    QVERIFY(QMetaObject::invokeMethod(iom, "slotProcessBeat",
        Qt::DirectConnection, Q_ARG(int, 145)));
    QCOMPARE(iom->bpmNumber(), 127);
    QCOMPARE(beats.count(), source == InputOutputMap::Plugin ? 1 : 0);
    iom->clearExternalBpm();
    QVERIFY(!iom->m_externalBpmLock);
}

void InputOutputMap_Test::canonicalPcmClock()
{
    Doc doc(nullptr, 0);
    auto capture = QSharedPointer<AudioTestCapture>::create();
    doc.m_inputCapture = capture;
    capture->setAnalyzer(doc.audioAnalyzer());
    QVERIFY(doc.addAudioProfile(new AudioProfile(7, &doc)));
    auto *iom = doc.inputOutputMap();
    iom->setBeatGeneratorType(InputOutputMap::Audio);
    iom->m_audioPollTimer.stop();
    QSignalSpy beats(iom, &InputOutputMap::beat);
    std::array<float, 500> samples{};
    AudioFrame frame;
    frame.samples = samples.data();
    frame.sourceEpoch = 17;
    uint64_t previousCount = 0, expectedBeats = 0;
    int validPolls = 0;
    bool seeded = false;
    for (int hop = 0; hop < 2400; ++hop)
    {
        double squareSum = 0, peak = 0;
        for (int i = 0; i < 500; ++i)
        {
            const double time = double(hop * 500 + i) / 30000;
            const double eighth = 60.0 / 110 / 2;
            const double age = std::fmod(time, eighth);
            const double amplitude = int(time / eighth) % 2 ? 0.65 : 0.9;
            samples[i] = float(amplitude * std::exp(-age * 65) *
                (0.7 * std::sin(2 * 3.141592653589793 * 65 * time) +
                 0.3 * std::sin(2 * 3.141592653589793 * 1700 * time)));
            squareSum += samples[i] * samples[i];
            peak = std::max(peak, std::abs(double(samples[i])));
        }
        frame.frameIndex = hop + 1;
        frame.sampleTime = hop * 500;
        frame.rms = std::sqrt(squareSum / 500);
        frame.peak = peak;
        frame.rmsDb = 20 * std::log10(std::max(frame.rms, 1e-12));
        frame.peakDb = 20 * std::log10(std::max(peak, 1e-12));
        frame.volumeNorm = std::clamp(1 + frame.rmsDb / 100, 0.0, 1.0);
        doc.audioAnalyzer()->processFrame(frame);
        if (hop % 37 != 0 && hop != 2399)
            continue;
        const auto snapshot = doc.audioSnapshot();
        if (snapshot.music.valid)
        {
            ++validPolls;
            if (seeded)
                expectedBeats += snapshot.events.beat - previousCount;
        }
        previousCount = snapshot.events.beat;
        seeded = true;
        QVERIFY(QMetaObject::invokeMethod(iom, "slotPollAudio", Qt::DirectConnection));
        if (snapshot.music.valid)
        {
            QVERIFY(iom->bpmNumber() > 0);
            QVERIFY(std::abs(iom->bpmNumber() - qRound(snapshot.music.bpm)) <= 2);
            QCOMPARE(doc.masterTimer()->bpmNumber(), iom->bpmNumber());
        }
        else
            QCOMPARE(iom->bpmNumber(), 0);
        while (iom->m_pendingAudioBeats)
            doc.masterTimer()->timerTick();
        QCOMPARE(uint64_t(beats.count()), expectedBeats);
    }
    QVERIFY(validPolls > 10);
    QVERIFY(expectedBeats > 10);
    qInfo() << "Canonical native 110-eighth integration: valid polls" << validPolls
            << "pulses" << expectedBeats << "final BPM" << iom->bpmNumber();
}

void InputOutputMap_Test::C1_audioBpmJitter_data()
{
    QTest::addColumn<int>("cadence");
    QTest::addColumn<int>("repeats");
    QTest::addColumn<int>("trace");
    for (int cadence : {16, 100})
        for (int repeats : {1, 3})
            for (int trace = 0; trace < 7; ++trace)
                QTest::newRow(qPrintable(QString("%1ms-%2polls-trace%3")
                    .arg(cadence).arg(repeats).arg(trace))) << cadence << repeats << trace;
}

void InputOutputMap_Test::C1_audioBpmJitter()
{
    QFETCH(int, cadence);
    QFETCH(int, repeats);
    QFETCH(int, trace);
    AudioClockTest clock;
    QVERIFY(clock.publish(120, 0));
    QSignalSpy accepted(clock.iom, &InputOutputMap::bpmNumberChanged);
    QSignalSpy master(clock.doc.masterTimer(), &MasterTimer::bpmNumberChanged);
    int nextObservation = 100, observation = 0;
    for (int time = cadence; time < 30000 + cadence; time += cadence)
    {
        if (time >= nextObservation)
        {
            QVERIFY(clock.publish(jitterBpm(observation, trace, nextObservation),
                nextObservation, repeats));
            nextObservation += trace < 3 && observation++ % 2 ? 200 : 100;
        }
        else
            QVERIFY(clock.poll(repeats));
        QCOMPARE(clock.iom->bpmNumber(), 120);
        QCOMPARE(clock.doc.masterTimer()->bpmNumber(), 120);
        QCOMPARE(clock.doc.masterTimer()->beatTimeDuration(), 500);
    }
    QCOMPARE(accepted.count(), 0);
    QCOMPARE(master.count(), 0);

    // One changed source frame cannot supply a second supporting observation.
    AudioClockTest duplicate;
    duplicate.snapshot.music.bpm = 120;
    duplicate.snapshot.publishTimeNs = AudioRenderView::nowNs();
    duplicate.snapshot.frameSequence = 1;
    duplicate.profile->channel()->injectSnapshot(duplicate.snapshot);
    QVERIFY(duplicate.poll());
    duplicate.snapshot.music.bpm = 121;
    duplicate.snapshot.publishTimeNs = AudioRenderView::nowNs();
    ++duplicate.snapshot.frameSequence;
    duplicate.profile->channel()->injectSnapshot(duplicate.snapshot);
    QVERIFY(duplicate.poll(repeats));
    QCOMPARE(duplicate.iom->bpmNumber(), 120);
    for (int elapsed = 0; elapsed < 1100; elapsed += 100)
    {
        QTest::qWait(100);
        QVERIFY(duplicate.poll(repeats));
        QCOMPARE(duplicate.iom->bpmNumber(), elapsed < 200 ? 120 : 0);
    }
    QCOMPARE(duplicate.doc.masterTimer()->bpmNumber(), 120);
}

void InputOutputMap_Test::C2_audioBeatTiming_data()
{
    QTest::addColumn<int>("repeats");
    QTest::addColumn<int>("batch");
    QTest::addColumn<int>("trace");
    QTest::addColumn<QString>("reset");
    for (int trace = 0; trace < 7; ++trace)
        for (const QString &reset : {QString("none"), QString("epoch"), QString("profile")})
            for (int batch : {1, 4})
                for (int repeats : {1, 3})
                    QTest::newRow(qPrintable(QString("%1-%2beats-%3polls-trace%4")
                        .arg(reset).arg(batch).arg(repeats).arg(trace)))
                        << repeats << batch << trace << reset;
}

void InputOutputMap_Test::C2_audioBeatTiming()
{
    QFETCH(int, repeats);
    QFETCH(int, batch);
    QFETCH(int, trace);
    QFETCH(QString, reset);
    AudioClockTest control, jitter;
    QVERIFY(control.publish(120, 0));
    QVERIFY(jitter.publish(120, 0));
    auto *constantChaser = control.chaser();
    auto *jitterChaser = jitter.chaser();
    QList<QPair<int, int>> constantSteps, jitterSteps;
    int tick = 0;
    connect(constantChaser, &Chaser::currentStepChanged, this,
        [&](int step) { constantSteps.append({tick, step}); });
    connect(jitterChaser, &Chaser::currentStepChanged, this,
        [&](int step) { jitterSteps.append({tick, step}); });
    QSignalSpy beats(jitter.iom, &InputOutputMap::beat);
    int queued = 0, delivered = 0, observation = 0, nextObservation = 100;
    bool consumed = false;
    const int ticks = 16000 / MasterTimer::tick() + batch + 2;
    for (tick = 0; tick < ticks; ++tick)
    {
        const int time = tick * MasterTimer::tick();
        if (time >= nextObservation)
        {
            bool replaced = false;
            if (time >= 7700 && observation < 10000 && reset != "none")
            {
                replaced = true;
                for (auto *clock : {&control, &jitter})
                {
                    if (reset == "epoch")
                        ++clock->snapshot.sourceEpoch;
                    else
                    {
                        clock->profile = new AudioProfile(29, &clock->doc);
                        QVERIFY(clock->doc.addAudioProfile(clock->profile));
                        clock->doc.setActiveAudioProfileId(29);
                    }
                    clock->snapshot.events.beat = 100;
                }
                queued = 0;
                observation = 10000;
            }
            else if (time > 0 && time / (500 * batch) >
                     (time - 100) / (500 * batch))
            {
                control.snapshot.events.beat += batch;
                jitter.snapshot.events.beat += batch;
                queued += batch;
            }
            QVERIFY(control.publish(120, nextObservation, repeats));
            QVERIFY(jitter.publish(replaced ? 120 : jitterBpm(observation, trace, nextObservation),
                nextObservation, repeats));
            ++observation;
            nextObservation += 100;
        }
        QVERIFY(jitter.poll(repeats));
        control.doc.masterTimer()->timerTick();
        jitter.doc.masterTimer()->timerTick();
        const bool expected = queued > 0;
        if (expected)
        {
            --queued;
            ++delivered;
        }
        QCOMPARE(jitterChaser->consumedBeats.last(), consumed);
        QCOMPARE(jitterChaser->consumedBeats.last(), constantChaser->consumedBeats.last());
        QCOMPARE(beats.count(), delivered);
        QCOMPARE(jitterSteps, constantSteps);
        consumed = expected;
    }
    QVERIFY(constantSteps.size() >= 30);
    QCOMPARE(delivered, 32);
    QCOMPARE(jitterChaser->consumedBeats.size(), ticks);
}

void InputOutputMap_Test::C3_audioTempoChanges_data()
{
    QTest::addColumn<int>("base");
    QTest::addColumn<int>("offset");
    QTest::addColumn<int>("cadence");
    QTest::addColumn<int>("repeats");
    for (int base : {90, 120, 180})
        for (int offset : {-18, -8, -3, -2, -1, 1, 2, 3, 8})
            for (int cadence : {16, 100})
                for (int repeats : {1, 3})
                    QTest::newRow(qPrintable(QString("%1-%2-%3ms-%4polls")
                        .arg(base).arg(offset).arg(cadence).arg(repeats)))
                        << base << offset << cadence << repeats;
    QTest::newRow("128-to-110") << 128 << -18 << 16 << 3;
    QTest::newRow("staircase") << 120 << 0 << 16 << 3;
}

void InputOutputMap_Test::C3_audioTempoChanges()
{
    QFETCH(int, base);
    QFETCH(int, offset);
    QFETCH(int, cadence);
    QFETCH(int, repeats);
    AudioClockTest clock, control;
    QVERIFY(clock.publish(base, 0));
    QVERIFY(control.publish(base, 0));
    auto *actualChaser = clock.chaser();
    auto *controlChaser = control.chaser();
    QList<QPair<int, int>> actualSteps, controlSteps;
    int tick = 0;
    connect(actualChaser, &Chaser::currentStepChanged, this,
        [&](int step) { actualSteps.append({tick, step}); });
    connect(controlChaser, &Chaser::currentStepChanged, this,
        [&](int step) { controlSteps.append({tick, step}); });
    int time = 0, tickRemainder = 0;
    const QList<int> targets = offset ? QList<int>{base + offset, base}
                                     : QList<int>{121, 122, 123};
    for (int target : targets)
    {
        int adoptedAt = -1, nextObservation = time + 100;
        const int firstObservation = nextObservation;
        for (int elapsed = 0; elapsed <= (offset ? 1400 : 1100); elapsed += cadence)
        {
            time += cadence;
            if (time >= nextObservation)
            {
                QVERIFY(clock.publish(target, nextObservation, repeats));
                nextObservation += 100;
            }
            else
                QVERIFY(clock.poll(repeats));
            const bool observed = nextObservation > firstObservation;
            const int expected = observed &&
                (std::abs(target - base) > 2 ||
                 nextObservation - 100 >= firstObservation + 1000) ? target : base;
            QCOMPARE(clock.iom->bpmNumber(), expected);
            if (clock.iom->bpmNumber() == target && adoptedAt < 0)
                adoptedAt = nextObservation - 100;
            if (adoptedAt >= 0)
                QCOMPARE(clock.iom->bpmNumber(), target);
            if (nextObservation - 100 >= firstObservation + 1000)
                QCOMPARE(clock.iom->bpmNumber(), target);
            if (std::abs(target - base) > 2 && nextObservation > firstObservation)
                QCOMPARE(clock.iom->bpmNumber(), target);
            QCOMPARE(clock.doc.masterTimer()->bpmNumber(), clock.iom->bpmNumber());
            QCOMPARE(clock.doc.masterTimer()->beatTimeDuration(),
                60000 / clock.iom->bpmNumber());
            control.iom->setBpmNumber(expected);
            // Both real runners consume the accepted transition at the same tick.
            tickRemainder += cadence;
            while (tickRemainder >= int(MasterTimer::tick()))
            {
                clock.doc.masterTimer()->timerTick();
                control.doc.masterTimer()->timerTick();
                QCOMPARE(actualSteps, controlSteps);
                tickRemainder -= MasterTimer::tick();
                ++tick;
            }
        }
        QVERIFY(adoptedAt >= firstObservation);
        QVERIFY(adoptedAt - firstObservation <= 1000);
        if (std::abs(target - base) > 2)
            QCOMPARE(adoptedAt, firstObservation);
        base = target;
    }
    QVERIFY(actualSteps.size() >= 3);
}

void InputOutputMap_Test::C4_audioShiftedNoise_data()
{
    QTest::addColumn<int>("base");
    QTest::addColumn<int>("direction");
    QTest::addColumn<int>("phase");
    QTest::addColumn<bool>("fractional");
    QTest::addColumn<int>("cadence");
    QTest::addColumn<int>("repeats");
    QTest::addColumn<QString>("interruption");
    for (int base : {90, 120, 180})
        for (int direction : {-1, 1})
            for (int phase : {0, 1})
                for (bool fractional : {false, true})
                    for (int cadence : {16, 100})
                        for (int repeats : {1, 3})
                            for (const QString &interruption :
                                 {QString("none"), QString("baseline"), QString("opposite")})
                                QTest::newRow(qPrintable(QString("%1-%2-phase%3-%4-%5ms-%6polls-%7")
                                    .arg(base).arg(direction).arg(phase).arg(fractional)
                                    .arg(cadence).arg(repeats).arg(interruption)))
                                    << base << direction << phase << fractional
                                    << cadence << repeats << interruption;
    QTest::newRow("duplicate-stale") << 120 << 1 << 0 << false << 100 << 3
                                    << QString("duplicate");
}

void InputOutputMap_Test::C4_audioShiftedNoise()
{
    QFETCH(int, base);
    QFETCH(int, direction);
    QFETCH(int, phase);
    QFETCH(bool, fractional);
    QFETCH(int, cadence);
    QFETCH(int, repeats);
    QFETCH(QString, interruption);
    AudioClockTest clock, control;
    if (interruption == "duplicate")
    {
        clock.snapshot.music.bpm = base;
        clock.snapshot.publishTimeNs = AudioRenderView::nowNs();
        clock.snapshot.frameSequence = 1;
        clock.profile->channel()->injectSnapshot(clock.snapshot);
        QVERIFY(clock.poll(repeats));
        clock.snapshot.music.bpm = base + direction;
        clock.snapshot.publishTimeNs = AudioRenderView::nowNs();
        ++clock.snapshot.frameSequence;
        clock.profile->channel()->injectSnapshot(clock.snapshot);
        QVERIFY(clock.poll(repeats));
        QCOMPARE(clock.iom->bpmNumber(), base);
        QTest::qWait(1100);
        QVERIFY(clock.poll(repeats));
        QCOMPARE(clock.iom->bpmNumber(), 0);
        QCOMPARE(clock.doc.masterTimer()->bpmNumber(), base);
        QCOMPARE(clock.iom->m_pendingAudioBeats, uint64_t(0));
        return;
    }
    QVERIFY(clock.publish(base, 0));
    QVERIFY(control.publish(base, 0));
    auto *actualChaser = clock.chaser();
    auto *controlChaser = control.chaser();
    QList<QPair<int, int>> actualSteps, controlSteps;
    int tick = 0;
    connect(actualChaser, &Chaser::currentStepChanged, this,
        [&](int step) { actualSteps.append({tick, step}); });
    connect(controlChaser, &Chaser::currentStepChanged, this,
        [&](int step) { controlSteps.append({tick, step}); });
    QSignalSpy accepted(clock.iom, &InputOutputMap::bpmNumberChanged);
    QSignalSpy master(clock.doc.masterTimer(), &MasterTimer::bpmNumberChanged);
    QSignalSpy beats(clock.iom, &InputOutputMap::beat);
    const int firstSupport = interruption == "none" ? 100 : 1100;
    const int deadline = firstSupport + 1000;
    int nextObservation = 100, observedAt = 0, tickRemainder = 0;
    for (int time = cadence; time < deadline + 30000 + cadence; time += cadence)
    {
        if (time >= nextObservation)
        {
            observedAt = nextObservation;
            double raw = base + direction * (1 + ((observedAt / 100 + phase) % 2));
            if (fractional)
                raw -= direction * 0.49;
            if (interruption != "none" && observedAt == 1000)
                raw = base + (interruption == "opposite" ? -direction : 0);
            clock.snapshot.events.beat = observedAt / 500;
            control.snapshot.events.beat = clock.snapshot.events.beat;
            QVERIFY(clock.publish(raw, observedAt, repeats));
            const double controlRaw = interruption != "none" && observedAt == 1000
                ? raw : base + direction;
            QVERIFY(control.publish(controlRaw, observedAt, repeats));
            nextObservation += 100;
        }
        else
        {
            QVERIFY(clock.poll(repeats));
            QVERIFY(control.poll(repeats));
        }
        const int expected = observedAt >= deadline ? base + direction : base;
        QCOMPARE(clock.iom->bpmNumber(), expected);
        QCOMPARE(control.iom->bpmNumber(), expected);
        QCOMPARE(clock.doc.masterTimer()->bpmNumber(), expected);
        QCOMPARE(clock.doc.masterTimer()->beatTimeDuration(), 60000 / expected);
        QCOMPARE(accepted.count(), observedAt >= deadline ? 1 : 0);
        QCOMPARE(master.count(), observedAt >= deadline ? 1 : 0);
        tickRemainder += cadence;
        while (tickRemainder >= int(MasterTimer::tick()))
        {
            clock.doc.masterTimer()->timerTick();
            control.doc.masterTimer()->timerTick();
            QCOMPARE(actualChaser->consumedBeats.last(), controlChaser->consumedBeats.last());
            QCOMPARE(actualSteps, controlSteps);
            tickRemainder -= MasterTimer::tick();
            ++tick;
        }
    }
    QVERIFY(actualSteps.size() > 30);
    QCOMPARE(beats.count(), observedAt / 500);
}

void InputOutputMap_Test::C1_audioBpmLifecycle_data()
{
    QTest::addColumn<QString>("change");
    QTest::addColumn<bool>("alternating");
    for (const QString &change : {QString("invalid"), QString("unavailable"), QString("stale"),
         QString("zero"), QString("negative"), QString("nan"), QString("infinity"),
         QString("overflow"), QString("profile"), QString("source"), QString("epoch"),
         QString("mode")})
        for (bool alternating : {false, true})
            QTest::newRow(qPrintable(QString("%1-%2").arg(change).arg(alternating)))
                << change << alternating;
}

void InputOutputMap_Test::C1_audioBpmLifecycle()
{
    QFETCH(QString, change);
    QFETCH(bool, alternating);
    AudioClockTest clock;
    QVERIFY(clock.publish(120, 0));
    clock.snapshot.events.beat = 4;
    QVERIFY(clock.publish(121, 100));
    for (int time = 200; time <= 900; time += 100)
        QVERIFY(clock.publish(alternating && time % 200 == 0 ? 122 : 121, time));
    QCOMPARE(clock.iom->bpmNumber(), 120);
    QCOMPARE(clock.iom->m_pendingAudioBeats, uint64_t(4));
    const bool replacement = change == "profile" || change == "source" ||
        change == "epoch" || change == "mode";
    if (change == "profile")
    {
        clock.profile = new AudioProfile(29, &clock.doc);
        QVERIFY(clock.doc.addAudioProfile(clock.profile));
        clock.doc.setActiveAudioProfileId(29);
    }
    else if (change == "source")
        clock.snapshot.sourceId = "replacement";
    else if (change == "epoch")
        ++clock.snapshot.sourceEpoch;
    else if (change == "mode")
    {
        clock.iom->setBeatGeneratorType(InputOutputMap::Plugin);
        clock.iom->setBeatGeneratorType(InputOutputMap::Audio);
        clock.iom->m_audioPollTimer.stop();
    }
    else if (change == "invalid")
        clock.snapshot.music.valid = false;
    else if (change == "unavailable")
        clock.snapshot.available = false;
    double bpm = 121;
    if (change == "zero") bpm = 0;
    if (change == "negative") bpm = -1;
    if (change == "nan") bpm = std::numeric_limits<double>::quiet_NaN();
    if (change == "infinity") bpm = std::numeric_limits<double>::infinity();
    if (change == "overflow") bpm = double(std::numeric_limits<int>::max()) + 1;
    if (change == "stale")
    {
        clock.snapshot.publishTimeNs = AudioRenderView::nowNs() - 251000000;
        clock.profile->channel()->injectSnapshot(clock.snapshot);
        QVERIFY(clock.poll());
    }
    else
        QVERIFY(clock.publish(bpm, 1000));
    QCOMPARE(clock.iom->m_pendingAudioBeats, uint64_t(0));
    QCOMPARE(clock.iom->bpmNumber(), replacement ? 121 : 0);
    QCOMPARE(clock.doc.masterTimer()->bpmNumber(), replacement ? 121 : 120);
    clock.snapshot.available = true;
    clock.snapshot.music.valid = true;
    QVERIFY(clock.publish(121, 1100));
    QCOMPARE(clock.iom->bpmNumber(), 121);
    if (!replacement)
        QCOMPARE(clock.iom->m_pendingAudioBeats, uint64_t(0));
    while (clock.iom->m_pendingAudioBeats)
        clock.doc.masterTimer()->timerTick();
    QSignalSpy beats(clock.iom, &InputOutputMap::beat);
    ++clock.snapshot.events.beat;
    QVERIFY(clock.publish(121, 1200));
    clock.doc.masterTimer()->timerTick();
    QCOMPARE(beats.count(), 1);
}

void InputOutputMap_Test::C1_nativeAudioBpm_data()
{
    QTest::addColumn<int>("target");
    QTest::newRow("steady-120") << 120;
    QTest::newRow("up-121") << 121;
    QTest::newRow("down-119") << 119;
    QTest::newRow("up-128") << 128;
    QTest::newRow("down-110") << 110;
}

void InputOutputMap_Test::C1_nativeAudioBpm()
{
    QFETCH(int, target);
    AudioClockTest clock;
    QSignalSpy beats(clock.iom, &InputOutputMap::beat);
    std::array<float, 500> samples{};
    AudioFrame frame;
    frame.samples = samples.data();
    frame.sourceEpoch = 17;
    uint64_t previousCount = 0, expectedBeats = 0;
    bool seeded = false;
    int validPolls = 0, smallAdoptions = 0, largeAdoptions = 0, completedTransients = 0;
    int previousRaw = 0, rawSince = 0, baselineRaw = 0, firstShift = -1;
    int supportSince = -1, supportSide = 0;
    QList<int> supported;
    int excursionBpm = 0, excursionSince = 0;
    bool excursionStable = true;
    double phase = 0;
    for (int hop = 0; hop < 4800; ++hop)
    {
        double squareSum = 0, peak = 0;
        const int tempo = hop < 2400 ? 120 : target;
        for (int i = 0; i < 500; ++i)
        {
            const double time = double(hop * 500 + i) / 30000;
            phase += double(tempo) / 60 / 30000 * 2;
            const double age = (phase - std::floor(phase)) * 60 / tempo / 2;
            const double amplitude = int(phase) % 2 ? 0.65 : 0.9;
            samples[i] = float(amplitude * std::exp(-age * 65) *
                (0.7 * std::sin(2 * 3.141592653589793 * 65 * time) +
                 0.3 * std::sin(2 * 3.141592653589793 * 1700 * time)));
            squareSum += samples[i] * samples[i];
            peak = std::max(peak, std::abs(double(samples[i])));
        }
        frame.frameIndex = hop + 1;
        frame.sampleTime = hop * 500;
        frame.rms = std::sqrt(squareSum / 500);
        frame.peak = peak;
        frame.rmsDb = 20 * std::log10(std::max(frame.rms, 1e-12));
        frame.peakDb = 20 * std::log10(std::max(peak, 1e-12));
        frame.volumeNorm = std::clamp(1 + frame.rmsDb / 100, 0.0, 1.0);
        clock.doc.audioAnalyzer()->processFrame(frame);
        auto snapshot = clock.doc.audioSnapshot();
        // Replay the production observation at its PCM time, independently of CPU speed.
        snapshot.publishTimeNs = clock.origin + int64_t(hop) * 500 * 1000000000 / 30000;
        clock.profile->channel()->injectSnapshot(snapshot);
        const int raw = snapshot.music.valid ? qRound(snapshot.music.bpm) : 0;
        const int before = clock.iom->bpmNumber();
        const int durationBefore = clock.doc.masterTimer()->beatTimeDuration();
        const bool rawChanged = raw != previousRaw;
        if (rawChanged)
            rawSince = hop;
        if (hop < 2400)
            baselineRaw = raw;
        if (raw)
        {
            ++validPolls;
            if (seeded)
                expectedBeats += snapshot.events.beat - previousCount;
            if (hop >= 2400 && firstShift < 0 && raw != baselineRaw &&
                (std::abs(raw - baselineRaw) > 2 || hop - rawSince >= 60))
                firstShift = rawSince;
        }
        previousCount = snapshot.events.beat;
        seeded = snapshot.available;
        int expected = before;
        if (!raw || !before || std::abs(raw - before) > 2)
        {
            expected = raw;
            supportSince = -1;
        }
        else if (raw == before)
            supportSince = -1;
        else
        {
            const int side = raw > before ? 1 : -1;
            if (supportSince < 0 || supportSide != side)
            {
                supportSince = hop;
                supportSide = side;
                supported.clear();
            }
            supported.append(raw);
            if (hop - supportSince >= 60)
            {
                expected = side > 0
                    ? *std::min_element(supported.begin(), supported.end())
                    : *std::max_element(supported.begin(), supported.end());
                ++smallAdoptions;
                supportSince = -1;
            }
        }
        if (!excursionBpm && before && raw != before && raw &&
            std::abs(raw - before) <= 2)
        {
            excursionBpm = before;
            excursionSince = hop;
            excursionStable = durationBefore == 60000 / before;
        }
        QVERIFY(clock.poll());
        const int accepted = clock.iom->bpmNumber();
        QCOMPARE(accepted, expected);
        if (!raw)
            QCOMPARE(accepted, 0);
        else
        {
            if (!before || std::abs(raw - before) > 2)
            {
                QCOMPARE(accepted, raw);
                if (before) ++largeAdoptions;
            }
            QCOMPARE(clock.doc.masterTimer()->bpmNumber(), accepted);
            QCOMPARE(clock.doc.masterTimer()->beatTimeDuration(), 60000 / accepted);
        }
        if (excursionBpm)
        {
            excursionStable = excursionStable && accepted == excursionBpm &&
                clock.doc.masterTimer()->beatTimeDuration() == 60000 / excursionBpm;
            if (raw == excursionBpm && hop - excursionSince < 60)
            {
                QVERIFY(excursionStable);
                ++completedTransients;
                qInfo() << "native transient returned to" << excursionBpm
                        << "after ms" << (hop - excursionSince) * 500 / 30;
                excursionBpm = 0;
            }
            else if (!raw || hop - excursionSince >= 60 ||
                     std::abs(raw - excursionBpm) > 2)
                excursionBpm = 0;
        }
        if (rawChanged || accepted != before)
            qInfo() << "PCM ms" << hop * 500 / 30 << "raw" << snapshot.music.bpm
                    << "accepted" << accepted << "beat counter" << snapshot.events.beat
                    << "phase" << snapshot.music.beatPhase
                    << "confidence" << snapshot.music.beatConfidence;
        previousRaw = raw;
        const auto unchanged = clock.doc.audioSnapshot();
        QCOMPARE(unchanged.music.bpm, snapshot.music.bpm);
        QCOMPARE(unchanged.music.beatPhase, snapshot.music.beatPhase);
        QCOMPARE(unchanged.music.barPhase, snapshot.music.barPhase);
        QCOMPARE(unchanged.music.beatConfidence, snapshot.music.beatConfidence);
        QCOMPARE(unchanged.events.onset, snapshot.events.onset);
        QCOMPARE(unchanged.events.beat, snapshot.events.beat);
        QCOMPARE(unchanged.events.kick, snapshot.events.kick);
        QCOMPARE(unchanged.events.bar, snapshot.events.bar);
        while (clock.iom->m_pendingAudioBeats)
            clock.doc.masterTimer()->timerTick();
        QCOMPARE(uint64_t(beats.count()), expectedBeats);
    }
    QVERIFY(validPolls > 100);
    QVERIFY(expectedBeats > 30);
    if (!completedTransients)
        qInfo() << "native transient stimulus absent";
    if (target != 120)
        QVERIFY(firstShift >= 2400);
    qInfo() << "Native PCM target" << target << "valid polls" << validPolls
            << "pulses" << expectedBeats << "delayed sustained shifts" << smallAdoptions
            << "large adoptions" << largeAdoptions
            << "completed native transients" << completedTransients
            << "onset-to-estimate ms"
            << (firstShift < 0 ? -1 : (firstShift - 2400) * 500 / 30)
            << "final raw" << clock.doc.audioSnapshot().music.bpm
            << "accepted" << clock.iom->bpmNumber();
}

QTEST_GUILESS_MAIN(InputOutputMap_Test)
