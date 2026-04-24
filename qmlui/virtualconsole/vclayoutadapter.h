/*
  Q Light Controller Plus
  vclayoutadapter.h

  Copyright (c) Massimo Callegari

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef VCLAYOUTADAPTER_H
#define VCLAYOUTADAPTER_H

#include "vcbridge.h"  // for WidgetSnapshot, LayoutPlan, ReflowOptions

class VCWidget;
class VirtualConsole;

namespace VCLayoutAdapter {

/** Build a WidgetSnapshot tree from a live VCFrame/VCPage (or any VCWidget root). */
VCBridge::WidgetSnapshot snapshotFrame(VCWidget *root);

/** Apply a LayoutPlan to live widgets via VCWidget::setGeometry(). */
void applyPlan(VirtualConsole *vc, const VCBridge::LayoutPlan &plan);

} // namespace VCLayoutAdapter

#endif
