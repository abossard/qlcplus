/*
  Q Light Controller
  mastertimer_test.cpp

  Copyright (C) Heikki Junnila

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
#include "mastertimer_test.h"
#include "dmxsource_stub.h"
#include "function_stub.h"
#include "mastertimer.h"
#include "qlcchannel.h"
#include "universe.h"
#include "qlcfile.h"
#include "doc.h"
#undef private

#include "../common/resource_paths.h"

#include "timingdiagnostics.h"

namespace
{
    QStringList g_timingMsgs;
    QtMessageHandler g_prevHandler = nullptr;

    void timingCaptureHandler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
    {
        Q_UNUSED(type);
        Q_UNUSED(ctx);
        if (msg.contains(QStringLiteral("[timing]")))
            g_timingMsgs << msg;
    }
}

void MasterTimer_Test::initTestCase()
{
    m_doc = new Doc(this);

    QDir dir(INTERNAL_FIXTUREDIR);
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtFixture));
    QVERIFY(m_doc->fixtureDefCache()->loadMap(dir) == true);
}

void MasterTimer_Test::cleanupTestCase()
{
    delete m_doc;
}

void MasterTimer_Test::init()
{
}

void MasterTimer_Test::cleanup()
{
    m_doc->clearContents();
}

void MasterTimer_Test::initial()
{
    MasterTimer* mt = m_doc->masterTimer();

    QVERIFY(mt->runningFunctions() == 0);
    QVERIFY(mt->m_functionList.size() == 0);
    QVERIFY(mt->m_functionListMutex.tryLock() == true);
    mt->m_functionListMutex.unlock();

    QVERIFY(mt->m_dmxSourceList.size() == 0);
    QVERIFY(mt->m_dmxSourceListMutex.tryLock() == true);
    mt->m_dmxSourceListMutex.unlock();

    //QVERIFY(mt->m_running == false);
    QVERIFY(mt->m_stopAllFunctions == false);
}

void MasterTimer_Test::startStop()
{
    MasterTimer* mt = m_doc->masterTimer();

    mt->start();
    QTest::qWait(100);

    QVERIFY(mt->runningFunctions() == 0);
    QVERIFY(mt->m_functionList.size() == 0);
    QVERIFY(mt->m_dmxSourceList.size() == 0);
    // QVERIFY(mt->m_running == true);
    QVERIFY(mt->m_stopAllFunctions == false);

    mt->stop();
    QTest::qWait(100);

    QVERIFY(mt->runningFunctions() == 0);
    QVERIFY(mt->m_functionList.size() == 0);
    QVERIFY(mt->m_dmxSourceList.size() == 0);
    // QVERIFY(mt->m_running == false);
    QVERIFY(mt->m_stopAllFunctions == false);
}

void MasterTimer_Test::startStopFunction()
{
    MasterTimer* mt = m_doc->masterTimer();
    mt->start();

    Function_Stub fs(m_doc);

    QVERIFY(mt->runningFunctions() == 0);

    mt->startFunction(NULL);
    QVERIFY(mt->runningFunctions() == 0);

    mt->startFunction(&fs);
    mt->timerTick();
    QVERIFY(mt->runningFunctions() == 1);

    mt->startFunction(&fs);
    QVERIFY(mt->runningFunctions() == 1);

    QTest::qWait(100);
    fs.stop(FunctionParent::master());
    QTest::qWait(100);

    QVERIFY(mt->runningFunctions() == 0);
}

void MasterTimer_Test::registerUnregisterDMXSource()
{
    MasterTimer* mt = m_doc->masterTimer();
    QVERIFY(mt->m_dmxSourceList.size() == 0);

    DMXSource_Stub s1;
    /* Normal registration */
    mt->registerDMXSource(&s1);
    QVERIFY(mt->m_dmxSourceList.size() == 1);
    QVERIFY(mt->m_dmxSourceList.at(0) == &s1);

    /* No double additions */
    mt->registerDMXSource(&s1);
    QVERIFY(mt->m_dmxSourceList.size() == 1);
    QVERIFY(mt->m_dmxSourceList.at(0) == &s1);

    DMXSource_Stub s2;
    /* Normal registration of another source */
    mt->registerDMXSource(&s2);
    QVERIFY(mt->m_dmxSourceList.size() == 2);
    QVERIFY(mt->m_dmxSourceList.at(0) == &s1);
    QVERIFY(mt->m_dmxSourceList.at(1) == &s2);

    /* No double additions */
    mt->registerDMXSource(&s2);
    QVERIFY(mt->m_dmxSourceList.size() == 2);
    QVERIFY(mt->m_dmxSourceList.at(0) == &s1);
    QVERIFY(mt->m_dmxSourceList.at(1) == &s2);

    /* No double additions */
    mt->registerDMXSource(&s1);
    QVERIFY(mt->m_dmxSourceList.size() == 2);
    QVERIFY(mt->m_dmxSourceList.at(0) == &s1);
    QVERIFY(mt->m_dmxSourceList.at(1) == &s2);

    /* Removal of a source */
    mt->unregisterDMXSource(&s1);
    QVERIFY(mt->m_dmxSourceList.size() == 1);
    QVERIFY(mt->m_dmxSourceList.at(0) == &s2);

    /* No double removals */
    mt->unregisterDMXSource(&s1);
    QVERIFY(mt->m_dmxSourceList.size() == 1);
    QVERIFY(mt->m_dmxSourceList.at(0) == &s2);

    /* Removal of the last source */
    mt->unregisterDMXSource(&s2);
    QVERIFY(mt->m_dmxSourceList.size() == 0);
}

void MasterTimer_Test::interval()
{
    MasterTimer* mt = m_doc->masterTimer();
    Function_Stub fs(m_doc);
    DMXSource_Stub dss;

    mt->start();
    QTest::qWait(100);

    fs.start(mt, FunctionParent::master());
    mt->timerTick();
    QVERIFY(mt->runningFunctions() == 1);

    mt->registerDMXSource(&dss);
    QVERIFY(mt->m_dmxSourceList.size() == 1);

    const int expectedWrites = int(MasterTimer::frequency());
    QElapsedTimer elapsed;
    elapsed.start();
    while ((fs.m_writeCalls < expectedWrites ||
            dss.m_writeCalls < expectedWrites) &&
           elapsed.elapsed() < 2000)
    {
        QTest::qWait(5);
    }
    const int functionWrites = fs.m_writeCalls;
    const int sourceWrites = dss.m_writeCalls;
    const qint64 writeDuration = elapsed.elapsed();

    fs.stop(FunctionParent::master());
    elapsed.restart();
    while (mt->runningFunctions() != 0 && elapsed.elapsed() < 1000)
        QTest::qWait(5);
    mt->unregisterDMXSource(&dss);
    mt->stop();

    QVERIFY(functionWrites >= expectedWrites);
    QVERIFY(sourceWrites >= expectedWrites);
    QVERIFY(writeDuration >= qint64(expectedWrites - 1) * MasterTimer::tick());
    QVERIFY(mt->runningFunctions() == 0);
    QVERIFY(mt->m_dmxSourceList.size() == 0);
}

void MasterTimer_Test::functionInitiatedStop()
{
    MasterTimer* mt = m_doc->masterTimer();
    Function_Stub fs(m_doc);

    mt->start();

    fs.start(mt, FunctionParent::master());
    mt->timerTick();
    QVERIFY(mt->runningFunctions() == 1);

    /* Wait a while so that the function starts running */
    QTest::qWait(100);

    /* Stop the function after it has been running for a while */
    fs.stop(FunctionParent::master());

    /* Wait a while so that the function stops */
    QTest::qWait(100);

    /* Verify that the function is really stopped and the correct
       pre&post handlers have been called. */
    QVERIFY(mt->runningFunctions() == 0);
    QVERIFY(fs.m_preRunCalls == 1);
    QVERIFY(fs.m_writeCalls > 0);
    QVERIFY(fs.m_postRunCalls == 1);
}

void MasterTimer_Test::runMultipleFunctions()
{
    MasterTimer* mt = m_doc->masterTimer();
    mt->start();

    Function_Stub fs1(m_doc);
    fs1.start(mt, FunctionParent::master());
    mt->timerTick();
    QVERIFY(mt->runningFunctions() == 1);

    Function_Stub fs2(m_doc);
    fs2.start(mt, FunctionParent::master());
    mt->timerTick();
    QVERIFY(mt->runningFunctions() == 2);

    Function_Stub fs3(m_doc);
    fs3.start(mt, FunctionParent::master());
    mt->timerTick();
    QVERIFY(mt->runningFunctions() == 3);

    /* Wait a while so that the functions start running */
    QTest::qWait(100);

    /* Stop the functions after they have been running for a while */
    fs1.stop(FunctionParent::master());
    fs2.stop(FunctionParent::master());
    fs3.stop(FunctionParent::master());

    /* Wait a while so that the functions stop */
    QTest::qWait(100);

    QVERIFY(mt->runningFunctions() == 0);
}

void MasterTimer_Test::stopAllFunctions()
{
    MasterTimer* mt = m_doc->masterTimer();
    mt->start();

    Function_Stub fs1(m_doc);
    fs1.start(mt, FunctionParent::master());

    DMXSource_Stub s1;
    mt->registerDMXSource(&s1);

    Function_Stub fs2(m_doc);
    fs2.start(mt, FunctionParent::master());

    DMXSource_Stub s2;
    mt->registerDMXSource(&s2);

    Function_Stub fs3(m_doc);
    fs3.start(mt, FunctionParent::master());

    QTest::qWait(60);

    QVERIFY(mt->runningFunctions() == 3);
    QVERIFY(mt->m_dmxSourceList.size() == 2);

    mt->stopAllFunctions();
    QVERIFY(mt->runningFunctions() == 0);
    QVERIFY(mt->m_dmxSourceList.size() == 2); // Shouldn't stop

    mt->unregisterDMXSource(&s1);
    mt->unregisterDMXSource(&s2);
}

void MasterTimer_Test::stop()
{
    MasterTimer* mt = m_doc->masterTimer();
    mt->start();

    Function_Stub fs1(m_doc);
    fs1.start(mt, FunctionParent::master());

    Function_Stub fs2(m_doc);
    fs2.start(mt, FunctionParent::master());

    Function_Stub fs3(m_doc);
    fs3.start(mt, FunctionParent::master());

    QTest::qWait(60);
    QVERIFY(mt->runningFunctions() == 3);

    mt->stop();
    QTest::qWait(60);
    QVERIFY(mt->runningFunctions() == 0);
    // QVERIFY(mt->m_running == false);
}

void MasterTimer_Test::restart()
{
    MasterTimer* mt = m_doc->masterTimer();
    mt->start();

    Function_Stub fs1(m_doc);
    fs1.start(mt, FunctionParent::master());

    Function_Stub fs2(m_doc);
    fs2.start(mt, FunctionParent::master());

    Function_Stub fs3(m_doc);
    fs3.start(mt, FunctionParent::master());

    QTest::qWait(60);
    QVERIFY(mt->runningFunctions() == 3);

    mt->stop();
    QTest::qWait(60);
    QVERIFY(mt->runningFunctions() == 0);
    QVERIFY(mt->m_functionList.size() == 0);
    QVERIFY(mt->m_functionListMutex.tryLock() == true);
    mt->m_functionListMutex.unlock();
    // QVERIFY(mt->m_running == false);
    QVERIFY(mt->m_stopAllFunctions == false);

    mt->start();
    QVERIFY(mt->runningFunctions() == 0);
    QVERIFY(mt->m_functionList.size() == 0);
    QVERIFY(mt->m_functionListMutex.tryLock() == true);
    mt->m_functionListMutex.unlock();
    // QVERIFY(mt->m_running == true);
    QVERIFY(mt->m_stopAllFunctions == false);

    fs1.start(mt, FunctionParent::master());
    fs2.start(mt, FunctionParent::master());
    fs3.start(mt, FunctionParent::master());
    QTest::qWait(60);
    QVERIFY(mt->runningFunctions() == 3);

    mt->stopAllFunctions();
}

/* C2-2: a slow tick's cost is attributed to the responsible function.
   Two functions with unequal write() cost run in the same tick; the diagnostic
   must name the costlier one as dominant, distinct from the cheap one, and
   report the effective budget. Uses an injected clock so the rate-bounded
   summary flushes deterministically without relying on wall time. */
void MasterTimer_Test::diagAttributesSlowFunction()
{
    MasterTimer* mt = m_doc->masterTimer();
    const int budgetMs = int(MasterTimer::tick());

    // Stop any timer thread left running by a prior test (e.g. restart()), so
    // the scheduler loop does not concurrently write the wake member while this
    // test drives timerTick() directly and expects dispatchLate=unknown.
    mt->stop();

    Function_Stub fsSlow(m_doc);
    Function_Stub fsFast(m_doc);
    fsSlow.setID(101);
    fsFast.setID(102);
    fsSlow.m_writeSleepUs = qMax(25, budgetMs * 2) * 1000; // always over budget
    fsFast.m_writeSleepUs = 2000;

    qint64 now = 0;
    g_timingMsgs.clear();
    g_prevHandler = qInstallMessageHandler(timingCaptureHandler);
    TimingDiag::resetForTest();
    TimingDiag::setClockForTest([&now]() { return now; });
    TimingDiag::setIntervalMsForTest(1000);
    TimingDiag::setEnabledForTest(true);

    fsSlow.start(mt, FunctionParent::master());
    fsFast.start(mt, FunctionParent::master());

    now = 0;
    mt->timerTick();               // register + first slow tick (window opens)
    mt->timerTick();               // second slow tick (same window)
    QVERIFY(mt->runningFunctions() == 2);
    now = 1000;
    mt->timerTick();               // crosses the interval -> flush window 1

    QStringList lines;
    foreach (const QString &m, g_timingMsgs)
        if (m.contains(QStringLiteral("[timing] callback consumed period")))
            lines << m;

    TimingDiag::setEnabledForTest(false);
    TimingDiag::setClockForTest(nullptr);
    TimingDiag::resetForTest();
    qInstallMessageHandler(g_prevHandler);

    fsSlow.stop(FunctionParent::master());
    fsFast.stop(FunctionParent::master());
    mt->timerTick();

    QVERIFY2(lines.size() >= 1, "no callback-consumed-period diagnostic was emitted");
    const QString &l = lines.first();
    QVERIFY2(l.contains(QString("(id %1)").arg(fsSlow.id())), qPrintable(l));   // maxWrite is the slow one
    QVERIFY2(!l.contains(QString("(id %1)").arg(fsFast.id())), qPrintable(l));  // not the cheap one
    QVERIFY2(l.contains("period=20.000ms"), qPrintable(l));  // real 1e9/frequency at 50Hz
    QVERIFY2(l.contains("maxCallback="), qPrintable(l));
    QVERIFY2(l.contains("functions="), qPrintable(l));       // summed function time (both writes)
    QVERIFY2(l.contains("unattributed="), qPrintable(l));    // maxWrite is containment, not sole cause
    QVERIFY2(l.contains("dispatchLate=unknown"), qPrintable(l));
    QVERIFY2(l.contains("maxWrite="), qPrintable(l));
    QVERIFY2(l.contains("type="), qPrintable(l));            // owning Function type carried
    QVERIFY2(l.contains("phase=write"), qPrintable(l));      // the timed boundary phase
    QVERIFY2(!l.contains("dominant"), qPrintable(l));        // no over-claiming label
}

/* C2-2: an over-period callback that ran no Function::write() (e.g. slow
   tickReady work with an empty running list) must not invent a write identity.
   The costly work here is a DirectConnection slot on tickReady() - included in
   the callback sample - while no function writes, so the maxWrite identity stays
   Function::invalidId() and the whole field collapses to "maxWrite=unknown", never a
   magic sentinel id or a fictitious write phase. Exercised through the real
   timerTick() production path, not the formatter directly. */
void MasterTimer_Test::diagNoWriteSlowCallbackIsNone()
{
    MasterTimer* mt = m_doc->masterTimer();

    // Stop any timer thread from a prior test so it does not drive ticks
    // concurrently while this test calls timerTick() directly.
    mt->stop();
    QVERIFY(mt->runningFunctions() == 0);   // no Function::write() will run

    // A slow synchronous consumer of tickReady() inflates the callback past the
    // period without any function write. Real wall time: the callback sample is
    // a QElapsedTimer, not the injected diagnostics clock.
    QMetaObject::Connection c = connect(mt, &MasterTimer::tickReady, mt,
                                        []() { QTest::qSleep(25); },
                                        Qt::DirectConnection);

    qint64 now = 0;
    g_timingMsgs.clear();
    g_prevHandler = qInstallMessageHandler(timingCaptureHandler);
    TimingDiag::resetForTest();
    TimingDiag::setClockForTest([&now]() { return now; });
    TimingDiag::setIntervalMsForTest(1000);
    TimingDiag::setEnabledForTest(true);

    now = 0;
    mt->timerTick();               // slow, no-write tick (window opens)
    now = 1000;
    mt->timerTick();               // crosses the interval -> flush window 1

    QStringList lines;
    foreach (const QString &m, g_timingMsgs)
        if (m.contains(QStringLiteral("[timing] callback consumed period")))
            lines << m;

    TimingDiag::setEnabledForTest(false);
    TimingDiag::setClockForTest(nullptr);
    TimingDiag::resetForTest();
    qInstallMessageHandler(g_prevHandler);
    disconnect(c);

    QVERIFY2(lines.size() >= 1, "no callback-consumed-period diagnostic was emitted");
    const QString &l = lines.first();
    QVERIFY2(l.contains("maxWrite=unknown"), qPrintable(l));  // collapsed unknown identity
    QVERIFY2(!l.contains("phase=write"), qPrintable(l));      // no invented write phase
    QVERIFY2(!l.contains("4294967295"), qPrintable(l));       // no magic sentinel id
    QVERIFY2(l.contains("functions=0.000ms"), qPrintable(l)); // no function write was timed
    QVERIFY2(l.contains("maxCallback="), qPrintable(l));      // the slow callback is still reported
}

/* C2-4: with diagnostics disabled the tick path is unchanged - functions still
   run and no diagnostic work or output happens. */
void MasterTimer_Test::diagDisabledPreservesBehavior()
{
    MasterTimer* mt = m_doc->masterTimer();

    Function_Stub fsSlow(m_doc);
    fsSlow.m_writeSleepUs = qMax(25, int(MasterTimer::tick()) * 2) * 1000;

    g_timingMsgs.clear();
    g_prevHandler = qInstallMessageHandler(timingCaptureHandler);
    TimingDiag::resetForTest();
    TimingDiag::setEnabledForTest(false);

    fsSlow.start(mt, FunctionParent::master());
    mt->timerTick();
    mt->timerTick();

    const int writes = fsSlow.m_writeCalls;
    const int running = mt->runningFunctions();

    qInstallMessageHandler(g_prevHandler);

    fsSlow.stop(FunctionParent::master());
    mt->timerTick();

    QCOMPARE(g_timingMsgs.size(), 0);   // disabled path is silent
    QVERIFY(writes > 0);                // function still ran
    QVERIFY(running == 1);
}

QTEST_MAIN(MasterTimer_Test)
