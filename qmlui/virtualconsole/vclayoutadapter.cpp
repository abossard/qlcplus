/*
  Q Light Controller Plus
  vclayoutadapter.cpp

  Copyright (c) Massimo Callegari

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include "vclayoutadapter.h"

#include "virtualconsole.h"
#include "vcframe.h"
#include "vcwidget.h"

namespace VCLayoutAdapter {

VCBridge::WidgetSnapshot snapshotFrame(VCWidget *widget)
{
    VCBridge::WidgetSnapshot snap;
    if (!widget)
        return snap;

    snap.id = widget->id();
    snap.type = widget->type();
    snap.geometry = widget->geometry().toRect();
    snap.parentID = -1;

    VCFrame *frame = qobject_cast<VCFrame *>(widget);
    if (frame)
    {
        snap.showHeader = frame->showHeader();
        for (VCWidget *child : frame->children(false))
        {
            VCBridge::WidgetSnapshot childSnap = snapshotFrame(child);
            childSnap.parentID = snap.id;
            snap.children.append(childSnap);
        }
    }
    return snap;
}

QVariantMap applyPlan(VirtualConsole *vc, const VCBridge::LayoutPlan &plan)
{
    if (!vc)
        return {{"accepted", false}, {"applied", false}, {"error", "Virtual Console unavailable"}};
    QMap<quint32, QRectF> rectangles;
    for (auto it = plan.geometries.constBegin(); it != plan.geometries.constEnd(); ++it)
    {
        VCWidget *widget = vc->widget(it.key());
        if (!widget)
            return {{"accepted", false}, {"applied", false},
                    {"error", QString("Unknown widget %1").arg(it.key())}};
        if (widget->geometry().toRect() == it.value())
            continue;
        const qreal scale = widget->scaleFactor();
        const QRectF rect = it.value();
        rectangles.insert(widget->id(), QRectF(rect.x() / scale, rect.y() / scale,
                                               rect.width() / scale, rect.height() / scale));
    }
    return vc->applyGeometryPlan(rectangles);
}

GridLayout::LayoutSnapshot capture(VCFrame *root)
{
    GridLayout::LayoutSnapshot snapshot;
    if (!root)
        return snapshot;
    snapshot.rootId = root->id();
    VirtualConsole *vc = nullptr;
    for (QObject *parent = root->parent(); parent && !vc; parent = parent->parent())
        vc = qobject_cast<VirtualConsole *>(parent);
    std::function<void(VCFrame *)> visit = [&](VCFrame *frame) {
        GridLayout::LayoutContainer container;
        container.id = frame->id();
        container.size = frame->logicalGeometry().size();
        container.minimumSize = frame->minimumSize();
        for (int page = 0; page < frame->totalPagesNumber(); ++page)
        {
            GridLayout::LayoutPage pageValue;
            pageValue.subpage = page;
            pageValue.contentRect = frame->contentRect();
            pageValue.columns = frame->layoutMode() == VCFrame::LayoutGrid ? frame->gridColumns() : 0;
            pageValue.preserveColumnBands = qobject_cast<VCPage *>(frame) != nullptr;
            pageValue.snappingEnabled = vc && vc->snapping() && vc->snappingSize() > 0;
            pageValue.snapStep = vc ? QSizeF(vc->snappingSize(), vc->snappingSize()) : QSizeF(1, 1);
            container.pages.append(pageValue);
            int order = 0;
            for (const QVariant &id : frame->orderedChildIDs(page))
            {
                VCWidget *child = nullptr;
                for (VCWidget *candidate : frame->children(false))
                    if (candidate->id() == id.toUInt())
                        child = candidate;
                if (!child)
                    continue;
                GridLayout::LayoutItem item;
                item.id = child->id();
                item.parentId = frame->id();
                item.subpage = page;
                item.order = order++;
                item.rect = child->logicalGeometry();
                item.minimumSize = child->minimumSize();
                snapshot.items.append(item);
            }
        }
        snapshot.containers.append(container);
        for (VCWidget *child : frame->children(false))
            if (VCFrame *nested = qobject_cast<VCFrame *>(child))
                visit(nested);
    };
    visit(root);
    return snapshot;
}

bool sameSnapshot(const GridLayout::LayoutSnapshot &a, const GridLayout::LayoutSnapshot &b)
{
    if (a.rootId != b.rootId || a.items.size() != b.items.size() ||
        a.containers.size() != b.containers.size())
        return false;
    for (int i = 0; i < a.items.size(); ++i)
    {
        const auto &x = a.items[i], &y = b.items[i];
        if (x.id != y.id || x.parentId != y.parentId || x.subpage != y.subpage ||
            x.order != y.order || x.rect != y.rect || x.minimumSize != y.minimumSize)
            return false;
    }
    for (int i = 0; i < a.containers.size(); ++i)
    {
        const auto &x = a.containers[i], &y = b.containers[i];
        if (x.id != y.id || x.size != y.size || x.minimumSize != y.minimumSize ||
            x.pages.size() != y.pages.size())
            return false;
        for (int j = 0; j < x.pages.size(); ++j)
        {
            const auto &p = x.pages[j], &q = y.pages[j];
            if (p.subpage != q.subpage || p.contentRect != q.contentRect ||
                p.columns != q.columns || p.preserveColumnBands != q.preserveColumnBands ||
                p.snappingEnabled != q.snappingEnabled || p.snapStep != q.snapStep)
                return false;
        }
    }
    return true;
}

} // namespace VCLayoutAdapter
