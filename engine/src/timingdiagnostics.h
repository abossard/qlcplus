/*
  Q Light Controller Plus
  timingdiagnostics.h

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

#ifndef TIMINGDIAGNOSTICS_H
#define TIMINGDIAGNOSTICS_H

#include <QtGlobal>
#include <QString>
#include <QVector>
#include <functional>

/** @addtogroup engine Engine
 * @{
 */

/**
 * Opt-in, rate-bounded timing diagnostics for the MasterTimer playback path.
 *
 * Purpose: attribute where a tick's wall time went - callback duration vs the
 * scheduler period, the largest single function write(), and (for HUEScript) the
 * synchronous handoff split into queue wait vs worker-render time - without
 * changing scheduling, function ordering, audio freshness or DMX output.
 *
 * Disabled by default. Enable before launch with
 *
 *     QLCPLUS_TIMING_DIAG=1        (optional QLCPLUS_TIMING_DIAG_MS=<interval>)
 *
 * A restart is required. When disabled every entry point is a single relaxed
 * atomic load and does no timing, allocation or formatting; callers also guard
 * with enabled() so argument work is skipped too.
 *
 * Measurement contract (deliberately conservative):
 *  - Each duration uses one monotonic clock domain. Scheduler timestamps use
 *    the native clock; cross-thread HUE timestamps use nowNs().
 *    Metrics are aggregated as ns and only formatted to fractional ms (3 dp) at
 *    output, so separately rounded values never corrupt a residual.
 *  - Windows are bounded monotonic event windows: a window begins at its first
 *    event and ends at the first tick after the interval elapses (not fixed or
 *    wall-aligned). flushExpired() emits at most one summary per bucket per such
 *    window, driven each tick so a rare event is not withheld until the next
 *    one; flushAll() emits the final sub-interval window on shutdown. The
 *    reported window duration is the actual elapsed span.
 *  - A value of -1 means "not measured" and prints as "unknown", never 0.
 *  - Reports name measured boundaries only. The largest write is "maxWrite"
 *    (containment), never "responsible/dominant"; the callback-vs-period line is
 *    "callback consumed period", not proof of overrun or of any specific
 *    scheduler late warning (only scheduler-side correlation could establish
 *    that, which this does not attempt).
 */
namespace TimingDiag
{
    /** Cheap gate: a single relaxed atomic load after first resolution. */
    bool enabled();

    /** Monotonic wall clock in nanoseconds, readable from any thread. Used to
     *  timestamp the synchronous HUEScript handoff across the two threads. */
    qint64 nowNs();

    /**
     * Per-thread CPU time in nanoseconds for the CALLING thread, or -1 when the
     * platform clock is unsupported or the read fails. This is on-CPU time only:
     * time the thread was descheduled (waiting on a lock, runnable but off-core)
     * does NOT advance it, which is exactly what lets a worker's wall span be
     * split into compute vs not-running-on-this-thread. Must be sampled ON the
     * thread being measured; a caller blocked on a handoff would measure its own
     * idle CPU, not the worker's. Resolution is platform-dependent (microseconds
     * on some systems) and it still includes any native work the thread runs,
     * so "worker CPU" is not automatically "pure script compute". */
    qint64 threadCpuNs();

    /**
     * Current capture identity. Increments whenever a new capture begins (an
     * off->on enable transition, or an explicit reset). A sample tagged with a
     * capture id that no longer matches this value straddled a control change
     * and is discarded rather than mixed into the new capture. Relaxed atomic
     * load; safe from any thread. */
    qint64 captureId();

    /* ---- runtime production control (e.g. from the MCP timing tool) ---- */

    /** Enable or disable sampling at runtime without a restart. An off->on
     *  transition starts a NEW capture (bumps captureId, clears retained
     *  aggregates); an on->off transition stops sampling but RETAINS the last
     *  capture's aggregates so a snapshot after disable still reports them.
     *  Distinct from setEnabledForTest, which only forces the gate. */
    void setEnabled(bool on);

    /** Change the reporting/window interval in milliseconds at runtime. Ignored
     *  when intervalMs <= 0 (callers should reject that before calling). */
    void setIntervalMs(qint64 intervalMs);

    /** Begin a fresh capture: bump captureId, clear all retained aggregates and
     *  the discarded-sample counter, regardless of the enabled state. In-flight
     *  samples from the previous capture are rejected on completion. */
    void reset();

    /** One bucket's capture-cumulative aggregate, retained across periodic log
     *  flushes and across disable until the next reset/new-capture. The metric
     *  fields m0..m4 carry the worst sample's paired values; their meaning
     *  depends on kind (all ns, -1 == unknown):
     *   - "scheduler": m0=maxDispatchLate m1=period m2=priorCallback m3=priorFinishedLate
     *   - "period":    m0=maxCallback m1=period m2=maxWrite m3=functions m4=dispatchLate
     *   - "hue":       m0=maxWait m1=queue m2=worker m3=workerCpu
     *   - "rgb":       (count only) */
    struct BucketStat
    {
        QString kind;                 // scheduler | period | hue | rgb
        QString key;                  // bucket key, e.g. "hue:3"
        quint32 id = 0xFFFFFFFFu;     // worst sample's Function id (unknown sentinel)
        QString name;
        QString type;
        qint64 count = 0;             // cumulative events this capture
        qint64 windowMs = 0;          // elapsed span of this capture
        qint64 worstPrimaryNs = -1;   // the ordering metric of the worst sample
        qint64 m0 = -1, m1 = -1, m2 = -1, m3 = -1, m4 = -1;
        bool slept = false;           // scheduler: worst dispatch slept?
        qint64 unknownTicks = 0;      // scheduler: ticks with no clock sample
        qint64 clockReadFailures = 0; // scheduler: failed clock reads
        qint64 cpuUnknownCount = 0;   // hue: renders whose worker CPU was unknown
    };

    /** A non-consuming snapshot of the current capture. Reading it never clears
     *  or advances any state, so repeated reads (and reads after a periodic
     *  flush or after disable) return the same retained aggregates. */
    struct CaptureStat
    {
        bool enabled = false;
        qint64 captureId = 0;
        qint64 intervalMs = 0;
        qint64 elapsedMs = 0;         // now - capture start
        qint64 discardedSamples = 0;  // samples rejected for a stale capture id
        QVector<BucketStat> buckets;
    };

    /** Bounded, non-consuming capture statistics. Safe from any thread; does not
     *  require the gate to be enabled (so retained results survive disable). */
    CaptureStat snapshot();

    /**
     * C2-3: one ordinary RGBMatrix beat-step advance. Replaces the former
     * unbounded "Elapsed exceeded" line with a bounded summary (count + identity
     * per interval). Does not alter step advancement.
     */
    void rgbStepAdvance(quint32 functionId, const QString &name);

    /**
     * C2-2: a MasterTimer tick whose whole callback (measured through the end of
     * emit tickReady()) consumed at least the scheduler period. This is callback
     * pressure. dispatchLatenessNs is the actual dispatch start minus the
     * scheduled deadline. It can include preceding callback work, not just
     * wake delay. It is -1 when no scheduler sample is available.
     *
     * @param budgetNs   real scheduler period (1e9/frequency).
     * @param callbackNs whole-callback wall time, or -1.
     * @param functionsNs summed wall time of all timed function write()s, or -1;
     *                   callback minus this is the unattributed remainder
     *                   (preRun/postRun, tickReady slots, beats, universe
     *                   claim/release, DMX sources, signals - none individually
     *                   timed).
     * @param maxWriteId largest single write()'s Function id (Function::invalidId
     *                   when none was timed).
     * @param maxWriteType that Function's type; the phase is always write().
     * @param maxWriteName that Function's name.
     * @param maxWriteNs that write()'s duration, or -1. Containment, not cause.
     */
    void callbackConsumedPeriod(qint64 budgetNs, qint64 callbackNs, qint64 functionsNs,
                                qint64 dispatchLatenessNs, quint32 maxWriteId,
                                const QString &maxWriteType,
                                const QString &maxWriteName, qint64 maxWriteNs,
                                qint64 sampleCaptureId = -1);

    /** One Unix scheduler dispatch, including ticks with cheap callbacks.
     *  Report the worst dispatch's preceding callback and whether it slept.
     *  All supplied durations use the native scheduler clock; -1 is unknown.
     *  Unknown starts and clock-read failures are counted in the bounded report,
     *  so a known maximum does not conceal incomplete sampling.
     *  priorFinishedLateNs is max(0, previous callback end - current deadline).
     *  A late post-sleep start differs from a callback that already missed the
     *  deadline. Neither value isolates OS scheduling from diagnostic overhead. */
    void schedulerDispatch(qint64 budgetNs, qint64 dispatchLatenessNs, bool slept,
                           qint64 priorCallbackNs, qint64 priorFinishedLateNs,
                           int clockReadFailures, qint64 sampleCaptureId = -1);

    /**
     * C2-2: split of one synchronous HUEScript render handoff, from four
     * timestamps on one clock: Q before invoke, Jstart at worker entry, Jend at
     * worker exit, R after return.
     *
     * @param waitNs   total caller block, R - Q, or -1.
     * @param queueNs  Jstart - Q: post/queue/descheduling before the worker ran, or -1.
     * @param workerNs Jend - Jstart: worker-render wall time (script call plus
     *                 marshalling; NOT isolated script compute), or -1.
     * @param workerCpuNs on-CPU thread time of the worker across the same
     *                 Jstart..Jend span, sampled ON the worker thread, or -1 when
     *                 the thread CPU clock is unsupported/failed. worker - workerCpu
     *                 is time the render was not running on that core
     *                 (descheduling/contention); queue wait BEFORE Jstart is a
     *                 separate boundary, not worker descheduling.
     * @param sampleCaptureId capture identity sampled at Q; when it no longer
     *                 matches the live capture the sample straddled a control
     *                 change and is discarded. -1 means "do not check" (used by
     *                 unit tests that drive the recorder directly).
     *
     * The summary also derives returnTail = wait - queue - worker. queue and
     * worker are components of the same (max-wait) render, not independent maxima.
     */
    void hueHandoff(quint32 functionId, const QString &name,
                    qint64 waitNs, qint64 queueNs, qint64 workerNs,
                    qint64 workerCpuNs = -1, qint64 sampleCaptureId = -1);

    /** Emit and reset any bucket whose interval has elapsed. Call once per tick
     *  (only meaningful when enabled). Formatting happens outside the lock. */
    void flushExpired();

    /** Emit and reset EVERY non-empty bucket regardless of interval age. Call on
     *  shutdown so an isolated final incident is not dropped; the reported window
     *  duration is the actual (possibly short) span. */
    void flushAll();

    /* ---- test hooks: deterministic, not used by production code ---- */

    /** Force the gate on/off, bypassing the environment. */
    void setEnabledForTest(bool on);
    /** Inject a monotonic millisecond window clock; pass nullptr to restore real. */
    void setClockForTest(std::function<qint64()> nowMs);
    /** Set the reporting interval in milliseconds (must be > 0). */
    void setIntervalMsForTest(qint64 intervalMs);
    /** Drop all accumulated buckets and window state. */
    void resetForTest();
}

/** @} */

#endif // TIMINGDIAGNOSTICS_H
