/*
  Q Light Controller Plus - Unit test
  vc_bridge_structs_test.cpp

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

#include <QtTest>

#include "vc_bridge_structs_test.h"
#include "vcbridge.h"

// ========== WidgetDetails defaults ==========

void VCBridgeStructs_Test::widgetDetails_defaultValues()
{
    VCBridge::WidgetDetails d;

    QCOMPARE(d.id, -1);
    QVERIFY(d.type.isEmpty());
    QVERIFY(d.caption.isEmpty());
    QCOMPARE(d.geometry, QRect());
    QCOMPARE(d.functionID, (quint32)0);
    QVERIFY(d.action.isEmpty());
    QVERIFY(d.sliderMode.isEmpty());
    QVERIFY(d.channels.isEmpty());
    QVERIFY(d.inputMappings.isEmpty());
    QCOMPARE(d.parentID, -1);

    // Slider defaults
    QVERIFY(d.clickAndGoType.isEmpty());
    QVERIFY(d.valueDisplayStyle.isEmpty());
    QCOMPARE(d.sliderInvertedAppearance, false);
    QCOMPARE(d.rangeLowLimit, (qreal)0);
    QCOMPARE(d.rangeHighLimit, (qreal)255);
    QCOMPARE(d.monitorEnabled, false);

    // XY Pad defaults
    QVERIFY(d.displayMode.isEmpty());
    QCOMPARE(d.invertedAppearance, false);
    QVERIFY(d.xyPadFixtures.isEmpty());
    QCOMPARE(d.xyPadPosition, QPointF());

    // Audio Triggers defaults
    QCOMPARE(d.captureEnabled, false);
    QCOMPARE(d.volumeLevel, 100);
    QCOMPARE(d.barsNumber, 0);

    // Button extended defaults
    QVERIFY(d.iconPath.isEmpty());
    QCOMPARE(d.startupIntensityEnabled, false);
    QCOMPARE(d.startupIntensity, (qreal)1.0);
    QCOMPARE(d.flashOverride, false);
    QCOMPARE(d.flashForceLTP, false);
    QCOMPARE(d.stopAllFadeTime, 0);

    // Slider extended defaults
    QVERIFY(d.widgetStyle.isEmpty());
    QCOMPARE(d.catchValues, false);

    // Frame defaults
    QCOMPARE(d.multipageMode, false);
    QCOMPARE(d.totalPages, 1);
    QCOMPARE(d.currentPage, 0);
    QCOMPARE(d.pagesLoop, false);
    QCOMPARE(d.headerVisible, true);
    QCOMPARE(d.enableButtonVisible, false);
    QCOMPARE(d.collapsed, false);
    QCOMPARE(d.soloframeMixing, false);
    QCOMPARE(d.excludeMonitoredFunctions, false);

    // CueList defaults
    QVERIFY(d.nextPrevBehavior.isEmpty());
    QVERIFY(d.playbackLayout.isEmpty());
    QVERIFY(d.sideFaderMode.isEmpty());

    // Clock defaults
    QVERIFY(d.clockType.isEmpty());
    QCOMPARE(d.countdownH, 0);
    QCOMPARE(d.countdownM, 0);
    QCOMPARE(d.countdownS, 0);
    QVERIFY(d.clockSchedules.isEmpty());

    // SpeedDial defaults
    QVERIFY(d.speedDialFunctions.isEmpty());
    QVERIFY(d.speedDialPresets.isEmpty());
    QCOMPARE(d.absoluteValueMin, (quint32)0);
    QCOMPARE(d.absoluteValueMax, (quint32)0);
    QCOMPARE(d.speedDialVisibilityMask, (quint32)0);
    QCOMPARE(d.resetFactorOnDialChange, false);

    // Matrix defaults
    QCOMPARE(d.matrixVisibilityMask, (quint32)0);
    QCOMPARE(d.matrixInstantApply, false);

    // XY Pad presets
    QVERIFY(d.xyPadPresets.isEmpty());

    // Base widget extended
    QVERIFY(d.backgroundImage.isEmpty());
    QCOMPARE(d.disabled, false);
}

// ========== Config struct defaults ==========

void VCBridgeStructs_Test::buttonConfig_allOptionalEmpty()
{
    VCBridge::ButtonConfig cfg;
    QVERIFY(!cfg.functionID.has_value());
    QVERIFY(!cfg.action.has_value());
    QVERIFY(!cfg.iconPath.has_value());
    QVERIFY(!cfg.startupIntensityEnabled.has_value());
    QVERIFY(!cfg.startupIntensity.has_value());
    QVERIFY(!cfg.flashOverride.has_value());
    QVERIFY(!cfg.flashForceLTP.has_value());
    QVERIFY(!cfg.stopAllFadeTime.has_value());
}

void VCBridgeStructs_Test::frameConfig_allOptionalEmpty()
{
    VCBridge::FrameConfig cfg;
    QVERIFY(!cfg.multipageMode.has_value());
    QVERIFY(!cfg.totalPages.has_value());
    QVERIFY(!cfg.currentPage.has_value());
    QVERIFY(!cfg.pagesLoop.has_value());
    QVERIFY(!cfg.headerVisible.has_value());
    QVERIFY(!cfg.enableButtonVisible.has_value());
    QVERIFY(!cfg.collapsed.has_value());
    QVERIFY(!cfg.soloframeMixing.has_value());
    QVERIFY(!cfg.excludeMonitoredFunctions.has_value());
}

void VCBridgeStructs_Test::matrixConfig_allOptionalEmpty()
{
    VCBridge::MatrixConfig cfg;
    QVERIFY(!cfg.functionID.has_value());
    QVERIFY(!cfg.color1.has_value());
    QVERIFY(!cfg.color2.has_value());
    QVERIFY(!cfg.color3.has_value());
    QVERIFY(!cfg.color4.has_value());
    QVERIFY(!cfg.color5.has_value());
    QVERIFY(!cfg.animation.has_value());
    QVERIFY(!cfg.instantApply.has_value());
    QVERIFY(!cfg.visibilityMask.has_value());
}

void VCBridgeStructs_Test::cueListConfig_allOptionalEmpty()
{
    VCBridge::CueListConfig cfg;
    QVERIFY(!cfg.chaserID.has_value());
    QVERIFY(!cfg.nextPrevBehavior.has_value());
    QVERIFY(!cfg.playbackLayout.has_value());
    QVERIFY(!cfg.sideFaderMode.has_value());
}

void VCBridgeStructs_Test::clockConfig_allOptionalEmpty()
{
    VCBridge::ClockConfig cfg;
    QVERIFY(!cfg.clockType.has_value());
    QVERIFY(!cfg.countdownH.has_value());
    QVERIFY(!cfg.countdownM.has_value());
    QVERIFY(!cfg.countdownS.has_value());
    QVERIFY(!cfg.schedules.has_value());
}

void VCBridgeStructs_Test::speedDialConfig_allOptionalEmpty()
{
    VCBridge::SpeedDialConfig cfg;
    QVERIFY(!cfg.functions.has_value());
    QVERIFY(!cfg.presets.has_value());
    QVERIFY(!cfg.absoluteValueMin.has_value());
    QVERIFY(!cfg.absoluteValueMax.has_value());
    QVERIFY(!cfg.visibilityMask.has_value());
    QVERIFY(!cfg.resetFactorOnDialChange.has_value());
}

void VCBridgeStructs_Test::sliderConfig_allOptionalEmpty()
{
    VCBridge::SliderConfig cfg;
    QVERIFY(!cfg.clickAndGoType.has_value());
    QVERIFY(!cfg.valueDisplayStyle.has_value());
    QVERIFY(!cfg.invertedAppearance.has_value());
    QVERIFY(!cfg.rangeLowLimit.has_value());
    QVERIFY(!cfg.rangeHighLimit.has_value());
    QVERIFY(!cfg.monitorEnabled.has_value());
    QVERIFY(!cfg.gmValueMode.has_value());
    QVERIFY(!cfg.gmChannelMode.has_value());
}

// ========== Config struct population ==========

void VCBridgeStructs_Test::buttonConfig_setAndRead()
{
    VCBridge::ButtonConfig cfg;
    cfg.functionID = 42;
    cfg.action = "flash";
    cfg.iconPath = "/icons/go.png";
    cfg.startupIntensityEnabled = true;
    cfg.startupIntensity = 0.75;
    cfg.flashOverride = true;
    cfg.flashForceLTP = false;
    cfg.stopAllFadeTime = 2000;

    QCOMPARE(cfg.functionID.value(), (quint32)42);
    QCOMPARE(cfg.action.value(), QString("flash"));
    QCOMPARE(cfg.iconPath.value(), QString("/icons/go.png"));
    QCOMPARE(cfg.startupIntensityEnabled.value(), true);
    QCOMPARE(cfg.startupIntensity.value(), (qreal)0.75);
    QCOMPARE(cfg.flashOverride.value(), true);
    QCOMPARE(cfg.flashForceLTP.value(), false);
    QCOMPARE(cfg.stopAllFadeTime.value(), 2000);
}

void VCBridgeStructs_Test::frameConfig_setAndRead()
{
    VCBridge::FrameConfig cfg;
    cfg.multipageMode = true;
    cfg.totalPages = 5;
    cfg.currentPage = 2;
    cfg.pagesLoop = true;
    cfg.headerVisible = false;
    cfg.enableButtonVisible = true;
    cfg.collapsed = false;
    cfg.soloframeMixing = true;
    cfg.excludeMonitoredFunctions = true;

    QCOMPARE(cfg.multipageMode.value(), true);
    QCOMPARE(cfg.totalPages.value(), 5);
    QCOMPARE(cfg.currentPage.value(), 2);
    QCOMPARE(cfg.pagesLoop.value(), true);
    QCOMPARE(cfg.headerVisible.value(), false);
    QCOMPARE(cfg.enableButtonVisible.value(), true);
    QCOMPARE(cfg.collapsed.value(), false);
    QCOMPARE(cfg.soloframeMixing.value(), true);
    QCOMPARE(cfg.excludeMonitoredFunctions.value(), true);
}

void VCBridgeStructs_Test::clockScheduleInfo_defaults()
{
    VCBridge::ClockScheduleInfo info;
    QCOMPARE(info.functionID, (quint32)-1);
    QCOMPARE(info.hour, 0);
    QCOMPARE(info.minute, 0);
    QCOMPARE(info.second, 0);
}

void VCBridgeStructs_Test::speedDialFunctionInfo_defaults()
{
    VCBridge::SpeedDialFunctionInfo info;
    QCOMPARE(info.functionID, (quint32)-1);
    QCOMPARE(info.fadeInMultiplier, QString("none"));
    QCOMPARE(info.fadeOutMultiplier, QString("none"));
    QCOMPARE(info.durationMultiplier, QString("1"));
}

void VCBridgeStructs_Test::fontConfig_setAndRead()
{
    VCBridge::FontConfig cfg;

    // Defaults: all empty
    QVERIFY(!cfg.family.has_value());
    QVERIFY(!cfg.pointSize.has_value());
    QVERIFY(!cfg.bold.has_value());
    QVERIFY(!cfg.italic.has_value());

    // Set values
    cfg.family = "Courier New";
    cfg.pointSize = 18;
    cfg.bold = true;
    cfg.italic = false;

    QCOMPARE(cfg.family.value(), QString("Courier New"));
    QCOMPARE(cfg.pointSize.value(), 18);
    QCOMPARE(cfg.bold.value(), true);
    QCOMPARE(cfg.italic.value(), false);
}

void VCBridgeStructs_Test::xyPadPresetInfo_defaults()
{
    VCBridge::XYPadPresetInfo info;
    QVERIFY(info.name.isEmpty());
    QVERIFY(info.type.isEmpty());
    QCOMPARE(info.position, QPointF());
    QCOMPARE(info.functionID, (quint32)-1);
}

QTEST_MAIN(VCBridgeStructs_Test)
