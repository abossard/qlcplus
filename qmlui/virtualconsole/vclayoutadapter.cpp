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

void applyPlan(VirtualConsole *vc, const VCBridge::LayoutPlan &plan)
{
    if (!vc)
        return;

    for (auto it = plan.geometries.constBegin(); it != plan.geometries.constEnd(); ++it)
    {
        VCWidget *widget = vc->widget(it.key());
        if (widget)
            widget->setGeometry(QRectF(it.value()));
    }
}

} // namespace VCLayoutAdapter
