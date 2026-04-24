/*
  Q Light Controller Plus
  gridlayout.h

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

#ifndef GRIDLAYOUT_H
#define GRIDLAYOUT_H

#include <QRect>
#include <QVector>

namespace GridLayout {

struct GridCell {
    int x, y, w, h;
};

struct GridItem {
    int id;
    GridCell cell;
};

GridCell pixelsToCells(const QRect &pixelRect, int cellW, int cellH);

QRect cellsToPixels(const GridCell &cell, int cellW, int cellH);

QVector<GridItem> compactVertical(const QVector<GridItem> &items);

QVector<GridItem> resolveCollisions(const QVector<GridItem> &items, int movedId, bool compact = true);

bool cellsOverlap(const GridCell &a, const GridCell &b);

int cellWidth(int frameWidth, int columns);

} // namespace GridLayout

#endif // GRIDLAYOUT_H
