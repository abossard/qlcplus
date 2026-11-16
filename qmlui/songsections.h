/*
  Q Light Controller Plus
  songsections.h

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

#ifndef SONGSECTIONS_H
#define SONGSECTIONS_H

#include <QList>
#include <QString>

#include "edmanalysisreader.h"

/** A section the user edited, in song time (ms from the audio start). It
 *  stays as the user left it, whatever a later analysis says */
struct SectionPin
{
    QString label;
    double startMs = 0;
    double endMs = 0;

    bool operator==(const SectionPin &other) const
    {
        return label == other.label && startMs == other.startMs && endMs == other.endMs;
    }
};

/** The user's section edits of one song, on top of its analysis: pins by
 *  start, never overlapping. The analysis fills the time no pin covers */
struct SectionEdits
{
    QList<SectionPin> pins;

    bool isEmpty() const { return pins.isEmpty(); }
    bool operator==(const SectionEdits &other) const { return pins == other.pins; }
};

/** A section of the song as the user sees it: the analysis' or an edited one.
 *  An edited section is the user's word: 100 % sure */
struct SongSection : EdmSection
{
    bool edited = false;
};

/** One action on the sections; section is an index into effectiveSections() */
struct SectionAction
{
    enum Type
    {
        MoveBoundary,   // the boundary between section and section + 1 to timeMs
        Relabel,        // section gets label
        Split,          // section in two at timeMs
        Merge,          // section and section + 1 become one, with the label of section
        Reset,          // section back to the analysis
        ResetAll        // every section back to the analysis
    };
    Type type = ResetAll;
    int section = -1;
    double timeMs = 0;   // song time
    QString label;
};

/** "move_boundary", "relabel", "split", "merge", "reset", "reset_all"; false for anything else */
bool sectionActionType(const QString &name, SectionAction::Type *type);

/** The bar line of the analysis' grid nearest to ms */
double snapToBar(const EdmGrid &grid, double ms);

/** The analysis' sections with the edits on top: a pin replaces the analysis
 *  wherever it lies, the analysis keeps the rest (pieces under half a bar are
 *  left out). Sorted by start */
QList<SongSection> effectiveSections(const EdmAnalysis &analysis, const SectionEdits &edits);

/** The edits after the action. New boundaries snap to the analysis' bar grid;
 *  every section the action changes becomes a pin. A refused action returns
 *  the edits unchanged and says why in error */
SectionEdits applySectionAction(const EdmAnalysis &analysis, const SectionEdits &edits,
                                const SectionAction &action, QString *error = nullptr);

#endif
