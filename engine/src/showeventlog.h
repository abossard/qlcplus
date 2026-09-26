/*
  Q Light Controller Plus
  showeventlog.h

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

#ifndef SHOWEVENTLOG_H
#define SHOWEVENTLOG_H

#include <QtGlobal>
#include <QString>
#include <QUuid>
#include <QVector>

class QObject;

/** @addtogroup engine Engine
 * @{
 */

/**
 * Memory-only record of recording/playback-flow decisions and outcomes, kept
 * only while the shared debug panel is visible.
 *
 * One process-wide ring of at most Capacity entries, like TimingDiag. The
 * panel opens an observation generation; closing it clears the ring. While
 * closed, generation() is 0 and a producer returns before building anything:
 *
 *     const quint64 g = ShowEventLog::generation();
 *     if (g != 0) { ShowEventLog::Entry e; e.generation = g; ...; ShowEventLog::append(e); }
 *
 * A producer that finishes later keeps the generation it started under; the
 * ring refuses it once that generation closed. Appending never waits for the
 * GUI: at most one payload-free queued call reaches the wake receiver, which
 * reads a bounded snapshot on the GUI thread. The timer thread only try-locks
 * the ring and counts a dropped diagnostic when it cannot.
 *
 * Apart from that, a failed-operation summary (count and latest reason) is
 * kept whether the panel is open or not. It holds no event history.
 */
namespace ShowEventLog
{
    static constexpr int Capacity = 2000;
    static constexpr quint32 NoId = 0xFFFFFFFFU;

    enum class Phase : quint8
    {
        Input,     //!< mapped input a route did not pass to recording or its control
        Decision,  //!< what recording did with an accepted control request
        Commit,    //!< an authored change offered to the Show
        Transport, //!< REC, binding and playback state requests
        Execute    //!< native execution of accepted input (live) or recorded commands (Replay)
    };

    enum class Outcome : quint8
    {
        Received,
        Recorded,    //!< accepted by the recorder
        Ignored,     //!< intentionally not recorded (REC off, not user input, unchanged)
        Unsupported, //!< a control or mode recording does not support
        Rejected,    //!< validation or a gate refused it, nothing changed
        Published,   //!< the Show stored the change
        Failed,      //!< an operation that should have happened did not
        Queued,      //!< accepted, waiting behind earlier work
        Requested,   //!< a native operation was requested; not proof of output
        Applied,     //!< a synchronous native result is known
        Skipped,     //!< nothing executed, with a reason
        Cancelled    //!< dropped by a newer traversal, reset or reconfiguration
    };

    /** Which debug view a failure concerns */
    enum class View : quint8 { Events, References };

    static constexpr int LabelLength = 80;
    static constexpr int ReasonLength = 160; //!< also before/after, which name target, role and order

    /** Values only; texts are truncated when appended */
    struct Entry
    {
        quint64 seq = 0;        //!< arrival order, never reused in the process
        quint64 generation = 0; //!< the observation the producer started under
        qint64 arrivalMs = 0;   //!< monotonic, independent of Show time
        qint64 showTimeMs = -1; //!< -1 when unknown; paused/rewound times repeat or decrease
        Phase phase = Phase::Input;
        Outcome outcome = Outcome::Received;
        int origin = -1; //!< a ShowCommandOrigin value, -1 when none
        quint32 showId = NoId;
        quint32 widgetId = NoId;
        quint32 functionId = NoId;
        quint32 commandId = NoId;
        QUuid controlId;
        quint64 take = 0;
        quint64 traversal = 0;
        quint64 trace = 0; //!< shared by the entries one input or batch caused
        QString control;
        QString action;
        QString value;
        QString before;
        QString after;
        QString reason;
    };

    /** 0 while closed, else the current observation. Relaxed atomic load. */
    quint64 generation();

    /** The monotonic clock of Entry::arrivalMs */
    qint64 nowMs();

    /** A trace token for the entries one input causes, 0 while closed */
    quint64 newTrace();

    /** Append if e.generation is still the current observation. From the
     *  MasterTimer thread pass timerThread: it never waits on the lock. */
    void append(const Entry &e, bool timerThread = false);

    /** GUI: start a fresh, empty observation and return its generation */
    quint64 open();
    /** GUI: end the observation and release its entries */
    void close();

    struct Snapshot
    {
        quint64 generation = 0;
        QVector<Entry> entries; //!< seq > afterSeq, oldest first, at most Capacity
        quint64 evicted = 0;    //!< overflowed out of the ring this observation
        quint64 dropped = 0;    //!< timer-thread diagnostics lost to contention
    };
    /** GUI: the current observation's entries after afterSeq */
    Snapshot snapshot(quint64 afterSeq);

    /** A failed operation, counted whether or not the panel is open. Any
     *  thread: it waits only on a lock held for a pointer or a QString copy,
     *  never on the ring. */
    void failure(const QString &reason, View view);

    struct Summary
    {
        quint64 count = 0;
        QString reason;
        View view = View::Events;
    };
    Summary summary();
    /** Workspace replacement */
    void resetSummary();

    /** GUI: the object whose drainShowEvents() slot the single queued wake
     *  calls, nullptr to detach */
    void setWakeReceiver(QObject *receiver);
    /** GUI: called first by the drain, so the next append may wake again */
    void wakeHandled();

    /** Measurements for tests */
    struct Stats
    {
        quint64 offered = 0;   //!< append() calls, current or stale
        quint64 stale = 0;     //!< refused: closed or older generation
        quint64 wakesPosted = 0;
    };
    Stats stats();
    void resetStatsForTest();
    /** Hold the ring lock, as a GUI snapshot does, to test contention */
    void lockRingForTest();
    void unlockRingForTest();
}

/** @} */

#endif // SHOWEVENTLOG_H
