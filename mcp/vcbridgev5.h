/*
  Q Light Controller Plus
  vcbridgev5.h

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

#ifndef VCBRIDGEV5_H
#define VCBRIDGEV5_H

#include "vcbridge.h"

class Doc;
class VirtualConsole;

class VCBridgeV5 : public VCBridge
{
public:
    VCBridgeV5(Doc *doc, VirtualConsole *vc);

    int addPage(const QString &name) override;
    QList<PageInfo> pages() const override;
    int pagesCount() const override;

    int addFrame(int pageIndex, const QRect &geometry,
                 const QString &caption, bool solo) override;
    int addFrameInFrame(int parentID, const QRect &geometry,
                        const QString &caption, bool solo) override;

    int addButton(int parentID, const QRect &geometry,
                  quint32 functionID, const QString &caption,
                  const QString &action,
                  int stopAllFadeTime = 0) override;

    int addSlider(int parentID, const QRect &geometry,
                  const QString &mode, const QString &caption,
                  quint32 functionID,
                  const QList<QPair<quint32, quint32>> &channels) override;

    int addXYPad(int parentID, const QRect &geometry,
                 const QList<quint32> &fixtureIDs) override;

    int addCueList(int parentID, const QRect &geometry,
                   quint32 chaserID, const QString &caption) override;

    int addLabel(int parentID, const QRect &geometry,
                 const QString &text) override;

    bool mapWidgetInput(int widgetID, quint32 universe,
                        quint32 channel) override;

    bool setWidgetFeedback(int widgetID,
                           int idleValue, int activeValue, int monitorValue,
                           int idleMidiCh, int activeMidiCh, int monitorMidiCh) override;

    bool setWidgetColors(int widgetID,
                         const QColor &bgColor = QColor(),
                         const QColor &fgColor = QColor()) override;

    int addSpeedDial(int parentID, const QRect &geometry,
                     const QList<quint32> &functionIDs) override;

    int addAudioTriggers(int parentID, const QRect &geometry) override;

    int addClock(int parentID, const QRect &geometry,
                 const QString &clockType) override;

    int findPageByName(const QString &name) const override;
    int findWidgetByCaption(int parentID, const QString &widgetType,
                            const QString &caption) const override;
    QRect nextWidgetPosition(int parentID, int width, int height) const override;
    QRect nextWidgetPositionFlow(int parentID, int widgetWidth, int widgetHeight,
                                  int columns = 0) const override;
    void setWidgetGeometry(int widgetID, const QRect &geo) override;
    bool removeWidget(int widgetID) override;

    // Widget details query
    WidgetDetails getWidgetDetails(int widgetID) const override;

    // Widget property mutations
    bool setWidgetCaption(int widgetID, const QString &caption) override;
    bool setButtonFunction(int widgetID, quint32 functionID) override;
    bool setButtonAction(int widgetID, const QString &action) override;
    bool setSliderMode(int widgetID, const QString &mode) override;
    bool setSliderFunction(int widgetID, quint32 functionID) override;
    bool setSliderChannels(int widgetID, const QList<QPair<quint32, quint32>> &channels) override;

    // Input mapping
    bool addWidgetInput(int widgetID, quint32 universe, quint32 channel) override;
    bool removeWidgetInput(int widgetID, quint32 universe, quint32 channel) override;

    // Widget reparenting
    bool reparentWidget(int widgetID, int newParentID, const QRect &geo) override;

private:
    Doc *m_doc;
    VirtualConsole *m_vc;
};

#endif // VCBRIDGEV5_H
