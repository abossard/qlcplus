/*
  Q Light Controller Plus
  showcommandrecorder.h

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

#ifndef SHOWCOMMANDRECORDER_H
#define SHOWCOMMANDRECORDER_H

#include <QObject>
#include <QVariantList>
#include <QVector>

#include "showcommandtrack.h"

class Show;
class Doc;

/**
 * Recording controller for the Show command track.
 *
 * Narrow adapter between real user input, the authoritative Show clock and the
 * pure ShowCommandFsm. It owns the recording state and a value copy of the
 * bound Show's track; every decision comes from ShowCommandFsm, every side
 * effect is a single publication to the Show.
 *
 * Unidirectional: VC controls submit accepted user input, transport hosts push
 * clock/resolution events, the UI reads state. Nothing reads back from engine
 * output and nothing mirrors running functions.
 *
 * The VC controls reach the recorder through instance(), the same way they
 * reach Tardis. The app creates exactly one.
 */
class ShowCommandRecorder : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool recording READ isRecording WRITE setRecordingProperty NOTIFY stateChanged)
    Q_PROPERTY(int phase READ phaseValue NOTIFY stateChanged)
    Q_PROPERTY(QString phaseName READ phaseName NOTIFY stateChanged)
    Q_PROPERTY(QString targetName READ targetName NOTIFY stateChanged)
    Q_PROPERTY(int position READ position NOTIFY positionChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QVariantList commands READ commands NOTIFY commandsChanged)
    Q_PROPERTY(int extent READ extent NOTIFY commandsChanged)

public:
    explicit ShowCommandRecorder(Doc *doc, QObject *parent = nullptr);
    ~ShowCommandRecorder();

    /** The app-owned instance, or nullptr when no UI is running */
    static ShowCommandRecorder *instance();

    /*********************************************************************
     * Transport and binding
     *********************************************************************/
public:
    bool isRecording() const;

    /** Property setter: same as setRecording(), result visible through lastError */
    void setRecordingProperty(bool on) { setRecording(on); }

    /** Arm/disarm. Arming without a resolved Show waits; the first resolved
     *  Show binds and stays bound through pause, seek and loop. Binding also
     *  sets the Show's transient recording intent, so a command-only Show
     *  keeps playing while it is being recorded. */
    bool setRecording(bool on);

    /** The Show currently resolved by the host (Show Manager selection or the
     *  Perform FSM's active show). A different Show suspends recording. */
    bool setResolvedShow(quint32 showId);

    /** Playing state, fed by the existing transport events (Perform FSM, Show
     *  Manager playback state). The recorder never reads the runtime's own
     *  running flag from the UI thread, and authoring does not depend on it:
     *  a bound recorder authors at the frozen position while paused. */
    void setPlaying(bool playing);

    /** True while accepted user input is authored: recording and bound */
    bool isAuthoring() const;

    /** Identity of the current take. A control that captures an input for
     *  later delivery stores this with it, so a gesture can never be written
     *  into the Show that happens to be bound when it finally arrives. */
    quint64 takeId() const;

    /** The Show this take belongs to, InvalidId when nothing is bound */
    quint32 boundShowId() const;

    int phaseValue() const;
    QString phaseName() const;
    QString targetName() const;

    /** The bound Show's own authoritative position. There is no second
     *  transport here: a paused Show simply keeps reporting its frozen time. */
    int position() const;
    QString lastError() const;

    /*********************************************************************
     * Input seam
     *********************************************************************/
public:
    /** Offer one resolved user command. The control has already applied it
     *  live; this only authors and publishes. Returns true when a command was
     *  stored. Programmatic, audio and replay origins never author.
     *
     *  Always invoked on the UI thread: a control whose native execution is
     *  deferred publishes its captured intent as a queued value signal rather
     *  than calling in from the timer thread. */
    bool submitUserInput(const ShowCommandInput &input);

    /** A supported compound gesture: ordered commands from one user action,
     *  authored at the same position in the given order. */
    bool submitUserInput(const QVector<ShowCommandInput> &inputs);

public slots:
    /** A control whose native execution is deferred publishing one resolved
     *  command, queued from the engine thread with the Show time it was
     *  captured at. */
    void slotResolvedUserCommand(int action, quint32 functionId, qreal intensity,
                                 quint32 timeMs, int origin, quint64 takeId);

public:
    /** A control or mode this slice cannot record faithfully. Live control is
     *  unaffected; the user sees why nothing was stored. */
    void reportUnsupported(const QString &what);


    /*********************************************************************
     * Authored data
     *********************************************************************/
public:
    /** The tracked Show's authored commands */
    const ShowCommandTrack &track() const;

    /** Editable view for the Show command editor */
    QVariantList commands() const;

    /** Functions a command may target, for the editor's target chooser */
    QVariantList availableTargets() const;
    int extent() const;

    Q_INVOKABLE bool setExtent(int ms);
    Q_INVOKABLE bool setCommandAction(quint32 id, const QString &action);
    Q_INVOKABLE bool setCommandTarget(quint32 id, quint32 functionId);
    Q_INVOKABLE bool retimeCommand(quint32 id, int ms);
    Q_INVOKABLE bool setCommandIntensity(quint32 id, qreal value);
    Q_INVOKABLE bool removeCommand(quint32 id);

signals:
    /** The current take is about to be closed. A control holding an accepted
     *  input that the engine has not executed yet settles it now, into this
     *  take, before the binding changes. */
    void takeAboutToFinalize(quint64 takeId);

    void stateChanged();
    void positionChanged();
    void lastErrorChanged();
    void commandsChanged();

private slots:
    /** Someone else published a track for the tracked Show */
    void slotCommandTrackChanged();

    /** The bound Show's own clock moved. A backward move ends the segment:
     *  accepted input settles into it, its end becomes a floor for the Show's
     *  duration, the take identity changes and the echo shield is reset. */
    void slotShowClock();

private:
    /** The Show whose track is authored and displayed: the bound one while
     *  recording, otherwise the resolved one. */
    quint32 trackedShowId() const;
    Show *trackedShow() const;

    /** Reload the track value from the tracked Show */
    void reloadTrack();

    /** The authored timestamp is the Show time the input happened at. Read
     *  from the host's own snapshot, never derived from a second playhead or
     *  from the tick that later executes the effect. */
    void syncPosition(Show *show);

    /** Commit an accepted transition: store the authored commands, extend the
     *  authored lifetime and publish the track together with the traversal
     *  metadata that keeps live input from echoing back. */
    /** Author at an explicit Show time, for input whose execution was deferred */
    bool authorAt(const QVector<ShowCommandInput> &inputs, quint32 positionMs);

    bool commit(const ShowCommandTransition &transition, Show *show);

    /** The single place that hands a candidate track to the Show */
    bool publishTrack(const ShowCommandTrack &candidate,
                      const QSet<quint32> &alreadyApplied, Show *show);

    /** Editor publication: no live ids, errors are the user's to see */
    bool publishEdit(const ShowCommandTrack &candidate);
    bool editFailed(const QString &error);

    /** Close the take on the bound Show: settle what is still pending and give
     *  the Show the duration that was actually captured. False when the Show
     *  rejected it, in which case nothing about the take changes. */
    bool finalizeTake();

    /** The duration the Show must keep: what was authored before this take,
     *  plus the end of every segment already captured. */
    quint32 extentFloor() const;

    void setError(const QString &error);

    static ShowCommandRecorder *s_instance;

    Doc *m_doc;
    ShowCommandState m_state;
    ShowCommandTrack m_track;
    quint32 m_trackedShowId = ShowCommand::InvalidId;
    QString m_lastError;
    bool m_publishing = false;
    quint64 m_takeId = 0;
    quint32 m_clockPosition = 0;
    quint32 m_authoredExtentFloor = 0;
};

#endif // SHOWCOMMANDRECORDER_H
