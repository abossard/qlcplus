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

#include "function.h"
#include "showcommandtrack.h"
#include "track.h"

class QXmlStreamReader;
class ShowRunner;

/** @addtogroup engine_functions Functions
 * @{
 */

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

    /** Request a local seek for an already-running autonomous Show. */
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
     * @param error filled with the reason when the track is rejected
     */
    bool setCommandTrack(const ShowCommandTrack &track,
                         const QSet<quint32> &alreadyApplied = QSet<quint32>(),
                         QString *error = nullptr);

    /** Transient recording intent. It keeps a command-only or short Show
     *  playing and is never stored in the workspace. */
    void setCommandRecording(bool enabled);
    bool commandRecording() const;

    /** Authoritative elapsed milliseconds of this Show, never editor ruler
     *  beats. Safe from any thread: an externally synced Show reports the
     *  clock its host pushes, including while paused or idle, and an
     *  autonomous one reports the position its runtime last reached. */
    quint32 commandPosition() const;

signals:
    /** Emitted when the authored command data changed */
    void commandTrackChanged();

private:
    friend class ShowRunner;

    /** Publication counter, so the runtime can skip unchanged snapshots */
    quint64 commandTrackRevision() const;

    /** Runtime snapshot: the published track plus the live ids to skip */
    ShowCommandTrack commandTrackSnapshot(QSet<quint32> *alreadyApplied) const;

    /** The runtime began another traversal (seek, loop or stop), so live
     *  no-echo suppression from the previous one is over */
    void commandTraversalRestarted();

    /** The runtime reports the authoritative position */
    void setCommandPosition(quint32 ms);

    /** Publish without target validation, for loading and copying */
    void storeCommandTrack(const ShowCommandTrack &track,
                           const QSet<quint32> &alreadyApplied = QSet<quint32>());

    /** Empty when every command target is usable */
    QString commandTargetError(const ShowCommandTrack &track) const;

    mutable QMutex m_commandTrackMutex;
    ShowCommandTrack m_commandTrack;
    QSet<quint32> m_commandAppliedIds;
    std::atomic<quint64> m_commandTrackRevision{0};
    std::atomic_bool m_commandRecording{false};
    std::atomic<quint32> m_commandPosition{0};

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

protected slots:
    /** Called whenever one of this function's child functions stops */
    void slotChildStopped(quint32 fid);

signals:
    void externalElapsedTimeChanged(quint32 ms);
    void timeChanged(quint32);
    void showFinished();

protected:
    ShowRunner *m_runner;
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
