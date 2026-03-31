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
#include "grandmaster.h"
#include "inputoutputmap.h"
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
        else if (mode == "grandmaster")
            slider->setSliderMode(VCSlider::GrandMaster);
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
    // Delegate to addXYPadEx with default config
    QList<XYPadFixtureConfig> configs;
    for (quint32 fxID : fixtureIDs)
    {
        XYPadFixtureConfig cfg;
        cfg.fixtureID = fxID;
        configs.append(cfg);
    }
    return addXYPadEx(parentID, geometry, configs, "degrees", false);
}

int VCBridgeV5::addXYPadEx(int parentID, const QRect &geometry,
                            const QList<XYPadFixtureConfig> &fixtures,
                            const QString &displayMode,
                            bool invertedAppearance)
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

        for (const XYPadFixtureConfig &cfg : fixtures)
        {
            Fixture *fxi = m_doc->fixture(cfg.fixtureID);
            if (!fxi) continue;

            bool isDefault = (cfg.xMin == 0.0 && cfg.xMax == 1.0
                              && cfg.yMin == 0.0 && cfg.yMax == 1.0
                              && !cfg.xReverse && !cfg.yReverse);

            // Add the fixture/head to the pad
            if (cfg.head == 0 && isDefault)
                xyPad->addFixture(QVariant::fromValue(fxi));
            else
                xyPad->addHead(cfg.fixtureID, cfg.head);

            // Apply custom axis ranges if non-default
            if (!isDefault)
            {
                xyPad->setFixtureRange(cfg.fixtureID, cfg.head,
                                       cfg.xMin, cfg.xMax, cfg.xReverse,
                                       cfg.yMin, cfg.yMax, cfg.yReverse);
            }
        }

        // Set display mode
        if (displayMode == "percentage")
            xyPad->setDisplayMode(VCXYPad::Percentage);
        else if (displayMode == "dmx")
            xyPad->setDisplayMode(VCXYPad::DMX);
        else
            xyPad->setDisplayMode(VCXYPad::Degrees);

        xyPad->setInvertedAppearance(invertedAppearance);
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

VCBridge::FeedbackInfo VCBridgeV5::getWidgetFeedback(int widgetID) const
{
    FeedbackInfo fb;
    VCWidget *widget = m_vc->widget(widgetID);
    if (!widget || widget->inputSources().isEmpty())
        return fb;

    auto src = widget->inputSources().first();
    fb.idleValue = src->feedbackValue(QLCInputFeedback::LowerValue);
    fb.activeValue = src->feedbackValue(QLCInputFeedback::UpperValue);
    fb.monitorValue = src->feedbackValue(QLCInputFeedback::MonitorValue);
    fb.idleMidiCh = src->feedbackExtraParams(QLCInputFeedback::LowerValue).toInt();
    fb.activeMidiCh = src->feedbackExtraParams(QLCInputFeedback::UpperValue).toInt();
    fb.monitorMidiCh = src->feedbackExtraParams(QLCInputFeedback::MonitorValue).toInt();
    return fb;
}

int VCBridgeV5::widgetInputSourceCount(int widgetID) const
{
    VCWidget *widget = m_vc->widget(widgetID);
    if (!widget) return 0;
    return widget->inputSources().size();
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

        // Extended slider properties
        switch (slider->clickAndGoType())
        {
            case VCSlider::CnGNone: d.clickAndGoType = "none"; break;
            case VCSlider::CnGColors: d.clickAndGoType = "colors"; break;
            case VCSlider::CnGPreset: d.clickAndGoType = "preset"; break;
        }
        switch (slider->valueDisplayStyle())
        {
            case VCSlider::DMXValue: d.valueDisplayStyle = "dmx"; break;
            case VCSlider::PercentageValue: d.valueDisplayStyle = "percentage"; break;
        }
        d.sliderInvertedAppearance = slider->invertedAppearance();
        d.rangeLowLimit = slider->rangeLowLimit();
        d.rangeHighLimit = slider->rangeHighLimit();
        d.monitorEnabled = slider->monitorEnabled();

        if (slider->sliderMode() == VCSlider::GrandMaster)
        {
            auto gmValMode = slider->grandMasterValueMode();
            d.gmValueMode = (gmValMode == GrandMaster::Reduce) ? "reduce" : "limit";
            auto gmChMode = slider->grandMasterChannelMode();
            d.gmChannelMode = (gmChMode == GrandMaster::AllChannels) ? "allchannels" : "intensity";
        }
    }

    // CueList-specific
    VCCueList *cuelist = qobject_cast<VCCueList*>(widget);
    if (cuelist)
        d.functionID = cuelist->chaserID();

    // XYPad-specific
    VCXYPad *xyPad = qobject_cast<VCXYPad*>(widget);
    if (xyPad)
    {
        switch (xyPad->displayMode())
        {
            case VCXYPad::Percentage: d.displayMode = "percentage"; break;
            case VCXYPad::Degrees: d.displayMode = "degrees"; break;
            case VCXYPad::DMX: d.displayMode = "dmx"; break;
        }
        d.invertedAppearance = xyPad->invertedAppearance();
        d.xyPadPosition = xyPad->currentPosition();

        for (const auto &fxItem : xyPad->fixtures())
        {
            XYPadFixtureInfo info;
            info.fixtureID = fxItem.m_head.fxi;
            info.head = fxItem.m_head.head;
            info.xMin = fxItem.m_xMin;
            info.xMax = fxItem.m_xMax;
            info.xReverse = fxItem.m_xReverse;
            info.yMin = fxItem.m_yMin;
            info.yMax = fxItem.m_yMax;
            info.yReverse = fxItem.m_yReverse;

            Fixture *fxi = m_doc->fixture(info.fixtureID);
            if (fxi)
            {
                info.name = fxi->name();
                QRectF degRange = fxi->degreesRange(info.head);
                if (!degRange.isNull())
                {
                    info.panDegreesMax = degRange.width();
                    info.tiltDegreesMax = degRange.height();
                }
            }
            d.xyPadFixtures.append(info);
        }
    }

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
    else if (mode == "grandmaster") slider->setSliderMode(VCSlider::GrandMaster);
    else return false;
    return true;
}

bool VCBridgeV5::configureSlider(int widgetID, const SliderConfig &config)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCSlider *slider = qobject_cast<VCSlider*>(widget);
    if (!slider) return false;

    if (config.clickAndGoType.has_value())
    {
        const QString &v = config.clickAndGoType.value();
        if (v == "colors") slider->setClickAndGoType(VCSlider::CnGColors);
        else if (v == "preset") slider->setClickAndGoType(VCSlider::CnGPreset);
        else slider->setClickAndGoType(VCSlider::CnGNone);
    }
    if (config.valueDisplayStyle.has_value())
    {
        const QString &v = config.valueDisplayStyle.value();
        if (v == "percentage") slider->setValueDisplayStyle(VCSlider::PercentageValue);
        else slider->setValueDisplayStyle(VCSlider::DMXValue);
    }
    if (config.invertedAppearance.has_value())
        slider->setInvertedAppearance(config.invertedAppearance.value());
    if (config.rangeLowLimit.has_value())
        slider->setRangeLowLimit(config.rangeLowLimit.value());
    if (config.rangeHighLimit.has_value())
        slider->setRangeHighLimit(config.rangeHighLimit.value());
    if (config.monitorEnabled.has_value())
        slider->setMonitorEnabled(config.monitorEnabled.value());
    if (config.gmValueMode.has_value())
    {
        const QString &v = config.gmValueMode.value();
        slider->setGrandMasterValueMode(
            v == "reduce" ? GrandMaster::Reduce : GrandMaster::Limit);
    }
    if (config.gmChannelMode.has_value())
    {
        const QString &v = config.gmChannelMode.value();
        slider->setGrandMasterChannelMode(
            v == "allchannels" ? GrandMaster::AllChannels : GrandMaster::Intensity);
    }
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

// --- XY Pad property mutations ---

bool VCBridgeV5::setXYPadPosition(int widgetID, qreal x, qreal y)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCXYPad *xyPad = qobject_cast<VCXYPad *>(widget);
    if (!xyPad) return false;
    xyPad->setCurrentPosition(QPointF(x, y));
    return true;
}

bool VCBridgeV5::setXYPadDisplayMode(int widgetID, const QString &mode)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCXYPad *xyPad = qobject_cast<VCXYPad *>(widget);
    if (!xyPad) return false;

    if (mode == "percentage")
        xyPad->setDisplayMode(VCXYPad::Percentage);
    else if (mode == "dmx")
        xyPad->setDisplayMode(VCXYPad::DMX);
    else if (mode == "degrees")
        xyPad->setDisplayMode(VCXYPad::Degrees);
    else
        return false;
    return true;
}

bool VCBridgeV5::setXYPadInvertedAppearance(int widgetID, bool inverted)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCXYPad *xyPad = qobject_cast<VCXYPad *>(widget);
    if (!xyPad) return false;
    xyPad->setInvertedAppearance(inverted);
    return true;
}

bool VCBridgeV5::addXYPadFixture(int widgetID, const XYPadFixtureConfig &config)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCXYPad *xyPad = qobject_cast<VCXYPad *>(widget);
    if (!xyPad) return false;

    Fixture *fxi = m_doc->fixture(config.fixtureID);
    if (!fxi) return false;

    xyPad->addHead(config.fixtureID, config.head);
    xyPad->setFixtureRange(config.fixtureID, config.head,
                           config.xMin, config.xMax, config.xReverse,
                           config.yMin, config.yMax, config.yReverse);
    return true;
}

bool VCBridgeV5::removeXYPadFixture(int widgetID, quint32 fixtureID, int head)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCXYPad *xyPad = qobject_cast<VCXYPad *>(widget);
    if (!xyPad) return false;
    return xyPad->removeHead(fixtureID, head);
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

// --- Layout analysis: snapshot / apply ---

static VCBridge::WidgetSnapshot snapshotWidget(VCWidget *widget)
{
    VCBridge::WidgetSnapshot snap;
    snap.id = widget->id();
    snap.type = widget->type();
    snap.geometry = widget->geometry().toRect();

    VCFrame *frame = qobject_cast<VCFrame *>(widget);
    if (frame)
    {
        snap.parentID = -1;  // filled by caller
        for (VCWidget *child : frame->children(false))
        {
            VCBridge::WidgetSnapshot childSnap = snapshotWidget(child);
            childSnap.parentID = snap.id;
            snap.children.append(childSnap);
        }
    }
    return snap;
}

VCBridge::WidgetSnapshot VCBridgeV5::snapshotFrame(int frameID) const
{
    VCWidget *widget = m_vc->widget(frameID);
    if (!widget) return WidgetSnapshot();
    return snapshotWidget(widget);
}

VCBridge::WidgetSnapshot VCBridgeV5::snapshotPage(int pageIndex) const
{
    VCPage *page = m_vc->page(pageIndex);
    if (!page) return WidgetSnapshot();
    return snapshotWidget(page);
}

void VCBridgeV5::applyLayoutPlan(const LayoutPlan &plan)
{
    for (auto it = plan.geometries.constBegin(); it != plan.geometries.constEnd(); ++it)
    {
        VCWidget *widget = m_vc->widget(it.key());
        if (widget)
            widget->setGeometry(QRectF(it.value()));
    }
}
