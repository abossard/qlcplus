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

#include <QHash>
#include <QQueue>
#include <QSet>
#include <algorithm>
#include <cmath>
#include <limits>

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
    c.w = cellW > 0 ? static_cast<int>(std::ceil(static_cast<double>(pixelRect.width()) / cellW)) : 0;
    c.h = cellH > 0 ? static_cast<int>(std::ceil(static_cast<double>(pixelRect.height()) / cellH)) : 0;
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

qreal arithmeticTolerance(qreal a, qreal b)
{
    return 8 * std::numeric_limits<qreal>::epsilon() * qMax(std::abs(a), std::abs(b));
}

bool less(qreal a, qreal b)
{
    return a < b && (!std::isfinite(a) || !std::isfinite(b)
                    || b - a > arithmeticTolerance(a, b));
}

qreal snapUp(qreal value, qreal step)
{
    const qreal units = value / step;
    const qreal nearest = std::round(units);
    // Keep an already-rounded grid multiple fixed without hiding a fractional step.
    const qreal tolerance = qMin(arithmeticTolerance(units, nearest), qreal(0.25));
    return (std::abs(units - nearest) <= tolerance ? nearest : std::ceil(units)) * step;
}

namespace {

using Rows = QVector<QVector<int>>;

LayoutPlan failure(const QString &code, const QString &message, quint32 widgetId = InvalidId,
                   quint32 parentId = InvalidId, qreal required = 0, qreal available = 0)
{
    LayoutPlan plan;
    plan.error = {code, message, widgetId, parentId, required, available};
    return plan;
}

bool layoutNumber(qreal value)
{
    return std::isfinite(value) && std::abs(value) <= std::numeric_limits<int>::max();
}

bool positiveSize(const QSizeF &size)
{
    return layoutNumber(size.width()) && layoutNumber(size.height())
        && size.width() > 0 && size.height() > 0;
}

bool finiteRect(const QRectF &rect)
{
    return layoutNumber(rect.x()) && layoutNumber(rect.y())
        && positiveSize(rect.size()) && layoutNumber(rect.x() + rect.width())
        && layoutNumber(rect.y() + rect.height());
}

qreal excess(qreal required, qreal available)
{
    return less(available, required) ? required - available : 0;
}

bool overlap(const QRectF &a, const QRectF &b)
{
    return less(qMax(a.x(), b.x()), qMin(a.right(), b.right()))
        && less(qMax(a.y(), b.y()), qMin(a.bottom(), b.bottom()));
}

bool sameSize(const QSizeF &a, const QSizeF &b)
{
    return !less(a.width(), b.width()) && !less(b.width(), a.width())
        && !less(a.height(), b.height()) && !less(b.height(), a.height());
}

Rows inferredRows(const QVector<LayoutItem> &items, qreal tolerance, int columns)
{
    Rows rows;
    qreal anchor = 0;
    qreal bottom = 0;
    for (int i = 0; i < items.size(); ++i)
    {
        const bool separated = !rows.isEmpty() && less(anchor, items[i].rect.y())
            && !less(items[i].rect.y(), bottom);
        if (!rows.isEmpty() && (less(items[i].rect.y(), anchor - tolerance)
            || (!separated && std::abs(items[i].rect.y() - anchor) <= tolerance
                && less(items[i].rect.x(), items[rows.last().last()].rect.x()))))
            return {};
        if (rows.isEmpty() || separated || std::abs(items[i].rect.y() - anchor) > tolerance
            || (columns > 0 && rows.last().size() >= columns))
        {
            rows.append(QVector<int>{});
            anchor = items[i].rect.y();
            bottom = items[i].rect.bottom();
        }
        rows.last().append(i);
        bottom = qMax(bottom, items[i].rect.bottom());
    }
    return rows;
}

Rows wrappedRows(const QVector<LayoutItem> &items, qreal width, qreal gap, int columns)
{
    Rows rows;
    qreal used = 0;
    for (int i = 0; i < items.size(); ++i)
    {
        const qreal next = used + (used > 0 ? gap : 0) + items[i].rect.width();
        if (rows.isEmpty() || less(width, next) || (columns > 0 && rows.last().size() >= columns))
        {
            rows.append(QVector<int>{});
            used = 0;
        }
        used += (rows.last().isEmpty() ? 0 : gap) + items[i].rect.width();
        rows.last().append(i);
    }
    return rows;
}

Tracks measureTracks(const QVector<LayoutItem> &items, const Rows &rows, quint32 parentId,
                     const LayoutPage &page, const QPointF &start, qreal gap)
{
    Tracks tracks;
    tracks.parentId = parentId;
    tracks.subpage = page.subpage;
    for (const auto &row : rows)
    {
        qreal height = 0;
        for (int column = 0; column < row.size(); ++column)
        {
            const QSizeF size = items[row[column]].rect.size();
            if (column >= tracks.columnWidths.size())
                tracks.columnWidths.append(size.width());
            else
                tracks.columnWidths[column] = qMax(tracks.columnWidths[column], size.width());
            height = qMax(height, size.height());
        }
        tracks.rowHeights.append(height);
    }
    if (page.snappingEnabled)
    {
        for (auto &width : tracks.columnWidths)
            width = snapUp(width, page.snapStep.width());
        for (auto &height : tracks.rowHeights)
            height = snapUp(height, page.snapStep.height());
    }
    qreal x = page.snappingEnabled ? snapUp(start.x(), page.snapStep.width()) : start.x();
    for (qreal width : tracks.columnWidths)
    {
        tracks.columnStarts.append(x);
        x += width + gap;
        if (page.snappingEnabled)
            x = snapUp(x, page.snapStep.width());
    }
    qreal y = page.snappingEnabled ? snapUp(start.y(), page.snapStep.height()) : start.y();
    for (qreal height : tracks.rowHeights)
    {
        tracks.rowStarts.append(y);
        y += height + gap;
        if (page.snappingEnabled)
            y = snapUp(y, page.snapStep.height());
    }
    return tracks;
}

enum class Policy { Align, Compact };

class Calculation
{
public:
    Calculation(const LayoutSnapshot &snapshot, const LayoutOptions &options)
        : m_snapshot(snapshot), m_options(options)
    {
        for (int i = 0; i < snapshot.items.size(); ++i)
        {
            m_items.insert(snapshot.items[i].id, i);
            m_rectangles.insert(snapshot.items[i].id, snapshot.items[i].rect);
        }
        for (int i = 0; i < snapshot.containers.size(); ++i)
            m_containers.insert(snapshot.containers[i].id, i);
    }

    LayoutPlan run(Policy policy)
    {
        validateInputs();
        if (!m_plan.ok())
            return failure(m_plan.error.code, m_plan.error.message, m_plan.error.widgetId,
                           m_plan.error.parentId, m_plan.error.required, m_plan.error.available);
        calculateContainer(m_snapshot.rootId, policy);
        if (m_plan.ok())
        {
            appendResults(m_snapshot.rootId);
            m_plan.error = GridLayout::validate(m_snapshot, m_plan, m_options);
        }
        if (!m_plan.ok())
            return failure(m_plan.error.code, m_plan.error.message, m_plan.error.widgetId,
                           m_plan.error.parentId, m_plan.error.required, m_plan.error.available);
        return m_plan;
    }

    Diagnostic validatePlan(const LayoutPlan &plan)
    {
        if (!plan.ok())
            return plan.error;
        validateInputs();
        if (!m_plan.ok())
            return m_plan.error;
        for (const auto &container : m_snapshot.containers)
            m_sizes.insert(container.id, container.size);
        QSet<quint32> placed;
        for (const auto &placement : plan.placements)
        {
            if (!m_items.contains(placement.id))
            {
                reject("planMetadataMismatch", "Layout plan references an unknown widget.",
                       placement.id, placement.parentId);
                return m_plan.error;
            }
            const auto &item = m_snapshot.items[m_items.value(placement.id)];
            if (item.parentId != placement.parentId || item.subpage != placement.subpage
                || item.order != placement.order)
            {
                reject("planMetadataMismatch", "Layout plan cannot change widget ancestry, subpage or order.",
                       placement.id, placement.parentId);
                return m_plan.error;
            }
            if (placed.contains(placement.id))
            {
                reject("duplicatePlacement", "Layout plan contains duplicate widget placements.",
                       placement.id, placement.parentId);
                return m_plan.error;
            }
            placed.insert(placement.id);
            if (!finiteRect(placement.rect))
            {
                reject("invalidRectangle", "Planned widget dimensions must be finite, positive and representable.",
                       placement.id, placement.parentId);
                return m_plan.error;
            }
            if (!m_containers.contains(item.id) && !sameSize(item.rect.size(), placement.rect.size()))
            {
                reject("unexpectedResize", "A layout plan must preserve leaf widget dimensions.",
                       placement.id, placement.parentId);
                return m_plan.error;
            }
            m_rectangles[placement.id] = placement.rect;
        }
        QSet<quint32> grown;
        for (const auto &growth : plan.growth)
        {
            if (!m_containers.contains(growth.id) || grown.contains(growth.id) || !positiveSize(growth.size))
            {
                reject("invalidGrowth", "Container growth must be unique, finite and refer to a known container.",
                       growth.id, growth.id);
                return m_plan.error;
            }
            const auto &container = m_snapshot.containers[m_containers.value(growth.id)];
            if (less(growth.size.width(), container.size.width())
                || less(growth.size.height(), container.size.height())
                || (less(container.size.width(), growth.size.width()) && !m_options.allowWidthGrowth)
                || (less(container.size.height(), growth.size.height()) && !m_options.allowHeightGrowth))
            {
                reject("invalidGrowth", "Container growth must be explicitly permitted and cannot shrink.",
                       growth.id, growth.id);
                return m_plan.error;
            }
            grown.insert(growth.id);
            m_sizes[growth.id] = growth.size;
        }
        for (const auto &container : m_snapshot.containers)
            if (container.id != m_snapshot.rootId
                && !sameSize(m_rectangles.value(container.id).size(), m_sizes.value(container.id)))
            {
                reject("containerSizeMismatch", "Planned frame dimensions must match its explicit container growth.",
                       container.id, container.id);
                return m_plan.error;
            }
        validateResult();
        return m_plan.error;
    }

private:
    void validateInputs()
    {
        if (!layoutNumber(m_options.padding) || !layoutNumber(m_options.gap)
            || m_options.padding < 0 || m_options.gap < 0)
        {
            reject("invalidOptions", "Padding and gap must be finite and non-negative.", InvalidId, InvalidId);
            return;
        }
        if (m_snapshot.rootId == InvalidId || !m_containers.contains(m_snapshot.rootId))
        {
            reject("invalidSnapshot", "The snapshot must contain the requested root container.", InvalidId, InvalidId);
            return;
        }
        validateSnapshot();
    }

    void validateResult()
    {
        for (const auto &container : m_snapshot.containers)
        {
            const QSizeF delta = m_sizes.value(container.id) - container.size;
            for (const auto &page : container.pages)
            {
                const QRectF content(page.contentRect.topLeft(), page.contentRect.size() + delta);
                if (!finiteRect(content))
                {
                    reject("arithmeticOverflow", "Grown content bounds exceed representable coordinates.",
                           container.id, container.id);
                    return;
                }
                const auto items = pageItems(container.id, page.subpage);
                for (int i = 0; i < items.size(); ++i)
                {
                    const auto &item = items[i];
                    if (less(item.rect.width(), item.minimumSize.width())
                        || less(item.rect.height(), item.minimumSize.height()))
                    {
                        reject("belowMinimum", "Planned widget dimensions are below the usability minimum.",
                               item.id, container.id);
                        return;
                    }
                    if (!finiteRect(item.rect) || less(item.rect.x(), content.x() + m_options.padding)
                        || less(item.rect.y(), content.y() + m_options.padding)
                        || less(content.right() - m_options.padding, item.rect.right())
                        || less(content.bottom() - m_options.padding, item.rect.bottom()))
                    {
                        reject("outsideContent", "Calculated widget lies outside its container content bounds.",
                               item.id, container.id);
                        return;
                    }
                    for (int j = 0; j < i; ++j)
                        if (overlap(item.rect, items[j].rect))
                        {
                            reject("overlappingWidgets", "Calculated co-visible widgets overlap.",
                                   item.id, container.id);
                            return;
                        }
                }
            }
        }
    }

    void validateSnapshot()
    {
        QSet<quint32> ids;
        for (const auto &container : m_snapshot.containers)
        {
            if (container.id == InvalidId || ids.contains(container.id))
            {
                reject("duplicateId", "Container IDs must be valid and unique.", container.id, container.id);
                return;
            }
            ids.insert(container.id);
            if (!positiveSize(container.size) || !positiveSize(container.minimumSize))
            {
                reject("invalidContainer", "Container dimensions must be finite, positive and representable.",
                       container.id, container.id);
                return;
            }
            if (less(container.size.width(), container.minimumSize.width())
                || less(container.size.height(), container.minimumSize.height()))
            {
                reject("belowMinimum", "Container dimensions are below the usability minimum.",
                       container.id, container.id);
                return;
            }
            if (container.pages.isEmpty())
            {
                reject("invalidSubpage", "Every container must declare its subpages.", container.id, container.id);
                return;
            }
            QSet<int> subpages;
            for (const auto &page : container.pages)
            {
                if (page.subpage < 0 || subpages.contains(page.subpage))
                {
                    reject("invalidSubpage", "Subpage IDs must be non-negative and unique.",
                           container.id, container.id);
                    return;
                }
                subpages.insert(page.subpage);
                if (!finiteRect(page.contentRect) || less(page.contentRect.x(), 0) || less(page.contentRect.y(), 0)
                    || less(container.size.width(), page.contentRect.right())
                    || less(container.size.height(), page.contentRect.bottom()) || page.columns < 0)
                {
                    reject("invalidContent", "Content bounds must lie inside the container.",
                           container.id, container.id);
                    return;
                }
                if (page.snappingEnabled && !positiveSize(page.snapStep))
                {
                    reject("invalidSnap", "Enabled snapping requires finite, positive steps.",
                           container.id, container.id);
                    return;
                }
            }
        }
        ids.clear();
        for (const auto &item : m_snapshot.items)
        {
            if (item.id == InvalidId || item.id == m_snapshot.rootId || ids.contains(item.id))
            {
                reject("duplicateId", "Widget IDs must be valid and unique.", item.id, item.parentId);
                return;
            }
            ids.insert(item.id);
            if (!finiteRect(item.rect) || !positiveSize(item.minimumSize))
            {
                reject("invalidRectangle", "Widget dimensions must be finite, positive and representable.",
                       item.id, item.parentId);
                return;
            }
            if (less(item.rect.width(), item.minimumSize.width())
                || less(item.rect.height(), item.minimumSize.height()))
            {
                reject("belowMinimum", "Widget dimensions are below the usability minimum.", item.id, item.parentId);
                return;
            }
            if (!m_containers.contains(item.parentId))
            {
                reject("unknownParent", "Widget parent is not a declared container.", item.id, item.parentId);
                return;
            }
            const auto &parent = m_snapshot.containers[m_containers.value(item.parentId)];
            if (std::none_of(parent.pages.begin(), parent.pages.end(), [&](const LayoutPage &page) {
                return page.subpage == item.subpage;
            }))
            {
                reject("invalidSubpage", "Widget subpage does not exist in its parent.", item.id, item.parentId);
                return;
            }
            if (item.order < 0)
            {
                reject("invalidOrder", "Widget order must be non-negative.", item.id, item.parentId);
                return;
            }
            for (const auto &other : m_snapshot.items)
                if (other.id != item.id && other.parentId == item.parentId && other.subpage == item.subpage
                    && other.order == item.order && m_items.value(other.id) < m_items.value(item.id))
                {
                    reject("invalidOrder", "Sibling order must be unique within each subpage.", item.id, item.parentId);
                    return;
                }
        }
        for (const auto &container : m_snapshot.containers)
        {
            if (container.id == m_snapshot.rootId)
                continue;
            if (!m_items.contains(container.id))
            {
                reject("unknownContainer", "A nested container requires a matching frame item.",
                       container.id, container.id);
                return;
            }
            const auto &item = m_snapshot.items[m_items.value(container.id)];
            if (!sameSize(item.rect.size(), container.size))
            {
                reject("containerSizeMismatch", "Frame item and container dimensions must agree.",
                       item.id, item.parentId);
                return;
            }
        }
        for (const auto &item : m_snapshot.items)
        {
            QSet<quint32> chain;
            quint32 current = item.id;
            while (current != m_snapshot.rootId)
            {
                if (chain.contains(current))
                {
                    reject("ownershipCycle", "Container ownership contains a cycle.", item.id, item.parentId);
                    return;
                }
                if (chain.size() >= 1024)
                {
                    reject("hierarchyTooDeep", "Container ownership exceeds the supported nesting depth.",
                           item.id, item.parentId);
                    return;
                }
                chain.insert(current);
                current = m_snapshot.items[m_items.value(current)].parentId;
            }
        }
    }

    QVector<LayoutItem> pageItems(quint32 id, int subpage) const
    {
        QVector<LayoutItem> items;
        for (auto item : m_snapshot.items)
        {
            if (item.parentId == id && item.subpage == subpage)
            {
                item.rect = m_rectangles.value(item.id);
                items.append(item);
            }
        }
        std::sort(items.begin(), items.end(), [](const LayoutItem &a, const LayoutItem &b) {
            return a.order < b.order;
        });
        return items;
    }

    QVector<LayoutPage> pages(quint32 id) const
    {
        QVector<LayoutPage> result = m_snapshot.containers[m_containers.value(id)].pages;
        std::sort(result.begin(), result.end(), [](const LayoutPage &a, const LayoutPage &b) {
            return a.subpage < b.subpage;
        });
        return result;
    }

    void reject(const QString &code, const QString &message, quint32 widgetId,
                quint32 parentId, qreal required = 0, qreal available = 0)
    {
        m_plan.error = {code, message, widgetId, parentId, required, available};
    }

    void calculateContainer(quint32 id, Policy policy)
    {
        if (!m_plan.ok() || m_sizes.contains(id))
            return;
        if (m_visiting.contains(id))
        {
            reject("ownershipCycle", "Container ownership contains a cycle.", id, id);
            return;
        }
        m_visiting.insert(id);
        const auto &container = m_snapshot.containers[m_containers.value(id)];
        if (!positiveSize(container.size) || !positiveSize(container.minimumSize))
        {
            reject("invalidContainer", "Container dimensions must be finite and positive.", id, id);
            return;
        }
        QSizeF required = container.size;
        for (const auto &page : pages(id))
        {
            if (!finiteRect(page.contentRect) || page.columns < 0)
            {
                reject("invalidContent", "Content bounds and column count must be valid.", InvalidId, id);
                return;
            }
            auto items = pageItems(id, page.subpage);
            for (auto &item : items)
            {
                if (m_containers.contains(item.id))
                {
                    calculateContainer(item.id, policy);
                    if (!m_plan.ok())
                        return;
                    item.rect.setSize(m_sizes.value(item.id));
                    m_rectangles[item.id].setSize(item.rect.size());
                }
                if (!finiteRect(item.rect) || !positiveSize(item.minimumSize))
                {
                    reject("invalidRectangle", "Widget dimensions must be finite and positive.", item.id, id);
                    return;
                }
                if (less(item.rect.width(), item.minimumSize.width())
                    || less(item.rect.height(), item.minimumSize.height()))
                {
                    reject("belowMinimum", "Widget dimensions are below the usability minimum.", item.id, id);
                    return;
                }
            }
            QSizeF pageRequired;
            switch (policy)
            {
                case Policy::Align:
                    pageRequired = page.preserveColumnBands
                        ? alignBands(id, page, items) : alignPage(id, page, items);
                    break;
                case Policy::Compact:
                    pageRequired = compactPage(id, page, items);
                    break;
            }
            if (!m_plan.ok())
                return;
            required.setWidth(qMax(required.width(), pageRequired.width()));
            required.setHeight(qMax(required.height(), pageRequired.height()));
        }
        m_visiting.remove(id);
        m_sizes.insert(id, required);
    }

    QSizeF compactPage(quint32 id, const LayoutPage &page, QVector<LayoutItem> items)
    {
        const QSizeF size = m_snapshot.containers[m_containers.value(id)].size;
        std::sort(items.begin(), items.end(), [](const LayoutItem &a, const LayoutItem &b) {
            if (a.rect.y() != b.rect.y()) return a.rect.y() < b.rect.y();
            if (a.rect.x() != b.rect.x()) return a.rect.x() < b.rect.x();
            if (a.order != b.order) return a.order < b.order;
            return a.id < b.id;
        });
        QVector<QRectF> occupied;
        qreal right = page.contentRect.x();
        qreal bottom = page.contentRect.y();
        for (const auto &item : items)
        {
            const qreal x = page.snappingEnabled ? snapUp(item.rect.x(), page.snapStep.width()) : item.rect.x();
            qreal y = page.contentRect.y() + m_options.padding;
            const qreal width = page.snappingEnabled
                ? snapUp(item.rect.width(), page.snapStep.width()) : item.rect.width();
            const qreal height = page.snappingEnabled
                ? snapUp(item.rect.height(), page.snapStep.height()) : item.rect.height();
            for (const auto &other : occupied)
                if (less(x, other.right()) && less(other.x(), x + width))
                    y = qMax(y, other.bottom() + m_options.gap);
            if (page.snappingEnabled)
                y = snapUp(y, page.snapStep.height());
            const QRectF footprint(x, y, width, height);
            if (!finiteRect(footprint))
            {
                reject("arithmeticOverflow", "Compacted grid spans exceed representable coordinates.", item.id, id);
                return {};
            }
            if (less(x, page.contentRect.x() + m_options.padding))
            {
                reject("outsideContent", "Compaction cannot move a widget outside the left content boundary.",
                       item.id, id);
                return {};
            }
            if (less(page.contentRect.right(), footprint.right() + m_options.padding) && !m_options.allowWidthGrowth)
            {
                reject("insufficientWidth", "Compacted widget cannot fit in the container content width.",
                       item.id, id, footprint.right() + m_options.padding - page.contentRect.x(),
                       page.contentRect.width());
                return {};
            }
            if (less(page.contentRect.bottom(), footprint.bottom() + m_options.padding) && !m_options.allowHeightGrowth)
            {
                reject("insufficientHeight", "Compacted widget cannot fit in the container content height.",
                       item.id, id, footprint.bottom() + m_options.padding - page.contentRect.y(),
                       page.contentRect.height());
                return {};
            }
            occupied.append(footprint);
            m_rectangles[item.id] = QRectF(QPointF(x, y), item.rect.size());
            right = qMax(right, footprint.right() + m_options.padding);
            bottom = qMax(bottom, footprint.bottom() + m_options.padding);
        }
        const QSizeF required(size.width() + excess(right, page.contentRect.right()),
                              size.height() + excess(bottom, page.contentRect.bottom()));
        if (!positiveSize(required))
        {
            reject("arithmeticOverflow", "Compacted container growth exceeds representable dimensions.", id, id);
            return {};
        }
        return required;
    }

    QSizeF alignBands(quint32 id, const LayoutPage &page, const QVector<LayoutItem> &items)
    {
        const QSizeF size = m_snapshot.containers[m_containers.value(id)].size;
        if (items.isEmpty())
            return size;
        QVector<LayoutItem> sorted = items;
        std::sort(sorted.begin(), sorted.end(), [](const LayoutItem &a, const LayoutItem &b) {
            if (a.rect.x() != b.rect.x()) return a.rect.x() < b.rect.x();
            if (a.rect.y() != b.rect.y()) return a.rect.y() < b.rect.y();
            if (a.order != b.order) return a.order < b.order;
            return a.id < b.id;
        });
        for (const auto &item : sorted)
        {
            qreal left = -std::numeric_limits<qreal>::infinity();
            qreal right = std::numeric_limits<qreal>::infinity();
            for (const auto &other : sorted)
                if (item.id != other.id && less(item.rect.x(), other.rect.right())
                    && less(other.rect.x(), item.rect.right()))
                {
                    left = qMax(left, other.rect.x());
                    right = qMin(right, other.rect.right());
                }
            if (!less(left, right))
            {
                reject("ambiguousColumns", "A spanning widget bridges disjoint page bands.", item.id, id);
                return {};
            }
        }
        QVector<QVector<LayoutItem>> bands;
        qreal right = 0;
        for (const auto &item : sorted)
        {
            if (bands.isEmpty() || !less(item.rect.x(), right))
            {
                bands.append(QVector<LayoutItem>{});
                right = item.rect.right();
            }
            bands.last().append(item);
            right = qMax(right, item.rect.right());
        }
        if (bands.size() == 1)
            return alignPage(id, page, items);
        int position = 0;
        for (auto &band : bands)
        {
            std::sort(band.begin(), band.end(), [](const LayoutItem &a, const LayoutItem &b) {
                return a.order < b.order;
            });
            for (const auto &item : band)
                if (item.id != items[position++].id)
                {
                    reject("conflictingColumnOrder", "Page bands conflict with stored sibling order.", item.id, id);
                    return {};
                }
        }
        QSizeF required = size;
        qreal nextX = page.contentRect.x();
        for (int region = 0; region < bands.size(); ++region)
        {
            qreal width = 0;
            for (const auto &item : bands[region])
                width = qMax(width, item.rect.width());
            qreal leading = m_options.padding;
            if (page.snappingEnabled)
            {
                leading = snapUp(nextX + m_options.padding, page.snapStep.width()) - nextX;
                width = snapUp(width, page.snapStep.width());
            }
            LayoutPage local = page;
            local.preserveColumnBands = false;
            local.contentRect = QRectF(nextX, page.contentRect.y(), leading + width + m_options.padding,
                                       page.contentRect.height());
            const QSizeF bandRequired = alignPage(id, local, bands[region], region);
            if (!m_plan.ok())
                return {};
            required.setHeight(qMax(required.height(), bandRequired.height()));
            const auto &tracks = m_tracks[id].last();
            nextX = tracks.columnStarts.last() + tracks.columnWidths.last() + m_options.gap - m_options.padding;
        }
        const qreal requiredRight = nextX - m_options.gap + 2 * m_options.padding;
        if (less(page.contentRect.right(), requiredRight) && !m_options.allowWidthGrowth)
        {
            reject("insufficientWidth", "Page column bands cannot fit in the container content width.",
                   items.last().id, id, requiredRight - page.contentRect.x(), page.contentRect.width());
            return {};
        }
        required.setWidth(size.width() + excess(requiredRight, page.contentRect.right()));
        if (!positiveSize(required))
        {
            reject("arithmeticOverflow", "Page column extents exceed representable dimensions.", id, id);
            return {};
        }
        return required;
    }

    QSizeF alignPage(quint32 id, const LayoutPage &page, const QVector<LayoutItem> &items, int region = 0)
    {
        const QSizeF size = m_snapshot.containers[m_containers.value(id)].size;
        if (items.isEmpty())
            return size;
        const qreal tolerance = page.snappingEnabled ? qMax(qreal(1), page.snapStep.height() / 2) : 1;
        qreal availableWidth = page.contentRect.width() - 2 * m_options.padding;
        for (const auto &item : items)
            if (less(availableWidth, item.rect.width()))
            {
                if (!m_options.allowWidthGrowth)
                {
                    reject("insufficientWidth", "Widget cannot fit in the container content width.",
                           item.id, id, item.rect.width(), availableWidth);
                    return {};
                }
                availableWidth = item.rect.width();
            }
        Rows rows = inferredRows(items, tolerance, page.columns);
        if (rows.isEmpty())
            rows = wrappedRows(items, availableWidth, m_options.gap, page.columns);
        const QPointF start = page.contentRect.topLeft() + QPointF(m_options.padding, m_options.padding);
        Tracks tracks;
        while (true)
        {
            tracks = measureTracks(items, rows, id, page, start, m_options.gap);
            tracks.region = region;
            if (!layoutNumber(tracks.columnStarts.last() + tracks.columnWidths.last() + m_options.padding)
                || !layoutNumber(tracks.rowStarts.last() + tracks.rowHeights.last() + m_options.padding))
            {
                reject("arithmeticOverflow", "Accumulated layout tracks exceed representable coordinates.", id, id);
                return {};
            }
            const qreal width = tracks.columnStarts.last() + tracks.columnWidths.last() - start.x();
            if (!less(availableWidth, width))
                break;
            int split = rows.size() - 1;
            while (split >= 0 && rows[split].size() <= 1)
                --split;
            if (split < 0)
            {
                if (m_options.allowWidthGrowth)
                    break;
                reject("insufficientWidth", "Column tracks cannot fit in the container.",
                       items[0].id, id, width, availableWidth);
                return {};
            }
            const int last = rows[split].takeLast();
            rows.insert(split + 1, QVector<int>{last});
        }
        const qreal right = tracks.columnStarts.last() + tracks.columnWidths.last() + m_options.padding;
        const qreal bottom = tracks.rowStarts.last() + tracks.rowHeights.last() + m_options.padding;
        if (less(page.contentRect.bottom(), bottom) && !m_options.allowHeightGrowth)
        {
            reject("insufficientHeight", "Rows cannot fit in the container content height.",
                   items[rows.last().last()].id, id,
                   bottom - page.contentRect.y(), page.contentRect.height());
            return {};
        }
        for (int row = 0; row < rows.size(); ++row)
        {
            for (int column = 0; column < rows[row].size(); ++column)
            {
                const auto &item = items[rows[row][column]];
                const QRectF rect(QPointF(tracks.columnStarts[column], tracks.rowStarts[row]), item.rect.size());
                m_rectangles[item.id] = rect;
            }
        }
        m_tracks[id].append(tracks);
        const QSizeF required(size.width() + excess(right, page.contentRect.right()),
                              size.height() + excess(bottom, page.contentRect.bottom()));
        if (!positiveSize(required))
        {
            reject("arithmeticOverflow", "Required container growth exceeds representable dimensions.", id, id);
            return {};
        }
        return required;
    }

    void appendResults(quint32 id)
    {
        const auto &container = m_snapshot.containers[m_containers.value(id)];
        if (m_sizes.value(id) != container.size)
            m_plan.growth.append({id, m_sizes.value(id)});
        m_plan.tracks += m_tracks.value(id);
        for (const auto &page : pages(id))
            for (const auto &item : pageItems(id, page.subpage))
            {
                const auto &original = m_snapshot.items[m_items.value(item.id)];
                if (item.rect != original.rect)
                    m_plan.placements.append({item.id, item.parentId, item.subpage, item.order, item.rect});
                if (m_containers.contains(item.id))
                    appendResults(item.id);
            }
    }

    const LayoutSnapshot &m_snapshot;
    const LayoutOptions &m_options;
    QHash<quint32, int> m_items;
    QHash<quint32, int> m_containers;
    QHash<quint32, QRectF> m_rectangles;
    QHash<quint32, QSizeF> m_sizes;
    QHash<quint32, QVector<Tracks>> m_tracks;
    QSet<quint32> m_visiting;
    LayoutPlan m_plan;
};

} // namespace

LayoutPlan align(const LayoutSnapshot &snapshot, const LayoutOptions &options)
{
    return Calculation(snapshot, options).run(Policy::Align);
}

LayoutPlan compact(const LayoutSnapshot &snapshot, const LayoutOptions &options)
{
    return Calculation(snapshot, options).run(Policy::Compact);
}

Diagnostic validate(const LayoutSnapshot &snapshot, const LayoutPlan &plan, const LayoutOptions &options)
{
    return Calculation(snapshot, options).validatePlan(plan);
}

} // namespace GridLayout
