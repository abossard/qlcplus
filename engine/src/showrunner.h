/*
  Q Light Controller
  showrunner.h

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

#ifndef SHOWRUNNER_H
#define SHOWRUNNER_H

#include <QObject>
#include <QMutex>
#include <QMap>
#include <QSet>

#include <atomic>
#include <limits>

#include <function.h>

class ShowFunction;
class Function;
class Track;
class Show;
class Doc;

/** @addtogroup engine_functions Functions
 * @{
 */

class ShowRunner final : public QObject
{
    Q_OBJECT

public:
    /** Time source for show progression. */
    enum SyncSource
    {
        Autonomous, //! ShowRunner increments elapsed time on each tick (default)
        External    //! Elapsed time is provided externally (e.g. from VDJ position)
    };

    ShowRunner(const Doc *doc, quint32 showID, quint32 startTime = 0);
    ~ShowRunner();

    /** Start the runner */
    void start();

    /** If running, pauses the runner and all the current running functions. */
    void setPause(bool enable);

    /** Stop the runner */
    void stop();

    void write(MasterTimer *timer);

    /** Set the time source for show progression. */
    void setSyncSource(SyncSource source);

    /** Get the current sync source. */
    SyncSource syncSource() const { return m_syncSource; }

    /** Provide the current elapsed time in milliseconds (thread-safe).
     *  Only used when syncSource is External. */
    void setExternalElapsedTime(quint32 ms);

    /** Queue a local seek for application on the timer thread. */
    void requestSeek(quint32 ms);

private:
    const Doc *m_doc;

    /** The reference of the show to play */
    Show* m_show;

    /** Time source mode */
    SyncSource m_syncSource;

    /** Externally-provided elapsed time (thread-safe via atomic) */
    std::atomic<quint32> m_externalElapsedTime{0};

    static constexpr quint64 NoSeekRequested = std::numeric_limits<quint64>::max();
    std::atomic<quint64> m_requestedSeekTime{NoSeekRequested};

    /** The list of time-based Functions the Show needs to play */
    QList <ShowFunction *> m_timeFunctions;

    /** Index of the item in m_timeFunctions to be considered for playback */
    int m_currentTimeFunctionIndex;

    /** Elapsed time since runner start. Used also to move the cursor in the track view */
    quint32 m_elapsedTime;

    /** The list of beat-based Functions the Show needs to play */
    QList <ShowFunction *> m_beatFunctions;

    /** Index of the item in m_beatFunctions to be considered for playback */
    int m_currentBeatFunctionIndex;

    /** Elapsed beats since runner start */
    quint32 m_elapsedBeats;

    /** Accumulated milliseconds used to synthesize the beat clock from the
     *  Show's own BPM when no global beat source is active (so a beat-based
     *  Show plays at the song BPM instead of freezing). */
    double m_internalBeatClockMs;

    /** Flag used to sinchronize playback to beats */
    bool beatSynced;

    /** m_elapsedTime at the moment beatSynced became true, and the ms
     *  equivalent of m_elapsedBeats at that same moment (i.e. the resumed
     *  beat position, if any). Used to derive a smoothly advancing, but
     *  beat-zeroed, cursor position for a BPM based Show (see write()) */
    quint32 m_syncElapsedTime;
    quint32 m_syncBeatsTime;

    /** Total time (in ms) the runner has to run, computed from m_timeFunctions */
    quint32 m_totalRunTime;

    /** Total time (in beats, expressed as ms, i.e. 1000 per beat) the
     *  runner has to run, computed from m_beatFunctions */
    quint32 m_totalRunBeats;

    /** List of the currently running Functions and their stop time */
    QList < QPair<Function *, quint32> > m_runningQueue;

    QSet<Function *> m_seekRestartFunctions;

    void seekTo(quint32 newTime);

    void syncPerformAudioSuppression();
    bool isAudioFunction(const Function *function) const;
    void stopRunningAudioFunctions();
    bool isFunctionScheduled(Function *function, quint32 stopTime) const;
    void markRunningAudioFunctionsDeferred(const QList<ShowFunction *> &functions,
                                           QSet<quint32> &deferred,
                                           quint32 elapsed);
    void startDeferredAudioFunctions(const QList<ShowFunction *> &functions,
                                     QSet<quint32> &deferred,
                                     quint32 elapsed);
    void applyTrackIntensity(ShowFunction *sf, Function *f);

private:
    FunctionParent functionParent() const;

signals:
    void timeChanged(quint32 time);
    void showFinished();

    /************************************************************************
     * Intensity
     ************************************************************************/
public:
    /**
     * Adjust the intensity of show track
     */
    void adjustIntensity(qreal fraction, const Track *track);

private:
    QMap<quint32, qreal> m_intensityMap;
    bool m_performAudioSuppressed = false;
    QSet<quint32> m_deferredTimeAudioFunctionIds;
    QSet<quint32> m_deferredBeatAudioFunctionIds;

};

/** @} */

#endif
