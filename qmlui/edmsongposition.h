/*
  Q Light Controller Plus
  edmsongposition.h

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

#ifndef EDMSONGPOSITION_H
#define EDMSONGPOSITION_H

#include "edmanalysisreader.h"

/** Where a song is, in its own bars. Index and distance fields are -1 when not available. */
struct EdmSongPosition
{
    bool valid = false;
    double bar = 0;              // bar 0 starts at the first downbeat, negative before it
    int section = -1;            // -1 past the end of the last section
    double barInSection = -1;
    double barsToNextSection = -1;
    int nextEvent = -1;          // first event of the asked kind after bar
    double barsToNextEvent = -1;
};

/** What a step of the playhead passed */
struct EdmCrossing
{
    enum Kind { SectionStart, Event };

    Kind kind = SectionStart;
    int index = -1;              // into sections or events
    double bar = 0;
    int confidence = kEdmNoConfidence;
};

struct EdmCrossings
{
    bool jumped = false;         // the playhead went backwards: nothing crossed
    int activeSection = -1;
    QList<EdmCrossing> crossed;  // in bar order, a section start before an event on the same bar
};

/** Bar position of a time in the audio file */
double edmBarAt(const EdmAnalysis &analysis, double elapsedMs);

/** Position at elapsedMs; eventKind picks which events nextEvent looks for (empty: none) */
EdmSongPosition edmSongPosition(const EdmAnalysis &analysis, double elapsedMs, const QString &eventKind = QString());

/** The section active at the first position seen, as the one crossing */
EdmCrossings edmFirstCrossings(const EdmAnalysis &analysis, double bar);

/** Section starts and events in (from, to]; the first section's start is only
 *  ever reported by edmFirstCrossings. to < from is a jump. */
EdmCrossings edmCrossings(const EdmAnalysis &analysis, double from, double to);

#endif
