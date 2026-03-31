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
#include "vcxypadpreset.h"
#include "vccuelist.h"
#include "vclabel.h"
#include "vcspeeddial.h"
#include "vcspeeddialpreset.h"
#include "vcaudiotriggers.h"
#include "vcclock.h"
#include "vcanimation.h"
#include "vcwidget.h"
#include "doc.h"
#include "fixture.h"
#include "function.h"
#include "scenevalue.h"
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

bool VCBridgeV5::configureAudioTriggerBar(int widgetID, const AudioBarConfig &config)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCAudioTriggers *at = qobject_cast<VCAudioTriggers *>(widget);
    if (!at) return false;

    if (config.barIndex < 0 || config.barIndex >= at->barsNumber())
        return false;

    at->selectBarForEditing(config.barIndex);

    // Set bar type
    VCAudioTriggers::BarType barType = VCAudioTriggers::None;
    if (config.type == "dmx") barType = VCAudioTriggers::DMXBar;
    else if (config.type == "function") barType = VCAudioTriggers::FunctionBar;
    else if (config.type == "widget") barType = VCAudioTriggers::VCWidgetBar;
    at->setBarType(barType);

    // Set thresholds
    at->setBarThresholds(config.minThreshold, config.maxThreshold);

    // Set type-specific assignments
    if (barType == VCAudioTriggers::FunctionBar && config.functionID != (quint32)-1)
        at->setBarFunction(config.functionID);
    else if (barType == VCAudioTriggers::VCWidgetBar && config.widgetID != (quint32)-1)
        at->setBarWidget(config.widgetID);
    else if (barType == VCAudioTriggers::DMXBar && !config.dmxChannels.isEmpty())
    {
        QList<SceneValue> svList;
        for (const auto &ch : config.dmxChannels)
            svList.append(SceneValue(ch.first, ch.second));
        at->setBarDmxChannels(svList);
    }

    return true;
}

bool VCBridgeV5::setAudioTriggerCapture(int widgetID, bool enabled)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCAudioTriggers *at = qobject_cast<VCAudioTriggers *>(widget);
    if (!at) return false;
    at->setCaptureEnabled(enabled);
    return true;
}

bool VCBridgeV5::setAudioTriggerVolume(int widgetID, int volume)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCAudioTriggers *at = qobject_cast<VCAudioTriggers *>(widget);
    if (!at) return false;
    at->setVolumeLevel(qBound(0, volume, 255));
    return true;
}

bool VCBridgeV5::setAudioTriggerBarsNumber(int widgetID, int count)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCAudioTriggers *at = qobject_cast<VCAudioTriggers *>(widget);
    if (!at) return false;
    if (count < 1) count = 1; // minimum: volume bar
    at->setBarsNumber(count);
    return true;
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

static QString multiplierToString(VCSpeedDial::SpeedMultiplier m)
{
    switch (m)
    {
        case VCSpeedDial::Zero: return "0";
        case VCSpeedDial::OneSixteenth: return "1/16";
        case VCSpeedDial::OneEighth: return "1/8";
        case VCSpeedDial::OneFourth: return "1/4";
        case VCSpeedDial::Half: return "1/2";
        case VCSpeedDial::One: return "1";
        case VCSpeedDial::Two: return "2";
        case VCSpeedDial::Four: return "4";
        case VCSpeedDial::Eight: return "8";
        case VCSpeedDial::Sixteen: return "16";
        default: return "none";
    }
}

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
        d.startupIntensityEnabled = button->startupIntensityEnabled();
        d.startupIntensity = button->startupIntensity();
        d.flashOverride = button->flashOverrides();
        d.flashForceLTP = button->flashForceLTP();
        d.stopAllFadeTime = button->stopAllFadeOutTime();
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

        // Widget style and catch values
        d.widgetStyle = (slider->widgetStyle() == VCSlider::WKnob) ? "knob" : "slider";
        d.catchValues = slider->catchValues();
    }

    // CueList-specific
    VCCueList *cuelist = qobject_cast<VCCueList*>(widget);
    if (cuelist)
    {
        d.functionID = cuelist->chaserID();
        switch (cuelist->nextPrevBehavior())
        {
            case VCCueList::DefaultRunFirst: d.nextPrevBehavior = "defaultRunFirst"; break;
            case VCCueList::RunNext: d.nextPrevBehavior = "runNext"; break;
            case VCCueList::Select: d.nextPrevBehavior = "select"; break;
            case VCCueList::Nothing: d.nextPrevBehavior = "nothing"; break;
        }
        switch (cuelist->playbackLayout())
        {
            case VCCueList::PlayPauseStop: d.playbackLayout = "playPauseStop"; break;
            case VCCueList::PlayStopPause: d.playbackLayout = "playStopPause"; break;
        }
        switch (cuelist->sideFaderMode())
        {
            case VCCueList::None: d.sideFaderMode = "none"; break;
            case VCCueList::Crossfade: d.sideFaderMode = "crossfade"; break;
            case VCCueList::Steps: d.sideFaderMode = "steps"; break;
        }
    }

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

        // XY Pad presets
        for (const QVariant &v : xyPad->presetsList())
        {
            QVariantMap pm = v.toMap();
            XYPadPresetInfo pi;
            pi.name = pm.value("name").toString();
            pi.functionID = pm.value("functionID").toUInt();
            pi.type = pm.value("typeString").toString().toLower();
            d.xyPadPresets.append(pi);
        }
    }

    // AudioTriggers-specific
    VCAudioTriggers *audioTrig = qobject_cast<VCAudioTriggers*>(widget);
    if (audioTrig)
    {
        d.captureEnabled = audioTrig->captureEnabled();
        d.volumeLevel = audioTrig->volumeLevel();
        d.barsNumber = audioTrig->barsNumber();

        QVariantList bInfo = audioTrig->barsInfo();
        for (const QVariant &v : bInfo)
        {
            QVariantMap bm = v.toMap();
            WidgetDetails::AudioBarInfo bar;
            bar.barIndex = bm.value("index").toInt();
            int bType = bm.value("type").toInt();
            switch (bType)
            {
                case VCAudioTriggers::DMXBar: bar.type = "dmx"; break;
                case VCAudioTriggers::FunctionBar: bar.type = "function"; break;
                case VCAudioTriggers::VCWidgetBar: bar.type = "widget"; break;
                default: bar.type = "none"; break;
            }
            bar.minThreshold = bm.value("minThreshold").toInt();
            bar.maxThreshold = bm.value("maxThreshold").toInt();

            if (bType == VCAudioTriggers::FunctionBar)
            {
                int fid = bm.value("intVal").toInt();
                if (fid >= 0)
                {
                    bar.functionID = fid;
                    Function *fn = m_doc->function(fid);
                    if (fn) bar.functionName = fn->name();
                }
            }
            else if (bType == VCAudioTriggers::VCWidgetBar)
            {
                int wid = bm.value("intVal").toInt();
                if (wid >= 0)
                {
                    bar.widgetID = wid;
                    bar.widgetName = bm.value("strVal").toString();
                }
            }

            d.audioBars.append(bar);
        }
    }

    // Frame-specific
    VCFrame *frame = qobject_cast<VCFrame*>(widget);
    if (frame && !qobject_cast<VCPage*>(widget))
    {
        d.multipageMode = frame->multiPageMode();
        d.totalPages = frame->totalPagesNumber();
        d.currentPage = frame->currentPage();
        d.pagesLoop = frame->pagesLoop();
        d.headerVisible = frame->showHeader();
        d.enableButtonVisible = frame->showEnable();
        d.collapsed = frame->isCollapsed();

        VCSoloFrame *soloFrame = qobject_cast<VCSoloFrame*>(widget);
        if (soloFrame)
        {
            d.soloframeMixing = soloFrame->soloframeMixing();
            d.excludeMonitoredFunctions = soloFrame->excludeMonitoredFunctions();
        }
    }

    // Animation/Matrix-specific
    VCAnimation *animation = qobject_cast<VCAnimation*>(widget);
    if (animation)
    {
        d.functionID = animation->functionID();
        d.matrixColor1 = animation->getColor1();
        d.matrixColor2 = animation->getColor2();
        d.matrixColor3 = animation->getColor3();
        d.matrixColor4 = animation->getColor4();
        d.matrixColor5 = animation->getColor5();
        QStringList algos = animation->algorithms();
        int algIdx = animation->algorithmIndex();
        if (algIdx >= 0 && algIdx < algos.size())
            d.matrixAnimation = algos.at(algIdx);
        d.matrixInstantApply = animation->instantChanges();
        d.matrixVisibilityMask = animation->visibilityMask();
    }

    // Clock-specific
    VCClock *clock = qobject_cast<VCClock*>(widget);
    if (clock)
    {
        switch (clock->clockType())
        {
            case VCClock::Clock: d.clockType = "clock"; break;
            case VCClock::Stopwatch: d.clockType = "stopwatch"; break;
            case VCClock::Countdown: d.clockType = "countdown"; break;
        }
        int targetMs = clock->targetTime();
        int totalSecs = targetMs / 1000;
        d.countdownH = totalSecs / 3600;
        d.countdownM = (totalSecs % 3600) / 60;
        d.countdownS = totalSecs % 60;

        for (VCClockSchedule *sched : clock->schedules())
        {
            ClockScheduleInfo csi;
            csi.functionID = sched->functionID();
            int startSecs = sched->startTime();
            csi.hour = startSecs / 3600;
            csi.minute = (startSecs % 3600) / 60;
            csi.second = startSecs % 60;
            d.clockSchedules.append(csi);
        }
    }

    // SpeedDial-specific
    VCSpeedDial *speedDial = qobject_cast<VCSpeedDial*>(widget);
    if (speedDial)
    {
        QMap<quint32, VCSpeedDial::VCSpeedDialFunction> funcs = speedDial->functions();
        for (auto it = funcs.constBegin(); it != funcs.constEnd(); ++it)
        {
            SpeedDialFunctionInfo fi;
            fi.functionID = it.value().m_fId;
            fi.fadeInMultiplier = multiplierToString(it.value().m_fadeInFactor);
            fi.fadeOutMultiplier = multiplierToString(it.value().m_fadeOutFactor);
            fi.durationMultiplier = multiplierToString(it.value().m_durationFactor);
            d.speedDialFunctions.append(fi);
        }
        for (const QVariant &v : speedDial->presetsList())
        {
            QVariantMap pm = v.toMap();
            SpeedDialPresetInfo pi;
            pi.name = pm.value("name").toString();
            pi.value = pm.value("value").toInt();
            d.speedDialPresets.append(pi);
        }
        d.absoluteValueMin = speedDial->timeMinimumValue();
        d.absoluteValueMax = speedDial->timeMaximumValue();
        d.speedDialVisibilityMask = speedDial->visibilityMask();
        d.resetFactorOnDialChange = speedDial->resetOnDialChange();
    }

    // Base widget extended properties
    if (widget->hasCustomFont())
    {
        QFont f = widget->font();
        d.fontConfig.family = f.family();
        d.fontConfig.pointSize = f.pointSize();
        d.fontConfig.bold = f.bold();
        d.fontConfig.italic = f.italic();
    }
    d.backgroundImage = widget->backgroundImage();
    d.disabled = widget->isDisabled();

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
        if (v == "colors" || v == "rgb" || v == "cmy") slider->setClickAndGoType(VCSlider::CnGColors);
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

// --- Matrix widget ---

int VCBridgeV5::addMatrix(int parentID, const QRect &geometry,
                          quint32 functionID, const QString &caption)
{
    VCWidget *parent = m_vc->widget(parentID);
    VCFrame *frame = qobject_cast<VCFrame *>(parent);
    if (!frame) return -1;

    VCWidget *widget = frame->addWidget(m_vc->currentPageItem(), "Animation",
                                        QPoint(geometry.x(), geometry.y()));
    if (!widget) return -1;

    VCAnimation *animation = qobject_cast<VCAnimation *>(widget);
    if (animation)
    {
        animation->setGeometry(geometry);
        if (!caption.isEmpty())
            animation->setCaption(caption);
        if (functionID != Function::invalidId())
            animation->setFunctionID(functionID);
    }
    return widget->id();
}

bool VCBridgeV5::configureMatrix(int widgetID, const MatrixConfig &config)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCAnimation *animation = qobject_cast<VCAnimation *>(widget);
    if (!animation) return false;

    if (config.functionID.has_value())
        animation->setFunctionID(config.functionID.value());
    if (config.color1.has_value())
        animation->setColor1(config.color1.value());
    if (config.color2.has_value())
        animation->setColor2(config.color2.value());
    if (config.color3.has_value())
        animation->setColor3(config.color3.value());
    if (config.color4.has_value())
        animation->setColor4(config.color4.value());
    if (config.color5.has_value())
        animation->setColor5(config.color5.value());
    if (config.animation.has_value())
    {
        QStringList algos = animation->algorithms();
        int idx = algos.indexOf(config.animation.value());
        if (idx >= 0)
            animation->setAlgorithmIndex(idx);
    }
    if (config.instantApply.has_value())
        animation->setInstantChanges(config.instantApply.value());
    if (config.visibilityMask.has_value())
        animation->setVisibilityMask(config.visibilityMask.value());
    return true;
}

// --- Button extended config ---

bool VCBridgeV5::configureButton(int widgetID, const ButtonConfig &config)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCButton *button = qobject_cast<VCButton *>(widget);
    if (!button) return false;

    if (config.functionID.has_value())
        button->setFunctionID(config.functionID.value());
    if (config.action.has_value())
    {
        const QString &a = config.action.value();
        if (a == "flash") button->setActionType(VCButton::Flash);
        else if (a == "blackout") button->setActionType(VCButton::Blackout);
        else if (a == "stopall") button->setActionType(VCButton::StopAll);
        else button->setActionType(VCButton::Toggle);
    }
    if (config.startupIntensityEnabled.has_value())
        button->setStartupIntensityEnabled(config.startupIntensityEnabled.value());
    if (config.startupIntensity.has_value())
        button->setStartupIntensity(config.startupIntensity.value());
    if (config.flashOverride.has_value())
        button->setFlashOverride(config.flashOverride.value());
    if (config.flashForceLTP.has_value())
        button->setFlashForceLTP(config.flashForceLTP.value());
    if (config.stopAllFadeTime.has_value())
        button->setStopAllFadeOutTime(config.stopAllFadeTime.value());
    return true;
}

// --- Frame extended config ---

bool VCBridgeV5::configureFrame(int widgetID, const FrameConfig &config)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCFrame *frame = qobject_cast<VCFrame *>(widget);
    if (!frame) return false;

    if (config.multipageMode.has_value())
        frame->setMultiPageMode(config.multipageMode.value());
    if (config.totalPages.has_value())
        frame->setTotalPagesNumber(config.totalPages.value());
    if (config.currentPage.has_value())
        frame->setCurrentPage(config.currentPage.value());
    if (config.pagesLoop.has_value())
        frame->setPagesLoop(config.pagesLoop.value());
    if (config.headerVisible.has_value())
        frame->setShowHeader(config.headerVisible.value());
    if (config.enableButtonVisible.has_value())
        frame->setShowEnable(config.enableButtonVisible.value());
    if (config.collapsed.has_value())
        frame->setCollapsed(config.collapsed.value());

    // SoloFrame-specific properties
    VCSoloFrame *soloFrame = qobject_cast<VCSoloFrame *>(widget);
    if (soloFrame)
    {
        if (config.soloframeMixing.has_value())
            soloFrame->setSoloframeMixing(config.soloframeMixing.value());
        if (config.excludeMonitoredFunctions.has_value())
            soloFrame->setExcludeMonitoredFunctions(config.excludeMonitoredFunctions.value());
    }
    return true;
}

// --- CueList extended config ---

bool VCBridgeV5::configureCueList(int widgetID, const CueListConfig &config)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCCueList *cuelist = qobject_cast<VCCueList *>(widget);
    if (!cuelist) return false;

    if (config.chaserID.has_value())
        cuelist->setChaserID(config.chaserID.value());
    if (config.nextPrevBehavior.has_value())
    {
        const QString &v = config.nextPrevBehavior.value();
        if (v == "runNext") cuelist->setNextPrevBehavior(VCCueList::RunNext);
        else if (v == "select") cuelist->setNextPrevBehavior(VCCueList::Select);
        else if (v == "nothing") cuelist->setNextPrevBehavior(VCCueList::Nothing);
        else cuelist->setNextPrevBehavior(VCCueList::DefaultRunFirst);
    }
    if (config.playbackLayout.has_value())
    {
        const QString &v = config.playbackLayout.value();
        if (v == "playStopPause") cuelist->setPlaybackLayout(VCCueList::PlayStopPause);
        else cuelist->setPlaybackLayout(VCCueList::PlayPauseStop);
    }
    if (config.sideFaderMode.has_value())
    {
        const QString &v = config.sideFaderMode.value();
        if (v == "crossfade") cuelist->setSideFaderMode(VCCueList::Crossfade);
        else if (v == "steps") cuelist->setSideFaderMode(VCCueList::Steps);
        else cuelist->setSideFaderMode(VCCueList::None);
    }
    return true;
}

// --- Clock extended config ---

bool VCBridgeV5::configureClock(int widgetID, const ClockConfig &config)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCClock *clock = qobject_cast<VCClock *>(widget);
    if (!clock) return false;

    if (config.clockType.has_value())
    {
        const QString &v = config.clockType.value();
        if (v == "stopwatch") clock->setClockType(VCClock::Stopwatch);
        else if (v == "countdown") clock->setClockType(VCClock::Countdown);
        else clock->setClockType(VCClock::Clock);
    }
    if (config.countdownH.has_value() || config.countdownM.has_value() || config.countdownS.has_value())
    {
        int h = config.countdownH.value_or(0);
        int m = config.countdownM.value_or(0);
        int s = config.countdownS.value_or(0);
        clock->setTargetTime((h * 3600 + m * 60 + s) * 1000);
    }
    if (config.schedules.has_value())
    {
        // Remove existing schedules (reverse order)
        while (!clock->schedules().isEmpty())
            clock->removeSchedule(0);

        for (const ClockScheduleInfo &info : config.schedules.value())
        {
            VCClockSchedule *sched = new VCClockSchedule(clock);
            sched->setFunctionID(info.functionID);
            sched->setStartTime(info.hour * 3600 + info.minute * 60 + info.second);
            clock->addSchedule(sched);
        }
    }
    return true;
}

// --- SpeedDial extended config ---

static VCSpeedDial::SpeedMultiplier stringToMultiplier(const QString &str)
{
    if (str == "0") return VCSpeedDial::Zero;
    if (str == "1/16") return VCSpeedDial::OneSixteenth;
    if (str == "1/8") return VCSpeedDial::OneEighth;
    if (str == "1/4") return VCSpeedDial::OneFourth;
    if (str == "1/2") return VCSpeedDial::Half;
    if (str == "1") return VCSpeedDial::One;
    if (str == "2") return VCSpeedDial::Two;
    if (str == "4") return VCSpeedDial::Four;
    if (str == "8") return VCSpeedDial::Eight;
    if (str == "16") return VCSpeedDial::Sixteen;
    return VCSpeedDial::None;
}

bool VCBridgeV5::configureSpeedDial(int widgetID, const SpeedDialConfig &config)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCSpeedDial *speedDial = qobject_cast<VCSpeedDial *>(widget);
    if (!speedDial) return false;

    if (config.functions.has_value())
    {
        QMap<quint32, VCSpeedDial::VCSpeedDialFunction> funcMap;
        for (const SpeedDialFunctionInfo &fi : config.functions.value())
        {
            VCSpeedDial::VCSpeedDialFunction f;
            f.m_fId = fi.functionID;
            f.m_fadeInFactor = stringToMultiplier(fi.fadeInMultiplier);
            f.m_fadeOutFactor = stringToMultiplier(fi.fadeOutMultiplier);
            f.m_durationFactor = stringToMultiplier(fi.durationMultiplier);
            funcMap.insert(fi.functionID, f);
        }
        speedDial->setFunctions(funcMap);
    }
    if (config.presets.has_value())
    {
        // Remove existing presets via public API
        QVariantList existingPresets = speedDial->presetsList();
        for (auto it = existingPresets.rbegin(); it != existingPresets.rend(); ++it)
            speedDial->removePreset(it->toMap().value("id").toUInt());

        for (const SpeedDialPresetInfo &pi : config.presets.value())
            speedDial->addPreset(pi.name, pi.value);
    }
    if (config.absoluteValueMin.has_value())
        speedDial->setTimeMinimumValue(config.absoluteValueMin.value());
    if (config.absoluteValueMax.has_value())
        speedDial->setTimeMaximumValue(config.absoluteValueMax.value());
    if (config.visibilityMask.has_value())
        speedDial->setVisibilityMask(config.visibilityMask.value());
    if (config.resetFactorOnDialChange.has_value())
        speedDial->setResetOnDialChange(config.resetFactorOnDialChange.value());
    return true;
}

// --- XY Pad presets ---

bool VCBridgeV5::setXYPadPresets(int widgetID, const QList<XYPadPresetInfo> &presets)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCXYPad *xyPad = qobject_cast<VCXYPad *>(widget);
    if (!xyPad) return false;

    // Remove existing presets via public API
    QVariantList existingPresets = xyPad->presetsList();
    for (auto it = existingPresets.rbegin(); it != existingPresets.rend(); ++it)
        xyPad->removePreset(it->toMap().value("id").toUInt());

    for (const XYPadPresetInfo &info : presets)
    {
        if (info.type == "position")
        {
            // Set current position before creating preset so it captures it
            xyPad->setCurrentPosition(info.position);
            int presetId = xyPad->addPositionPreset();
            if (presetId >= 0 && !info.name.isEmpty())
                xyPad->setPresetName(static_cast<quint8>(presetId), info.name);
        }
        else if (info.type == "efx" || info.type == "scene")
        {
            int presetId = xyPad->addFunctionPreset(info.functionID);
            if (presetId >= 0 && !info.name.isEmpty())
                xyPad->setPresetName(static_cast<quint8>(presetId), info.name);
        }
    }
    return true;
}

// --- Key sequences ---

static int resolveControlId(VCWidget *widget, const QString &sourceName)
{
    // Control IDs are defined per widget type
    VCButton *btn = qobject_cast<VCButton *>(widget);
    if (btn)
    {
        if (sourceName == "default") return 0;
        return -1;
    }

    VCSlider *slider = qobject_cast<VCSlider *>(widget);
    if (slider)
    {
        if (sourceName == "default") return 0;
        if (sourceName == "overrideReset") return 1;
        if (sourceName == "flashButton") return 2;
        return -1;
    }

    VCCueList *cuelist = qobject_cast<VCCueList *>(widget);
    if (cuelist)
    {
        if (sourceName == "next") return 0;
        if (sourceName == "previous") return 1;
        if (sourceName == "playback") return 2;
        if (sourceName == "stop") return 3;
        return -1;
    }

    VCClock *clock = qobject_cast<VCClock *>(widget);
    if (clock)
    {
        if (sourceName == "play") return 0;
        if (sourceName == "reset") return 1;
        return -1;
    }

    VCSpeedDial *speedDial = qobject_cast<VCSpeedDial *>(widget);
    if (speedDial)
    {
        if (sourceName == "tap") return 1;
        if (sourceName == "mult") return 2;
        if (sourceName == "div") return 3;
        if (sourceName == "multDivReset") return 4;
        if (sourceName == "apply") return 5;
        return -1;
    }

    VCFrame *frame = qobject_cast<VCFrame *>(widget);
    if (frame)
    {
        if (sourceName == "nextPage") return 0;
        if (sourceName == "previousPage") return 1;
        if (sourceName == "enable") return 2;
        return -1;
    }

    VCAudioTriggers *audioTrig = qobject_cast<VCAudioTriggers *>(widget);
    if (audioTrig)
    {
        if (sourceName == "default") return 0;
        return -1;
    }

    return -1;
}

bool VCBridgeV5::setWidgetKeySequence(int widgetID, const QString &sourceName,
                                      const QKeySequence &keySequence)
{
    VCWidget *widget = m_vc->widget(widgetID);
    if (!widget) return false;

    int controlId = resolveControlId(widget, sourceName);
    if (controlId < 0) return false;

    // Remove existing key sequence for this control ID
    QMap<QKeySequence, quint32> existing = widget->keySequenceMap();
    for (auto it = existing.constBegin(); it != existing.constEnd(); ++it)
    {
        if (it.value() == (quint32)controlId)
            widget->deleteKeySequence(it.key());
    }

    if (!keySequence.isEmpty())
        widget->addKeySequence(keySequence, (quint32)controlId);
    return true;
}

// --- Named input mapping ---

bool VCBridgeV5::mapWidgetInputByName(int widgetID, const QString &sourceName,
                                      quint32 universe, quint32 channel)
{
    VCWidget *widget = m_vc->widget(widgetID);
    if (!widget) return false;

    int controlId = resolveControlId(widget, sourceName);
    if (controlId < 0) return false;

    // Remove existing input source for this control ID
    for (auto &src : widget->inputSources())
    {
        if (src->id() == (quint32)controlId)
        {
            widget->deleteInputSurce(src->id(), src->universe(), src->channel());
            break;
        }
    }

    QSharedPointer<QLCInputSource> source(new QLCInputSource(universe, channel));
    source->setID((quint32)controlId);
    widget->addInputSource(source);

    // Register with page for event routing
    for (int i = 0; i < m_vc->pagesCount(); i++)
    {
        VCPage *page = m_vc->page(i);
        if (page)
            page->mapInputSource(source, widget, true);
    }
    return true;
}

// --- Base widget properties ---

bool VCBridgeV5::setWidgetFont(int widgetID, const FontConfig &font)
{
    VCWidget *widget = m_vc->widget(widgetID);
    if (!widget) return false;

    QFont f = widget->font();
    if (font.family.has_value())
        f.setFamily(font.family.value());
    if (font.pointSize.has_value())
        f.setPointSize(font.pointSize.value());
    if (font.bold.has_value())
        f.setBold(font.bold.value());
    if (font.italic.has_value())
        f.setItalic(font.italic.value());
    widget->setFont(f);
    return true;
}

bool VCBridgeV5::setWidgetBackgroundImage(int widgetID, const QString &path)
{
    VCWidget *widget = m_vc->widget(widgetID);
    if (!widget) return false;
    widget->setBackgroundImage(path);
    return true;
}

bool VCBridgeV5::setWidgetDisableState(int widgetID, bool disabled)
{
    VCWidget *widget = m_vc->widget(widgetID);
    if (!widget) return false;
    widget->setDisabled(disabled);
    return true;
}

// --- Page rename ---

bool VCBridgeV5::renamePage(int pageIndex, const QString &name)
{
    VCPage *page = m_vc->page(pageIndex);
    if (!page) return false;
    page->setCaption(name);
    return true;
}

// --- Slider extended ---

bool VCBridgeV5::setSliderWidgetStyle(int widgetID, const QString &style)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCSlider *slider = qobject_cast<VCSlider *>(widget);
    if (!slider) return false;

    if (style == "knob")
        slider->setWidgetStyle(VCSlider::WKnob);
    else
        slider->setWidgetStyle(VCSlider::WSlider);
    return true;
}

bool VCBridgeV5::setSliderCatchValues(int widgetID, bool enable)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCSlider *slider = qobject_cast<VCSlider *>(widget);
    if (!slider) return false;
    slider->setCatchValues(enable);
    return true;
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
