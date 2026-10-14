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
#include <QRectF>
#include <QSizeF>
#include <QString>
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

constexpr quint32 InvalidId = ~quint32(0);

qreal arithmeticTolerance(qreal a, qreal b);
bool less(qreal a, qreal b);
qreal snapUp(qreal value, qreal step);

struct LayoutItem
{
    quint32 id = InvalidId;
    quint32 parentId = InvalidId;
    int subpage = 0;
    int order = 0;
    QRectF rect;
    // Mandatory measured usability minimum, finite and positive in both dimensions.
    QSizeF minimumSize;
};

struct LayoutPage
{
    int subpage = 0;
    QRectF contentRect;
    int columns = 0;
    bool preserveColumnBands = false;
    bool snappingEnabled = false;
    QSizeF snapStep = QSizeF(1, 1);
};

struct LayoutContainer
{
    quint32 id = InvalidId;
    QSizeF size;
    // Mandatory measured frame minimum. Default/zero QSizeF values are invalid.
    QSizeF minimumSize;
    QVector<LayoutPage> pages;
};

struct LayoutSnapshot
{
    quint32 rootId = InvalidId;
    QVector<LayoutItem> items;
    QVector<LayoutContainer> containers;
};

struct LayoutOptions
{
    qreal padding = 0;
    qreal gap = 0;
    bool allowHeightGrowth = false;
    bool allowWidthGrowth = false;
};

struct Placement
{
    quint32 id = InvalidId;
    quint32 parentId = InvalidId;
    int subpage = 0;
    int order = 0;
    QRectF rect;
};

struct ContainerGrowth
{
    quint32 id = InvalidId;
    QSizeF size;
};

struct Tracks
{
    quint32 parentId = InvalidId;
    int subpage = 0;
    int region = 0;
    QVector<qreal> columnStarts;
    QVector<qreal> columnWidths;
    QVector<qreal> rowStarts;
    QVector<qreal> rowHeights;
};

struct Diagnostic
{
    QString code;
    QString message;
    quint32 widgetId = InvalidId;
    quint32 parentId = InvalidId;
    qreal required = 0;
    qreal available = 0;
};

struct LayoutPlan
{
    QVector<Placement> placements;
    QVector<ContainerGrowth> growth;
    QVector<Tracks> tracks;
    Diagnostic error;

    bool ok() const { return error.code.isEmpty(); }
};

// Rectangles are parent-local logical values. The root has no LayoutItem.
// Container pages include inactive subpages and exclude headers/borders.
// Growth retains each page's insets by adding the outer-size delta to its content size.
// Align preserves leaf sizes and stored order; compact retains the geometric grid order.
// Failure plans contain a diagnostic and no placements, growth or tracks.
LayoutPlan align(const LayoutSnapshot &snapshot, const LayoutOptions &options);
LayoutPlan compact(const LayoutSnapshot &snapshot, const LayoutOptions &options);
// Validates the complete candidate geometry, metadata and explicitly permitted growth.
// An empty diagnostic code means valid; stale/live-state admission remains the caller's.
Diagnostic validate(const LayoutSnapshot &snapshot, const LayoutPlan &plan, const LayoutOptions &options);

} // namespace GridLayout

#endif // GRIDLAYOUT_H
