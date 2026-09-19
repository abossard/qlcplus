/*
  Q Light Controller Plus
  mastertimer-unix.cpp

  Copyright (C) Heikki Junnila
                Christopher Staite
                Massimo Callegari

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

#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

#include <QDebug>

#include "mastertimer-unix.h"
#include "mastertimer.h"
#include "timingdiagnostics.h"

/****************************************************************************
 * MasterTimerPrivate
 ****************************************************************************/

MasterTimerPrivate::MasterTimerPrivate(MasterTimer* masterTimer)
    : QThread(masterTimer)
    , m_run(false)
{
    Q_ASSERT(masterTimer != NULL);
#if defined(Q_OS_MACOS) || defined(Q_OS_IOS)
    host_get_clock_service(mach_host_self(), SYSTEM_CLOCK, &cclock);
#endif
}

MasterTimerPrivate::~MasterTimerPrivate()
{
    stop();
}

void MasterTimerPrivate::stop()
{
    m_run = false;
    wait();
}

#if defined(Q_OS_MACOS) || defined(Q_OS_IOS)
int MasterTimerPrivate::compareTime(mach_timespec_t *time1, mach_timespec_t *time2)
#else
int MasterTimerPrivate::compareTime(struct timespec *time1, struct timespec *time2)
#endif
{
    if (time1->tv_sec < time2->tv_sec)
        return -1;
    else if (time1->tv_sec > time2->tv_sec)
        return 1;
    else if (time1->tv_nsec < time2->tv_nsec)
        return -1;
    else if (time1->tv_nsec > time2->tv_nsec)
        return 1;
    else
        return 0;
}

#if defined(Q_OS_MACOS) || defined(Q_OS_IOS)
static qint64 timestampNs(const mach_timespec_t &value)
#else
static qint64 timestampNs(const struct timespec &value)
#endif
{
    return qint64(value.tv_sec) * 1000000000LL + qint64(value.tv_nsec);
}

void MasterTimerPrivate::run()
{
    /* Don't start another thread */
    if (m_run == true)
        return;

    MasterTimer* mt = qobject_cast <MasterTimer*> (parent());
    Q_ASSERT(mt != NULL);

    /* How long to wait each loop, in nanoseconds */
    int nsTickTime = 1000000000L / mt->frequency();

    /* Allocate this from stack here so that GCC doesn't have
       to do it every time implicitly when gettimeofday() is called */
    int ret = 0;

    /* Allocate all the memory at the start so we don't waste any time */
#if defined(Q_OS_MACOS) || defined(Q_OS_IOS)
    mach_timespec_t* finish = static_cast<mach_timespec_t*> (malloc(sizeof(mach_timespec_t)));
    mach_timespec_t* current = static_cast<mach_timespec_t*> (malloc(sizeof(mach_timespec_t)));
#else
    struct timespec* finish = static_cast<struct timespec*> (malloc(sizeof(struct timespec)));
    struct timespec* current = static_cast<struct timespec*> (malloc(sizeof(struct timespec)));
#endif
    struct timespec* sleepTime = static_cast<struct timespec*> (malloc(sizeof(struct timespec)));
    struct timespec* remainingTime = static_cast<struct timespec*> (malloc(sizeof(struct timespec)));

    sleepTime->tv_sec = 0;

    // Runtime-controllable gate: re-read each iteration so diagnostics can be
    // enabled/disabled while the loop is already running (no restart). The
    // capture identity detects a reset or an off->on transition that lands
    // entirely between ticks, so prior-callback markers from an earlier capture
    // never bleed into a new one.
    qint64 cachedCaptureId = -1;
    qint64 previousStartNs = -1;
    qint64 previousEndNs = -1;
    const auto readDiagnosticClock = [&]() -> qint64
    {
        auto sample = *current;
#if defined(Q_OS_MACOS) || defined(Q_OS_IOS)
        const int result = clock_get_time(cclock, &sample);
#else
        const int result = clock_gettime(CLOCK_MONOTONIC, &sample);
#endif
        if (result != 0)
            return -1;
        return timestampNs(sample);
    };
    const auto dispatchTick = [&](bool slept)
    {
        // Single live-gate load per tick. When disabled this is the only added
        // work: no clock reads, no formatting, no allocation.
        if (!TimingDiag::enabled())
        {
            mt->timerTick();
            return;
        }

        // A changed capture identity (reset, or off->on between ticks) drops the
        // stale prior-callback markers so the first tick of a new capture reports
        // priorCallback=unknown rather than reusing another capture's values.
        const qint64 captureId = TimingDiag::captureId();
        if (captureId != cachedCaptureId)
        {
            previousStartNs = -1;
            previousEndNs = -1;
            cachedCaptureId = captureId;
        }

        const qint64 deadlineNs = timestampNs(*finish);
        const qint64 startNs = readDiagnosticClock();
        const qint64 dispatchLateNs = startNs < 0 ? -1 : qMax<qint64>(0, startNs - deadlineNs);
        mt->m_diagDispatchLatenessNs = dispatchLateNs;
        mt->timerTick();
        const qint64 endNs = readDiagnosticClock();

        // Pair dispatch lateness with the PRECEDING callback, not this new tick.
        const qint64 priorCallbackNs = previousStartNs < 0 || previousEndNs < 0
                                      ? -1 : previousEndNs - previousStartNs;
        const qint64 priorFinishedLateNs = previousEndNs < 0
                                          ? -1 : qMax<qint64>(0, previousEndNs - deadlineNs);
        // Tag with the capture id sampled at tick start; a control change during
        // the tick makes schedulerDispatch discard this straddling sample.
        TimingDiag::schedulerDispatch(nsTickTime, dispatchLateNs, slept,
                                     priorCallbackNs, priorFinishedLateNs,
                                     int(startNs < 0) + int(endNs < 0), captureId);
        previousStartNs = startNs;
        previousEndNs = endNs;
    };

    /* This is the start time for the timer */
#if defined(Q_OS_MACOS) || defined(Q_OS_IOS)
    ret = clock_get_time(cclock, finish);
#else
    ret = clock_gettime(CLOCK_MONOTONIC, finish);
#endif
    if (ret == -1)
    {
        qWarning() << Q_FUNC_INFO << "Unable to get the time accurately:"
                   << strerror(errno) << "- Stopping MasterTimerPrivate";
        m_run = false;
    }
    else
    {
        m_run = true;
    }

    bool clockFailed = ret == -1;
    qint64 windowStartNs = clockFailed ? 0 : timestampNs(*finish);
    qint64 lastCheckNs = windowStartNs;
    qint64 checkedTicks = 0, overdueTicks = 0, moderateTicks = 0, wholePeriodTicks = 0;
    qint64 worstOverdueNs = 0;
    const auto reportHealth = [&]()
    {
        if (overdueTicks > 0)
        {
            const bool warning = wholePeriodTicks > 0 || moderateTicks * 10 >= checkedTicks;
            const QString message =
                QString("[timer-health] pre-sleep deadline check: severity=%1 "
                        "checkedTicks=%2 in %3ms overdue=%4 moderate=%5 wholePeriod=%6 "
                        "worstOverdue=%7ms period=%8ms")
                    .arg(warning ? "warning" : "debug").arg(checkedTicks)
                    .arg(double(lastCheckNs - windowStartNs) / 1000000, 0, 'f', 6)
                    .arg(overdueTicks).arg(moderateTicks).arg(wholePeriodTicks)
                    .arg(double(worstOverdueNs) / 1000000, 0, 'f', 6)
                    .arg(double(nsTickTime) / 1000000, 0, 'f', 6);
            (warning ? qWarning() : qDebug()).noquote() << message;
        }
        windowStartNs = lastCheckNs;
        checkedTicks = overdueTicks = moderateTicks = wholePeriodTicks = 0;
        worstOverdueNs = 0;
    };

    while (m_run == true)
    {
        /* Add nsTickTime to the finish time, to calculate the end timestamp of this loop */
        finish->tv_sec += (finish->tv_nsec + nsTickTime) / 1000000000L;
        finish->tv_nsec = (finish->tv_nsec + nsTickTime) % 1000000000L;

#if defined(Q_OS_MACOS) || defined(Q_OS_IOS)
        ret = clock_get_time(cclock, current);
#else
        ret = clock_gettime(CLOCK_MONOTONIC, current);
#endif
        if (ret == -1)
        {
            qWarning() << Q_FUNC_INFO << "Unable to get the current time:"
                       << strerror(errno);
            clockFailed = true;
            m_run = false;
            break;
        }

        lastCheckNs = timestampNs(*current);
        ++checkedTicks;
        const qint64 overdueNs = lastCheckNs - timestampNs(*finish);
        if (overdueNs > 0)
        {
            ++overdueTicks;
            worstOverdueNs = qMax(worstOverdueNs, overdueNs);
            if (overdueNs >= nsTickTime)
                ++wholePeriodTicks;
            else if (overdueNs * 4 >= nsTickTime)
                ++moderateTicks;
        }
        if (lastCheckNs - windowStartNs >= 5000000000LL)
            reportHealth();

        /* A pre-sleep deadline check does not identify the cause of a delay. */
        if (compareTime(finish, current) <= 0)
        {
            /* No need to sleep. Immediately process the next tick */
            dispatchTick(false);
            /* Now the finish time needs to be recalibrated */
#if defined(Q_OS_MACOS) || defined(Q_OS_IOS)
            clock_get_time(cclock, finish);
#else
            clock_gettime(CLOCK_MONOTONIC, finish);
#endif
            continue;
        }

        /* Do a rough sleep using the kernel to return control.
           We know that this will never be seconds as we are dealing
           with jumps of under a second every time. */
        sleepTime->tv_sec = finish->tv_sec - current->tv_sec;
        if (finish->tv_nsec < current->tv_nsec)
        {
            sleepTime->tv_nsec = finish->tv_nsec + 1000000000L - current->tv_nsec ;
            sleepTime->tv_sec--; /* Decrease a second. */
        }
        else
            sleepTime->tv_nsec = finish->tv_nsec - current->tv_nsec;

        //qDebug() << Q_FUNC_INFO << "Sleeping ns:" << sleepTime->tv_nsec;

        ret = nanosleep(sleepTime, remainingTime);
        while (ret == -1 && sleepTime->tv_nsec > 100)
        {
            sleepTime->tv_nsec = remainingTime->tv_nsec;
            ret = nanosleep(sleepTime, remainingTime);
        }

        /* Execute the next timer event */
        dispatchTick(true);
    }

    if (!clockFailed)
        reportHealth();

    free(finish);
    free(current);
    free(sleepTime);
    free(remainingTime);
}
