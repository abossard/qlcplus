/*
  Q Light Controller Plus - Test Unit
  showrunner_test.cpp

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
#include <QDir>
#include <QSettings>
#include <cstring>
#define private public
#define protected public
#include "audio.h"
#include "audiorenderer_null.h"
#include "showrunner.h"
#include "mastertimer.h"
#include "universe.h"
#undef protected
#undef private
#include "fixture.h"
#include "show.h"
#include "showcommandtrack.h"
#include "track.h"
#include "scene.h"
#include "collection.h"
#include "doc.h"
#include "inputoutputmap.h"
#include "showrunner_test.h"

class AudioTestDecoder final : public AudioDecoder
{
public:
    AudioTestDecoder()
    {
        configure(48000, 2, PCM_S16LE);
    }

    AudioDecoder *createCopy() override { return new AudioTestDecoder; }
    int priority() const override { return 0; }
    QStringList supportedFormats() override { return {}; }
    bool initialize(const QString &) override { return true; }
    qint64 totalTime() override { return 60000; }
    void seek(qint64) override { }
    qint64 read(char *data, qint64 maxSize) override
    {
        std::memset(data, 0x7f, size_t(maxSize));
        QThread::msleep(1);
        return maxSize;
    }
    int bitrate() override { return 1536; }
};

static AudioRendererNull *attachNullRenderer(Audio *audio, bool start)
{
    auto *decoder = new AudioTestDecoder;
    auto *renderer = new AudioRendererNull;
    renderer->setDecoder(decoder);
    renderer->initialize(48000, 2, PCM_S16LE);
    renderer->setUserStop(false);
    audio->m_decoder = decoder;
    audio->m_audio_out = renderer;
    if (start)
        renderer->start();
    return renderer;
}

void ShowRunner_Test::initTestCase()
{
    const QString settingsPath = qEnvironmentVariable(
        "QLCPLUS_TEST_SETTINGS_PATH",
        QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("settings-showrunner"));
    QVERIFY(QDir().mkpath(settingsPath));
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsPath);
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, settingsPath);

    m_doc = new Doc(this);
    m_show = new Show(m_doc);
    m_doc->addFunction(m_show);
    m_scene = new Scene(m_doc);
    m_doc->addFunction(m_scene);
    m_track = new Track(m_scene->id());
    ShowFunction *sf = new ShowFunction(m_show->getLatestShowFunctionId());
    sf->setFunctionID(m_scene->id());
    sf->setStartTime(0);
    sf->setDuration(1000);
    m_track->addShowFunction(sf);
    m_show->addTrack(m_track);
}

void ShowRunner_Test::cleanupTestCase()
{
    delete m_doc;
}

void ShowRunner_Test::initRunner()
{
    ShowRunner runner(m_doc, m_show->id());
    QCOMPARE(runner.m_timeFunctions.count(), 1);
    QCOMPARE(runner.m_totalRunTime, quint32(1000));
}

void ShowRunner_Test::intensity()
{
    ShowRunner runner(m_doc, m_show->id());
    runner.adjustIntensity(0.5, m_track);
    QCOMPARE(runner.m_intensityMap[m_track->id()], 0.5);
}

void ShowRunner_Test::stopRunner()
{
    ShowRunner runner(m_doc, m_show->id());
    runner.m_elapsedTime = 500;
    runner.m_runningQueue.append(QPair<Function*,quint32>(m_scene,1000));
    runner.stop();
    QCOMPARE(runner.m_elapsedTime, quint32(0));
    QCOMPARE(runner.m_runningQueue.count(), 0);
}

void ShowRunner_Test::externalSyncDoesNotAutoIncrement()
{
    // C1: In External mode, write() does NOT auto-increment m_elapsedTime
    ShowRunner runner(m_doc, m_show->id());
    runner.setSyncSource(ShowRunner::External);
    QCOMPARE(runner.syncSource(), ShowRunner::External);

    quint32 timeBefore = runner.m_elapsedTime;
    // write() with External source and externalElapsedTime=0 should not increment
    runner.write(m_doc->masterTimer());
    QCOMPARE(runner.m_elapsedTime, timeBefore);
}

void ShowRunner_Test::externalSyncStartsFunctions()
{
    // C2: set external time to 500ms, function at 0ms with 1000ms duration should be started
    // Create a fresh show with a scene at time 0
    Show *show = new Show(m_doc);
    m_doc->addFunction(show);
    Scene *scene = new Scene(m_doc);
    m_doc->addFunction(scene);
    Track *track = new Track(scene->id());
    ShowFunction *sf = new ShowFunction(show->getLatestShowFunctionId());
    sf->setFunctionID(scene->id());
    sf->setStartTime(0);
    sf->setDuration(1000);
    track->addShowFunction(sf);
    show->addTrack(track);

    ShowRunner runner(m_doc, show->id());
    runner.setSyncSource(ShowRunner::External);
    runner.setExternalElapsedTime(500);
    runner.write(m_doc->masterTimer());

    // The function should have been started (it's in the running queue)
    QCOMPARE(runner.m_runningQueue.count(), 1);
    QCOMPARE(runner.m_elapsedTime, quint32(500));

    // Cleanup: stop
    runner.stop();
    m_doc->deleteFunction(scene->id());
    m_doc->deleteFunction(show->id());
}

void ShowRunner_Test::externalSyncForwardJump()
{
    // C3: function at 2000ms, jump external time from 0 to 2500ms in one step
    Show *show = new Show(m_doc);
    m_doc->addFunction(show);
    Scene *scene = new Scene(m_doc);
    m_doc->addFunction(scene);
    Track *track = new Track(scene->id());
    ShowFunction *sf = new ShowFunction(show->getLatestShowFunctionId());
    sf->setFunctionID(scene->id());
    sf->setStartTime(2000);
    sf->setDuration(1000);
    track->addShowFunction(sf);
    show->addTrack(track);

    ShowRunner runner(m_doc, show->id());
    runner.setSyncSource(ShowRunner::External);

    // Jump directly to 2500ms
    runner.setExternalElapsedTime(2500);
    runner.write(m_doc->masterTimer());

    // Function at 2000ms should have started
    QCOMPARE(runner.m_runningQueue.count(), 1);
    QCOMPARE(runner.m_currentTimeFunctionIndex, 1);

    runner.stop();
    m_doc->deleteFunction(scene->id());
    m_doc->deleteFunction(show->id());
}

void ShowRunner_Test::externalSyncBackwardSeek()
{
    // C4: advance to 3000ms (starting function at 2000ms), then seek to 500ms
    Show *show = new Show(m_doc);
    m_doc->addFunction(show);
    Scene *scene = new Scene(m_doc);
    m_doc->addFunction(scene);
    Track *track = new Track(scene->id());
    ShowFunction *sf = new ShowFunction(show->getLatestShowFunctionId());
    sf->setFunctionID(scene->id());
    sf->setStartTime(2000);
    sf->setDuration(2000);
    track->addShowFunction(sf);
    show->addTrack(track);

    ShowRunner runner(m_doc, show->id());
    runner.setSyncSource(ShowRunner::External);

    // First advance to 3000ms — function at 2000ms should start
    runner.setExternalElapsedTime(3000);
    runner.write(m_doc->masterTimer());
    QCOMPARE(runner.m_runningQueue.count(), 1);
    QCOMPARE(runner.m_currentTimeFunctionIndex, 1);

    // Now seek backward to 500ms — function should be stopped, index reset
    runner.setExternalElapsedTime(500);
    runner.write(m_doc->masterTimer());
    QCOMPARE(runner.m_runningQueue.count(), 0);
    QCOMPARE(runner.m_currentTimeFunctionIndex, 0);
    QCOMPARE(runner.m_elapsedTime, quint32(500));

    runner.stop();
    m_doc->deleteFunction(scene->id());
    m_doc->deleteFunction(show->id());
}

void ShowRunner_Test::localSeek_data()
{
    QTest::addColumn<quint32>("startTime");
    QTest::addColumn<quint32>("destination");
    QTest::addColumn<int>("activeCue");
    QTest::addColumn<int>("ticksToFutureCue");
    QTest::newRow("forward") << quint32(0) << quint32(1500) << 1 << 25;
    QTest::newRow("backward") << quint32(1500) << quint32(500) << 0 << 75;
}

void ShowRunner_Test::localSeek()
{
    QFETCH(quint32, startTime);
    QFETCH(quint32, destination);
    QFETCH(int, activeCue);
    QFETCH(int, ticksToFutureCue);

    Doc doc(nullptr);
    auto *timer = doc.masterTimer();
    auto *fixture = new Fixture(&doc);
    fixture->setUniverse(0);
    fixture->setAddress(0);
    fixture->setChannels(2);
    doc.addFixture(fixture);

    auto *show = new Show(&doc);
    doc.addFunction(show);
    auto *track = new Track(Function::invalidId());
    Scene *cues[] = {new Scene(&doc), new Scene(&doc), new Scene(&doc)};
    const quint32 cueStarts[] = {0, 1000, 2000};
    const uchar cueValues[] = {51, 137, 223};
    for (int i = 0; i < 3; ++i)
    {
        cues[i]->setValue(fixture->id(), 0, cueValues[i]);
        doc.addFunction(cues[i]);
        auto *item = new ShowFunction(show->getLatestShowFunctionId());
        item->setFunctionID(cues[i]->id());
        item->setStartTime(cueStarts[i]);
        item->setDuration(1000);
        track->addShowFunction(item);
    }
    show->addTrack(track);

    auto *independent = new Scene(&doc);
    independent->setValue(fixture->id(), 1, 77);
    doc.addFunction(independent);
    independent->start(timer, FunctionParent::master());
    show->start(timer, FunctionParent::master(), startTime);
    timer->timerTick();

    QSignalSpy position(show, &Show::timeChanged);
    show->requestSeek(destination);
    timer->timerTick();

    QList<Universe *> universes = doc.inputOutputMap()->claimUniverses();
    universes[0]->processFaders(MasterTimer::tick());
    const uchar seekOutput = universes[0]->preGMValue(0);
    const uchar independentSeekOutput = universes[0]->preGMValue(1);
    doc.inputOutputMap()->releaseUniverses(false);

    const quint32 seekPosition = position.last().first().toUInt();
    QList<bool> runningAfterSeek;
    for (int i = 0; i < 3; ++i)
        runningAfterSeek.append(cues[i]->isRunning());
    const quint32 activeElapsed = cues[activeCue]->elapsed();

    for (int i = 0; i < ticksToFutureCue; ++i)
        timer->timerTick();

    universes = doc.inputOutputMap()->claimUniverses();
    universes[0]->processFaders(MasterTimer::tick());
    const uchar futureOutput = universes[0]->preGMValue(0);
    const uchar independentFutureOutput = universes[0]->preGMValue(1);
    doc.inputOutputMap()->releaseUniverses(false);
    const bool futureRunning = cues[2]->isRunning();
    const bool independentRunning = independent->isRunning();

    show->stop(FunctionParent::master());
    independent->stop(FunctionParent::master());
    timer->timerTick();

    QCOMPARE(seekOutput, cueValues[activeCue]);
    QCOMPARE(independentSeekOutput, uchar(77));
    QCOMPARE(seekPosition, destination + MasterTimer::tick());
    for (int i = 0; i < 3; ++i)
        QCOMPARE(runningAfterSeek[i], i == activeCue);
    QCOMPARE(activeElapsed, quint32(520));
    QCOMPARE(futureOutput, uchar(223));
    QCOMPARE(independentFutureOutput, uchar(77));
    QVERIFY(futureRunning);
    QVERIFY(independentRunning);
}

void ShowRunner_Test::localSeekReusesActiveFunction_data()
{
    QTest::addColumn<quint32>("startTime");
    QTest::addColumn<quint32>("destination");
    QTest::addColumn<quint32>("cueStart");

    QTest::newRow("same-cue-forward") << quint32(100) << quint32(700) << quint32(0);
    QTest::newRow("same-cue-backward") << quint32(700) << quint32(100) << quint32(0);
    QTest::newRow("reused-cue-forward") << quint32(500) << quint32(1500) << quint32(1000);
    QTest::newRow("reused-cue-backward") << quint32(1500) << quint32(500) << quint32(0);
}

void ShowRunner_Test::localSeekReusesActiveFunction()
{
    QFETCH(quint32, startTime);
    QFETCH(quint32, destination);
    QFETCH(quint32, cueStart);

    Doc doc(nullptr);
    auto *timer = doc.masterTimer();
    QCOMPARE(doc.inputOutputMap()->outputPatchesCount(0), 0);

    auto *fixture = new Fixture(&doc);
    fixture->setUniverse(0);
    fixture->setAddress(0);
    fixture->setChannels(1);
    doc.addFixture(fixture);

    auto *scene = new Scene(&doc);
    scene->setValue(fixture->id(), 0, 200);
    doc.addFunction(scene);
    auto *show = new Show(&doc);
    doc.addFunction(show);
    auto *track = new Track(Function::invalidId());
    for (quint32 itemStart : {quint32(0), quint32(1000)})
    {
        auto *item = new ShowFunction(show->getLatestShowFunctionId());
        item->setFunctionID(scene->id());
        item->setStartTime(itemStart);
        item->setDuration(1000);
        track->addShowFunction(item);
    }
    show->addTrack(track);
    show->adjustAttribute(0.4, 0);
    show->start(timer, FunctionParent::master(), startTime);
    timer->timerTick();

    QSignalSpy position(show, &Show::timeChanged);
    show->requestSeek(destination);
    timer->timerTick();

    QList<Universe *> universes = doc.inputOutputMap()->claimUniverses();
    Universe *universe = universes[0];
    const int stoppingFaderCount = universe->faders().count();
    universe->processFaders(MasterTimer::tick());
    doc.inputOutputMap()->releaseUniverses(false);

    timer->timerTick();
    universes = doc.inputOutputMap()->claimUniverses();
    const int restartedFaderCount = universe->faders().count();
    universe->processFaders(MasterTimer::tick());
    const uchar output = universe->preGMValue(0);
    doc.inputOutputMap()->releaseUniverses(false);

    const quint32 elapsed = scene->elapsed();
    const quint32 showPosition = position.last().first().toUInt();
    show->stop(FunctionParent::master());
    timer->timerTick();

    const QString actual = QStringLiteral("elapsed=%1 position=%2 output=%3 faders=%4/%5")
            .arg(elapsed).arg(showPosition).arg(output)
            .arg(stoppingFaderCount).arg(restartedFaderCount);
    QVERIFY2(elapsed == destination - cueStart + MasterTimer::tick()
             && showPosition == destination + MasterTimer::tick()
             && output == uchar(80)
             && stoppingFaderCount == 1
             && restartedFaderCount == 1,
             qPrintable(actual));
}

void ShowRunner_Test::localSeekPreservesOtherOwner()
{
    Doc doc(nullptr);
    auto *timer = doc.masterTimer();
    QCOMPARE(doc.inputOutputMap()->outputPatchesCount(0), 0);

    auto *fixture = new Fixture(&doc);
    fixture->setUniverse(0);
    fixture->setAddress(0);
    fixture->setChannels(1);
    doc.addFixture(fixture);

    auto *scene = new Scene(&doc);
    scene->setValue(fixture->id(), 0, 200);
    doc.addFunction(scene);
    auto *show = new Show(&doc);
    doc.addFunction(show);
    auto *track = new Track(Function::invalidId());
    auto *item = new ShowFunction(show->getLatestShowFunctionId());
    item->setFunctionID(scene->id());
    item->setStartTime(0);
    item->setDuration(1000);
    track->addShowFunction(item);
    show->addTrack(track);
    show->adjustAttribute(0.4, 0);

    const FunctionParent manualOwner(FunctionParent::AutoVCWidget, 42);
    scene->start(timer, manualOwner);
    timer->timerTick();
    show->start(timer, FunctionParent::master());
    timer->timerTick();
    const quint32 elapsedBeforeSeek = scene->elapsed();

    QSignalSpy started(scene, &Function::running);
    QSignalSpy stopped(scene, SIGNAL(stopped(quint32)));
    QSignalSpy position(show, &Show::timeChanged);
    show->requestSeek(500);
    timer->timerTick();

    QList<Universe *> universes = doc.inputOutputMap()->claimUniverses();
    Universe *universe = universes[0];
    universe->processFaders(MasterTimer::tick());
    const uchar output = universe->preGMValue(0);
    const int faderCount = universe->faders().count();
    doc.inputOutputMap()->releaseUniverses(false);
    const quint32 elapsedAfterSeek = scene->elapsed();
    const quint32 showPosition = position.last().first().toUInt();
    const int startsDuringSeek = started.count();
    const int stopsDuringSeek = stopped.count();

    show->stop(FunctionParent::master());
    timer->timerTick();
    const bool runningAfterShowStop = scene->isRunning();
    scene->stop(manualOwner);
    timer->timerTick();

    QCOMPARE(elapsedAfterSeek, elapsedBeforeSeek + MasterTimer::tick());
    QCOMPARE(showPosition, quint32(500 + MasterTimer::tick()));
    QCOMPARE(output, uchar(80));
    QCOMPARE(faderCount, 1);
    QCOMPARE(startsDuringSeek, 0);
    QCOMPARE(stopsDuringSeek, 0);
    QVERIFY(runningAfterShowStop);
}

void ShowRunner_Test::localSeekDoesNotOverrideExternalControl()
{
    Doc doc(nullptr);
    auto *timer = doc.masterTimer();
    auto *fixture = new Fixture(&doc);
    fixture->setUniverse(0);
    fixture->setAddress(0);
    fixture->setChannels(1);
    doc.addFixture(fixture);

    auto *show = new Show(&doc);
    doc.addFunction(show);
    auto *track = new Track(Function::invalidId());
    Scene *cues[] = {new Scene(&doc), new Scene(&doc)};
    const uchar cueValues[] = {51, 137};
    for (int i = 0; i < 2; ++i)
    {
        cues[i]->setValue(fixture->id(), 0, cueValues[i]);
        doc.addFunction(cues[i]);
        auto *item = new ShowFunction(show->getLatestShowFunctionId());
        item->setFunctionID(cues[i]->id());
        item->setStartTime(i * 1000);
        item->setDuration(1000);
        track->addShowFunction(item);
    }
    show->addTrack(track);
    show->setSyncSource(ShowRunner::External);
    show->start(timer, FunctionParent::master());
    timer->timerTick();
    show->setExternalElapsedTime(500);
    timer->timerTick();

    QSignalSpy position(show, &Show::timeChanged);
    show->requestSeek(1500);
    timer->timerTick();
    const quint32 externalPosition = position.last().first().toUInt();
    const bool firstCueUnderExternalControl = cues[0]->isRunning();
    const bool secondCueUnderExternalControl = cues[1]->isRunning();

    show->setSyncSource(ShowRunner::Autonomous);
    timer->timerTick();
    QList<Universe *> universes = doc.inputOutputMap()->claimUniverses();
    universes[0]->processFaders(MasterTimer::tick());
    const uchar output = universes[0]->preGMValue(0);
    doc.inputOutputMap()->releaseUniverses(false);
    const quint32 autonomousPosition = position.last().first().toUInt();
    const bool firstCueAfterRelease = cues[0]->isRunning();
    const bool secondCueAfterRelease = cues[1]->isRunning();

    show->stop(FunctionParent::master());
    timer->timerTick();

    QCOMPARE(externalPosition, quint32(500));
    QVERIFY(firstCueUnderExternalControl);
    QVERIFY(!secondCueUnderExternalControl);
    QCOMPARE(autonomousPosition, quint32(520));
    QVERIFY(firstCueAfterRelease);
    QVERIFY(!secondCueAfterRelease);
    QCOMPARE(output, uchar(51));
}

void ShowRunner_Test::internalBeatClockUsesSongBpm()
{
    // With NO global beat source (MasterTimer defaults to None), a beat-based Show
    // must still advance, driven by the Show's own BPM. At 120 BPM a beat lasts
    // 500ms; the MasterTimer tick is 20ms (50Hz), so 1 beat == 25 ticks.
    QCOMPARE(m_doc->masterTimer()->beatSourceType(), MasterTimer::None);

    Show *show = new Show(m_doc);
    show->setTempoType(Function::Beats);
    show->setTimeDivisionBPM(120);
    m_doc->addFunction(show);

    Scene *scene = new Scene(m_doc);
    scene->setTempoType(Function::Beats);
    m_doc->addFunction(scene);

    Track *track = new Track(scene->id());
    ShowFunction *sf = new ShowFunction(show->getLatestShowFunctionId());
    sf->setFunctionID(scene->id());
    sf->setStartTime(1000);   // beat 1 (1000 == 1 beat)
    sf->setDuration(1000);
    track->addShowFunction(sf);
    show->addTrack(track);

    ShowRunner runner(m_doc, show->id());
    QCOMPARE(runner.m_beatFunctions.count(), 1);

    // 24 ticks => 480ms => 0.96 beat => function at beat 1 NOT yet started
    for (int i = 0; i < 24; i++)
        runner.write(m_doc->masterTimer());
    QVERIFY(runner.m_elapsedBeats < quint32(1000));
    QCOMPARE(runner.m_runningQueue.count(), 0);

    // 25th tick => 500ms => exactly 1 beat => function starts
    runner.write(m_doc->masterTimer());
    QCOMPARE(runner.m_elapsedBeats, quint32(1000));
    QCOMPARE(runner.m_runningQueue.count(), 1);

    runner.stop();
    QCOMPARE(runner.m_elapsedBeats, quint32(0));
    m_doc->deleteFunction(scene->id());
    m_doc->deleteFunction(show->id());
}

void ShowRunner_Test::internalBeatClockScalesWithSongBpm()
{
    // The internal clock rate is proportional to the Show BPM: after the same
    // number of ticks, a 120-BPM show has advanced twice as many beats as a
    // 60-BPM show (so a 60-BPM song played faster/slower scales correctly).
    QCOMPARE(m_doc->masterTimer()->beatSourceType(), MasterTimer::None);

    auto runBeats = [this](int bpm) -> quint32
    {
        Show *show = new Show(m_doc);
        show->setTempoType(Function::Beats);
        show->setTimeDivisionBPM(bpm);
        m_doc->addFunction(show);

        ShowRunner runner(m_doc, show->id());
        for (int i = 0; i < 25; i++)      // 25 ticks == 500ms
            runner.write(m_doc->masterTimer());
        quint32 beats = runner.m_elapsedBeats;
        runner.stop();
        m_doc->deleteFunction(show->id());
        return beats;
    };

    quint32 beats120 = runBeats(120); // 500ms @120bpm => 1 beat  => 1000
    quint32 beats60  = runBeats(60);  // 500ms @60bpm  => 0.5 beat => 500

    QCOMPARE(beats120, quint32(1000));
    QCOMPARE(beats60,  quint32(500));
    QCOMPARE(beats120, beats60 * 2);
}

void ShowRunner_Test::mixedTimelines_data()
{
    QTest::addColumn<int>("division");
    QTest::addColumn<int>("timeDuration");
    QTest::newRow("time-beats-finish-last") << int(Show::Time) << 100;
    QTest::newRow("beats-beats-finish-last") << int(Show::BPM_4_4) << 100;
    QTest::newRow("time-ms-finish-last") << int(Show::Time) << 1000;
    QTest::newRow("beats-ms-finish-last") << int(Show::BPM_4_4) << 1000;
}

void ShowRunner_Test::mixedTimelines()
{
    QFETCH(int, division);
    QFETCH(int, timeDuration);
    Doc doc(nullptr);
    auto *timer = doc.masterTimer();
    timer->setBeatSourceType(MasterTimer::External);
    auto *show = new Show(&doc);
    doc.addFunction(show);
    show->setTimeDivisionType(Show::TimeDivision(division));
    Scene *scenes[] = {new Scene(&doc), new Scene(&doc)};
    for (int i = 0; i < 2; ++i)
    {
        doc.addFunction(scenes[i]);
        scenes[i]->setTempoType(i ? Function::Beats : Function::Time);
        auto *track = new Track(scenes[i]->id());
        auto *item = new ShowFunction(show->getLatestShowFunctionId());
        item->setFunctionID(scenes[i]->id());
        item->setStartTime(0);
        item->setDuration(i ? 2000 : timeDuration);
        track->addShowFunction(item);
        show->addTrack(track);
    }
    ShowRunner runner(&doc, show->id());
    QSignalSpy finished(&runner, &ShowRunner::showFinished);
    QSignalSpy position(&runner, &ShowRunner::timeChanged);
    for (int i = 0; i < 10; ++i)
        runner.write(timer);
    QCOMPARE(runner.m_elapsedTime, quint32(200));
    QCOMPARE(runner.m_currentTimeFunctionIndex, 1);
    QCOMPARE(runner.m_currentBeatFunctionIndex, 0);
    QCOMPARE(finished.count(), 0);
    QCOMPARE(position.count(), division == Show::Time ? 10 : 0);
    timer->requestBeat();
    runner.write(timer);
    QCOMPARE(runner.m_currentBeatFunctionIndex, 1);
    QCOMPARE(position.last().first().toUInt(), division == Show::Time ? uint(220) : uint(20));
    runner.write(timer);
    QCOMPARE(finished.count(), 0);
    runner.write(timer);
    QCOMPARE(finished.count(), timeDuration == 100 ? 1 : 0);
    if (timeDuration == 1000)
    {
        while (finished.isEmpty())
            runner.write(timer);
        QCOMPARE(runner.m_elapsedTime, quint32(1000));
    }
    QCOMPARE(scenes[0]->tempoType(), Function::Time);
    QCOMPARE(scenes[1]->tempoType(), Function::Beats);
    runner.stop();
}

void ShowRunner_Test::timeOnlyCursor_data()
{
    QTest::addColumn<int>("division");
    QTest::addColumn<bool>("globalClock");
    QTest::addColumn<bool>("mutedBeatTrack");
    QTest::addColumn<quint32>("start");
    for (auto division : {Show::Time, Show::BPM_4_4})
        for (bool globalClock : {false, true})
            for (bool muted : {false, true})
                for (quint32 start : {0u, 200u})
                    QTest::newRow(qPrintable(QString("%1-clock%2-muted%3-start%4")
                                            .arg(int(division)).arg(globalClock).arg(muted).arg(start)))
                            << int(division) << globalClock << muted << start;
}

void ShowRunner_Test::timeOnlyCursor()
{
    QFETCH(int, division);
    QFETCH(bool, globalClock);
    QFETCH(bool, mutedBeatTrack);
    QFETCH(quint32, start);
    Doc doc(nullptr);
    auto *timer = doc.masterTimer();
    timer->setBeatSourceType(globalClock ? MasterTimer::External : MasterTimer::None);
    auto *show = new Show(&doc);
    doc.addFunction(show);
    show->setTimeDivision(Show::TimeDivision(division), 90);
    for (int i = 0; i < (mutedBeatTrack ? 2 : 1); ++i)
    {
        auto *scene = new Scene(&doc);
        scene->setTempoType(i ? Function::Beats : Function::Time);
        doc.addFunction(scene);
        auto *track = new Track(scene->id());
        track->setMute(i != 0);
        auto *item = new ShowFunction(show->getLatestShowFunctionId());
        item->setFunctionID(scene->id());
        item->setStartTime(0);
        item->setDuration(i ? 10000 : start + 5 * MasterTimer::tick());
        track->addShowFunction(item);
        show->addTrack(track);
    }
    ShowRunner runner(&doc, show->id(), start);
    QSignalSpy position(&runner, &ShowRunner::timeChanged);
    QSignalSpy finished(&runner, &ShowRunner::showFinished);
    for (int i = 0; i < 5; ++i)
    {
        if (globalClock && i == 2)
            timer->requestBeat();
        runner.write(timer);
    }
    runner.write(timer);
    QCOMPARE(finished.count(), 1);
    QCOMPARE(position.count(), 5);
    for (int i = 0; i < position.count(); ++i)
        QCOMPARE(position[i].first().toUInt(), start + (i + 1) * MasterTimer::tick());
    runner.stop();
}

void ShowRunner_Test::resumeMixedTimelines_data()
{
    QTest::addColumn<bool>("globalClock");
    QTest::addColumn<int>("songBpm");
    QTest::addColumn<int>("liveBpm");
    QTest::addColumn<quint32>("beatStart");
    QTest::newRow("global-slow-resume") << true << 120 << 30 << quint32(500);
    QTest::newRow("song-resume") << false << 60 << 120 << quint32(1800);
    QTest::newRow("zero-song-fallback") << false << 0 << 120 << quint32(3800);
}

void ShowRunner_Test::resumeMixedTimelines()
{
    QFETCH(bool, globalClock);
    QFETCH(int, songBpm);
    QFETCH(int, liveBpm);
    QFETCH(quint32, beatStart);
    Doc doc(nullptr);
    auto *timer = doc.masterTimer();
    timer->requestBpmNumber(liveBpm);
    timer->setBeatSourceType(globalClock ? MasterTimer::External : MasterTimer::None);
    auto *show = new Show(&doc);
    doc.addFunction(show);
    show->setTimeDivisionBPM(songBpm);
    auto *scene = new Scene(&doc);
    scene->setTempoType(Function::Beats);
    doc.addFunction(scene);
    auto *track = new Track(scene->id());
    auto *item = new ShowFunction(show->getLatestShowFunctionId());
    item->setFunctionID(scene->id());
    item->setStartTime(beatStart);
    item->setDuration(900);
    track->addShowFunction(item);
    show->addTrack(track);
    ShowRunner runner(&doc, show->id(), 2000);
    if (globalClock)
        timer->requestBeat();
    runner.write(timer);
    QCOMPARE(runner.m_runningQueue.count(), 1);
    QCOMPARE(scene->tempoType(), Function::Beats);
    runner.stop();
}

void ShowRunner_Test::externalMixedTimeline_data()
{
    QTest::addColumn<bool>("globalClock");
    QTest::newRow("global-beats") << true;
    QTest::newRow("no-source") << false;
}

void ShowRunner_Test::externalMixedTimeline()
{
    QFETCH(bool, globalClock);
    Doc doc(nullptr);
    auto *timer = doc.masterTimer();
    timer->setBeatSourceType(globalClock ? MasterTimer::External : MasterTimer::None);
    auto *show = new Show(&doc);
    doc.addFunction(show);
    show->setTimeDivisionBPM(60);
    Scene *scenes[] = {new Scene(&doc), new Scene(&doc)};
    for (int i = 0; i < 2; ++i)
    {
        doc.addFunction(scenes[i]);
        scenes[i]->setTempoType(i ? Function::Beats : Function::Time);
        auto *track = new Track(scenes[i]->id());
        auto *sf = new ShowFunction(show->getLatestShowFunctionId());
        sf->setFunctionID(scenes[i]->id());
        sf->setStartTime(i ? 2000 : 0);
        sf->setDuration(i ? 3000 : 1000);
        track->addShowFunction(sf);
        show->addTrack(track);
    }
    ShowRunner runner(&doc, show->id(), 3000);
    runner.setSyncSource(ShowRunner::External);
    QSignalSpy position(&runner, &ShowRunner::timeChanged);
    runner.setExternalElapsedTime(3000);
    runner.write(timer);
    QCOMPARE(runner.m_runningQueue.count(), 1);
    QCOMPARE(runner.m_runningQueue.first().first, scenes[1]);
    runner.setPause(true);
    timer->requestBeat();
    runner.write(timer);
    QCOMPARE(position.last().first().toUInt(), uint(3000));
    QCOMPARE(runner.m_elapsedBeats, uint(3000));
    runner.setPause(false);
    runner.setExternalElapsedTime(500);
    runner.write(timer);
    QCOMPARE(runner.m_runningQueue.count(), 1);
    QCOMPARE(runner.m_runningQueue.first().first, scenes[0]);
    QCOMPARE(position.last().first().toUInt(), uint(500));
    runner.stop();
}

void ShowRunner_Test::nativeAudioSuppression_data()
{
    QTest::addColumn<bool>("manualOwner");
    QTest::newRow("last-show-owner") << false;
    QTest::newRow("manual-co-owner") << true;
}

void ShowRunner_Test::nativeAudioSuppression()
{
    QFETCH(bool, manualOwner);
    Doc doc(nullptr);
    auto *show = new Show(&doc);
    doc.addFunction(show);
    auto *audio = new Audio(&doc);
    audio->setFadeOutSpeed(5000);
    doc.addFunction(audio);
    auto *track = new Track(audio->id());
    auto *item = new ShowFunction(show->getLatestShowFunctionId());
    item->setFunctionID(audio->id());
    item->setStartTime(0);
    item->setDuration(10000);
    track->addShowFunction(item);
    show->addTrack(track);

    ShowRunner runner(&doc, show->id());
    const FunctionParent showParent(FunctionParent::Function, show->id());
    audio->m_sources.append(showParent);
    if (manualOwner)
        audio->m_sources.append(FunctionParent(FunctionParent::ManualVCWidget, 77));
    audio->m_running = true;
    audio->m_stop = false;
    auto *renderer = attachNullRenderer(audio, true);
    QTRY_VERIFY_WITH_TIMEOUT(renderer->isRunning(), 1000);
    runner.m_runningQueue.append(qMakePair(static_cast<Function *>(audio), quint32(10000)));
    runner.m_currentTimeFunctionIndex = runner.m_timeFunctions.count();

    show->setPerformAudioSuppressed(true);
    runner.write(doc.masterTimer());

    QCOMPARE(runner.m_runningQueue.count(), 0);
    QCOMPARE(audio->stopped(), !manualOwner);
    QCOMPARE(audio->m_audio_out == nullptr, !manualOwner);
    if (manualOwner)
        QVERIFY(renderer->isRunning());
    else
        QVERIFY(!renderer->isRunning());

    audio->stop(FunctionParent::master());
    if (audio->m_audio_out != nullptr)
        audio->m_audio_out->stop();
}

void ShowRunner_Test::pausedPerformAdoptionKeepsAudioPaused()
{
    Doc doc(nullptr);
    auto *show = new Show(&doc);
    doc.addFunction(show);
    auto *audio = new Audio(&doc);
    audio->setFadeOutSpeed(5000);
    doc.addFunction(audio);
    auto *scene = new Scene(&doc);
    doc.addFunction(scene);
    for (Function *function : {static_cast<Function *>(audio), static_cast<Function *>(scene)})
    {
        auto *track = new Track(function->id());
        auto *item = new ShowFunction(show->getLatestShowFunctionId());
        item->setFunctionID(function->id());
        item->setStartTime(0);
        item->setDuration(10000);
        track->addShowFunction(item);
        show->addTrack(track);
    }

    ShowRunner runner(&doc, show->id());
    const FunctionParent showParent(FunctionParent::Function, show->id());
    auto *renderer = attachNullRenderer(audio, true);
    QTRY_VERIFY_WITH_TIMEOUT(renderer->isRunning(), 1000);
    for (Function *function : {static_cast<Function *>(audio), static_cast<Function *>(scene)})
    {
        function->m_sources.append(showParent);
        function->m_running = true;
        function->m_stop = false;
        function->setPause(true);
        runner.m_runningQueue.append(qMakePair(function, quint32(10000)));
    }
    runner.m_currentTimeFunctionIndex = runner.m_timeFunctions.count();

    show->setPerformAudioSuppressed(true);
    runner.setPause(false);

    QVERIFY(audio->isPaused());
    QVERIFY(!scene->isPaused());
    runner.write(doc.masterTimer());
    QCOMPARE(audio->m_audio_out, nullptr);
    QVERIFY(!renderer->isRunning());
    QCOMPARE(runner.m_runningQueue.count(), 1);
    QCOMPARE(runner.m_runningQueue.first().first, scene);
    runner.stop();
}

void ShowRunner_Test::nativeAudioNormalStopPreservesFade()
{
    Doc doc(nullptr);
    auto *audio = new Audio(&doc);
    audio->setFadeOutSpeed(5000);
    doc.addFunction(audio);
    const FunctionParent parent(FunctionParent::Function, 42);
    audio->m_sources.append(parent);
    audio->m_running = true;
    audio->m_stop = false;
    auto *renderer = attachNullRenderer(audio, false);

    audio->stop(parent);
    audio->postRun(doc.masterTimer(), {});

    QCOMPARE(audio->fadeOutSpeed(), uint(5000));
    QCOMPARE(audio->m_audio_out, renderer);
    QVERIFY(renderer->m_fadeStep < 0);
    QVERIFY(!renderer->m_userStop);
}

/*****************************************************************************
 * Command track playback
 *****************************************************************************/

namespace
{

Fixture *makeFixture(Doc &doc, int channels)
{
    auto *fixture = new Fixture(&doc);
    fixture->setUniverse(0);
    fixture->setAddress(0);
    fixture->setChannels(quint32(channels));
    doc.addFixture(fixture);
    return fixture;
}

Scene *makeScene(Doc &doc, quint32 fixtureId, quint32 channel, uchar value)
{
    auto *scene = new Scene(&doc);
    scene->setValue(fixtureId, channel, value);
    doc.addFunction(scene);
    return scene;
}

uchar sampleChannel(Doc &doc, int channel)
{
    QList<Universe *> universes = doc.inputOutputMap()->claimUniverses();
    universes[0]->processFaders(MasterTimer::tick());
    const uchar value = universes[0]->preGMValue(channel);
    doc.inputOutputMap()->releaseUniverses(false);
    return value;
}

/** Ticks needed for the runner to process an elapsed time of exactly ms */
int ticksToReach(quint32 ms)
{
    return int(ms / MasterTimer::tick()) + 1;
}

}

void ShowRunner_Test::commandPlaybackAppliesAuthoredOrder()
{
    Doc doc(nullptr);
    auto *timer = doc.masterTimer();
    auto *fixture = makeFixture(doc, 1);
    Scene *scene = makeScene(doc, fixture->id(), 0, 200);

    auto *show = new Show(&doc);
    doc.addFunction(show);

    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::start(1, 0, scene->id())));
    QVERIFY(track.insert(ShowCommand::setIntensity(2, 0, scene->id(), 0.5)));
    QVERIFY(track.insert(ShowCommand::setIntensity(3, 100, scene->id(), 0.25)));
    QVERIFY(track.insert(ShowCommand::stop(4, 200, scene->id())));
    track.setExtent(400);
    QString error;
    QVERIFY2(show->setCommandTrack(track, {}, &error), qPrintable(error));

    QSignalSpy finished(show, &Show::showFinished);
    int ticks = 0;
    auto advanceTo = [&](quint32 ms) {
        const int target = ticksToReach(ms);
        while (ticks < target)
        {
            timer->timerTick();
            ticks++;
        }
    };

    show->start(timer, FunctionParent::master());
    advanceTo(0);
    const bool startedAtZero = scene->isRunning();
    const uchar firstValue = sampleChannel(doc, 0);

    advanceTo(100);
    const uchar secondValue = sampleChannel(doc, 0);

    advanceTo(240);
    const bool runningAfterStop = scene->isRunning();
    const int finishedBeforeExtent = finished.count();

    advanceTo(400);
    const int finishedAtExtent = finished.count();

    show->stop(FunctionParent::master());
    timer->timerTick();

    QVERIFY2(startedAtZero, "a Start command must start its target through the Show");
    QCOMPARE(firstValue, uchar(100));  // 200 * 0.5
    QCOMPARE(secondValue, uchar(50));  // 200 * 0.25
    QCOMPARE(runningAfterStop, false);
    QCOMPARE(finishedBeforeExtent, 0);
    QCOMPARE(finishedAtExtent, 1);
}

void ShowRunner_Test::commandCollectionControlsItsChildren()
{
    Doc doc(nullptr);
    auto *timer = doc.masterTimer();
    auto *fixture = makeFixture(doc, 2);
    Scene *first = makeScene(doc, fixture->id(), 0, 200);
    Scene *second = makeScene(doc, fixture->id(), 1, 120);
    auto *collection = new Collection(&doc);
    collection->addFunction(first->id());
    collection->addFunction(second->id());
    doc.addFunction(collection);
    auto *show = new Show(&doc);
    doc.addFunction(show);
    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::start(1, 0, collection->id())));
    QVERIFY(track.insert(ShowCommand::setIntensity(2, 0, collection->id(), 0.5)));
    QVERIFY(track.insert(ShowCommand::stop(3, 400, collection->id())));
    QVERIFY(track.setExtent(600));
    QVERIFY(show->setCommandTrack(track));

    show->start(timer, FunctionParent::master());
    for (int i = 0; i < ticksToReach(100); ++i)
        timer->timerTick();
    const bool bothStarted = first->isRunning() && second->isRunning();
    const uchar firstValue = sampleChannel(doc, 0);
    const uchar secondValue = sampleChannel(doc, 1);
    for (int i = ticksToReach(100); i < ticksToReach(500); ++i)
        timer->timerTick();
    const bool anyStillRunning = collection->isRunning() ||
                                 first->isRunning() || second->isRunning();
    show->stop(FunctionParent::master());
    timer->timerTick();

    QVERIFY(bothStarted);
    QCOMPARE(firstValue, uchar(100));
    QCOMPARE(secondValue, uchar(60));
    QVERIFY(!anyStillRunning);
    QCOMPARE(show->commandTrack().referencedFunctionIds(), QList<quint32>{collection->id()});
}

void ShowRunner_Test::commandExternalAdvanceAppliesCrossedEvents_data()
{
    QTest::addColumn<QList<quint32>>("positions");
    QTest::newRow("single-forward-jump-or-delayed-update") << QList<quint32>{400};
    QTest::newRow("incremental-updates") << QList<quint32>{100, 150, 200, 250, 300, 350, 400};
}

void ShowRunner_Test::commandExternalAdvanceAppliesCrossedEvents()
{
    QFETCH(QList<quint32>, positions);
    Doc doc(nullptr);
    auto *timer = doc.masterTimer();
    auto *fixture = makeFixture(doc, 3);
    Scene *sustained = makeScene(doc, fixture->id(), 0, 200);
    Scene *released = makeScene(doc, fixture->id(), 1, 200);
    Scene *future = makeScene(doc, fixture->id(), 2, 200);
    auto *show = new Show(&doc);
    doc.addFunction(show);
    show->setSyncSource(ShowRunner::External);

    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::start(1, 100, sustained->id())));
    QVERIFY(track.insert(ShowCommand::setIntensity(2, 150, sustained->id(), 0.25)));
    QVERIFY(track.insert(ShowCommand::start(3, 200, released->id())));
    QVERIFY(track.insert(ShowCommand::setIntensity(4, 300, sustained->id(), 0.6)));
    QVERIFY(track.insert(ShowCommand::stop(5, 350, released->id())));
    QVERIFY(track.insert(ShowCommand::start(6, 800, future->id())));
    QVERIFY(track.setExtent(1000));
    QVERIFY(show->setCommandTrack(track));

    show->start(timer, FunctionParent::master());
    timer->timerTick();
    for (quint32 position : positions)
    {
        show->setExternalElapsedTime(position);
        timer->timerTick();
        timer->timerTick();
    }
    const bool sustainedRunning = sustained->isRunning();
    const bool releasedRunning = released->isRunning();
    const bool futureRunning = future->isRunning();
    const uchar recordedValue = sampleChannel(doc, 0);

    sustained->requestAttributeOverride(Function::Intensity, 0.9);
    show->setExternalElapsedTime(400);
    timer->timerTick();
    show->setExternalElapsedTime(450);
    timer->timerTick();
    const uchar manualValue = sampleChannel(doc, 0);

    show->stop(FunctionParent::master());
    timer->timerTick();

    QVERIFY(sustainedRunning);
    QVERIFY(!releasedRunning);
    QVERIFY(!futureRunning);
    QCOMPARE(recordedValue, uchar(120));
    QCOMPARE(manualValue, uchar(180));
}

void ShowRunner_Test::commandExternalBackwardRestoresAuthoredValues()
{
    Doc doc(nullptr);
    auto *timer = doc.masterTimer();
    auto *fixture = makeFixture(doc, 3);
    Scene *triggered = makeScene(doc, fixture->id(), 0, 200);
    Scene *valued = makeScene(doc, fixture->id(), 1, 200);
    Scene *withoutEarlierValue = makeScene(doc, fixture->id(), 2, 200);
    auto *show = new Show(&doc);
    doc.addFunction(show);
    show->setSyncSource(ShowRunner::External);

    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::start(1, 100, triggered->id())));
    QVERIFY(track.insert(ShowCommand::setIntensity(2, 200, valued->id(), 0.25)));
    QVERIFY(track.insert(ShowCommand::setIntensity(3, 400, valued->id(), 0.75)));
    QVERIFY(track.insert(ShowCommand::setIntensity(4, 500, withoutEarlierValue->id(), 0.9)));
    QVERIFY(track.setExtent(1000));
    QVERIFY(show->setCommandTrack(track));

    const FunctionParent manualOwner(FunctionParent::AutoVCWidget, 42);
    valued->start(timer, manualOwner);
    withoutEarlierValue->start(timer, manualOwner);
    timer->timerTick();
    QSignalSpy starts(triggered, &Function::running);
    show->start(timer, FunctionParent::master());
    timer->timerTick();
    show->setExternalElapsedTime(600);
    timer->timerTick();
    timer->timerTick();
    const uchar forwardValue = sampleChannel(doc, 1);
    const int forwardStarts = starts.count();

    show->setExternalElapsedTime(300);
    timer->timerTick();
    timer->timerTick();
    const uchar restoredValue = sampleChannel(doc, 1);
    const uchar retainedValue = sampleChannel(doc, 2);
    const bool historicalTriggerRunning = triggered->isRunning();
    const int startsAfterBackward = starts.count();

    show->stop(FunctionParent::master());
    valued->stop(manualOwner);
    withoutEarlierValue->stop(manualOwner);
    timer->timerTick();

    QCOMPARE(forwardValue, uchar(150));
    QCOMPARE(forwardStarts, 1);
    QCOMPARE(restoredValue, uchar(50));
    QCOMPARE(retainedValue, uchar(180));
    QVERIFY(!historicalTriggerRunning);
    QCOMPARE(startsAfterBackward, 1);
}

void ShowRunner_Test::commandStopReleasesOnlyShowOwner()
{
    Doc doc(nullptr);
    auto *timer = doc.masterTimer();
    auto *fixture = makeFixture(doc, 1);
    Scene *scene = makeScene(doc, fixture->id(), 0, 200);

    auto *show = new Show(&doc);
    doc.addFunction(show);

    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::start(1, 0, scene->id())));
    QVERIFY(track.insert(ShowCommand::stop(2, 100, scene->id())));
    track.setExtent(400);
    QVERIFY(show->setCommandTrack(track));

    const FunctionParent manualOwner(FunctionParent::AutoVCWidget, 42);
    scene->start(timer, manualOwner);
    timer->timerTick();

    int ticks = 0;
    auto advanceTo = [&](quint32 ms) {
        const int target = ticksToReach(ms);
        while (ticks < target)
        {
            timer->timerTick();
            ticks++;
        }
    };

    show->start(timer, FunctionParent::master());
    advanceTo(200);
    const bool stillRunningForManualOwner = scene->isRunning();

    scene->stop(manualOwner);
    timer->timerTick();
    const bool stoppedWithLastOwner = scene->isRunning();

    show->stop(FunctionParent::master());
    timer->timerTick();

    QVERIFY2(stillRunningForManualOwner,
             "a recorded Stop must release only this Show's playback");
    QCOMPARE(stoppedWithLastOwner, false);
}

void ShowRunner_Test::commandStopRetiresSupersededClipDeadline()
{
    Doc doc(nullptr);
    auto *timer = doc.masterTimer();
    auto *fixture = makeFixture(doc, 1);
    Scene *scene = makeScene(doc, fixture->id(), 0, 200);

    auto *show = new Show(&doc);
    doc.addFunction(show);
    auto *track = new Track(Function::invalidId());
    auto *item = new ShowFunction(show->getLatestShowFunctionId());
    item->setFunctionID(scene->id());
    item->setStartTime(100);
    item->setDuration(200); // ordinary clip 100 - 300
    track->addShowFunction(item);
    show->addTrack(track);

    ShowCommandTrack commands;
    QVERIFY(commands.insert(ShowCommand::stop(1, 180, scene->id())));
    QVERIFY(commands.insert(ShowCommand::start(2, 220, scene->id())));
    QVERIFY(commands.insert(ShowCommand::stop(3, 350, scene->id())));
    commands.setExtent(600);
    QVERIFY(show->setCommandTrack(commands));

    int ticks = 0;
    auto advanceTo = [&](quint32 ms) {
        const int target = ticksToReach(ms);
        while (ticks < target)
        {
            timer->timerTick();
            ticks++;
        }
    };

    show->start(timer, FunctionParent::master());
    advanceTo(140);
    const bool runningFromClip = scene->isRunning();

    advanceTo(200);
    const bool stoppedByCommand = scene->isRunning();

    advanceTo(260);
    const bool restartedByCommand = scene->isRunning();

    advanceTo(320); // past the superseded clip deadline of 300
    const bool survivedOldDeadline = scene->isRunning();

    advanceTo(380);
    const bool stoppedByLastCommand = scene->isRunning();

    show->stop(FunctionParent::master());
    timer->timerTick();

    QVERIFY(runningFromClip);
    QCOMPARE(stoppedByCommand, false);
    QVERIFY(restartedByCommand);
    QVERIFY2(survivedOldDeadline,
             "the superseded clip end must not terminate a later command activation");
    QCOMPARE(stoppedByLastCommand, false);
}

void ShowRunner_Test::commandSeekRestoresValuesWithoutTriggers()
{
    Doc doc(nullptr);
    auto *timer = doc.masterTimer();
    auto *fixture = makeFixture(doc, 3);
    Scene *triggered = makeScene(doc, fixture->id(), 0, 200);
    Scene *valued = makeScene(doc, fixture->id(), 1, 200);
    Scene *atBoundary = makeScene(doc, fixture->id(), 2, 200);

    auto *show = new Show(&doc);
    doc.addFunction(show);

    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::start(1, 100, triggered->id())));
    QVERIFY(track.insert(ShowCommand::setIntensity(2, 200, valued->id(), 0.25)));
    QVERIFY(track.insert(ShowCommand::setIntensity(3, 400, valued->id(), 0.75)));
    QVERIFY(track.insert(ShowCommand::start(4, 600, atBoundary->id())));
    track.setExtent(2000);
    QVERIFY(show->setCommandTrack(track));

    // an independent owner keeps the value target running across the seek
    const FunctionParent manualOwner(FunctionParent::AutoVCWidget, 42);
    valued->start(timer, manualOwner);
    timer->timerTick();

    QSignalSpy boundaryStarts(atBoundary, &Function::running);
    show->start(timer, FunctionParent::master());
    timer->timerTick();

    show->requestSeek(600);
    timer->timerTick();

    const bool historicalStartReplayed = triggered->isRunning();
    const uchar restoredValue = sampleChannel(doc, 1);
    const int boundaryStartsAfterSeek = boundaryStarts.count();

    for (int i = 0; i < 5; i++)
        timer->timerTick();
    const int boundaryStartsLater = boundaryStarts.count();

    show->stop(FunctionParent::master());
    valued->stop(manualOwner);
    timer->timerTick();

    QCOMPARE(historicalStartReplayed, false);
    QCOMPARE(restoredValue, uchar(150));  // 200 * 0.75, the latest value before 600
    QCOMPARE(boundaryStartsAfterSeek, 1); // the command at the destination runs once
    QCOMPARE(boundaryStartsLater, 1);
}

void ShowRunner_Test::commandLiveInputDoesNotEchoButLoopReplays()
{
    Doc doc(nullptr);
    auto *timer = doc.masterTimer();
    auto *fixture = makeFixture(doc, 2);
    Scene *first = makeScene(doc, fixture->id(), 0, 200);
    Scene *second = makeScene(doc, fixture->id(), 1, 200);

    auto *show = new Show(&doc);
    doc.addFunction(show);

    ShowCommandTrack empty;
    empty.setExtent(2000);
    QVERIFY(show->setCommandTrack(empty));

    QSignalSpy firstStarts(first, &Function::running);
    QSignalSpy secondStarts(second, &Function::running);

    show->start(timer, FunctionParent::master());
    for (int i = 0; i < 3; i++)
        timer->timerTick(); // consumed through 40ms

    // the recorder authored and executed two live inputs at 50ms, published
    // separately: neither may echo back through the lagging cursor
    ShowCommandTrack live;
    live.setExtent(2000);
    QVERIFY(live.insert(ShowCommand::start(1, 50, first->id())));
    QVERIFY(show->setCommandTrack(live, {1}));
    QVERIFY(live.insert(ShowCommand::start(2, 50, second->id())));
    QVERIFY(show->setCommandTrack(live, {2}));

    for (int i = 0; i < 3; i++)
        timer->timerTick();
    const int echoedFirst = firstStarts.count();
    const int echoedSecond = secondStarts.count();

    // a later traversal must replay them
    show->requestSeek(0);
    timer->timerTick();
    for (int i = 0; i < 5; i++)
        timer->timerTick();
    const int replayedFirst = firstStarts.count();
    const int replayedSecond = secondStarts.count();

    show->stop(FunctionParent::master());
    timer->timerTick();

    QCOMPARE(echoedFirst, 0);
    QCOMPARE(echoedSecond, 0);
    QCOMPARE(replayedFirst, 1);
    QCOMPARE(replayedSecond, 1);
}

void ShowRunner_Test::commandExtentAndRecordingKeepAlive_data()
{
    QTest::addColumn<quint32>("extent");
    QTest::addColumn<bool>("recording");
    QTest::addColumn<quint32>("position");
    QTest::addColumn<bool>("expectFinished");

    QTest::newRow("legacy empty show finishes") << quint32(0) << false << quint32(0) << true;
    QTest::newRow("extent not reached") << quint32(200) << false << quint32(100) << false;
    QTest::newRow("extent reached") << quint32(200) << false << quint32(200) << true;
    QTest::newRow("recording keeps an empty show alive") << quint32(0) << true << quint32(200) << false;
}

void ShowRunner_Test::commandExtentAndRecordingKeepAlive()
{
    QFETCH(quint32, extent);
    QFETCH(bool, recording);
    QFETCH(quint32, position);
    QFETCH(bool, expectFinished);

    Doc doc(nullptr);
    auto *timer = doc.masterTimer();
    auto *show = new Show(&doc);
    doc.addFunction(show);

    if (extent > 0)
    {
        ShowCommandTrack track;
        track.setExtent(extent);
        QVERIFY(show->setCommandTrack(track));
    }
    show->setCommandRecording(recording);

    QSignalSpy finished(show, &Show::showFinished);
    show->start(timer, FunctionParent::master());
    for (int i = 0; i < ticksToReach(position); i++)
        timer->timerTick();

    const int finishedCount = finished.count();
    show->stop(FunctionParent::master());
    timer->timerTick();

    QCOMPARE(finishedCount > 0, expectFinished);
}

void ShowRunner_Test::commandIntensityAfterNaturalCompletion()
{
    Doc doc(nullptr);
    auto *timer = doc.masterTimer();
    auto *fixture = makeFixture(doc, 1);

    // a Scene with nothing to output stops itself on its first write: the
    // Show never asked it to stop, so this is a natural completion
    auto *selfCompleting = new Scene(&doc);
    doc.addFunction(selfCompleting);
    Scene *coOwned = makeScene(doc, fixture->id(), 0, 200);

    auto *show = new Show(&doc);
    doc.addFunction(show);

    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::start(1, 0, selfCompleting->id())));
    QVERIFY(track.insert(ShowCommand::setIntensity(2, 0, selfCompleting->id(), 0.5)));
    QVERIFY(track.insert(ShowCommand::setIntensity(3, 300, selfCompleting->id(), 0.4)));
    QVERIFY(track.insert(ShowCommand::setIntensity(4, 300, coOwned->id(), 0.4)));
    track.setExtent(800);
    QString error;
    QVERIFY2(show->setCommandTrack(track, {}, &error), qPrintable(error));

    // an unrelated owner keeps its own target and its own override
    const FunctionParent manualOwner(FunctionParent::AutoVCWidget, 42);
    coOwned->start(timer, manualOwner);
    timer->timerTick();
    QVERIFY(coOwned->requestAttributeOverride(Function::Intensity, 0.5)
            != Function::invalidAttributeId());

    int ticks = 0;
    auto advanceTo = [&](quint32 ms) {
        const int target = ticksToReach(ms);
        while (ticks < target)
        {
            timer->timerTick();
            ticks++;
        }
    };

    show->start(timer, FunctionParent::master());
    advanceTo(0);
    const qreal ownedIntensity = selfCompleting->getAttributeValue(Function::Intensity);

    advanceTo(200);
    const bool completedNaturally = !selfCompleting->isRunning();

    // the completed run handed its override identifier back: somebody else now
    // holds the very number this runner used
    const int recycled = selfCompleting->requestAttributeOverride(Function::Intensity, 0.5);
    auto *witness = new Scene(&doc);
    doc.addFunction(witness);
    const int firstIdOfAnyRun = witness->requestAttributeOverride(Function::Intensity, 1.0);

    advanceTo(320);
    const qreal completedIntensity = selfCompleting->getAttributeValue(Function::Intensity);
    const qreal coOwnedIntensity = coOwned->getAttributeValue(Function::Intensity);

    show->stop(FunctionParent::master());
    coOwned->stop(manualOwner);
    timer->timerTick();

    QCOMPARE(ownedIntensity, 0.5);
    QVERIFY2(completedNaturally, "the target must finish on its own");
    QCOMPARE(recycled, firstIdOfAnyRun);
    // the value lands nowhere: a stopped target is a no-op, and the recycled
    // handle now belongs to somebody else
    QCOMPARE(completedIntensity, 0.5);
    // the native Intensity override is Single, so a running co-owned target
    // takes the newest value instead of mixing per owner
    QCOMPARE(coOwnedIntensity, 0.4);
}

void ShowRunner_Test::commandIntensityOnManuallyStoppedTarget()
{
    Doc doc(nullptr);
    auto *timer = doc.masterTimer();
    auto *fixture = makeFixture(doc, 1);
    Scene *scene = makeScene(doc, fixture->id(), 0, 200);

    auto *show = new Show(&doc);
    doc.addFunction(show);

    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::start(1, 0, scene->id())));
    QVERIFY(track.insert(ShowCommand::setIntensity(2, 200, scene->id(), 0.5)));
    track.setExtent(600);
    QVERIFY(show->setCommandTrack(track));

    int ticks = 0;
    auto advanceTo = [&](quint32 ms) {
        const int target = ticksToReach(ms);
        while (ticks < target)
        {
            timer->timerTick();
            ticks++;
        }
    };

    show->start(timer, FunctionParent::master());
    advanceTo(0);
    QVERIFY(scene->isRunning());

    // the user stops the target in the middle of playback
    scene->stop(FunctionParent::master());
    timer->timerTick();
    ticks++;

    advanceTo(300);
    const bool resurrected = scene->isRunning();

    show->stop(FunctionParent::master());
    timer->timerTick();

    QVERIFY2(resurrected == false,
             "an intensity command must never restart a manually stopped target");
}

void ShowRunner_Test::commandPositionFollowsTransport_data()
{
    QTest::addColumn<bool>("external");
    QTest::addColumn<quint32>("whileRunning");
    QTest::addColumn<quint32>("whilePaused");

    // an autonomous transport freezes on pause, an external one keeps reporting
    // the source clock the host is still pushing
    QTest::newRow("autonomous freezes") << false << quint32(40) << quint32(40);
    QTest::newRow("external follows the source") << true << quint32(1000) << quint32(5000);
}

void ShowRunner_Test::commandPositionFollowsTransport()
{
    QFETCH(bool, external);
    QFETCH(quint32, whileRunning);
    QFETCH(quint32, whilePaused);

    Doc doc(nullptr);
    auto *timer = doc.masterTimer();
    auto *show = new Show(&doc);
    doc.addFunction(show);

    ShowCommandTrack track;
    track.setExtent(4000);
    QVERIFY(show->setCommandTrack(track));

    if (external)
    {
        show->setSyncSource(ShowRunner::External);
        show->setExternalElapsedTime(1000);
    }

    show->start(timer, FunctionParent::master());
    for (int i = 0; i < 3; i++)
        timer->timerTick();
    const quint32 runningPosition = show->commandPosition();

    show->setPause(true);
    show->setExternalElapsedTime(5000);
    for (int i = 0; i < 3; i++)
        timer->timerTick();
    const quint32 pausedPosition = show->commandPosition();

    show->setPause(false);
    show->stop(FunctionParent::master());
    timer->timerTick();

    QCOMPARE(runningPosition, whileRunning);
    QCOMPARE(pausedPosition, whilePaused);
}

void ShowRunner_Test::commandLiveMarkSuppressesOnlyItsOwnEvent()
{
    Doc doc(nullptr);
    auto *timer = doc.masterTimer();
    auto *fixture = makeFixture(doc, 2);
    Scene *live = makeScene(doc, fixture->id(), 0, 200);
    Scene *authored = makeScene(doc, fixture->id(), 1, 200);

    auto *show = new Show(&doc);
    doc.addFunction(show);

    ShowCommandTrack empty;
    empty.setExtent(2000);
    QVERIFY(show->setCommandTrack(empty));

    QSignalSpy liveStarts(live, &Function::running);
    QSignalSpy authoredStarts(authored, &Function::running);

    show->start(timer, FunctionParent::master());
    for (int i = 0; i < 3; i++)
        timer->timerTick(); // consumed through 40ms

    /** One event was executed live by the recorder, the other was put at the
     *  very same millisecond by the editor. The mark is metadata about one id,
     *  so it may not consume the cursor and swallow its neighbour. */
    ShowCommandTrack track;
    track.setExtent(2000);
    QVERIFY(track.insert(ShowCommand::start(1, 50, live->id())));
    QVERIFY(track.insert(ShowCommand::start(2, 50, authored->id())));
    QVERIFY(show->setCommandTrack(track, {1}));

    for (int i = 0; i < 3; i++)
        timer->timerTick();

    const int liveEchoes = liveStarts.count();
    const int authoredRuns = authoredStarts.count();

    show->stop(FunctionParent::master());
    timer->timerTick();

    QCOMPARE(liveEchoes, 0);
    QCOMPARE(authoredRuns, 1);
}

void ShowRunner_Test::commandIntensityIgnoresRecycledOverrideId()
{
    Doc doc(nullptr);
    auto *timer = doc.masterTimer();
    auto *fixture = makeFixture(doc, 1);
    Scene *scene = makeScene(doc, fixture->id(), 0, 200);

    auto *show = new Show(&doc);
    doc.addFunction(show);

    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::start(1, 0, scene->id())));
    QVERIFY(track.insert(ShowCommand::setIntensity(2, 0, scene->id(), 0.5)));
    QVERIFY(track.insert(ShowCommand::setIntensity(3, 300, scene->id(), 0.4)));
    track.setExtent(800);
    QString error;
    QVERIFY2(show->setCommandTrack(track, {}, &error), qPrintable(error));

    int ticks = 0;
    auto advanceTo = [&](quint32 ms) {
        const int target = ticksToReach(ms);
        while (ticks < target)
        {
            timer->timerTick();
            ticks++;
        }
    };

    show->start(timer, FunctionParent::master());
    advanceTo(0);
    advanceTo(200);

    /** The target drops its overrides and somebody else claims the recycled
     *  identifier for a different attribute, all between two Show ticks. The
     *  target never stops, so a cached handle survives any liveness check. */
    scene->resetAttributes();
    const int recycled = scene->requestAttributeOverride(Scene::ParentIntensity, 0.5);
    QVERIFY(recycled != Function::invalidAttributeId());

    advanceTo(320);
    const qreal intensity = scene->getAttributeValue(Function::Intensity);
    const qreal parentIntensity = scene->getAttributeValue(Scene::ParentIntensity);

    show->stop(FunctionParent::master());
    timer->timerTick();

    QCOMPARE(intensity, 0.4);
    QVERIFY2(qFuzzyCompare(parentIntensity, 0.5),
             "an intensity command must never drive another attribute");
}

void ShowRunner_Test::commandIntensityIgnoresStaleClipQueueEntry()
{
    Doc doc(nullptr);
    auto *timer = doc.masterTimer();
    auto *fixture = makeFixture(doc, 1);
    Scene *scene = makeScene(doc, fixture->id(), 0, 200);

    auto *show = new Show(&doc);
    doc.addFunction(show);
    auto *track = new Track(Function::invalidId());
    auto *item = new ShowFunction(show->getLatestShowFunctionId());
    item->setFunctionID(scene->id());
    item->setStartTime(100);
    item->setDuration(200); // ordinary clip 100 - 300
    track->addShowFunction(item);
    show->addTrack(track);

    ShowCommandTrack commands;
    QVERIFY(commands.insert(ShowCommand::setIntensity(1, 200, scene->id(), 0.5)));
    commands.setExtent(600);
    QVERIFY(show->setCommandTrack(commands));

    int ticks = 0;
    auto advanceTo = [&](quint32 ms) {
        const int target = ticksToReach(ms);
        while (ticks < target)
        {
            timer->timerTick();
            ticks++;
        }
    };

    show->start(timer, FunctionParent::master());
    advanceTo(140);
    const bool runningFromClip = scene->isRunning();

    // the user shuts the target down completely, while its clip entry, and
    // therefore its queued end, is still sitting in the scheduler
    scene->stop(FunctionParent::master());
    timer->timerTick();
    ticks++;

    advanceTo(240);
    const bool resurrected = scene->isRunning();
    const qreal afterCommand = scene->getAttributeValue(Function::Intensity);

    // a later manual restart must not inherit anything the command left behind
    const FunctionParent manualOwner(FunctionParent::AutoVCWidget, 42);
    scene->start(timer, manualOwner);
    timer->timerTick();
    ticks++;
    const qreal afterRestart = scene->getAttributeValue(Function::Intensity);

    show->stop(FunctionParent::master());
    scene->stop(manualOwner);
    timer->timerTick();

    QVERIFY(runningFromClip);
    QCOMPARE(resurrected, false);
    QVERIFY2(qFuzzyCompare(afterCommand, 1.0),
             "a stale clip entry must not make a stopped target look active");
    QVERIFY2(qFuzzyCompare(afterRestart, 1.0),
             "a manual restart must not inherit a command override");
}

void ShowRunner_Test::commandSuppressedLiveStopRetiresClipDeadline()
{
    Doc doc(nullptr);
    auto *timer = doc.masterTimer();
    auto *fixture = makeFixture(doc, 1);
    Scene *scene = makeScene(doc, fixture->id(), 0, 200);

    auto *show = new Show(&doc);
    doc.addFunction(show);
    auto *clips = new Track(Function::invalidId());
    auto *item = new ShowFunction(show->getLatestShowFunctionId());
    item->setFunctionID(scene->id());
    item->setStartTime(100);
    item->setDuration(200); // ordinary clip 100 - 300
    clips->addShowFunction(item);
    show->addTrack(clips);

    ShowCommandTrack track;
    track.setExtent(600);
    QVERIFY(show->setCommandTrack(track));

    int ticks = 0;
    auto advanceTo = [&](quint32 ms) {
        const int target = ticksToReach(ms);
        while (ticks < target)
        {
            timer->timerTick();
            ticks++;
        }
    };

    show->start(timer, FunctionParent::master());
    advanceTo(140);
    const bool runningFromClip = scene->isRunning();

    /** The recorder executed this Stop live and publishes it marked, so the
     *  runtime must not execute it again - but the superseded clip end is
     *  scheduler bookkeeping that still has to be retired. */
    scene->stop(FunctionParent(FunctionParent::Function, show->id()));
    QVERIFY(track.insert(ShowCommand::stop(1, 180, scene->id())));
    QVERIFY(show->setCommandTrack(track, {1}));

    advanceTo(200);
    const bool stoppedLive = scene->isRunning();

    QVERIFY(track.insert(ShowCommand::start(2, 220, scene->id())));
    QVERIFY(track.insert(ShowCommand::stop(3, 350, scene->id())));
    QVERIFY(show->setCommandTrack(track));

    advanceTo(240);
    const bool restartedByCommand = scene->isRunning();

    // another publication re-offers the same live id: retirement is once-only
    // and must not touch the activation that is running now
    QVERIFY(show->setCommandTrack(track));

    advanceTo(320); // past the superseded clip deadline of 300
    const bool survivedOldDeadline = scene->isRunning();

    advanceTo(380);
    const bool stoppedByAuthoredStop = scene->isRunning();

    show->stop(FunctionParent::master());
    timer->timerTick();

    QVERIFY(runningFromClip);
    QCOMPARE(stoppedLive, false);
    QVERIFY(restartedByCommand);
    QVERIFY2(survivedOldDeadline,
             "a suppressed live Stop must still retire the superseded clip end");
    QCOMPARE(stoppedByAuthoredStop, false);
}

QTEST_APPLESS_MAIN(ShowRunner_Test)
