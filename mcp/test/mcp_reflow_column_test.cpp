/*
  Q Light Controller Plus - Unit test
  mcp_reflow_column_test.cpp

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

#include "mcp_reflow_column_test.h"

#include <QtTest>
#include <QList>
#include <QRect>
#include <QSet>

#include "vcbridge.h"

using WidgetSnapshot = VCBridge::WidgetSnapshot;
using ReflowOptions  = VCBridge::ReflowOptions;
using LayoutPlan     = VCBridge::LayoutPlan;

namespace {

// Child tuple: x, width, y, height
using ChildSpec = std::tuple<int, int, int, int>;

WidgetSnapshot buildPage(const QList<ChildSpec> &specs)
{
    WidgetSnapshot page;
    page.id = 1000;
    page.parentID = -1;
    page.type = 4;                // FrameWidget
    page.showHeader = false;
    page.geometry = QRect(0, 0, 800, 600);

    int nextId = 1;
    for (const ChildSpec &s : specs)
    {
        WidgetSnapshot c;
        c.id = nextId++;
        c.parentID = page.id;
        c.type = 1;                // ButtonWidget (leaf, no recursion into children)
        c.showHeader = false;
        c.geometry = QRect(std::get<0>(s), std::get<2>(s),
                           std::get<1>(s), std::get<3>(s));
        page.children.append(c);
    }
    return page;
}

// Count resulting columns by distinct x-coordinates of top-level children
// after reflowPage has run. reflowPage aligns all children in a column to
// the same x, so distinct x's == column count.
int countColumns(const WidgetSnapshot &page)
{
    QSet<int> xs;
    for (const WidgetSnapshot &c : page.children)
        xs.insert(c.geometry.x());
    return xs.size();
}

} // namespace

Q_DECLARE_METATYPE(QList<ChildSpec>)

void McpReflowColumnTest::columnDetection_data()
{
    QTest::addColumn<QList<ChildSpec>>("specs");
    QTest::addColumn<int>("overlapTolerance"); // -1 = use default (0 → auto)
    QTest::addColumn<int>("expectedColumns");

    // 1: empty
    QTest::newRow("01-empty")
        << QList<ChildSpec>{}
        << -1 << 0;

    // 2: single widget
    QTest::newRow("02-single")
        << QList<ChildSpec>{ {0, 100, 0, 60} }
        << -1 << 1;

    // 3: three stacked at x=0
    QTest::newRow("03-stacked-same-x")
        << QList<ChildSpec>{ {0, 100, 0, 60}, {0, 100, 60, 60}, {0, 100, 120, 60} }
        << -1 << 1;

    // 4: 1px overlap — should split (default tol=10)
    QTest::newRow("04-1px-overlap-splits")
        << QList<ChildSpec>{ {0, 200, 0, 60}, {199, 200, 0, 60} }
        << -1 << 2;

    // 5: clear 10px gap
    QTest::newRow("05-clear-gap")
        << QList<ChildSpec>{ {0, 200, 0, 60}, {210, 200, 0, 60} }
        << -1 << 2;

    // 6: staggered chain with 10px overlaps — with tol>10 these stay separate
    QTest::newRow("06-staggered-chain")
        << QList<ChildSpec>{ {0, 100, 0, 60}, {90, 100, 60, 60},
                             {180, 100, 120, 60}, {270, 100, 180, 60} }
        << 15 << 4;

    // 7: 50% overlaps — clearly one column (sloppy)
    QTest::newRow("07-50pct-overlap")
        << QList<ChildSpec>{ {0, 200, 0, 60}, {100, 200, 60, 60}, {200, 200, 120, 60} }
        << -1 << 1;

    // 8: wide spanner contains the others
    QTest::newRow("08-wide-spanner")
        << QList<ChildSpec>{ {0, 400, 0, 60}, {0, 100, 60, 60}, {300, 100, 60, 60} }
        << -1 << 1;

    // 9: no overlap, clear 200px gap
    QTest::newRow("09-no-overlap")
        << QList<ChildSpec>{ {0, 100, 0, 60}, {300, 100, 0, 60} }
        << -1 << 2;

    // 10: sloppy same-column (x jitter of a few px)
    QTest::newRow("10-sloppy-same-column")
        << QList<ChildSpec>{ {10, 100, 0, 60}, {12, 100, 60, 60}, {9, 100, 120, 60} }
        << -1 << 1;

    // 11: classic 2-column layout
    QTest::newRow("11-classic-2col")
        << QList<ChildSpec>{ {0, 100, 0, 60}, {0, 100, 60, 60},
                             {300, 100, 0, 60}, {300, 100, 60, 60} }
        << -1 << 2;

    // 12: partial overlap — first two overlap 50px, third is isolated
    QTest::newRow("12-partial-overlap")
        << QList<ChildSpec>{ {0, 200, 0, 60}, {150, 200, 60, 60}, {400, 200, 0, 60} }
        << -1 << 2;
}

void McpReflowColumnTest::columnDetection()
{
    QFETCH(QList<ChildSpec>, specs);
    QFETCH(int, overlapTolerance);
    QFETCH(int, expectedColumns);

    WidgetSnapshot page = buildPage(specs);

    ReflowOptions opts;
    if (overlapTolerance >= 0)
        opts.overlapTolerance = overlapTolerance;

    VCBridge::reflowPage(page, opts);

    const int actualColumns = countColumns(page);
    QCOMPARE(actualColumns, expectedColumns);
}

QTEST_MAIN(McpReflowColumnTest)
