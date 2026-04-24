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

#include "gridlayout_test.h"
#include "gridlayout.h"

using namespace GridLayout;

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

QTEST_MAIN(GridLayout_Test)
