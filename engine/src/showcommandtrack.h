/*
  Q Light Controller Plus
  showcommandtrack.h

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

#ifndef SHOWCOMMANDTRACK_H
#define SHOWCOMMANDTRACK_H

#include <QtGlobal>
#include <QString>
#include <QVector>
#include <QList>
#include <QHash>
#include <QSet>
#include <QUuid>
#include <QMap>
#include <variant>

class QXmlStreamReader;
class QXmlStreamWriter;

/** @addtogroup engine_functions Functions
 * @{
 */

#define KXMLShowCommandTrack QStringLiteral("CommandTrack")

/**
 * Timecoded command recording for a Show: value types, an editable track and the
 * pure transition functions that turn authored data plus transport state into
 * ordered effects.
 *
 * QtCore only. This module knows nothing about Doc, Function, widgets or a timer,
 * and it never observes engine activity. The host applies the returned effects
 * through ordinary engine behaviour; a command that lands on a manually stopped
 * function simply does nothing.
 *
 * Traversal rule (one rule for the whole module): a command is history once
 * `time < state.consumedThrough`. advance() consumes up to and including the new
 * position, seek() consumes strictly before its destination. A command authored
 * at time T therefore executes exactly once per traversal, when the playhead
 * reaches T, and executes again after a seek/loop back before it. Events authored
 * live are additionally excluded by id, so a lagging cursor still executes the
 * unrelated commands sharing their time.
 */

enum class ShowCommandAction : quint8
{
    Start,       //!< trigger: start the target through this Show
    Stop,        //!< trigger: release this Show's playback of the target
    SetIntensity,     //!< persistent value: absolute normalized intensity
    SetButtonState,   //!< VC state: a Toggle button's desired On/Off
    SetSliderPosition, //!< VC state: a slider's absolute normalized position
    SetSliderColors,   //!< VC state: Click & Go colors payload + brightness position
    SetSliderReset,    //!< VC state: release monitor override
    SetXYPadPosition,  //!< VC state: XY pad absolute position payload
    SetAnimationFader, //!< VC state: Matrix/Animation fader normalized position
    SetXYPadFloor,    //!< VC state: metre X/Y/Z floor target
    SetXYPadRanges,
    SetXYPadPositionPreset,
    SetXYPadFunctionPreset,
    SetXYPadGroupPreset,
    SetAnimationColor,
    SetAnimationContent,
    SetSliderChannel
};

/** What a VC state record expects its control to be. None on function commands. */
enum class ShowControlRole : quint8
{
    None,
    ToggleButton,
    FlashButton,
    BlackoutButton,
    FreezeButton,
    FreezeHoldButton,
    LevelSlider,
    AdjustSlider,     //!< drives the Function attribute keyed by ShowCommand::attribute
    SubmasterSlider,
    GrandMasterSlider,
    XYPad,
    AnimationFader
};

/** Provenance of a control change. Only real user input may author commands. */
enum class ShowCommandOrigin : quint8
{
    Pointer,      //!< mouse/touch on a VC control
    Midi,         //!< accepted MIDI input
    Audio,        //!< audio mapping
    Programmatic, //!< generic setter/script
    Replay,       //!< playback feedback of this very track
    Keyboard,     //!< key sequence on a VC control: local user input, like the pointer
    Osc           //!< accepted OSC input
};

struct ShowCommandColors
{
    quint8 red = 0, green = 0, blue = 0;
    quint8 white = 0, amber = 0, ultraviolet = 0;

    static bool decode(const QString &payload, ShowCommandColors *value);
    QString encode() const;
};

struct ShowCommandPanTilt
{
    qreal pan = 0.0;
    qreal tilt = 0.0;

    static bool decode(const QString &payload, ShowCommandPanTilt *value);
    QString encode() const;
};

struct ShowCommandFloor
{
    qreal x = 0.0, y = 0.0, z = 0.0;

    static bool decode(const QString &payload, ShowCommandFloor *value);
    QString encode() const;
};

struct ShowCommandRanges
{
    ShowCommandPanTilt horizontal, vertical;
};

struct ShowCommandChoice
{
    int choice = -1;
    bool active = false;
    ShowCommandPanTilt point;
};

struct ShowCommandMatrixColor
{
    enum class Operation { Replace, Reset, Component };
    int index = -1;
    Operation operation = Operation::Replace;
    ShowCommandColors color;
    int component = -1;
    int value = 0;
    int choice = -1;
};

struct ShowCommandProperty
{
    enum class Type { List, Range, Float, String };
    Type type = Type::String;
    QString text;
    qreal number = 0;
};

struct ShowCommandContent
{
    QString algorithm;
    QString text;
    QMap<QString, ShowCommandProperty> properties;
    int choice = -1;
};

struct ShowCommandChannel
{
    int value = 0;
    QString binding;
};

/** A closed native argument, without effect handles or mutable binding IDs. */
struct ShowCommandPayload
{
    using Value = std::variant<std::monostate, ShowCommandRanges, ShowCommandChoice,
                               ShowCommandMatrixColor, ShowCommandContent, ShowCommandChannel,
                               ShowCommandColors, ShowCommandPanTilt, ShowCommandFloor>;
    Value value;
    static bool decode(ShowCommandAction action, const QString &text, ShowCommandPayload *out,
                       QString *reason = nullptr);
    QString encode(ShowCommandAction action) const;
    QString validate(ShowCommandAction action) const;
    bool operator==(const ShowCommandPayload &other) const;
};

/** One authored command. Plain value type: comparable, copyable, no identity. */
struct ShowCommand
{
    /** Matches Function::invalidId() without depending on the engine */
    static constexpr quint32 InvalidId = 0xFFFFFFFFU;

    /** UINT_MAX stays the engine's "unset time" sentinel, so authored times stop
     *  one below it and cursor arithmetic (time + 1) cannot wrap */
    static constexpr quint32 MaxTime = 0xFFFFFFFEU;

    quint32 id = InvalidId;                             //!< stable within a track
    quint32 time = 0;                                   //!< milliseconds from Show start
    quint32 functionId = InvalidId;                     //!< stable Function target
    ShowCommandAction action = ShowCommandAction::Start;
    qreal intensity = 0.0;                              //!< SetIntensity only, finite, [0, 1]

    /** VC state records only: a persistent control identity instead of a Function */
    QUuid controlId;
    ShowControlRole role = ShowControlRole::None;
    QString attribute;     //!< AdjustSlider key; legacy complex text is normalized on ingress
    bool on = false;       //!< SetButtonState only
    qreal position = 0.0;  //!< SetSliderPosition only, finite, [0, 1]
    quint32 pairId = InvalidId; //!< optional hold pair identity on SetButtonState edges
    ShowCommandPayload payload;
    ShowCommand canonicalized() const;
    QString nativeArgument() const;

    /** Its place among commands sharing a time, issued by the track that holds
     *  it and kept through every edit, so an event moved away and back is in
     *  its old place again. Placement, not part of the value: operator== ignores it. */
    quint32 order = 0;

    static ShowCommand start(quint32 id, quint32 time, quint32 functionId);
    static ShowCommand stop(quint32 id, quint32 time, quint32 functionId);
    static ShowCommand setIntensity(quint32 id, quint32 time, quint32 functionId, qreal intensity);
    static ShowCommand setButtonState(quint32 id, quint32 time, const QUuid &controlId, bool on);
    static ShowCommand setSliderPosition(quint32 id, quint32 time, const QUuid &controlId,
                                         ShowControlRole role, const QString &attribute,
                                         qreal position);
    static ShowCommand setSliderColors(quint32 id, quint32 time, const QUuid &controlId,
                                       ShowControlRole role, const QString &payload,
                                       qreal position);
    static ShowCommand setSliderReset(quint32 id, quint32 time, const QUuid &controlId,
                                      ShowControlRole role, const QString &attribute);
    static ShowCommand setXYPadPosition(quint32 id, quint32 time, const QUuid &controlId,
                                        const QString &payload);
    static ShowCommand setAnimationFader(quint32 id, quint32 time, const QUuid &controlId,
                                         qreal position);

    /** Empty when the command is valid, otherwise the reason */
    static QString validate(const ShowCommand &cmd);

    static QString actionToString(ShowCommandAction action);
    /** false when the name is not a known action */
    static bool actionFromString(const QString &name, ShowCommandAction *action);
    static bool isControlAction(ShowCommandAction action);
    static bool hasTypedPayload(ShowCommandAction action);

    static QString roleToString(ShowControlRole role);
    /** false when the name is not a known role */
    static bool roleFromString(const QString &name, ShowControlRole *role);

    /** Null unless the text is exactly a braced or unbraced UUID */
    static QUuid controlIdFromString(const QString &text);

    bool operator==(const ShowCommand &other) const;
    bool operator!=(const ShowCommand &other) const { return !(*this == other); }
};

/** Presentation/edit membership derived from chronological adjacency, not a
 *  playback command or a persisted gesture. Times are elapsed milliseconds. */
struct ShowCommandGroup
{
    QVector<quint32> eventIds;
    ShowCommandAction action = ShowCommandAction::Start;
    QUuid controlId;
    ShowControlRole role = ShowControlRole::None;
    QString attribute;
    quint32 startTime = 0;
    quint32 endTime = 0;
};

/** One VC control expectation of a track: every record naming the same control
 *  in the same role (and attribute) counts towards one reference */
struct ShowControlReference
{
    QUuid controlId;
    ShowControlRole role = ShowControlRole::None;
    QString attribute;
    int count = 0;
};

/** Whether a VC state record can reach its control with the current configuration */
enum class ShowControlStatus : quint8
{
    Ready,        //!< one enabled, compatible, bound control; not a promise of output
    Missing,      //!< no control carries the identity
    Ambiguous,    //!< several controls carry the identity
    Incompatible, //!< the control's type, mode or attribute role changed
    Disabled,
    Unbound       //!< the role needs a Function binding the control lacks
};

/** What the GUI reports about one control carrying a recorded identity */
struct ShowControlSnapshot
{
    ShowControlRole role = ShowControlRole::None;
    QString attribute; //!< AdjustSlider only: the attribute key it drives
    bool enabled = true;
    bool bound = false; //!< has the Function binding its role needs
};

/** The native coupling of one Ready control, built on the GUI */
struct ShowControlCoupling
{
    QUuid controlId;
    quint32 functionId = ShowCommand::InvalidId;
    quint32 soloGroup = ShowCommand::InvalidId; //!< enclosing Solo Frame, InvalidId outside one
    QVector<quint32> startsFunctions;           //!< what starting functionId starts (Collection members)
    /** (member, Solo Frame) for each started member a Solo Frame button controls */
    QVector<QPair<quint32, quint32>> memberSoloGroups;
};

/** How a selection is retimed: one delta for all, or a stretch of its span */
enum class ShowRetimeKind : quint8
{
    Move,         //!< every selected time + delta
    StretchEnd,   //!< latest selected time + delta, earliest pinned, scaled between
    StretchStart  //!< earliest selected time + delta, latest pinned, scaled between
};

/** Why a selection retime was refused as a whole */
enum class ShowRetimeRefusal : quint8
{
    None,
    UnknownEvent, //!< nothing selected, or an id the track does not hold
    NoSpan,       //!< a stretch of a selection whose events share one time
    NoTargetSpan, //!< a stretch onto or past the pinned edge
    OutOfRange,   //!< a time before 0 or past ShowCommand::MaxTime
    Reordered     //!< rounding would put the selected events in another sequence
};

/**
 * The authored commands of one Show plus its authored playback extent.
 *
 * Commands are kept ordered by (time, order), order being issued at insertion. Editing is explicit and
 * local: nothing is overwritten implicitly and a rejected edit leaves the track
 * untouched. The extent is authored data, never derived from the last command and
 * never a source of synthesized Stops - the host decides what a Show does with it.
 */
class ShowCommandTrack
{
public:
    /** Serialization versions. A track without VC state records keeps writing the
     *  legacy version, which cannot hold them. Any other version fails to load. */
    static constexpr int LegacyVersion = 1;
    static constexpr int Version = 9;

    bool isEmpty() const;
    int count() const;

    /** Ordered by (time, order) */
    const QVector<ShowCommand> &commands() const;
    /** Partition commands in their existing order. Only adjacent slider samples
     *  with the same control/role/attribute join; every other command is a barrier.
     *  Recomputed after edits, without changing storage, ordering or playback. */
    QVector<ShowCommandGroup> groups() const;

    /** -1 when no command carries that id */
    int indexOfId(quint32 id) const;
    bool contains(quint32 id) const;

    /** Authored playback extent in milliseconds, independent of the commands */
    quint32 extent() const;
    /** Rejects the unset-time sentinel and keeps the previous extent. An extent
     *  below lastCommandTime() is allowed: the host decides what that means. */
    bool setExtent(quint32 ms, QString *error = nullptr);

    /** Time of the last authored command, 0 when empty. Host input for choosing
     *  an extent: an extent equal to this ends the Show on a trailing Start. */
    quint32 lastCommandTime() const;

    /** Free id for the next authored command, InvalidId when exhausted. Above
     *  every id this track value has held, so a removed id is never reissued. */
    quint32 nextEventId() const;
    /** The order the next inserted command gets */
    quint32 nextOrder() const;
    /** Never issue an id or order below these again, e.g. ones an undo
     *  history still names */
    void reserve(quint32 nextEventId, quint32 nextOrder);

    /** A new command, after every command sharing its time. cmd.order is
     *  ignored. Fails on an invalid command or a duplicate id. */
    bool insert(const ShowCommand &cmd, QString *error = nullptr);
    /** Put back a command that held cmd.order, in that place among its equal
     *  times. Fails also when the order is held by another command. */
    bool restore(const ShowCommand &cmd, QString *error = nullptr);
    /** Replaces the command carrying cmd.id, keeping its order, so at a new
     *  time it takes the place its order gives it. cmd.order is ignored.
     *  Fails when the id is unknown or the value invalid. */
    bool replace(const ShowCommand &cmd, QString *error = nullptr);
    /** Moves one command in time, leaving every other field and command alone */
    bool retime(quint32 id, quint32 time, QString *error = nullptr);
    bool remove(quint32 id, QString *error = nullptr);

    /** Retimes the selected events into result, a copy of this track: only
     *  their times change, every value, id and Order stays and so does every
     *  unselected event, which they may cross. A stretch scales each offset
     *  from the pinned edge by newSpan / oldSpan, rounding the distance half
     *  away from the pin to a millisecond, so both edges land exactly. Refused whole, result left
     *  alone, when the selected (time, Order) sequence would change. */
    ShowRetimeRefusal retimeSelection(const QVector<quint32> &ids, ShowRetimeKind kind, qint64 deltaMs,
                                      ShowCommandTrack *result) const;

    /** Sorted, unique Function targets referenced by this track's function
     *  commands. VC state records reference controls, not Functions. */
    QList<quint32> referencedFunctionIds() const;
    /** Unique control expectations in the order they are first referenced */
    QVector<ShowControlReference> referencedControls() const;
    /** Retargets every command pointing at `from`. Returns the number changed. */
    int remapFunctionId(quint32 from, quint32 to);

    /** Transactional: on failure the track keeps its previous contents */
    bool loadXML(QXmlStreamReader &root, QString *error = nullptr);
    bool saveXML(QXmlStreamWriter *doc) const;

private:
    QVector<ShowCommand> m_commands;
    quint32 m_extent = 0;
    quint32 m_nextEventId = 0; //!< high water mark of inserted ids
    quint32 m_nextOrder = 0;   //!< high water mark of issued orders

    /** Insert at its (time, order) place, raising the high water marks */
    bool place(const ShowCommand &cmd, QString *error);
};

/** What the recorder is doing, derived from the state below */
enum class ShowRecordPhase : quint8
{
    Off,      //!< not recording
    Armed,    //!< recording requested, no Show bound yet
    Bound,    //!< bound to the resolved Show
    Suspended //!< bound, but another Show is currently resolved
};

/** One occurrence the host executed live, fixed when it was captured */
struct ShowLiveMark
{
    quint32 time = 0;
    /** The Function a live Stop stopped, InvalidId for any other action:
     *  its superseded clip end is retired once, whatever is edited later */
    quint32 stopFunctionId = ShowCommand::InvalidId;

    bool operator==(const ShowLiveMark &other) const
    {
        return time == other.time && stopFunctionId == other.stopFunctionId;
    }
};

/** Live no-echo marks: event id -> the occurrence the host executed live.
 *  A mark guards that occurrence only, never the id at another time. */
using ShowLiveMarks = QHash<quint32, ShowLiveMark>;

/** The live mark of a command as it was captured */
inline ShowLiveMark liveMarkOf(const ShowCommand &cmd)
{
    return { cmd.time, cmd.action == ShowCommandAction::Stop ? cmd.functionId : ShowCommand::InvalidId };
}

/**
 * Transport and recording state. Deliberately holds no running functions, owners,
 * clips or engine observations - only what the transitions need.
 */
struct ShowCommandState
{
    bool recording = false;
    bool playing = false;
    quint32 boundShowId = ShowCommand::InvalidId;    //!< sticky while recording
    quint32 resolvedShowId = ShowCommand::InvalidId; //!< Show currently resolved by the host
    quint32 position = 0;                            //!< authoritative transport milliseconds
    quint32 consumedThrough = 0;                     //!< commands before this are history

    /** Events authored live during this traversal, which the host already executed,
     *  at the time they were captured. Traversal metadata of the cursor, not a
     *  registry of anything running: a command is skipped only while it still sits
     *  at its mark's time, and a mark is dropped once the playhead consumes that
     *  time, wherever its event has moved since. */
    ShowLiveMarks consumedLiveEventIds;

    ShowRecordPhase phase() const;
    bool operator==(const ShowCommandState &other) const;
    bool operator!=(const ShowCommandState &other) const { return !(*this == other); }
};

/** A resolved user control change offered to the recorder */
struct ShowCommandInput
{
    ShowCommandOrigin origin = ShowCommandOrigin::Programmatic;
    ShowCommandAction action = ShowCommandAction::Start;
    quint32 functionId = ShowCommand::InvalidId;
    qreal intensity = 0.0; //!< SetIntensity only; a trigger carrying a value is rejected

    /** VC state records only, validated like the authored ShowCommand fields */
    QUuid controlId;
    ShowControlRole role = ShowControlRole::None;
    QString attribute;
    bool on = false;
    qreal position = 0.0;
    ShowCommandPayload payload;
};

/** Result of one transition: the next state plus what the host must do with it */
struct ShowCommandTransition
{
    ShowCommandState state;
    QVector<ShowCommand> effects;  //!< ordered commands for the host to apply
    QVector<ShowCommand> authored; //!< commands the host must commit to the track
    /** Why an offered input authored nothing, empty when there is nothing to explain.
     *  Input the recorder is not meant to author is ignored without a reason. */
    QString error;
};

/**
 * Pure transitions. Every function is a calculation: same arguments, same result,
 * no side effects, no hidden state.
 */
namespace ShowCommandFsm
{
    /** Arming binds the resolved Show at once; disarming drops the binding */
    ShowCommandState setRecording(const ShowCommandState &state, bool on);

    /** A Show resolved for the current source. While armed this binds the target;
     *  a later different Show suspends recording instead of retargeting it. */
    ShowCommandState setResolvedShow(const ShowCommandState &state, quint32 showId);

    /** Pausing freezes the position, not the recorder: live input keeps authoring
     *  at that frozen timestamp and Record intent survives */
    ShowCommandState setPlaying(const ShowCommandState &state, bool playing);

    /** Forward playback to positionMs. Effects are the commands due in
     *  [consumedThrough, positionMs], in authored order. */
    ShowCommandTransition advance(const ShowCommandTrack &track,
                                  const ShowCommandState &state, quint32 positionMs);

    /** Reposition to positionMs: a backward jump or loop, Play from a cursor,
     *  or an external clock's first sample. No effects: nothing before the
     *  destination is replayed or restored, and commands at exactly
     *  positionMs are due on the next advance(). Live marks of the previous
     *  pass are cleared; Record intent and binding survive. A forward jump
     *  is no seek: advance() plays the crossed interval. */
    ShowCommandState seek(const ShowCommandState &state, quint32 positionMs);

    /** Offer real user input to the recorder. Authors at most one command at the
     *  authoritative position and marks that occurrence, its id at its time, as
     *  consumed, so the freshly authored command cannot echo back through
     *  advance() during this traversal while every other due command still
     *  executes. The host has already executed the input live, so this returns
     *  no effects. */
    ShowCommandTransition userInput(const ShowCommandTrack &track,
                                    const ShowCommandState &state, const ShowCommandInput &input);

    /** Whether input of this origin is a person operating a control, the only
     *  input that may author */
    bool isUserOrigin(ShowCommandOrigin origin);

    /** Native Toggle button state as the control reports it */
    enum class ShowButtonNative : quint8 { Inactive, Active, Monitoring };
    /** The single native operation that reaches a recorded button state */
    enum class ShowButtonOp : quint8 { None, Start, Stop };

    ShowButtonOp buttonOp(ShowButtonNative native, bool desiredOn);

    /** Maps a recorded 0..1 slider position into the control's current range,
     *  rounding once. false, leaving value alone, for a nonfinite or out of
     *  range position, an empty/reversed range, or a range whose bounds or
     *  result an int cannot hold. */
    bool sliderTarget(qreal position, qreal low, qreal high, int *value);

    /** One validation rule for replay and the referenced-controls view: the
     *  suitability of the controls matching expected.controlId. reason is
     *  cleared when Ready. Level, Submaster and GrandMaster sliders need no
     *  Function binding. */
    ShowControlStatus resolveControl(const ShowCommand &expected,
                                     const QVector<ShowControlSnapshot> &matches, QString *reason);

    /** Whether replaying a state into `next` must wait for the native effect
     *  of the op just applied to `done`: the same control, the same valid
     *  function, the same Solo Frame, a function that starting `done` starts,
     *  or the Solo Frame of one */
    bool controlDependsOn(const ShowControlCoupling &next, const ShowControlCoupling &done);
}

/** @} */

#endif // SHOWCOMMANDTRACK_H
