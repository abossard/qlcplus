/*
  Q Light Controller Plus
  show.h

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

#ifndef SHOW_H
#define SHOW_H

#include <QMutex>
#include <QList>
#include <QSet>
#include <atomic>
#include <limits>
#include <memory>
#include <optional>

#include "function.h"
#include "showcommandtrack.h"
#include "track.h"

class QXmlStreamReader;
class ShowRunner;

/** @addtogroup engine_functions Functions
 * @{
 */

/** A contiguous run of crossed VC state records, handed to the GUI executor.
 *  The runner holds everything crossed after it until it is acknowledged. */
struct ShowControlBatch
{
    quint64 traversal = 0; //!< stamped when the playhead crossed the records
    quint64 seq = 0;
    bool restore = false;
    QVector<ShowCommand> commands;
    QVector<quint32> engineStarted;
    QVector<quint32> engineStopped;
};

/** One native effect a receipt waits for */
struct ShowFunctionExpectation
{
    quint32 functionId = ShowCommand::InvalidId;
    bool expectLive = true;
    /** A Collection whose start starts functionId: judged once that start ran */
    quint32 causeFunctionId = ShowCommand::InvalidId;
};

struct ShowReceiptState;

class Show final : public Function
{
    Q_OBJECT
    Q_DISABLE_COPY(Show)

    /*********************************************************************
     * Initialization
     *********************************************************************/
public:
    Show(Doc* doc);
    virtual ~Show();

    /** @reimp */
    QIcon getIcon() const override;

    /** @reimp */
    quint32 totalDuration() override;

    /*********************************************************************
     * Copying
     *********************************************************************/
public:
    /** @reimp */
    Function* createCopy(Doc* doc, bool addToDoc = true) override;

    /** Copy the contents for this function from another function */
    bool copyFrom(const Function* function) override;

    /*********************************************************************
     * Time division
     *********************************************************************/
public:
    enum TimeDivision
    {
        Time = 0,
        BPM_4_4,
        BPM_3_4,
        BPM_2_4,
        /** Time-stored mode: item positions stay in milliseconds (like Time),
         *  while the editor paints the beat grid and POI markers read live
         *  from the VirtualDJ database.xml of the show's audio file. */
        VDJBeat,
        Invalid
    };
    Q_ENUM(TimeDivision)

    /** Divisions storing item positions in milliseconds (as opposed to
     *  beat units, where 1000 == 1 beat) */
    static bool isTimeBasedDivision(Show::TimeDivision type)
    {
        return type == Time || type == VDJBeat;
    }

    /** Set the show time division type (Time, BPM) */
    void setTimeDivision(Show::TimeDivision type, int BPM);

    Show::TimeDivision timeDivisionType() const;
    void setTimeDivisionType(Show::TimeDivision type);
    int beatsDivision() const;

    int timeDivisionBPM() const;
    void setTimeDivisionBPM(int BPM);

    static QString tempoToString(Show::TimeDivision type);
    static Show::TimeDivision stringToTempo(const QString& tempo);

private:
    TimeDivision m_timeDivisionType;
    int m_timeDivisionBPM;

    /*********************************************************************
     * External sync
     *********************************************************************/
public:
    /** Set the sync source for this show's runner. */
    void setSyncSource(int source);

    /** Get the current sync source. */
    int syncSource() const { return m_syncSource; }

    /** Store the latest externally-observed elapsed time in milliseconds. */
    void setExternalElapsedTime(quint32 ms);
    quint32 externalElapsedTime() const
    {
        return m_externalElapsedTime.load(std::memory_order_relaxed);
    }

    /** Request a local seek for an already-running autonomous Show. Crossed
     *  VC work of the current traversal is cancelled at once. Ignored, with
     *  nothing cancelled, while an external clock owns the position. */
    void requestSeek(quint32 ms);

    /** Transient runtime suppression for Audio functions while Perform owns
     *  this Show. Not persisted in workspace XML. */
    void setPerformAudioSuppressed(bool suppress);
    bool performAudioSuppressed() const
    {
        return m_performAudioSuppressed.load(std::memory_order_relaxed);
    }

private:
    int m_syncSource;  // stored as int to avoid header dependency on ShowRunner enum
    std::atomic<quint32> m_externalElapsedTime{0};
    std::atomic_bool m_performAudioSuppressed;
    static constexpr quint64 NoSeekRequested = std::numeric_limits<quint64>::max();
    std::atomic<quint64> m_requestedSeekTime{NoSeekRequested};

    /*********************************************************************
     * Tracks
     *********************************************************************/
public:
    /**
     * Add a track to this show. If the track is already a
     * member of the show, this call fails.
     *
     * @param id The track to add
     * @return true if successful, otherwise false
     */
    bool addTrack(Track *track, quint32 id = Track::invalidId());

    /**
     * Remove a track from this show. If the track is not a
     * member of the show, this call fails.
     *
     * @param id The track to remove
     * @return true if successful, otherwise false
     */
    bool removeTrack(quint32 id);

    /** Get a track by id */
    Track *track(quint32 id) const;

    /** Get a reference to a Track from the provided Scene ID */
    Track *getTrackFromSceneID(quint32 id) const;

    /** Get a reference to a Track from the provided ShowFunction ID */
    Track *getTrackFromShowFunctionID(quint32 id) const;

    /** Get the number of tracks in the Show */
    int getTracksCount() const;

    /** Move a track ID up or down */
    void moveTrack(Track *track, int direction);

    /** Get a list of available tracks */
    QList <Track*> tracks() const;

private:
    /** Create a new track ID */
    quint32 createTrackId();

protected:
    /** Map of the available tracks coupled by ID */
    QMap <quint32,Track*> m_tracks;

    /** Latest assigned track ID */
    quint32 m_latestTrackId;

    /*********************************************************************
     * Show Functions
     *********************************************************************/
public:
    /** Get a unique ID for the creation of a new ShowFunction */
    quint32 getLatestShowFunctionId();

    /** Get a reference to a ShowFunction from the provided uinique ID */
    ShowFunction *showFunction(quint32 id) const;

protected:
    /** Latest assigned unique ShowFunction ID */
    quint32 m_latestShowFunctionID;

    /*********************************************************************
     * Command track
     *********************************************************************/
public:
    /** Thread-safe copy of the authored command track */
    ShowCommandTrack commandTrack() const;

    /**
     * Validate and publish a command track. The candidate is rejected as a
     * whole, so a failed edit never replaces valid data.
     *
     * @param track the authored commands and playback extent. An extent below
     *              the last authored command is raised to it; nothing is ever
     *              appended past the last command.
     * @param alreadyApplied ids the caller executed live during the current
     *              traversal. They are remembered until the runtime starts
     *              another traversal, so a lagging cursor cannot echo them.
     *              While the Show is stopped they are history instead: the
     *              traversal begins when a start is accepted, and replays them.
     * @param error filled with the reason when the track is rejected
     */
    bool setCommandTrack(const ShowCommandTrack &track,
                         const QSet<quint32> &alreadyApplied = QSet<quint32>(),
                         QString *error = nullptr);

    /** Same, for ids executed live in appliedTraversal: under the track lock
     *  they stay live ids only while it is still the current traversal, and
     *  are history a later one replays otherwise */
    bool setCommandTrack(const ShowCommandTrack &track, const QSet<quint32> &alreadyApplied,
                         quint64 appliedTraversal, QString *error);

    /** Any thread: no start was accepted that has not ended yet. Queued,
     *  running, paused and stopping all count as playing. */
    bool commandPlaybackStopped() const;

    /** An editor edit: published only if commandPlaybackStopped() holds as it
     *  is stored, decided under the same lock that accepts a start, so either
     *  the edit is in before the start or the start wins and it is refused */
    bool setStoppedCommandTrack(const ShowCommandTrack &track, QString *error);

    /** Transient recording intent. It keeps a command-only or short Show
     *  playing and is never stored in the workspace. */
    void setCommandRecording(bool enabled);
    bool commandRecording() const;

    /** Authoritative elapsed milliseconds of this Show, never editor ruler
     *  beats. Safe from any thread: an externally synced Show reports the
     *  clock its host pushes, including while paused or idle, and an
     *  autonomous one reports the position its runtime last reached. */
    quint32 commandPosition() const;

    /** Any thread: batches and receipts of an older traversal are cancelled */
    quint64 commandTraversal() const;

    /** GUI thread: a VC executor applies this Show's batches while attached.
     *  With none, crossed VC records are dropped. */
    void attachControlExecutor(bool attach);

    /** GUI thread: the VC batches crossed since the last call, in order */
    QVector<ShowControlBatch> takeControlBatches();

    /** GUI thread: the batch is applied. A batch of an earlier traversal is
     *  ignored, the runner already dropped what was waiting behind it. */
    void acknowledgeControlBatch(quint64 traversal, quint64 seq);

    /** GUI thread: report functionReceiptReady(traversal, opId) once every
     *  expectation holds, judged by the timer thread after a tick's starts.
     *  A later traversal cancels it without a report. */
    void requestFunctionReceipt(quint64 traversal, quint64 opId,
                                const QVector<ShowFunctionExpectation> &expectations);

    /** Any thread: crossed work of the current traversal is still owed, a VC
     *  batch not acknowledged yet, effects queued behind it, or a catch-up
     *  Start/Stop settling. Pause completes once this is false. */
    bool commandWorkPending() const;

    /** Any thread: the crossed commands of the current traversal not handed
     *  to the executor or not run yet, in order: queued VC batches, what waits
     *  behind the held batch, and a catch-up Start/Stop still settling */
    QVector<ShowCommand> pendingCommandWork() const;

signals:
    /** Emitted from the timer thread when the owed crossed work of the
     *  runner ran or was dropped */
    void commandWorkDrained();

    /** Emitted when the authored command data changed */
    void commandTrackChanged();

    /** Emitted from the timer thread when a VC batch was published */
    void controlBatchesReady();

    /** Emitted from the timer thread when a requested receipt holds */
    void functionReceiptReady(quint64 traversal, quint64 opId);

    /** Emitted from the GUI or timer thread, outside every Show lock, when a
     *  seek or stop cancelled the crossed VC work */
    void commandTraversalCancelled();

private:
    friend class ShowRunner;

    /** Publication counter, so the runtime can skip unchanged snapshots */
    quint64 commandTrackRevision() const;

    /** Runtime snapshot: the published track plus the live ids to skip */
    ShowCommandTrack commandTrackSnapshot(QSet<quint32> *alreadyApplied) const;

    /** The runtime began another traversal (seek, loop or stop) after the
     *  one it last saw, so live no-echo suppression from that one is over.
     *  Returns the stamp the runner keeps. Consuming a requested seek keeps
     *  what was published since the request, and a retired runner changes
     *  nothing: the newer Play owns it */
    quint64 commandTraversalRestarted(quint64 traversal, bool requested);

    /** The runtime reports the authoritative position */
    void setCommandPosition(quint32 ms);

    /** With m_commandTrackMutex held: start a traversal, dropping the
     *  outbox and every pending receipt */
    void cancelControlWork();

    /** Stamp and queue a VC batch for the GUI, returning its seq, or 0 when
     *  a requested seek or stop already cancelled it, run was retired, or
     *  traversal is no longer the current one.
     *  owedAfter is the work left behind it, published in the same step */
    quint64 publishControlBatch(quint64 run, quint64 traversal, const QVector<ShowCommand> &commands,
                                const QVector<ShowCommand> &owedAfter,
                                const QVector<quint32> &engineStarted = QVector<quint32>(),
                                const QVector<quint32> &engineStopped = QVector<quint32>());

    /** Timer thread: the runner's owed work, besides its held batch seq.
     *  Nothing is owed once run's work is cancelled or traversal ended. */
    void setCommandWork(quint64 run, quint64 traversal, quint64 heldSeq, const QVector<ShowCommand> &owed);
    bool hasControlExecutor() const { return m_controlExecutors.load(std::memory_order_acquire) > 0; }
    bool controlBatchAcknowledged(quint64 seq) const;

    /** The run a runner belongs to, the count of Stops when it was created */
    quint64 commandRun() const { return m_commandStops.load(std::memory_order_acquire); }

    /** A Stop retired the runner of this run, even when a newer Play followed */
    bool runRetired(quint64 run) const;

    /** A requested seek or stop is ending the current traversal, or run was retired */
    bool controlWorkCancelled(quint64 run) const;

    /** run's work of traversal may claim its next operation: no Stop retired
     *  run, and no seek, stop or discard has ended traversal since */
    bool commandTraversalCurrent(quint64 run, quint64 traversal) const;

    /** A seek request cancelled *traversal, and a sync source change discarded
     *  it: nothing will restart run's traversal, so its owed work is dropped.
     *  Sets *traversal to the current one then. */
    bool commandTraversalDiscarded(quint64 run, quint64 *traversal) const;

    /** Timer thread: functionId reached what a Start (expectLive) or a Stop
     *  of it waits for, judged like a receipt expectation */
    bool functionSettled(quint32 functionId, bool expectLive) const;

    bool publishCommandTrack(const ShowCommandTrack &track, const QSet<quint32> &alreadyApplied,
                             std::optional<quint64> appliedTraversal, QString *error,
                             bool onlyStopped = false);

    /** Publish without target validation, for loading and copying. With
     *  onlyStopped, false and nothing stored unless commandPlaybackStopped(). */
    bool storeCommandTrack(const ShowCommandTrack &track,
                           const QSet<quint32> &alreadyApplied = QSet<quint32>(),
                           std::optional<quint64> appliedTraversal = std::nullopt,
                           bool onlyStopped = false);

    /** Empty when every command target is usable */
    QString commandTargetError(const ShowCommandTrack &track) const;

    mutable QMutex m_commandTrackMutex;
    ShowCommandTrack m_commandTrack;
    QSet<quint32> m_commandAppliedIds;
    std::atomic<quint64> m_commandTrackRevision{0};
    std::atomic_bool m_commandRecording{false};
    std::atomic<quint32> m_commandPosition{0};
    std::atomic<int> m_controlExecutors{0};
    /** Set by every Stop until preRun replaces the runner: a Play accepted
     *  meanwhile belongs to the next runner, so the retired one must not tick */
    std::atomic_bool m_runnerRetired{false};
    /** Counts every Stop, so preRun can tell one landed while it installed
     *  the runner. Changed with m_commandTrackMutex held */
    std::atomic<quint64> m_commandStops{0};

    /** Guarded by m_commandTrackMutex: starts accepted, the accepted starts
     *  the current run began with, and those whose run ended */
    quint64 m_startsAccepted = 0;
    quint64 m_startsRunning = 0;
    quint64 m_startsEnded = 0;

    /** Guarded by m_commandTrackMutex */
    quint64 m_commandTraversal = 0;
    /** Live ids of the traversal a pending requested seek ends: removed when
     *  it is consumed, kept when a sync source change discards it */
    QSet<quint32> m_requestedSeekIds;
    quint64 m_lastControlSeq = 0;
    quint64 m_ackedControlSeq = 0;
    QVector<ShowControlBatch> m_controlBatches;
    /** The runner's owed work, current only for m_commandWorkTraversal */
    quint64 m_commandWorkTraversal = 0;
    quint64 m_commandWorkHeldSeq = 0;
    QVector<ShowCommand> m_commandWork;

    /** Shared with the timer-thread observers of pending receipts, so a
     *  callback in flight outlives this Show */
    std::shared_ptr<ShowReceiptState> m_receipts;

    /*********************************************************************
     * Save & Load
     *********************************************************************/
public:
    /** Save function's contents to an XML document */
    bool saveXML(QXmlStreamWriter *doc) const override;

    /** Load function's contents from an XML document */
    bool loadXML(QXmlStreamReader &root) override;

    /** @reimp */
    void postLoad() override;

public:
    /** @reimp */
    bool contains(quint32 functionId) const override;

    /** @reimp */
    QList<quint32> components() const override;

    /*********************************************************************
     * Running
     *********************************************************************/
public:
    /** @reimp */
    void preRun(MasterTimer* timer) override;

    /** @reimp */
    void setPause(bool enable) override;

    /** @reimp */
    void write(MasterTimer* timer, QList<Universe*> universes) override;

    /** @reimp */
    void postRun(MasterTimer* timer, QList<Universe*> universes) override;

protected:
    /** @reimp */
    void startRequested() override;

    /** @reimp */
    void stopRequested() override;

protected slots:
    /** Called whenever one of this function's child functions stops */
    void slotChildStopped(quint32 fid);

signals:
    void externalElapsedTimeChanged(quint32 ms);
    void timeChanged(quint32);
    void showFinished();

protected:
    ShowRunner *m_runner;
    /** Timer thread: the pause state the runner last applied */
    bool m_runnerPaused = false;
    /** Number of currently running children */
    QSet <quint32> m_runningChildren;

    /*************************************************************************
     * Attributes
     *************************************************************************/
public:
    /** @reimp */
    int adjustAttribute(qreal fraction, int attributeId = 0) override;
};

/** @} */

#endif
