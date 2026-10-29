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
 * map, master intensity from that level, dominant when the clip and this
 * Show are. It plays with its Audio suppressed and gets its own settings
 * back once its clip ends. A Function plays once: of its clips active at a
 * time, the one that starts last drives it.
 */
class ShowClipDriver final
{
public:
    ShowClipDriver(const Doc *doc, quint32 showId);

    /** Start f, not running for this Show yet, for clip at its clip time at,
     *  in the clock units of f */
    void start(const ShowFunction *clip, Function *f, quint32 at, const QMap<quint32, qreal> &trackIntensities);

    /** clip, also active, plays f too */
    void join(const ShowFunction *clip, Function *f);

    /** Each tick: what is still running takes the clock, level and dominance
     *  of its driving clip at the Show's clocks; a nested Show that is not
     *  running gets its settings back */
    void drive(quint32 elapsedTime, quint32 elapsedBeats, const QMap<quint32, qreal> &trackIntensities,
               const QList<QPair<Function *, quint32>> &running);

    /** A track intensity changed: the levels follow at once, paused too */
    void applyLevels(const QMap<quint32, qreal> &trackIntensities);

    /** Every nested Show gets its settings back */
    void releaseAll();

    /** A clip of f active at a seek target keeps f running: its clock moves */
    static bool keepsThroughSeek(const Function *f);

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
        Show *nested = nullptr;
        Settings saved;
    };

    Clip clipOf(const ShowFunction *clip) const;
    void applyLevel(Drive &drive, const QMap<quint32, qreal> &trackIntensities);
    void release(const Drive &drive);

    const Doc *m_doc;
    Show *m_show;
    QHash<quint32, Drive> m_drives;
};

/** @} */

#endif
