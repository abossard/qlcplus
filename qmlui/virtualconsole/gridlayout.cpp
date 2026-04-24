/*
  Q Light Controller Plus
  gridlayout.cpp

  Copyright (C) Massimo Callegari

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

#include "gridlayout.h"

#include <QQueue>
#include <QSet>
#include <algorithm>
#include <cmath>

namespace GridLayout {

static int roundDiv(int numerator, int denominator)
{
    if (denominator <= 0)
        return 0;
    // Round to nearest for non-negative values, typical for pixel->cell conversion.
    double v = static_cast<double>(numerator) / static_cast<double>(denominator);
    return static_cast<int>(std::llround(v));
}

GridCell pixelsToCells(const QRect &pixelRect, int cellW, int cellH)
{
    GridCell c;
    c.x = roundDiv(pixelRect.x(), cellW);
    c.y = roundDiv(pixelRect.y(), cellH);
    c.w = roundDiv(pixelRect.width(), cellW);
    c.h = roundDiv(pixelRect.height(), cellH);
    if (c.w < 1) c.w = 1;
    if (c.h < 1) c.h = 1;
    return c;
}

QRect cellsToPixels(const GridCell &cell, int cellW, int cellH)
{
    return QRect(cell.x * cellW, cell.y * cellH, cell.w * cellW, cell.h * cellH);
}

bool cellsOverlap(const GridCell &a, const GridCell &b)
{
    return a.x < b.x + b.w
        && b.x < a.x + a.w
        && a.y < b.y + b.h
        && b.y < a.y + a.h;
}

static bool horizontallyOverlaps(const GridCell &a, const GridCell &b)
{
    return a.x < b.x + b.w && b.x < a.x + a.w;
}

QVector<GridItem> compactVertical(const QVector<GridItem> &items)
{
    QVector<GridItem> result = items;
    // Stable sort by (y, x) — preserves relative order of equals.
    std::stable_sort(result.begin(), result.end(), [](const GridItem &a, const GridItem &b) {
        if (a.cell.y != b.cell.y)
            return a.cell.y < b.cell.y;
        return a.cell.x < b.cell.x;
    });

    for (int i = 0; i < result.size(); ++i)
    {
        int minY = 0;
        for (int j = 0; j < i; ++j)
        {
            if (horizontallyOverlaps(result[i].cell, result[j].cell))
            {
                int below = result[j].cell.y + result[j].cell.h;
                if (below > minY)
                    minY = below;
            }
        }
        result[i].cell.y = minY;
    }
    return result;
}

QVector<GridItem> resolveCollisions(const QVector<GridItem> &items, int movedId, bool compact)
{
    QVector<GridItem> result = items;

    // Index lookup
    auto indexOf = [&](int id) -> int {
        for (int i = 0; i < result.size(); ++i)
            if (result[i].id == id)
                return i;
        return -1;
    };

    int movedIdx = indexOf(movedId);
    if (movedIdx < 0)
        return result;

    QQueue<int> worklist;
    QSet<int> enqueued;
    worklist.enqueue(movedId);
    enqueued.insert(movedId);

    while (!worklist.isEmpty())
    {
        int curId = worklist.dequeue();
        int curIdx = indexOf(curId);
        if (curIdx < 0) continue;
        const GridCell curCell = result[curIdx].cell;

        for (int i = 0; i < result.size(); ++i)
        {
            if (result[i].id == curId)
                continue;
            if (cellsOverlap(curCell, result[i].cell))
            {
                int newY = curCell.y + curCell.h;
                if (newY > result[i].cell.y)
                {
                    result[i].cell.y = newY;
                    if (!enqueued.contains(result[i].id))
                    {
                        worklist.enqueue(result[i].id);
                        enqueued.insert(result[i].id);
                    }
                }
            }
        }
    }

    if (compact)
    {
        // Compact non-moved items only; pin the moved item at its current y.
        int pinnedY = result[indexOf(movedId)].cell.y;
        GridCell pinnedCell = result[indexOf(movedId)].cell;

        // Run a constrained compaction: for each item (sorted by y,x) other than moved,
        // drop it as far up as possible but not above the pinned item's bottom if they overlap horizontally,
        // and not overlapping any other placed item.
        QVector<GridItem> sorted = result;
        std::stable_sort(sorted.begin(), sorted.end(), [](const GridItem &a, const GridItem &b) {
            if (a.cell.y != b.cell.y) return a.cell.y < b.cell.y;
            return a.cell.x < b.cell.x;
        });

        QVector<GridItem> placed;
        placed.reserve(sorted.size());

        // Place the moved item first at its fixed position.
        for (const auto &it : sorted)
        {
            if (it.id == movedId)
            {
                placed.append(it);
                break;
            }
        }

        for (const auto &it : sorted)
        {
            if (it.id == movedId)
                continue;
            int minY = 0;
            for (const auto &p : placed)
            {
                if (horizontallyOverlaps(it.cell, p.cell))
                {
                    int below = p.cell.y + p.cell.h;
                    if (below > minY)
                        minY = below;
                }
            }
            GridItem moved = it;
            moved.cell.y = minY;
            placed.append(moved);
        }

        // Preserve original ordering of result (by id).
        for (int i = 0; i < result.size(); ++i)
        {
            for (const auto &p : placed)
            {
                if (p.id == result[i].id)
                {
                    result[i].cell = p.cell;
                    break;
                }
            }
        }
        // Ensure moved pinned
        result[indexOf(movedId)].cell = pinnedCell;
        Q_UNUSED(pinnedY);
    }

    return result;
}

int cellWidth(int frameWidth, int columns)
{
    if (columns <= 0)
        return 0;
    return frameWidth / columns;
}

} // namespace GridLayout
