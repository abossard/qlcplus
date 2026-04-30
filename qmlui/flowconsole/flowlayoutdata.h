/*
  Q Light Controller Plus
  flowlayoutdata.h

  Copyright (c) Massimo Callegari

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

#ifndef FLOWLAYOUTDATA_H
#define FLOWLAYOUTDATA_H

#include <QString>
#include <QList>

class VCWidget;

struct FlowWidgetEntry
{
    VCWidget *widget = nullptr;
    int colSpan = 1;
};

struct FlowSection
{
    quint32 id = 0;
    QString caption;
    QString sizePreset = "full";  // "full", "half", "third", "quarter"
    int columns = 4;
    bool isSolo = false;
    bool isCollapsed = false;
    QList<FlowWidgetEntry> widgets;
};

struct FlowPage
{
    QString name;
    QList<FlowSection> sections;
};

#endif // FLOWLAYOUTDATA_H
