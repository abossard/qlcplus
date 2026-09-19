/*
  Q Light Controller Plus
  timingdiagnostics.cpp

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

#include <QMutex>
#include <QHash>
#include <QVector>
#include <QDebug>
#include <atomic>
#include <chrono>

#include "timingdiagnostics.h"

#if defined(Q_OS_WIN)
#  include <windows.h>
#elif defined(Q_OS_MACOS) || defined(Q_OS_IOS)
#  include <mach/mach.h>
#  include <mach/thread_act.h>
#  include <time.h>
#else
#  include <time.h>
#endif

namespace
{
    /* -1 unresolved, 0 disabled, 1 enabled. Resolved from the environment on
       first use, then a plain relaxed load on the hot path. A production or test
       setter stores 0/1 directly, after which the one-shot environment resolve
       never clobbers it. */
    std::atomic<int> s_enabled{-1};
    std::atomic<bool> s_envResolved{false};

    /* Capture identity. Bumped by beginNewCapture() (a new run of sampling).
       A sample tagged with a stale id straddled a control change and is dropped.
       Atomic so the scheduler loop and HUE worker can read it without the lock. */
    std::atomic<qint64> s_captureId{0};

    enum Kind { RgbStep, Period, Handoff, Scheduler };

    struct Sample
    {
        quint32 id = 0;
        QString name;
        QString type;
        qint64 a = -1;  // report-specific ns metric
        qint64 b = -1;
        qint64 c = -1;
        qint64 d = -1;
        qint64 e = -1;
        bool slept = false;
        int clockReadFailures = 0;
    };

    struct Window
    {
        Kind kind = RgbStep;
        /* Live window: emitted and reset by flushExpired() each interval. */
        bool started = false;
        qint64 startMs = 0;
        int count = 0;
        int unknownTicks = 0;
        int clockReadFailures = 0;
        qint64 worstPrimary = -1;
        Sample worst;
        /* Capture-cumulative aggregate: survives flush and disable; cleared only
           when a new capture begins (s_windows is emptied by beginNewCapture). */
        bool cumStarted = false;
        qint64 cumStartMs = 0;
        qint64 cumCount = 0;
        qint64 cumUnknownTicks = 0;
        qint64 cumClockReadFailures = 0;
        qint64 cumCpuUnknown = 0;
        qint64 cumWorstPrimary = -1;
        Sample cumWorst;
    };

    QMutex s_mutex;
    QHash<QString, Window> s_windows;
    qint64 s_intervalMs = 1000;
    qint64 s_captureStartMs = 0;    // guarded by s_mutex
    qint64 s_captureEndMs = 0;      // frozen recording end while not advancing (guarded)
    qint64 s_discarded = 0;         // guarded by s_mutex
    std::function<qint64()> s_clock; // null => real monotonic clock

    qint64 realNowNs()
    {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
                   std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    qint64 nowMs()
    {
        return s_clock ? s_clock() : (realNowNs() / 1000000);
    }

    int resolveEnv()
    {
        if (!qEnvironmentVariableIsSet("QLCPLUS_TIMING_DIAG"))
            return 0;
        const QByteArray v = qgetenv("QLCPLUS_TIMING_DIAG");
        return (v == "0" || v == "false" || v.isEmpty()) ? 0 : 1;
    }

    /* Resolve QLCPLUS_TIMING_DIAG(_MS) from the environment exactly once. Never
       clobbers a value a production/test setter already chose: the enabled gate
       is only seeded from the environment while it is still unresolved (-1), and
       the interval only when the env var is present. Initialization is serialized
       under s_mutex and the completion flag is published LAST with release, so a
       thread that observes s_envResolved via acquire also observes the published
       s_enabled/interval - it can never read a still-unresolved -1 gate or race a
       runtime setter against the initializer. */
    void resolveEnvOnce()
    {
        if (s_envResolved.load(std::memory_order_acquire))
            return;

        QMutexLocker locker(&s_mutex);
        if (s_envResolved.load(std::memory_order_relaxed))
            return;                 // another thread finished while we waited

        if (s_enabled.load(std::memory_order_relaxed) < 0)
            s_enabled.store(resolveEnv(), std::memory_order_relaxed);

        if (qEnvironmentVariableIsSet("QLCPLUS_TIMING_DIAG_MS"))
        {
            bool ok = false;
            const qint64 ms = qgetenv("QLCPLUS_TIMING_DIAG_MS").toLongLong(&ok);
            if (ok && ms > 0)
                s_intervalMs = ms;  // already under s_mutex
        }

        // Anchor the initial capture at first resolve so an env-launch capture
        // reports a sensible duration instead of "now - 0". A disabled launch
        // stays frozen at duration 0 until an explicit enable starts a capture.
        const qint64 nowMsVal = s_clock ? s_clock() : (realNowNs() / 1000000);
        s_captureStartMs = nowMsVal;
        s_captureEndMs = nowMsVal;

        // Publish completion LAST so the writes above are visible to any thread
        // that observes s_envResolved via acquire.
        s_envResolved.store(true, std::memory_order_release);
    }

    /* Start a fresh capture: bump the identity, drop retained aggregates and the
       discarded counter, and re-anchor the capture start. A fresh capture has
       zero recording duration until it advances (i.e. while enabled). Caller
       holds s_mutex. */
    void beginNewCaptureLocked()
    {
        s_windows.clear();
        s_discarded = 0;
        s_captureStartMs = s_clock ? s_clock() : (realNowNs() / 1000000);
        s_captureEndMs = s_captureStartMs;   // duration 0 until it advances
        s_captureId.fetch_add(1, std::memory_order_relaxed);
    }

    QString ms3(qint64 ns)
    {
        if (ns < 0)
            return QStringLiteral("unknown");
        return QString::number(double(ns) / 1e6, 'f', 3) + QStringLiteral("ms");
    }

    // Function::invalidId() is UINT32_MAX; render it as "unknown" rather than a
    // raw sentinel number, since id 0 is a valid Function id.
    QString idStr(quint32 id)
    {
        return id == 0xFFFFFFFFu ? QStringLiteral("unknown") : QString::number(id);
    }

    /* Accumulate one sample into its bucket. Never emits; flushExpired() is the
       sole emitter, so a rare event is reported within one tick of interval
       expiry rather than only when the next event arrives. A sample whose
       captureId no longer matches the live capture straddled a control change
       (reset / off->on) and is discarded instead of mixed in. */
    void record(Kind kind, const QString &key, const Sample &sample, qint64 primary,
                qint64 sampleCaptureId, bool cpuUnknown = false)
    {
        QMutexLocker locker(&s_mutex);

        if (sampleCaptureId >= 0 &&
            sampleCaptureId != s_captureId.load(std::memory_order_relaxed))
        {
            ++s_discarded;
            return;
        }

        Window &w = s_windows[key];
        w.kind = kind;

        /* Live window (periodic [timing] flush). */
        if (!w.started)
        {
            w.started = true;
            w.startMs = nowMs();
            w.count = 0;
            w.unknownTicks = 0;
            w.clockReadFailures = 0;
            w.worstPrimary = -1;
        }
        w.count++;
        if (kind == Scheduler)
        {
            w.unknownTicks += int(sample.a < 0);
            w.clockReadFailures += sample.clockReadFailures;
        }
        if (primary >= w.worstPrimary)
        {
            w.worstPrimary = primary;
            w.worst = sample;
        }

        /* Capture-cumulative aggregate (snapshot; survives flush + disable). */
        if (!w.cumStarted)
        {
            w.cumStarted = true;
            w.cumStartMs = nowMs();
        }
        w.cumCount++;
        if (kind == Scheduler)
        {
            w.cumUnknownTicks += int(sample.a < 0);
            w.cumClockReadFailures += sample.clockReadFailures;
        }
        if (kind == Handoff && cpuUnknown)
            w.cumCpuUnknown++;
        if (primary >= w.cumWorstPrimary)
        {
            w.cumWorstPrimary = primary;
            w.cumWorst = sample;
        }
    }

    struct Closed
    {
        Kind kind;
        Sample s;
        int count;
        qint64 windowMs;
        int unknownTicks;
        int clockReadFailures;
    };

    QString format(const Closed &c)
    {
        const Sample &s = c.s;
        switch (c.kind)
        {
        case RgbStep:
            return QStringLiteral("[timing] RGBMatrix step advance id=%1 name=\"%2\" advances=%3 in %4ms")
                    .arg(idStr(s.id)).arg(s.name).arg(c.count).arg(c.windowMs);
        case Period:
        {
            const qint64 unattributed = (s.a >= 0 && s.d >= 0) ? qMax<qint64>(0, s.a - s.d) : -1;
            // When no function write was timed (invalid id), collapse the whole
            // maxWrite identity to "unknown" rather than inventing an empty name,
            // "?" type and a write phase that did not happen. Matches the "unknown"
            // convention used for an unmeasured value (ms3) and invalidId (idStr).
            QString maxWrite;
            if (s.id == 0xFFFFFFFFu)
                maxWrite = QStringLiteral("maxWrite=unknown");
            else
                maxWrite = QStringLiteral("maxWrite=\"%1\" type=%2 phase=write (id %3) %4")
                           .arg(s.name).arg(s.type.isEmpty() ? QStringLiteral("?") : s.type)
                           .arg(s.id).arg(ms3(s.c));
            // Dispatch lateness includes preceding callback delays. This line and the
            // HUEScript handoff line are independent per-window summaries: the
            // max-wait render need not belong to this max-callback tick, so they
            // are separate leads, not proof that a handoff dominated this callback.
            return QStringLiteral("[timing] callback consumed period: count=%1 in %2ms period=%3 "
                                  "maxCallback=%4 functions=%5 unattributed=%6 dispatchLate=%7 %8")
                    .arg(c.count).arg(c.windowMs).arg(ms3(s.b)).arg(ms3(s.a)).arg(ms3(s.d))
                    .arg(ms3(unattributed)).arg(ms3(s.e)).arg(maxWrite);
        }
        case Scheduler:
            return QStringLiteral("[timing] scheduler dispatch: ticks=%1 in %2ms period=%3 "
                                  "maxDispatchLate=%4 slept=%5 priorCallback=%6 priorFinishedLate=%7 "
                                  "measuredTicks=%8 unknownTicks=%9 clockReadFailures=%10")
                    .arg(c.count).arg(c.windowMs).arg(ms3(s.b)).arg(ms3(s.a))
                    .arg(s.slept ? QStringLiteral("yes") : QStringLiteral("no"))
                    .arg(ms3(s.c)).arg(ms3(s.d)).arg(c.count - c.unknownTicks)
                    .arg(c.unknownTicks).arg(c.clockReadFailures);
        case Handoff:
        default:
        {
            // a=wait, b=queue, c=worker, d=workerCpu. returnTail = wait - queue - worker.
            const qint64 tail = (s.a >= 0 && s.b >= 0 && s.c >= 0)
                                ? qMax<qint64>(0, s.a - s.b - s.c) : -1;
            // worker wall minus on-CPU thread time: time the render was not
            // running on its core (descheduling/contention), NOT queue wait and
            // NOT proof of any one scheduler cause. Unknown if either is unknown.
            const qint64 offCpu = (s.c >= 0 && s.d >= 0)
                                  ? qMax<qint64>(0, s.c - s.d) : -1;
            return QStringLiteral("[timing] HUEScript handoff id=%1 name=\"%2\" renders=%3 in %4ms "
                                  "maxWait=%5 queue=%6 worker=%7 workerCpu=%8 workerOffCpu=%9 returnTail=%10 "
                                  "(queue/worker are components of the max-wait render; "
                                  "worker=on-CPU+off-CPU, workerCpu measured on the JS worker thread)")
                    .arg(idStr(s.id)).arg(s.name).arg(c.count).arg(c.windowMs)
                    .arg(ms3(s.a)).arg(ms3(s.b)).arg(ms3(s.c)).arg(ms3(s.d))
                    .arg(ms3(offCpu)).arg(ms3(tail));
        }
        }
    }
}

namespace TimingDiag
{

bool enabled()
{
    // Hot path: an acquire load of the resolved flag (predicted true) followed by
    // a relaxed load of the gate. The acquire pairs with the release in
    // resolveEnvOnce so a "resolved" observation guarantees the gate is published;
    // no clock read, allocation or formatting on either branch.
    if (Q_UNLIKELY(!s_envResolved.load(std::memory_order_acquire)))
        resolveEnvOnce();
    return s_enabled.load(std::memory_order_relaxed) != 0;
}

qint64 nowNs()
{
    return realNowNs();
}

qint64 threadCpuNs()
{
#if defined(Q_OS_WIN)
    FILETIME creation, exit, kernel, user;
    if (!GetThreadTimes(GetCurrentThread(), &creation, &exit, &kernel, &user))
        return -1;
    const auto to100ns = [](const FILETIME &ft) -> qint64 {
        ULARGE_INTEGER v;
        v.LowPart = ft.dwLowDateTime;
        v.HighPart = ft.dwHighDateTime;
        return qint64(v.QuadPart);
    };
    // FILETIME kernel/user are in 100-ns units of CPU time.
    return (to100ns(kernel) + to100ns(user)) * 100;
#elif defined(Q_OS_MACOS) || defined(Q_OS_IOS)
    // thread_info gives per-thread user+system CPU time. mach_thread_self()
    // returns a send right that must be released, or the port table leaks.
    mach_port_t self = mach_thread_self();
    thread_basic_info_data_t info;
    mach_msg_type_number_t count = THREAD_BASIC_INFO_COUNT;
    const kern_return_t kr = thread_info(self, THREAD_BASIC_INFO,
                                         reinterpret_cast<thread_info_t>(&info), &count);
    mach_port_deallocate(mach_task_self(), self);
    if (kr != KERN_SUCCESS)
        return -1;
    const qint64 userNs = qint64(info.user_time.seconds) * 1000000000LL
                          + qint64(info.user_time.microseconds) * 1000LL;
    const qint64 sysNs = qint64(info.system_time.seconds) * 1000000000LL
                          + qint64(info.system_time.microseconds) * 1000LL;
    return userNs + sysNs;
#elif defined(CLOCK_THREAD_CPUTIME_ID)
    struct timespec ts;
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) != 0)
        return -1;
    return qint64(ts.tv_sec) * 1000000000LL + qint64(ts.tv_nsec);
#else
    return -1;
#endif
}

qint64 captureId()
{
    return s_captureId.load(std::memory_order_relaxed);
}

void rgbStepAdvance(quint32 functionId, const QString &name)
{
    if (!enabled())
        return;
    Sample s;
    s.id = functionId;
    s.name = name;
    record(RgbStep, QStringLiteral("rgb:") + QString::number(functionId), s, 0,
           s_captureId.load(std::memory_order_relaxed));
}

void callbackConsumedPeriod(qint64 budgetNs, qint64 callbackNs, qint64 functionsNs,
                            qint64 dispatchLatenessNs, quint32 maxWriteId,
                            const QString &maxWriteType,
                            const QString &maxWriteName, qint64 maxWriteNs,
                            qint64 sampleCaptureId)
{
    if (!enabled())
        return;
    Sample s;
    s.id = maxWriteId;
    s.name = maxWriteName;
    s.type = maxWriteType;
    s.a = callbackNs;
    s.b = budgetNs;
    s.c = maxWriteNs;
    s.d = functionsNs;
    s.e = dispatchLatenessNs;
    record(Period, QStringLiteral("period"), s, callbackNs, sampleCaptureId);
}

void schedulerDispatch(qint64 budgetNs, qint64 dispatchLatenessNs, bool slept,
                       qint64 priorCallbackNs, qint64 priorFinishedLateNs,
                       int clockReadFailures, qint64 sampleCaptureId)
{
    if (!enabled())
        return;
    Sample s;
    s.a = dispatchLatenessNs;
    s.b = budgetNs;
    s.c = priorCallbackNs;
    s.d = priorFinishedLateNs;
    s.slept = slept;
    s.clockReadFailures = clockReadFailures;
    record(Scheduler, QStringLiteral("scheduler"), s, dispatchLatenessNs, sampleCaptureId);
}

void hueHandoff(quint32 functionId, const QString &name,
                qint64 waitNs, qint64 queueNs, qint64 workerNs,
                qint64 workerCpuNs, qint64 sampleCaptureId)
{
    if (!enabled())
        return;
    Sample s;
    s.id = functionId;
    s.name = name;
    s.a = waitNs;
    s.b = queueNs;
    s.c = workerNs;
    s.d = workerCpuNs;
    record(Handoff, QStringLiteral("hue:") + QString::number(functionId), s, waitNs,
           sampleCaptureId, workerCpuNs < 0);
}

void flushExpired()
{
    if (!enabled())
        return;

    QVector<Closed> toEmit;
    {
        QMutexLocker locker(&s_mutex);
        const qint64 t = nowMs();
        for (auto it = s_windows.begin(); it != s_windows.end(); ++it)
        {
            Window &w = it.value();
            if (w.started && w.count > 0 && (t - w.startMs) >= s_intervalMs)
            {
                toEmit.append({w.kind, w.worst, w.count, t - w.startMs,
                               w.unknownTicks, w.clockReadFailures});
                w.started = false;
                w.count = 0;
                w.worstPrimary = -1;
            }
        }
    }
    for (const Closed &c : toEmit)
        qWarning().noquote() << format(c);
}

void flushAll()
{
    if (!enabled())
        return;

    QVector<Closed> toEmit;
    {
        QMutexLocker locker(&s_mutex);
        const qint64 t = nowMs();
        for (auto it = s_windows.begin(); it != s_windows.end(); ++it)
        {
            Window &w = it.value();
            if (w.started && w.count > 0)
            {
                toEmit.append({w.kind, w.worst, w.count, t - w.startMs,
                               w.unknownTicks, w.clockReadFailures});
                w.started = false;
                w.count = 0;
                w.worstPrimary = -1;
            }
        }
    }
    for (const Closed &c : toEmit)
        qWarning().noquote() << format(c);
}

void setEnabled(bool on)
{
    resolveEnvOnce();
    // Serialize the whole transition (was-check, off->on new capture, gate store)
    // under the aggregate lock so it stays coherent with a concurrent reset() and
    // snapshot(): a snapshot never sees the gate flip without the matching
    // capture change, and vice versa.
    QMutexLocker locker(&s_mutex);
    const bool was = s_enabled.load(std::memory_order_relaxed) != 0;
    if (on && !was)
    {
        // off -> on: start a fresh capture so retained aggregates and prior
        // markers from an earlier capture cannot bleed into this one.
        beginNewCaptureLocked();
    }
    else if (!on && was)
    {
        // on -> off: freeze the recording end so a retained completed capture
        // reports a fixed duration; snapshot must not keep growing the
        // denominator against frozen counts.
        s_captureEndMs = s_clock ? s_clock() : (realNowNs() / 1000000);
    }
    // on -> off retains the last capture's aggregates for a post-run snapshot.
    s_enabled.store(on ? 1 : 0, std::memory_order_relaxed);
}

void setIntervalMs(qint64 intervalMs)
{
    resolveEnvOnce();
    if (intervalMs <= 0)
        return;
    QMutexLocker locker(&s_mutex);
    s_intervalMs = intervalMs;
}

void reset()
{
    resolveEnvOnce();
    QMutexLocker locker(&s_mutex);
    beginNewCaptureLocked();
}

CaptureStat snapshot()
{
    resolveEnvOnce();

    CaptureStat out;
    QMutexLocker locker(&s_mutex);
    // Read the capture identity AND the aggregates under one lock. reset() and
    // setEnabled() mutate captureId and the windows under this same lock, so the
    // returned captureId always matches the aggregates it is reported with.
    out.enabled = s_enabled.load(std::memory_order_relaxed) != 0;
    out.captureId = s_captureId.load(std::memory_order_relaxed);
    // While enabled the capture is advancing, so its end is "now". While disabled
    // it is a completed recording with a frozen end, so elapsedMs and every
    // bucket windowMs stay fixed against the frozen counts - a later rate/overhead
    // comparison is not skewed by growing capture age.
    const qint64 t = out.enabled ? nowMs() : s_captureEndMs;
    out.intervalMs = s_intervalMs;
    out.elapsedMs = qMax<qint64>(0, t - s_captureStartMs);
    out.discardedSamples = s_discarded;

    for (auto it = s_windows.constBegin(); it != s_windows.constEnd(); ++it)
    {
        const Window &w = it.value();
        if (!w.cumStarted || w.cumCount <= 0)
            continue;
        BucketStat b;
        switch (w.kind)
        {
        case RgbStep:    b.kind = QStringLiteral("rgb"); break;
        case Period:     b.kind = QStringLiteral("period"); break;
        case Handoff:    b.kind = QStringLiteral("hue"); break;
        case Scheduler:  b.kind = QStringLiteral("scheduler"); break;
        }
        b.key = it.key();
        b.id = w.cumWorst.id;
        b.name = w.cumWorst.name;
        b.type = w.cumWorst.type;
        b.count = w.cumCount;
        b.windowMs = qMax<qint64>(0, t - w.cumStartMs);
        b.worstPrimaryNs = w.cumWorstPrimary;
        b.m0 = w.cumWorst.a;
        b.m1 = w.cumWorst.b;
        b.m2 = w.cumWorst.c;
        b.m3 = w.cumWorst.d;
        b.m4 = w.cumWorst.e;
        b.slept = w.cumWorst.slept;
        b.unknownTicks = w.cumUnknownTicks;
        b.clockReadFailures = w.cumClockReadFailures;
        b.cpuUnknownCount = w.cumCpuUnknown;
        out.buckets.append(b);
    }
    return out;
}

void setEnabledForTest(bool on)
{
    s_enabled.store(on ? 1 : 0, std::memory_order_relaxed);
}

void setClockForTest(std::function<qint64()> nowMs)
{
    QMutexLocker locker(&s_mutex);
    s_clock = std::move(nowMs);
}

void setIntervalMsForTest(qint64 intervalMs)
{
    QMutexLocker locker(&s_mutex);
    if (intervalMs > 0)
        s_intervalMs = intervalMs;
}

void resetForTest()
{
    QMutexLocker locker(&s_mutex);
    s_windows.clear();
    s_discarded = 0;
    s_captureStartMs = s_clock ? s_clock() : (realNowNs() / 1000000);
    s_captureEndMs = s_captureStartMs;
}

} // namespace TimingDiag
