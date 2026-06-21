/*
  Q Light Controller Plus - Unit test
  vdjbridge_test.cpp
*/

#include <QtTest>
#include <QSignalSpy>

// Access private members for test setup
#define private public
#include "showfactory.h"
#include "mastertimer.h"
#undef private

#include "vdjbridge_test.h"
#include "vdjbridge.h"
#include "vdjdeckmodel.h"
#include "songloadtracker.h"

#include "doc.h"
#include "show.h"
#include "scene.h"
#include "track.h"
#include "showfunction.h"
#include "function.h"

void VdjBridge_Test::initialState()
{
    VdjBridge b;
    QCOMPARE(b.connected(), false);
    QCOMPARE(b.beatCount(), 0);
}

void VdjBridge_Test::beatTicksCounterAndConnected()
{
    VdjBridge b;
    QSignalSpy connectedSpy(&b, &VdjBridge::connectedChanged);
    QSignalSpy beatSpy(&b, &VdjBridge::beatReceived);

    b.onBeat();

    QCOMPARE(b.connected(), true);
    QCOMPARE(b.beatCount(), 1);
    QCOMPARE(connectedSpy.count(), 1);
    QCOMPARE(beatSpy.count(), 1);

    b.onBeat();
    QCOMPARE(b.beatCount(), 2);
    // connected stays true — no extra connectedChanged emission
    QCOMPARE(connectedSpy.count(), 1);
}

// --- Helper: create a Doc + Show with enough duration for testing ---

struct TestFixture {
    Doc *doc;
    VdjBridge *bridge;
    Show *show;
    Scene *scene;

    TestFixture()
    {
        doc = new Doc(nullptr, 4);
        bridge = new VdjBridge();
        bridge->setDoc(doc);

        // Create a Show with External sync and 5-minute duration
        show = new Show(doc);
        show->setName("Test Artist - Test Song");
        show->setSyncSource(1); // External
        doc->addFunction(show);

        scene = new Scene(doc);
        doc->addFunction(scene);

        Track *track = new Track(Function::invalidId(), show);
        track->setName("Audio");
        show->addTrack(track);

        ShowFunction *sf = track->createShowFunction(scene->id());
        sf->setStartTime(0);
        sf->setDuration(300000); // 5 minutes — won't auto-stop

        // Inject filepath→showId into ShowFactory's private mapping
        ShowFactory *factory = bridge->showFactory();
        factory->m_filepathToShowId.insert("/music/test.mp3", show->id());
        factory->m_createdShows.insert("/music/test.mp3");

        // Set deck 0's filepath via the bridge (onDeckTrigger is a private slot)
        QMetaObject::invokeMethod(bridge, "onDeckTrigger",
            Q_ARG(int, 0),
            Q_ARG(QString, "get_filepath"),
            Q_ARG(QVariant, QVariant("/music/test.mp3")));
    }

    ~TestFixture()
    {
        doc->masterTimer()->stop();
        delete bridge;
        delete doc;
    }

    void startMasterTimer()
    {
        doc->masterTimer()->start();
    }

    void processTick()
    {
        // Let MasterTimer thread process the start queue
        QTest::qWait(50);
    }

    void sendPlay(bool on)
    {
        QMetaObject::invokeMethod(bridge, "onDeckTrigger",
            Q_ARG(int, 0),
            Q_ARG(QString, "play"),
            Q_ARG(QVariant, QVariant(on ? "on" : "off")));
    }
};

// --- C3: When VDJ deck play=true, auto-start the show ---

void VdjBridge_Test::autoStartShowOnPlay()
{
    TestFixture f;
    f.startMasterTimer();

    QVERIFY(!f.show->isRunning());

    f.sendPlay(true);
    f.processTick();

    QVERIFY(f.show->isRunning());
    QVERIFY(!f.show->isPaused());
    QCOMPARE(f.show->syncSource(), 1); // External
}

// --- C5: When VDJ deck play=false while running, auto-pause ---

void VdjBridge_Test::autoPauseShowOnPlayOff()
{
    TestFixture f;
    f.startMasterTimer();

    // Start the show
    f.sendPlay(true);
    f.processTick();
    QVERIFY(f.show->isRunning());

    // Pause it
    f.sendPlay(false);

    QVERIFY(f.show->isRunning());  // still running, just paused
    QVERIFY(f.show->isPaused());
}

// --- C6: When VDJ deck play=true while paused, auto-resume ---

void VdjBridge_Test::autoResumeShowOnPlayOn()
{
    TestFixture f;
    f.startMasterTimer();

    // Start the show
    f.sendPlay(true);
    f.processTick();
    QVERIFY(f.show->isRunning());

    // Pause it
    f.sendPlay(false);
    QVERIFY(f.show->isPaused());

    // Resume it
    f.sendPlay(true);

    QVERIFY(f.show->isRunning());
    QVERIFY(!f.show->isPaused());
}

// --- Full cycle: start → pause → resume (no restart) ---

void VdjBridge_Test::autoStartPauseResumeCycle()
{
    TestFixture f;
    f.startMasterTimer();

    // Initially not running
    QVERIFY(!f.show->isRunning());
    QVERIFY(!f.show->isPaused());

    // 1. Play → auto-start
    f.sendPlay(true);
    f.processTick();
    QVERIFY(f.show->isRunning());
    QVERIFY(!f.show->isPaused());

    // 2. Stop → auto-pause (not stop)
    f.sendPlay(false);
    QVERIFY(f.show->isRunning());
    QVERIFY(f.show->isPaused());

    // 3. Play again → auto-resume (not restart)
    f.sendPlay(true);
    QVERIFY(f.show->isRunning());
    QVERIFY(!f.show->isPaused());

    // 4. Stop again → paused again
    f.sendPlay(false);
    QVERIFY(f.show->isRunning());
    QVERIFY(f.show->isPaused());

    // 5. Play once more → resumes smoothly
    f.sendPlay(true);
    QVERIFY(f.show->isRunning());
    QVERIFY(!f.show->isPaused());
}

QTEST_MAIN(VdjBridge_Test)
