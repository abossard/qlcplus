/*
  Q Light Controller Plus
  vcbridgev4.cpp

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

#include "vcbridgev4.h"
#include "vc_tools_common.h"
#include "virtualconsole.h"
#include "vcframe.h"
#include "vcsoloframe.h"
#include "vcbutton.h"
#include "vcslider.h"
#include "vcxypad.h"
#include "vcxypadfixture.h"
#include "vcxypadpreset.h"
#include "vccuelist.h"
#include "vclabel.h"
#include "vcspeeddial.h"
#include "vcspeeddialfunction.h"
#include "vcspeeddialpreset.h"
#include "vcaudiotriggers.h"
#include "vcclock.h"
#include "vcmatrix.h"
#include "vcwidget.h"
#include "vcframepageshortcut.h"
#include "audiobar.h"
#include "doc.h"
#include "fixture.h"
#include "function.h"
#include "scenevalue.h"
#include "grandmaster.h"
#include "inputoutputmap.h"
#include "qlcinputsource.h"
#include "qlcinputfeedback.h"

#include <QDebug>

// ---------------------------------------------------------------------------
// Helper: resolve parentID to a VCFrame*. Falls back to root frame for
// invalid IDs (including -1 / UINT_MAX). This is the single point of
// parent resolution — all addWidget methods must use this.
// ---------------------------------------------------------------------------

static VCFrame *resolveParentFrame(VirtualConsole *vc, int parentID)
{
    // -1 (or any invalid ID) → root frame
    if (parentID < 0)
        return vc->contents();

    VCWidget *w = vc->widget(static_cast<quint32>(parentID));
    if (!w)
        return vc->contents();

    VCFrame *frame = qobject_cast<VCFrame *>(w);
    return frame ? frame : vc->contents();
}

// ---------------------------------------------------------------------------
// Safe widget registration for programmatic creation.
// Unlike VirtualConsole::setupWidget(), this does NOT call
// clearWidgetSelection() (crashes on stale pointers) or
// move(lastClickPoint()) (overwrites intended geometry).
// ---------------------------------------------------------------------------

static void registerWidget(VirtualConsole *vc, VCWidget *widget, VCFrame *parent)
{
    vc->addWidgetInMap(widget);

    // Inline connectWidgetToParent logic (it's private in VirtualConsole)
    if (parent->multipageMode())
    {
        widget->setPage(parent->currentPage());
        parent->addWidgetToPageMap(widget);
    }
    else
    {
        widget->setPage(0);
    }

    widget->show();
    widget->raise();
}

// ---------------------------------------------------------------------------
// Source definition table — maps widget class → valid {name, ID, description}.
// IDs match v4's static quint8 constants.
// ---------------------------------------------------------------------------

// V4 input source IDs per widget type:
//   VCFrame: nextPage=0, previousPage=1, enable=2, shortcuts=20+
//   VCButton: default=0
//   VCSlider: slider=0, overrideReset=1, flashButton=2
//   VCXYPad: pan=0, tilt=1, width=2, height=3, panFine=4, tiltFine=5
//   VCCueList: next=0, previous=1, playback=2, stop=3, sideFader=4
//   VCSpeedDial: absolute=0, tap=1, mult=2, div=3, multDivReset=4, apply=5
//   VCClock: play=0, reset=1
//   VCAudioTriggers: default=0, volumeControl=1

static const QMap<QString, QList<VCBridge::SourceDef>> s_sourceDefTable = {
    {"VCButton",        {{"default", 0, "Button pressure"}}},
    {"VCSlider",        {{"default", 0, "Slider control"},
                         {"overrideReset", 1, "Reset control"},
                         {"flashButton", 2, "Flash control"}}},
    {"VCCueList",       {{"next", 0, "Next cue"},
                         {"previous", 1, "Previous cue"},
                         {"playback", 2, "Play/stop/pause"},
                         {"stop", 3, "Stop/pause"},
                         {"sideFader", 4, "Side fader"}}},
    {"VCXYPad",         {{"pan", 0, "Pan / Horizontal axis"},
                         {"tilt", 1, "Tilt / Vertical axis"},
                         {"width", 2, "Width"},
                         {"height", 3, "Height"},
                         {"panFine", 4, "Pan fine"},
                         {"tiltFine", 5, "Tilt fine"}}},
    {"VCSpeedDial",     {{"absolute", 0, "Time wheel"},
                         {"tap", 1, "Tap button"},
                         {"mult", 2, "Multiply button"},
                         {"div", 3, "Divide button"},
                         {"multDivReset", 4, "Reset button"},
                         {"apply", 5, "Apply button"}}},
    {"VCClock",         {{"play", 0, "Play/pause timer"},
                         {"reset", 1, "Reset timer"}}},
    {"VCFrame",         {{"nextPage", 0, "Next page"},
                         {"previousPage", 1, "Previous page"},
                         {"enable", 2, "Enable/disable frame"}}},
    {"VCSoloFrame",     {{"nextPage", 0, "Next page"},
                         {"previousPage", 1, "Previous page"},
                         {"enable", 2, "Enable/disable frame"}}},
    {"VCAudioTriggers", {{"default", 0, "Enable/disable capture"},
                         {"volumeControl", 1, "Volume control"}}},
    {"VCMatrix",        {{"default", 0, "Intensity fader"}}},
    {"VCLabel",         {}},
};

// V4 VCFrame shortcut base ID
static const quint8 V4_SHORTCUT_BASE_ID = 20;

static int resolveControlId(VCWidget *widget, const QString &sourceName)
{
    if (!widget) return -1;

    // Frame/SoloFrame page shortcuts: "shortcut0", "shortcut1", ...
    if (sourceName.startsWith("shortcut"))
    {
        if (!qobject_cast<VCFrame *>(widget)) return -1;
        bool ok = false;
        int idx = sourceName.mid(8).toInt(&ok);
        if (!ok || idx < 0) return -1;
        return V4_SHORTCUT_BASE_ID + idx;
    }

    // Frame/SoloFrame legacy page syntax: "page0", "page1", ...
    if (sourceName.startsWith("page"))
    {
        if (!qobject_cast<VCFrame *>(widget)) return -1;
        bool ok = false;
        int idx = sourceName.mid(4).toInt(&ok);
        if (!ok || idx < 0) return -1;
        return V4_SHORTCUT_BASE_ID + idx;
    }

    // XYPad/SpeedDial presets: "preset0", "preset1", ...
    if (sourceName.startsWith("preset"))
    {
        if (!qobject_cast<VCXYPad *>(widget) && !qobject_cast<VCSpeedDial *>(widget))
            return -1;
        bool ok = false;
        int idx = sourceName.mid(6).toInt(&ok);
        if (!ok || idx < 0) return -1;
        return 30 + idx;
    }

    // Static table lookup
    QString typeName = QString::fromLatin1(widget->metaObject()->className());
    auto it = s_sourceDefTable.constFind(typeName);
    if (it == s_sourceDefTable.constEnd()) return -1;
    for (const auto &def : it.value())
    {
        if (def.name == sourceName) return def.id;
    }
    return -1;
}

static QString resolveSourceName(VCWidget *widget, quint32 id)
{
    if (!widget) return QString();

    // Frame shortcuts range
    if (id >= V4_SHORTCUT_BASE_ID && qobject_cast<VCFrame *>(widget))
        return QStringLiteral("shortcut%1").arg(id - V4_SHORTCUT_BASE_ID);

    // XYPad/SpeedDial presets range
    if (id >= 30 && (qobject_cast<VCXYPad *>(widget) || qobject_cast<VCSpeedDial *>(widget)))
        return QStringLiteral("preset%1").arg(id - 30);

    // Static table reverse lookup
    QString typeName = QString::fromLatin1(widget->metaObject()->className());
    auto defs = s_sourceDefTable.value(typeName);
    for (const auto &def : defs)
    {
        if (def.id == id) return def.name;
    }
    return QString();
}

// ---------------------------------------------------------------------------
// Multiplier helpers (v4 uses VCSpeedDialFunction::SpeedMultiplier)
// ---------------------------------------------------------------------------

static QString multiplierToString(VCSpeedDialFunction::SpeedMultiplier m)
{
    switch (m)
    {
        case VCSpeedDialFunction::Zero: return "0";
        case VCSpeedDialFunction::OneSixteenth: return "1/16";
        case VCSpeedDialFunction::OneEighth: return "1/8";
        case VCSpeedDialFunction::OneFourth: return "1/4";
        case VCSpeedDialFunction::Half: return "1/2";
        case VCSpeedDialFunction::One: return "1";
        case VCSpeedDialFunction::Two: return "2";
        case VCSpeedDialFunction::Four: return "4";
        case VCSpeedDialFunction::Eight: return "8";
        case VCSpeedDialFunction::Sixteen: return "16";
        default: return "none";
    }
}

static VCSpeedDialFunction::SpeedMultiplier stringToMultiplier(const QString &str)
{
    if (str == "0") return VCSpeedDialFunction::Zero;
    if (str == "1/16") return VCSpeedDialFunction::OneSixteenth;
    if (str == "1/8") return VCSpeedDialFunction::OneEighth;
    if (str == "1/4") return VCSpeedDialFunction::OneFourth;
    if (str == "1/2") return VCSpeedDialFunction::Half;
    if (str == "1") return VCSpeedDialFunction::One;
    if (str == "2") return VCSpeedDialFunction::Two;
    if (str == "4") return VCSpeedDialFunction::Four;
    if (str == "8") return VCSpeedDialFunction::Eight;
    if (str == "16") return VCSpeedDialFunction::Sixteen;
    return VCSpeedDialFunction::None;
}

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

VCBridgeV4::VCBridgeV4(Doc *doc, VirtualConsole *vc)
    : m_doc(doc)
    , m_vc(vc)
{
}

QRect VCBridgeV4::snapRect(const QRect &rect) const
{
    int s = snappingSize();
    if (s <= 1) return rect;

    return QRect(
        qRound((qreal)rect.x() / s) * s,
        qRound((qreal)rect.y() / s) * s,
        qMax(qRound((qreal)rect.width() / s) * s, s),
        qMax(qRound((qreal)rect.height() / s) * s, s)
    );
}

int VCBridgeV4::snappingSize() const
{
    // v4 uses VCProperties for grid; default 10
    // VCProperties doesn't expose gridSize directly; use 5 as default
    return 5;
}

// ---------------------------------------------------------------------------
// Pages — v4 uses root VCFrame multipage mode
// ---------------------------------------------------------------------------

int VCBridgeV4::addPage(const QString &name)
{
    VCFrame *root = m_vc->contents();
    if (!root) return -1;

    if (!root->multipageMode())
    {
        // Ensure header layout exists before enabling multipage
        // (setMultipageMode inserts widgets into m_hbox which is created by the header)
        root->setHeaderVisible(true);
        root->setMultipageMode(true);
        root->setTotalPagesNumber(1);
    }

    int newTotal = root->totalPagesNumber() + 1;
    root->setTotalPagesNumber(newTotal);

    // Set page name via shortcuts
    int pageIdx = newTotal - 1;
    QList<VCFramePageShortcut *> shortcuts = root->shortcuts();
    if (pageIdx < shortcuts.size() && !name.isEmpty())
        shortcuts.at(pageIdx)->setName(name);

    return pageIdx;
}

QList<VCBridge::PageInfo> VCBridgeV4::pages() const
{
    QList<PageInfo> result;
    VCFrame *root = m_vc->contents();
    if (!root) return result;

    int numPages = root->multipageMode() ? root->totalPagesNumber() : 1;

    for (int p = 0; p < numPages; p++)
    {
        PageInfo pi;
        pi.index = p;

        // Get page name from shortcuts
        QList<VCFramePageShortcut *> shortcuts = root->shortcuts();
        if (p < shortcuts.size() && !shortcuts.at(p)->name().isEmpty())
            pi.name = shortcuts.at(p)->name();
        else
            pi.name = QString("Page %1").arg(p + 1);

        // Collect children on this page
        QList<QObject *> children = root->children();
        for (QObject *obj : children)
        {
            VCWidget *w = qobject_cast<VCWidget *>(obj);
            if (!w) continue;
            if (root->multipageMode() && w->page() != p) continue;

            WidgetInfo wi;
            wi.id = w->id();
            wi.type = VCType::widgetTypeToMcp(w->type());
            wi.caption = w->caption();
            wi.geometry = w->geometry();
            wi.functionID = Function::invalidId();

            VCButton *btn = qobject_cast<VCButton *>(w);
            if (btn) wi.functionID = btn->function();
            VCCueList *cl = qobject_cast<VCCueList *>(w);
            if (cl) wi.functionID = cl->chaserID();

            pi.widgets.append(wi);
        }

        result.append(pi);
    }
    return result;
}

int VCBridgeV4::pagesCount() const
{
    VCFrame *root = m_vc->contents();
    if (!root) return 0;
    return root->multipageMode() ? root->totalPagesNumber() : 1;
}

// ---------------------------------------------------------------------------
// Frames
// ---------------------------------------------------------------------------

int VCBridgeV4::addFrame(int pageIndex, const QRect &geometry,
                         const QString &caption, bool solo)
{
    VCFrame *root = m_vc->contents();
    if (!root) return -1;

    VCWidget *widget;
    if (solo)
        widget = new VCSoloFrame(root, m_doc);
    else
        widget = new VCFrame(root, m_doc);

    registerWidget(m_vc, widget, root);

    widget->setGeometry(snapRect(geometry));
    widget->setCaption(caption);

    // If multipage, assign to the requested page
    if (root->multipageMode())
    {
        widget->setPage(pageIndex);
        root->addWidgetToPageMap(widget);
    }

    return widget->id();
}

int VCBridgeV4::addFrameInFrame(int parentID, const QRect &geometry,
                                 const QString &caption, bool solo)
{
    VCFrame *frame = resolveParentFrame(m_vc, parentID);
    if (!frame) return -1;

    VCWidget *widget;
    if (solo)
        widget = new VCSoloFrame(frame, m_doc);
    else
        widget = new VCFrame(frame, m_doc);

    registerWidget(m_vc, widget, frame);

    widget->setGeometry(snapRect(geometry));
    widget->setCaption(caption);

    return widget->id();
}

// ---------------------------------------------------------------------------
// Button
// ---------------------------------------------------------------------------

int VCBridgeV4::addButton(int parentID, const QRect &geometry,
                          quint32 functionID, const QString &caption,
                          const QString &action,
                          int stopAllFadeTime)
{
    VCFrame *frame = resolveParentFrame(m_vc, parentID);
    if (!frame) return -1;

    VCButton *button = new VCButton(frame, m_doc);
    registerWidget(m_vc, button, frame);

    button->setGeometry(snapRect(geometry));
    button->setCaption(caption);
    if (functionID != Function::invalidId())
        button->setFunction(functionID);

    if (action == "flash")
        button->setAction(VCButton::Flash);
    else if (action == "blackout")
        button->setAction(VCButton::Blackout);
    else if (action == "stopall")
    {
        button->setAction(VCButton::StopAll);
        if (stopAllFadeTime > 0)
            button->setStopAllFadeOutTime(stopAllFadeTime);
    }

    return button->id();
}

// ---------------------------------------------------------------------------
// Slider
// ---------------------------------------------------------------------------

int VCBridgeV4::addSlider(int parentID, const QRect &geometry,
                          const QString &mode, const QString &caption,
                          quint32 functionID,
                          const QList<QPair<quint32, quint32>> &channels)
{
    VCFrame *frame = resolveParentFrame(m_vc, parentID);
    if (!frame) return -1;

    VCSlider *slider = new VCSlider(frame, m_doc);
    registerWidget(m_vc, slider, frame);

    slider->setGeometry(snapRect(geometry));
    slider->setCaption(caption);

    if (mode == "submaster")
        slider->setSliderMode(VCSlider::Submaster);
    else if (mode == "playback")
    {
        slider->setSliderMode(VCSlider::Playback);
        slider->setPlaybackFunction(functionID);
    }
    else // "level"
    {
        slider->setSliderMode(VCSlider::Level);
        for (const auto &ch : channels)
            slider->addLevelChannel(ch.first, ch.second);
    }

    return slider->id();
}

// ---------------------------------------------------------------------------
// XY Pad
// ---------------------------------------------------------------------------

int VCBridgeV4::addXYPad(int parentID, const QRect &geometry,
                         const QList<quint32> &fixtureIDs)
{
    QList<XYPadFixtureConfig> configs;
    for (quint32 fxID : fixtureIDs)
    {
        XYPadFixtureConfig cfg;
        cfg.fixtureID = fxID;
        configs.append(cfg);
    }
    return addXYPadEx(parentID, geometry, configs, "degrees", false);
}

int VCBridgeV4::addXYPadEx(int parentID, const QRect &geometry,
                            const QList<XYPadFixtureConfig> &fixtures,
                            const QString &displayMode,
                            bool invertedAppearance)
{
    VCFrame *frame = resolveParentFrame(m_vc, parentID);
    if (!frame) return -1;

    VCXYPad *xyPad = new VCXYPad(frame, m_doc);
    registerWidget(m_vc, xyPad, frame);
    xyPad->setGeometry(snapRect(geometry));

    for (const XYPadFixtureConfig &cfg : fixtures)
    {
        Fixture *fxi = m_doc->fixture(cfg.fixtureID);
        if (!fxi) continue;

        VCXYPadFixture padFxi(m_doc);
        padFxi.setHead(GroupHead(cfg.fixtureID, cfg.head));
        padFxi.setX(cfg.xMin, cfg.xMax, cfg.xReverse);
        padFxi.setY(cfg.yMin, cfg.yMax, cfg.yReverse);
        xyPad->appendFixture(padFxi);
    }

    xyPad->setInvertedAppearance(invertedAppearance);
    Q_UNUSED(displayMode);
    // v4 VCXYPad doesn't have a display mode property

    return xyPad->id();
}

bool VCBridgeV4::setXYPadPosition(int widgetID, qreal x, qreal y)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCXYPad *xyPad = qobject_cast<VCXYPad *>(widget);
    if (!xyPad) return false;

    // v4 XY pad receives position via slotPositionChanged in DMX coords (0-255)
    // Scale from 0.0-1.0 to 0-255 range
    xyPad->slotPositionChanged(QPointF(x * 255.0, y * 255.0));
    return true;
}

bool VCBridgeV4::setXYPadDisplayMode(int widgetID, const QString &mode)
{
    Q_UNUSED(widgetID);
    Q_UNUSED(mode);
    // v4 VCXYPad doesn't have display mode property
    return false;
}

bool VCBridgeV4::setXYPadInvertedAppearance(int widgetID, bool inverted)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCXYPad *xyPad = qobject_cast<VCXYPad *>(widget);
    if (!xyPad) return false;
    xyPad->setInvertedAppearance(inverted);
    return true;
}

bool VCBridgeV4::addXYPadFixture(int widgetID, const XYPadFixtureConfig &config)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCXYPad *xyPad = qobject_cast<VCXYPad *>(widget);
    if (!xyPad) return false;

    Fixture *fxi = m_doc->fixture(config.fixtureID);
    if (!fxi) return false;

    VCXYPadFixture padFxi(m_doc);
    padFxi.setHead(GroupHead(config.fixtureID, config.head));
    padFxi.setX(config.xMin, config.xMax, config.xReverse);
    padFxi.setY(config.yMin, config.yMax, config.yReverse);
    xyPad->appendFixture(padFxi);
    return true;
}

bool VCBridgeV4::removeXYPadFixture(int widgetID, quint32 fixtureID, int head)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCXYPad *xyPad = qobject_cast<VCXYPad *>(widget);
    if (!xyPad) return false;
    xyPad->removeFixture(GroupHead(fixtureID, head));
    return true;
}

// ---------------------------------------------------------------------------
// CueList
// ---------------------------------------------------------------------------

int VCBridgeV4::addCueList(int parentID, const QRect &geometry,
                           quint32 chaserID, const QString &caption)
{
    VCFrame *frame = resolveParentFrame(m_vc, parentID);
    if (!frame) return -1;

    VCCueList *cuelist = new VCCueList(frame, m_doc);
    registerWidget(m_vc, cuelist, frame);

    cuelist->setGeometry(snapRect(geometry));
    cuelist->setChaser(chaserID);
    if (!caption.isEmpty())
        cuelist->setCaption(caption);

    return cuelist->id();
}

// ---------------------------------------------------------------------------
// Label
// ---------------------------------------------------------------------------

int VCBridgeV4::addLabel(int parentID, const QRect &geometry,
                         const QString &text)
{
    VCFrame *frame = resolveParentFrame(m_vc, parentID);
    if (!frame) return -1;

    VCLabel *label = new VCLabel(frame, m_doc);
    registerWidget(m_vc, label, frame);

    label->setGeometry(snapRect(geometry));
    label->setCaption(text);

    return label->id();
}

// ---------------------------------------------------------------------------
// Input mapping
// ---------------------------------------------------------------------------

bool VCBridgeV4::mapWidgetInput(int widgetID, quint32 universe, quint32 channel)
{
    VCWidget *widget = m_vc->widget(widgetID);
    if (!widget) return false;

    QSharedPointer<QLCInputSource> source(new QLCInputSource(universe, channel));
    widget->setInputSource(source);
    return true;
}

bool VCBridgeV4::setWidgetFeedback(int widgetID,
                                    int idleValue, int activeValue, int monitorValue,
                                    int idleMidiCh, int activeMidiCh, int monitorMidiCh)
{
    VCWidget *widget = m_vc->widget(widgetID);
    if (!widget) return false;

    QSharedPointer<QLCInputSource> src = widget->inputSource();
    if (src.isNull()) return false;

    src->setFeedbackValue(QLCInputFeedback::LowerValue, (uchar)idleValue);
    src->setFeedbackValue(QLCInputFeedback::UpperValue, (uchar)activeValue);
    src->setFeedbackValue(QLCInputFeedback::MonitorValue, (uchar)monitorValue);
    src->setFeedbackExtraParams(QLCInputFeedback::LowerValue, QVariant(idleMidiCh));
    src->setFeedbackExtraParams(QLCInputFeedback::UpperValue, QVariant(activeMidiCh));
    src->setFeedbackExtraParams(QLCInputFeedback::MonitorValue, QVariant(monitorMidiCh));
    return true;
}

VCBridge::FeedbackInfo VCBridgeV4::getWidgetFeedback(int widgetID) const
{
    FeedbackInfo fb;
    VCWidget *widget = m_vc->widget(widgetID);
    if (!widget) return fb;

    QSharedPointer<QLCInputSource> src = widget->inputSource();
    if (src.isNull()) return fb;

    fb.idleValue = src->feedbackValue(QLCInputFeedback::LowerValue);
    fb.activeValue = src->feedbackValue(QLCInputFeedback::UpperValue);
    fb.monitorValue = src->feedbackValue(QLCInputFeedback::MonitorValue);
    fb.idleMidiCh = src->feedbackExtraParams(QLCInputFeedback::LowerValue).toInt();
    fb.activeMidiCh = src->feedbackExtraParams(QLCInputFeedback::UpperValue).toInt();
    fb.monitorMidiCh = src->feedbackExtraParams(QLCInputFeedback::MonitorValue).toInt();
    return fb;
}

int VCBridgeV4::widgetInputSourceCount(int widgetID) const
{
    VCWidget *widget = m_vc->widget(widgetID);
    if (!widget) return 0;

    // v4 stores inputs in QHash<quint8, QSharedPointer<QLCInputSource>>
    // Count non-null entries
    int count = 0;
    // Access the default source and type-specific ones
    // A simple approach: check IDs 0..31
    for (quint8 id = 0; id < 32; id++)
    {
        QSharedPointer<QLCInputSource> src = widget->inputSource(id);
        if (!src.isNull() && src->isValid())
            count++;
    }
    return count;
}

QList<VCBridge::SourceDef> VCBridgeV4::getWidgetSourceDefs(int widgetID) const
{
    VCWidget *widget = m_vc->widget(widgetID);
    if (!widget) return {};

    QString typeName = QString::fromLatin1(widget->metaObject()->className());
    auto result = s_sourceDefTable.value(typeName);

    // For multipage frames, add dynamic shortcut entries
    VCFrame *frame = qobject_cast<VCFrame *>(widget);
    if (frame && frame->multipageMode() && frame->totalPagesNumber() > 1)
    {
        for (int i = 0; i < frame->totalPagesNumber(); i++)
            result.append({QStringLiteral("shortcut%1").arg(i),
                          quint32(V4_SHORTCUT_BASE_ID + i),
                          QStringLiteral("Jump to page %1").arg(i)});
    }
    return result;
}

bool VCBridgeV4::setWidgetFeedbackByName(int widgetID, const QString &sourceName,
                                          int idleVal, int activeVal, int monitorVal,
                                          int idleCh, int activeCh, int monitorCh)
{
    VCWidget *widget = m_vc->widget(widgetID);
    if (!widget) return false;

    int controlId = resolveControlId(widget, sourceName);
    if (controlId < 0) return false;

    QSharedPointer<QLCInputSource> src = widget->inputSource((quint8)controlId);
    if (src.isNull() || !src->isValid()) return false;

    src->setFeedbackValue(QLCInputFeedback::LowerValue, (uchar)idleVal);
    src->setFeedbackValue(QLCInputFeedback::UpperValue, (uchar)activeVal);
    src->setFeedbackValue(QLCInputFeedback::MonitorValue, (uchar)monitorVal);
    src->setFeedbackExtraParams(QLCInputFeedback::LowerValue, QVariant(idleCh));
    src->setFeedbackExtraParams(QLCInputFeedback::UpperValue, QVariant(activeCh));
    src->setFeedbackExtraParams(QLCInputFeedback::MonitorValue, QVariant(monitorCh));
    return true;
}

VCBridge::FeedbackInfo VCBridgeV4::getWidgetFeedbackByName(int widgetID, const QString &sourceName) const
{
    FeedbackInfo fb;
    VCWidget *widget = m_vc->widget(widgetID);
    if (!widget) return fb;

    int controlId = resolveControlId(widget, sourceName);
    if (controlId < 0) return fb;

    QSharedPointer<QLCInputSource> src = widget->inputSource((quint8)controlId);
    if (src.isNull() || !src->isValid()) return fb;

    fb.idleValue = src->feedbackValue(QLCInputFeedback::LowerValue);
    fb.activeValue = src->feedbackValue(QLCInputFeedback::UpperValue);
    fb.monitorValue = src->feedbackValue(QLCInputFeedback::MonitorValue);
    fb.idleMidiCh = src->feedbackExtraParams(QLCInputFeedback::LowerValue).toInt();
    fb.activeMidiCh = src->feedbackExtraParams(QLCInputFeedback::UpperValue).toInt();
    fb.monitorMidiCh = src->feedbackExtraParams(QLCInputFeedback::MonitorValue).toInt();
    return fb;
}

// ---------------------------------------------------------------------------
// Widget colors
// ---------------------------------------------------------------------------

bool VCBridgeV4::setWidgetColors(int widgetID, const QColor &bgColor, const QColor &fgColor)
{
    VCWidget *widget = m_vc->widget(widgetID);
    if (!widget) return false;

    if (bgColor.isValid())
        widget->setBackgroundColor(bgColor);
    if (fgColor.isValid())
        widget->setForegroundColor(fgColor);
    return true;
}

// ---------------------------------------------------------------------------
// Speed Dial
// ---------------------------------------------------------------------------

int VCBridgeV4::addSpeedDial(int parentID, const QRect &geometry,
                              const QList<quint32> &functionIDs)
{
    VCFrame *frame = resolveParentFrame(m_vc, parentID);
    if (!frame) return -1;

    VCSpeedDial *speedDial = new VCSpeedDial(frame, m_doc);
    registerWidget(m_vc, speedDial, frame);
    speedDial->setGeometry(snapRect(geometry));

    QList<VCSpeedDialFunction> funcs;
    for (quint32 fid : functionIDs)
        funcs.append(VCSpeedDialFunction(fid));
    speedDial->setFunctions(funcs);

    return speedDial->id();
}

// ---------------------------------------------------------------------------
// Audio Triggers
// ---------------------------------------------------------------------------

int VCBridgeV4::addAudioTriggers(int parentID, const QRect &geometry)
{
    VCFrame *frame = resolveParentFrame(m_vc, parentID);
    if (!frame) return -1;

    VCAudioTriggers *triggers = new VCAudioTriggers(frame, m_doc);
    registerWidget(m_vc, triggers, frame);
    triggers->setGeometry(snapRect(geometry));

    return triggers->id();
}

bool VCBridgeV4::configureAudioTriggerBar(int widgetID, const AudioBarConfig &config)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCAudioTriggers *at = qobject_cast<VCAudioTriggers *>(widget);
    if (!at) return false;

    AudioBar *bar = at->getSpectrumBar(config.barIndex);
    if (!bar) return false;

    // Set bar type
    int barType = AudioBar::None;
    if (config.type == "dmx") barType = AudioBar::DMXBar;
    else if (config.type == "function") barType = AudioBar::FunctionBar;
    else if (config.type == "widget") barType = AudioBar::VCWidgetBar;
    at->setSpectrumBarType(config.barIndex, barType);

    // Set thresholds (convert from 0-100 scale to 0-255)
    bar->m_minThreshold = (uchar)(config.minThreshold * 255 / 100);
    bar->m_maxThreshold = (uchar)(config.maxThreshold * 255 / 100);
    bar->m_divisor = config.divisor;

    // Set type-specific assignments
    if (barType == AudioBar::FunctionBar && config.functionID != (quint32)-1)
        bar->m_function = m_doc->function(config.functionID);
    else if (barType == AudioBar::VCWidgetBar && config.widgetID != (quint32)-1)
        bar->m_widgetID = config.widgetID;
    else if (barType == AudioBar::DMXBar && !config.dmxChannels.isEmpty())
    {
        bar->m_dmxChannels.clear();
        for (const auto &ch : config.dmxChannels)
            bar->m_dmxChannels.append(SceneValue(ch.first, ch.second));
    }

    return true;
}

bool VCBridgeV4::setAudioTriggerCapture(int widgetID, bool enabled)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCAudioTriggers *at = qobject_cast<VCAudioTriggers *>(widget);
    if (!at) return false;
    at->enableCapture(enabled);
    return true;
}

bool VCBridgeV4::setAudioTriggerVolume(int widgetID, int volume)
{
    Q_UNUSED(widgetID);
    Q_UNUSED(volume);
    // v4 VCAudioTriggers doesn't have a setVolumeLevel() API
    // Volume is managed by the audio input plugin
    return false;
}

bool VCBridgeV4::setAudioTriggerBarsNumber(int widgetID, int count)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCAudioTriggers *at = qobject_cast<VCAudioTriggers *>(widget);
    if (!at) return false;
    if (count < 1) count = 1;
    at->setSpectrumBarsNumber(count);
    return true;
}

// ---------------------------------------------------------------------------
// Clock
// ---------------------------------------------------------------------------

int VCBridgeV4::addClock(int parentID, const QRect &geometry,
                          const QString &clockType)
{
    VCFrame *frame = resolveParentFrame(m_vc, parentID);
    if (!frame) return -1;

    VCClock *clock = new VCClock(frame, m_doc);
    registerWidget(m_vc, clock, frame);
    clock->setGeometry(snapRect(geometry));

    if (clockType == "stopwatch")
        clock->setClockType(VCClock::Stopwatch);
    else if (clockType == "countdown")
        clock->setClockType(VCClock::Countdown);
    else
        clock->setClockType(VCClock::Clock);

    return clock->id();
}

// ---------------------------------------------------------------------------
// Idempotency lookups
// ---------------------------------------------------------------------------

int VCBridgeV4::findPageByName(const QString &name) const
{
    VCFrame *root = m_vc->contents();
    if (!root) return -1;

    QList<VCFramePageShortcut *> shortcuts = root->shortcuts();
    for (int i = 0; i < shortcuts.size(); i++)
    {
        if (shortcuts.at(i)->name() == name)
            return i;
    }
    return -1;
}

int VCBridgeV4::findWidgetByCaption(int parentID, const QString &widgetType,
                                     const QString &caption) const
{
    VCFrame *frame = resolveParentFrame(m_vc, parentID);
    if (!frame) return -1;

    // Accept both MCP names ("button", "soloframe") and display names ("Button", "Solo frame")
    int vcType = VCType::fromString(widgetType.toStdString());
    if (vcType == VCType::Unknown)
        vcType = VCType::fromDisplayString(widgetType);

    // Map VCType enum → v4 widget type int
    static const QMap<int, int> typeMap = {
        {VCType::Button,        VCWidget::ButtonWidget},
        {VCType::Slider,        VCWidget::SliderWidget},
        {VCType::XYPad,         VCWidget::XYPadWidget},
        {VCType::Frame,         VCWidget::FrameWidget},
        {VCType::SoloFrame,     VCWidget::SoloFrameWidget},
        {VCType::SpeedDial,     VCWidget::SpeedDialWidget},
        {VCType::CueList,       VCWidget::CueListWidget},
        {VCType::Label,         VCWidget::LabelWidget},
        {VCType::AudioTriggers, VCWidget::AudioTriggersWidget},
        {VCType::Animation,     VCWidget::AnimationWidget},
        {VCType::Clock,         VCWidget::ClockWidget},
    };
    int targetType = typeMap.value(vcType, VCWidget::UnknownWidget);

    // v4: iterate QWidget children
    for (QObject *obj : frame->children())
    {
        VCWidget *w = qobject_cast<VCWidget *>(obj);
        if (w && w->type() == targetType && w->caption() == caption)
            return w->id();
    }
    return -1;
}

QRect VCBridgeV4::nextWidgetPosition(int parentID, int width, int height) const
{
    VCFrame *frame = resolveParentFrame(m_vc, parentID);
    if (!frame)
        return QRect(5, 5, width, height);

    const int pad = 5;
    int maxBottom = 40; // leave space for frame header
    for (QObject *obj : frame->children())
    {
        VCWidget *w = qobject_cast<VCWidget *>(obj);
        if (!w) continue;
        int bottom = w->geometry().y() + w->geometry().height() + pad;
        if (bottom > maxBottom)
            maxBottom = bottom;
    }
    return QRect(pad, maxBottom, width, height);
}

QRect VCBridgeV4::nextWidgetPositionFlow(int parentID, int widgetWidth, int widgetHeight,
                                          int columns) const
{
    VCFrame *frame = resolveParentFrame(m_vc, parentID);
    if (!frame)
        return QRect(5, 5, widgetWidth, widgetHeight);

    int parentWidth = frame->geometry().width();

    int childCount = 0;
    for (QObject *obj : frame->children())
    {
        if (qobject_cast<VCWidget *>(obj))
            childCount++;
    }

    return computeFlowPosition(parentWidth, 40, childCount, widgetWidth, widgetHeight, columns, 5);
}

void VCBridgeV4::setWidgetGeometry(int widgetID, const QRect &geo)
{
    VCWidget *widget = m_vc->widget(widgetID);
    if (widget)
        widget->setGeometry(snapRect(geo));
}

bool VCBridgeV4::removeWidget(int widgetID)
{
    VCWidget *widget = m_vc->widget(widgetID);
    if (!widget) return false;

    // Disconnect from parent and delete
    VCWidget *parent = qobject_cast<VCWidget *>(widget->parentWidget());
    if (parent)
    {
        VCFrame *parentFrame = qobject_cast<VCFrame *>(parent);
        if (parentFrame)
            parentFrame->removeWidgetFromPageMap(widget);
    }

    delete widget;
    return true;
}

// ---------------------------------------------------------------------------
// Widget details query
// ---------------------------------------------------------------------------

VCBridge::WidgetDetails VCBridgeV4::getWidgetDetails(int widgetID) const
{
    VCWidget *widget = m_vc->widget(widgetID);
    if (!widget) return WidgetDetails();

    WidgetDetails d;
    d.id = widgetID;
    d.type = VCType::widgetTypeToMcp(widget->type());
    d.caption = widget->caption();
    d.geometry = widget->geometry();
    d.bgColor = widget->backgroundColor();
    d.fgColor = widget->foregroundColor();

    // Parent ID
    VCWidget *parent = qobject_cast<VCWidget *>(widget->parentWidget());
    if (parent)
        d.parentID = (int)parent->id();

    // Input mappings — scan all source IDs
    for (quint8 id = 0; id < 32; id++)
    {
        QSharedPointer<QLCInputSource> src = widget->inputSource(id);
        if (!src.isNull() && src->isValid())
        {
            InputMapping m;
            m.universe = src->universe();
            m.channel = src->channel();
            m.sourceId = id;
            m.sourceName = resolveSourceName(widget, id);
            m.feedback.idleValue = src->feedbackValue(QLCInputFeedback::LowerValue);
            m.feedback.activeValue = src->feedbackValue(QLCInputFeedback::UpperValue);
            m.feedback.monitorValue = src->feedbackValue(QLCInputFeedback::MonitorValue);
            m.feedback.idleMidiCh = src->feedbackExtraParams(QLCInputFeedback::LowerValue).toInt();
            m.feedback.activeMidiCh = src->feedbackExtraParams(QLCInputFeedback::UpperValue).toInt();
            m.feedback.monitorMidiCh = src->feedbackExtraParams(QLCInputFeedback::MonitorValue).toInt();
            d.inputMappings.append(m);
        }
    }

    // Valid source definitions
    d.validSources = getWidgetSourceDefs(widgetID);

    // Button-specific
    VCButton *button = qobject_cast<VCButton *>(widget);
    if (button)
    {
        d.functionID = button->function();
        d.action = VCButton::actionToString(button->action()).toLower();
        d.buttonState = (int)button->state();
        d.startupIntensityEnabled = button->isStartupIntensityEnabled();
        d.startupIntensity = button->startupIntensity();
        d.flashOverride = button->flashOverrides();
        d.flashForceLTP = button->flashForceLTP();
        d.stopAllFadeTime = button->stopAllFadeTime();
        d.iconPath = button->iconPath();
    }

    // Slider-specific
    VCSlider *slider = qobject_cast<VCSlider *>(widget);
    if (slider)
    {
        switch (slider->sliderMode())
        {
            case VCSlider::Level: d.sliderMode = "level"; break;
            case VCSlider::Playback: d.sliderMode = "playback"; break;
            case VCSlider::Submaster: d.sliderMode = "submaster"; break;
        }
        d.functionID = slider->playbackFunction();
        for (auto &lc : slider->levelChannels())
            d.channels.append(qMakePair(lc.fixture, lc.channel));

        // Extended slider properties
        ClickAndGoWidget::ClickAndGo cng = slider->clickAndGoType();
        if (cng == ClickAndGoWidget::None) d.clickAndGoType = "none";
        else if (cng == ClickAndGoWidget::Preset) d.clickAndGoType = "preset";
        else d.clickAndGoType = "colors"; // All color types map to "colors"
        switch (slider->valueDisplayStyle())
        {
            case VCSlider::ExactValue: d.valueDisplayStyle = "dmx"; break;
            case VCSlider::PercentageValue: d.valueDisplayStyle = "percentage"; break;
        }
        d.sliderInvertedAppearance = slider->invertedAppearance();
        d.rangeLowLimit = slider->levelLowLimit();
        d.rangeHighLimit = slider->levelHighLimit();
        d.monitorEnabled = slider->channelsMonitorEnabled();

        d.widgetStyle = (slider->widgetStyle() == VCSlider::WKnob) ? "knob" : "slider";
        d.catchValues = slider->catchValues();
    }

    // CueList-specific
    VCCueList *cuelist = qobject_cast<VCCueList *>(widget);
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
    VCXYPad *xyPad = qobject_cast<VCXYPad *>(widget);
    if (xyPad)
    {
        d.invertedAppearance = xyPad->invertedAppearance();
        // v4 doesn't have displayMode

        for (const VCXYPadFixture &fxItem : xyPad->fixtures())
        {
            XYPadFixtureInfo info;
            info.fixtureID = fxItem.head().fxi;
            info.head = fxItem.head().head;
            info.xMin = fxItem.xMin();
            info.xMax = fxItem.xMax();
            info.xReverse = fxItem.xReverse();
            info.yMin = fxItem.yMin();
            info.yMax = fxItem.yMax();
            info.yReverse = fxItem.yReverse();

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
        for (VCXYPadPreset *preset : xyPad->presets())
        {
            XYPadPresetInfo pi;
            pi.name = preset->m_name;
            pi.functionID = preset->m_funcID;
            switch (preset->m_type)
            {
                case VCXYPadPreset::Position: pi.type = "position"; break;
                case VCXYPadPreset::EFX: pi.type = "efx"; break;
                case VCXYPadPreset::Scene: pi.type = "scene"; break;
                case VCXYPadPreset::FixtureGroup: pi.type = "fixtureGroup"; break;
            }
            d.xyPadPresets.append(pi);
        }
    }

    // AudioTriggers-specific
    VCAudioTriggers *audioTrig = qobject_cast<VCAudioTriggers *>(widget);
    if (audioTrig)
    {
        d.barsNumber = 0;
        QList<AudioBar *> bars = audioTrig->getAudioBars();
        d.barsNumber = bars.size();

        for (int i = 0; i < bars.size(); i++)
        {
            AudioBar *bar = bars.at(i);
            if (!bar) continue;

            WidgetDetails::AudioBarInfo barInfo;
            barInfo.barIndex = i;
            switch (bar->m_type)
            {
                case AudioBar::DMXBar: barInfo.type = "dmx"; break;
                case AudioBar::FunctionBar: barInfo.type = "function"; break;
                case AudioBar::VCWidgetBar: barInfo.type = "widget"; break;
                default: barInfo.type = "none"; break;
            }
            barInfo.minThreshold = bar->m_minThreshold;
            barInfo.maxThreshold = bar->m_maxThreshold;
            barInfo.divisor = bar->m_divisor;

            if (bar->m_type == AudioBar::FunctionBar)
            {
                barInfo.functionID = bar->m_function ? bar->m_function->id() : Function::invalidId();
                if (bar->m_function) barInfo.functionName = bar->m_function->name();
            }
            else if (bar->m_type == AudioBar::VCWidgetBar)
            {
                barInfo.widgetID = bar->m_widgetID;
            }

            d.audioBars.append(barInfo);
        }
    }

    // Frame-specific
    VCFrame *frame = qobject_cast<VCFrame *>(widget);
    if (frame && frame != m_vc->contents())
    {
        d.multipageMode = frame->multipageMode();
        d.totalPages = frame->totalPagesNumber();
        d.currentPage = frame->currentPage();
        d.pagesLoop = frame->pagesLoop();
        d.headerVisible = frame->isHeaderVisible();
        d.enableButtonVisible = frame->isEnableButtonVisible();
        d.collapsed = frame->isCollapsed();

        // Page labels from shortcuts
        QList<VCFramePageShortcut *> shortcuts = frame->shortcuts();
        for (VCFramePageShortcut *sc : shortcuts)
            d.pageLabels.append(sc->name());

        VCSoloFrame *soloFrame = qobject_cast<VCSoloFrame *>(widget);
        if (soloFrame)
        {
            d.soloframeMixing = soloFrame->soloframeMixing();
            d.excludeMonitoredFunctions = soloFrame->excludeMonitoredFunctions();
        }
    }

    // Matrix-specific (v4 uses VCMatrix, not VCAnimation)
    VCMatrix *matrix = qobject_cast<VCMatrix *>(widget);
    if (matrix)
    {
        d.functionID = matrix->function();
        d.matrixColor1 = matrix->mtxColor(1);
        d.matrixColor2 = matrix->mtxColor(2);
        d.matrixColor3 = matrix->mtxColor(3);
        d.matrixColor4 = matrix->mtxColor(4);
        d.matrixColor5 = matrix->mtxColor(5);
        d.matrixInstantApply = matrix->instantChanges();
        d.matrixVisibilityMask = matrix->visibilityMask();
        // TODO: animation name from VCMatrix (need animationValue() or similar)
    }

    // Clock-specific
    VCClock *clock = qobject_cast<VCClock *>(widget);
    if (clock)
    {
        switch (clock->clockType())
        {
            case VCClock::Clock: d.clockType = "clock"; break;
            case VCClock::Stopwatch: d.clockType = "stopwatch"; break;
            case VCClock::Countdown: d.clockType = "countdown"; break;
        }
        d.countdownH = clock->getHours();
        d.countdownM = clock->getMinutes();
        d.countdownS = clock->getSeconds();

        for (const VCClockSchedule &sched : clock->schedules())
        {
            ClockScheduleInfo csi;
            csi.functionID = sched.function();
            QDateTime dt = sched.time();
            csi.hour = dt.time().hour();
            csi.minute = dt.time().minute();
            csi.second = dt.time().second();
            d.clockSchedules.append(csi);
        }
    }

    // SpeedDial-specific
    VCSpeedDial *speedDial = qobject_cast<VCSpeedDial *>(widget);
    if (speedDial)
    {
        for (const VCSpeedDialFunction &func : speedDial->functions())
        {
            SpeedDialFunctionInfo fi;
            fi.functionID = func.functionId;
            fi.fadeInMultiplier = multiplierToString(func.fadeInMultiplier);
            fi.fadeOutMultiplier = multiplierToString(func.fadeOutMultiplier);
            fi.durationMultiplier = multiplierToString(func.durationMultiplier);
            d.speedDialFunctions.append(fi);
        }
        for (VCSpeedDialPreset *preset : speedDial->presets())
        {
            SpeedDialPresetInfo pi;
            pi.name = preset->m_name;
            pi.value = preset->m_value;
            d.speedDialPresets.append(pi);
        }
        d.absoluteValueMin = speedDial->absoluteValueMin();
        d.absoluteValueMax = speedDial->absoluteValueMax();
        d.speedDialVisibilityMask = speedDial->visibilityMask();
        d.resetFactorOnDialChange = speedDial->resetFactorOnDialChange();
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

// ---------------------------------------------------------------------------
// Widget property mutations
// ---------------------------------------------------------------------------

bool VCBridgeV4::setWidgetCaption(int widgetID, const QString &caption)
{
    VCWidget *widget = m_vc->widget(widgetID);
    if (!widget) return false;
    widget->setCaption(caption);
    return true;
}

bool VCBridgeV4::setButtonFunction(int widgetID, quint32 functionID)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCButton *button = qobject_cast<VCButton *>(widget);
    if (!button) return false;
    button->setFunction(functionID);
    return true;
}

bool VCBridgeV4::setButtonAction(int widgetID, const QString &action)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCButton *button = qobject_cast<VCButton *>(widget);
    if (!button) return false;

    if (action == "flash") button->setAction(VCButton::Flash);
    else if (action == "blackout") button->setAction(VCButton::Blackout);
    else if (action == "stopall") button->setAction(VCButton::StopAll);
    else button->setAction(VCButton::Toggle);
    return true;
}

bool VCBridgeV4::setSliderMode(int widgetID, const QString &mode)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCSlider *slider = qobject_cast<VCSlider *>(widget);
    if (!slider) return false;

    if (mode == "level") slider->setSliderMode(VCSlider::Level);
    else if (mode == "playback") slider->setSliderMode(VCSlider::Playback);
    else if (mode == "submaster") slider->setSliderMode(VCSlider::Submaster);
    else return false;
    return true;
}

bool VCBridgeV4::setSliderFunction(int widgetID, quint32 functionID)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCSlider *slider = qobject_cast<VCSlider *>(widget);
    if (!slider) return false;
    slider->setPlaybackFunction(functionID);
    return true;
}

bool VCBridgeV4::setSliderChannels(int widgetID, const QList<QPair<quint32, quint32>> &channels)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCSlider *slider = qobject_cast<VCSlider *>(widget);
    if (!slider) return false;

    slider->clearLevelChannels();
    for (const auto &ch : channels)
        slider->addLevelChannel(ch.first, ch.second);
    return true;
}

bool VCBridgeV4::configureSlider(int widgetID, const SliderConfig &config)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCSlider *slider = qobject_cast<VCSlider *>(widget);
    if (!slider) return false;

    if (config.clickAndGoType.has_value())
    {
        const QString &v = config.clickAndGoType.value();
        if (v == "colors" || v == "rgb") slider->setClickAndGoType(ClickAndGoWidget::RGB);
        else if (v == "preset") slider->setClickAndGoType(ClickAndGoWidget::Preset);
        else slider->setClickAndGoType(ClickAndGoWidget::None);
    }
    if (config.valueDisplayStyle.has_value())
    {
        const QString &v = config.valueDisplayStyle.value();
        if (v == "percentage") slider->setValueDisplayStyle(VCSlider::PercentageValue);
        else slider->setValueDisplayStyle(VCSlider::ExactValue);
    }
    if (config.invertedAppearance.has_value())
        slider->setInvertedAppearance(config.invertedAppearance.value());
    if (config.rangeLowLimit.has_value())
        slider->setLevelLowLimit((uchar)config.rangeLowLimit.value());
    if (config.rangeHighLimit.has_value())
        slider->setLevelHighLimit((uchar)config.rangeHighLimit.value());
    if (config.monitorEnabled.has_value())
        slider->setChannelsMonitorEnabled(config.monitorEnabled.value());
    // GrandMaster mode not available in v4 VCSlider
    return true;
}

// ---------------------------------------------------------------------------
// Input mapping (add/remove)
// ---------------------------------------------------------------------------

bool VCBridgeV4::addWidgetInput(int widgetID, quint32 universe, quint32 channel)
{
    VCWidget *widget = m_vc->widget(widgetID);
    if (!widget) return false;

    QSharedPointer<QLCInputSource> source(new QLCInputSource(universe, channel));
    widget->setInputSource(source);
    return true;
}

bool VCBridgeV4::removeWidgetInput(int widgetID, quint32 universe, quint32 channel)
{
    VCWidget *widget = m_vc->widget(widgetID);
    if (!widget) return false;

    // v4: find the source by universe/channel and remove
    for (quint8 id = 0; id < 32; id++)
    {
        QSharedPointer<QLCInputSource> src = widget->inputSource(id);
        if (!src.isNull() && src->isValid()
            && src->universe() == universe && src->channel() == channel)
        {
            widget->setInputSource(QSharedPointer<QLCInputSource>(), id);
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Widget reparenting
// ---------------------------------------------------------------------------

bool VCBridgeV4::reparentWidget(int widgetID, int newParentID, const QRect &geo)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCWidget *newParent = m_vc->widget(newParentID);
    if (!widget || !newParent) return false;

    VCFrame *targetFrame = qobject_cast<VCFrame *>(newParent);
    if (!targetFrame) return false;

    // Remove from old parent's page map
    VCWidget *oldParent = qobject_cast<VCWidget *>(widget->parentWidget());
    if (oldParent)
    {
        VCFrame *oldFrame = qobject_cast<VCFrame *>(oldParent);
        if (oldFrame)
            oldFrame->removeWidgetFromPageMap(widget);
    }

    // Reparent
    widget->setParent(targetFrame);
    widget->setGeometry(snapRect(geo));

    // Add to new parent's page map
    targetFrame->addWidgetToPageMap(widget);
    widget->show();

    return true;
}

// ---------------------------------------------------------------------------
// Matrix widget (v4 uses VCMatrix)
// ---------------------------------------------------------------------------

int VCBridgeV4::addMatrix(int parentID, const QRect &geometry,
                          quint32 functionID, const QString &caption)
{
    VCFrame *frame = resolveParentFrame(m_vc, parentID);
    if (!frame) return -1;

    VCMatrix *matrix = new VCMatrix(frame, m_doc);
    registerWidget(m_vc, matrix, frame);

    matrix->setGeometry(snapRect(geometry));
    if (!caption.isEmpty())
        matrix->setCaption(caption);
    if (functionID != Function::invalidId())
        matrix->setFunction(functionID);

    return matrix->id();
}

bool VCBridgeV4::configureMatrix(int widgetID, const MatrixConfig &config)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCMatrix *matrix = qobject_cast<VCMatrix *>(widget);
    if (!matrix) return false;

    if (config.functionID.has_value())
        matrix->setFunction(config.functionID.value());
    if (config.color1.has_value())
        matrix->slotSetColor1(config.color1.value());
    if (config.color2.has_value())
        matrix->slotSetColor2(config.color2.value());
    if (config.color3.has_value())
        matrix->slotSetColor3(config.color3.value());
    if (config.color4.has_value())
        matrix->slotSetColor4(config.color4.value());
    if (config.color5.has_value())
        matrix->slotSetColor5(config.color5.value());
    if (config.animation.has_value())
        matrix->slotSetAnimationValue(config.animation.value());
    if (config.instantApply.has_value())
        matrix->setInstantChanges(config.instantApply.value());
    if (config.visibilityMask.has_value())
        matrix->setVisibilityMask(config.visibilityMask.value());
    return true;
}

// ---------------------------------------------------------------------------
// Button extended config
// ---------------------------------------------------------------------------

bool VCBridgeV4::configureButton(int widgetID, const ButtonConfig &config)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCButton *button = qobject_cast<VCButton *>(widget);
    if (!button) return false;

    if (config.functionID.has_value())
        button->setFunction(config.functionID.value());
    if (config.action.has_value())
    {
        const QString &a = config.action.value();
        if (a == "flash") button->setAction(VCButton::Flash);
        else if (a == "blackout") button->setAction(VCButton::Blackout);
        else if (a == "stopall") button->setAction(VCButton::StopAll);
        else button->setAction(VCButton::Toggle);
    }
    if (config.iconPath.has_value())
        button->setIconPath(config.iconPath.value());
    if (config.startupIntensityEnabled.has_value())
        button->enableStartupIntensity(config.startupIntensityEnabled.value());
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

// ---------------------------------------------------------------------------
// Frame extended config
// ---------------------------------------------------------------------------

bool VCBridgeV4::configureFrame(int widgetID, const FrameConfig &config)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCFrame *frame = qobject_cast<VCFrame *>(widget);
    if (!frame) return false;

    if (config.multipageMode.has_value())
        frame->setMultipageMode(config.multipageMode.value());
    if (config.totalPages.has_value())
        frame->setTotalPagesNumber(config.totalPages.value());
    if (config.currentPage.has_value())
        frame->slotSetPage(config.currentPage.value());
    if (config.pagesLoop.has_value())
        frame->setPagesLoop(config.pagesLoop.value());
    if (config.pageLabels.has_value())
    {
        const QStringList &labels = config.pageLabels.value();
        QList<VCFramePageShortcut *> shortcuts = frame->shortcuts();
        for (int i = 0; i < labels.size() && i < shortcuts.size(); i++)
            shortcuts.at(i)->setName(labels.at(i));
    }
    if (config.headerVisible.has_value())
        frame->setHeaderVisible(config.headerVisible.value());
    if (config.enableButtonVisible.has_value())
        frame->setEnableButtonVisible(config.enableButtonVisible.value());
    if (config.collapsed.has_value())
    {
        // slotCollapseButtonToggled is a protected slot; invoke via meta-object
        QMetaObject::invokeMethod(frame, "slotCollapseButtonToggled",
                                  Q_ARG(bool, config.collapsed.value()));
    }

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

// ---------------------------------------------------------------------------
// CueList extended config
// ---------------------------------------------------------------------------

bool VCBridgeV4::configureCueList(int widgetID, const CueListConfig &config)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCCueList *cuelist = qobject_cast<VCCueList *>(widget);
    if (!cuelist) return false;

    if (config.chaserID.has_value())
        cuelist->setChaser(config.chaserID.value());
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

// ---------------------------------------------------------------------------
// Clock extended config
// ---------------------------------------------------------------------------

bool VCBridgeV4::configureClock(int widgetID, const ClockConfig &config)
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
        clock->setCountdown(h, m, s);
    }
    if (config.schedules.has_value())
    {
        clock->removeAllSchedule();

        for (const ClockScheduleInfo &info : config.schedules.value())
        {
            VCClockSchedule sched;
            sched.setFunction(info.functionID);
            QDateTime dt;
            dt.setTime(QTime(info.hour, info.minute, info.second));
            sched.setTime(dt);
            clock->addSchedule(sched);
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// SpeedDial extended config
// ---------------------------------------------------------------------------

bool VCBridgeV4::configureSpeedDial(int widgetID, const SpeedDialConfig &config)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCSpeedDial *speedDial = qobject_cast<VCSpeedDial *>(widget);
    if (!speedDial) return false;

    if (config.functions.has_value())
    {
        QList<VCSpeedDialFunction> funcList;
        for (const SpeedDialFunctionInfo &fi : config.functions.value())
        {
            VCSpeedDialFunction f(fi.functionID,
                                  stringToMultiplier(fi.fadeInMultiplier),
                                  stringToMultiplier(fi.fadeOutMultiplier),
                                  stringToMultiplier(fi.durationMultiplier));
            funcList.append(f);
        }
        speedDial->setFunctions(funcList);
    }
    if (config.presets.has_value())
    {
        speedDial->resetPresets();
        quint8 presetId = 0;
        for (const SpeedDialPresetInfo &pi : config.presets.value())
        {
            VCSpeedDialPreset preset(presetId++);
            preset.m_name = pi.name;
            preset.m_value = pi.value;
            speedDial->addPreset(preset);
        }
    }
    if (config.absoluteValueMin.has_value() || config.absoluteValueMax.has_value())
    {
        uint min = config.absoluteValueMin.value_or(speedDial->absoluteValueMin());
        uint max = config.absoluteValueMax.value_or(speedDial->absoluteValueMax());
        speedDial->setAbsoluteValueRange(min, max);
    }
    if (config.visibilityMask.has_value())
        speedDial->setVisibilityMask(config.visibilityMask.value());
    if (config.resetFactorOnDialChange.has_value())
        speedDial->setResetFactorOnDialChange(config.resetFactorOnDialChange.value());
    return true;
}

// ---------------------------------------------------------------------------
// XY Pad presets
// ---------------------------------------------------------------------------

bool VCBridgeV4::setXYPadPresets(int widgetID, const QList<XYPadPresetInfo> &presets)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCXYPad *xyPad = qobject_cast<VCXYPad *>(widget);
    if (!xyPad) return false;

    xyPad->resetPresets();

    quint8 presetId = 0;
    for (const XYPadPresetInfo &info : presets)
    {
        VCXYPadPreset preset(presetId++);
        preset.m_name = info.name;

        if (info.type == "position")
        {
            preset.m_type = VCXYPadPreset::Position;
            preset.setPosition(info.position);
        }
        else if (info.type == "efx")
        {
            preset.m_type = VCXYPadPreset::EFX;
            preset.m_funcID = info.functionID;
        }
        else if (info.type == "scene")
        {
            preset.m_type = VCXYPadPreset::Scene;
            preset.m_funcID = info.functionID;
        }
        else if (info.type == "fixtureGroup")
        {
            preset.m_type = VCXYPadPreset::FixtureGroup;
        }

        xyPad->addPreset(preset);
    }
    return true;
}

// ---------------------------------------------------------------------------
// Key sequences
// ---------------------------------------------------------------------------

bool VCBridgeV4::setWidgetKeySequence(int widgetID, const QString &sourceName,
                                      const QKeySequence &keySequence)
{
    VCWidget *widget = m_vc->widget(widgetID);
    if (!widget) return false;

    // Button: single key sequence
    VCButton *button = qobject_cast<VCButton *>(widget);
    if (button && sourceName == "default")
    {
        button->setKeySequence(keySequence);
        return true;
    }

    // Slider: override reset or flash button key sequences
    VCSlider *slider = qobject_cast<VCSlider *>(widget);
    if (slider)
    {
        if (sourceName == "overrideReset")
        {
            slider->setOverrideResetKeySequence(keySequence);
            return true;
        }
        if (sourceName == "flashButton")
        {
            slider->setPlaybackFlashKeySequence(keySequence);
            return true;
        }
    }

    // Frame: next/previous page, enable
    VCFrame *frame = qobject_cast<VCFrame *>(widget);
    if (frame)
    {
        if (sourceName == "nextPage")
        {
            frame->setNextPageKeySequence(keySequence);
            return true;
        }
        if (sourceName == "previousPage")
        {
            frame->setPreviousPageKeySequence(keySequence);
            return true;
        }
        if (sourceName == "enable")
        {
            frame->setEnableKeySequence(keySequence);
            return true;
        }
    }

    // Audio Triggers
    VCAudioTriggers *at = qobject_cast<VCAudioTriggers *>(widget);
    if (at && sourceName == "default")
    {
        at->setKeySequence(keySequence);
        return true;
    }

    // Clock: play/reset
    VCClock *clock = qobject_cast<VCClock *>(widget);
    if (clock)
    {
        if (sourceName == "play")
        {
            clock->setPlayKeySequence(keySequence);
            return true;
        }
        if (sourceName == "reset")
        {
            clock->setResetKeySequence(keySequence);
            return true;
        }
    }

    // SpeedDial: tap, mult, div, multDivReset, apply
    VCSpeedDial *speedDial = qobject_cast<VCSpeedDial *>(widget);
    if (speedDial)
    {
        if (sourceName == "tap")
        {
            speedDial->setTapKeySequence(keySequence);
            return true;
        }
        if (sourceName == "mult")
        {
            speedDial->setMultKeySequence(keySequence);
            return true;
        }
        if (sourceName == "div")
        {
            speedDial->setDivKeySequence(keySequence);
            return true;
        }
        if (sourceName == "multDivReset")
        {
            speedDial->setMultDivResetKeySequence(keySequence);
            return true;
        }
        if (sourceName == "apply")
        {
            speedDial->setApplyKeySequence(keySequence);
            return true;
        }
    }

    return false;
}

// ---------------------------------------------------------------------------
// Named input mapping
// ---------------------------------------------------------------------------

bool VCBridgeV4::mapWidgetInputByName(int widgetID, const QString &sourceName,
                                      quint32 universe, quint32 channel)
{
    VCWidget *widget = m_vc->widget(widgetID);
    if (!widget) return false;

    int controlId = resolveControlId(widget, sourceName);
    if (controlId < 0) return false;

    QSharedPointer<QLCInputSource> source(new QLCInputSource(universe, channel));
    widget->setInputSource(source, (quint8)controlId);
    return true;
}

// ---------------------------------------------------------------------------
// Base widget properties
// ---------------------------------------------------------------------------

bool VCBridgeV4::setWidgetFont(int widgetID, const FontConfig &font)
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

bool VCBridgeV4::setWidgetBackgroundImage(int widgetID, const QString &path)
{
    VCWidget *widget = m_vc->widget(widgetID);
    if (!widget) return false;
    widget->setBackgroundImage(path);
    return true;
}

bool VCBridgeV4::setWidgetDisableState(int widgetID, bool disabled)
{
    VCWidget *widget = m_vc->widget(widgetID);
    if (!widget) return false;
    widget->setDisableState(disabled);
    return true;
}

// ---------------------------------------------------------------------------
// Page rename
// ---------------------------------------------------------------------------

bool VCBridgeV4::renamePage(int pageIndex, const QString &name)
{
    VCFrame *root = m_vc->contents();
    if (!root) return false;

    QList<VCFramePageShortcut *> shortcuts = root->shortcuts();
    if (pageIndex < 0 || pageIndex >= shortcuts.size())
        return false;

    shortcuts.at(pageIndex)->setName(name);
    return true;
}

// ---------------------------------------------------------------------------
// Slider extended
// ---------------------------------------------------------------------------

bool VCBridgeV4::setSliderWidgetStyle(int widgetID, const QString &style)
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

bool VCBridgeV4::setSliderCatchValues(int widgetID, bool enable)
{
    VCWidget *widget = m_vc->widget(widgetID);
    VCSlider *slider = qobject_cast<VCSlider *>(widget);
    if (!slider) return false;
    slider->setCatchValues(enable);
    return true;
}

// ---------------------------------------------------------------------------
// Layout analysis: snapshot / apply
// ---------------------------------------------------------------------------

static VCBridge::WidgetSnapshot snapshotWidget(VCWidget *widget)
{
    VCBridge::WidgetSnapshot snap;
    snap.id = widget->id();
    snap.type = widget->type();
    snap.geometry = widget->geometry();

    VCFrame *frame = qobject_cast<VCFrame *>(widget);
    if (frame)
    {
        snap.showHeader = frame->isHeaderVisible();
        snap.parentID = -1; // filled by caller

        for (QObject *obj : frame->children())
        {
            VCWidget *child = qobject_cast<VCWidget *>(obj);
            if (!child) continue;

            VCBridge::WidgetSnapshot childSnap = snapshotWidget(child);
            childSnap.parentID = snap.id;
            snap.children.append(childSnap);
        }
    }
    return snap;
}

VCBridge::WidgetSnapshot VCBridgeV4::snapshotFrame(int frameID) const
{
    VCWidget *widget = m_vc->widget(frameID);
    if (!widget) return WidgetSnapshot();
    return snapshotWidget(widget);
}

VCBridge::WidgetSnapshot VCBridgeV4::snapshotPage(int pageIndex) const
{
    VCFrame *root = m_vc->contents();
    if (!root) return WidgetSnapshot();

    // In v4, all pages share the root frame. Snapshot the root frame
    // but filter children to the requested page.
    WidgetSnapshot snap;
    snap.id = root->id();
    snap.type = root->type();
    snap.geometry = root->geometry();
    snap.showHeader = root->isHeaderVisible();

    for (QObject *obj : root->children())
    {
        VCWidget *child = qobject_cast<VCWidget *>(obj);
        if (!child) continue;

        if (root->multipageMode() && child->page() != pageIndex)
            continue;

        WidgetSnapshot childSnap = snapshotWidget(child);
        childSnap.parentID = snap.id;
        snap.children.append(childSnap);
    }

    return snap;
}

void VCBridgeV4::applyLayoutPlan(const LayoutPlan &plan)
{
    for (auto it = plan.geometries.constBegin(); it != plan.geometries.constEnd(); ++it)
    {
        VCWidget *widget = m_vc->widget(it.key());
        if (widget)
            widget->setGeometry(snapRect(it.value()));
    }
}
