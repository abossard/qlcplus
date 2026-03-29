/*
  Q Light Controller Plus
  vcbridgev5.cpp

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

#include "vcbridgev5.h"
#include "virtualconsole.h"
#include "vcpage.h"
#include "vcframe.h"
#include "vcsoloframe.h"
#include "vcbutton.h"
#include "vcslider.h"
#include "vcxypad.h"
#include "vccuelist.h"
#include "vclabel.h"
#include "vcspeeddial.h"
#include "vcaudiotriggers.h"
#include "vcclock.h"
#include "vcwidget.h"
#include "doc.h"
#include "fixture.h"
#include "function.h"
#include "qlcinputsource.h"
#include "qlcinputfeedback.h"

#include <QQmlEngine>
#include <QDebug>

VCBridgeV5::VCBridgeV5(Doc *doc, VirtualConsole *vc)
    : m_doc(doc)
    , m_vc(vc)
{
}

int VCBridgeV5::addPage(const QString &name)
{
    int idx = m_vc->pagesCount();
    m_vc->addPage(idx);
    VCPage *page = m_vc->page(idx);
    if (page && !name.isEmpty())
        page->setCaption(name);
    return idx;
}

QList<VCBridge::PageInfo> VCBridgeV5::pages() const
{
    QList<PageInfo> result;
    for (int i = 0; i < m_vc->pagesCount(); i++)
    {
        VCPage *page = m_vc->page(i);
        if (!page) continue;
        PageInfo pi;
        pi.index = i;
        pi.name = page->caption().isEmpty()
            ? QString("Page %1").arg(i + 1) : page->caption();

        // Populate widgets from the page's children
        for (VCWidget *w : page->children(true))
        {
            WidgetInfo wi;
            wi.id = w->id();
            wi.type = VCWidget::typeToString(w->type());
            wi.caption = w->caption();
            wi.geometry = w->geometry().toRect();
            wi.functionID = Function::invalidId();

            VCButton *btn = qobject_cast<VCButton *>(w);
            if (btn) wi.functionID = btn->functionID();
            VCCueList *cl = qobject_cast<VCCueList *>(w);
            if (cl) wi.functionID = cl->chaserID();

            pi.widgets.append(wi);
        }

        result.append(pi);
    }
    return result;
}

int VCBridgeV5::pagesCount() const
{
    return m_vc->pagesCount();
}

int VCBridgeV5::addFrame(int pageIndex, const QRect &geometry,
                         const QString &caption, bool solo)
{
    VCPage *page = m_vc->page(pageIndex);
    if (!page) return -1;

    VCWidget *widget = page->addWidget(m_vc->currentPageItem(), solo ? "Solo frame" : "Frame",
                                       QPoint(geometry.x(), geometry.y()));
    if (!widget) return -1;
    widget->setGeometry(geometry);
    widget->setCaption(caption);
    return widget->id();
}

int VCBridgeV5::addFrameInFrame(int parentID, const QRect &geometry,
                                 const QString &caption, bool solo)
{
    VCWidget *parent = m_vc->widget(parentID);
    VCFrame *frame = qobject_cast<VCFrame*>(parent);
    if (!frame) return -1;

    VCWidget *widget = frame->addWidget(nullptr, solo ? "Solo frame" : "Frame",
                                        QPoint(geometry.x(), geometry.y()));
    if (!widget) return -1;
    widget->setGeometry(geometry);
    widget->setCaption(caption);
    return widget->id();
}

int VCBridgeV5::addButton(int parentID, const QRect &geometry,
                          quint32 functionID, const QString &caption,
                          const QString &action,
                          int stopAllFadeTime)
{
    VCWidget *parent = m_vc->widget(parentID);
    VCFrame *frame = qobject_cast<VCFrame *>(parent);
    if (!frame) return -1;

    VCWidget *widget = frame->addWidget(m_vc->currentPageItem(), "Button",
                                        QPoint(geometry.x(), geometry.y()));
    if (!widget) return -1;

    VCButton *button = qobject_cast<VCButton *>(widget);
    if (button)
    {
        button->setGeometry(geometry);
        button->setCaption(caption);
        if (functionID != Function::invalidId())
            button->setFunctionID(functionID);

        if (action == "flash")
            button->setActionType(VCButton::Flash);
        else if (action == "blackout")
            button->setActionType(VCButton::Blackout);
        else if (action == "stopall")
        {
            button->setActionType(VCButton::StopAll);
            if (stopAllFadeTime > 0)
                button->setStopAllFadeOutTime(stopAllFadeTime);
        }
    }
    return widget->id();
}

int VCBridgeV5::addSlider(int parentID, const QRect &geometry,
                          const QString &mode, const QString &caption,
                          quint32 functionID,
                          const QList<QPair<quint32, quint32>> &channels)
{
    VCWidget *parent = m_vc->widget(parentID);
    VCFrame *frame = qobject_cast<VCFrame *>(parent);
    if (!frame) return -1;

    VCWidget *widget = frame->addWidget(m_vc->currentPageItem(), "Slider",
                                        QPoint(geometry.x(), geometry.y()));
    if (!widget) return -1;

    VCSlider *slider = qobject_cast<VCSlider *>(widget);
    if (slider)
    {
        slider->setGeometry(geometry);
        slider->setCaption(caption);

        if (mode == "submaster")
            slider->setSliderMode(VCSlider::Submaster);
        else if (mode == "playback")
        {
            slider->setSliderMode(VCSlider::Adjust);
            slider->setControlledFunction(functionID);
        }
        else // "level"
        {
            slider->setSliderMode(VCSlider::Level);
            for (const auto &ch : channels)
                slider->addLevelChannel(ch.first, ch.second);
        }
    }
    return widget->id();
}

int VCBridgeV5::addXYPad(int parentID, const QRect &geometry,
                         const QList<quint32> &fixtureIDs)
{
    VCWidget *parent = m_vc->widget(parentID);
    VCFrame *frame = qobject_cast<VCFrame *>(parent);
    if (!frame) return -1;

    VCWidget *widget = frame->addWidget(m_vc->currentPageItem(), "XYPad",
                                        QPoint(geometry.x(), geometry.y()));
    if (!widget) return -1;

    VCXYPad *xyPad = qobject_cast<VCXYPad *>(widget);
    if (xyPad)
    {
        xyPad->setGeometry(geometry);
        for (quint32 fxID : fixtureIDs)
        {
            Fixture *fxi = m_doc->fixture(fxID);
            if (fxi)
                xyPad->addFixture(QVariant::fromValue(fxi));
        }
    }
    return widget->id();
}

int VCBridgeV5::addCueList(int parentID, const QRect &geometry,
                           quint32 chaserID, const QString &caption)
{
    VCWidget *parent = m_vc->widget(parentID);
    VCFrame *frame = qobject_cast<VCFrame *>(parent);
    if (!frame) return -1;

    VCWidget *widget = frame->addWidget(m_vc->currentPageItem(), "CueList",
                                        QPoint(geometry.x(), geometry.y()));
    if (!widget) return -1;

    VCCueList *cuelist = qobject_cast<VCCueList *>(widget);
    if (cuelist)
    {
        cuelist->setGeometry(geometry);
        cuelist->setChaserID(chaserID);
        if (!caption.isEmpty())
            cuelist->setCaption(caption);
    }
    return widget->id();
}

int VCBridgeV5::addLabel(int parentID, const QRect &geometry,
                         const QString &text)
{
    VCWidget *parent = m_vc->widget(parentID);
    VCFrame *frame = qobject_cast<VCFrame *>(parent);
    if (!frame) return -1;

    VCWidget *widget = frame->addWidget(m_vc->currentPageItem(), "Label",
                                        QPoint(geometry.x(), geometry.y()));
    if (!widget) return -1;

    widget->setGeometry(geometry);
    widget->setCaption(text);
    return widget->id();
}

bool VCBridgeV5::mapWidgetInput(int widgetID, quint32 universe, quint32 channel)
{
    VCWidget *widget = m_vc->widget(widgetID);
    if (!widget) return false;

    // Remove all existing input sources to prevent duplicates on re-run
    while (!widget->inputSources().isEmpty())
    {
        auto source = widget->inputSources().first();
        widget->deleteInputSurce(source->id(), source->universe(), source->channel());
    }

    m_vc->createAndAddInputSource(widget, universe, channel);
    return true;
}

bool VCBridgeV5::setWidgetFeedback(int widgetID,
                                    int idleValue, int activeValue, int monitorValue,
                                    int idleMidiCh, int activeMidiCh, int monitorMidiCh)
{
    VCWidget *widget = m_vc->widget(widgetID);
    if (!widget) return false;

    for (auto &source : widget->inputSources())
    {
        source->setFeedbackValue(QLCInputFeedback::LowerValue, (uchar)idleValue);
        source->setFeedbackValue(QLCInputFeedback::UpperValue, (uchar)activeValue);
        source->setFeedbackValue(QLCInputFeedback::MonitorValue, (uchar)monitorValue);
        source->setFeedbackExtraParams(QLCInputFeedback::LowerValue, QVariant(idleMidiCh));
        source->setFeedbackExtraParams(QLCInputFeedback::UpperValue, QVariant(activeMidiCh));
        source->setFeedbackExtraParams(QLCInputFeedback::MonitorValue, QVariant(monitorMidiCh));
        return true;
    }
    return false;
}

bool VCBridgeV5::setWidgetColors(int widgetID, const QColor &bgColor, const QColor &fgColor)
{
    VCWidget *widget = m_vc->widget(widgetID);
    if (!widget) return false;

    if (bgColor.isValid())
        widget->setBackgroundColor(bgColor);
    if (fgColor.isValid())
        widget->setForegroundColor(fgColor);
    return true;
}

int VCBridgeV5::addSpeedDial(int parentID, const QRect &geometry,
                              const QList<quint32> &functionIDs)
{
    VCWidget *parent = m_vc->widget(parentID);
    VCFrame *frame = qobject_cast<VCFrame *>(parent);
    if (!frame) return -1;

    VCWidget *widget = frame->addWidget(m_vc->currentPageItem(), "Speed",
                                        QPoint(geometry.x(), geometry.y()));
    if (!widget) return -1;

    VCSpeedDial *speedDial = qobject_cast<VCSpeedDial *>(widget);
    if (speedDial)
    {
        speedDial->setGeometry(geometry);
        for (quint32 fid : functionIDs)
            speedDial->addFunction(fid);
    }
    return widget->id();
}

int VCBridgeV5::addAudioTriggers(int parentID, const QRect &geometry)
{
    VCWidget *parent = m_vc->widget(parentID);
    VCFrame *frame = qobject_cast<VCFrame *>(parent);
    if (!frame) return -1;

    VCWidget *widget = frame->addWidget(m_vc->currentPageItem(), "Audio Triggers",
                                        QPoint(geometry.x(), geometry.y()));
    if (!widget) return -1;

    widget->setGeometry(geometry);
    return widget->id();
}

int VCBridgeV5::addClock(int parentID, const QRect &geometry,
                          const QString &clockType)
{
    VCWidget *parent = m_vc->widget(parentID);
    VCFrame *frame = qobject_cast<VCFrame *>(parent);
    if (!frame) return -1;

    VCWidget *widget = frame->addWidget(m_vc->currentPageItem(), "Clock",
                                        QPoint(geometry.x(), geometry.y()));
    if (!widget) return -1;

    VCClock *clock = qobject_cast<VCClock *>(widget);
    if (clock)
    {
        clock->setGeometry(geometry);
        if (clockType == "stopwatch")
            clock->setClockType(VCClock::Stopwatch);
        else if (clockType == "countdown")
            clock->setClockType(VCClock::Countdown);
        else
            clock->setClockType(VCClock::Clock);
    }
    return widget->id();
}

int VCBridgeV5::findPageByName(const QString &name) const
{
    for (int i = 0; i < m_vc->pagesCount(); i++)
    {
        VCPage *page = m_vc->page(i);
        if (page && page->caption() == name)
            return i;
    }
    return -1;
}

int VCBridgeV5::findWidgetByCaption(int parentID, const QString &widgetType,
                                     const QString &caption) const
{
    VCWidget *parent = m_vc->widget(parentID);
    VCFrame *frame = qobject_cast<VCFrame *>(parent);
    if (!frame) return -1;

    int targetType = VCWidget::UnknownWidget;
    if (widgetType == "Button") targetType = VCWidget::ButtonWidget;
    else if (widgetType == "Slider") targetType = VCWidget::SliderWidget;
    else if (widgetType == "XYPad") targetType = VCWidget::XYPadWidget;
    else if (widgetType == "Frame") targetType = VCWidget::FrameWidget;
    else if (widgetType == "Solo frame") targetType = VCWidget::SoloFrameWidget;
    else if (widgetType == "Speed") targetType = VCWidget::SpeedWidget;
    else if (widgetType == "CueList") targetType = VCWidget::CueListWidget;
    else if (widgetType == "Label") targetType = VCWidget::LabelWidget;
    else if (widgetType == "Audio Triggers") targetType = VCWidget::AudioTriggersWidget;
    else if (widgetType == "Clock") targetType = VCWidget::ClockWidget;

    for (VCWidget *w : frame->children())
    {
        if (w->type() == targetType && w->caption() == caption)
            return w->id();
    }
    return -1;
}

QRect VCBridgeV5::nextWidgetPosition(int parentID, int width, int height) const
{
    VCWidget *parent = m_vc->widget(parentID);
    VCFrame *frame = qobject_cast<VCFrame *>(parent);
    if (!frame)
        return QRect(5, 5, width, height);

    const int pad = 5;
    int maxBottom = 40; // leave space for frame header
    for (VCWidget *w : frame->children())
    {
        int bottom = (int)w->geometry().y() + (int)w->geometry().height() + pad;
        if (bottom > maxBottom)
            maxBottom = bottom;
    }
    return QRect(pad, maxBottom, width, height);
}

QRect VCBridgeV5::nextWidgetPositionFlow(int parentID, int widgetWidth, int widgetHeight,
                                          int columns) const
{
    VCWidget *parent = m_vc->widget(parentID);
    VCFrame *frame = qobject_cast<VCFrame *>(parent);
    if (!frame)
        return QRect(5, 5, widgetWidth, widgetHeight);

    int parentWidth = (int)frame->geometry().width();
    int childCount = frame->children().count();

    return computeFlowPosition(parentWidth, 40, childCount, widgetWidth, widgetHeight, columns, 5);
}

void VCBridgeV5::setWidgetGeometry(int widgetID, const QRect &geo)
{
    VCWidget *widget = m_vc->widget(widgetID);
    if (widget)
        widget->setGeometry(QRectF(geo));
}

bool VCBridgeV5::removeWidget(int widgetID)
{
    VCWidget *widget = m_vc->widget(widgetID);
    if (!widget) return false;

    m_vc->deleteVCWidgets(QVariantList{widgetID});
    return true;
}

// --- Widget details query ---

VCBridge::WidgetDetails VCBridgeV5::getWidgetDetails(int widgetID) const
{
    VCWidget *widget = m_vc->widget(widgetID);
    if (!widget) return WidgetDetails();

    WidgetDetails d;
    d.id = widgetID;
    d.type = VCWidget::typeToString(widget->type());
    d.caption = widget->caption();
    d.geometry = widget->geometry().toRect();
    d.bgColor = widget->backgroundColor();
    d.fgColor = widget->foregroundColor();

    // Parent ID
    VCWidget *parent = qobject_cast<VCWidget*>(widget->parent());
    if (parent)
        d.parentID = (int)parent->id();

    // Input mappings
    for (auto &src : widget->inputSources())
    {
        InputMapping m;
        m.universe = src->universe();
        m.channel = src->channel();
        d.inputMappings.append(m);
    }

    // Feedback from first input source
    if (!widget->inputSources().isEmpty())
    {
        auto src = widget->inputSources().first();
        d.feedback.idleValue = src->feedbackValue(QLCInputFeedback::LowerValue);
        d.feedback.activeValue = src->feedbackValue(QLCInputFeedback::UpperValue);
        d.feedback.monitorValue = src->feedbackValue(QLCInputFeedback::MonitorValue);
        d.feedback.idleMidiCh = src->feedbackExtraParams(QLCInputFeedback::LowerValue).toInt();
        d.feedback.activeMidiCh = src->feedbackExtraParams(QLCInputFeedback::UpperValue).toInt();
        d.feedback.monitorMidiCh = src->feedbackExtraParams(QLCInputFeedback::MonitorValue).toInt();
    }

    // Button-specific
    VCButton *button = qobject_cast<VCButton*>(widget);
    if (button)
    {
        d.functionID = button->functionID();
        d.action = VCButton::actionToString(button->actionType()).toLower();
    }

    // Slider-specific
    VCSlider *slider = qobject_cast<VCSlider*>(widget);
    if (slider)
    {
        switch (slider->sliderMode())
        {
            case VCSlider::Level: d.sliderMode = "level"; break;
            case VCSlider::Adjust: d.sliderMode = "playback"; break;
            case VCSlider::Submaster: d.sliderMode = "submaster"; break;
            case VCSlider::GrandMaster: d.sliderMode = "grandmaster"; break;
        }
        d.functionID = slider->controlledFunction();
        for (auto &sv : slider->levelChannels())
            d.channels.append(qMakePair(sv.fxi, sv.channel));
    }

    // CueList-specific
    VCCueList *cuelist = qobject_cast<VCCueList*>(widget);
    if (cuelist)
        d.functionID = cuelist->chaserID();

    return d;
}

// --- Widget property mutations ---

bool VCBridgeV5::setWidgetCaption(int widgetID, const QString &caption)
{
    VCWidget *widget = m_vc->widget(widgetID);
    if (!widget) return false;
    widget->setCaption(caption);
    return true;
}

bool VCBridgeV5::setButtonFunction(int widgetID, quint32 functionID)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCButton *button = qobject_cast<VCButton*>(widget);
    if (!button) return false;
    button->setFunctionID(functionID);
    return true;
}

bool VCBridgeV5::setButtonAction(int widgetID, const QString &action)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCButton *button = qobject_cast<VCButton*>(widget);
    if (!button) return false;

    if (action == "flash") button->setActionType(VCButton::Flash);
    else if (action == "blackout") button->setActionType(VCButton::Blackout);
    else if (action == "stopall") button->setActionType(VCButton::StopAll);
    else button->setActionType(VCButton::Toggle);
    return true;
}

bool VCBridgeV5::setSliderMode(int widgetID, const QString &mode)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCSlider *slider = qobject_cast<VCSlider*>(widget);
    if (!slider) return false;

    if (mode == "level") slider->setSliderMode(VCSlider::Level);
    else if (mode == "playback") slider->setSliderMode(VCSlider::Adjust);
    else if (mode == "submaster") slider->setSliderMode(VCSlider::Submaster);
    else return false;
    return true;
}

bool VCBridgeV5::setSliderFunction(int widgetID, quint32 functionID)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCSlider *slider = qobject_cast<VCSlider*>(widget);
    if (!slider) return false;
    slider->setControlledFunction(functionID);
    return true;
}

bool VCBridgeV5::setSliderChannels(int widgetID, const QList<QPair<quint32, quint32>> &channels)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCSlider *slider = qobject_cast<VCSlider*>(widget);
    if (!slider) return false;

    slider->clearLevelChannels();
    for (auto &ch : channels)
        slider->addLevelChannel(ch.first, ch.second);
    return true;
}

// --- Input mapping ---

bool VCBridgeV5::addWidgetInput(int widgetID, quint32 universe, quint32 channel)
{
    VCWidget *widget = m_vc->widget(widgetID);
    if (!widget) return false;
    m_vc->createAndAddInputSource(widget, universe, channel);
    return true;
}

bool VCBridgeV5::removeWidgetInput(int widgetID, quint32 universe, quint32 channel)
{
    VCWidget *widget = m_vc->widget(widgetID);
    if (!widget) return false;

    for (auto &src : widget->inputSources())
    {
        if (src->universe() == universe && src->channel() == channel)
        {
            widget->deleteInputSurce(src->id(), universe, channel);
            return true;
        }
    }
    return false;
}

// --- Widget reparenting ---

bool VCBridgeV5::reparentWidget(int widgetID, int newParentID, const QRect &geo)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCWidget *newParent = m_vc->widget(newParentID);
    if (!widget || !newParent) return false;

    VCFrame *targetFrame = qobject_cast<VCFrame*>(newParent);
    if (!targetFrame) return false;

    bool ok = m_vc->reparentWidget(widget, targetFrame);
    if (ok)
        widget->setGeometry(QRectF(geo));
    return ok;
}
