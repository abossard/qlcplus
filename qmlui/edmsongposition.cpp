/*
  Q Light Controller Plus
  edmsongposition.cpp

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

#include "edmsongposition.h"

#include <algorithm>

#include <QtMath>

/** Bars are snapped to a millionth so a time computed from a bar lands on it */
static double snapped(double bar)
{
    return qRound64(bar * 1e6) / 1e6;
}

static int sectionAt(const EdmAnalysis &analysis, double bar)
{
    if (analysis.valid == false || bar >= analysis.sections.last().endBar)
        return -1;

    int found = 0;
    for (int i = 1; i < analysis.sections.size(); i++)
    {
        if (analysis.sections[i].startBar <= bar)
            found = i;
    }
    return found;
}

double edmBarAt(const EdmAnalysis &analysis, double elapsedMs)
{
    if (analysis.valid == false)
        return 0;
    return snapped((elapsedMs - analysis.grid.firstDownbeatMs) / analysis.grid.barMs);
}

EdmSongPosition edmSongPosition(const EdmAnalysis &analysis, double elapsedMs, const QString &eventKind)
{
    EdmSongPosition p;
    if (analysis.valid == false)
        return p;

    p.valid = true;
    p.bar = edmBarAt(analysis, elapsedMs);
    p.section = sectionAt(analysis, p.bar);
    if (p.section >= 0)
    {
        p.barInSection = qMax(0.0, p.bar - analysis.sections[p.section].startBar);
        if (p.section + 1 < analysis.sections.size())
            p.barsToNextSection = analysis.sections[p.section].endBar - p.bar;
    }

    if (eventKind.isEmpty() == false)
    {
        for (int i = 0; i < analysis.events.size(); i++)
        {
            const EdmEvent &e = analysis.events[i];
            if (e.kind == eventKind && e.bar > p.bar && (p.nextEvent < 0 || e.bar < analysis.events[p.nextEvent].bar))
                p.nextEvent = i;
        }
        if (p.nextEvent >= 0)
            p.barsToNextEvent = analysis.events[p.nextEvent].bar - p.bar;
    }
    return p;
}

static EdmCrossing sectionStart(const EdmAnalysis &analysis, int index)
{
    return {EdmCrossing::SectionStart, index, analysis.sections[index].startBar, analysis.sections[index].confidence};
}

EdmCrossings edmFirstCrossings(const EdmAnalysis &analysis, double bar)
{
    EdmCrossings result;
    result.activeSection = sectionAt(analysis, bar);
    if (result.activeSection >= 0)
        result.crossed.append(sectionStart(analysis, result.activeSection));
    return result;
}

EdmCrossings edmCrossings(const EdmAnalysis &analysis, double from, double to)
{
    EdmCrossings result;
    result.activeSection = sectionAt(analysis, to);
    if (analysis.valid == false)
        return result;
    if (to < from)
    {
        result.jumped = true;
        return result;
    }

    for (int i = 1; i < analysis.sections.size(); i++)
    {
        if (analysis.sections[i].startBar > from && analysis.sections[i].startBar <= to)
            result.crossed.append(sectionStart(analysis, i));
    }
    for (int i = 0; i < analysis.events.size(); i++)
    {
        const EdmEvent &e = analysis.events[i];
        if (e.bar > from && e.bar <= to)
            result.crossed.append({EdmCrossing::Event, i, e.bar, e.confidence});
    }
    std::stable_sort(result.crossed.begin(), result.crossed.end(), [](const EdmCrossing &a, const EdmCrossing &b) {
        return a.bar != b.bar ? a.bar < b.bar : a.kind < b.kind;
    });
    return result;
}
