#include <QtTest>
#include <QThread>
#include <atomic>
#include <errno.h>
#include <functional>
#include <time.h>

#include "mastertimer_unix_test.h"
#include "timingdiagnostics.h"

namespace
{
    constexpr qint64 MS = 1000000;
    qint64 clockNs = 0;
    qint64 originNs = 0;
    uint frequency = 50;
    QList<qint64> costs;
    QList<qint64> wakeDelays;
    QList<qint64> starts;
    QList<qint64> sleeps;
    QStringList messages;
    QList<int> messageTypes;
    QList<qint64> messageTimes;
    int clockReads = 0;
    int failedRead = 0;
    // Optional per-tick hook, invoked at the top of the stubbed callback with the
    // 0-based index of the tick whose callback is running. Used to flip runtime
    // controls mid-run; a control changed here takes effect on the NEXT tick's
    // dispatch gate, exactly as an external MCP call would.
    std::function<void(qsizetype)> onTick;
}

class VirtualMasterTimer : public QObject
{
public:
    static uint frequency() { return ::frequency; }
    qint64 m_diagDispatchLatenessNs = -1;
    void timerTick();
};

// Keep the production scheduling loop; replace only its clock, sleep and callback.
#define MasterTimer VirtualMasterTimer
#define MasterTimerPrivate VirtualMasterTimerPrivate
#define private public
#include "mastertimer-unix.h"
#undef private

static VirtualMasterTimerPrivate *driver = nullptr;

void VirtualMasterTimer::timerTick()
{
    const qsizetype index = starts.size();
    Q_ASSERT(index < costs.size());
    if (onTick)
        onTick(index);
    starts.append(clockNs - originNs);
    clockNs += costs.at(index);
    TimingDiag::flushExpired();
    if (starts.size() == costs.size())
        driver->m_run = false;
}

#if defined(Q_OS_MACOS) || defined(Q_OS_IOS)
static kern_return_t readClock(clock_serv_t, mach_timespec_t *value)
#else
static int readClock(clockid_t, timespec *value)
#endif
{
    ++clockReads;
    if (clockReads == failedRead ||
        (failedRead == -1 && clockReads >= 3 && (clockReads - 3) % 3 < 2))
    {
        errno = EIO;
        return -1;
    }
    value->tv_sec = clockNs / 1000000000LL;
    value->tv_nsec = clockNs % 1000000000LL;
    return 0;
}

static int virtualSleep(const timespec *request, timespec *)
{
    const qint64 duration = request->tv_sec * 1000000000LL + request->tv_nsec;
    Q_ASSERT(duration >= 0);
    const qsizetype index = sleeps.size();
    sleeps.append(duration);
    clockNs += duration;
    if (index < wakeDelays.size())
        clockNs += wakeDelays.at(index);
    return 0;
}

#define MASTERTIMER_H
#if defined(Q_OS_MACOS) || defined(Q_OS_IOS)
#define clock_get_time readClock
#else
#define clock_gettime readClock
#endif
#define nanosleep virtualSleep
#define qobject_cast static_cast
#include "mastertimer-unix.cpp"
#undef qobject_cast
#undef nanosleep
#if defined(Q_OS_MACOS) || defined(Q_OS_IOS)
#undef clock_get_time
#else
#undef clock_gettime
#endif
#undef MASTERTIMER_H
#undef MasterTimerPrivate
#undef MasterTimer

void MasterTimerUnix_Test::diagnostics_data()
{
    QTest::addColumn<bool>("enabled");
    QTest::addColumn<uint>("hz");
    QTest::addColumn<qint64>("initialNs");
    QTest::addColumn<QList<qint64>>("callbackCosts");
    QTest::addColumn<QList<qint64>>("sleepDelays");
    QTest::addColumn<QList<qint64>>("expectedStarts");
    QTest::addColumn<QStringList>("expectedFields");
    QTest::addColumn<int>("failedClockRead");

    for (bool enabled : {false, true})
    {
        const QByteArray suffix = enabled ? "-enabled" : "-disabled";
        QTest::newRow(("callback" + suffix).constData())
            << enabled << uint(50) << qint64(0)
            << QList<qint64>{25 * MS, 5 * MS, 5 * MS, 5 * MS}
            << QList<qint64>{}
            << QList<qint64>{20 * MS, 45 * MS, 70 * MS, 90 * MS}
            << QStringList{"period=20.000ms", "maxDispatchLate=5.000ms", "slept=no",
                           "priorCallback=25.000ms", "priorFinishedLate=5.000ms"} << 0;
        QTest::newRow(("wake" + suffix).constData())
            << enabled << uint(50) << qint64(0)
            << QList<qint64>{5 * MS, 5 * MS, 5 * MS, 5 * MS}
            << QList<qint64>{0, 25 * MS}
            << QList<qint64>{20 * MS, 65 * MS, 70 * MS, 95 * MS}
            << QStringList{"period=20.000ms", "maxDispatchLate=25.000ms", "slept=yes",
                           "priorCallback=5.000ms", "priorFinishedLate=0.000ms"} << 0;
        QTest::newRow(("first-wake" + suffix).constData())
            << enabled << uint(50) << qint64(0)
            << QList<qint64>{5 * MS, 5 * MS, 5 * MS, 5 * MS}
            << QList<qint64>{3 * MS}
            << QList<qint64>{23 * MS, 40 * MS, 60 * MS, 80 * MS}
            << QStringList{"maxDispatchLate=3.000ms", "slept=yes",
                           "priorCallback=unknown", "priorFinishedLate=unknown"} << 0;
        QTest::newRow(("rollover" + suffix).constData())
            << enabled << uint(50) << qint64(980 * MS)
            << QList<qint64>{25 * MS, 5 * MS, 5 * MS, 5 * MS}
            << QList<qint64>{}
            << QList<qint64>{20 * MS, 45 * MS, 70 * MS, 90 * MS}
            << QStringList{"maxDispatchLate=5.000ms", "priorFinishedLate=5.000ms"} << 0;
        QTest::newRow(("fractional-period" + suffix).constData())
            << enabled << uint(60) << qint64(0)
            << QList<qint64>{20 * MS, MS, MS, MS}
            << QList<qint64>{}
            << QList<qint64>{16666666, 36666666, 54333332, 70999998}
            << QStringList{"period=16.667ms", "maxDispatchLate=3.333ms",
                           "priorCallback=20.000ms", "priorFinishedLate=3.333ms"} << 0;
        QTest::newRow(("exact-deadline-policy" + suffix).constData())
            << enabled << uint(50) << qint64(0)
            << QList<qint64>{20 * MS, 5 * MS, 5 * MS, 5 * MS}
            << QList<qint64>{}
            << QList<qint64>{20 * MS, 40 * MS, 65 * MS, 85 * MS}
            << QStringList{"maxDispatchLate=0.000ms"} << 0;
        QTest::newRow(("diagnostic-clock-failure" + suffix).constData())
            << enabled << uint(50) << qint64(0)
            << QList<qint64>{5 * MS}
            << QList<qint64>{}
            << QList<qint64>{20 * MS}
            << QStringList{"maxDispatchLate=unknown", "priorCallback=unknown",
                           "measuredTicks=0", "unknownTicks=1", "clockReadFailures=1"}
            << (enabled ? 3 : 0);
        QTest::newRow(("mixed-clock-failure" + suffix).constData())
            << enabled << uint(50) << qint64(0)
            << QList<qint64>{5 * MS, 5 * MS, 5 * MS, 5 * MS}
            << QList<qint64>{}
            << QList<qint64>{20 * MS, 40 * MS, 60 * MS, 80 * MS}
            << QStringList{"maxDispatchLate=0.000ms", "measuredTicks=3",
                           "unknownTicks=1", "clockReadFailures=1"}
            << (enabled ? 3 : 0);
        QTest::newRow(("unavailable-diagnostic-clock" + suffix).constData())
            << enabled << uint(50) << qint64(0)
            << QList<qint64>{5 * MS, 5 * MS, 5 * MS, 5 * MS}
            << QList<qint64>{}
            << QList<qint64>{20 * MS, 40 * MS, 60 * MS, 80 * MS}
            << QStringList{"maxDispatchLate=unknown", "measuredTicks=0",
                           "unknownTicks=4", "clockReadFailures=8"}
            << (enabled ? -1 : 0);
    }
}

void MasterTimerUnix_Test::diagnostics()
{
    QFETCH(bool, enabled);
    QFETCH(uint, hz);
    QFETCH(qint64, initialNs);
    QFETCH(QList<qint64>, callbackCosts);
    QFETCH(QList<qint64>, sleepDelays);
    QFETCH(QList<qint64>, expectedStarts);
    QFETCH(QStringList, expectedFields);
    QFETCH(int, failedClockRead);

    ::frequency = hz;
    clockNs = originNs = initialNs;
    costs = callbackCosts;
    wakeDelays = sleepDelays;
    starts.clear();
    sleeps.clear();
    messages.clear();
    clockReads = 0;
    failedRead = failedClockRead;
    TimingDiag::resetForTest();
    TimingDiag::setEnabledForTest(enabled);
    TimingDiag::setClockForTest([] { return clockNs / MS; });
    TimingDiag::setIntervalMsForTest(1000);
    const auto previousHandler = qInstallMessageHandler(
        [](QtMsgType, const QMessageLogContext &, const QString &message)
        {
            if (message.startsWith("[timing]"))
                messages.append(message);
        });

    VirtualMasterTimer timer;
    VirtualMasterTimerPrivate loop(&timer);
    driver = &loop;
    loop.run();
    clockNs += 1000 * MS;
    TimingDiag::flushExpired();

    qInstallMessageHandler(previousHandler);
    TimingDiag::setEnabledForTest(false);
    TimingDiag::setClockForTest(nullptr);
    TimingDiag::resetForTest();

    QCOMPARE(starts, expectedStarts);
    if (!enabled)
        QCOMPARE(clockReads, 1 + starts.size() + (starts.size() - sleeps.size()));
    const QStringList reports = messages.filter("[timing] scheduler dispatch:");
    QCOMPARE(reports.size(), enabled ? 1 : 0);
    QCOMPARE(messages.size(), reports.size());
    if (enabled)
    {
        QVERIFY2(reports.first().contains(QString("ticks=%1").arg(costs.size())),
                 qPrintable(reports.first()));
        for (const QString &field : expectedFields)
            QVERIFY2(reports.first().contains(field), qPrintable(reports.first()));
    }
}

namespace
{
    // Drive the real scheduling loop with the virtual clock over the given
    // callback costs, capturing [timing] output and force-flushing the final
    // window. The module globals hold the results. The caller sets/clears onTick
    // and the TimingDiag gate before invoking.
    void runScheduler(uint hz, qint64 initialNs, const QList<qint64> &callbackCosts,
                      const QList<qint64> &sleepDelays, int failedClockRead = 0)
    {
        ::frequency = hz;
        clockNs = originNs = initialNs;
        costs = callbackCosts;
        wakeDelays = sleepDelays;
        starts.clear();
        sleeps.clear();
        messages.clear();
        messageTypes.clear();
        messageTimes.clear();
        clockReads = 0;
        failedRead = failedClockRead;
        TimingDiag::setClockForTest([] { return clockNs / MS; });
        TimingDiag::setIntervalMsForTest(1000);

        const auto previousHandler = qInstallMessageHandler(
            [](QtMsgType type, const QMessageLogContext &, const QString &message)
            {
                messages.append(message);
                messageTypes.append(type);
                messageTimes.append(clockNs - originNs);
            });

        VirtualMasterTimer timer;
        VirtualMasterTimerPrivate loop(&timer);
        driver = &loop;
        loop.run();
        clockNs += 1000 * MS;   // force the final window past the interval
        TimingDiag::flushExpired();

        qInstallMessageHandler(previousHandler);
    }
}

void MasterTimerUnix_Test::healthMessages_data()
{
    QTest::addColumn<bool>("enabled");
    QTest::addColumn<uint>("hz");
    QTest::addColumn<qint64>("initialNs");
    QTest::addColumn<QList<qint64>>("callbackCosts");
    QTest::addColumn<QList<qint64>>("sleepDelays");
    QTest::addColumn<QList<qint64>>("expectedStarts");
    QTest::addColumn<QStringList>("expectedReports");
    QTest::addColumn<QList<int>>("expectedTypes");
    QTest::addColumn<QList<qint64>>("expectedTimes");

    const auto report = [](bool warning, int checked, qint64 duration, int overdue,
                           int moderate, int whole, qint64 worst, qint64 period)
    {
        return QString("[timer-health] pre-sleep deadline check: severity=%1 "
                       "checkedTicks=%2 in %3ms overdue=%4 moderate=%5 wholePeriod=%6 "
                       "worstOverdue=%7ms period=%8ms")
            .arg(warning ? "warning" : "debug").arg(checked)
            .arg(double(duration) / MS, 0, 'f', 6).arg(overdue).arg(moderate).arg(whole)
            .arg(double(worst) / MS, 0, 'f', 6).arg(double(period) / MS, 0, 'f', 6);
    };
    for (bool enabled : {false, true})
    {
        const QByteArray suffix = enabled ? "-enabled" : "-disabled";
        QTest::newRow(("nanosecond-rollover-tail" + suffix).constData())
            << enabled << uint(50) << qint64(960 * MS - 1)
            << QList<qint64>{20 * MS + 1, 5 * MS} << QList<qint64>{}
            << QList<qint64>{20 * MS, 40 * MS + 1}
            << QStringList{report(false, 2, 40 * MS + 1, 1, 0, 0, 1, 20 * MS)}
            << QList<int>{QtDebugMsg} << QList<qint64>{45 * MS + 1};
        QTest::newRow(("disjoint-boundaries" + suffix).constData())
            << enabled << uint(50) << qint64(0)
            << QList<qint64>{20 * MS + 1, 0, 25 * MS - 1, 0, 25 * MS, 0,
                             27 * MS, 0, 40 * MS - 1, 0, 40 * MS, 0}
            << QList<qint64>{}
            << QList<qint64>{20 * MS, 40 * MS + 1, 60 * MS + 1, 85 * MS,
                             105 * MS, 130 * MS, 150 * MS, 177 * MS, 197 * MS,
                             237 * MS - 1, 257 * MS - 1, 297 * MS - 1}
            << QStringList{report(true, 12, 297 * MS - 1, 6, 3, 1, 20 * MS, 20 * MS)}
            << QList<int>{QtWarningMsg} << QList<qint64>{297 * MS - 1};
        QTest::newRow(("fractional-quarter-period" + suffix).constData())
            << enabled << uint(60) << qint64(0)
            << QList<qint64>{20833332, 0, 20833333, 0} << QList<qint64>{}
            << QList<qint64>{16666666, 37499998, 54166664, 74999997}
            << QStringList{report(true, 4, 74999997, 2, 1, 0, 4166667, 16666666)}
            << QList<int>{QtWarningMsg} << QList<qint64>{74999997};
        QTest::newRow(("25hz-disjoint-boundaries" + suffix).constData())
            << enabled << uint(25) << qint64(0)
            << QList<qint64>{50 * MS - 1, 0, 50 * MS, 0, 80 * MS - 1, 0, 80 * MS, 0}
            << QList<qint64>{}
            << QList<qint64>{40 * MS, 90 * MS - 1, 130 * MS - 1, 180 * MS - 1,
                             220 * MS - 1, 300 * MS - 2, 340 * MS - 2, 420 * MS - 2}
            << QStringList{report(true, 8, 420 * MS - 2, 4, 2, 1, 40 * MS, 40 * MS)}
            << QList<int>{QtWarningMsg} << QList<qint64>{420 * MS - 2};
        QTest::newRow(("equality-no-sleep-rebase" + suffix).constData())
            << enabled << uint(50) << qint64(0)
            << QList<qint64>{20 * MS, 5 * MS, 5 * MS, 5 * MS} << QList<qint64>{}
            << QList<qint64>{20 * MS, 40 * MS, 65 * MS, 85 * MS}
            << QStringList{} << QList<int>{} << QList<qint64>{};
        QTest::newRow(("late-wake-not-check" + suffix).constData())
            << enabled << uint(50) << qint64(0)
            << QList<qint64>{0} << QList<qint64>{27 * MS} << QList<qint64>{47 * MS}
            << QStringList{} << QList<int>{} << QList<qint64>{};

        for (uint hz : {50u, 25u})
        {
            const qint64 period = 1000000000LL / hz;
            for (int checked : {11, 10})
            {
                QList<qint64> callbacks(checked, 0), dispatches;
                callbacks[0] = period + 7 * MS;
                for (int i = 0; i < checked; ++i)
                    dispatches.append((i + 1) * period + (i ? 7 * MS : 0));
                const bool warning = hz == 50 && checked == 10;
                QTest::newRow((QByteArray::number(hz) + "hz-7ms-" +
                               QByteArray::number(checked) + "-checks" + suffix).constData())
                    << enabled << hz << qint64(0) << callbacks << QList<qint64>{}
                    << dispatches
                    << QStringList{report(warning, checked, dispatches.at(checked - 2),
                                          1, hz == 50 ? 1 : 0, 0, 7 * MS, period)}
                    << QList<int>{warning ? QtWarningMsg : QtDebugMsg}
                    << QList<qint64>{dispatches.last()};
            }
        }

        QList<qint64> cleanCosts(504, 0), cleanStarts;
        for (int i = 0; i < cleanCosts.size(); ++i)
            cleanStarts.append((i + 1) * 20 * MS);
        QTest::newRow(("clean-multiple-windows" + suffix).constData())
            << enabled << uint(50) << qint64(0) << cleanCosts << QList<qint64>{}
            << cleanStarts << QStringList{} << QList<int>{} << QList<qint64>{};

        QList<qint64> resetCosts = cleanCosts, resetStarts = cleanStarts;
        resetCosts[0] = 27 * MS;
        resetCosts[502] = 45 * MS;
        resetCosts[503] = 5 * MS;
        for (int i = 1; i < resetStarts.size(); ++i)
            resetStarts[i] += 7 * MS;
        resetStarts[503] = 10112 * MS;
        QTest::newRow(("nonclean-clean-tail-reset" + suffix).constData())
            << enabled << uint(50) << qint64(0) << resetCosts << QList<qint64>{}
            << resetStarts
            << QStringList{report(false, 251, 5007 * MS, 1, 1, 0, 7 * MS, 20 * MS),
                           report(true, 3, 105 * MS, 1, 0, 1, 25 * MS, 20 * MS)}
            << QList<int>{QtDebugMsg, QtWarningMsg} << QList<qint64>{5007 * MS, 10117 * MS};

        QList<qint64> overloadCosts(204, 40 * MS), overloadStarts;
        for (int i = 0; i < overloadCosts.size(); ++i)
            overloadStarts.append((100 * (i / 2) + (i % 2 ? 60 : 20)) * MS);
        QTest::newRow(("overload-two-windows-tail" + suffix).constData())
            << enabled << uint(50) << qint64(0) << overloadCosts << QList<qint64>{}
            << overloadStarts
            << QStringList{report(true, 101, 5000 * MS, 50, 0, 50, 20 * MS, 20 * MS),
                           report(true, 100, 5000 * MS, 50, 0, 50, 20 * MS, 20 * MS),
                           report(true, 3, 160 * MS, 2, 0, 2, 20 * MS, 20 * MS)}
            << QList<int>{QtWarningMsg, QtWarningMsg, QtWarningMsg}
            << QList<qint64>{5000 * MS, 10000 * MS, 10200 * MS};
        QTest::newRow(("long-jump-crossing-check-tail" + suffix).constData())
            << enabled << uint(50) << qint64(0)
            << QList<qint64>{16000 * MS, 0, 27 * MS, 5 * MS} << QList<qint64>{}
            << QList<qint64>{20 * MS, 16020 * MS, 16040 * MS, 16067 * MS}
            << QStringList{report(true, 2, 16020 * MS, 1, 0, 1, 15980 * MS, 20 * MS),
                           report(true, 2, 47 * MS, 1, 1, 0, 7 * MS, 20 * MS)}
            << QList<int>{QtWarningMsg, QtWarningMsg}
            << QList<qint64>{16020 * MS, 16072 * MS};
    }
}

void MasterTimerUnix_Test::healthMessages()
{
    QFETCH(bool, enabled);
    QFETCH(uint, hz);
    QFETCH(qint64, initialNs);
    QFETCH(QList<qint64>, callbackCosts);
    QFETCH(QList<qint64>, sleepDelays);
    QFETCH(QList<qint64>, expectedStarts);
    QFETCH(QStringList, expectedReports);
    QFETCH(QList<int>, expectedTypes);
    QFETCH(QList<qint64>, expectedTimes);

    TimingDiag::resetForTest();
    TimingDiag::setEnabledForTest(enabled);
    runScheduler(hz, initialNs, callbackCosts, sleepDelays);
    TimingDiag::setEnabledForTest(false);
    TimingDiag::setClockForTest(nullptr);
    TimingDiag::resetForTest();

    QCOMPARE(starts, expectedStarts);
    QCOMPARE(clockReads, 1 + starts.size() + (starts.size() - sleeps.size())
                         + (enabled ? 2 * starts.size() : 0));
    QStringList reports;
    QList<int> types;
    QList<qint64> times;
    for (int i = 0; i < messages.size(); ++i)
    {
        if (messages.at(i).startsWith("[timer-health]"))
        {
            reports.append(messages.at(i));
            types.append(messageTypes.at(i));
            times.append(messageTimes.at(i));
        }
    }
    QCOMPARE(reports, expectedReports);
    QCOMPARE(types, expectedTypes);
    QCOMPARE(times, expectedTimes);
    QVERIFY(messages.filter("Time is late by").isEmpty());
    QVERIFY(messages.filter("MasterTimer is running late!").isEmpty());
}

void MasterTimerUnix_Test::schedulingClockFailure_data()
{
    QTest::addColumn<bool>("enabled");
    QTest::addColumn<int>("failedClockRead");
    QTest::addColumn<QList<qint64>>("callbackCosts");
    QTest::addColumn<QList<qint64>>("expectedStarts");
    QTest::addColumn<QList<qint64>>("expectedSleeps");
    QTest::addColumn<QStringList>("expectedHealth");
    QTest::addColumn<qint64>("errorTime");

    for (bool enabled : {false, true})
    {
        const QByteArray suffix = enabled ? "-enabled" : "-disabled";
        QTest::newRow(("initial-read" + suffix).constData())
            << enabled << 1 << QList<qint64>{5 * MS}
            << QList<qint64>{} << QList<qint64>{} << QStringList{} << qint64(0);
        QTest::newRow(("current-read" + suffix).constData())
            << enabled << 2 << QList<qint64>{5 * MS}
            << QList<qint64>{} << QList<qint64>{} << QStringList{} << qint64(0);
        QTest::newRow(("current-after-overdue" + suffix).constData())
            << enabled << (enabled ? 9 : 5) << QList<qint64>{27 * MS, 0, MS}
            << QList<qint64>{20 * MS, 47 * MS} << QList<qint64>{20 * MS}
            << QStringList{} << qint64(47 * MS);
        QTest::newRow(("current-after-complete-and-overdue-tail" + suffix).constData())
            << enabled << (enabled ? 16 : 8)
            << QList<qint64>{5000 * MS, 0, 27 * MS, 0, MS}
            << QList<qint64>{20 * MS, 5020 * MS, 5040 * MS, 5067 * MS}
            << QList<qint64>{20 * MS, 20 * MS}
            << QStringList{"[timer-health] pre-sleep deadline check: severity=warning "
                           "checkedTicks=2 in 5020.000000ms overdue=1 moderate=0 wholePeriod=1 "
                           "worstOverdue=4980.000000ms period=20.000000ms"}
            << qint64(5067 * MS);
    }
}

void MasterTimerUnix_Test::schedulingClockFailure()
{
    QFETCH(bool, enabled);
    QFETCH(int, failedClockRead);
    QFETCH(QList<qint64>, callbackCosts);
    QFETCH(QList<qint64>, expectedStarts);
    QFETCH(QList<qint64>, expectedSleeps);
    QFETCH(QStringList, expectedHealth);
    QFETCH(qint64, errorTime);

    TimingDiag::resetForTest();
    TimingDiag::setEnabledForTest(enabled);
    runScheduler(50, 0, callbackCosts, {}, failedClockRead);
    TimingDiag::setEnabledForTest(false);
    TimingDiag::setClockForTest(nullptr);
    TimingDiag::resetForTest();

    QCOMPARE(starts, expectedStarts);
    QCOMPARE(sleeps, expectedSleeps);
    QCOMPARE(clockReads, failedClockRead);
    QCOMPARE(messages.filter("[timer-health]"), expectedHealth);
    const QStringList errors = messages.filter("Unable to get");
    QCOMPARE(errors.size(), 1);
    QVERIFY2(errors.first().contains(failedClockRead == 1 ? "Unable to get the time accurately:"
                                                        : "Unable to get the current time:"),
             qPrintable(errors.first()));
    QVERIFY(errors.first().contains(QString::fromLocal8Bit(strerror(EIO))));
    QCOMPARE(errors.first().contains("- Stopping MasterTimerPrivate"), failedClockRead == 1);
    const int errorIndex = messages.indexOf(errors.first());
    QCOMPARE(messageTypes.at(errorIndex), int(QtWarningMsg));
    QCOMPARE(messageTimes.at(errorIndex), errorTime);
    if (!expectedHealth.isEmpty())
    {
        const int healthIndex = messages.indexOf(expectedHealth.first());
        QVERIFY(healthIndex < errorIndex);
        QCOMPARE(messageTypes.at(healthIndex), int(QtWarningMsg));
        QCOMPARE(messageTimes.at(healthIndex), 5020 * MS);
    }
}

/* C0-2: enabling while the loop already runs begins sampling on a later tick and
   the first sampled tick carries no prior-callback state. Disabled ticks add no
   diagnostic clock reads. */
void MasterTimerUnix_Test::runtimeEnableStartsSamplingAndResetsMarkers()
{
    TimingDiag::setEnabledForTest(false);
    TimingDiag::resetForTest();
    onTick = [](qsizetype index) {
        if (index == 2)
            TimingDiag::setEnabled(true);   // production off->on: fresh capture
    };

    runScheduler(50, 0, {5 * MS, 5 * MS, 5 * MS, 5 * MS}, {});

    const QStringList reports = messages.filter("[timing] scheduler dispatch:");
    QCOMPARE(reports.size(), 1);
    // Only the last tick was enabled: one sampled tick, no prior marker.
    QVERIFY2(reports.first().contains("dispatch: ticks=1"), qPrintable(reports.first()));
    QVERIFY2(reports.first().contains("priorCallback=unknown"), qPrintable(reports.first()));
    QVERIFY2(reports.first().contains("measuredTicks=1"), qPrintable(reports.first()));
    // 1 initial finish read + one current read per iteration + 2 diagnostic reads
    // for the single enabled tick. The three disabled ticks add zero.
    QCOMPARE(clockReads, 1 + starts.size() + (starts.size() - sleeps.size()) + 2);

    onTick = nullptr;
    TimingDiag::setEnabledForTest(false);
    TimingDiag::setClockForTest(nullptr);
    TimingDiag::resetForTest();
}

/* C0-2: an enable -> disable -> enable cycle drops the prior-callback markers of
   the earlier capture, so the first tick after re-enable reports unknown rather
   than reusing the last pre-disable callback duration. */
void MasterTimerUnix_Test::enableDisableEnableResetsPriorMarkers()
{
    TimingDiag::setEnabledForTest(false);
    TimingDiag::resetForTest();
    onTick = [](qsizetype index) {
        if (index == 0)      TimingDiag::setEnabled(true);   // ticks 1,2 sampled
        else if (index == 2) TimingDiag::setEnabled(false);  // ticks 3,4 idle
        else if (index == 4) TimingDiag::setEnabled(true);   // tick 5: fresh capture
    };

    runScheduler(50, 0, {5 * MS, 5 * MS, 5 * MS, 5 * MS, 5 * MS, 5 * MS}, {});

    const QStringList reports = messages.filter("[timing] scheduler dispatch:");
    QCOMPARE(reports.size(), 1);   // re-enable cleared capture #1; only tick 5 remains
    QVERIFY2(reports.first().contains("dispatch: ticks=1"), qPrintable(reports.first()));
    // Had the markers not been reset, this would read priorCallback=5.000ms.
    QVERIFY2(reports.first().contains("priorCallback=unknown"), qPrintable(reports.first()));

    onTick = nullptr;
    TimingDiag::setEnabledForTest(false);
    TimingDiag::setClockForTest(nullptr);
    TimingDiag::resetForTest();
}

/* C0-8: a reset while enabled starts a fresh capture. The tick whose record
   straddles the reset is discarded (not mixed into the new capture), and the
   next tick samples cleanly with no prior-callback carry-over. */
void MasterTimerUnix_Test::resetWhileEnabledStartsFreshCapture()
{
    TimingDiag::setEnabledForTest(true);
    TimingDiag::resetForTest();
    onTick = [](qsizetype index) {
        if (index == 3)
            TimingDiag::reset();   // lands between tick 3's id read and its record
    };

    runScheduler(50, 0, {5 * MS, 5 * MS, 5 * MS, 5 * MS, 5 * MS}, {});

    const QStringList reports = messages.filter("[timing] scheduler dispatch:");
    QCOMPARE(reports.size(), 1);   // reset cleared ticks 0-2; only tick 4 remains
    QVERIFY2(reports.first().contains("dispatch: ticks=1"), qPrintable(reports.first()));
    QVERIFY2(reports.first().contains("priorCallback=unknown"), qPrintable(reports.first()));
    // Tick 3's record carried the superseded capture id and was rejected.
    QCOMPARE(TimingDiag::snapshot().discardedSamples, qint64(1));

    onTick = nullptr;
    TimingDiag::setEnabledForTest(false);
    TimingDiag::setClockForTest(nullptr);
    TimingDiag::resetForTest();
}

QTEST_MAIN(MasterTimerUnix_Test)