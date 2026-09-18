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
#define private public
#include "showrunner.h"
#undef private
#include "show.h"
#include "track.h"
#include "scene.h"
#include "doc.h"
#include "mastertimer.h"
#include "showrunner_test.h"

void ShowRunner_Test::initTestCase()
{
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

QTEST_APPLESS_MAIN(ShowRunner_Test)
