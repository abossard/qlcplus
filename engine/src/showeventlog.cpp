/*
  Q Light Controller Plus
  showeventlog.cpp

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

#include <QElapsedTimer>
#include <QMetaObject>
#include <QMutex>
#include <QObject>
#include <atomic>
#include <vector>

#include "showeventlog.h"

namespace ShowEventLog
{
namespace
{
std::atomic<quint64> g_generation{0};
std::atomic<quint64> g_lastGeneration{0};
std::atomic<quint64> g_trace{0};
std::atomic<bool> g_wakePending{false};
std::atomic<quint64> g_failures{0};
std::atomic<quint64> g_dropped{0}; //!< of the current observation

struct Log
{
    QMutex mutex; //!< the ring
    /** The wake receiver and the failure summary. Held only for a pointer
     *  or a QString copy, so the timer thread may wait on it. Never taken
     *  before mutex. */
    QMutex wakeMutex;
    std::vector<Entry> ring; //!< Capacity slots once opened, released on close
    int head = 0;            //!< oldest entry
    int count = 0;
    quint64 seq = 0;
    quint64 evicted = 0;
    QObject *receiver = nullptr;      //!< wakeMutex
    quint64 wakesPosted = 0;          //!< wakeMutex
    QString reason;                   //!< wakeMutex
    View view = View::Events;         //!< wakeMutex
    Stats stats;
};

Log &log()
{
    static Log instance;
    return instance;
}

QElapsedTimer &clock()
{
    static QElapsedTimer timer = []() { QElapsedTimer t; t.start(); return t; }();
    return timer;
}

/** One payload-free call, however many entries follow */
void wake(Log &l)
{
    QMutexLocker locker(&l.wakeMutex);
    if (l.receiver == nullptr || g_wakePending.exchange(true))
        return;
    l.wakesPosted++;
    QMetaObject::invokeMethod(l.receiver, "drainShowEvents", Qt::QueuedConnection);
}

bool lock(Log &l, bool timerThread)
{
    if (!timerThread)
    {
        l.mutex.lock();
        return true;
    }
    return l.mutex.tryLock();
}
} // namespace

quint64 generation()
{
    return g_generation.load(std::memory_order_relaxed);
}

qint64 nowMs()
{
    return clock().elapsed();
}

quint64 newTrace()
{
    return generation() == 0 ? 0 : g_trace.fetch_add(1, std::memory_order_relaxed) + 1;
}

void append(const Entry &e, bool timerThread)
{
    Log &l = log();
    if (!lock(l, timerThread))
    {
        // the timer thread never waits for the ring: the diagnostic is lost,
        // counted, and the counter change wakes the reader like an entry
        if (e.generation != 0 && e.generation == generation())
        {
            g_dropped.fetch_add(1, std::memory_order_relaxed);
            wake(l);
        }
        return;
    }

    l.stats.offered++;
    if (e.generation == 0 || e.generation != g_generation.load(std::memory_order_relaxed) || l.ring.empty())
    {
        l.stats.stale++;
        l.mutex.unlock();
        return;
    }

    int slot = (l.head + l.count) % Capacity;
    if (l.count == Capacity)
    {
        slot = l.head;
        l.head = (l.head + 1) % Capacity;
        l.evicted++;
    }
    else
    {
        l.count++;
    }

    Entry &stored = l.ring[size_t(slot)];
    stored = e;
    stored.seq = ++l.seq;
    stored.arrivalMs = nowMs();
    stored.control.truncate(LabelLength);
    stored.action.truncate(LabelLength);
    stored.value.truncate(LabelLength);
    stored.before.truncate(ReasonLength);
    stored.after.truncate(ReasonLength);
    stored.reason.truncate(ReasonLength);
    wake(l);
    l.mutex.unlock();
}

quint64 open()
{
    Log &l = log();
    QMutexLocker locker(&l.mutex);
    l.ring.assign(size_t(Capacity), Entry());
    l.head = 0;
    l.count = 0;
    l.evicted = 0;
    g_dropped.store(0, std::memory_order_relaxed);
    const quint64 next = g_lastGeneration.fetch_add(1) + 1;
    g_generation.store(next, std::memory_order_relaxed);
    return next;
}

void close()
{
    Log &l = log();
    std::vector<Entry> released;
    {
        QMutexLocker locker(&l.mutex);
        g_generation.store(0, std::memory_order_relaxed);
        released.swap(l.ring);
        l.head = 0;
        l.count = 0;
        l.evicted = 0;
        g_dropped.store(0, std::memory_order_relaxed);
    }
}

Snapshot snapshot(quint64 afterSeq)
{
    Log &l = log();
    QMutexLocker locker(&l.mutex);
    Snapshot s;
    s.generation = g_generation.load(std::memory_order_relaxed);
    s.evicted = l.evicted;
    s.dropped = g_dropped.load(std::memory_order_relaxed);
    for (int i = 0; i < l.count; i++)
    {
        const Entry &e = l.ring[size_t((l.head + i) % Capacity)];
        if (e.seq > afterSeq)
            s.entries.append(e);
    }
    return s;
}

void failure(const QString &reason, View view)
{
    Log &l = log();
    {
        QMutexLocker locker(&l.wakeMutex);
        g_failures.fetch_add(1, std::memory_order_relaxed);
        l.reason = reason.left(ReasonLength);
        l.view = view;
    }
    wake(l);
}

Summary summary()
{
    Log &l = log();
    QMutexLocker locker(&l.wakeMutex);
    Summary s;
    s.count = g_failures.load(std::memory_order_relaxed);
    s.reason = l.reason;
    s.view = l.view;
    return s;
}

void resetSummary()
{
    Log &l = log();
    QMutexLocker locker(&l.wakeMutex);
    g_failures.store(0, std::memory_order_relaxed);
    l.reason.clear();
    l.view = View::Events;
}

void setWakeReceiver(QObject *receiver)
{
    Log &l = log();
    QMutexLocker locker(&l.wakeMutex);
    l.receiver = receiver;
    // a wake posted to a destroyed receiver was discarded with it
    if (receiver == nullptr)
        g_wakePending.store(false);
}

void wakeHandled()
{
    g_wakePending.store(false);
}

Stats stats()
{
    Log &l = log();
    QMutexLocker locker(&l.mutex);
    Stats s = l.stats;
    QMutexLocker wakeLocker(&l.wakeMutex);
    s.wakesPosted = l.wakesPosted;
    return s;
}

void lockRingForTest()
{
    log().mutex.lock();
}

void unlockRingForTest()
{
    log().mutex.unlock();
}

void resetStatsForTest()
{
    Log &l = log();
    QMutexLocker locker(&l.mutex);
    l.stats = Stats();
    QMutexLocker wakeLocker(&l.wakeMutex);
    l.wakesPosted = 0;
}
}
