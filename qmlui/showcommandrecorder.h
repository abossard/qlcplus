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
#include <QPointer>
#include <QVariantList>
#include <QHash>
#include <QSet>
#include <QVector>
#include <QPointF>
#include <climits>
#include <functional>

#include "showcommandtrack.h"
#include "showcontrolaction.h"
#include "showeventlog.h"
#include "scenevalue.h"
#include "show.h"

class Doc;
class PerformFsm;
class VCButton;
class VCSlider;
class VCWidget;
class VirtualConsole;

/** What a control's native operation acts on: its replay snapshot plus the
 *  destination it writes, the bound Function or the Level channels */
struct ShowControlConfiguration
{
    ShowControlSnapshot snapshot;
    quint32 functionId = ShowCommand::InvalidId;
    QList<SceneValue> levelChannels;
    int sliderClickAndGoType = -1;
    QVariantMap nativeBinding;
    QPointer<Function> functionLifetime;
    QVector<QPointer<QObject>> nativeLifetimes;
    qreal sliderLow = 0.0;
    qreal sliderHigh = 255.0;

    bool operator==(const ShowControlConfiguration &other) const
    {
        return snapshot.role == other.snapshot.role && snapshot.attribute == other.snapshot.attribute &&
               snapshot.enabled == other.snapshot.enabled && snapshot.bound == other.snapshot.bound &&
               functionId == other.functionId && levelChannels == other.levelChannels &&
               sliderClickAndGoType == other.sliderClickAndGoType &&
               nativeBinding == other.nativeBinding && functionLifetime == other.functionLifetime &&
               nativeLifetimes == other.nativeLifetimes;
    }
    bool operator!=(const ShowControlConfiguration &other) const { return !(*this == other); }
};

/** One event an editor edit changed: what it was and what it became, each
 *  with its equal-time order. Absent before means nothing carried the id
 *  (not an edit here), absent after a deletion. */
struct ShowCommandEditEntry
{
    quint32 id = ShowCommand::InvalidId;
    bool hasBefore = false;
    ShowCommand before;
    bool hasAfter = false;
    ShowCommand after;
};

/** One undo step of the Recordings editor: an ID-addressed delta of one Show */
struct ShowCommandEdit
{
    quint32 showId = ShowCommand::InvalidId;
    int serial = 0;
    QVector<ShowCommandEditEntry> entries;
};
Q_DECLARE_METATYPE(ShowCommandEdit)

/** Diagnostics: the facts one input or operation began with while the debug
 *  panel observed it, kept with its later outcomes so they never report the
 *  Show, take or caption current at completion. Empty (generation 0) when
 *  the panel was closed. */
struct ShowTraceCause
{
    quint64 generation = 0;
    quint64 trace = 0;
    quint32 showId = ShowCommand::InvalidId;
    quint64 take = 0;
    int origin = -1;
    quint32 widgetId = ShowCommand::InvalidId;
    QUuid controlId;
    QString control;
};

/** One accepted user control request. Immutable once the recorder stamped it:
 *  a deferred request executes exactly what was accepted, or not at all. */
struct ShowControlRequest
{
    QPointer<VCWidget> control;
    ShowControlRole role = ShowControlRole::None;
    bool buttonState = false;  //!< true for SetButtonState, false for SetSliderPosition
    bool sliderColors = false; //!< true for SetSliderColors
    bool sliderChannel = false;
    bool sliderReset = false;  //!< true for SetSliderReset
    bool xyPadPosition = false; //!< true for SetXYPadPosition
    bool xyPadFloor = false;
    bool animationFader = false; //!< true for SetAnimationFader
    bool on = false;            //!< ToggleButton: the desired state, normalized at acceptance
    int value = 0;              //!< sliders: the accepted value
    QString attribute;          //!< SetSliderColors payload
    QPointF xyPosition;         //!< SetXYPadPosition payload
    ShowCommandFloor floorPosition;
    ShowCommandInput nativeInput;
    bool updateFeedback = true; //!< sliders
    ShowCommandOrigin origin = ShowCommandOrigin::Pointer;
    quint32 acceptedTimeMs = 0;
    quint64 epoch = 0;
    ShowControlConfiguration accepted; //!< the control's configuration at acceptance
    ShowTraceCause cause; //!< diagnostics at acceptance
    /** The Show the capture authored it on, InvalidId when not authored:
     *  only an authored request is a timeline operation */
    quint32 authoredShowId = ShowCommand::InvalidId;
};

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
    Q_PROPERTY(QString targetName READ targetName NOTIFY targetNameChanged)
    Q_PROPERTY(int position READ position NOTIFY positionChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QVariantList commands READ commands NOTIFY commandsChanged)
    Q_PROPERTY(QVariantList groups READ groups NOTIFY commandsChanged)
    Q_PROPERTY(int extent READ extent NOTIFY commandsChanged)
    Q_PROPERTY(QString editBlockedReason READ editBlockedReason NOTIFY editGateChanged)
    Q_PROPERTY(int lastEditSerial READ lastEditSerial NOTIFY commandsChanged)
    Q_PROPERTY(bool editSessionActive READ editSessionActive NOTIFY editSessionChanged)
    Q_PROPERTY(QString editSessionEndReason READ editSessionEndReason NOTIFY editSessionChanged)
    Q_PROPERTY(QVariantList referencedControls READ referencedControls NOTIFY referencesChanged)
    Q_PROPERTY(int clipboardCount READ clipboardCount NOTIFY clipboardChanged)

public:
    explicit ShowCommandRecorder(Doc *doc, QObject *parent = nullptr);
    ~ShowCommandRecorder();

    /** The app-owned instance, or nullptr when no UI is running */
    static ShowCommandRecorder *instance();

    /** The controls that recorded VC states replay into */
    void setVirtualConsole(VirtualConsole *vc);

    /** Replay batches the executor is still working through */
    int pendingControlRuns() const { return m_controlRuns.count(); }

    /** Accepted user input on a Toggle button or a slider. Executes at once,
     *  unless it depends on crossed replay work still owed or on an earlier
     *  queued request; then it waits in order. Stamped here with its
     *  acceptance time, request epoch and the control's configuration: a
     *  request whose control changed it meanwhile is cancelled and reported.
     *  While authoring, its desired state is recorded first, once. */
    void requestUserControl(ShowControlRequest request);

    /** Save boundary: publish the accepted input no Show has taken yet, and
     *  end those Shows and the take's Show at their content. Waits for no queued request; recording and
     *  playback go on. False, keeping every candidate, when a Show rejects it. */
    bool checkpoint(bool disarming = false);

    /** Drop the accepted input no Show has taken */
    void discardUnpublished();

    /** Workspace replacement: disarm without publishing anything, drop the
     *  accepted input no Show has taken and forget the resolved Show and
     *  cursor. Needs nothing to validate. */
    void discardRecording();

    /** Requests waiting behind replay work, oldest first */
    const QVector<ShowControlRequest> &pendingUserRequests() const { return m_userRequests; }

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
     *  Perform FSM's active show). While a take is bound, a different Show is
     *  an automatic handover: the take checkpoints and Record rearms on the new
     *  Show, or waits armed when none is resolved. A take that cannot be saved
     *  disarms, keeps its accepted input and returns false. */
    bool setResolvedShow(quint32 showId);

    /** Follow the Show an engaged Perform resolves, the automatic source of
     *  handovers. Connect before any Show Manager follower, so a bound take
     *  moves before the Show Manager's selection lock is asked. */
    void followPerform(PerformFsm *fsm);

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

    /** A stopped or paused host moved its cursor on a Show: input accepted on
     *  it is stamped there until that Show's clock moves. Seeks nothing. */
    void setHostCursor(quint32 showId, quint32 ms);
    QString lastError() const;

    /*********************************************************************
     * Input seam
     *********************************************************************/
public:
    /** Offer one resolved user command at the current position. Returns true
     *  when a command was stored. Programmatic, audio and replay origins never
     *  author. UI thread. */
    bool submitUserInput(const ShowCommandInput &input);

    /** A supported compound gesture: ordered commands from one user action,
     *  authored at the same position in the given order. */
    bool submitUserInput(const QVector<ShowCommandInput> &inputs);

    /** User input on a control or mode that cannot be recorded. Live control
     *  is unaffected; the user sees why nothing was stored. */
    void reportUnsupported(const QString &what, ShowCommandOrigin origin, VCWidget *control = nullptr);

    /** Diagnostics only: mapped input a route did not hand to recording or
     *  to its control. Returns at once while the debug panel is closed. */
    static void traceInput(VCWidget *control, ShowCommandOrigin origin, ShowEventLog::Outcome outcome,
                           const char *reason, int value = INT_MIN);

    /** Diagnostics only: a direct code request (WebAccess, native undo)
     *  reached an output-producing native control operation. Nothing while
     *  the panel is closed or inside a TracedCall. */
    static void traceDirectRequest(VCWidget *control, int value);

    /** Diagnostics: native control operations called within its scope belong
     *  to an input the caller traces itself (user, Audio), so they add no
     *  direct code request. GUI thread. */
    class TracedCall
    {
    public:
        TracedCall();
        ~TracedCall();
    private:
        bool m_previous;
    };


    /*********************************************************************
     * Authored data
     *********************************************************************/
public:
    /** The tracked Show's authored commands */
    const ShowCommandTrack &track() const;

    /** Rows of the Recordings editor, in track order: time, control
     *  identity, caption and current binding, action and value */
    QVariantList commands() const;
    QVariantList groups() const;
    /** The listed SetSliderPosition events of the tracked Show as three parallel
     *  lists {ids, times, positions}, in stored (time, Order) order. Other and
     *  unknown ids are skipped; all lists are empty for another Show. A read-only
     *  snapshot for drawing: no signal, nothing kept. */
    Q_INVOKABLE QVariantMap sliderSamples(quint32 showId, const QVariantList &eventIds) const;
    int extent() const;

    /** Why the Show's recordings cannot be edited now, empty when they can.
     *  Stopped, playing, paused and recording Shows all take edits: an edit
     *  is published at once, and the runner processes what it meets next. */
    bool editAllowed(quint32 showId, QString *reason = nullptr) const;
    /** editAllowed() for the tracked Show */
    QString editBlockedReason() const;

    /** Editor edits. Each names the Show its row belongs to and is refused
     *  when that is no longer the tracked Show, when editing is not allowed,
     *  or when any result is invalid; then nothing changes. A change is one
     *  undo step and marks the project modified; a no-op is neither. */
    Q_INVOKABLE bool retimeCommand(quint32 showId, quint32 id, qreal ms);
    /** Intensity of SetIntensity, position of SetSliderPosition, 0..1 */
    Q_INVOKABLE bool setCommandValue(quint32 showId, quint32 id, qreal value);
    Q_INVOKABLE bool setCommandState(quint32 showId, quint32 id, bool on);
    /** The editor's cell text: field time (seconds), value (percent) or state (On/Off) */
    Q_INVOKABLE bool editCommandText(quint32 showId, quint32 id, const QString &field, const QString &text);
    Q_INVOKABLE bool removeCommands(quint32 showId, const QVariantList &ids);
    /** Move the events by one shared delta: direction times stepMs rounded once */
    Q_INVOKABLE bool moveCommands(quint32 showId, const QVariantList &ids, int direction, qreal stepMs);
    /** Put the earliest event on the nearest beat anchorMs + k * beatMs, the
     *  others by the same delta */
    Q_INVOKABLE bool snapCommands(quint32 showId, const QVariantList &ids, qreal beatMs, qreal anchorMs);
    /** Legacy function commands only */
    Q_INVOKABLE bool setCommandAction(quint32 showId, quint32 id, const QString &action);
    Q_INVOKABLE bool setCommandTarget(quint32 showId, quint32 id, quint32 functionId);

    /** One edit session at a time, for a drag, a stretch or a cell draft:
     *  begin freezes the Show, the exact ids and each event as it is now,
     *  with its Order. Recording, regrouping and edits of other events go on
     *  meanwhile. Commit applies to exactly those ids only while each still
     *  holds what was frozen; otherwise it is refused with a reason and
     *  nothing changes. Commit and cancel end the session. Starting another
     *  ends the previous one. */
    Q_INVOKABLE bool beginEditSession(quint32 showId, const QVariantList &ids);
    /** kind: 0 move, 1 stretch the end, 2 stretch the start (ShowRetimeKind).
     *  {times: {id: ms}} of the session's events, or {reason}, publishing nothing */
    Q_INVOKABLE QVariantMap previewRetime(int kind, qreal deltaMs) const;
    Q_INVOKABLE bool commitRetime(int kind, qreal deltaMs);
    /** editCommandText() of the session's single event. Text that is no valid
     *  value is refused and keeps the session, so the draft can be corrected. */
    Q_INVOKABLE bool commitCellText(const QString &field, const QString &text);
    Q_INVOKABLE QVariantMap typedDraft(quint32 showId, quint32 id) const;
    Q_INVOKABLE QVariantMap typedEditorInfo(quint32 id, const QString &algorithm) const;
    Q_INVOKABLE QVariantMap previewTypedEdit(const QVariantMap &drafts) const;
    Q_INVOKABLE bool commitTypedEdit(const QVariantMap &drafts);
    /** Ends the session, committing nothing; a reason (a deliberate selection
     *  change, a deleted target) is shown as lastError */
    Q_INVOKABLE void cancelEditSession(const QString &reason = QString());
    bool editSessionActive() const { return m_session.show != nullptr; }
    /** Why the last session ended without its own commit or cancel, empty otherwise */
    QString editSessionEndReason() const { return m_sessionEndReason; }

    /** Recording clipboard: value copies of the exact events, in track order.
     *  Read only, no edit gate; the clipboard changes only when every id is
     *  found. Kept for the workspace, emptied when it is replaced. */
    Q_INVOKABLE bool copyCommands(quint32 showId, const QVariantList &ids);
    int clipboardCount() const { return m_clipboard.count(); }
    /** New events of the copies, the earliest at atMs, the others at their
     *  copied offsets and ties, with fresh ids and orders: one undo step. The
     *  new ids in track order, none (nothing changed) when refused. */
    Q_INVOKABLE QVariantList pasteCommands(quint32 showId, qreal atMs);

    /** While the editor shows the rows, their controls' and functions' own
     *  change notifications refresh them (commandsChanged) */
    Q_INVOKABLE void setRowsObserved(bool observed);

    /** Referenced controls view: one row per VC identity the tracked Show's
     *  records name, problems first, each expected role with its count and
     *  its suitability through the replay resolver. Configuration only: Ready
     *  says nothing about running functions, hardware or output. */
    QVariantList referencedControls() const;

    /** While the debug panel is open, the controls and functions the track
     *  names refresh referencedControls (referencesChanged). Independent of
     *  the editor's setRowsObserved. */
    void setReferencesObserved(bool observed);

    /** The serial of the last edit step, which Tardis can undo by it */
    int lastEditSerial() const { return m_editSerial; }

    /** Tardis: undo or redo one editor step on its own Show. Refused, leaving
     *  every event alone, when editing is not allowed or when an event it
     *  names is no longer what the step left; the reason is lastError. */
    bool applyHistoryEdit(const QVariant &edit, bool undo);

signals:
    void stateChanged();
    /** The tracked Show changed or was renamed */
    void targetNameChanged();
    void positionChanged();
    void lastErrorChanged();
    void commandsChanged();
    void editGateChanged();
    void referencesChanged();
    void clipboardChanged();
    void editSessionChanged();

private slots:
    /** Someone else published a track for the tracked Show */
    void slotCommandTrackChanged();

    /** Replay executor: a Show published crossed VC batches */
    void slotControlBatchesReady();
    void slotFunctionReceiptReady(quint64 traversal, quint64 opId);
    void slotRecordedWriteRetired(quint64 generation, int outcome, quint32 functionId, int effect,
                                  const QString &reason);
    void slotObservedControlDestroyed(QObject *object);
    void slotCommandTraversalCancelled();
    /** A Show stopped or changed its time source: its timeline facts end */
    void slotTimelineReset();
    void slotFunctionAdded(quint32 id);
    /** A Show's playback began or ended: the edit gate follows it */
    void slotShowPlayback();
    /** A Show stopped: release replay-owned holds before timeline facts reset */
    void slotShowStopped(quint32 sourceId);
    /** Follow the controls and functions the current rows show */
    void observeRowSources();

    /** The bound Show's own clock moved. A backward move ends the segment:
     *  accepted input settles into it, its end becomes a floor for the Show's
     *  duration, the take identity changes and the echo shield is reset. */
    void slotShowClock();

private:
    /** The Show whose track is authored and displayed: the bound one while
     *  recording, otherwise the resolved one. */
    quint32 trackedShowId() const;
    Show *trackedShow() const;
    /** The Show time input on this Show is accepted at now: the host cursor
     *  moved on its stopped or paused autonomous clock, else its position */
    quint32 acceptancePosition(const Show *show) const;

    /** Reload the track value from the tracked Show */
    void reloadTrack();

    /** The authored timestamp is the Show time the input happened at. Read
     *  from the host's own snapshot, never derived from a second playhead or
     *  from the tick that later executes the effect. */
    void syncPosition(Show *show);

    /** Author at an explicit Show time: the acceptance time of the input */
    bool authorAt(const QVector<ShowCommandInput> &inputs, quint32 positionMs);

    /** Commit an accepted transition: keep the authored commands and publish
     *  them with the traversal metadata that keeps live input from echoing back */
    bool commit(const ShowCommandTransition &transition, Show *show);

    /** The single place that hands a candidate track to the Show, with the
     *  ids executed live in appliedTraversal. Fills error when it rejects it;
     *  reporting it is the caller's. */
    bool publishTrack(const ShowCommandTrack &candidate, const QSet<quint32> &alreadyApplied,
                      quint64 appliedTraversal, Show *show, QString *error);

    /** The Show an editor edit of showId may change, nullptr (reported) if none */
    Show *editableShow(quint32 showId);
    /** Editor publication of the candidate: the delta of the affected ids
     *  becomes one undo step. No live ids, errors are the user's to see. */
    bool publishEdit(Show *show, ShowCommandTrack candidate, const QVector<quint32> &affected);
    /** After a live edit, align replay-owned hold effects with the authored
     *  state at the current playhead so moved/deleted holds release at once. */
    void reconcileReplayOwnedHoldsAtCursor(Show *show);
    /** Replace one command, for the value and state edits */
    bool replaceCommand(quint32 showId, quint32 id, const std::function<QString(ShowCommand &)> &change);
    /** Move the events by delta ms, keeping their order among themselves */
    bool shiftCommands(Show *show, const QVector<quint32> &ids, qint64 delta);
    bool editFailed(const QString &error);

    /** Hand a Show the accepted input it has not taken yet, if any, ending at
     *  its last command. A change it publishes is unsaved work. The Show keeps running while recording, never a saved tail. */
    bool publishUnpublished(Show *show, bool disarming, QString *error);
    /** Save-only projection: checkpoint while REC stays on closes open holds
     *  in the saved snapshot without mutating the live track. */
    ShowCommandTrack projectedCheckpointTrack(Show *show, bool *projected) const;
    void armCheckpointSaveProjection();

    /** checkpoint() without reporting: error names the last rejection */
    bool publishAccepted(bool disarming, QString *error);

    void setError(const QString &error);

    /** Diagnostics while the debug panel is open. A request traces only in
     *  the observation it was accepted in; the rest in the current one. */
    ShowEventLog::Entry traceEntry(quint64 generation, ShowEventLog::Phase phase,
                                   ShowEventLog::Outcome outcome, const QString &reason) const;
    /** The facts of an input or operation beginning now, while observed */
    ShowTraceCause newCause(ShowCommandOrigin origin, VCWidget *control) const;
    ShowTraceCause newCause() const;
    /** An entry of that cause, empty generation when it is not the current observation */
    ShowEventLog::Entry causeEntry(const ShowTraceCause &cause, ShowEventLog::Phase phase,
                                   ShowEventLog::Outcome outcome, const QString &reason) const;
    void traceRequest(const ShowControlRequest &request, ShowEventLog::Phase phase,
                      ShowEventLog::Outcome outcome, const QString &reason) const;
    /** The recorder's decision on the input being authored now */
    void traceDecision(ShowEventLog::Outcome outcome, const QString &reason, const ShowCommandInput *input) const;
    void traceCommand(const ShowTraceCause &cause, ShowEventLog::Phase phase, ShowEventLog::Outcome outcome,
                      const ShowCommand *before, const ShowCommand *after, quint32 showId, const QString &reason,
                      const QString &action = QString()) const;
    void traceExtent(const ShowTraceCause &cause, quint32 showId, quint32 before, quint32 after,
                     const QString &reason) const;
    /** What a successful publication changed: one entry per changed command and the extent */
    void traceTrackDelta(const ShowTraceCause &cause, const ShowCommandTrack &before, const ShowCommandTrack &after,
                         quint32 showId, const QString &reason) const;
    /** The cause of the input or operation being handled now */
    ShowTraceCause currentCause() const;
    /** checkpoint() as one observed operation of its own */
    bool publishCheckpoint(bool disarming, QString *error);
    /** The explicit operation (checkpoint, handover) being handled now */
    const ShowTraceCause *m_operation = nullptr;
    void traceTransport(ShowEventLog::Outcome outcome, const QString &action, const QString &value,
                        const QString &reason = QString()) const;
    /** A failed operation: the Problems summary, and lastError */
    void reportFailure(const QString &error, ShowEventLog::View view = ShowEventLog::View::Events);
    /** The request being captured, for the entries it causes */
    const ShowControlRequest *m_tracedRequest = nullptr;

    /** Take a new recording state: a new take on the Show it tracks */
    void enterRecordingState(const ShowCommandState &next);

    /** One Show's batch while the executor works through it */
    struct ControlRun
    {
        QPointer<Show> show;
        ShowControlBatch batch;
        int next = 0;
        quint64 waitingOp = 0;
        /** Slider writes (widget id, generation) the run waits to see
         *  retired, then the native effects they must have taken: the starts
         *  and stops those writes made, or a start one may have queued before
         *  anything waited */
        QVector<QPair<quint32, quint64>> waitingWrites;
        QVector<ShowFunctionExpectation> barrier;
        /** Starts and stops the runner made before this batch, waited on only
         *  by an op that depends on them */
        QVector<QPair<ShowControlCoupling, ShowCommandFsm::ShowButtonOp>> engineDone;
        /** Applied ops whose native effect nothing has waited for yet */
        QVector<QPair<ShowControlCoupling, ShowCommandFsm::ShowButtonOp>> unsettled;
        /** A rollback batch already applied its delta */
        bool rolledBack = false;
        quint32 claimedCommand = ShowCommand::InvalidId;
        QPointer<VCWidget> claimedControl;
        ShowControlConfiguration claimedConfiguration;
    };

    /** One control around one timeline operation (a replayed op, or an
     *  authored user request): its native state just before and after. A
     *  button is 1 while Active, a slider its raw value. */
    struct TimelineFact
    {
        quint32 showId;
        quint32 time; //!< the command time, or the capture's acceptance time
        QPointer<VCWidget> control;
        int before;
        int after;
        bool xyState = false;
        QPointF xyBefore;
        QPointF xyAfter;
        ShowControlAction::State nativeBefore, nativeAfter;
        bool conditionalOnStart = false;
        bool superseded = false;
        FunctionParent owner = FunctionParent::master();
        bool ownerRecorded = false;
        bool ownerBefore = false;
    };
    /** Every Show's timeline facts in the order they happened. Dropped after
     *  a rollback's time, and when the Show stops or switches its source. */
    QVector<TimelineFact> m_timeline;
    /** A control's affected closure, read before an operation: the control,
     *  the buttons of the Solo Frames its start reaches, or a Submaster's
     *  frame sliders. after is what a native start leaves a sibling in. */
    QVector<TimelineFact> closureOf(VCWidget *control, const FunctionParent &owner) const;
    /** Journal a closure once its operation applied: the control is read
     *  again, a button sibling takes the Solo effect only if started */
    void journal(QVector<TimelineFact> closure, quint32 showId, quint32 time, bool started,
                 const FunctionParent &owner, const ShowControlAction::Receipt &receipt);
    /** Return what this Show's timeline changed after the run's rollback time
     *  to its state before the first of those changes, as one delta */
    void rollBackTimeline(ControlRun &run);
    /** A replayed slider write the run waits for, then a start it may cause */
    void awaitRecordedWrite(ControlRun &run, VCWidget *control, quint64 generation);

    /** Replay executor: every Show hands its batches here, selected or not */
    void attachShow(Show *show);
    /** Re-resolved by identity at every step, nullptr unless Ready */
    VCWidget *resolveControl(const ShowCommand &cmd) const;
    ShowControlConfiguration configurationOf(VCWidget *widget, const ShowCommandInput *input = nullptr) const;
    /** What the control's native operation acts on now, for display */
    QString bindingText(VCWidget *widget) const;
    /** control may be nullptr for a start that no control made */
    ShowControlCoupling couplingOf(VCWidget *control, quint32 functionId) const;
    /** Apply until an op must wait for a receipt. True once the run is over:
     *  acknowledged, or abandoned to a later traversal. */
    bool continueControlRun(ControlRun &run);

    /** While authoring, record an accepted request's desired state at its
     *  acceptance time, before it executes or waits */
    void captureUserRequest(ShowControlRequest &request);
    /** Execute the queued requests nothing owed or earlier holds back */
    void drainUserRequests();
    /** The request at index depends on crossed work still owed or on an earlier request */
    bool userRequestWaits(int index) const;
    /** A workspace reset or a Show deletion ends every accepted request */
    void dropUserRequests();
    void slotFunctionRemoved(quint32 id);

    static ShowCommandRecorder *s_instance;

    Doc *m_doc;
    VirtualConsole *m_vc = nullptr;
    QVector<ControlRun> m_controlRuns;
    QVector<ShowControlRequest> m_userRequests;
    quint64 m_requestEpoch = 0;
    bool m_drainingRequests = false;
    quint64 m_lastReceiptOp = 0;
    QVector<QPointer<Show>> m_attachedShows;
    ShowCommandState m_state;
    ShowCommandTrack m_track;
    quint32 m_trackedShowId = ShowCommand::InvalidId;
    QString m_lastError;
    bool m_publishing = false;
    quint64 m_takeId = 0;
    quint32 m_clockPosition = 0;
    quint32 m_hostCursorShowId = ShowCommand::InvalidId;
    quint32 m_hostCursor = 0;
    /** Accepted input a Show rejected, kept for the next publication */
    struct Unpublished
    {
        quint32 showId;
        ShowCommand command;
        quint64 traversal; //!< the traversal it ran live in
        ShowTraceCause cause; //!< diagnostics of the input it came from
    };
    QVector<Unpublished> m_unpublished;
    /** The Show's track with the accepted input it has not taken yet */
    ShowCommandTrack withUnpublished(const Show *show) const;
    QVector<ShowCommand> m_clipboard;
    /** Function targets of copied commands deleted since, or already absent
     *  at, the Copy: the Doc may give their ids to other Functions */
    QSet<quint32> m_clipboardDeletedTargets;
    /** Per Show ID, the event ids and equal-time orders issued in this
     *  workspace so far: a Show restored from XML (native undo of its
     *  deletion) keeps them unissued, since the editor's undo history may
     *  still name them */
    QHash<quint32, QPair<quint32, quint32>> m_eventIdFloors;
    /** Replay-owned FreezeHold controls currently active per show. */
    QHash<quint32, QSet<QUuid>> m_replayFreezeHolds;
    /** Replay-owned Flash button controls currently active per show. */
    QHash<quint32, QSet<QUuid>> m_replayFlashButtons;
    /** Replay-owned slider flash controls currently active per show. */
    QHash<quint32, QSet<QUuid>> m_replaySliderFlashes;
    /** User-owned FreezeHold controls currently active. */
    QSet<quint32> m_userFreezeHolds;
    /** User-owned Flash button holds currently active. */
    QSet<quint32> m_userFlashHolds;
    /** User-owned slider flash holds currently active. */
    QSet<quint32> m_userSliderFlashHolds;
    /** User-owned global latch intent that stays authoritative until next recorded change. */
    QHash<quint32, bool> m_userFreezeIntent;
    QHash<quint32, bool> m_userBlackoutIntent;
    void rememberEventIds(const Show *show);
    int m_editSerial = 0;

    /** The open edit session: its Show and its events as they were frozen */
    struct EditSession
    {
        QPointer<Show> show;
        QVector<quint32> ids;
        QVector<ShowCommand> basis;
        QHash<quint32, QVariantMap> floorMetadata;
        QHash<quint32, ShowControlConfiguration> nativeMetadata;
    };
    EditSession m_session;
    QString m_sessionEndReason;
    /** Ends the session: no reason for its own commit or cancel; a reason,
     *  shown as lastError, when something else ended it */
    void endEditSession(const QString &reason = QString());
    /** Why the session cannot be committed, empty while it can */
    QString sessionConflict() const;
    QString retimeRefusalText(ShowRetimeRefusal refusal, ShowRetimeKind kind) const;
    bool m_rowsObserved = false;
    bool m_referencesObserved = false;
    bool m_rowsRefreshPending = false;
    QVector<QMetaObject::Connection> m_rowSources;
};

#endif // SHOWCOMMANDRECORDER_H
