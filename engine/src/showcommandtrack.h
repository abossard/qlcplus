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
#include <QSet>

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
    SetIntensity //!< persistent value: absolute normalized intensity
};

/** Provenance of a control change. Only real user input may author commands. */
enum class ShowCommandOrigin : quint8
{
    Pointer,      //!< mouse/touch on a VC control
    Midi,         //!< accepted MIDI input
    Audio,        //!< audio mapping
    Programmatic, //!< generic setter/script
    Replay,       //!< playback feedback of this very track
    Keyboard      //!< key sequence on a VC control: local user input, like the pointer
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

    static ShowCommand start(quint32 id, quint32 time, quint32 functionId);
    static ShowCommand stop(quint32 id, quint32 time, quint32 functionId);
    static ShowCommand setIntensity(quint32 id, quint32 time, quint32 functionId, qreal intensity);

    /** Empty when the command is valid, otherwise the reason */
    static QString validate(const ShowCommand &cmd);

    static QString actionToString(ShowCommandAction action);
    /** false when the name is not a known action */
    static bool actionFromString(const QString &name, ShowCommandAction *action);

    bool operator==(const ShowCommand &other) const;
    bool operator!=(const ShowCommand &other) const { return !(*this == other); }
};

/**
 * The authored commands of one Show plus its authored playback extent.
 *
 * Commands are kept ordered by (time, insertion order). Editing is explicit and
 * local: nothing is overwritten implicitly and a rejected edit leaves the track
 * untouched. The extent is authored data, never derived from the last command and
 * never a source of synthesized Stops - the host decides what a Show does with it.
 */
class ShowCommandTrack
{
public:
    /** Serialization version. A file with any other version fails to load. */
    static constexpr int Version = 1;

    bool isEmpty() const;
    int count() const;

    /** Ordered by (time, insertion order) */
    const QVector<ShowCommand> &commands() const;

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

    /** Free id for the next authored command, InvalidId when exhausted */
    quint32 nextEventId() const;

    /** Fails on an invalid command or a duplicate id */
    bool insert(const ShowCommand &cmd, QString *error = nullptr);
    /** Replaces the command carrying cmd.id, keeping its place among equal times
     *  unless the time changes. Fails when the id is unknown or the value invalid. */
    bool replace(const ShowCommand &cmd, QString *error = nullptr);
    /** Moves one command in time, leaving every other field and command alone */
    bool retime(quint32 id, quint32 time, QString *error = nullptr);
    bool remove(quint32 id, QString *error = nullptr);

    /** Sorted, unique Function targets referenced by this track */
    QList<quint32> referencedFunctionIds() const;
    /** Retargets every command pointing at `from`. Returns the number changed. */
    int remapFunctionId(quint32 from, quint32 to);

    /** Transactional: on failure the track keeps its previous contents */
    bool loadXML(QXmlStreamReader &root, QString *error = nullptr);
    bool saveXML(QXmlStreamWriter *doc) const;

private:
    QVector<ShowCommand> m_commands;
    quint32 m_extent = 0;
};

/** What the recorder is doing, derived from the state below */
enum class ShowRecordPhase : quint8
{
    Off,      //!< not recording
    Armed,    //!< recording requested, no Show bound yet
    Bound,    //!< bound to the resolved Show
    Suspended //!< bound, but another Show is currently resolved
};

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

    /** Events authored live during this traversal, which the host already executed.
     *  Traversal metadata of the cursor, not a registry of anything running: an id
     *  is dropped again as soon as the playhead consumes its time. */
    QSet<quint32> consumedLiveEventIds;

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

    /** Jump or loop to positionMs. Effects restore the latest authored intensity
     *  per target from before the destination; historical Start/Stop are not
     *  replayed. Record intent and binding survive. */
    ShowCommandTransition seek(const ShowCommandTrack &track,
                               const ShowCommandState &state, quint32 positionMs);

    /** Offer real user input to the recorder. Authors at most one command at the
     *  authoritative position and marks its id as consumed, so the freshly authored
     *  command cannot echo back through advance() during this traversal while every
     *  other due command still executes. The host has already executed the input
     *  live, so this returns no effects. */
    ShowCommandTransition userInput(const ShowCommandTrack &track,
                                    const ShowCommandState &state, const ShowCommandInput &input);
}

/** @} */

#endif // SHOWCOMMANDTRACK_H
