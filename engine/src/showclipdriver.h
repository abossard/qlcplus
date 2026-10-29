/*
  Q Light Controller Plus
  showclipdriver.h

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

#ifndef SHOWCLIPDRIVER_H
#define SHOWCLIPDRIVER_H

#include <QHash>
#include <QList>
#include <QMap>
#include <QPair>
#include <QVector>
#include <optional>

#include "functionparent.h"
#include "showclipautomation.h"

class ShowFunction;
class Function;
class Show;
class Doc;

/** @addtogroup engine_functions Functions
 * @{
 */

/**
 * Plays the clips of one Show runner through their automation, on the timer
 * thread. A clip's Function starts at the clip's in-point, at the level of
 * its track intensity x the envelope x the Show's master intensity. A nested
 * Show follows its clip every tick instead: External clock from the position
 * map, paused by its clip in a deck pause, master intensity from that level,
 * dominant when the clip and this Show are, with its Audio suppressed when
 * this Show suppresses nested audio or its own. One that ends itself inside
 * its clip plays again from where the map moves below its end, as live
 * Perform plays it again.
 * It gets its own settings back once its clip ends, or at once when somebody
 * else stopped it. One live Perform drives is Perform's: a clip neither
 * starts it, nor drives it, nor gives it anything back.
 * A Function plays once: of its clips active at a time, the one that starts
 * last drives it. Commands multiply their intensity into the clip's level.
 * A Function another owner plays on gets back the level this Show's clip
 * replaced, unless somebody wrote over the clip's since.
 */
class ShowClipDriver final
{
public:
    ShowClipDriver(const Doc *doc, quint32 showId);

    /** Start f, not running for this Show yet, for clip at its clip time at,
     *  in the clock units of f */
    void start(const ShowFunction *clip, Function *f, quint32 at, const QMap<quint32, qreal> &trackIntensities);

    /** clip, also active, plays f too; one this Show plays without a clip
     *  driving it is taken over */
    void join(const ShowFunction *clip, Function *f);

    /** Each tick: what is still running takes the clock, level and dominance
     *  of its driving clip at the Show's clocks; a nested Show somebody else
     *  stopped gets its settings back, one that ended itself keeps them to
     *  its clip's end */
    void drive(quint32 elapsedTime, quint32 elapsedBeats, const QMap<quint32, qreal> &trackIntensities,
               const QList<QPair<Function *, quint32>> &running);

    /** A track intensity changed: the levels follow at once, paused too */
    void applyLevels(const QMap<quint32, qreal> &trackIntensities);

    /** A command set f's intensity: false when no clip plays f, else the clip
     *  multiplies it into its level, at once */
    bool setCommandIntensity(Function *f, qreal value, const QMap<quint32, qreal> &trackIntensities);

    /** The intensity commands set on f while a clip plays it for this Show,
     *  none otherwise */
    std::optional<qreal> commandIntensity(const Function *f) const;

    /** The value of f's Intensity override (Single), 1.0 without one */
    static qreal intensityOverride(const Function *f);

    /** Every nested Show gets its settings back */
    void releaseAll();

    /** A clip of f active at a seek target keeps f running: its clock moves */
    static bool keepsThroughSeek(const Function *f);

    /** f is a Show live Perform drives */
    static bool performed(const Function *f);

private:
    struct Clip
    {
        const ShowFunction *clip = nullptr;
        ShowClipAutomation automation;
        quint32 trackId = 0;
    };

    /** What a nested Show had before its clip took it */
    struct Settings
    {
        int syncSource = 0;
        bool audioSuppressed = false;
        qreal masterIntensity = 1.0;
        bool dominant = true;
    };

    struct Drive
    {
        Function *function = nullptr;
        /** Every clip that started or joined it */
        QVector<Clip> clips;
        int driving = 0;
        /** Time in the driving clip */
        quint32 at = 0;
        /** Last level applied to a Function that is no Show */
        qreal level = -1.0;
        /** Last level this Show's clips wrote on it, also before a restart */
        qreal wrote = -1.0;
        /** The level the clips' writes replaced */
        std::optional<qreal> replaced;
        /** What commands set, multiplied into the level */
        qreal command = 1.0;
        /** The last tick found this Show playing it */
        bool owned = true;
        Show *nested = nullptr;
        Settings saved;
        /** The clip suppressed the nested Show's Audio */
        bool audioSuppressed = false;
    };

    FunctionParent parent() const;
    /** A new drive of f for clip at its clip time at, taking over a nested Show */
    Drive &adopt(const ShowFunction *clip, Function *f, quint32 at);
    Clip clipOf(const ShowFunction *clip) const;
    void applyLevel(Drive &drive, const QMap<quint32, qreal> &trackIntensities);
    void release(const Drive &drive);

    const Doc *m_doc;
    Show *m_show;
    QHash<quint32, Drive> m_drives;
};

/** @} */

#endif
