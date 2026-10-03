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
#include "show.h"
#undef protected
#undef private
#include "fixture.h"
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

/** A clip that remembers the thread its pause reached it on */
class PauseProbeFunction final : public Function
{
public:
    explicit PauseProbeFunction(Doc *doc) : Function(doc, Function::SceneType) {}

    void setPause(bool enable) override
    {
        pauseThread.store(QThread::currentThread());
        Function::setPause(enable);
    }

    std::atomic<QThread *> pauseThread{nullptr};
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
    // the first sample is where the traversal begins; later ones cross intervals
    show->setExternalElapsedTime(0);
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

void ShowRunner_Test::commandExternalBackwardRollsBackItsWindow()
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
    show->setExternalElapsedTime(0);
    timer->timerTick();
    show->setExternalElapsedTime(600);
    timer->timerTick();
    timer->timerTick();
    const uchar forwardValue = sampleChannel(doc, 1);
    const int forwardStarts = starts.count();

    // back to 300: what the window (300, 600] changed returns, nothing earlier replays
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
    // .75 at 400 returns to the .25 it replaced
    QCOMPARE(restoredValue, uchar(50));
    // .9 at 500 returns to the full value it replaced
    QCOMPARE(retainedValue, uchar(200));
    // the Start at 100 lies before the window: it keeps its target, nothing restarts
    QVERIFY(historicalTriggerRunning);
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

void ShowRunner_Test::commandForwardSeekPlaysCrossedCommands()
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

    // forward: the commands crossed up to 600 play, in order
    show->requestSeek(600);
    timer->timerTick();

    const bool crossedStartRan = triggered->isRunning();
    const uchar restoredValue = sampleChannel(doc, 1);
    const int boundaryStartsAfterSeek = boundaryStarts.count();

    for (int i = 0; i < 5; i++)
        timer->timerTick();
    const int boundaryStartsLater = boundaryStarts.count();

    show->stop(FunctionParent::master());
    valued->stop(manualOwner);
    timer->timerTick();

    QVERIFY(crossedStartRan);
    QCOMPARE(restoredValue, uchar(150));  // .25, then 200 * 0.75
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

void ShowRunner_Test::commandLiveIdsNeedTheirTraversal_data()
{
    QTest::addColumn<bool>("seekBeforeAccept");
    QTest::addColumn<bool>("seekBeforePublish");
    QTest::addColumn<int>("starts");

    QTest::newRow("accepted in the current traversal") << false << false << 0;
    // a stale traversal at publication, given to the public API: this does
    // not reproduce the recorder's thread race, it pins the locked decision
    QTest::newRow("traversal ended before publication") << false << true << 1;
    QTest::newRow("accepted after a consumed seek") << true << false << 0;
}

void ShowRunner_Test::commandLiveIdsNeedTheirTraversal()
{
    QFETCH(bool, seekBeforeAccept);
    QFETCH(bool, seekBeforePublish);
    QFETCH(int, starts);

    Doc doc(nullptr);
    auto *timer = doc.masterTimer();
    auto *fixture = makeFixture(doc, 1);
    Scene *live = makeScene(doc, fixture->id(), 0, 200);

    auto *show = new Show(&doc);
    doc.addFunction(show);

    ShowCommandTrack empty;
    empty.setExtent(2000);
    QVERIFY(show->setCommandTrack(empty));

    QSignalSpy liveStarts(live, &Function::running);

    show->start(timer, FunctionParent::master());
    for (int i = 0; i < 3; i++)
        timer->timerTick(); // consumed through 40ms

    // each seek is consumed before the input is accepted or published
    if (seekBeforeAccept)
    {
        show->requestSeek(0);
        timer->timerTick();
    }
    // the recorder executed the input live in this traversal
    const quint64 accepted = show->commandTraversal();
    if (seekBeforePublish)
    {
        show->requestSeek(0);
        timer->timerTick();
    }

    ShowCommandTrack track;
    track.setExtent(2000);
    QVERIFY(track.insert(ShowCommand::start(1, 50, live->id())));
    QVERIFY(show->setCommandTrack(track, {1}, accepted, nullptr));

    for (int i = 0; i < 6; i++)
        timer->timerTick();
    const int played = liveStarts.count();

    show->stop(FunctionParent::master());
    timer->timerTick();

    QCOMPARE(played, starts);
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

void ShowRunner_Test::commandLiveOccurrenceAfterEdit_data()
{
    QTest::addColumn<QStringList>("steps");
    QTest::addColumn<QStringList>("liveStarts"); // elapsed ms of each tick starting the live Scene

    QTest::newRow("unrelated publication keeps it suppressed")
        << QStringList({ "pass", "unrelated" }) << QStringList();
    QTest::newRow("passed, unrelated publication, moved ahead plays when crossed")
        << QStringList({ "pass", "unrelated", "ahead" }) << QStringList({ "300" });
    QTest::newRow("moved ahead before it is reached plays at its new time")
        << QStringList({ "ahead" }) << QStringList({ "300" });
    QTest::newRow("moved ahead and back before it is reached stays suppressed")
        << QStringList({ "ahead", "back" }) << QStringList();
    // a mark keeps the capture's time even if a later publication names the id again
    QTest::newRow("moved ahead and offered live again plays at its new time")
        << QStringList({ "ahead-relive" }) << QStringList({ "300" });
}

void ShowRunner_Test::commandLiveOccurrenceAfterEdit()
{
    QFETCH(QStringList, steps);
    QFETCH(QStringList, liveStarts);

    Doc doc(nullptr);
    auto *timer = doc.masterTimer();
    auto *fixture = makeFixture(doc, 2);
    Scene *live = makeScene(doc, fixture->id(), 0, 200);
    Scene *other = makeScene(doc, fixture->id(), 1, 200);

    auto *show = new Show(&doc);
    doc.addFunction(show);

    ShowCommandTrack track;
    track.setExtent(2000);
    QVERIFY(show->setCommandTrack(track));

    QSignalSpy starts(live, &Function::running);
    QStringList seen;
    int ticks = 0;
    auto advanceTo = [&](quint32 ms) {
        const int target = ticksToReach(ms);
        while (ticks < target)
        {
            const int before = starts.count();
            timer->timerTick();
            if (starts.count() != before)
                seen.append(QString::number(ticks * MasterTimer::tick()));
            ticks++;
        }
    };

    show->start(timer, FunctionParent::master());
    advanceTo(40);

    // the recorder executed this input live and publishes it marked
    QVERIFY(track.insert(ShowCommand::start(1, 50, live->id())));
    QVERIFY(show->setCommandTrack(track, {1}));

    // edits and other publications carry no live ids
    for (const QString &step : std::as_const(steps))
    {
        if (step == QLatin1String("pass"))
            advanceTo(100);
        else if (step == QLatin1String("unrelated"))
            QVERIFY(track.insert(ShowCommand::start(2, 1500, other->id())) && show->setCommandTrack(track));
        else if (step == QLatin1String("ahead"))
            QVERIFY(track.retime(1, 300) && show->setCommandTrack(track));
        else if (step == QLatin1String("ahead-relive"))
            QVERIFY(track.retime(1, 300) && show->setCommandTrack(track, {1}));
        else if (step == QLatin1String("back"))
            QVERIFY(track.retime(1, 50) && show->setCommandTrack(track));
        else
            QFAIL(qPrintable(step));
    }
    advanceTo(400);

    show->stop(FunctionParent::master());
    live->stop(FunctionParent::master());
    timer->timerTick();

    QCOMPARE(seen, liveStarts);
}

void ShowRunner_Test::commandLiveStopMovedAhead_data()
{
    QTest::addColumn<QString>("edit");    // what happens to the live Stop
    QTest::addColumn<bool>("editFirst");  // published before the runner saw the capture
    QTest::addColumn<QStringList>("running"); // the target at 240, 280, 320 and 420 ms

    // the Stop ran live at 180: the clip end at 300 is retired whichever
    // snapshot the runner saw, and only the edited Stop decides the rest
    const QStringList movedAhead({ "on", "on", "on", "off" });
    const QStringList noStop({ "on", "on", "on", "on" });
    QTest::newRow("moved ahead after the runner saw it") << "move" << false << movedAhead;
    QTest::newRow("moved ahead before the runner saw it") << "move" << true << movedAhead;
    QTest::newRow("retargeted before the runner saw it") << "retarget" << true << noStop;
    QTest::newRow("deleted before the runner saw it") << "delete" << true << noStop;
    QTest::newRow("deleted after the runner saw it") << "delete" << false << noStop;
}

void ShowRunner_Test::commandLiveStopMovedAhead()
{
    QFETCH(QString, edit);
    QFETCH(bool, editFirst);
    QFETCH(QStringList, running);

    Doc doc(nullptr);
    auto *timer = doc.masterTimer();
    auto *fixture = makeFixture(doc, 2);
    Scene *scene = makeScene(doc, fixture->id(), 0, 200);
    Scene *other = makeScene(doc, fixture->id(), 1, 200);

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
    QVERIFY(scene->isRunning());

    // executed live at 180 by the recorder, published marked
    scene->stop(FunctionParent(FunctionParent::Function, show->id()));
    QVERIFY(track.insert(ShowCommand::stop(1, 180, scene->id())));
    QVERIFY(show->setCommandTrack(track, {1}));

    const auto publishEdit = [&]() {
        QVERIFY(track.insert(ShowCommand::start(2, 220, scene->id())));
        if (edit == QLatin1String("move"))
            QVERIFY(track.retime(1, 400));
        else if (edit == QLatin1String("retarget"))
        {
            ShowCommand stop = track.commands().at(track.indexOfId(1));
            stop.functionId = other->id();
            stop.time = 400;
            QVERIFY(track.replace(stop));
        }
        else
            QVERIFY(track.remove(1));
        QVERIFY(show->setCommandTrack(track));
    };
    if (editFirst)
        publishEdit();
    advanceTo(200);
    const bool stoppedLive = scene->isRunning() == false;
    if (!editFirst)
        publishEdit();

    QStringList seen;
    for (quint32 ms : { 240u, 280u, 320u, 420u })
    {
        advanceTo(ms);
        seen.append(scene->isRunning() ? QStringLiteral("on") : QStringLiteral("off"));
    }

    show->stop(FunctionParent::master());
    scene->stop(FunctionParent::master());
    other->stop(FunctionParent::master());
    timer->timerTick();

    QVERIFY(stoppedLive);
    QCOMPARE(seen, running);
}

void ShowRunner_Test::commandLiveStopRetiresOnce_data()
{
    QTest::addColumn<QStringList>("publishAt"); // ms of publications after the capture
    QTest::addColumn<QStringList>("running");   // the target at 450 and 520 ms

    // the live Stop retired the first clip's end only: a later clip of the
    // same Function still ends at 500, however often the mark is re-offered
    QTest::newRow("no later publication") << QStringList() << QStringList({ "on", "off" });
    QTest::newRow("unrelated publication during the later clip")
        << QStringList({ "450" }) << QStringList({ "on", "off" });
    QTest::newRow("publications before and during the later clip")
        << QStringList({ "380", "420", "460" }) << QStringList({ "on", "off" });
}

void ShowRunner_Test::commandLiveStopRetiresOnce()
{
    QFETCH(QStringList, publishAt);
    QFETCH(QStringList, running);

    Doc doc(nullptr);
    auto *timer = doc.masterTimer();
    auto *fixture = makeFixture(doc, 2);
    Scene *scene = makeScene(doc, fixture->id(), 0, 200);
    Scene *other = makeScene(doc, fixture->id(), 1, 200);

    auto *show = new Show(&doc);
    doc.addFunction(show);
    auto *clips = new Track(Function::invalidId());
    for (const auto &span : { qMakePair(100u, 200u), qMakePair(400u, 100u) }) // 100 - 300, 400 - 500
    {
        auto *item = new ShowFunction(show->getLatestShowFunctionId());
        item->setFunctionID(scene->id());
        item->setStartTime(span.first);
        item->setDuration(span.second);
        clips->addShowFunction(item);
    }
    show->addTrack(clips);

    ShowCommandTrack track;
    track.setExtent(800);
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
    QVERIFY(scene->isRunning());

    // executed live at 180 by the recorder, published marked
    scene->stop(FunctionParent(FunctionParent::Function, show->id()));
    QVERIFY(track.insert(ShowCommand::stop(1, 180, scene->id())));
    QVERIFY(show->setCommandTrack(track, {1}));
    advanceTo(200);

    quint32 nextId = 2;
    QStringList seen;
    const auto sample = [&]() { seen.append(scene->isRunning() ? QStringLiteral("on") : QStringLiteral("off")); };
    for (const QString &at : std::as_const(publishAt))
    {
        const quint32 ms = at.toUInt();
        advanceTo(ms);
        // an unrelated event far ahead, so the runner takes a new revision
        QVERIFY(track.insert(ShowCommand::start(nextId++, 700, other->id())));
        QVERIFY(show->setCommandTrack(track));
    }
    advanceTo(450);
    sample();
    advanceTo(520);
    sample();

    show->stop(FunctionParent::master());
    scene->stop(FunctionParent::master());
    other->stop(FunctionParent::master());
    timer->timerTick();

    QCOMPARE(seen, running);
}

void ShowRunner_Test::commandEditWhilePlaying_data()
{
    QTest::addColumn<quint32>("editAt");
    QTest::addColumn<QString>("edit");
    QTest::addColumn<QStringList>("effects"); // "ms:what" for each tick with an effect

    const QStringList control({ "0:A+", "100:A@0.8", "200:A@0.5", "300:B+", "400:D+" });
    QTest::newRow("no edit") << 140u << "none" << control;
    QTest::newRow("edited future event plays its edited data") << 140u << "value"
        << QStringList({ "0:A+", "100:A@0.8", "200:A@0.25", "300:B+", "400:D+" });
    QTest::newRow("event deleted ahead does not play") << 140u << "delete"
        << QStringList({ "0:A+", "100:A@0.8", "200:A@0.5", "400:D+" });
    QTest::newRow("edit behind the cursor gives no output this pass") << 140u << "behind" << control;
    QTest::newRow("passed event moved ahead plays again") << 140u << "ahead"
        << QStringList({ "0:A+", "100:A@0.8", "200:A@0.5", "300:B+", "400:D+", "500:A@0.8" });
    // one publication between two ticks: the crossing tick sees all of it
    QTest::newRow("publication before the crossing is taken whole") << 180u << "both"
        << QStringList({ "0:A+", "100:A@0.8", "200:B+,A@0.25", "400:D+" });
    QTest::newRow("publication after the crossing leaves that tick old") << 200u << "both"
        << QStringList({ "0:A+", "100:A@0.8", "200:A@0.5", "400:D+" });
}

void ShowRunner_Test::commandEditWhilePlaying()
{
    QFETCH(quint32, editAt);
    QFETCH(QString, edit);
    QFETCH(QStringList, effects);

    Doc doc(nullptr);
    auto *timer = doc.masterTimer();
    auto *fixture = makeFixture(doc, 3);
    Scene *a = makeScene(doc, fixture->id(), 0, 200);
    Scene *b = makeScene(doc, fixture->id(), 1, 200);
    Scene *d = makeScene(doc, fixture->id(), 2, 200);

    auto *show = new Show(&doc);
    doc.addFunction(show);

    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::start(1, 0, a->id())));
    QVERIFY(track.insert(ShowCommand::setIntensity(2, 200, a->id(), 0.5)));
    QVERIFY(track.insert(ShowCommand::start(3, 300, b->id())));
    QVERIFY(track.insert(ShowCommand::setIntensity(4, 100, a->id(), 0.8)));
    QVERIFY(track.insert(ShowCommand::start(5, 400, d->id())));
    QVERIFY(track.setExtent(1000));
    QVERIFY(show->setCommandTrack(track));

    const QVector<QPair<QString, Scene *>> scenes({ { "A", a }, { "B", b }, { "D", d } });
    QVector<int> startCounts(scenes.count(), 0);
    QList<QSignalSpy *> spies;
    for (const auto &scene : scenes)
        spies.append(new QSignalSpy(scene.second, &Function::running));
    const auto cleanup = qScopeGuard([&]() { qDeleteAll(spies); });
    qreal intensity = a->getAttributeValue(Function::Intensity);

    QStringList seen;
    int ticks = 0;
    auto advanceTo = [&](quint32 ms) {
        const int target = ticksToReach(ms);
        while (ticks < target)
        {
            timer->timerTick();
            QStringList what;
            for (int i = 0; i < scenes.count(); i++)
            {
                if (spies.at(i)->count() != startCounts.at(i))
                    what.append(scenes.at(i).first + QLatin1Char('+'));
                startCounts[i] = spies.at(i)->count();
            }
            const qreal now = a->getAttributeValue(Function::Intensity);
            if (!qFuzzyCompare(now, intensity))
                what.append(QStringLiteral("A@") + QString::number(now));
            intensity = now;
            if (!what.isEmpty())
                seen.append(QString::number(ticks * MasterTimer::tick()) + QLatin1Char(':') + what.join(QLatin1Char(',')));
            ticks++;
        }
    };

    show->start(timer, FunctionParent::master());
    advanceTo(editAt);
    const quint64 traversal = show->commandTraversal();

    // a manual edit publishes with no live ids
    if (edit == QLatin1String("value") || edit == QLatin1String("both"))
        QVERIFY(track.replace(ShowCommand::setIntensity(2, 200, a->id(), 0.25)));
    if (edit == QLatin1String("both"))
        QVERIFY(track.retime(3, 200));
    if (edit == QLatin1String("delete"))
        QVERIFY(track.remove(3));
    if (edit == QLatin1String("behind"))
        QVERIFY(track.replace(ShowCommand::setIntensity(4, 100, a->id(), 0.3)));
    if (edit == QLatin1String("ahead"))
        QVERIFY(track.retime(4, 500));
    if (edit != QLatin1String("none"))
        QVERIFY(show->setCommandTrack(track));

    advanceTo(600);
    const quint64 traversalAfter = show->commandTraversal();

    show->stop(FunctionParent::master());
    for (const auto &scene : scenes)
        scene.second->stop(FunctionParent::master());
    timer->timerTick();

    QCOMPARE(seen, effects);
    QCOMPARE(traversalAfter, traversal);
}

void ShowRunner_Test::commandEditKeepsCrossedWork_data()
{
    QTest::addColumn<QString>("edit");

    QTest::newRow("queued Start deleted") << "delete";
    QTest::newRow("queued Start moved ahead") << "retime";
    QTest::newRow("queued Start retargeted") << "retarget";
}

void ShowRunner_Test::commandEditKeepsCrossedWork()
{
    QFETCH(QString, edit);

    Doc doc(nullptr);
    auto *timer = doc.masterTimer();
    auto *fixture = makeFixture(doc, 2);
    Scene *f = makeScene(doc, fixture->id(), 0, 200);
    Scene *g = makeScene(doc, fixture->id(), 1, 200);
    auto *show = new Show(&doc);
    doc.addFunction(show);
    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::setButtonState(1, 100, QUuid::createUuid(), true)));
    QVERIFY(track.insert(ShowCommand::start(2, 100, f->id())));
    QVERIFY(track.setExtent(4000));
    QVERIFY(show->setCommandTrack(track));
    show->attachControlExecutor(true);
    const auto detach = qScopeGuard([&]() { show->attachControlExecutor(false); });
    QSignalSpy fStarts(f, &Function::running);
    QSignalSpy gStarts(g, &Function::running);

    show->start(timer, FunctionParent::master());
    QVector<ShowControlBatch> published;
    for (int i = 0; i < ticksToReach(100) + 1 && published.isEmpty(); i++)
    {
        timer->timerTick();
        published = show->takeControlBatches();
    }
    QCOMPARE(published.count(), 1);
    timer->timerTick();

    // the batch is held, the Start of F waits behind it
    const QVector<ShowCommand> owedBefore = show->pendingCommandWork();
    const quint64 traversal = show->commandTraversal();
    QVERIFY(std::any_of(owedBefore.cbegin(), owedBefore.cend(), [&](const ShowCommand &cmd) {
        return cmd.id == 2 && cmd.functionId == f->id();
    }));

    if (edit == QLatin1String("delete"))
        QVERIFY(track.remove(2));
    else if (edit == QLatin1String("retime"))
        QVERIFY(track.retime(2, 900));
    else
        QVERIFY(track.replace(ShowCommand::start(2, 100, g->id())));
    QVERIFY(show->setCommandTrack(track));

    timer->timerTick(); // the runner takes the new snapshot
    const QVector<ShowCommand> owedAfter = show->pendingCommandWork();
    const int fBeforeAck = fStarts.count();

    show->acknowledgeControlBatch(published.first().traversal, published.first().seq);
    for (int i = 0; i < 5; i++)
        timer->timerTick();

    const bool owedLater = show->commandWorkPending();
    const int fStarted = fStarts.count();
    const int gStarted = gStarts.count();
    const quint64 traversalAfter = show->commandTraversal();

    show->stop(FunctionParent::master());
    f->stop(FunctionParent::master());
    g->stop(FunctionParent::master());
    timer->timerTick();

    QVERIFY(owedAfter == owedBefore);
    QCOMPARE(fBeforeAck, 0);
    // completed with the data it was crossed with
    QCOMPARE(fStarted, 1);
    QCOMPARE(gStarted, 0);
    QVERIFY(!owedLater);
    QCOMPARE(traversalAfter, traversal);
}

void ShowRunner_Test::commandEditKeepsSettlingWork_data()
{
    QTest::addColumn<QString>("edit");
    QTest::addColumn<QStringList>("effects"); // "tick:what" for each tick with an effect
    QTest::addColumn<QString>("outcome");     // name=state[@intensity]/running() count/stopped() count

    // a forward seek to 5 s plays its crossed interval one Start or Stop per
    // settled tick; the edit is published while the crossed Stop of F (id 2) is still settling
    const QStringList control({ "0:F+", "1:F-", "2:F+", "3:F@0.8" });
    QTest::newRow("no edit") << "none" << control << "F=on@0.8/2/1,G=off/0/0";
    QTest::newRow("settling Stop deleted") << "delete" << control << "F=on@0.8/2/1,G=off/0/0";
    QTest::newRow("settling Stop retargeted to G") << "retarget" << control << "F=on@0.8/2/1,G=off/0/0";
    QTest::newRow("queued Start behind it retargeted to G") << "queued" << control << "F=on@0.8/2/1,G=off/0/0";
    // moved ahead of the cursor: the accepted Stop finishes now, the moved one
    // is met again at 6 s (tick 50) and stops F once more, which resets its intensity
    QTest::newRow("settling Stop moved ahead of the cursor") << "ahead"
        << QStringList({ "0:F+", "1:F-", "2:F+", "3:F@0.8", "50:F-,F@1" }) << "F=off/2/2,G=off/0/0";
}

void ShowRunner_Test::commandEditKeepsSettlingWork()
{
    QFETCH(QString, edit);
    QFETCH(QStringList, effects);
    QFETCH(QString, outcome);

    Doc doc(nullptr);
    auto *timer = doc.masterTimer();
    auto *fixture = makeFixture(doc, 2);
    Scene *f = makeScene(doc, fixture->id(), 0, 200);
    Scene *g = makeScene(doc, fixture->id(), 1, 200);
    auto *show = new Show(&doc);
    doc.addFunction(show);

    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::start(1, 1000, f->id())));
    QVERIFY(track.insert(ShowCommand::stop(2, 2000, f->id())));
    QVERIFY(track.insert(ShowCommand::start(3, 3000, f->id())));
    QVERIFY(track.insert(ShowCommand::setIntensity(4, 4000, f->id(), 0.8)));
    // the VC record that makes the jump settle serially lies far ahead
    QVERIFY(track.insert(ShowCommand::setButtonState(9, 100000, QUuid::createUuid(), true)));
    QVERIFY(track.setExtent(200000));
    QVERIFY(show->setCommandTrack(track));
    show->attachControlExecutor(true);
    const auto detach = qScopeGuard([&]() { show->attachControlExecutor(false); });

    QSignalSpy fStarts(f, &Function::running);
    QSignalSpy fStops(f, qOverload<quint32>(&Function::stopped));
    QSignalSpy gStarts(g, &Function::running);
    QSignalSpy gStops(g, qOverload<quint32>(&Function::stopped));
    int counts[4] = { 0, 0, 0, 0 };
    qreal intensity = f->getAttributeValue(Function::Intensity);
    QStringList seen;
    int ticks = 0;
    const auto tick = [&]() {
        timer->timerTick();
        QStringList what;
        const int now[4] = { int(fStarts.count()), int(fStops.count()), int(gStarts.count()), int(gStops.count()) };
        const char *names[4] = { "F+", "F-", "G+", "G-" };
        for (int i = 0; i < 4; i++)
        {
            if (now[i] != counts[i])
                what.append(QString::fromLatin1(names[i]));
            counts[i] = now[i];
        }
        const qreal level = f->getAttributeValue(Function::Intensity);
        if (!qFuzzyCompare(level, intensity))
            what.append(QStringLiteral("F@") + QString::number(level));
        intensity = level;
        if (!what.isEmpty())
            seen.append(QString::number(ticks) + QLatin1Char(':') + what.join(QLatin1Char(',')));
        ticks++;
    };

    show->start(timer, FunctionParent::master());
    timer->timerTick();
    show->requestSeek(5000);
    // until the crossed Stop of F is the settling work
    const auto settlingStop = [&]() {
        const QVector<ShowCommand> owed = show->pendingCommandWork();
        return !owed.isEmpty() && owed.first().action == ShowCommandAction::Stop
            && owed.first().functionId == f->id() && owed.first().id == ShowCommand::InvalidId;
    };
    for (int i = 0; i < 10 && !settlingStop(); i++)
        tick();
    QVERIFY2(settlingStop(), qPrintable(seen.join(QLatin1Char(' '))));
    const quint64 traversal = show->commandTraversal();
    const QVector<ShowCommand> owedBefore = show->pendingCommandWork();

    // a manual edit publishes with no live ids
    if (edit == QLatin1String("delete"))
        QVERIFY(track.remove(2));
    else if (edit == QLatin1String("retarget"))
        QVERIFY(track.replace(ShowCommand::stop(2, 2000, g->id())));
    else if (edit == QLatin1String("queued"))
        QVERIFY(track.replace(ShowCommand::start(3, 3000, g->id())));
    else if (edit == QLatin1String("ahead"))
        QVERIFY(track.retime(2, 6000));
    if (edit != QLatin1String("none"))
        QVERIFY(show->setCommandTrack(track));
    QVERIFY(show->pendingCommandWork() == owedBefore);

    while (ticks < ticksToReach(1200) + 1)
        tick();
    const quint64 traversalAfter = show->commandTraversal();

    const QString state = (f->isRunning() ? QStringLiteral("on@") + QString::number(f->getAttributeValue(Function::Intensity))
                                          : QStringLiteral("off"))
        + QStringLiteral("/%1/%2").arg(fStarts.count()).arg(fStops.count());
    const QString gState = (g->isRunning() ? QStringLiteral("on") : QStringLiteral("off"))
        + QStringLiteral("/%1/%2").arg(gStarts.count()).arg(gStops.count());

    show->stop(FunctionParent::master());
    f->stop(FunctionParent::master());
    g->stop(FunctionParent::master());
    timer->timerTick();

    QCOMPARE(seen, effects);
    QCOMPARE(QStringLiteral("F=") + state + QStringLiteral(",G=") + gState, outcome);
    QCOMPARE(traversalAfter, traversal);
}

void ShowRunner_Test::functionReceiptSeesStartThatAlreadyFinished()
{
    Doc doc(nullptr);
    auto *timer = doc.masterTimer();
    auto *show = new Show(&doc);
    doc.addFunction(show);

    // nothing to output: it stops itself on its first write
    auto *selfCompleting = new Scene(&doc);
    doc.addFunction(selfCompleting);
    QSignalSpy ran(selfCompleting, &Function::running);
    QSignalSpy ended(selfCompleting, qOverload<quint32>(&Function::stopped));
    selfCompleting->start(timer, FunctionParent::master());
    for (int i = 0; i < 4 && ended.isEmpty(); i++)
        timer->timerTick();
    QCOMPARE(ran.count(), 1);
    QCOMPARE(ended.count(), 1);

    // the start op is only armed once its function has come and gone
    QSignalSpy receipts(show, &Show::functionReceiptReady);
    ShowFunctionExpectation started;
    started.functionId = selfCompleting->id();
    show->requestFunctionReceipt(0, 1, { started });
    timer->timerTick();

    QCOMPARE(receipts.count(), 1);
}

void ShowRunner_Test::functionReceiptStopCompletesWhenAnotherOwnerRestarts()
{
    Doc doc(nullptr);
    auto *timer = doc.masterTimer();
    auto *fixture = makeFixture(doc, 1);
    Scene *scene = makeScene(doc, fixture->id(), 0, 200);
    auto *show = new Show(&doc);
    doc.addFunction(show);

    scene->start(timer, FunctionParent::master());
    timer->timerTick();
    QVERIFY(scene->isRunning());

    QSignalSpy receipts(show, &Show::functionReceiptReady);
    scene->stop(FunctionParent::master());
    ShowFunctionExpectation stopped;
    stopped.functionId = scene->id();
    stopped.expectLive = false;
    show->requestFunctionReceipt(0, 1, { stopped });
    // another owner takes it over before the stop lands
    scene->start(timer, FunctionParent(FunctionParent::AutoVCWidget, 42));
    for (int i = 0; i < 3; i++)
        timer->timerTick();

    QVERIFY(scene->isRunning());
    QCOMPARE(receipts.count(), 1);
    scene->stop(FunctionParent::master());
    timer->timerTick();
}

void ShowRunner_Test::functionReceiptShowDeletedWhilePending()
{
    Doc doc(nullptr);
    auto *timer = doc.masterTimer();
    auto *fixture = makeFixture(doc, 1);
    Scene *scene = makeScene(doc, fixture->id(), 0, 200);
    auto *show = new Show(&doc);
    doc.addFunction(show);

    ShowFunctionExpectation started;
    started.functionId = scene->id();
    show->requestFunctionReceipt(0, 1, { started });
    const std::weak_ptr<ShowReceiptState> receipts = show->m_receipts;
    QVERIFY(doc.deleteFunction(show->id()));

    // the start the receipt waited for happens after its Show is gone
    scene->start(timer, FunctionParent::master());
    timer->timerTick();
    timer->timerTick();

    QVERIFY(scene->isRunning());
    // no observer outlives the Show: nothing holds its receipt state
    QVERIFY(receipts.expired());
    scene->stop(FunctionParent::master());
    timer->timerTick();
}

void ShowRunner_Test::functionReceiptCauseEndedBeforeArming()
{
    Doc doc(nullptr);
    auto *timer = doc.masterTimer();
    auto *fixture = makeFixture(doc, 1);
    Scene *child = makeScene(doc, fixture->id(), 0, 200);
    auto *collection = new Collection(&doc);
    collection->addFunction(child->id());
    doc.addFunction(collection);
    auto *show = new Show(&doc);
    doc.addFunction(show);

    QSignalSpy ended(collection, qOverload<quint32>(&Function::stopped));
    collection->start(timer, FunctionParent::master());
    timer->timerTick();
    timer->timerTick();
    collection->stop(FunctionParent::master());
    for (int i = 0; i < 4 && ended.isEmpty(); i++)
        timer->timerTick();
    QCOMPARE(ended.count(), 1);

    // the child OFF is only armed once its Collection has come and gone
    QSignalSpy receipts(show, &Show::functionReceiptReady);
    ShowFunctionExpectation childOff;
    childOff.functionId = child->id();
    childOff.expectLive = false;
    childOff.causeFunctionId = collection->id();
    show->requestFunctionReceipt(0, 1, { childOff });
    timer->timerTick();

    QCOMPARE(receipts.count(), 1);
}

void ShowRunner_Test::commandControlRecordWithoutExecutorIsDropped()
{
    Doc doc(nullptr);
    auto *timer = doc.masterTimer();
    auto *fixture = makeFixture(doc, 1);
    Scene *scene = makeScene(doc, fixture->id(), 0, 200);
    auto *show = new Show(&doc);
    doc.addFunction(show);

    // no Virtual Console attached to this Show applies the button record
    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::setButtonState(1, 0, QUuid::createUuid(), true)));
    QVERIFY(track.insert(ShowCommand::start(2, 100, scene->id())));
    QVERIFY(track.setExtent(400));
    QString error;
    QVERIFY2(show->setCommandTrack(track, {}, &error), qPrintable(error));

    QSignalSpy finished(show, &Show::showFinished);
    show->start(timer, FunctionParent::master());
    for (int i = 0; i < ticksToReach(100); i++)
        timer->timerTick();
    const bool legacyApplied = scene->isRunning();
    for (int i = 0; i < ticksToReach(600) && finished.isEmpty(); i++)
        timer->timerTick();
    timer->timerTick();

    QVERIFY(legacyApplied);
    QCOMPARE(finished.count(), 1);
    QVERIFY(!show->isRunning());
    scene->stop(FunctionParent::master());
    timer->timerTick();
}

void ShowRunner_Test::commandCancelledPublicationDropsLegacyRemainder_data()
{
    QTest::addColumn<bool>("seekPending");

    QTest::newRow("seek pending") << true;
    QTest::newRow("show stopped") << false;
}

void ShowRunner_Test::commandCancelledPublicationDropsLegacyRemainder()
{
    QFETCH(bool, seekPending);

    Doc doc(nullptr);
    auto *timer = doc.masterTimer();
    auto *fixture = makeFixture(doc, 1);
    Scene *scene = makeScene(doc, fixture->id(), 0, 200);
    auto *show = new Show(&doc);
    doc.addFunction(show);
    ShowCommandTrack track;
    QVERIFY(track.setExtent(2000));
    QVERIFY(show->setCommandTrack(track));
    show->attachControlExecutor(true);

    show->start(timer, FunctionParent::master());
    timer->timerTick();

    // the crossing is dispatched while the cancellation is taking effect
    if (seekPending)
        show->requestSeek(1500);
    else
        show->stop(FunctionParent::master());
    QSignalSpy started(scene, &Function::running);
    show->m_runner->dispatchCommandEffects({
        ShowCommand::setButtonState(1, 100, QUuid::createUuid(), true),
        ShowCommand::start(2, 100, scene->id()) });
    timer->timerTick();
    timer->timerTick();

    QVERIFY2(started.isEmpty(), "the cancelled traversal's legacy remainder must not run");
    show->attachControlExecutor(false);
    show->stop(FunctionParent::master());
    scene->stop(FunctionParent::master());
    timer->timerTick();
}

void ShowRunner_Test::commandForwardSeekWithoutExecutorPlaysLegacyValues()
{
    Doc doc(nullptr);
    auto *timer = doc.masterTimer();
    auto *fixture = makeFixture(doc, 1);
    Scene *target = makeScene(doc, fixture->id(), 0, 200);
    auto *show = new Show(&doc);
    doc.addFunction(show);
    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::setButtonState(1, 100, QUuid::createUuid(), true)));
    QVERIFY(track.insert(ShowCommand::setIntensity(2, 200, target->id(), 0.3)));
    QVERIFY(track.setExtent(4000));
    QVERIFY(show->setCommandTrack(track));

    target->start(timer, FunctionParent::master());
    show->start(timer, FunctionParent::master());
    timer->timerTick();
    show->requestSeek(1000);
    for (int i = 0; i < 5; i++)
        timer->timerTick();

    // no VC executor: the forward seek plays the crossed legacy value inline
    QVERIFY(show->takeControlBatches().isEmpty());
    QCOMPARE(target->getAttributeValue(Function::Intensity), qreal(0.3));
    QVERIFY(show->isRunning());

    show->stop(FunctionParent::master());
    target->stop(FunctionParent::master());
    timer->timerTick();
}

void ShowRunner_Test::commandPlayFromCursorReplaysNoHistory_data()
{
    QTest::addColumn<bool>("executor");
    QTest::addColumn<bool>("started");

    // the only VC record lies after the destination; history before the cursor never runs
    QTest::newRow("executor attached") << true << false;
    QTest::newRow("no executor") << false << false;
}

void ShowRunner_Test::commandPlayFromCursorReplaysNoHistory()
{
    QFETCH(bool, executor);
    QFETCH(bool, started);

    Doc doc(nullptr);
    auto *timer = doc.masterTimer();
    auto *fixture = makeFixture(doc, 1);
    Scene *scene = makeScene(doc, fixture->id(), 0, 200);
    auto *show = new Show(&doc);
    doc.addFunction(show);
    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::start(1, 10000, scene->id())));
    QVERIFY(track.insert(ShowCommand::setButtonState(2, 100000, QUuid::createUuid(), true)));
    QVERIFY(track.setExtent(200000));
    QVERIFY(show->setCommandTrack(track));
    if (executor)
        show->attachControlExecutor(true);
    const auto detach = qScopeGuard([&]() {
        if (executor)
            show->attachControlExecutor(false);
    });

    show->start(timer, FunctionParent::master(), 50000);
    for (int i = 0; i < 3; i++)
        timer->timerTick();

    QCOMPARE(scene->isRunning(), started);

    show->stop(FunctionParent::master());
    scene->stop(FunctionParent::master());
    timer->timerTick();
}

void ShowRunner_Test::commandJumpSettlesLegacyStartStop_data()
{
    QTest::addColumn<QString>("rig");
    QTest::addColumn<QString>("outcome");

    // name=state[@intensity]/running() count/stopped() count, after a forward seek to 5s
    QTest::newRow("start, stop, start, then .8") << QStringLiteral("serial")
        << QStringLiteral("F=on@0.8/2/1,G=off/0/0");
    QTest::newRow("same, over a destination clip of F") << QStringLiteral("clip")
        << QStringLiteral("F=on@0.8/2/1,G=off/0/0");
    QTest::newRow("another owner keeps F through the Stop") << QStringLiteral("owner")
        << QStringLiteral("F=on@0.8/0/0,G=off/0/0");
    QTest::newRow("a start that ends at once, then the next") << QStringLiteral("ends")
        << QStringLiteral("F=off/1/1,G=on@1/1/0");
    QTest::newRow("a start already owned, then the next") << QStringLiteral("owned")
        << QStringLiteral("F=on@1/1/0,G=on@1/1/0");
    QTest::newRow("seek during the wait") << QStringLiteral("seek")
        << QStringLiteral("F=off/1/1,G=off/0/0");
    QTest::newRow("stop during the wait") << QStringLiteral("stop")
        << QStringLiteral("F=off/1/1,G=off/0/0");
    // a seek to 200ms; a clip queues F again at 240, just after the Stop retired it
    QTest::newRow("a clip restart queued behind the Stop") << QStringLiteral("pending clip")
        << QStringLiteral("F=on@0.8/2/1,G=off/0/0");
    // F runs ahead of the Show; the Stop drops its last owner after F's write,
    // and another owner queues a restart of the still running F before the next tick
    QTest::newRow("a restart queued for the still running F") << QStringLiteral("running restart")
        << QStringLiteral("F=on@0.8/1/1,G=off/0/0");
    // the Stop leaves F to its other owner, which stops and restarts it before the next tick
    QTest::newRow("the remaining owner restarts F") << QStringLiteral("owner restart")
        << QStringLiteral("F=on@0.8/1/1,G=off/0/0");
}

void ShowRunner_Test::commandJumpSettlesLegacyStartStop()
{
    QFETCH(QString, rig);
    QFETCH(QString, outcome);

    Doc doc(nullptr);
    auto *timer = doc.masterTimer();
    auto *fixture = makeFixture(doc, 2);
    // with nothing to output a Scene stops itself on its first write
    Scene *f = rig == QStringLiteral("ends") ? new Scene(&doc) : makeScene(doc, fixture->id(), 0, 200);
    if (rig == QStringLiteral("ends"))
        doc.addFunction(f);
    Scene *g = makeScene(doc, fixture->id(), 1, 200);
    auto *show = new Show(&doc);
    doc.addFunction(show);
    if (rig == QStringLiteral("clip") || rig == QStringLiteral("pending clip"))
    {
        auto *clips = new Track(Function::invalidId());
        auto *item = new ShowFunction(show->getLatestShowFunctionId());
        item->setFunctionID(f->id());
        // "clip" covers the destination only, as Play never crossed it
        item->setStartTime(rig == QStringLiteral("clip") ? 2500 : 240);
        item->setDuration(100000);
        clips->addShowFunction(item);
        show->addTrack(clips);
    }

    ShowCommandTrack track;
    if (rig == QStringLiteral("pending clip") || rig == QStringLiteral("running restart") ||
        rig == QStringLiteral("owner restart"))
    {
        QVERIFY(track.insert(ShowCommand::start(1, 10, f->id())));
        QVERIFY(track.insert(ShowCommand::stop(2, 20, f->id())));
        QVERIFY(track.insert(ShowCommand::setIntensity(3, 30, f->id(), 0.8)));
    }
    else if (rig == QStringLiteral("serial") || rig == QStringLiteral("clip"))
    {
        QVERIFY(track.insert(ShowCommand::start(1, 1000, f->id())));
        QVERIFY(track.insert(ShowCommand::stop(2, 2000, f->id())));
        QVERIFY(track.insert(ShowCommand::start(3, 3000, f->id())));
        QVERIFY(track.insert(ShowCommand::setIntensity(4, 4000, f->id(), 0.8)));
    }
    else if (rig == QStringLiteral("owner"))
    {
        QVERIFY(track.insert(ShowCommand::start(1, 1000, f->id())));
        QVERIFY(track.insert(ShowCommand::stop(2, 2000, f->id())));
        QVERIFY(track.insert(ShowCommand::setIntensity(3, 3000, f->id(), 0.8)));
    }
    else
    {
        QVERIFY(track.insert(ShowCommand::start(1, 1000, f->id())));
        if (rig == QStringLiteral("owned"))
            QVERIFY(track.insert(ShowCommand::start(2, 1500, f->id())));
        QVERIFY(track.insert(ShowCommand::start(3, 2000, g->id())));
    }
    // the VC record that makes the jump settle serially lies after the destination
    QVERIFY(track.insert(ShowCommand::setButtonState(9, 100000, QUuid::createUuid(), true)));
    QVERIFY(track.setExtent(200000));
    QVERIFY(show->setCommandTrack(track));
    show->attachControlExecutor(true);
    const auto detach = qScopeGuard([&]() { show->attachControlExecutor(false); });

    const FunctionParent owner(FunctionParent::AutoVCWidget, 42);
    const FunctionParent restarter(FunctionParent::AutoVCWidget, 43);
    if (rig == QStringLiteral("owner") || rig == QStringLiteral("running restart") ||
        rig == QStringLiteral("owner restart"))
    {
        f->start(timer, owner);
        timer->timerTick();
    }
    QSignalSpy fStarts(f, &Function::running);
    QSignalSpy fStops(f, qOverload<quint32>(&Function::stopped));
    QSignalSpy gStarts(g, &Function::running);
    QSignalSpy gStops(g, qOverload<quint32>(&Function::stopped));

    const bool early = rig == QStringLiteral("pending clip") || rig == QStringLiteral("running restart") ||
                       rig == QStringLiteral("owner restart");
    show->start(timer, FunctionParent::master());
    timer->timerTick();
    // a requested forward jump plays its crossed interval serially
    show->requestSeek(early ? 200 : 5000);
    timer->timerTick();
    if (rig == QStringLiteral("running restart"))
    {
        // the Show's Start made it an owner; the first owner lets go
        f->stop(owner);
        timer->timerTick();
        QVERIFY(f->isRunning() && f->stopped());
        f->start(timer, restarter);
    }
    else if (rig == QStringLiteral("owner restart"))
    {
        // the Stop released the Show's share only
        timer->timerTick();
        QVERIFY(f->isRunning() && !f->stopped());
        f->stop(owner);
        f->start(timer, owner);
    }
    if (rig == QStringLiteral("seek"))
        show->requestSeek(0);
    else if (rig == QStringLiteral("stop"))
        show->stop(FunctionParent::master());
    for (int i = 0; i < 10; i++)
        timer->timerTick();

    const auto state = [](Function *function, const QSignalSpy &starts, const QSignalSpy &stops) {
        QString text = function->isRunning() ? QStringLiteral("on@") +
                       QString::number(function->getAttributeValue(Function::Intensity)) : QStringLiteral("off");
        return text + QStringLiteral("/%1/%2").arg(starts.count()).arg(stops.count());
    };
    QCOMPARE(QStringLiteral("F=") + state(f, fStarts, fStops) + QStringLiteral(",G=") + state(g, gStarts, gStops),
             outcome);

    show->stop(FunctionParent::master());
    f->stop(FunctionParent::master());
    g->stop(FunctionParent::master());
    timer->timerTick();
}

void ShowRunner_Test::commandTraversalBoundaries_data()
{
    QTest::addColumn<QString>("rig");
    QTest::addColumn<QString>("midway");
    QTest::addColumn<QString>("outcome");

    // name=state[@intensity]/running() count/stopped() count
    QTest::newRow("D=0 runs its records once") << QStringLiteral("zero") << QString()
        << QStringLiteral("F=on@0.5/1/0,G=off/0/0");
    QTest::newRow("records before the cursor never run, at it once") << QStringLiteral("destination") << QString()
        << QStringLiteral("F=off/0/0,G=on@1/1/0");
    QTest::newRow("unmoved resume runs nothing before the cursor") << QStringLiteral("resume")
        << QStringLiteral("F=off/0/0,G=off/0/0") << QStringLiteral("F=off/0/0,G=off/0/0");
    QTest::newRow("paused move is inert until Play") << QStringLiteral("paused move")
        << QStringLiteral("F=off/0/0,G=off/0/0") << QStringLiteral("F=on@1/1/0,G=off/0/0");
    QTest::newRow("ordinary crossing runs in its own tick") << QStringLiteral("crossing")
        << QStringLiteral("F=on@0.8/1/0,G=off/0/0") << QStringLiteral("F=on@0.8/1/0,G=off/0/0");
    QTest::newRow("live id authored during the restart wait") << QStringLiteral("stalled live")
        << QString() << QStringLiteral("F=on@1/2/1,G=off/0/0");
    QTest::newRow("live id of the previous traversal replays") << QStringLiteral("previous live")
        << QString() << QStringLiteral("F=off/0/0,G=on@1/1/0");
    // Play, a live id, Stop, Play again: all accepted before the first tick
    QTest::newRow("live id of an aborted Play replays") << QStringLiteral("aborted play")
        << QString() << QStringLiteral("F=off/0/0,G=on@1/1/0");
    // a runner exists, then Stop and Play are accepted before its next tick
    QTest::newRow("live id after restarting a running Show") << QStringLiteral("restart")
        << QString() << QStringLiteral("F=off/0/0,G=off/0/0");
    QTest::newRow("live id after two rapid restarts") << QStringLiteral("restart twice")
        << QString() << QStringLiteral("F=off/0/0,G=off/0/0");
    QTest::newRow("live id between two rapid restarts replays") << QStringLiteral("restart aborted")
        << QString() << QStringLiteral("F=off/0/0,G=on@1/1/0");
    QTest::newRow("seek during the restarted traversal replays the live id") << QStringLiteral("restart seek")
        << QString() << QStringLiteral("F=off/0/0,G=on@1/1/0");
    QTest::newRow("stop during the restarted traversal") << QStringLiteral("restart stop")
        << QString() << QStringLiteral("F=off/0/0,G=off/0/0");
    QTest::newRow("live id authored after a requested seek") << QStringLiteral("requested seek live")
        << QString() << QStringLiteral("F=off/0/0,G=off/0/0");
    // External: requestSeek is ignored, the external clock moves on its own
    QTest::newRow("external loop after an ignored seek replays the live id") << QStringLiteral("external loop")
        << QString() << QStringLiteral("F=off/0/0,G=on@1/1/0");
    QTest::newRow("ignored external seek keeps the live id") << QStringLiteral("external ignored seek")
        << QString() << QStringLiteral("F=off/0/0,G=off/0/0");
    // test access: the retired runner's seek lands after Stop, Play and a live id
    QTest::newRow("retired runner seek in flight") << QStringLiteral("retired seek")
        << QString() << QStringLiteral("F=off/0/0,G=off/0/0");
    // Stop, Play and a live id land while preRun installs the first runner
    QTest::newRow("stop inside preRun") << QStringLiteral("stop inside prerun")
        << QString() << QStringLiteral("F=off/0/0,G=off/0/0");
    // a seek request pending across a sync source switch is discarded
    QTest::newRow("autonomous request dropped by external") << QStringLiteral("switch to external")
        << QString() << QStringLiteral("F=off/0/0,G=off/0/0");
    QTest::newRow("external request dropped by autonomous") << QStringLiteral("switch to autonomous")
        << QString() << QStringLiteral("F=on@1/1/0,G=off/0/0");
}

void ShowRunner_Test::commandTraversalBoundaries()
{
    QFETCH(QString, rig);
    QFETCH(QString, midway);
    QFETCH(QString, outcome);

    Doc doc(nullptr);
    auto *timer = doc.masterTimer();
    auto *fixture = makeFixture(doc, 2);
    Scene *f = makeScene(doc, fixture->id(), 0, 200);
    Scene *g = makeScene(doc, fixture->id(), 1, 200);
    auto *show = new Show(&doc);
    doc.addFunction(show);
    if (rig == QStringLiteral("stalled live"))
    {
        auto *clips = new Track(Function::invalidId());
        auto *item = new ShowFunction(show->getLatestShowFunctionId());
        item->setFunctionID(f->id());
        item->setStartTime(0);
        item->setDuration(100000);
        clips->addShowFunction(item);
        show->addTrack(clips);
    }

    ShowCommandTrack track;
    if (rig == QStringLiteral("zero"))
    {
        QVERIFY(track.insert(ShowCommand::start(1, 0, f->id())));
        QVERIFY(track.insert(ShowCommand::setIntensity(2, 0, f->id(), 0.5)));
    }
    else if (rig == QStringLiteral("destination"))
    {
        QVERIFY(track.insert(ShowCommand::start(1, 1000, f->id())));
        QVERIFY(track.insert(ShowCommand::start(2, 2000, g->id())));
    }
    else if (rig == QStringLiteral("resume"))
    {
        QVERIFY(track.insert(ShowCommand::start(1, 1000, f->id())));
        QVERIFY(track.insert(ShowCommand::stop(2, 1500, f->id())));
    }
    else if (rig == QStringLiteral("paused move"))
    {
        QVERIFY(track.insert(ShowCommand::start(1, 1000, f->id())));
    }
    else if (rig == QStringLiteral("switch to autonomous"))
    {
        QVERIFY(track.insert(ShowCommand::start(1, 1100, f->id())));
    }
    else if (rig == QStringLiteral("crossing"))
    {
        QVERIFY(track.insert(ShowCommand::start(1, 101, f->id())));
        QVERIFY(track.insert(ShowCommand::stop(2, 102, f->id())));
        QVERIFY(track.insert(ShowCommand::start(3, 103, f->id())));
        QVERIFY(track.insert(ShowCommand::setIntensity(4, 104, f->id(), 0.8)));
    }
    // the VC record that makes a backward move publish its rollback lies far ahead
    QVERIFY(track.insert(ShowCommand::setButtonState(9, 100000, QUuid::createUuid(), true)));
    QVERIFY(track.setExtent(200000));
    QVERIFY(show->setCommandTrack(track));
    show->attachControlExecutor(true);
    const auto detach = qScopeGuard([&]() { show->attachControlExecutor(false); });

    QSignalSpy fStarts(f, &Function::running);
    QSignalSpy fStops(f, qOverload<quint32>(&Function::stopped));
    QSignalSpy gStarts(g, &Function::running);
    QSignalSpy gStops(g, qOverload<quint32>(&Function::stopped));
    const auto state = [&]() {
        const auto one = [](Function *function, const QSignalSpy &starts, const QSignalSpy &stops) {
            QString text = function->isRunning() ? QStringLiteral("on@") +
                           QString::number(function->getAttributeValue(Function::Intensity)) : QStringLiteral("off");
            return text + QStringLiteral("/%1/%2").arg(starts.count()).arg(stops.count());
        };
        return QStringLiteral("F=") + one(f, fStarts, fStops) + QStringLiteral(",G=") + one(g, gStarts, gStops);
    };
    // the executor acknowledges what it is handed, rollbacks included
    const auto ticks = [timer, show](int count) {
        for (int i = 0; i < count; i++)
        {
            timer->timerTick();
            for (const ShowControlBatch &batch : show->takeControlBatches())
                show->acknowledgeControlBatch(batch.traversal, batch.seq);
        }
    };
    // what the recorder publishes for an input it just executed live
    const auto authorLive = [&](quint32 time) {
        QVERIFY(track.insert(ShowCommand::start(5, time, g->id())));
        QVERIFY(show->setCommandTrack(track, { 5 }));
    };

    QString reached;
    if (rig == QStringLiteral("zero") || rig == QStringLiteral("crossing") ||
        rig == QStringLiteral("paused move") || rig == QStringLiteral("stalled live") ||
        rig == QStringLiteral("previous live") || rig == QStringLiteral("aborted play") ||
        rig.startsWith(QStringLiteral("restart")) || rig == QStringLiteral("requested seek live") ||
        rig.startsWith(QStringLiteral("external")) || rig == QStringLiteral("retired seek") ||
        rig.startsWith(QStringLiteral("switch")) || rig == QStringLiteral("stop inside prerun"))
    {
        if (rig.startsWith(QStringLiteral("external")) || rig == QStringLiteral("switch to autonomous"))
            show->setSyncSource(ShowRunner::External);
        show->start(timer, FunctionParent::master());
    }
    else
        show->start(timer, FunctionParent::master(), 2000);
    if (rig == QStringLiteral("aborted play"))
    {
        authorLive(0);
        show->stop(FunctionParent::master());
        show->start(timer, FunctionParent::master());
    }

    if (rig.startsWith(QStringLiteral("restart")))
    {
        ticks(5);
        for (int i = 0; i < (rig == QStringLiteral("restart twice") ? 2 : 1); i++)
        {
            show->stop(FunctionParent::master());
            show->start(timer, FunctionParent::master());
        }
        authorLive(0);
        if (rig == QStringLiteral("restart aborted"))
        {
            show->stop(FunctionParent::master());
            show->start(timer, FunctionParent::master());
        }
        ticks(1);
        if (rig == QStringLiteral("restart seek"))
            show->requestSeek(0);
        else if (rig == QStringLiteral("restart stop"))
            show->stop(FunctionParent::master());
    }

    if (rig == QStringLiteral("resume"))
    {
        ticks(5);
        reached = state();
        show->setPause(true);
        ticks(3);
        show->setPause(false);
    }
    else if (rig == QStringLiteral("paused move"))
    {
        ticks(2);
        show->setPause(true);
        show->requestSeek(2000);
        ticks(5);
        reached = state();
        show->setPause(false);
    }
    else if (rig == QStringLiteral("crossing"))
    {
        ticks(ticksToReach(120));
        reached = state();
    }
    else if (rig == QStringLiteral("stalled live"))
    {
        ticks(5);
        show->requestSeek(1000);
        // the clip's Scene is still finishing its stop, so the traversal waits
        ticks(1);
        authorLive(1000);
    }
    else if (rig.startsWith(QStringLiteral("external")))
    {
        show->setExternalElapsedTime(1000);
        ticks(2);
        if (rig == QStringLiteral("external ignored seek"))
            authorLive(1100);
        show->requestSeek(1500);
        ticks(1);
        if (rig == QStringLiteral("external loop"))
        {
            authorLive(1000);
            show->setExternalElapsedTime(0);
            ticks(1);
        }
        show->setExternalElapsedTime(1200);
    }
    else if (rig == QStringLiteral("stop inside prerun"))
    {
        // Function::preRun emits running() before the runner is installed
        bool once = true;
        QObject::connect(show, &Function::running, show, [&]() {
            if (!std::exchange(once, false))
                return;
            show->stop(FunctionParent::master());
            show->start(timer, FunctionParent::master());
            authorLive(0);
        }, Qt::DirectConnection);
        ticks(1);
    }
    else if (rig == QStringLiteral("switch to external"))
    {
        ticks(5);
        const quint32 at = show->commandPosition();
        authorLive(at + 200);
        show->requestSeek(1500);
        // the runner is one tick past its reported position: no backward jump
        show->setExternalElapsedTime(at + MasterTimer::tick());
        show->setSyncSource(ShowRunner::External);
        ticks(1);
        show->setExternalElapsedTime(at + 300);
    }
    else if (rig == QStringLiteral("switch to autonomous"))
    {
        show->setExternalElapsedTime(1000);
        ticks(2);
        authorLive(1150);
        show->requestSeek(0);
        show->setSyncSource(ShowRunner::Autonomous);
    }
    else if (rig == QStringLiteral("retired seek"))
    {
        ticks(5);
        ShowRunner *retired = show->m_runner;
        show->stop(FunctionParent::master());
        show->start(timer, FunctionParent::master());
        authorLive(0);
        retired->seekTo(0, true);
    }
    else if (rig == QStringLiteral("requested seek live"))
    {
        ticks(5);
        // accepted after the seek request, before the runner consumes it
        show->requestSeek(0);
        authorLive(0);
    }
    else if (rig == QStringLiteral("previous live"))
    {
        ticks(3);
        authorLive(500);
        ticks(1);
        show->requestSeek(0);
        ticks(ticksToReach(600));
    }
    ticks(10);

    QCOMPARE(reached, midway);
    QCOMPARE(state(), outcome);

    show->stop(FunctionParent::master());
    f->stop(FunctionParent::master());
    g->stop(FunctionParent::master());
    timer->timerTick();
}

void ShowRunner_Test::commandJumpStopEndsDestinationClip()
{
    Doc doc(nullptr);
    auto *timer = doc.masterTimer();
    auto *fixture = makeFixture(doc, 1);
    Scene *f = makeScene(doc, fixture->id(), 0, 200);
    auto *show = new Show(&doc);
    doc.addFunction(show);
    auto *clips = new Track(Function::invalidId());
    auto *item = new ShowFunction(show->getLatestShowFunctionId());
    item->setFunctionID(f->id());
    item->setStartTime(100);
    item->setDuration(400); // clip 100 - 500
    clips->addShowFunction(item);
    show->addTrack(clips);

    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::stop(1, 20, f->id())));
    QVERIFY(track.insert(ShowCommand::setButtonState(2, 100000, QUuid::createUuid(), true)));
    QVERIFY(track.setExtent(200000));
    QVERIFY(show->setCommandTrack(track));
    show->attachControlExecutor(true);
    const auto detach = qScopeGuard([&]() { show->attachControlExecutor(false); });
    QSignalSpy starts(f, &Function::running);
    QSignalSpy stops(f, qOverload<quint32>(&Function::stopped));

    // the destination clip is scheduled first, then the crossed Stop lands on it
    show->start(timer, FunctionParent::master());
    timer->timerTick();
    show->requestSeek(200);
    for (int i = 0; i < 5; i++)
        timer->timerTick();
    const bool runningAt300 = f->isRunning();
    for (int i = 0; i < 20; i++)
        timer->timerTick();

    QCOMPARE(runningAt300, false);
    QVERIFY(!f->isRunning());
    QCOMPARE(starts.count(), 1);
    QCOMPARE(stops.count(), 1);

    show->stop(FunctionParent::master());
    timer->timerTick();
}

void ShowRunner_Test::commandCancelledJumpRunsNoLegacyRemainder_data()
{
    QTest::addColumn<bool>("seekPending");

    QTest::newRow("seek") << true;
    QTest::newRow("stop") << false;
}

void ShowRunner_Test::commandCancelledJumpRunsNoLegacyRemainder()
{
    QFETCH(bool, seekPending);

    Doc doc(nullptr);
    auto *timer = doc.masterTimer();
    auto *fixture = makeFixture(doc, 1);
    Scene *target = makeScene(doc, fixture->id(), 0, 200);
    auto *show = new Show(&doc);
    doc.addFunction(show);
    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::setButtonState(1, 100, QUuid::createUuid(), true)));
    QVERIFY(track.insert(ShowCommand::setIntensity(2, 200, target->id(), 0.3)));
    QVERIFY(track.setExtent(4000));
    QVERIFY(show->setCommandTrack(track));
    show->attachControlExecutor(true);
    const auto detach = qScopeGuard([&]() { show->attachControlExecutor(false); });

    target->start(timer, FunctionParent::master());
    show->start(timer, FunctionParent::master());
    timer->timerTick();
    show->requestSeek(1000);
    timer->timerTick();
    const QVector<ShowControlBatch> published = show->takeControlBatches();
    QCOMPARE(published.count(), 1);
    QCOMPARE(published.first().commands.count(), 1);

    // acknowledged, then cancelled before the timer consumes the acknowledgement;
    // only a backward request cancels
    show->acknowledgeControlBatch(published.first().traversal, published.first().seq);
    if (seekPending)
        show->requestSeek(500);
    else
        show->stop(FunctionParent::master());
    show->m_runner->dispatchCommandEffects({});
    timer->timerTick();

    QCOMPARE(target->getAttributeValue(Function::Intensity), qreal(1.0));

    show->stop(FunctionParent::master());
    target->stop(FunctionParent::master());
    timer->timerTick();
}

void ShowRunner_Test::commandJumpNaturalEndDrains()
{
    Doc doc(nullptr);
    auto *timer = doc.masterTimer();
    auto *fixture = makeFixture(doc, 2);
    Scene *f = makeScene(doc, fixture->id(), 0, 200);
    Scene *g = makeScene(doc, fixture->id(), 1, 200);
    auto *show = new Show(&doc);
    doc.addFunction(show);
    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::start(1, 100, f->id())));
    QVERIFY(track.insert(ShowCommand::stop(2, 200, f->id())));
    QVERIFY(track.insert(ShowCommand::setButtonState(3, 250, QUuid::createUuid(), true)));
    QVERIFY(track.insert(ShowCommand::start(4, 300, g->id())));
    QVERIFY(track.setExtent(300));
    QVERIFY(show->setCommandTrack(track));
    show->attachControlExecutor(true);
    const auto detach = qScopeGuard([&]() { show->attachControlExecutor(false); });
    QSignalSpy finished(show, &Show::showFinished);
    QSignalSpy fStarts(f, &Function::running);
    QSignalSpy fStops(f, qOverload<quint32>(&Function::stopped));
    QSignalSpy gStarts(g, &Function::running);
    int gLiveAtFinish = -1;
    QObject probe;
    connect(show, &Show::showFinished, &probe, [&]() { gLiveAtFinish = int(g->isRunning() && !g->stopped()); });

    // a forward seek to the extent: the whole content is crossed, and it holds the end
    show->start(timer, FunctionParent::master());
    timer->timerTick();
    show->requestSeek(300);
    int batches = 0;
    int finishedWhileOwed = 0;
    for (int i = 0; i < 20 && finished.isEmpty(); i++)
    {
        timer->timerTick();
        for (const ShowControlBatch &batch : show->takeControlBatches())
        {
            batches++;
            finishedWhileOwed += finished.count();
            show->acknowledgeControlBatch(batch.traversal, batch.seq);
        }
    }
    timer->timerTick();

    QCOMPARE(finished.count(), 1);
    QCOMPARE(finishedWhileOwed, 0);
    // the final Start settled natively before the Show ended
    QCOMPARE(gLiveAtFinish, 1);
    QCOMPARE(batches, 1);
    QCOMPARE(fStarts.count(), 1);
    QCOMPARE(fStops.count(), 1);
    QCOMPARE(gStarts.count(), 1);

    f->stop(FunctionParent::master());
    g->stop(FunctionParent::master());
    timer->timerTick();
}

void ShowRunner_Test::commandPausedDrainsCrossedWork_data()
{
    QTest::addColumn<int>("seekWhilePaused"); // -1: none
    QTest::addColumn<int>("fStarted");

    QTest::newRow("acknowledged") << -1 << 1;
    // a backward request cancels the traversal: its owed work is dropped
    QTest::newRow("backward seek requested, then acknowledged") << 50 << 0;
    // a forward request cancels nothing: the owed work still drains
    QTest::newRow("forward seek requested, then acknowledged") << 1500 << 1;
}

void ShowRunner_Test::commandPausedDrainsCrossedWork()
{
    QFETCH(int, seekWhilePaused);
    QFETCH(int, fStarted);

    Doc doc(nullptr);
    auto *timer = doc.masterTimer();
    auto *fixture = makeFixture(doc, 2);
    Scene *f = makeScene(doc, fixture->id(), 0, 200);
    Scene *g = makeScene(doc, fixture->id(), 1, 200);
    auto *show = new Show(&doc);
    doc.addFunction(show);
    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::setButtonState(1, 100, QUuid::createUuid(), true)));
    QVERIFY(track.insert(ShowCommand::start(2, 100, f->id())));
    QVERIFY(track.insert(ShowCommand::start(3, 260, g->id())));
    QVERIFY(track.setExtent(4000));
    QVERIFY(show->setCommandTrack(track));
    show->attachControlExecutor(true);
    const auto detach = qScopeGuard([&]() { show->attachControlExecutor(false); });
    QSignalSpy fStarts(f, &Function::running);
    QSignalSpy gStarts(g, &Function::running);

    show->start(timer, FunctionParent::master());
    QVector<ShowControlBatch> published;
    for (int i = 0; i < ticksToReach(100) + 1 && published.isEmpty(); i++)
    {
        timer->timerTick();
        published = show->takeControlBatches();
    }
    QCOMPARE(published.count(), 1);
    timer->timerTick();
    QCOMPARE(fStarts.count(), 0);

    // Pause while F waits behind the batch, which the GUI finishes afterwards
    show->setPause(true);
    const quint32 pausedAt = show->commandPosition();
    const bool pendingWhilePaused = show->commandWorkPending();
    QSignalSpy drained(show, &Show::commandWorkDrained);
    // a cancelled traversal is dropped, never waited for
    if (seekWhilePaused >= 0)
        show->requestSeek(quint32(seekWhilePaused));
    show->acknowledgeControlBatch(published.first().traversal, published.first().seq);
    for (int i = 0; i < 5; i++)
        timer->timerTick();

    QCOMPARE(fStarts.count(), fStarted);
    QCOMPARE(gStarts.count(), 0);
    QCOMPARE(show->commandPosition(), pausedAt);
    QVERIFY(pendingWhilePaused);
    QVERIFY(!show->commandWorkPending());
    QCOMPARE(drained.count(), 1);

    // the seek waits for Resume
    show->setPause(false);
    timer->timerTick();
    timer->timerTick();
    if (seekWhilePaused < 0)
        QVERIFY(show->commandPosition() > pausedAt && show->commandPosition() < 1500);
    else if (quint32(seekWhilePaused) < pausedAt)
        QVERIFY(show->commandPosition() < pausedAt);
    else
        QVERIFY(show->commandPosition() >= quint32(seekWhilePaused));

    show->stop(FunctionParent::master());
    f->stop(FunctionParent::master());
    timer->timerTick();
}

void ShowRunner_Test::commandCrossedWorkVisibleBeforeItsFirstOperation()
{
    Doc doc(nullptr);
    auto *timer = doc.masterTimer();
    auto *fixture = makeFixture(doc, 2);
    Scene *a = makeScene(doc, fixture->id(), 0, 200);
    Scene *f = makeScene(doc, fixture->id(), 1, 200);
    auto *show = new Show(&doc);
    doc.addFunction(show);
    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::setIntensity(1, 100, a->id(), 0.5)));
    QVERIFY(track.insert(ShowCommand::start(2, 100, f->id())));
    QVERIFY(track.setExtent(4000));
    QVERIFY(show->setCommandTrack(track));

    a->start(timer, FunctionParent::master());
    show->start(timer, FunctionParent::master());
    timer->timerTick();

    // the GUI looks while the first crossed operation takes native effect
    QVector<ShowCommand> owedDuringFirst;
    bool looked = false;
    QObject probe;
    connect(a, &Function::attributeChanged, &probe, [&]() {
        if (!looked)
            owedDuringFirst = show->pendingCommandWork();
        looked = true;
    }, Qt::DirectConnection);
    for (int i = 0; i < ticksToReach(100) && !looked; i++)
        timer->timerTick();

    QVERIFY(looked);
    QVERIFY(std::any_of(owedDuringFirst.cbegin(), owedDuringFirst.cend(), [&](const ShowCommand &cmd) {
        return cmd.action == ShowCommandAction::Start && cmd.functionId == f->id();
    }));

    QObject::disconnect(a, nullptr, &probe, nullptr);
    show->stop(FunctionParent::master());
    a->stop(FunctionParent::master());
    f->stop(FunctionParent::master());
    timer->timerTick();
}

void ShowRunner_Test::pauseReachesClipsOnTheTimerThread()
{
    Doc doc(nullptr);
    auto *timer = doc.masterTimer();
    auto *clip = new PauseProbeFunction(&doc);
    doc.addFunction(clip);
    auto *show = new Show(&doc);
    doc.addFunction(show);
    auto *clips = new Track(Function::invalidId());
    auto *item = new ShowFunction(show->getLatestShowFunctionId());
    item->setFunctionID(clip->id());
    item->setStartTime(0);
    item->setDuration(60000);
    clips->addShowFunction(item);
    show->addTrack(clips);

    timer->start();
    show->start(timer, FunctionParent::master());
    QTRY_VERIFY(clip->isRunning());

    // the timer thread owns the runner's clip queue, which a paused tick drains into
    show->setPause(true);
    QTRY_VERIFY(clip->isPaused());
    QThread *pausedOn = clip->pauseThread.load();
    show->setPause(false);
    QTRY_VERIFY(!clip->isPaused());
    QThread *resumedOn = clip->pauseThread.load();

    show->stop(FunctionParent::master());
    QTRY_VERIFY(!show->isRunning());
    timer->stop();

    QVERIFY(pausedOn != nullptr && pausedOn != QThread::currentThread());
    QVERIFY(resumedOn != nullptr && resumedOn != QThread::currentThread());
}

void ShowRunner_Test::commandIgnoredSeekStrandsNothing_data()
{
    QTest::addColumn<QString>("seek");
    QTest::addColumn<bool>("liveAfterRequest");
    QTest::addColumn<int>("fStarted");
    QTest::addColumn<int>("liveStarted");
    QTest::addColumn<bool>("prefixReplayed");

    // a backward request cancelled, then discarded by the switch: its owed work is dropped, no seek
    QTest::newRow("autonomous request, switched to external") << QStringLiteral("switched") << false << 0 << 0 << false;
    QTest::newRow("switched, live id accepted after the request") << QStringLiteral("switched") << true << 0 << 0 << false;
    // controls: a consumed forward request cancels nothing and keeps the live mark,
    // and crosses no VC record
    QTest::newRow("autonomous request consumed") << QStringLiteral("consumed") << false << 1 << 0 << false;
    QTest::newRow("ignored under external") << QStringLiteral("external") << false << 1 << 0 << false;
}

void ShowRunner_Test::commandIgnoredSeekStrandsNothing()
{
    QFETCH(QString, seek);
    QFETCH(bool, liveAfterRequest);
    QFETCH(int, fStarted);
    QFETCH(int, liveStarted);
    QFETCH(bool, prefixReplayed);

    Doc doc(nullptr);
    auto *timer = doc.masterTimer();
    auto *fixture = makeFixture(doc, 3);
    Scene *f = makeScene(doc, fixture->id(), 0, 200);
    Scene *g = makeScene(doc, fixture->id(), 1, 200);
    Scene *live = makeScene(doc, fixture->id(), 2, 200);
    auto *show = new Show(&doc);
    doc.addFunction(show);
    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::setButtonState(1, 100, QUuid::createUuid(), true)));
    QVERIFY(track.insert(ShowCommand::start(2, 100, f->id())));
    QVERIFY(track.insert(ShowCommand::start(3, 400, live->id())));
    QVERIFY(track.insert(ShowCommand::start(4, 600, g->id())));
    QVERIFY(track.setExtent(4000));
    QVERIFY(show->setCommandTrack(track));
    show->attachControlExecutor(true);
    const auto detach = qScopeGuard([&]() { show->attachControlExecutor(false); });
    QSignalSpy fStarts(f, &Function::running);
    QSignalSpy liveStarts(live, &Function::running);
    QSignalSpy gStarts(g, &Function::running);

    if (seek == QLatin1String("external"))
        show->setSyncSource(ShowRunner::External);
    show->start(timer, FunctionParent::master());
    if (seek == QLatin1String("external"))
    {
        // the first sample begins the traversal, the next one crosses
        show->setExternalElapsedTime(0);
        timer->timerTick();
        show->setExternalElapsedTime(200);
    }
    QVector<ShowControlBatch> published;
    for (int i = 0; i < ticksToReach(200) && published.isEmpty(); i++)
    {
        timer->timerTick();
        published = show->takeControlBatches();
    }
    QCOMPARE(published.count(), 1);
    // the Start at 400 was executed live: it must not echo
    if (!liveAfterRequest)
        QVERIFY(show->setCommandTrack(track, { 3 }));

    // the button batch and Start F are still owed; only a backward request cancels them
    show->requestSeek(seek == QLatin1String("switched") ? 0 : 1500);
    if (seek == QLatin1String("switched"))
    {
        show->setSyncSource(ShowRunner::External);
        show->setExternalElapsedTime(200);
    }
    if (liveAfterRequest)
        QVERIFY(show->setCommandTrack(track, { 3 }));
    show->acknowledgeControlBatch(published.first().traversal, published.first().seq);
    int laterBatches = 0;
    const auto tick = [&](int count) {
        for (int i = 0; i < count; i++)
        {
            timer->timerTick();
            for (const ShowControlBatch &batch : show->takeControlBatches())
            {
                laterBatches++;
                show->acknowledgeControlBatch(batch.traversal, batch.seq);
            }
        }
    };
    tick(3);
    if (seek != QLatin1String("consumed"))
        show->setExternalElapsedTime(700);
    tick(10);

    // the later crossing applies, nothing of the cancelled traversal does
    QCOMPARE(gStarts.count(), 1);
    QCOMPARE(fStarts.count(), fStarted);
    QCOMPARE(liveStarts.count(), liveStarted);
    QCOMPARE(laterBatches > 0, prefixReplayed);
    QCOMPARE(show->commandPosition() >= 1500, seek == QLatin1String("consumed"));
    QVERIFY(!show->commandWorkPending());

    show->stop(FunctionParent::master());
    f->stop(FunctionParent::master());
    g->stop(FunctionParent::master());
    live->stop(FunctionParent::master());
    timer->timerTick();
}

void ShowRunner_Test::commandJumpDispatchesOnlyItsWindow_data()
{
    QTest::addColumn<QString>("script");
    QTest::addColumn<QStringList>("segments");
    QTest::addColumn<QString>("starts");

    // VC records: 1@0 2@5000 3@53700 4@120000 5@139300 21@139400 6@140000
    // 7,8@141000 (Order tie) 9@143000 10@150000; legacy Start S@53700 and
    // F@139400 (before 21); ordinary clip G over the whole song.
    // A segment lists the dispatched VC ids in order; R<T> is a rollback batch to T.
    const QList<std::tuple<const char *, const char *, QStringList, const char *>> rows{
        { "external first sample 0 runs the event at 0 once", "playext;ext:0;cut;ext:0;cut",
          { "1", "" }, "S=0,F=0,G=1" },
        { "external sample present at Play, cursor 0", "ext:120000;playext;ext:120000;cut",
          { "4" }, "S=0,F=0,G=1" },
        { "external stale sample, then a fresh one", "ext:5000;playext;tick:3;cut;ext:120000;cut",
          { "", "4" }, "S=0,F=0,G=1" },
        { "external: two fresh samples before the first write, the earlier one anchors",
          "startext;set:0;set:5000;tick:4;cut;set:53700;tick:4;cut", { "1,2", "3" }, "S=1,F=0,G=1" },
        { "external: a sample present at Play anchors nothing", "set:0;startext;set:5000;tick:4;cut",
          { "2" }, "S=0,F=0,G=1" },
        { "external forward crossing", "playext;ext:139300;cut;ext:141000;cut",
          { "5", "21,6,7,8" }, "S=0,F=1,G=1" },
        { "user loop 139300-143000", "playext;ext:0;cut;ext:53700;cut;ext:139300;cut;ext:143000;cut;"
                                     "ext:139300;cut;ext:143000;cut",
          { "1", "2,3", "4,5", "21,6,7,8,9", "R139300,5", "21,6,7,8,9" }, "S=1,F=2,G=2" },
        { "Play from T", "play:120000;cut", { "4" }, "S=0,F=0,G=1" },
        { "Play from 0", "play:0;cut", { "1" }, "S=0,F=0,G=1" },
        { "requested forward seek", "play:139300;cut;seek:141000;cut", { "5", "21,6,7,8" }, "S=0,F=1,G=2" },
        { "requested backward seek", "play:141000;cut;seek:139300;cut", { "7,8", "R139300,5" }, "S=0,F=0,G=2" },
        { "seek to 0 runs the event at 0 once", "play:5000;cut;seek:0;cut;tick:3;cut",
          { "2", "R0,1", "" }, "S=0,F=0,G=2" },
        { "backward, then forward before the consume", "play:140000;cut;req:139300;seek:143000;cut",
          { "6", "R143000,9" }, "S=0,F=0,G=2" },
        { "forward, then backward before the consume", "play:140000;cut;req:143000;seek:139300;cut",
          { "6", "R139300,5" }, "S=0,F=0,G=2" },
        { "forward, then forward keeps the latest", "play:139300;cut;req:141000;seek:143000;cut",
          { "5", "21,6,7,8,9" }, "S=0,F=1,G=2" },
        { "forward request passed before its consume", "play:139940;tick:1;cut;staleseek:140000;tick:5;cut",
          { "6", "" }, "S=0,F=0,G=1" },
        { "paused forward request", "play:139300;cut;pause;req:141000;tick:3;cut;resume;cut",
          { "5", "", "21,6,7,8" }, "S=0,F=1,G=2" },
        { "paused backward request", "play:141000;cut;pause;req:139300;tick:3;cut;resume;cut",
          { "7,8", "", "R139300,5" }, "S=0,F=0,G=2" },
        { "forward request keeps the held batch, settles F first",
          "hold;play:139300;cut;seek:141000;cut;ack;cut", { "5", "", "21,6,7,8" }, "S=0,F=1,G=2" },
        { "backward request drops the held batch", "hold;play:141000;cut;req:139300;ack;cut",
          { "7,8", "R139300,5" }, "S=0,F=0,G=2" },
        { "live mark survives a forward seek, plays after a backward one",
          "play:139300;cut;live:22@140500;tick:1;cut;seek:141000;cut;seek:139300;cut;tick:90;cut",
          { "5", "", "21,6,7,8", "R139300,5", "21,6,22,7,8" }, "S=0,F=2,G=3" },
        { "capture between a backward request and its consume", "play:141000;cut;req:139300;live:23@139300;tick:4;cut",
          { "7,8", "R139300,5" }, "S=0,F=0,G=2" },
    };
    for (const auto &row : rows)
        QTest::newRow(std::get<0>(row)) << QString::fromLatin1(std::get<1>(row)) << std::get<2>(row)
                                        << QString::fromLatin1(std::get<3>(row));
}

void ShowRunner_Test::commandJumpDispatchesOnlyItsWindow()
{
    QFETCH(QString, script);
    QFETCH(QStringList, segments);
    QFETCH(QString, starts);

    Doc doc(nullptr);
    auto *timer = doc.masterTimer();
    auto *fixture = makeFixture(doc, 3);
    Scene *s = makeScene(doc, fixture->id(), 0, 200);
    Scene *f = makeScene(doc, fixture->id(), 1, 200);
    Scene *g = makeScene(doc, fixture->id(), 2, 200);
    auto *show = new Show(&doc);
    doc.addFunction(show);
    auto *clips = new Track(Function::invalidId());
    auto *item = new ShowFunction(show->getLatestShowFunctionId());
    item->setFunctionID(g->id());
    item->setStartTime(0);
    item->setDuration(200000);
    clips->addShowFunction(item);
    show->addTrack(clips);

    ShowCommandTrack track;
    const auto vc = [&track](quint32 id, quint32 time) {
        return track.insert(ShowCommand::setButtonState(id, time, QUuid::createUuid(), true));
    };
    QVERIFY(vc(1, 0) && vc(2, 5000) && vc(3, 53700) && vc(4, 120000) && vc(5, 139300));
    QVERIFY(track.insert(ShowCommand::start(11, 53700, s->id())));
    QVERIFY(track.insert(ShowCommand::start(20, 139400, f->id())));
    QVERIFY(vc(21, 139400) && vc(6, 140000) && vc(7, 141000) && vc(8, 141000) && vc(9, 143000) && vc(10, 150000));
    QVERIFY(track.setExtent(200000));
    QVERIFY(show->setCommandTrack(track));
    show->attachControlExecutor(true);
    const auto detach = qScopeGuard([&]() { show->attachControlExecutor(false); });
    QSignalSpy sStarts(s, &Function::running);
    QSignalSpy fStarts(f, &Function::running);
    QSignalSpy gStarts(g, &Function::running);

    // the GUI executor: takes every batch, acknowledges it unless holding
    bool holding = false;
    QVector<ShowControlBatch> held;
    QStringList dispatched;
    QStringList seen;
    const auto tick = [&](int count) {
        for (int i = 0; i < count; i++)
        {
            timer->timerTick();
            for (const ShowControlBatch &batch : show->takeControlBatches())
            {
                // a rollback carries its target and nothing else
                if (batch.rollbackTo.has_value())
                    dispatched.append(QStringLiteral("R%1").arg(*batch.rollbackTo) +
                                      (batch.commands.isEmpty() ? QString() : QStringLiteral("+commands")));
                for (const ShowCommand &cmd : batch.commands)
                    dispatched.append(QString::number(cmd.id));
                if (holding)
                    held.append(batch);
                else
                    show->acknowledgeControlBatch(batch.traversal, batch.seq);
            }
        }
    };

    for (const QString &step : script.split(QLatin1Char(';')))
    {
        const QString verb = step.section(QLatin1Char(':'), 0, 0);
        const QString arg = step.section(QLatin1Char(':'), 1);
        if (verb == QLatin1String("play"))
        {
            show->start(timer, FunctionParent::master(), arg.toUInt());
            tick(4);
        }
        else if (verb == QLatin1String("playext"))
        {
            show->setSyncSource(ShowRunner::External);
            show->start(timer, FunctionParent::master());
            tick(2);
        }
        else if (verb == QLatin1String("ext"))
        {
            show->setExternalElapsedTime(arg.toUInt());
            tick(6);
        }
        else if (verb == QLatin1String("startext"))
        {
            show->setSyncSource(ShowRunner::External);
            show->start(timer, FunctionParent::master());
        }
        else if (verb == QLatin1String("set"))
            show->setExternalElapsedTime(arg.toUInt());
        else if (verb == QLatin1String("req"))
            show->requestSeek(arg.toUInt());
        else if (verb == QLatin1String("seek"))
        {
            show->requestSeek(arg.toUInt());
            tick(4);
        }
        else if (verb == QLatin1String("staleseek"))
        {
            // the GUI read the position one tick before the runner moved past arg
            const quint32 reached = show->m_commandPosition.load();
            QVERIFY(reached >= arg.toUInt());
            show->m_commandPosition.store(arg.toUInt() - 1);
            show->requestSeek(arg.toUInt());
            show->m_commandPosition.store(reached);
        }
        else if (verb == QLatin1String("tick"))
            tick(arg.toInt());
        else if (verb == QLatin1String("pause"))
        {
            show->setPause(true);
            tick(1);
        }
        else if (verb == QLatin1String("resume"))
        {
            show->setPause(false);
            tick(4);
        }
        else if (verb == QLatin1String("hold"))
            holding = true;
        else if (verb == QLatin1String("ack"))
        {
            holding = false;
            for (const ShowControlBatch &batch : std::as_const(held))
                show->acknowledgeControlBatch(batch.traversal, batch.seq);
            held.clear();
            tick(4);
        }
        else if (verb == QLatin1String("live"))
        {
            // what the recorder publishes for an input it just executed live
            const quint32 id = arg.section(QLatin1Char('@'), 0, 0).toUInt();
            const quint32 time = arg.section(QLatin1Char('@'), 1, 1).toUInt();
            QVERIFY(track.insert(ShowCommand::setButtonState(id, time, QUuid::createUuid(), true)));
            QVERIFY(show->setCommandTrack(track, { id }));
        }
        else if (verb == QLatin1String("cut"))
            seen.append(std::exchange(dispatched, QStringList()).join(QLatin1Char(',')));
        else
            QFAIL(qPrintable(QStringLiteral("unknown verb ") + step));
    }

    QCOMPARE(seen, segments);
    QCOMPARE(QStringLiteral("S=%1,F=%2,G=%3").arg(sStarts.count()).arg(fStarts.count()).arg(gStarts.count()),
             starts);

    show->stop(FunctionParent::master());
    timer->timerTick();
}

void ShowRunner_Test::commandBackwardRollsBackOwnLegacyEffects_data()
{
    QTest::addColumn<QString>("rig");
    QTest::addColumn<QString>("before");  // F before the jump
    QTest::addColumn<QString>("after");   // F and clip G right after it
    QTest::addColumn<QString>("later");   // F once the next pass reached 450

    // F=state[@intensity]/running() count/stopped() count; window (200, 600]
    QTest::newRow("a Start in the window stops") << QStringLiteral("start in window")
        << QStringLiteral("F=on@1/1/0") << QStringLiteral("F=off/1/1,G=on/2") << QStringLiteral("F=on@1/2/1");
    QTest::newRow("a Stop in the window starts again") << QStringLiteral("stop in window")
        << QStringLiteral("F=off/1/1") << QStringLiteral("F=on@1/2/1,G=on/2") << QStringLiteral("F=off/2/2");
    QTest::newRow("a Start before the window keeps running") << QStringLiteral("start before")
        << QStringLiteral("F=on@1/1/0") << QStringLiteral("F=on@1/1/0,G=on/2") << QStringLiteral("F=on@1/1/0");
    QTest::newRow("an intensity in the window returns") << QStringLiteral("intensity in window")
        << QStringLiteral("F=on@0.3/1/0") << QStringLiteral("F=on@1/1/0,G=on/2") << QStringLiteral("F=on@0.3/1/0");
    QTest::newRow("a forward jump keeps F running") << QStringLiteral("forward")
        << QStringLiteral("F=on@1/1/0") << QStringLiteral("F=on@1/1/0,G=on/2") << QStringLiteral("F=on@1/1/0");
    QTest::newRow("Show Stop stops both") << QStringLiteral("stop")
        << QStringLiteral("F=on@1/1/0") << QStringLiteral("F=off/1/1,G=off/1") << QStringLiteral("F=off/1/1");
}

void ShowRunner_Test::commandBackwardRollsBackOwnLegacyEffects()
{
    QFETCH(QString, rig);
    QFETCH(QString, before);
    QFETCH(QString, after);
    QFETCH(QString, later);

    Doc doc(nullptr);
    auto *timer = doc.masterTimer();
    auto *fixture = makeFixture(doc, 2);
    Scene *f = makeScene(doc, fixture->id(), 0, 200);
    Scene *g = makeScene(doc, fixture->id(), 1, 200);
    auto *show = new Show(&doc);
    doc.addFunction(show);
    // the ordinary clip G beside the command-owned F
    auto *clips = new Track(Function::invalidId());
    auto *item = new ShowFunction(show->getLatestShowFunctionId());
    item->setFunctionID(g->id());
    item->setStartTime(0);
    item->setDuration(5000);
    clips->addShowFunction(item);
    show->addTrack(clips);

    ShowCommandTrack track;
    if (rig == QStringLiteral("start in window"))
        QVERIFY(track.insert(ShowCommand::start(1, 400, f->id())));
    else if (rig == QStringLiteral("stop in window"))
    {
        QVERIFY(track.insert(ShowCommand::start(1, 100, f->id())));
        QVERIFY(track.insert(ShowCommand::stop(2, 400, f->id())));
    }
    else if (rig == QStringLiteral("intensity in window"))
    {
        QVERIFY(track.insert(ShowCommand::start(1, 100, f->id())));
        QVERIFY(track.insert(ShowCommand::setIntensity(2, 400, f->id(), 0.3)));
    }
    else
        QVERIFY(track.insert(ShowCommand::start(1, 100, f->id())));
    QVERIFY(track.setExtent(5000));
    QVERIFY(show->setCommandTrack(track));

    QSignalSpy fStarts(f, &Function::running);
    QSignalSpy fStops(f, qOverload<quint32>(&Function::stopped));
    QSignalSpy gStarts(g, &Function::running);
    const auto ticks = [timer](int count) {
        for (int i = 0; i < count; i++)
            timer->timerTick();
    };
    const auto fState = [&]() {
        QString text = f->isRunning() && !f->stopped()
                       ? QStringLiteral("on@") + QString::number(f->getAttributeValue(Function::Intensity))
                       : QStringLiteral("off");
        return QStringLiteral("F=") + text + QStringLiteral("/%1/%2").arg(fStarts.count()).arg(fStops.count());
    };

    show->start(timer, FunctionParent::master());
    const bool forward = rig == QStringLiteral("forward");
    ticks(ticksToReach(forward ? 300 : 600) + 1);
    const QString reachedBefore = fState();

    if (rig == QStringLiteral("stop"))
        show->stop(FunctionParent::master());
    else
        show->requestSeek(forward ? 600 : 200);
    ticks(4);
    const QString reachedAfter = fState() + QStringLiteral(",G=") +
                                 (g->isRunning() && !g->stopped() ? QStringLiteral("on") : QStringLiteral("off")) +
                                 QStringLiteral("/%1").arg(gStarts.count());
    for (int i = 0; i < 100 && show->isRunning() && show->commandPosition() < 450; i++)
        timer->timerTick();
    ticks(2);
    const QString reachedLater = fState();

    QCOMPARE(reachedBefore, before);
    QCOMPARE(reachedAfter, after);
    QCOMPARE(reachedLater, later);

    show->stop(FunctionParent::master());
    f->stop(FunctionParent::master());
    timer->timerTick();
}

QTEST_APPLESS_MAIN(ShowRunner_Test)
