/*
  Q Light Controller Plus - Unit test
  timingdiagnostics_test.cpp

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
#include <QThread>
#include <QElapsedTimer>

#include <atomic>
#include <thread>

#include "timingdiagnostics_test.h"
#include "timingdiagnostics.h"

static const qint64 MS = 1000000; // ns per ms, for readable fixtures

static QStringList g_msgs;
static QtMessageHandler g_prev = nullptr;
static qint64 g_now = 0; // injected window clock, milliseconds

static void captureHandler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
    Q_UNUSED(type);
    Q_UNUSED(ctx);
    if (msg.contains(QStringLiteral("[timing]")))
        g_msgs << msg;
}

static QStringList timingLines()
{
    return g_msgs;
}

void TimingDiagnostics_Test::init()
{
    g_msgs.clear();
    g_now = 0;
    g_prev = qInstallMessageHandler(captureHandler);
    TimingDiag::resetForTest();
    TimingDiag::setClockForTest([]() { return g_now; });
    TimingDiag::setIntervalMsForTest(1000);
    TimingDiag::setEnabledForTest(true);
}

void TimingDiagnostics_Test::cleanup()
{
    TimingDiag::setEnabledForTest(false);
    TimingDiag::setClockForTest(nullptr);
    TimingDiag::resetForTest();
    qInstallMessageHandler(g_prev);
    g_msgs.clear();
}

/* Disabled gate: every entry point (record and flush) is silent. */
void TimingDiagnostics_Test::disabledGateEmitsNothing_data()
{
    QTest::addColumn<QString>("report");
    QTest::newRow("rgbStepAdvance") << "rgb";
    QTest::newRow("callbackConsumedPeriod") << "period";
    QTest::newRow("hueHandoff") << "hue";
}

void TimingDiagnostics_Test::disabledGateEmitsNothing()
{
    QFETCH(QString, report);
    TimingDiag::setEnabledForTest(false);

    for (int i = 0; i < 5; i++)
    {
        g_now = i * 1000;
        if (report == "rgb")
            TimingDiag::rgbStepAdvance(1, "M");
        else if (report == "period")
            TimingDiag::callbackConsumedPeriod(20 * MS, 40 * MS, 30 * MS, -1, 1, "Scene", "F", 30 * MS);
        else
            TimingDiag::hueHandoff(1, "H", 50 * MS, 40 * MS, 10 * MS);
        TimingDiag::flushExpired();
    }

    QCOMPARE(timingLines().size(), 0);
}

/* A single rare event is emitted within one interval by the periodic flush,
   without waiting for a later event (the lazy-flush defect this replaces). */
void TimingDiagnostics_Test::rareEventFlushesWithinOneInterval()
{
    TimingDiag::callbackConsumedPeriod(20 * MS, 25 * MS, 20 * MS, 2 * MS, 5, "HUEMatrix", "Solo", 25 * MS);
    TimingDiag::flushExpired();               // same instant: not yet expired
    QCOMPARE(timingLines().size(), 0);

    g_now = 1000;
    TimingDiag::flushExpired();               // no new event needed
    const QStringList lines = timingLines();
    QCOMPARE(lines.size(), 1);
    QVERIFY2(lines.first().contains("count=1"), qPrintable(lines.first()));
    QVERIFY2(lines.first().contains("in 1000ms"), qPrintable(lines.first())); // actual window span
}

/* A sub-interval final window (which flushExpired never emits) must not be
   dropped: flushAll() on shutdown emits it with its actual short span, so the
   isolated/final incident count is retained. */
void TimingDiagnostics_Test::finalTailIsNotDropped()
{
    TimingDiag::rgbStepAdvance(3, "TAIL");
    TimingDiag::rgbStepAdvance(3, "TAIL");
    g_now = 200;                           // only 200ms elapsed, < 1000ms interval
    TimingDiag::flushExpired();
    QCOMPARE(timingLines().size(), 0);     // not expired yet -> flushExpired emits nothing

    TimingDiag::flushAll();                // shutdown flush
    const QStringList lines = timingLines();
    QCOMPARE(lines.size(), 1);
    QVERIFY2(lines.first().contains("advances=2"), qPrintable(lines.first()));
    QVERIFY2(lines.first().contains("in 200ms"), qPrintable(lines.first())); // actual short span
}

/* Rate bound: at most one summary per interval, carrying the closed window's
   count and its actual duration. */
void TimingDiagnostics_Test::rateBoundOneSummaryPerInterval()
{
    for (int i = 0; i < 5; i++)
        TimingDiag::rgbStepAdvance(7, "BLUE");
    TimingDiag::flushExpired();
    QCOMPARE(timingLines().size(), 0);        // nothing flushed mid-window

    g_now = 1000;
    TimingDiag::flushExpired();               // closes window 1
    TimingDiag::rgbStepAdvance(7, "BLUE");
    g_now = 2000;
    TimingDiag::flushExpired();               // closes window 2

    const QStringList lines = timingLines();
    QCOMPARE(lines.size(), 2);
    QVERIFY2(lines.at(0).contains("advances=5"), qPrintable(lines.at(0)));
    QVERIFY2(lines.at(0).contains("id=7"), qPrintable(lines.at(0)));
    QVERIFY2(lines.at(0).contains("BLUE"), qPrintable(lines.at(0)));
    QVERIFY2(lines.at(1).contains("advances=1"), qPrintable(lines.at(1)));
}

/* The period summary retains the over-period count and worst callback, and
   attributes the worst tick to the right maxWrite (containment), carrying its
   Function type and phase. */
void TimingDiagnostics_Test::periodRetainsCountAndMax()
{
    TimingDiag::callbackConsumedPeriod(20 * MS, 25 * MS, 20 * MS, 1 * MS, 1, "Scene", "SceneA", 25 * MS);
    TimingDiag::callbackConsumedPeriod(20 * MS, 40 * MS, 30 * MS, 3 * MS, 2, "HUEMatrix", "HueB", 40 * MS); // worst
    TimingDiag::callbackConsumedPeriod(20 * MS, 10 * MS, 8 * MS, 2 * MS, 3, "RGBMatrix", "RgbC", 10 * MS);

    g_now = 1000;
    TimingDiag::flushExpired();

    const QStringList lines = timingLines();
    QCOMPARE(lines.size(), 1);
    const QString &l = lines.first();
    QVERIFY2(l.contains("count=3"), qPrintable(l));
    QVERIFY2(l.contains("maxCallback=40.000ms"), qPrintable(l));
    QVERIFY2(l.contains("functions=30.000ms"), qPrintable(l));
    QVERIFY2(l.contains("unattributed=10.000ms"), qPrintable(l));
    QVERIFY2(l.contains("dispatchLate=3.000ms"), qPrintable(l));
    QVERIFY2(l.contains("maxWrite=\"HueB\""), qPrintable(l));
    QVERIFY2(l.contains("type=HUEMatrix"), qPrintable(l));
    QVERIFY2(l.contains("phase=write"), qPrintable(l));
    QVERIFY2(!l.contains("SceneA"), qPrintable(l));
    QVERIFY2(!l.contains("RgbC"), qPrintable(l));
    // conservative labelling: no over-claiming words
    QVERIFY2(!l.contains("dominant"), qPrintable(l));
    QVERIFY2(!l.contains("responsible"), qPrintable(l));
}

/* A real id 0 write prints its identity; a no-write over-period callback (no
   timed write) collapses the whole maxWrite identity to "none" - never an
   invented empty name/type/phase or a magic sentinel id. */
void TimingDiagnostics_Test::periodUnknownMaxWriteIsNotZero()
{
    // Measured maxWrite with a real id 0 must print "0.000ms", never "unknown".
    TimingDiag::callbackConsumedPeriod(20 * MS, 30 * MS, 25 * MS, 5 * MS, 0, "Scene", "Zero", 0);
    g_now = 1000;
    TimingDiag::flushExpired();
    QCOMPARE(timingLines().size(), 1);
    QVERIFY2(timingLines().first().contains("(id 0) 0.000ms"), qPrintable(timingLines().first()));

    // No maxWrite measured (over-period callback whose cost is not in a write):
    // the whole identity collapses to "maxWrite=unknown".
    g_msgs.clear();
    TimingDiag::resetForTest();
    g_now = 2000;
    const quint32 invalidId = 0xFFFFFFFFu;
    TimingDiag::callbackConsumedPeriod(20 * MS, 30 * MS, 0, -1, invalidId, "?", "", -1);
    g_now = 3000;
    TimingDiag::flushExpired();

    const QStringList lines = timingLines();
    QCOMPARE(lines.size(), 1);
    const QString &l = lines.first();
    QVERIFY2(l.contains("maxWrite=unknown"), qPrintable(l));   // collapsed identity
    QVERIFY2(!l.contains("phase=write"), qPrintable(l));        // no invented write phase
    QVERIFY2(!l.contains("4294967295"), qPrintable(l));         // no magic sentinel id
    QVERIFY2(!l.contains("type=?"), qPrintable(l));
    QVERIFY2(l.contains("dispatchLate=unknown"), qPrintable(l));
    QVERIFY2(!l.contains("(id 0)"), qPrintable(l));
}

/* Aggregation keeps nanoseconds and formats fractional ms, so a residual is
   exact and sub-ms values are not flattened to 0. */
void TimingDiagnostics_Test::fractionalMsResidualIsExact()
{
    // 20.4ms callback, 19.9ms functions -> 0.5ms unattributed. Rounding to whole
    // ms first would give 20 - 20 = 0, which would be wrong.
    TimingDiag::callbackConsumedPeriod(20 * MS, 20400000, 19900000, 1 * MS, 2, "HUEMatrix", "H", 400000);
    g_now = 1000;
    TimingDiag::flushExpired();

    const QStringList lines = timingLines();
    QCOMPARE(lines.size(), 1);
    const QString &l = lines.first();
    QVERIFY2(l.contains("maxCallback=20.400ms"), qPrintable(l));
    QVERIFY2(l.contains("functions=19.900ms"), qPrintable(l));
    QVERIFY2(l.contains("unattributed=0.500ms"), qPrintable(l));   // exact, not 0
    QVERIFY2(l.contains("(id 2) 0.400ms"), qPrintable(l));         // 0.4ms not flattened
}

/* The HUEScript split reports queue and worker separately (queue = Jstart-Q,
   worker = Jend-Jstart) and derives returnTail; an unknown worker keeps worker
   and returnTail unknown. */
void TimingDiagnostics_Test::hueHandoffSplitsWaitFromWorker_data()
{
    QTest::addColumn<qint64>("waitNs");
    QTest::addColumn<qint64>("queueNs");
    QTest::addColumn<qint64>("workerNs");
    QTest::addColumn<QString>("expectQueue");
    QTest::addColumn<QString>("expectWorker");
    QTest::addColumn<QString>("expectTail");

    QTest::newRow("queue dominates") << qint64(50 * MS) << qint64(40 * MS) << qint64(8 * MS)
        << "queue=40.000ms" << "worker=8.000ms" << "returnTail=2.000ms";
    QTest::newRow("worker dominates") << qint64(50 * MS) << qint64(3 * MS) << qint64(45 * MS)
        << "queue=3.000ms" << "worker=45.000ms" << "returnTail=2.000ms";
    QTest::newRow("worker unmeasured") << qint64(30 * MS) << qint64(5 * MS) << qint64(-1)
        << "queue=5.000ms" << "worker=unknown" << "returnTail=unknown";
}

void TimingDiagnostics_Test::hueHandoffSplitsWaitFromWorker()
{
    QFETCH(qint64, waitNs);
    QFETCH(qint64, queueNs);
    QFETCH(qint64, workerNs);
    QFETCH(QString, expectQueue);
    QFETCH(QString, expectWorker);
    QFETCH(QString, expectTail);

    TimingDiag::hueHandoff(3, "BASSHUE", waitNs, queueNs, workerNs);
    g_now = 1000;
    TimingDiag::flushExpired();

    const QStringList lines = timingLines();
    QCOMPARE(lines.size(), 1);
    const QString &l = lines.first();
    QVERIFY2(l.contains("BASSHUE"), qPrintable(l));
    QVERIFY2(l.contains(expectQueue), qPrintable(l));
    QVERIFY2(l.contains(expectWorker), qPrintable(l));
    QVERIFY2(l.contains(expectTail), qPrintable(l));
}

namespace
{
    const TimingDiag::BucketStat *bucketOfKind(const TimingDiag::CaptureStat &s,
                                               const QString &kind)
    {
        for (const TimingDiag::BucketStat &b : s.buckets)
            if (b.kind == kind)
                return &b;
        return nullptr;
    }
}

/* C0-1: the production runtime control path (enable -> record -> snapshot ->
   disable retains -> re-enable starts a fresh capture -> reset bumps identity),
   driven through the real API rather than the test gate. */
void TimingDiagnostics_Test::productionControlRoundTrip()
{
    g_now = 0;
    TimingDiag::setEnabled(false);              // known off baseline
    TimingDiag::resetForTest();

    TimingDiag::setEnabled(true);               // off -> on: new capture
    QVERIFY(TimingDiag::enabled());
    const qint64 cap = TimingDiag::captureId();

    TimingDiag::schedulerDispatch(20 * MS, 3 * MS, true, 5 * MS, 0, 0, cap);
    TimingDiag::hueHandoff(3, "BASSHUE", 40 * MS, 2 * MS, 35 * MS, 12 * MS, cap);
    TimingDiag::callbackConsumedPeriod(20 * MS, 25 * MS, 20 * MS, 3 * MS, 2,
                                       "HUEMatrix", "HueB", 25 * MS, cap);

    TimingDiag::CaptureStat snap = TimingDiag::snapshot();
    QVERIFY(snap.enabled);
    QCOMPARE(snap.captureId, cap);
    QCOMPARE(snap.discardedSamples, qint64(0));
    QCOMPARE(snap.buckets.size(), 3);

    const TimingDiag::BucketStat *hue = bucketOfKind(snap, "hue");
    QVERIFY(hue != nullptr);
    QCOMPARE(hue->count, qint64(1));
    QCOMPARE(hue->worstPrimaryNs, qint64(40 * MS)); // maxWait
    QCOMPARE(hue->m2, qint64(35 * MS));             // worker
    QCOMPARE(hue->m3, qint64(12 * MS));             // workerCpu
    QCOMPARE(hue->cpuUnknownCount, qint64(0));

    // on -> off retains the aggregates for a post-run snapshot.
    TimingDiag::setEnabled(false);
    QVERIFY(!TimingDiag::enabled());
    TimingDiag::CaptureStat afterDisable = TimingDiag::snapshot();
    QVERIFY(!afterDisable.enabled);
    QCOMPARE(afterDisable.captureId, cap);
    QCOMPARE(afterDisable.buckets.size(), 3);       // survived disable

    // off -> on again starts a fresh capture: identity bumps, aggregates clear.
    TimingDiag::setEnabled(true);
    QVERIFY(TimingDiag::captureId() > cap);
    QCOMPARE(TimingDiag::snapshot().buckets.size(), 0);

    // explicit reset bumps the identity again.
    const qint64 cap2 = TimingDiag::captureId();
    TimingDiag::reset();
    QVERIFY(TimingDiag::captureId() > cap2);
}

/* C0-5: snapshot aggregates are bounded, survive a periodic flush and a disable,
   and repeated reads never consume them. */
void TimingDiagnostics_Test::snapshotSurvivesFlushDisableAndIsNonConsuming()
{
    g_now = 0;
    TimingDiag::setEnabledForTest(true);
    TimingDiag::reset();                             // anchor a fresh capture
    const qint64 cap = TimingDiag::captureId();

    // Varied counts across all kinds, including a worst that must be retained.
    TimingDiag::rgbStepAdvance(7, "BLUE");
    TimingDiag::rgbStepAdvance(7, "BLUE");
    TimingDiag::schedulerDispatch(20 * MS, 1 * MS, false, 5 * MS, 0, 0, cap);
    TimingDiag::schedulerDispatch(20 * MS, 9 * MS, true, 6 * MS, 0, 0, cap);   // worst
    TimingDiag::hueHandoff(3, "BASSHUE", 40 * MS, 2 * MS, 35 * MS, 20 * MS, cap); // worst
    TimingDiag::hueHandoff(3, "BASSHUE", 12 * MS, 1 * MS, 9 * MS, -1, cap);       // cpu unknown

    const auto checkAggregates = [&](const TimingDiag::CaptureStat &s) {
        QCOMPARE(s.captureId, cap);
        const TimingDiag::BucketStat *sched = bucketOfKind(s, "scheduler");
        const TimingDiag::BucketStat *hue = bucketOfKind(s, "hue");
        const TimingDiag::BucketStat *rgb = bucketOfKind(s, "rgb");
        QVERIFY(sched && hue && rgb);
        QCOMPARE(sched->count, qint64(2));
        QCOMPARE(sched->worstPrimaryNs, qint64(9 * MS));   // worst dispatch late retained
        QCOMPARE(hue->count, qint64(2));
        QCOMPARE(hue->worstPrimaryNs, qint64(40 * MS));    // worst wait retained
        QCOMPARE(hue->m2, qint64(35 * MS));                // its paired worker
        QCOMPARE(hue->m3, qint64(20 * MS));                // its paired workerCpu
        QCOMPARE(hue->cpuUnknownCount, qint64(1));         // one render had unknown cpu
        QCOMPARE(rgb->count, qint64(2));
    };

    checkAggregates(TimingDiag::snapshot());

    // A periodic flush emits and resets the LIVE window but must not touch the
    // retained aggregates.
    g_now = 1000;
    TimingDiag::flushExpired();
    QVERIFY(!timingLines().isEmpty());              // the flush did emit
    checkAggregates(TimingDiag::snapshot());        // aggregates intact after flush

    // Disable retains, and repeated reads are non-consuming.
    TimingDiag::setEnabledForTest(false);
    checkAggregates(TimingDiag::snapshot());
    checkAggregates(TimingDiag::snapshot());

    // Explicit reset clears the capture.
    TimingDiag::reset();
    QCOMPARE(TimingDiag::snapshot().buckets.size(), 0);
}

/* C0-8: a sample whose capture identity was superseded by a reset/new-capture
   while it was in flight is discarded, never mixed into the new capture. */
void TimingDiagnostics_Test::staleCaptureSampleIsDiscarded()
{
    g_now = 0;
    TimingDiag::setEnabledForTest(true);
    TimingDiag::reset();                             // capture A
    const qint64 capA = TimingDiag::captureId();

    // A control change lands while a render is still in flight under capA.
    TimingDiag::reset();                             // capture B
    const qint64 capB = TimingDiag::captureId();
    QVERIFY(capB > capA);

    // The in-flight render completes and reports its stale capA identity.
    TimingDiag::hueHandoff(3, "BASSHUE", 40 * MS, 2 * MS, 35 * MS, 12 * MS, capA);
    TimingDiag::CaptureStat snap = TimingDiag::snapshot();
    QCOMPARE(snap.captureId, capB);
    QCOMPARE(snap.discardedSamples, qint64(1));      // rejected, counted as discarded
    QCOMPARE(snap.buckets.size(), 0);                // not mixed into capture B

    // A fresh sample under capB is recorded normally; discarded count is stable.
    TimingDiag::hueHandoff(3, "BASSHUE", 10 * MS, 1 * MS, 8 * MS, 5 * MS, capB);
    TimingDiag::CaptureStat snap2 = TimingDiag::snapshot();
    QCOMPARE(snap2.discardedSamples, qint64(1));
    QCOMPARE(snap2.buckets.size(), 1);
    QCOMPARE(snap2.buckets.first().count, qint64(1));
}

/* C0-3: worker CPU is reported as a field distinct from worker wall time, with a
   derived off-CPU remainder, and an unavailable CPU clock stays "unknown" and is
   counted rather than reported as zero. */
void TimingDiagnostics_Test::workerCpuIsSeparateFieldAndUnknownAware()
{
    g_now = 0;
    TimingDiag::setEnabledForTest(true);
    TimingDiag::reset();
    const qint64 cap = TimingDiag::captureId();

    // worker wall 35ms, on-CPU 20ms -> 15ms not running on that core.
    TimingDiag::hueHandoff(3, "BASSHUE", 40 * MS, 2 * MS, 35 * MS, 20 * MS, cap);
    g_now = 1000;
    TimingDiag::flushExpired();
    QCOMPARE(timingLines().size(), 1);
    const QString known = timingLines().first();
    QVERIFY2(known.contains("worker=35.000ms"), qPrintable(known));
    QVERIFY2(known.contains("workerCpu=20.000ms"), qPrintable(known));
    QVERIFY2(known.contains("workerOffCpu=15.000ms"), qPrintable(known));

    // Unavailable CPU clock: unknown, not zero, and coverage is counted.
    g_msgs.clear();
    TimingDiag::reset();
    const qint64 cap2 = TimingDiag::captureId();
    TimingDiag::hueHandoff(3, "BASSHUE", 12 * MS, 1 * MS, 9 * MS, -1, cap2);

    const TimingDiag::CaptureStat unknownSnap = TimingDiag::snapshot();
    const TimingDiag::BucketStat *hue = bucketOfKind(unknownSnap, "hue");
    QVERIFY(hue != nullptr);
    QCOMPARE(hue->m2, qint64(9 * MS));               // worker wall known
    QCOMPARE(hue->m3, qint64(-1));                   // worker cpu unknown, not 0
    QCOMPARE(hue->cpuUnknownCount, qint64(1));

    g_now = 2000;
    TimingDiag::flushExpired();
    const QString unknown = timingLines().first();
    QVERIFY2(unknown.contains("workerCpu=unknown"), qPrintable(unknown));
    QVERIFY2(unknown.contains("workerOffCpu=unknown"), qPrintable(unknown));
}

/* C0-8 hardening: snapshot() must read its capture identity and its aggregates
   under one lock. A writer thread repeatedly starts a new capture and records a
   sample whose Function id equals the new captureId, so every retained bucket
   encodes the capture it belongs to. A reader thread snapshots concurrently and
   asserts every non-empty bucket's id equals the snapshot's own captureId. If
   the metadata is read before the aggregate lock, a reset landing in that window
   pairs an old captureId with the new capture's aggregates and the invariant
   breaks. This drives the production API directly; it fabricates no tokens. */
void TimingDiagnostics_Test::snapshotCaptureIdMatchesAggregatesUnderConcurrency()
{
    TimingDiag::setClockForTest(nullptr);   // real clock; this test is timing-agnostic
    TimingDiag::setEnabledForTest(true);
    TimingDiag::reset();

    std::atomic<bool> stop{false};
    std::atomic<bool> mismatch{false};
    std::atomic<qint64> mismatchCap{-1};
    std::atomic<qint64> mismatchBucket{-1};
    std::atomic<qint64> snapshots{0};

    std::thread writer([&]() {
        while (!stop.load(std::memory_order_relaxed))
        {
            TimingDiag::reset();
            const qint64 cap = TimingDiag::captureId();
            // Function id == captureId: the bucket's identity encodes its capture.
            TimingDiag::rgbStepAdvance(quint32(cap), QStringLiteral("C"));
        }
    });

    std::thread reader([&]() {
        while (!stop.load(std::memory_order_relaxed))
        {
            const TimingDiag::CaptureStat s = TimingDiag::snapshot();
            for (const TimingDiag::BucketStat &b : s.buckets)
            {
                if (b.count > 0 && qint64(b.id) != s.captureId)
                {
                    mismatchCap.store(s.captureId, std::memory_order_relaxed);
                    mismatchBucket.store(qint64(b.id), std::memory_order_relaxed);
                    mismatch.store(true, std::memory_order_relaxed);
                }
            }
            snapshots.fetch_add(1, std::memory_order_relaxed);
        }
    });

    QElapsedTimer t;
    t.start();
    while (!mismatch.load(std::memory_order_relaxed)
           && snapshots.load(std::memory_order_relaxed) < 3000000
           && t.elapsed() < 4000)
        QThread::yieldCurrentThread();

    stop.store(true, std::memory_order_relaxed);
    writer.join();
    reader.join();

    TimingDiag::setEnabledForTest(false);
    TimingDiag::resetForTest();

    QVERIFY2(!mismatch.load(std::memory_order_relaxed),
             qPrintable(QString("snapshot captureId=%1 carried a bucket from capture=%2 "
                                "(non-atomic metadata/aggregate read)")
                        .arg(mismatchCap.load()).arg(mismatchBucket.load())));
}

/* C0-5 hardening: once a capture is disabled it is a completed recording, so its
   reported duration and counts must stay frozen no matter how much wall time
   passes before the next control action. Otherwise elapsedMs/windowMs keep
   growing against frozen counts and skew a later rate/overhead comparison. */
void TimingDiagnostics_Test::disabledCaptureDurationIsFrozen()
{
    g_now = 0;
    TimingDiag::setEnabled(false);          // known off baseline
    TimingDiag::resetForTest();

    TimingDiag::setEnabled(true);           // off->on at t=0 (captureStart=0)
    const qint64 cap = TimingDiag::captureId();

    g_now = 100;
    TimingDiag::hueHandoff(3, "H", 40 * MS, 2 * MS, 35 * MS, 20 * MS, cap); // first event at t=100

    // Active snapshot: duration advances with the clock.
    g_now = 500;
    const TimingDiag::CaptureStat active = TimingDiag::snapshot();
    QVERIFY(active.enabled);
    QCOMPARE(active.elapsedMs, qint64(500));           // now - captureStart
    QCOMPARE(active.buckets.size(), 1);
    QCOMPARE(active.buckets.first().windowMs, qint64(400)); // now - firstEvent
    const qint64 frozenCount = active.buckets.first().count;
    const qint64 frozenWindow = active.buckets.first().windowMs;

    // Disable freezes the recording end at t=500.
    TimingDiag::setEnabled(false);

    // Advance the clock substantially: duration and counts must NOT move.
    g_now = 500000;
    const TimingDiag::CaptureStat d1 = TimingDiag::snapshot();
    QVERIFY(!d1.enabled);
    QCOMPARE(d1.elapsedMs, qint64(500));               // frozen, not 500000
    QCOMPARE(d1.buckets.size(), 1);
    QCOMPARE(d1.buckets.first().count, frozenCount);
    QCOMPARE(d1.buckets.first().windowMs, frozenWindow);

    // A repeated snapshot much later is still identical (non-consuming + frozen).
    g_now = 9999999;
    const TimingDiag::CaptureStat d2 = TimingDiag::snapshot();
    QCOMPARE(d2.elapsedMs, qint64(500));
    QCOMPARE(d2.buckets.first().count, frozenCount);
    QCOMPARE(d2.buckets.first().windowMs, frozenWindow);

    // Re-enable starts a fresh, empty, zero-duration capture.
    g_now = 10000000;
    TimingDiag::setEnabled(true);
    QVERIFY(TimingDiag::captureId() > cap);
    const TimingDiag::CaptureStat fresh = TimingDiag::snapshot();
    QVERIFY(fresh.enabled);
    QCOMPARE(fresh.buckets.size(), 0);
    QCOMPARE(fresh.elapsedMs, qint64(0));              // captureStart == now
}

QTEST_MAIN(TimingDiagnostics_Test)