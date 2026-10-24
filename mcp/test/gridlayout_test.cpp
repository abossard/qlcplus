/*
  Q Light Controller Plus - Unit test
  gridlayout_test.cpp

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

#include <QtTest>
#include <QVector>
#include <cmath>
#include <limits>

#include "gridlayout_test.h"
#include "gridlayout.h"

using namespace GridLayout;

Q_DECLARE_METATYPE(LayoutSnapshot)
Q_DECLARE_METATYPE(LayoutOptions)
Q_DECLARE_METATYPE(LayoutPlan)

static LayoutSnapshot panel(const QVector<QRectF> &rectangles, const QSizeF &size, int columns = 0)
{
    LayoutSnapshot snapshot;
    snapshot.rootId = 100;
    LayoutPage page;
    page.contentRect = QRectF(QPointF(0, 0), size);
    page.columns = columns;
    snapshot.containers.append({100, size, QSizeF(1, 1), {page}});
    for (int i = 0; i < rectangles.size(); ++i)
        snapshot.items.append({quint32(i + 1), 100, 0, i, rectangles[i], QSizeF(1, 1)});
    return snapshot;
}

static QByteArray snapshotValues(const LayoutSnapshot &snapshot)
{
    QByteArray bytes;
    QDataStream out(&bytes, QIODevice::WriteOnly);
    out << snapshot.rootId;
    for (const auto &item : snapshot.items)
        out << item.id << item.parentId << item.subpage << item.order << item.rect << item.minimumSize;
    for (const auto &container : snapshot.containers)
    {
        out << container.id << container.size << container.minimumSize;
        for (const auto &page : container.pages)
            out << page.subpage << page.contentRect << page.columns << page.preserveColumnBands
                << page.snappingEnabled << page.snapStep;
    }
    return bytes;
}

static QVector<QRectF> plannedRectangles(const LayoutSnapshot &snapshot, const LayoutPlan &plan)
{
    QVector<QRectF> rectangles;
    for (const auto &item : snapshot.items)
    {
        QRectF rect = item.rect;
        for (const auto &placement : plan.placements)
            if (placement.id == item.id)
                rect = placement.rect;
        rectangles.append(rect);
    }
    return rectangles;
}

static LayoutSnapshot withPlan(LayoutSnapshot snapshot, const LayoutPlan &plan)
{
    for (auto &item : snapshot.items)
        for (const auto &placement : plan.placements)
            if (item.id == placement.id)
                item.rect = placement.rect;
    for (auto &container : snapshot.containers)
        for (const auto &growth : plan.growth)
            if (container.id == growth.id)
            {
                const QSizeF delta = growth.size - container.size;
                for (auto &page : container.pages)
                    page.contentRect.setSize(page.contentRect.size() + delta);
                container.size = growth.size;
            }
    return snapshot;
}

static QByteArray planValues(const LayoutPlan &plan)
{
    QByteArray bytes;
    QDataStream out(&bytes, QIODevice::WriteOnly);
    for (const auto &placement : plan.placements)
        out << placement.id << placement.parentId << placement.subpage << placement.order << placement.rect;
    for (const auto &growth : plan.growth)
        out << growth.id << growth.size;
    for (const auto &tracks : plan.tracks)
        out << tracks.parentId << tracks.subpage << tracks.region << tracks.columnStarts
            << tracks.columnWidths << tracks.rowStarts << tracks.rowHeights;
    out << plan.error.code << plan.error.message << plan.error.widgetId << plan.error.parentId
        << plan.error.required << plan.error.available;
    return bytes;
}

static void verifyGeometry(const LayoutSnapshot &snapshot, const LayoutPlan &plan, const LayoutOptions &options)
{
    const LayoutSnapshot arranged = withPlan(snapshot, plan);
    for (const auto &container : arranged.containers)
        for (const auto &page : container.pages)
        {
            QVector<QRectF> siblings;
            for (const auto &item : arranged.items)
            {
                if (item.parentId != container.id || item.subpage != page.subpage)
                    continue;
                QVERIFY(std::isfinite(item.rect.x()) && std::isfinite(item.rect.y()));
                QVERIFY(item.rect.width() >= item.minimumSize.width());
                QVERIFY(item.rect.height() >= item.minimumSize.height());
                QVERIFY(item.rect.x() >= page.contentRect.x() + options.padding);
                QVERIFY(item.rect.y() >= page.contentRect.y() + options.padding);
                QVERIFY(item.rect.right() <= page.contentRect.right() - options.padding);
                QVERIFY(item.rect.bottom() <= page.contentRect.bottom() - options.padding);
                for (const auto &other : siblings)
                    QVERIFY(!item.rect.intersects(other));
                siblings.append(item.rect);
            }
        }
    for (const auto &tracks : plan.tracks)
    {
        for (int i = 1; i < tracks.columnStarts.size(); ++i)
            QVERIFY(tracks.columnStarts[i] >= tracks.columnStarts[i - 1] + tracks.columnWidths[i - 1] + options.gap);
        for (int i = 1; i < tracks.rowStarts.size(); ++i)
            QVERIFY(tracks.rowStarts[i] >= tracks.rowStarts[i - 1] + tracks.rowHeights[i - 1] + options.gap);
    }
}

// Helper: encode items as "id:x,y,w,h;id:x,y,w,h;..." for readable parametrization.
static QVector<GridItem> parseItems(const QByteArray &s)
{
    QVector<GridItem> out;
    if (s.trimmed().isEmpty())
        return out;
    const QList<QByteArray> parts = s.split(';');
    for (const QByteArray &p : parts)
    {
        if (p.trimmed().isEmpty()) continue;
        const QList<QByteArray> kv = p.split(':');
        GridItem it;
        it.id = kv[0].toInt();
        const QList<QByteArray> nums = kv[1].split(',');
        it.cell.x = nums[0].toInt();
        it.cell.y = nums[1].toInt();
        it.cell.w = nums[2].toInt();
        it.cell.h = nums[3].toInt();
        out.append(it);
    }
    return out;
}

static QByteArray serializeItems(const QVector<GridItem> &items)
{
    // Serialize sorted by id for stable comparison.
    QVector<GridItem> copy = items;
    std::stable_sort(copy.begin(), copy.end(), [](const GridItem &a, const GridItem &b) {
        return a.id < b.id;
    });
    QByteArray out;
    for (int i = 0; i < copy.size(); ++i)
    {
        if (i) out += ';';
        const GridItem &it = copy[i];
        out += QByteArray::number(it.id) + ':'
             + QByteArray::number(it.cell.x) + ','
             + QByteArray::number(it.cell.y) + ','
             + QByteArray::number(it.cell.w) + ','
             + QByteArray::number(it.cell.h);
    }
    return out;
}

// ========== compactVertical ==========

void GridLayout_Test::compactVertical_data()
{
    QTest::addColumn<QByteArray>("input");
    QTest::addColumn<QByteArray>("expected");

    QTest::newRow("empty") << QByteArray("") << QByteArray("");
    QTest::newRow("single floating item")
        << QByteArray("1:0,5,2,1")
        << QByteArray("1:0,0,2,1");
    QTest::newRow("same column with gap")
        << QByteArray("1:0,0,2,1;2:0,3,2,1")
        << QByteArray("1:0,0,2,1;2:0,1,2,1");
    QTest::newRow("different columns independent")
        << QByteArray("1:0,0,2,1;2:2,3,2,1")
        << QByteArray("1:0,0,2,1;2:2,0,2,1");
    QTest::newRow("stacked three")
        << QByteArray("1:0,0,4,2;2:1,5,2,1;3:0,8,4,1")
        << QByteArray("1:0,0,4,2;2:1,2,2,1;3:0,3,4,1");
}

void GridLayout_Test::compactVertical()
{
    QFETCH(QByteArray, input);
    QFETCH(QByteArray, expected);

    QVector<GridItem> items = parseItems(input);
    QVector<GridItem> out = GridLayout::compactVertical(items);
    QCOMPARE(serializeItems(out), expected);
}

// ========== resolveCollisions ==========

void GridLayout_Test::resolveCollisions_data()
{
    QTest::addColumn<QByteArray>("input");
    QTest::addColumn<int>("movedId");
    QTest::addColumn<QByteArray>("expected");

    QTest::newRow("single overlap push")
        << QByteArray("1:0,0,2,2;2:0,0,2,2")
        << 1
        << QByteArray("1:0,0,2,2;2:0,2,2,2");

    QTest::newRow("cascade push-down chain")
        << QByteArray("1:0,0,2,2;2:0,1,2,2;3:0,2,2,2")
        << 1
        << QByteArray("1:0,0,2,2;2:0,2,2,2;3:0,4,2,2");

    QTest::newRow("no overlap unchanged")
        << QByteArray("1:0,0,2,2;2:3,0,2,2")
        << 1
        << QByteArray("1:0,0,2,2;2:3,0,2,2");
}

void GridLayout_Test::resolveCollisions()
{
    QFETCH(QByteArray, input);
    QFETCH(int, movedId);
    QFETCH(QByteArray, expected);

    QVector<GridItem> items = parseItems(input);
    QVector<GridItem> out = GridLayout::resolveCollisions(items, movedId, true);
    QCOMPARE(serializeItems(out), expected);
}

// ========== pixelsToCells ==========

void GridLayout_Test::pixelsToCells_data()
{
    QTest::addColumn<int>("px");
    QTest::addColumn<int>("py");
    QTest::addColumn<int>("pw");
    QTest::addColumn<int>("ph");
    QTest::addColumn<int>("cellW");
    QTest::addColumn<int>("cellH");
    QTest::addColumn<int>("ex");
    QTest::addColumn<int>("ey");
    QTest::addColumn<int>("ew");
    QTest::addColumn<int>("eh");

    QTest::newRow("basic 50,100 / 100x60")
        << 50 << 100 << 200 << 120
        << 100 << 60
        << 1 << 2 << 2 << 2;
    QTest::newRow("min size clamp (tiny rect)")
        << 0 << 0 << 10 << 10
        << 100 << 60
        << 0 << 0 << 1 << 1;
    QTest::newRow("fractional spans enclose widget")
        << 0 << 0 << 140 << 84
        << 100 << 60
        << 0 << 0 << 2 << 2;
}

void GridLayout_Test::pixelsToCells()
{
    QFETCH(int, px); QFETCH(int, py); QFETCH(int, pw); QFETCH(int, ph);
    QFETCH(int, cellW); QFETCH(int, cellH);
    QFETCH(int, ex); QFETCH(int, ey); QFETCH(int, ew); QFETCH(int, eh);

    GridCell c = GridLayout::pixelsToCells(QRect(px, py, pw, ph), cellW, cellH);
    QCOMPARE(c.x, ex);
    QCOMPARE(c.y, ey);
    QCOMPARE(c.w, ew);
    QCOMPARE(c.h, eh);
}

// ========== cellsToPixels ==========

void GridLayout_Test::cellsToPixels_data()
{
    QTest::addColumn<int>("cx");
    QTest::addColumn<int>("cy");
    QTest::addColumn<int>("cw");
    QTest::addColumn<int>("ch");
    QTest::addColumn<int>("cellW");
    QTest::addColumn<int>("cellH");
    QTest::addColumn<int>("ex");
    QTest::addColumn<int>("ey");
    QTest::addColumn<int>("ew");
    QTest::addColumn<int>("eh");

    QTest::newRow("basic cell(1,2,2,2) / 100x60")
        << 1 << 2 << 2 << 2
        << 100 << 60
        << 100 << 120 << 200 << 120;
}

void GridLayout_Test::cellsToPixels()
{
    QFETCH(int, cx); QFETCH(int, cy); QFETCH(int, cw); QFETCH(int, ch);
    QFETCH(int, cellW); QFETCH(int, cellH);
    QFETCH(int, ex); QFETCH(int, ey); QFETCH(int, ew); QFETCH(int, eh);

    QRect r = GridLayout::cellsToPixels(GridCell{cx, cy, cw, ch}, cellW, cellH);
    QCOMPARE(r.x(), ex);
    QCOMPARE(r.y(), ey);
    QCOMPARE(r.width(), ew);
    QCOMPARE(r.height(), eh);
}

void GridLayout_Test::roundtripConversion()
{
    const int cellW = 100;
    const int cellH = 60;
    const QVector<GridCell> samples = {
        {0, 0, 1, 1},
        {1, 2, 2, 2},
        {5, 10, 3, 4},
        {7, 0, 1, 8},
    };
    for (const GridCell &c : samples)
    {
        QRect r = GridLayout::cellsToPixels(c, cellW, cellH);
        GridCell back = GridLayout::pixelsToCells(r, cellW, cellH);
        QCOMPARE(back.x, c.x);
        QCOMPARE(back.y, c.y);
        QCOMPARE(back.w, c.w);
        QCOMPARE(back.h, c.h);
    }
}

// ========== cellsOverlap ==========

void GridLayout_Test::cellsOverlap_data()
{
    QTest::addColumn<int>("ax"); QTest::addColumn<int>("ay");
    QTest::addColumn<int>("aw"); QTest::addColumn<int>("ah");
    QTest::addColumn<int>("bx"); QTest::addColumn<int>("by");
    QTest::addColumn<int>("bw"); QTest::addColumn<int>("bh");
    QTest::addColumn<bool>("expected");

    QTest::newRow("partial overlap")
        << 0 << 0 << 2 << 2 << 1 << 1 << 2 << 2 << true;
    QTest::newRow("adjacent (horizontal)")
        << 0 << 0 << 2 << 2 << 2 << 0 << 2 << 2 << false;
    QTest::newRow("identical")
        << 0 << 0 << 2 << 2 << 0 << 0 << 2 << 2 << true;
    QTest::newRow("adjacent (vertical)")
        << 0 << 0 << 2 << 2 << 0 << 2 << 2 << 2 << false;
    QTest::newRow("far apart")
        << 0 << 0 << 1 << 1 << 10 << 10 << 1 << 1 << false;
}

void GridLayout_Test::cellsOverlap()
{
    QFETCH(int, ax); QFETCH(int, ay); QFETCH(int, aw); QFETCH(int, ah);
    QFETCH(int, bx); QFETCH(int, by); QFETCH(int, bw); QFETCH(int, bh);
    QFETCH(bool, expected);

    bool r = GridLayout::cellsOverlap(GridCell{ax, ay, aw, ah}, GridCell{bx, by, bw, bh});
    QCOMPARE(r, expected);
}

void GridLayout_Test::cellWidth_basic()
{
    QCOMPARE(GridLayout::cellWidth(1200, 12), 100);
    QCOMPARE(GridLayout::cellWidth(1000, 12), 83);
    QCOMPARE(GridLayout::cellWidth(100, 0), 0);
}

void GridLayout_Test::alignTracks_data()
{
    QTest::addColumn<LayoutSnapshot>("snapshot");
    QTest::addColumn<LayoutOptions>("options");
    QTest::addColumn<QVector<QRectF>>("expected");
    QTest::addColumn<QVector<qreal>>("widths");
    QTest::addColumn<QVector<qreal>>("heights");

    LayoutOptions options;
    options.padding = 5;
    options.gap = 10;
    QTest::newRow("tall sliders wide buttons square pads")
        << panel({QRectF(12, 15, 20, 120), QRectF(45, 15, 80, 25),
                  QRectF(10, 170, 60, 60), QRectF(90, 170, 100, 20)}, QSizeF(200, 250))
        << options
        << QVector<QRectF>{QRectF(5, 5, 20, 120), QRectF(75, 5, 80, 25),
                           QRectF(5, 135, 60, 60), QRectF(75, 135, 100, 20)}
        << QVector<qreal>{60, 100} << QVector<qreal>{120, 60};
    QTest::newRow("split last row when track maxima exceed width")
        << panel({QRectF(0, 0, 80, 20), QRectF(90, 0, 20, 20),
                  QRectF(0, 40, 20, 20), QRectF(30, 40, 80, 20)}, QSizeF(130, 100))
        << options
        << QVector<QRectF>{QRectF(5, 5, 80, 20), QRectF(95, 5, 20, 20),
                           QRectF(5, 35, 20, 20), QRectF(5, 65, 80, 20)}
        << QVector<qreal>{80, 20} << QVector<qreal>{20, 20, 20};
    QTest::newRow("equal positions retain explicit order")
        << panel({QRectF(40, 40, 20, 20), QRectF(40, 40, 20, 20),
                  QRectF(40, 40, 20, 20)}, QSizeF(70, 100))
        << options
        << QVector<QRectF>{QRectF(5, 5, 20, 20), QRectF(35, 5, 20, 20), QRectF(5, 35, 20, 20)}
        << QVector<qreal>{20, 20} << QVector<qreal>{20, 20};
    QTest::newRow("conflicting geometry wraps stored order not y order")
        << panel({QRectF(80, 100, 20, 20), QRectF(0, 0, 20, 20),
                  QRectF(0, 100, 20, 20)}, QSizeF(100, 150))
        << options
        << QVector<QRectF>{QRectF(5, 5, 20, 20), QRectF(35, 5, 20, 20), QRectF(65, 5, 20, 20)}
        << QVector<qreal>{20, 20, 20} << QVector<qreal>{20};
    QTest::newRow("fixed row anchor not transitive or tall intersection")
        << panel({QRectF(0, 0, 20, 120), QRectF(30, 0.75, 20, 20),
                  QRectF(60, 1.5, 20, 20)}, QSizeF(100, 200))
        << options
        << QVector<QRectF>{QRectF(5, 5, 20, 120), QRectF(35, 5, 20, 20), QRectF(5, 135, 20, 20)}
        << QVector<qreal>{20, 20} << QVector<qreal>{120, 20};
    QTest::newRow("explicit column cap preserves forward sequence")
        << panel({QRectF(0, 0, 20, 20), QRectF(30, 0, 20, 20), QRectF(60, 0, 20, 20)},
                 QSizeF(100, 100), 2)
        << options
        << QVector<QRectF>{QRectF(5, 5, 20, 20), QRectF(35, 5, 20, 20), QRectF(5, 35, 20, 20)}
        << QVector<qreal>{20, 20} << QVector<qreal>{20, 20};
    LayoutSnapshot disabled = panel({QRectF(0, 0, 17.5, 23.5), QRectF(40, 0, 21.5, 9.5)},
                                    QSizeF(100, 100));
    disabled.containers[0].pages[0].snapStep = QSizeF(std::numeric_limits<qreal>::quiet_NaN(), 0);
    options.padding = 3.5;
    options.gap = 7.5;
    QTest::newRow("disabled snapping preserves fractional values")
        << disabled << options
        << QVector<QRectF>{QRectF(3.5, 3.5, 17.5, 23.5), QRectF(28.5, 3.5, 21.5, 9.5)}
        << QVector<qreal>{17.5, 21.5} << QVector<qreal>{23.5};
    LayoutSnapshot snapped = panel({QRectF(0, 0, 17, 23), QRectF(40, 0, 21, 9),
                                   QRectF(0, 50, 17, 11)}, QSizeF(100, 100));
    snapped.containers[0].pages[0].snappingEnabled = true;
    snapped.containers[0].pages[0].snapStep = QSizeF(10, 10);
    options.padding = 3;
    options.gap = 7;
    QTest::newRow("snap boundaries once without resizing leaves")
        << snapped << options
        << QVector<QRectF>{QRectF(10, 10, 17, 23), QRectF(40, 10, 21, 9), QRectF(10, 50, 17, 11)}
        << QVector<qreal>{20, 30} << QVector<qreal>{30, 20};
    options = {};
    QTest::newRow("thin adjacent rows remain idempotent inside row tolerance")
        << panel({QRectF(0, 0, 20, 1), QRectF(0, 20, 20, 1)}, QSizeF(100, 100))
        << options
        << QVector<QRectF>{QRectF(0, 0, 20, 1), QRectF(0, 1, 20, 1)}
        << QVector<qreal>{20} << QVector<qreal>{1, 1};
}

void GridLayout_Test::alignTracks()
{
    QFETCH(LayoutSnapshot, snapshot);
    QFETCH(LayoutOptions, options);
    QFETCH(QVector<QRectF>, expected);
    QFETCH(QVector<qreal>, widths);
    QFETCH(QVector<qreal>, heights);
    const QByteArray before = snapshotValues(snapshot);
    const LayoutPlan plan = GridLayout::align(snapshot, options);
    QVERIFY2(plan.ok(), qPrintable(plan.error.message));
    verifyGeometry(snapshot, plan, options);
    QCOMPARE(plannedRectangles(snapshot, plan), expected);
    QCOMPARE(plan.tracks.size(), 1);
    QCOMPARE(plan.tracks[0].columnWidths, widths);
    QCOMPARE(plan.tracks[0].rowHeights, heights);
    QCOMPARE(snapshotValues(snapshot), before);
    const LayoutSnapshot arranged = withPlan(snapshot, plan);
    const LayoutPlan repeated = GridLayout::align(snapshot, options);
    QCOMPARE(planValues(repeated), planValues(plan));
    const LayoutPlan second = GridLayout::align(arranged, options);
    QVERIFY2(second.ok(), qPrintable(second.error.message));
    QVERIFY(second.placements.isEmpty());
    QVERIFY(second.growth.isEmpty());
    QCOMPARE(plannedRectangles(arranged, second), expected);
    for (const auto &placement : plan.placements)
    {
        const auto &item = snapshot.items[int(placement.id) - 1];
        QCOMPARE(placement.parentId, item.parentId);
        QCOMPARE(placement.subpage, item.subpage);
        QCOMPARE(placement.order, item.order);
        QCOMPARE(placement.rect.size(), item.rect.size());
    }
}

void GridLayout_Test::alignFailures_data()
{
    QTest::addColumn<LayoutSnapshot>("snapshot");
    QTest::addColumn<LayoutOptions>("options");
    QTest::addColumn<QString>("code");
    QTest::addColumn<quint32>("widgetId");

    QTest::newRow("impossible width diagnoses offending widget")
        << panel({QRectF(0, 0, 120, 20)}, QSizeF(100, 100))
        << LayoutOptions{} << QString("insufficientWidth") << quint32(1);
    QTest::newRow("height refusal returns no partial plan")
        << panel({QRectF(10, 40, 20, 120)}, QSizeF(100, 100))
        << LayoutOptions{} << QString("insufficientHeight") << quint32(1);

    const LayoutSnapshot valid = panel({QRectF(10, 10, 20, 20)}, QSizeF(100, 100));
    LayoutSnapshot invalid = valid;
    invalid.items.append(invalid.items[0]);
    QTest::newRow("duplicate IDs")
        << invalid << LayoutOptions{} << QString("duplicateId") << quint32(1);
    invalid = valid;
    invalid.items[0].parentId = 200;
    QTest::newRow("unknown parent")
        << invalid << LayoutOptions{} << QString("unknownParent") << quint32(1);
    invalid = valid;
    invalid.items[0].subpage = 2;
    QTest::newRow("inactive page must exist")
        << invalid << LayoutOptions{} << QString("invalidSubpage") << quint32(1);
    invalid = valid;
    invalid.items.append({2, 100, 0, 0, QRectF(40, 10, 20, 20), QSizeF(1, 1)});
    QTest::newRow("duplicate explicit order")
        << invalid << LayoutOptions{} << QString("invalidOrder") << quint32(2);
    invalid = valid;
    invalid.items[0].order = -1;
    QTest::newRow("negative explicit order")
        << invalid << LayoutOptions{} << QString("invalidOrder") << quint32(1);
    invalid = valid;
    invalid.items[0].rect.setX(std::numeric_limits<qreal>::quiet_NaN());
    QTest::newRow("nonfinite rectangle")
        << invalid << LayoutOptions{} << QString("invalidRectangle") << quint32(1);
    invalid = valid;
    invalid.items[0].rect.setWidth(qreal(std::numeric_limits<int>::max()) + 1);
    QTest::newRow("integer conversion overflow")
        << invalid << LayoutOptions{} << QString("invalidRectangle") << quint32(1);
    invalid = valid;
    invalid.items[0].minimumSize = QSizeF(25, 20);
    QTest::newRow("never shrink or stretch leaf to hide undersizing")
        << invalid << LayoutOptions{} << QString("belowMinimum") << quint32(1);
    invalid = valid;
    invalid.containers[0].pages[0].snappingEnabled = true;
    invalid.containers[0].pages[0].snapStep = QSizeF(0, 10);
    QTest::newRow("enabled snap requires positive finite steps")
        << invalid << LayoutOptions{} << QString("invalidSnap") << quint32(100);
    invalid = valid;
    invalid.containers[0].pages[0].contentRect.setX(-1);
    QTest::newRow("content bounds outside outer container")
        << invalid << LayoutOptions{} << QString("invalidContent") << quint32(100);
    invalid = valid;
    invalid.containers[0].pages.append(invalid.containers[0].pages[0]);
    QTest::newRow("duplicate subpages")
        << invalid << LayoutOptions{} << QString("invalidSubpage") << quint32(100);
    invalid = valid;
    invalid.containers[0].pages.clear();
    QTest::newRow("no declared subpages")
        << invalid << LayoutOptions{} << QString("invalidSubpage") << quint32(100);
    invalid = valid;
    invalid.containers[0].minimumSize = QSizeF(101, 20);
    QTest::newRow("outer container below minimum")
        << invalid << LayoutOptions{} << QString("belowMinimum") << quint32(100);
    invalid = valid;
    invalid.containers.append(invalid.containers[0]);
    QTest::newRow("duplicate container ID")
        << invalid << LayoutOptions{} << QString("duplicateId") << quint32(100);
    invalid = valid;
    invalid.containers.append({1, QSizeF(21, 20), QSizeF(1, 1),
                               {LayoutPage{0, QRectF(0, 0, 21, 20)}}});
    QTest::newRow("frame item and container size disagreement")
        << invalid << LayoutOptions{} << QString("containerSizeMismatch") << quint32(1);
    invalid = valid;
    invalid.containers.append({2, QSizeF(20, 20), QSizeF(1, 1),
                               {LayoutPage{0, QRectF(0, 0, 20, 20)}}});
    QTest::newRow("orphan frame domain")
        << invalid << LayoutOptions{} << QString("unknownContainer") << quint32(2);
    invalid = valid;
    invalid.items[0].parentId = 1;
    invalid.containers.append({1, QSizeF(20, 20), QSizeF(1, 1),
                               {LayoutPage{0, QRectF(0, 0, 20, 20)}}});
    QTest::newRow("disconnected self cycle")
        << invalid << LayoutOptions{} << QString("ownershipCycle") << quint32(1);
    LayoutOptions badOptions;
    badOptions.padding = std::numeric_limits<qreal>::infinity();
    QTest::newRow("nonfinite explicit options")
        << valid << badOptions << QString("invalidOptions") << InvalidId;
    invalid = panel({QRectF(0, 0, 20, 20), QRectF(30, 0, 20, 20)}, QSizeF(100, 100));
    badOptions = {};
    badOptions.gap = std::numeric_limits<int>::max();
    badOptions.allowHeightGrowth = true;
    QTest::newRow("accumulated track overflow")
        << invalid << badOptions << QString("arithmeticOverflow") << quint32(100);
    invalid = panel({QRectF(0, 0, 81, 20)}, QSizeF(100, 100));
    invalid.containers[0].pages[0].snappingEnabled = true;
    invalid.containers[0].pages[0].snapStep = QSizeF(20, 20);
    badOptions = {};
    badOptions.padding = 1;
    QTest::newRow("snapped far edge cannot clip")
        << invalid << badOptions << QString("insufficientWidth") << quint32(1);
}

void GridLayout_Test::alignFailures()
{
    QFETCH(LayoutSnapshot, snapshot);
    QFETCH(LayoutOptions, options);
    QFETCH(QString, code);
    QFETCH(quint32, widgetId);
    const QByteArray before = snapshotValues(snapshot);
    const LayoutPlan plan = GridLayout::align(snapshot, options);
    QVERIFY(!plan.ok());
    QCOMPARE(plan.error.code, code);
    QCOMPARE(plan.error.widgetId, widgetId);
    QVERIFY(!plan.error.message.isEmpty());
    QVERIFY(plan.placements.isEmpty());
    QVERIFY(plan.growth.isEmpty());
    QVERIFY(plan.tracks.isEmpty());
    QCOMPARE(snapshotValues(snapshot), before);
}

void GridLayout_Test::alignHierarchy_data()
{
    QTest::addColumn<LayoutSnapshot>("snapshot");
    QTest::addColumn<LayoutOptions>("options");
    QTest::addColumn<QVector<QRectF>>("expected");
    QTest::addColumn<QVector<QSizeF>>("sizes");

    LayoutSnapshot snapshot = panel({QRectF(20, 20, 120, 70), QRectF(160, 20, 80, 90)},
                                    QSizeF(300, 220));
    LayoutPage first;
    first.contentRect = QRectF(5, 20, 110, 45);
    LayoutPage inactive = first;
    inactive.subpage = 1;
    snapshot.containers.append({1, QSizeF(120, 70), QSizeF(30, 30), {first, inactive}});
    LayoutPage second;
    second.contentRect = QRectF(5, 20, 70, 65);
    snapshot.containers.append({2, QSizeF(80, 90), QSizeF(30, 30), {second}});
    snapshot.items.append({3, 1, 0, 0, QRectF(15, 30, 20, 20), QSizeF(20, 20)});
    snapshot.items.append({4, 1, 0, 1, QRectF(50, 30, 60, 60), QSizeF(60, 60)});
    snapshot.items.append({5, 1, 1, 0, QRectF(15, 30, 20, 120), QSizeF(20, 100)});
    snapshot.items.append({6, 2, 0, 0, QRectF(20, 35, 50, 20), QSizeF(30, 20)});
    LayoutOptions options;
    options.padding = 5;
    options.gap = 10;
    options.allowHeightGrowth = true;
    QTest::newRow("unequal nested frames inactive page maximum")
        << snapshot << options
        << QVector<QRectF>{QRectF(5, 5, 120, 155), QRectF(135, 5, 80, 90),
                           QRectF(10, 25, 20, 20), QRectF(40, 25, 60, 60),
                           QRectF(10, 25, 20, 120), QRectF(10, 25, 50, 20)}
        << QVector<QSizeF>{QSizeF(120, 155)};

    LayoutSnapshot empty = panel({QRectF(50, 50, 80, 90)}, QSizeF(200, 200));
    empty.containers.append({1, QSizeF(80, 90), QSizeF(30, 30), {second}});
    QTest::newRow("empty frame never collapses")
        << empty << options << QVector<QRectF>{QRectF(5, 5, 80, 90)}
        << QVector<QSizeF>{};

    LayoutSnapshot wide = panel({QRectF(10, 10, 120, 20)}, QSizeF(100, 60));
    options.allowWidthGrowth = true;
    QTest::newRow("explicit root width growth")
        << wide << options << QVector<QRectF>{QRectF(5, 5, 120, 20)}
        << QVector<QSizeF>{QSizeF(130, 60)};
    LayoutSnapshot deep = panel({QRectF(20, 20, 120, 80)}, QSizeF(300, 300));
    LayoutPage outer;
    outer.contentRect = QRectF(5, 20, 110, 55);
    deep.containers.append({1, QSizeF(120, 80), QSizeF(1, 1), {outer}});
    deep.items.append({2, 1, 0, 0, QRectF(15, 30, 90, 50), QSizeF(1, 1)});
    LayoutPage inner;
    inner.contentRect = QRectF(5, 20, 80, 25);
    deep.containers.append({2, QSizeF(90, 50), QSizeF(1, 1), {inner}});
    deep.items.append({3, 2, 0, 0, QRectF(15, 30, 20, 120), QSizeF(1, 1)});
    QTest::newRow("two-level measurement propagates growth to ancestors")
        << deep << options
        << QVector<QRectF>{QRectF(5, 5, 120, 190), QRectF(10, 25, 90, 155), QRectF(10, 25, 20, 120)}
        << QVector<QSizeF>{QSizeF(120, 190), QSizeF(90, 155)};
}

void GridLayout_Test::alignHierarchy()
{
    QFETCH(LayoutSnapshot, snapshot);
    QFETCH(LayoutOptions, options);
    QFETCH(QVector<QRectF>, expected);
    QFETCH(QVector<QSizeF>, sizes);
    const QByteArray before = snapshotValues(snapshot);
    const LayoutPlan plan = GridLayout::align(snapshot, options);
    QVERIFY2(plan.ok(), qPrintable(plan.error.message));
    verifyGeometry(snapshot, plan, options);
    QCOMPARE(plannedRectangles(snapshot, plan), expected);
    QVector<QSizeF> actualSizes;
    for (const auto &growth : plan.growth)
        actualSizes.append(growth.size);
    QCOMPARE(actualSizes, sizes);
    QCOMPARE(snapshotValues(snapshot), before);
    LayoutSnapshot reordered = snapshot;
    std::reverse(reordered.items.begin(), reordered.items.end());
    std::reverse(reordered.containers.begin(), reordered.containers.end());
    for (auto &container : reordered.containers)
        std::reverse(container.pages.begin(), container.pages.end());
    const LayoutPlan reorderedPlan = GridLayout::align(reordered, options);
    QCOMPARE(planValues(reorderedPlan), planValues(plan));
    const LayoutSnapshot arranged = withPlan(snapshot, plan);
    const LayoutPlan second = GridLayout::align(arranged, options);
    QVERIFY2(second.ok(), qPrintable(second.error.message));
    QVERIFY(second.placements.isEmpty());
    QVERIFY(second.growth.isEmpty());
    QCOMPARE(plannedRectangles(arranged, second), expected);
    for (const auto &placement : plan.placements)
    {
        const auto &item = snapshot.items[int(placement.id) - 1];
        QCOMPARE(placement.parentId, item.parentId);
        QCOMPARE(placement.subpage, item.subpage);
        QCOMPARE(placement.order, item.order);
    }
}

void GridLayout_Test::alignBands_data()
{
    QTest::addColumn<LayoutSnapshot>("snapshot");
    QTest::addColumn<QString>("code");
    QTest::addColumn<QVector<QRectF>>("expected");

    LayoutSnapshot snapshot = panel({QRectF(0, 0, 100, 50), QRectF(0, 70, 80, 30),
                                    QRectF(130, 0, 70, 60), QRectF(130, 90, 70, 20)},
                                   QSizeF(240, 200));
    snapshot.containers[0].pages[0].preserveColumnBands = true;
    QTest::newRow("disjoint page bands preserve contiguous stored order")
        << snapshot << QString{}
        << QVector<QRectF>{QRectF(5, 5, 100, 50), QRectF(5, 65, 80, 30),
                           QRectF(115, 5, 70, 60), QRectF(115, 75, 70, 20)};
    LayoutSnapshot snapped = snapshot;
    snapped.containers[0].pages[0].snappingEnabled = true;
    snapped.containers[0].pages[0].snapStep = QSizeF(10, 10);
    QTest::newRow("snapped page bands include leading boundary slack")
        << snapped << QString{}
        << QVector<QRectF>{QRectF(10, 10, 100, 50), QRectF(10, 70, 80, 30),
                           QRectF(120, 10, 70, 60), QRectF(120, 80, 70, 20)};
    snapshot.items[1].order = 2;
    snapshot.items[2].order = 1;
    QTest::newRow("interleaved stored order refuses column inference")
        << snapshot << QString("conflictingColumnOrder") << QVector<QRectF>{};
    snapshot = panel({QRectF(0, 0, 80, 20), QRectF(150, 0, 80, 20), QRectF(0, 50, 230, 20)},
                     QSizeF(250, 150));
    snapshot.containers[0].pages[0].preserveColumnBands = true;
    QTest::newRow("spanning item refuses ambiguous page bands")
        << snapshot << QString("ambiguousColumns") << QVector<QRectF>{};
}

void GridLayout_Test::alignBands()
{
    QFETCH(LayoutSnapshot, snapshot);
    QFETCH(QString, code);
    QFETCH(QVector<QRectF>, expected);
    LayoutOptions options;
    options.padding = 5;
    options.gap = 10;
    const QByteArray before = snapshotValues(snapshot);
    const LayoutPlan plan = GridLayout::align(snapshot, options);
    QCOMPARE(plan.error.code, code);
    QCOMPARE(snapshotValues(snapshot), before);
    if (!code.isEmpty())
    {
        QVERIFY(plan.placements.isEmpty());
        return;
    }
    QCOMPARE(plannedRectangles(snapshot, plan), expected);
    verifyGeometry(snapshot, plan, options);
    QCOMPARE(plan.tracks.size(), 2);
    QCOMPARE(plan.tracks[0].columnWidths, QVector<qreal>{100});
    QCOMPARE(plan.tracks[1].columnWidths, QVector<qreal>{70});
    const LayoutSnapshot arranged = withPlan(snapshot, plan);
    const LayoutPlan second = GridLayout::align(arranged, options);
    QVERIFY2(second.ok(), qPrintable(second.error.message));
    QVERIFY(second.placements.isEmpty());
    QVERIFY(second.growth.isEmpty());
}

void GridLayout_Test::compactPlans_data()
{
    QTest::addColumn<LayoutSnapshot>("snapshot");
    QTest::addColumn<LayoutOptions>("options");
    QTest::addColumn<QVector<QRectF>>("expected");

    LayoutSnapshot snapshot = panel({QRectF(0, 60, 140, 84), QRectF(100, 200, 100, 60)},
                                   QSizeF(400, 300));
    snapshot.containers[0].pages[0].snappingEnabled = true;
    snapshot.containers[0].pages[0].snapStep = QSizeF(100, 60);
    QTest::newRow("fractional grid occupancy does not overlap restored dimensions")
        << snapshot << LayoutOptions{}
        << QVector<QRectF>{QRectF(0, 0, 140, 84), QRectF(100, 120, 100, 60)};
    snapshot = panel({QRectF(3.5, 70, 17.5, 23.5), QRectF(12, 110, 21.5, 9.5),
                      QRectF(70, 60, 15, 50)}, QSizeF(100, 150));
    snapshot.containers[0].pages[0].contentRect = QRectF(2, 20, 96, 125);
    LayoutOptions options;
    options.padding = 1.5;
    options.gap = 2.5;
    QTest::newRow("disabled snapping retains x and leaf sizes below header")
        << snapshot << options
        << QVector<QRectF>{QRectF(3.5, 21.5, 17.5, 23.5), QRectF(12, 47.5, 21.5, 9.5),
                           QRectF(70, 21.5, 15, 50)};
}

void GridLayout_Test::compactPlans()
{
    QFETCH(LayoutSnapshot, snapshot);
    QFETCH(LayoutOptions, options);
    QFETCH(QVector<QRectF>, expected);
    const QByteArray before = snapshotValues(snapshot);
    const LayoutPlan plan = GridLayout::compact(snapshot, options);
    QVERIFY2(plan.ok(), qPrintable(plan.error.message));
    verifyGeometry(snapshot, plan, options);
    QCOMPARE(plannedRectangles(snapshot, plan), expected);
    QCOMPARE(snapshotValues(snapshot), before);
    const LayoutSnapshot arranged = withPlan(snapshot, plan);
    const LayoutPlan second = GridLayout::compact(arranged, options);
    QVERIFY2(second.ok(), qPrintable(second.error.message));
    QVERIFY(second.placements.isEmpty());
    QVERIFY(second.growth.isEmpty());
}

void GridLayout_Test::fractionalPlans_data()
{
    QTest::addColumn<LayoutSnapshot>("snapshot");
    QTest::addColumn<LayoutOptions>("options");
    QTest::addColumn<bool>("compactPolicy");
    QTest::addColumn<QVector<QRectF>>("expected");
    QTest::addColumn<QString>("code");

    LayoutSnapshot snapshot = panel({QRectF(0.25, 0, 1, 1)}, QSizeF(10, 10));
    snapshot.containers[0].pages[0].snappingEnabled = true;
    snapshot.containers[0].pages[0].snapStep = QSizeF(0.1, 0.1);
    QTest::newRow("decimal snapped compact position remains fixed")
        << snapshot << LayoutOptions{} << true
        << QVector<QRectF>{QRectF(0.3, 0, 1, 1)} << QString{};

    snapshot = panel({QRectF(0, 0, 0.3, 0.3)}, QSizeF(0.5, 0.5));
    snapshot.items[0].minimumSize = QSizeF(0.1, 0.1);
    snapshot.containers[0].minimumSize = QSizeF(0.1, 0.1);
    LayoutOptions options;
    options.padding = 0.1;
    QTest::newRow("decimal padded align exact fit")
        << snapshot << options << false
        << QVector<QRectF>{QRectF(0.1, 0.1, 0.3, 0.3)} << QString{};
    snapshot.containers[0].pages[0].snappingEnabled = true;
    snapshot.containers[0].pages[0].snapStep = QSizeF(0.1, 0.1);
    QTest::newRow("decimal padded snapped align exact fit")
        << snapshot << options << false
        << QVector<QRectF>{QRectF(0.1, 0.1, 0.3, 0.3)} << QString{};
    snapshot.containers[0].pages[0].contentRect.setWidth(0.5 - 1e-8);
    QTest::newRow("meaningful fractional width deficit still refuses")
        << snapshot << options << false << QVector<QRectF>{} << QString("insufficientWidth");

    snapshot = panel({QRectF(5, 5, 22.3, 10)}, QSizeF(32.3, 50));
    snapshot.containers[0].minimumSize = QSizeF(10, 10);
    options.padding = 5;
    options.gap = 5;
    QTest::newRow("review exact fit at production-scale decimal width")
        << snapshot << options << false
        << QVector<QRectF>{QRectF(5, 5, 22.3, 10)} << QString{};
    snapshot = panel({QRectF(38, 10, 20, 10)}, QSizeF(400, 300));
    snapshot.containers[0].pages[0].snappingEnabled = true;
    snapshot.containers[0].pages[0].snapStep = QSizeF(2.1 * 3, 2.1 * 3);
    QTest::newRow("review pixel-density step compact repeat")
        << snapshot << options << true
        << QVector<QRectF>{QRectF(44.1, 6.3, 20, 10)} << QString{};
    snapshot = panel({QRectF(0, 0, 37, 10), QRectF(40, 0, 69.30000000000001, 10),
                      QRectF(120, 0, 63.00000000000001, 10)}, QSizeF(400, 100));
    snapshot.containers[0].pages[0].snappingEnabled = true;
    snapshot.containers[0].pages[0].snapStep = QSizeF(2.1 * 3, 2.1 * 3);
    options = {};
    QTest::newRow("review adjacent snapped tracks tolerate one ULP")
        << snapshot << options << false
        << QVector<QRectF>{QRectF(0, 0, 37, 10), QRectF(37.8, 0, 69.3, 10), QRectF(107.1, 0, 63, 10)}
        << QString{};
}

void GridLayout_Test::fractionalPlans()
{
    QFETCH(LayoutSnapshot, snapshot);
    QFETCH(LayoutOptions, options);
    QFETCH(bool, compactPolicy);
    QFETCH(QVector<QRectF>, expected);
    QFETCH(QString, code);
    const QByteArray before = snapshotValues(snapshot);
    const LayoutPlan plan = compactPolicy ? GridLayout::compact(snapshot, options)
                                         : GridLayout::align(snapshot, options);
    QCOMPARE(plan.error.code, code);
    QCOMPARE(snapshotValues(snapshot), before);
    if (!code.isEmpty())
    {
        QVERIFY(plan.placements.isEmpty());
        QVERIFY(plan.growth.isEmpty());
        return;
    }
    QCOMPARE(plannedRectangles(snapshot, plan), expected);
    QVERIFY(plan.growth.isEmpty());
    const LayoutSnapshot arranged = withPlan(snapshot, plan);
    const LayoutPlan second = compactPolicy ? GridLayout::compact(arranged, options)
                                           : GridLayout::align(arranged, options);
    QVERIFY2(second.ok(), qPrintable(second.error.message));
    QVERIFY(second.placements.isEmpty());
    QVERIFY(second.growth.isEmpty());
}

void GridLayout_Test::validatePlans_data()
{
    QTest::addColumn<LayoutSnapshot>("snapshot");
    QTest::addColumn<LayoutPlan>("plan");
    QTest::addColumn<LayoutOptions>("options");
    QTest::addColumn<QString>("code");

    const LayoutSnapshot snapshot = panel({QRectF(0, 0, 0.2, 0.3), QRectF(0.3, 0, 0.3, 0.3)},
                                         QSizeF(1, 1));
    LayoutSnapshot fractional = snapshot;
    for (auto &item : fractional.items)
        item.minimumSize = QSizeF(0.1, 0.1);
    LayoutPlan plan;
    plan.placements.append({1, 100, 0, 0, QRectF(0.1, 0.1, 0.2, 0.3)});
    plan.placements.append({2, 100, 0, 1, QRectF(0.3, 0.1, 0.3, 0.3)});
    QTest::newRow("ULP touching edges remain adjacent")
        << fractional << plan << LayoutOptions{} << QString{};
    plan.placements[1].rect.moveLeft(0.3 - 1e-8);
    QTest::newRow("meaningful fractional overlap refuses")
        << fractional << plan << LayoutOptions{} << QString("overlappingWidgets");
    plan.placements[1].rect.moveLeft(0.3);
    plan.placements[0].rect.moveLeft(-1e-8);
    QTest::newRow("meaningful fractional bounds overflow refuses")
        << fractional << plan << LayoutOptions{} << QString("outsideContent");
    plan.placements[0].rect.moveLeft(0.1);
    plan.placements[0].order = 1;
    QTest::newRow("identity metadata cannot change")
        << fractional << plan << LayoutOptions{} << QString("planMetadataMismatch");
    plan.placements[0].order = 0;
    plan.placements.append(plan.placements[0]);
    QTest::newRow("duplicate placement refuses")
        << fractional << plan << LayoutOptions{} << QString("duplicatePlacement");
    plan.placements.removeLast();
    plan.placements[0].rect.setWidth(0.2 + 1e-8);
    QTest::newRow("leaf size cannot change")
        << fractional << plan << LayoutOptions{} << QString("unexpectedResize");
    plan.placements[0].rect.setWidth(0.2);
    plan.growth.append({100, QSizeF(2, 1)});
    QTest::newRow("unapproved width growth refuses")
        << fractional << plan << LayoutOptions{} << QString("invalidGrowth");
    LayoutOptions options;
    options.allowWidthGrowth = true;
    QTest::newRow("approved explicit root growth validates")
        << fractional << plan << options << QString{};
    plan.growth[0].size = QSizeF(0.5, 1);
    QTest::newRow("growth cannot shrink container")
        << fractional << plan << options << QString("invalidGrowth");
    plan.growth.clear();
    fractional.items[0].minimumSize = QSizeF();
    QTest::newRow("default minimum is not measured")
        << fractional << plan << options << QString("invalidRectangle");
}

void GridLayout_Test::validatePlans()
{
    QFETCH(LayoutSnapshot, snapshot);
    QFETCH(LayoutPlan, plan);
    QFETCH(LayoutOptions, options);
    QFETCH(QString, code);
    const QByteArray beforeSnapshot = snapshotValues(snapshot);
    const QByteArray beforePlan = planValues(plan);
    const Diagnostic diagnostic = GridLayout::validate(snapshot, plan, options);
    QCOMPARE(diagnostic.code, code);
    QCOMPARE(snapshotValues(snapshot), beforeSnapshot);
    QCOMPARE(planValues(plan), beforePlan);
    if (!code.isEmpty())
        QVERIFY(!diagnostic.message.isEmpty());
}

void GridLayout_Test::arrange_data()
{
    QTest::addColumn<int>("mode");
    QTest::addColumn<bool>("fill");
    QTest::addColumn<int>("columns");
    QTest::addColumn<QRectF>("content");
    QTest::addColumn<QList<QSizeF>>("sizes");
    QTest::addColumn<QList<QRectF>>("expected"); // empty: refused
    using M = GridLayout::ArrangeMode;
    const QList<QSizeF> two {QSizeF(100, 80), QSizeF(50, 60)};
    const QList<QSizeF> four(4, QSizeF(50, 50));
    QTest::newRow("row") << int(M::Row) << false << 0 << QRectF(0, 30, 500, 200) << two
                         << QList<QRectF>{QRectF(10, 40, 100, 80), QRectF(120, 40, 50, 60)};
    QTest::newRow("row-fill") << int(M::Row) << true << 0 << QRectF(0, 30, 500, 200) << two
                              << QList<QRectF>{QRectF(10, 40, 235, 180), QRectF(255, 40, 235, 180)};
    QTest::newRow("column") << int(M::Column) << false << 0 << QRectF(0, 30, 500, 200) << two
                            << QList<QRectF>{QRectF(10, 40, 100, 80), QRectF(10, 130, 50, 60)};
    QTest::newRow("column-fill") << int(M::Column) << true << 0 << QRectF(0, 30, 500, 200) << two
                                 << QList<QRectF>{QRectF(10, 40, 480, 85), QRectF(10, 135, 480, 85)};
    QTest::newRow("grid") << int(M::Grid) << false << 2 << QRectF(0, 0, 500, 500) << four
                          << QList<QRectF>{QRectF(10, 10, 50, 50), QRectF(70, 10, 50, 50),
                                           QRectF(10, 70, 50, 50), QRectF(70, 70, 50, 50)};
    QTest::newRow("grid-fill") << int(M::Grid) << true << 2 << QRectF(0, 0, 210, 110) << four
                               << QList<QRectF>{QRectF(10, 10, 90, 40), QRectF(110, 10, 90, 40),
                                                QRectF(10, 60, 90, 40), QRectF(110, 60, 90, 40)};
    // cells of 21.67 px get whole pixel edges, so neighbours keep the gap
    QTest::newRow("grid-fill-rounds") << int(M::Grid) << true << 3 << QRectF(0, 0, 105, 40) << QList<QSizeF>(3, QSizeF(10, 10))
                                      << QList<QRectF>{QRectF(10, 10, 22, 20), QRectF(42, 10, 21, 20), QRectF(73, 10, 22, 20)};
    QTest::newRow("flow") << int(M::Flow) << false << 0 << QRectF(0, 0, 200, 200) << QList<QSizeF>(3, QSizeF(80, 40))
                          << QList<QRectF>{QRectF(10, 10, 80, 40), QRectF(100, 10, 80, 40), QRectF(10, 60, 80, 40)};
    QTest::newRow("too-small-fill") << int(M::Row) << true << 0 << QRectF(0, 0, 60, 100) << two << QList<QRectF>();
    QTest::newRow("too-wide") << int(M::Row) << false << 0 << QRectF(0, 0, 150, 100) << two << QList<QRectF>();
    QTest::newRow("too-tall-flow") << int(M::Flow) << false << 0 << QRectF(0, 0, 100, 100) << two << QList<QRectF>();
    QTest::newRow("empty") << int(M::Grid) << true << 2 << QRectF(0, 0, 100, 100) << QList<QSizeF>() << QList<QRectF>();
}

void GridLayout_Test::arrange()
{
    QFETCH(int, mode);
    QFETCH(bool, fill);
    QFETCH(int, columns);
    QFETCH(QRectF, content);
    QFETCH(QList<QSizeF>, sizes);
    QFETCH(QList<QRectF>, expected);
    GridLayout::ArrangeSpec spec;
    spec.mode = GridLayout::ArrangeMode(mode);
    spec.gap = 10;
    spec.columns = columns;
    spec.fill = fill;

    const GridLayout::ArrangeResult result = GridLayout::arrange(spec, content, sizes, QList<QSizeF>(sizes.size(), QSizeF(20, 20)));
    QCOMPARE(result.rects, expected);
    QCOMPARE(result.error.isEmpty(), !expected.isEmpty() || sizes.isEmpty());
}

QTEST_MAIN(GridLayout_Test)
