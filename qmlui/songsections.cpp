/*
  Q Light Controller Plus
  songsections.cpp

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

#include "songsections.h"
#include "edmstructuremapping.h"

#include <QtMath>
#include <algorithm>

bool sectionActionType(const QString &name, SectionAction::Type *type)
{
    static const QList<std::pair<QString, SectionAction::Type>> names {
        { "move_boundary", SectionAction::MoveBoundary }, { "relabel", SectionAction::Relabel },
        { "split", SectionAction::Split }, { "merge", SectionAction::Merge },
        { "reset", SectionAction::Reset }, { "reset_all", SectionAction::ResetAll } };
    for (const auto &[n, t] : names)
    {
        if (n == name)
        {
            *type = t;
            return true;
        }
    }
    return false;
}

double snapToBar(const EdmGrid &grid, double ms)
{
    if (grid.barMs > 0 == false)
        return ms;
    return grid.firstDownbeatMs + qRound64((ms - grid.firstDownbeatMs) / grid.barMs) * grid.barMs;
}

static double barAt(const EdmGrid &grid, double ms)
{
    return grid.barMs > 0 ? (ms - grid.firstDownbeatMs) / grid.barMs : 0;
}

/** The parts of [start, end) no pin covers */
static QList<std::pair<double, double>> uncovered(double start, double end, const QList<SectionPin> &pins)
{
    QList<std::pair<double, double>> parts { { start, end } };
    for (const SectionPin &pin : pins)
    {
        QList<std::pair<double, double>> next;
        for (const auto &[s, e] : parts)
        {
            if (pin.endMs <= s || pin.startMs >= e)
            {
                next.append({ s, e });
                continue;
            }
            if (pin.startMs > s)
                next.append({ s, pin.startMs });
            if (pin.endMs < e)
                next.append({ pin.endMs, e });
        }
        parts = next;
    }
    return parts;
}

QList<SongSection> effectiveSections(const EdmAnalysis &analysis, const SectionEdits &edits)
{
    const EdmGrid &grid = analysis.grid;
    QList<SongSection> pieces;
    for (const EdmSection &s : analysis.sections)
    {
        for (const auto &[start, end] : uncovered(s.startMs, s.endMs, edits.pins))
        {
            if (end <= start)
                continue;
            SongSection piece;
            static_cast<EdmSection &>(piece) = s;
            piece.startMs = start;
            piece.endMs = end;
            if (start != s.startMs)
                piece.startBar = barAt(grid, start);
            if (end != s.endMs)
                piece.endBar = barAt(grid, end);
            pieces.append(piece);
        }
    }

    // a piece under half a bar is noise of a shifted analysis: it joins the analysis piece next to it
    const double shortMs = grid.barMs > 0 ? grid.barMs / 2 : 0;
    for (int i = 0; i < pieces.count();)
    {
        const SongSection &p = pieces.at(i);
        if (p.endMs - p.startMs >= shortMs)
        {
            i++;
            continue;
        }
        if (i + 1 < pieces.count() && pieces.at(i + 1).startMs == p.endMs)
        {
            pieces[i + 1].startMs = p.startMs;
            pieces[i + 1].startBar = p.startBar;
        }
        else if (i > 0 && pieces.at(i - 1).endMs == p.startMs)
        {
            pieces[i - 1].endMs = p.endMs;
            pieces[i - 1].endBar = p.endBar;
        }
        else
        {
            i++;
            continue;
        }
        pieces.removeAt(i);
    }

    for (const SectionPin &pin : edits.pins)
    {
        SongSection s;
        s.label = pin.label;
        s.startMs = pin.startMs;
        s.endMs = pin.endMs;
        s.startBar = barAt(grid, pin.startMs);
        s.endBar = barAt(grid, pin.endMs);
        s.confidence = 100;
        s.labelConfidence = 100;
        s.edited = true;
        pieces.append(s);
    }
    std::stable_sort(pieces.begin(), pieces.end(),
                     [](const SongSection &a, const SongSection &b) { return a.startMs < b.startMs; });
    return pieces;
}

/** The edits with these pins on top: older pins under them are cut away */
static SectionEdits pinned(const SectionEdits &edits, const QList<SectionPin> &pins)
{
    SectionEdits result;
    for (const SectionPin &old : edits.pins)
    {
        for (const auto &[start, end] : uncovered(old.startMs, old.endMs, pins))
        {
            if (end > start)
                result.pins.append({ old.label, start, end });
        }
    }
    result.pins.append(pins);
    std::sort(result.pins.begin(), result.pins.end(),
              [](const SectionPin &a, const SectionPin &b) { return a.startMs < b.startMs; });
    return result;
}

static SectionEdits refuse(const SectionEdits &edits, QString *error, const QString &why)
{
    if (error)
        *error = why;
    return edits;
}

SectionEdits applySectionAction(const EdmAnalysis &analysis, const SectionEdits &edits,
                                const SectionAction &action, QString *error)
{
    if (error)
        error->clear();
    if (action.type == SectionAction::ResetAll)
        return SectionEdits();

    const QList<SongSection> sections = effectiveSections(analysis, edits);
    const int i = action.section;
    if (i < 0 || i >= sections.count())
        return refuse(edits, error, QString("no section %1, the song has %2").arg(i).arg(sections.count()));
    const SongSection &s = sections.at(i);
    const bool hasNext = i + 1 < sections.count();

    switch (action.type)
    {
        case SectionAction::MoveBoundary:
        {
            if (hasNext == false)
                return refuse(edits, error, QString("section %1 is the last, it has no boundary to move").arg(i));
            const SongSection &next = sections.at(i + 1);
            const double at = snapToBar(analysis.grid, action.timeMs);
            if (at <= s.startMs || at >= next.endMs)
                return refuse(edits, error, QString("the boundary at %1 ms (bar snapped) leaves section %2 or %3 empty")
                                                .arg(at).arg(i).arg(i + 1));
            return pinned(edits, { { s.label, s.startMs, at }, { next.label, at, next.endMs } });
        }
        case SectionAction::Relabel:
            if (EdmStructureMapping::labels().contains(action.label) == false)
                return refuse(edits, error, QString("unknown label '%1', expected one of %2")
                                                .arg(action.label, EdmStructureMapping::labels().join(", ")));
            return pinned(edits, { { action.label, s.startMs, s.endMs } });
        case SectionAction::Split:
        {
            const double at = snapToBar(analysis.grid, action.timeMs);
            if (at <= s.startMs || at >= s.endMs)
                return refuse(edits, error, QString("%1 ms (bar snapped) is not inside section %2").arg(at).arg(i));
            return pinned(edits, { { s.label, s.startMs, at }, { s.label, at, s.endMs } });
        }
        case SectionAction::Merge:
            if (hasNext == false)
                return refuse(edits, error, QString("section %1 is the last, it has no next to merge with").arg(i));
            return pinned(edits, { { s.label, s.startMs, sections.at(i + 1).endMs } });
        case SectionAction::Reset:
        {
            SectionEdits result;
            for (const SectionPin &pin : edits.pins)
            {
                if (s.edited == false || pin.startMs != s.startMs || pin.endMs != s.endMs)
                    result.pins.append(pin);
            }
            return result;
        }
        case SectionAction::ResetAll:
            break;
    }
    return SectionEdits();
}
