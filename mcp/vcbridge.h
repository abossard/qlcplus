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
#include <QKeySequence>
#include <QRect>
#include <QString>
#include <QVariant>
#include <QList>
#include <QMap>
#include <algorithm>
#include <optional>

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

    struct FeedbackInfo
    {
        int idleValue = 0;
        int activeValue = 0;
        int monitorValue = 0;
        int idleMidiCh = 0;
        int activeMidiCh = 0;
        int monitorMidiCh = 0;
    };

    struct InputMapping
    {
        quint32 universe;
        quint32 channel;
        quint32 sourceId = 0;
        QString sourceName;
        FeedbackInfo feedback;
    };

    /** Describes a valid input source on a widget type */
    struct SourceDef
    {
        QString name;        // e.g., "pan", "tilt", "next"
        quint32 id;          // QLC+ input source ID
        QString description; // e.g., "Pan (X axis)"
    };

    /** Per-fixture configuration for XY Pad creation */
    struct XYPadFixtureConfig
    {
        quint32 fixtureID = 0;
        int head = 0;           // head index for multi-head fixtures
        qreal xMin = 0.0;      // pan range minimum (0.0–1.0 proportion)
        qreal xMax = 1.0;      // pan range maximum
        bool xReverse = false;
        qreal yMin = 0.0;      // tilt range minimum (0.0–1.0 proportion)
        qreal yMax = 1.0;      // tilt range maximum
        bool yReverse = false;
    };

    /** Fixture info returned from XY Pad widget details */
    struct XYPadFixtureInfo
    {
        quint32 fixtureID = 0;
        int head = 0;
        QString name;
        qreal xMin = 0.0;
        qreal xMax = 1.0;
        bool xReverse = false;
        qreal yMin = 0.0;
        qreal yMax = 1.0;
        bool yReverse = false;
        qreal panDegreesMax = 0;   // from fixture physical spec
        qreal tiltDegreesMax = 0;
    };

    /** Optional slider configuration for create/update operations. */
    struct SliderConfig
    {
        std::optional<QString> clickAndGoType;    // "none"/"colors"/"preset"
        std::optional<QString> valueDisplayStyle; // "dmx"/"percentage"
        std::optional<bool>    invertedAppearance;
        std::optional<qreal>   rangeLowLimit;     // 0–255
        std::optional<qreal>   rangeHighLimit;    // 0–255
        std::optional<bool>    monitorEnabled;
        std::optional<QString> gmValueMode;       // "limit"/"reduce"  (grandmaster only)
        std::optional<QString> gmChannelMode;     // "intensity"/"allchannels" (grandmaster only)
    };

    /** Button extended configuration */
    struct ButtonConfig
    {
        std::optional<quint32> functionID;
        std::optional<QString> action;         // "toggle"/"flash"/"blackout"/"stopall"
        std::optional<QString> iconPath;
        std::optional<bool> startupIntensityEnabled;
        std::optional<qreal> startupIntensity; // 0.0-1.0
        std::optional<bool> flashOverride;
        std::optional<bool> flashForceLTP;
        std::optional<int> stopAllFadeTime;    // ms
    };

    /** Frame/SoloFrame configuration */
    struct FrameConfig
    {
        std::optional<bool> multipageMode;
        std::optional<int> totalPages;
        std::optional<int> currentPage;
        std::optional<bool> pagesLoop;
        std::optional<QStringList> pageLabels;
        std::optional<bool> headerVisible;
        std::optional<bool> enableButtonVisible;
        std::optional<bool> collapsed;
        // SoloFrame only:
        std::optional<bool> soloframeMixing;
        std::optional<bool> excludeMonitoredFunctions;
    };

    /** CueList configuration */
    struct CueListConfig
    {
        std::optional<quint32> chaserID;
        std::optional<QString> nextPrevBehavior;  // "defaultRunFirst"/"runNext"/"select"/"nothing"
        std::optional<QString> playbackLayout;    // "playPauseStop"/"playStopPause"
        std::optional<QString> sideFaderMode;     // "none"/"crossfade"/"steps"
    };

    /** Matrix (Animation) configuration */
    struct MatrixConfig
    {
        std::optional<quint32> functionID;
        std::optional<QColor> color1, color2, color3, color4, color5;
        std::optional<QString> animation;        // algorithm name
        std::optional<bool> instantApply;
        std::optional<quint32> visibilityMask;
    };

    /** Clock schedule entry */
    struct ClockScheduleInfo
    {
        quint32 functionID = (quint32)-1;
        int hour = 0, minute = 0, second = 0;
    };

    /** Clock configuration */
    struct ClockConfig
    {
        std::optional<QString> clockType;        // "clock"/"stopwatch"/"countdown"
        std::optional<int> countdownH, countdownM, countdownS;
        std::optional<QList<ClockScheduleInfo>> schedules;
    };

    /** SpeedDial function with per-function multipliers */
    struct SpeedDialFunctionInfo
    {
        quint32 functionID = (quint32)-1;
        QString fadeInMultiplier = "none";    // "none","0","1/16","1/8","1/4","1/2","1","2","4","8","16"
        QString fadeOutMultiplier = "none";
        QString durationMultiplier = "1";
    };

    /** SpeedDial preset (named speed value) */
    struct SpeedDialPresetInfo
    {
        QString name;
        int value = 0;  // milliseconds
    };

    /** SpeedDial configuration */
    struct SpeedDialConfig
    {
        std::optional<QList<SpeedDialFunctionInfo>> functions;
        std::optional<QList<SpeedDialPresetInfo>> presets;
        std::optional<quint32> absoluteValueMin;
        std::optional<quint32> absoluteValueMax;
        std::optional<quint32> visibilityMask;
        std::optional<bool> resetFactorOnDialChange;
    };

    /** XY Pad preset */
    struct XYPadPresetInfo
    {
        QString name;
        QString type;  // "position"/"efx"/"scene"/"fixtureGroup"
        QPointF position;           // for position type
        quint32 functionID = (quint32)-1;  // for efx/scene type
    };

    /** Font configuration */
    struct FontConfig
    {
        std::optional<QString> family;
        std::optional<int> pointSize;
        std::optional<bool> bold;
        std::optional<bool> italic;
    };

    struct WidgetDetails
    {
        int id = -1;
        QString type;
        QString caption;
        QRect geometry;
        quint32 functionID = 0;
        QString action;          // Button only: toggle/flash/blackout/stopall
        QString sliderMode;      // Slider only: level/playback/submaster/grandmaster
        QList<QPair<quint32, quint32>> channels;  // Slider level-mode channels
        QList<InputMapping> inputMappings;
        QColor bgColor;
        QColor fgColor;
        QList<SourceDef> validSources;
        int parentID = -1;

        // Slider extended properties
        QString clickAndGoType;               // "none"/"colors"/"preset"
        QString valueDisplayStyle;            // "dmx"/"percentage"
        bool sliderInvertedAppearance = false;
        qreal rangeLowLimit = 0;
        qreal rangeHighLimit = 255;
        bool monitorEnabled = false;
        QString gmValueMode;                  // "limit"/"reduce"
        QString gmChannelMode;                // "intensity"/"allchannels"

        // XY Pad specific
        QString displayMode;                  // "degrees", "percentage", "dmx"
        bool invertedAppearance = false;
        QList<XYPadFixtureInfo> xyPadFixtures;
        QPointF xyPadPosition;                // current position (0.0–1.0)

        // Audio Triggers specific
        bool captureEnabled = false;
        int volumeLevel = 100;                // 0–255
        int barsNumber = 0;
        struct AudioBarInfo
        {
            int barIndex = 0;
            QString type;                     // "none", "dmx", "function", "widget"
            int minThreshold = 51;            // 0–255 (default 20%)
            int maxThreshold = 204;           // 0–255 (default 80%)
            int divisor = 1;
            quint32 functionID = (quint32)-1;
            QString functionName;
            quint32 widgetID = (quint32)-1;
            QString widgetName;
            QList<QPair<quint32, quint32>> dmxChannels; // {fixtureID, channel}
        };
        QList<AudioBarInfo> audioBars;

        // Button extended
        QString iconPath;
        bool startupIntensityEnabled = false;
        qreal startupIntensity = 1.0;
        bool flashOverride = false;
        bool flashForceLTP = false;
        int stopAllFadeTime = 0;

        // Slider extended
        QString widgetStyle;              // "slider"/"knob"
        bool catchValues = false;

        // Frame extended
        bool multipageMode = false;
        int totalPages = 1;
        int currentPage = 0;
        bool pagesLoop = false;
        QStringList pageLabels;
        bool headerVisible = true;
        bool enableButtonVisible = false;
        bool collapsed = false;
        bool soloframeMixing = false;
        bool excludeMonitoredFunctions = false;

        // CueList extended
        QString nextPrevBehavior;
        QString playbackLayout;
        QString sideFaderMode;

        // Clock extended
        QString clockType;
        int countdownH = 0, countdownM = 0, countdownS = 0;
        QList<ClockScheduleInfo> clockSchedules;

        // SpeedDial extended
        QList<SpeedDialFunctionInfo> speedDialFunctions;
        QList<SpeedDialPresetInfo> speedDialPresets;
        quint32 absoluteValueMin = 0;
        quint32 absoluteValueMax = 0;
        quint32 speedDialVisibilityMask = 0;
        bool resetFactorOnDialChange = false;

        // Matrix extended
        quint32 matrixVisibilityMask = 0;
        bool matrixInstantApply = false;
        QColor matrixColor1, matrixColor2, matrixColor3, matrixColor4, matrixColor5;
        QString matrixAnimation;

        // XY Pad presets
        QList<XYPadPresetInfo> xyPadPresets;

        // Base widget extended
        FontConfig fontConfig;
        QString backgroundImage;
        bool disabled = false;
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

    /** Extended XY Pad creation with per-fixture config, display mode, and inverted Y */
    virtual int addXYPadEx(int parentID, const QRect &geometry,
                           const QList<XYPadFixtureConfig> &fixtures,
                           const QString &displayMode = "degrees",
                           bool invertedAppearance = false)
    {
        // Default: delegate to simple version (backwards compat)
        QList<quint32> ids;
        for (const auto &f : fixtures)
            ids.append(f.fixtureID);
        Q_UNUSED(displayMode); Q_UNUSED(invertedAppearance);
        return addXYPad(parentID, geometry, ids);
    }

    // XY Pad runtime position control
    virtual bool setXYPadPosition(int widgetID, qreal x, qreal y)
        { Q_UNUSED(widgetID); Q_UNUSED(x); Q_UNUSED(y); return false; }

    // XY Pad property mutations
    virtual bool setXYPadDisplayMode(int widgetID, const QString &mode)
        { Q_UNUSED(widgetID); Q_UNUSED(mode); return false; }
    virtual bool setXYPadInvertedAppearance(int widgetID, bool inverted)
        { Q_UNUSED(widgetID); Q_UNUSED(inverted); return false; }
    virtual bool addXYPadFixture(int widgetID, const XYPadFixtureConfig &config)
        { Q_UNUSED(widgetID); Q_UNUSED(config); return false; }
    virtual bool removeXYPadFixture(int widgetID, quint32 fixtureID, int head = 0)
        { Q_UNUSED(widgetID); Q_UNUSED(fixtureID); Q_UNUSED(head); return false; }

    virtual int addCueList(int parentID, const QRect &geometry,
                           quint32 chaserID, const QString &caption) = 0;

    virtual int addLabel(int parentID, const QRect &geometry,
                         const QString &text) = 0;

    // Input mapping
    virtual bool mapWidgetInput(int widgetID, quint32 universe,
                                quint32 channel) = 0;

    // Feedback (legacy — operates on first input source)
    virtual bool setWidgetFeedback(int widgetID,
                                   int idleValue, int activeValue, int monitorValue,
                                   int idleMidiCh, int activeMidiCh, int monitorMidiCh) = 0;

    // Read feedback from the widget's first input source
    virtual FeedbackInfo getWidgetFeedback(int widgetID) const
        { Q_UNUSED(widgetID); return FeedbackInfo(); }

    // Number of input sources on a widget
    virtual int widgetInputSourceCount(int widgetID) const
        { Q_UNUSED(widgetID); return 0; }

    // Get valid source names/IDs for a widget type
    virtual QList<SourceDef> getWidgetSourceDefs(int widgetID) const
        { Q_UNUSED(widgetID); return {}; }

    // Set feedback on a specific named source
    virtual bool setWidgetFeedbackByName(int widgetID, const QString &sourceName,
                                         int idleVal, int activeVal, int monitorVal,
                                         int idleCh, int activeCh, int monitorCh)
        { Q_UNUSED(widgetID); Q_UNUSED(sourceName);
          Q_UNUSED(idleVal); Q_UNUSED(activeVal); Q_UNUSED(monitorVal);
          Q_UNUSED(idleCh); Q_UNUSED(activeCh); Q_UNUSED(monitorCh); return false; }

    // Get feedback for a specific named source
    virtual FeedbackInfo getWidgetFeedbackByName(int widgetID, const QString &sourceName) const
        { Q_UNUSED(widgetID); Q_UNUSED(sourceName); return FeedbackInfo(); }

    // Widget colors
    virtual bool setWidgetColors(int widgetID,
                                 const QColor &bgColor = QColor(),
                                 const QColor &fgColor = QColor()) = 0;

    // Speed Dial widget
    virtual int addSpeedDial(int parentID, const QRect &geometry,
                             const QList<quint32> &functionIDs) = 0;

    // Audio Triggers widget
    virtual int addAudioTriggers(int parentID, const QRect &geometry) = 0;

    /** Per-bar configuration for audio triggers */
    struct AudioBarConfig
    {
        int barIndex = 0;
        QString type = "none";               // "none", "dmx", "function", "widget"
        int minThreshold = 20;               // 0–100 scale (default 20%)
        int maxThreshold = 80;               // 0–100 scale (default 80%)
        int divisor = 1;
        quint32 functionID = (quint32)-1;
        quint32 widgetID = (quint32)-1;
        QList<QPair<quint32, quint32>> dmxChannels; // {fixtureID, channel}
    };

    // Audio Triggers property mutations
    virtual bool configureAudioTriggerBar(int widgetID, const AudioBarConfig &config)
        { Q_UNUSED(widgetID); Q_UNUSED(config); return false; }
    virtual bool setAudioTriggerCapture(int widgetID, bool enabled)
        { Q_UNUSED(widgetID); Q_UNUSED(enabled); return false; }
    virtual bool setAudioTriggerVolume(int widgetID, int volume)
        { Q_UNUSED(widgetID); Q_UNUSED(volume); return false; }
    virtual bool setAudioTriggerBarsNumber(int widgetID, int count)
        { Q_UNUSED(widgetID); Q_UNUSED(count); return false; }

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
    virtual bool configureSlider(int widgetID, const SliderConfig &config)
        { Q_UNUSED(widgetID); Q_UNUSED(config); return false; }

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

    // Matrix widget
    virtual int addMatrix(int parentID, const QRect &geometry,
                          quint32 functionID, const QString &caption)
        { Q_UNUSED(parentID); Q_UNUSED(geometry); Q_UNUSED(functionID); Q_UNUSED(caption); return -1; }
    virtual bool configureMatrix(int widgetID, const MatrixConfig &config)
        { Q_UNUSED(widgetID); Q_UNUSED(config); return false; }

    // Button extended config
    virtual bool configureButton(int widgetID, const ButtonConfig &config)
        { Q_UNUSED(widgetID); Q_UNUSED(config); return false; }

    // Frame extended config
    virtual bool configureFrame(int widgetID, const FrameConfig &config)
        { Q_UNUSED(widgetID); Q_UNUSED(config); return false; }

    // CueList extended config
    virtual bool configureCueList(int widgetID, const CueListConfig &config)
        { Q_UNUSED(widgetID); Q_UNUSED(config); return false; }

    // Clock extended config
    virtual bool configureClock(int widgetID, const ClockConfig &config)
        { Q_UNUSED(widgetID); Q_UNUSED(config); return false; }

    // SpeedDial extended config
    virtual bool configureSpeedDial(int widgetID, const SpeedDialConfig &config)
        { Q_UNUSED(widgetID); Q_UNUSED(config); return false; }

    // XY Pad presets
    virtual bool setXYPadPresets(int widgetID, const QList<XYPadPresetInfo> &presets)
        { Q_UNUSED(widgetID); Q_UNUSED(presets); return false; }

    // Key sequences (generic — sourceName determines which input source)
    virtual bool setWidgetKeySequence(int widgetID, const QString &sourceName,
                                       const QKeySequence &keySequence)
        { Q_UNUSED(widgetID); Q_UNUSED(sourceName); Q_UNUSED(keySequence); return false; }

    // Multi-input mapping with named source
    virtual bool mapWidgetInputByName(int widgetID, const QString &sourceName,
                                       quint32 universe, quint32 channel)
        { Q_UNUSED(widgetID); Q_UNUSED(sourceName); Q_UNUSED(universe); Q_UNUSED(channel); return false; }

    // Base widget properties
    virtual bool setWidgetFont(int widgetID, const FontConfig &font)
        { Q_UNUSED(widgetID); Q_UNUSED(font); return false; }
    virtual bool setWidgetBackgroundImage(int widgetID, const QString &path)
        { Q_UNUSED(widgetID); Q_UNUSED(path); return false; }
    virtual bool setWidgetDisableState(int widgetID, bool disabled)
        { Q_UNUSED(widgetID); Q_UNUSED(disabled); return false; }

    // Page rename
    virtual bool renamePage(int pageIndex, const QString &name)
        { Q_UNUSED(pageIndex); Q_UNUSED(name); return false; }

    // Slider widget style (slider/knob)
    virtual bool setSliderWidgetStyle(int widgetID, const QString &style)
        { Q_UNUSED(widgetID); Q_UNUSED(style); return false; }
    virtual bool setSliderCatchValues(int widgetID, bool enable)
        { Q_UNUSED(widgetID); Q_UNUSED(enable); return false; }

    // ─── Layout analysis types (pure value types) ───────────────────

    /** Lightweight snapshot of a widget tree — no Qt object pointers. */
    struct WidgetSnapshot
    {
        int id = -1;
        int parentID = -1;
        int type = 0;               // VCWidget::WidgetType (or 0 for unknown)
        QRect geometry;
        QList<WidgetSnapshot> children;
    };

    /** Describes one pair of overlapping sibling widgets. */
    struct OverlapInfo
    {
        int widgetA;
        int widgetB;
        QRect intersection;
    };

    /** Options controlling reflow behaviour. */
    struct ReflowOptions
    {
        int columns = 0;            // 0 = auto-compute from parent width
        int pad = 5;
        int headerHeight = 40;
        int framePad = 10;          // vertical gap between top-level frames
        int defaultButtonWidth = 100;
        int defaultButtonHeight = 60;
        int defaultSliderWidth = 60;
        int defaultSliderHeight = 200;
    };

    /** The result of a layout computation: proposed geometry changes + detected overlaps. */
    struct LayoutPlan
    {
        QMap<int, QRect> geometries;     // widgetID → new geometry
        QList<OverlapInfo> overlaps;     // overlaps detected after reflow
    };

    // ─── Pure layout functions (static, no side effects) ────────────

    /** Detect overlapping widgets among a list of siblings. O(n²). */
    static QList<OverlapInfo> detectOverlaps(const QList<WidgetSnapshot> &siblings)
    {
        QList<OverlapInfo> result;
        for (int i = 0; i < siblings.size(); ++i)
        {
            for (int j = i + 1; j < siblings.size(); ++j)
            {
                QRect inter = siblings[i].geometry.intersected(siblings[j].geometry);
                if (!inter.isEmpty())
                    result.append({siblings[i].id, siblings[j].id, inter});
            }
        }
        return result;
    }

    /**
     * Reflow children within a container snapshot (mutates snapshot in-place).
     * Groups children by type: buttons → sliders → nested frames → others,
     * then lays each group out using computeFlowPosition.
     * Recurses into nested frames first (bottom-up) to determine their height.
     * Returns the required height of the container.
     */
    static int reflowChildren(WidgetSnapshot &container, const ReflowOptions &opts)
    {
        if (container.children.isEmpty())
            return opts.headerHeight + opts.pad;

        // Classify children by type, preserving order within each group
        QList<WidgetSnapshot *> buttons, sliders, frames, others;
        for (int i = 0; i < container.children.size(); ++i)
        {
            WidgetSnapshot &c = container.children[i];
            switch (c.type)
            {
                case 1: // ButtonWidget
                    buttons.append(&c); break;
                case 2: // SliderWidget
                    sliders.append(&c); break;
                case 4: // FrameWidget
                case 5: // SoloFrameWidget
                    frames.append(&c); break;
                default:
                    others.append(&c); break;
            }
        }

        int parentWidth = container.geometry.width();
        int y = opts.headerHeight;

        // Buttons in flow grid
        if (!buttons.isEmpty())
        {
            int cols = opts.columns > 0 ? opts.columns
                : qMax(1, (parentWidth - opts.pad) / (opts.defaultButtonWidth + opts.pad));
            for (int i = 0; i < buttons.size(); ++i)
            {
                QRect pos = computeFlowPosition(parentWidth, y, i,
                    opts.defaultButtonWidth, opts.defaultButtonHeight, cols, opts.pad);
                buttons[i]->geometry = pos;
            }
            int rows = (buttons.size() + cols - 1) / cols;
            y += rows * (opts.defaultButtonHeight + opts.pad);
        }

        // Sliders in flow grid
        if (!sliders.isEmpty())
        {
            int cols = opts.columns > 0 ? opts.columns
                : qMax(1, (parentWidth - opts.pad) / (opts.defaultSliderWidth + opts.pad));
            for (int i = 0; i < sliders.size(); ++i)
            {
                QRect pos = computeFlowPosition(parentWidth, y, i,
                    opts.defaultSliderWidth, opts.defaultSliderHeight, cols, opts.pad);
                sliders[i]->geometry = pos;
            }
            int rows = (sliders.size() + cols - 1) / cols;
            y += rows * (opts.defaultSliderHeight + opts.pad);
        }

        // Nested frames — recurse first to determine height
        for (WidgetSnapshot *f : frames)
        {
            int nestedHeight = reflowChildren(*f, opts);
            f->geometry = QRect(opts.pad, y, parentWidth - 2 * opts.pad, nestedHeight);
            y += nestedHeight + opts.pad;
        }

        // Other widgets — stack vertically
        for (WidgetSnapshot *o : others)
        {
            o->geometry = QRect(opts.pad, y, parentWidth - 2 * opts.pad, o->geometry.height());
            y += o->geometry.height() + opts.pad;
        }

        return y;
    }

    /**
     * Reflow an entire page: stacks top-level frames vertically,
     * reflowing each frame's children recursively.
     * Returns a LayoutPlan with all proposed geometry changes.
     */
    static LayoutPlan reflowPage(WidgetSnapshot &page, const ReflowOptions &opts)
    {
        LayoutPlan plan;
        int y = opts.pad;
        int pageWidth = page.geometry.width();

        for (int i = 0; i < page.children.size(); ++i)
        {
            WidgetSnapshot &child = page.children[i];
            bool isFrame = (child.type == 4 || child.type == 5);

            if (isFrame)
            {
                int requiredHeight = reflowChildren(child, opts);
                child.geometry = QRect(opts.pad, y, pageWidth - 2 * opts.pad, requiredHeight);
                y += requiredHeight + opts.framePad;
            }
            else
            {
                child.geometry = QRect(opts.pad, y, child.geometry.width(), child.geometry.height());
                y += child.geometry.height() + opts.pad;
            }
        }

        // Collect all changed geometries (recursive)
        collectGeometries(page, plan);

        // Detect overlaps at each level
        plan.overlaps = detectOverlaps(page.children);

        return plan;
    }

    /** Helper: recursively collect widget geometries from a snapshot tree into a LayoutPlan. */
    static void collectGeometries(const WidgetSnapshot &node, LayoutPlan &plan)
    {
        for (const WidgetSnapshot &child : node.children)
        {
            plan.geometries.insert(child.id, child.geometry);
            collectGeometries(child, plan);
        }
    }

    // ─── Virtual snapshot / apply (override in UI-specific bridge) ───

    virtual WidgetSnapshot snapshotFrame(int frameID) const
        { Q_UNUSED(frameID); return WidgetSnapshot(); }
    virtual WidgetSnapshot snapshotPage(int pageIndex) const
        { Q_UNUSED(pageIndex); return WidgetSnapshot(); }
    virtual void applyLayoutPlan(const LayoutPlan &plan)
        { Q_UNUSED(plan); }
};

#endif // VCBRIDGE_H
