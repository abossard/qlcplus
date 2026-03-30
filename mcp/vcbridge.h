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

#include <QColor>
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

    struct InputMapping
    {
        quint32 universe;
        quint32 channel;
    };

    struct FeedbackInfo
    {
        int idleValue = 0;
        int activeValue = 0;
        int monitorValue = 0;
        int idleMidiCh = 0;
        int activeMidiCh = 0;
        int monitorMidiCh = 0;
    };

    struct WidgetDetails
    {
        int id = -1;
        QString type;
        QString caption;
        QRect geometry;
        quint32 functionID = 0;
        QString action;          // Button only: toggle/flash/blackout/stopall
        QString sliderMode;      // Slider only: level/playback/submaster
        QList<QPair<quint32, quint32>> channels;  // Slider level-mode channels
        QList<InputMapping> inputMappings;
        QColor bgColor;
        QColor fgColor;
        FeedbackInfo feedback;
        int parentID = -1;
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
    virtual int addFrameInFrame(int parentID, const QRect &geometry,
                                const QString &caption, bool solo)
        { Q_UNUSED(parentID); Q_UNUSED(geometry); Q_UNUSED(caption); Q_UNUSED(solo); return -1; }

    // Widgets — return widget ID, -1 on failure
    virtual int addButton(int parentID, const QRect &geometry,
                          quint32 functionID, const QString &caption,
                          const QString &action,
                          int stopAllFadeTime = 0) = 0;

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

    // Feedback
    virtual bool setWidgetFeedback(int widgetID,
                                   int idleValue, int activeValue, int monitorValue,
                                   int idleMidiCh, int activeMidiCh, int monitorMidiCh) = 0;

    // Read feedback from the widget's first input source
    virtual FeedbackInfo getWidgetFeedback(int widgetID) const
        { Q_UNUSED(widgetID); return FeedbackInfo(); }

    // Number of input sources on a widget
    virtual int widgetInputSourceCount(int widgetID) const
        { Q_UNUSED(widgetID); return 0; }

    // Widget colors
    virtual bool setWidgetColors(int widgetID,
                                 const QColor &bgColor = QColor(),
                                 const QColor &fgColor = QColor()) = 0;

    // Speed Dial widget
    virtual int addSpeedDial(int parentID, const QRect &geometry,
                             const QList<quint32> &functionIDs) = 0;

    // Audio Triggers widget
    virtual int addAudioTriggers(int parentID, const QRect &geometry) = 0;

    // Clock widget
    virtual int addClock(int parentID, const QRect &geometry,
                         const QString &clockType) = 0;

    // Idempotency lookups (default: not found → always create)
    virtual int findPageByName(const QString &name) const
        { Q_UNUSED(name); return -1; }
    virtual int findWidgetByCaption(int parentID, const QString &widgetType,
                                    const QString &caption) const
        { Q_UNUSED(parentID); Q_UNUSED(widgetType); Q_UNUSED(caption); return -1; }

    // Auto-layout: returns next available position inside a parent frame
    virtual QRect nextWidgetPosition(int parentID, int width, int height) const
        { Q_UNUSED(parentID); return QRect(0, 0, width, height); }

    // Horizontal flow layout: places widgets left-to-right, wrapping to next row.
    // columns=0 auto-computes from parent width and widget width.
    virtual QRect nextWidgetPositionFlow(int parentID, int widgetWidth, int widgetHeight,
                                         int columns = 0) const
        { Q_UNUSED(parentID); Q_UNUSED(columns); return QRect(0, 0, widgetWidth, widgetHeight); }

    // Widget details query
    virtual WidgetDetails getWidgetDetails(int widgetID) const
        { Q_UNUSED(widgetID); return WidgetDetails(); }

    // Widget property mutations
    virtual bool setWidgetCaption(int widgetID, const QString &caption)
        { Q_UNUSED(widgetID); Q_UNUSED(caption); return false; }
    virtual bool setButtonFunction(int widgetID, quint32 functionID)
        { Q_UNUSED(widgetID); Q_UNUSED(functionID); return false; }
    virtual bool setButtonAction(int widgetID, const QString &action)
        { Q_UNUSED(widgetID); Q_UNUSED(action); return false; }
    virtual bool setSliderMode(int widgetID, const QString &mode)
        { Q_UNUSED(widgetID); Q_UNUSED(mode); return false; }
    virtual bool setSliderFunction(int widgetID, quint32 functionID)
        { Q_UNUSED(widgetID); Q_UNUSED(functionID); return false; }
    virtual bool setSliderChannels(int widgetID, const QList<QPair<quint32, quint32>> &channels)
        { Q_UNUSED(widgetID); Q_UNUSED(channels); return false; }

    // Input mapping (add without clearing existing)
    virtual bool addWidgetInput(int widgetID, quint32 universe, quint32 channel)
        { Q_UNUSED(widgetID); Q_UNUSED(universe); Q_UNUSED(channel); return false; }
    virtual bool removeWidgetInput(int widgetID, quint32 universe, quint32 channel)
        { Q_UNUSED(widgetID); Q_UNUSED(universe); Q_UNUSED(channel); return false; }

    // Widget reparenting — move widget to a different frame
    virtual bool reparentWidget(int widgetID, int newParentID, const QRect &geo)
        { Q_UNUSED(widgetID); Q_UNUSED(newParentID); Q_UNUSED(geo); return false; }

    // Resize an existing widget
    virtual void setWidgetGeometry(int widgetID, const QRect &geo)
        { Q_UNUSED(widgetID); Q_UNUSED(geo); }

    /** Pure-math flow layout helper: computes position for the childCount-th widget. */
    static QRect computeFlowPosition(int parentWidth, int headerHeight,
                                      int childCount, int widgetWidth, int widgetHeight,
                                      int columns, int pad)
    {
        if (columns <= 0)
            columns = qMax(1, (parentWidth - pad) / (widgetWidth + pad));

        int effectiveWidth = (parentWidth - (columns + 1) * pad) / columns;

        int row = childCount / columns;
        int col = childCount % columns;
        int x = pad + col * (effectiveWidth + pad);
        int y = headerHeight + row * (widgetHeight + pad);

        return QRect(x, y, effectiveWidth, widgetHeight);
    }

    // Widget deletion
    virtual bool removeWidget(int widgetID)
        { Q_UNUSED(widgetID); return false; }
};

#endif // VCBRIDGE_H
