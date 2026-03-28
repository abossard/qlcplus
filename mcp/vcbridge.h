/*
  Q Light Controller Plus
  vcbridge.h

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

#ifndef VCBRIDGE_H
#define VCBRIDGE_H

#include <QRect>
#include <QString>
#include <QVariant>
#include <QList>

/**
 * Abstract interface for Virtual Console operations.
 * Decouples MCP server tools from the specific UI (v4 Widget vs v5 QML).
 * Each UI provides its own implementation.
 */
class VCBridge
{
public:
    virtual ~VCBridge() {}

    struct WidgetInfo
    {
        int id;
        QString type;
        QString caption;
        QRect geometry;
        quint32 functionID;
    };

    struct PageInfo
    {
        int index;
        QString name;
        QList<WidgetInfo> widgets;
    };

    // Pages
    virtual int addPage(const QString &name) = 0;
    virtual QList<PageInfo> pages() const = 0;
    virtual int pagesCount() const = 0;

    // Frames
    virtual int addFrame(int pageIndex, const QRect &geometry,
                         const QString &caption, bool solo) = 0;

    // Widgets — return widget ID, -1 on failure
    virtual int addButton(int parentID, const QRect &geometry,
                          quint32 functionID, const QString &caption,
                          const QString &action) = 0;

    virtual int addSlider(int parentID, const QRect &geometry,
                          const QString &mode, const QString &caption,
                          quint32 functionID,
                          const QList<QPair<quint32, quint32>> &channels) = 0;

    virtual int addXYPad(int parentID, const QRect &geometry,
                         const QList<quint32> &fixtureIDs) = 0;

    virtual int addCueList(int parentID, const QRect &geometry,
                           quint32 chaserID, const QString &caption) = 0;

    virtual int addLabel(int parentID, const QRect &geometry,
                         const QString &text) = 0;

    // Input mapping
    virtual bool mapWidgetInput(int widgetID, quint32 universe,
                                quint32 channel) = 0;
};

#endif // VCBRIDGE_H
