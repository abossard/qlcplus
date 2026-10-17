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

namespace {

class MinimalVCBridge final : public VCBridge
{
public:
    int addPage(const QString &) override { return -1; }
    QList<PageInfo> pages() const override { return {}; }
    int pagesCount() const override { return 0; }
    int addFrame(int, const QRect &, const QString &, bool) override { return -1; }
    int addButton(int, const QRect &, quint32, const QString &, const QString &, int) override { return -1; }
    int addSlider(int, const QRect &, const QString &, const QString &, quint32, const QList<QPair<quint32, quint32>> &) override { return -1; }
    int addXYPad(int, const QRect &, const QList<quint32> &) override { return -1; }
    int addCueList(int, const QRect &, quint32, const QString &) override { return -1; }
    int addLabel(int, const QRect &, const QString &) override { return -1; }
    bool mapWidgetInput(int, quint32, quint32) override { return false; }
    bool setWidgetFeedback(int, int, int, int, int, int, int) override { return false; }
    bool setWidgetColors(int, const QColor &, const QColor &) override { return false; }
    int addSpeedDial(int, const QRect &, const QList<quint32> &) override { return -1; }
    int addAudioTriggers(int, const QRect &) override { return -1; }
    int addClock(int, const QRect &, const QString &) override { return -1; }
    int addRecordPanel(int, const QRect &) override { return -1; }
};

} // namespace

// ========== WidgetDetails defaults ==========

void VCBridgeStructs_Test::widgetDetails_defaultValues()
{
    VCBridge::WidgetDetails d;

    QCOMPARE(d.id, -1);
    QVERIFY(d.type.isEmpty());
    QVERIFY(d.caption.isEmpty());
    QCOMPARE(d.geometry, QRect());
    QCOMPARE(d.functionID, (quint32)-1);
    QVERIFY(d.action.isEmpty());
    QVERIFY(d.sliderMode.isEmpty());
    QVERIFY(d.channels.isEmpty());
    QVERIFY(d.inputMappings.isEmpty());
    QCOMPARE(d.parentID, -1);
    QCOMPARE(d.childPageIndex, 0);

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
    QVERIFY(d.pageLabels.isEmpty());
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
    QVERIFY(!cfg.pagesLoop.has_value());
    QVERIFY(!cfg.pageLabels.has_value());
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
    cfg.pagesLoop = true;
    cfg.pageLabels = QStringList{"Intro", "Drop", "Outro"};
    cfg.headerVisible = false;
    cfg.enableButtonVisible = true;
    cfg.collapsed = false;
    cfg.soloframeMixing = true;
    cfg.excludeMonitoredFunctions = true;

    QCOMPARE(cfg.multipageMode.value(), true);
    QCOMPARE(cfg.totalPages.value(), 5);
    QCOMPARE(cfg.pagesLoop.value(), true);
    QCOMPARE(cfg.pageLabels.value(), (QStringList{"Intro", "Drop", "Outro"}));
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

void VCBridgeStructs_Test::authoringMethods_defaultUnsupportedBridge()
{
    MinimalVCBridge bridge;

    VCBridge::AuthoringSetupQuery query;
    const VCBridge::AuthoringReadResult readResult = bridge.queryAuthoring(VCBridge::AuthoringQuery(query));
    QCOMPARE(readResult.code, QStringLiteral("unsupportedBridge"));
    QVERIFY(!readResult.error.isEmpty());
    QCOMPARE(readResult.complete, false);
    QVERIFY(readResult.nextCursor.isEmpty());

    VCBridge::AuthoringLayoutRequest request;
    VCBridge::AuthoringGeometryChange geometry;
    geometry.widgetID = 42;
    geometry.logicalRect = QRectF(10.0, 20.0, 100.0, 80.0);
    request.geometryChanges.append(geometry);
    const VCBridge::AuthoringPreparationResult prepResult = bridge.prepareAuthoring(VCBridge::AuthoringRequest(request));
    QCOMPARE(prepResult.code, QStringLiteral("unsupportedBridge"));
    QVERIFY(!prepResult.error.isEmpty());
    QVERIFY(prepResult.preparationToken.isEmpty());
    QCOMPARE(prepResult.expiresInMs, 0);

    const VCBridge::AuthoringApplyResult applyResult = bridge.applyAuthoring(QStringLiteral("token"));
    QCOMPARE(applyResult.code, QStringLiteral("unsupportedBridge"));
    QVERIFY(!applyResult.error.isEmpty());
    QCOMPARE(applyResult.applied, false);
    QCOMPARE(applyResult.tokenConsumed, false);
}

void VCBridgeStructs_Test::authoringContracts_closedDefaults()
{
    VCBridge::AuthoringSetupQuery setup;
    QCOMPARE(setup.kind, VCBridge::AuthoringSetupQuery::Kind::Widgets);
    QCOMPARE(setup.limit, 100);
    QCOMPARE(setup.recursive, true);
    QCOMPARE(setup.pageWidgetID, (quint32)-1);
    QCOMPARE(setup.frameWidgetID, (quint32)-1);
    QCOMPARE(setup.childSubpage, -1);

    VCBridge::AuthoringTargetControlQuery controls;
    QCOMPARE(controls.kind, VCBridge::AuthoringTargetControlQuery::Kind::Targets);
    QCOMPARE(controls.limit, 100);
    QCOMPARE(controls.inputUniverseID, (quint32)-1);

    VCBridge::AuthoringRecordingQuery recordings;
    QCOMPARE(recordings.kind, VCBridge::AuthoringRecordingQuery::Kind::References);
    QCOMPARE(recordings.limit, 100);
    QCOMPARE(recordings.showFunctionID, (quint32)-1);
    QCOMPARE(recordings.fromTimeMs, (qint64)-1);
    QCOMPARE(recordings.toTimeMs, (qint64)-1);

    VCBridge::AuthoringLayoutRequest layout;
    QCOMPARE(layout.operation, VCBridge::AuthoringLayoutRequest::Operation::Geometry);
    QCOMPARE(layout.snapToGrid, false);
    QCOMPARE(layout.requireAtomic, false);
    QCOMPARE(layout.diffDetail, QStringLiteral("summary"));
    QVERIFY(!layout.destinationFrameWidgetID.has_value());
    QVERIFY(!layout.pageWidgetID.has_value());
    QVERIFY(!layout.frameWidgetID.has_value());

    VCBridge::AuthoringBindingItem binding;
    QCOMPARE(binding.kind, VCBridge::AuthoringBindingItem::Kind::Function);
    QCOMPARE(binding.action, VCBridge::AuthoringBindingItem::Action::Set);
    QCOMPARE(binding.widgetID, (quint32)-1);
    QVERIFY(!binding.controlSlotID.has_value());
    QVERIFY(!binding.functionID.has_value());
    QVERIFY(!binding.sourceKey.has_value());
    QVERIFY(!binding.destination.has_value());
    QVERIFY(!binding.idleValue.has_value());
    QVERIFY(!binding.idleChannel.has_value());

    VCBridge::AuthoringBindingsRequest bindings;
    QCOMPARE(bindings.allowFanout, false);
    QCOMPARE(bindings.requireAtomic, false);
    QCOMPARE(bindings.diffDetail, QStringLiteral("summary"));
    QVERIFY(bindings.items.isEmpty());

    VCBridge::AuthoringRecordPanelItem panelItem;
    QCOMPARE(panelItem.widgetID, (quint32)-1);
    QVERIFY(!panelItem.targetFolder.has_value());
    QVERIFY(!panelItem.defaultFadeIn.has_value());

    VCBridge::AuthoringRecordPanelRequest panelReq;
    QCOMPARE(panelReq.requireAtomic, false);
    QCOMPARE(panelReq.diffDetail, QStringLiteral("summary"));
    QVERIFY(panelReq.items.isEmpty());

    VCBridge::AuthoringReadResult read;
    QCOMPARE(read.version, 1);
    QCOMPARE(read.complete, false);
    QVERIFY(read.rows.isEmpty());
    QCOMPARE(read.work.scannedSummaryRows, 0);
    QVERIFY(read.limitations.isEmpty());

    VCBridge::AuthoringPreparationItem prepItem;
    QCOMPARE(prepItem.changedCount, 0);
    QVERIFY(prepItem.changes.isEmpty());
    QVERIFY(prepItem.limitations.isEmpty());

    VCBridge::AuthoringPreparationResult prep;
    QCOMPARE(prep.version, 1);
    QCOMPARE(prep.diffDetail, VCBridge::AuthoringPreparationResult::DiffDetail::Summary);
    QCOMPARE(prep.diffComplete, true);
    QVERIFY(prep.preconditions.isEmpty());
    QVERIFY(prep.limitations.isEmpty());
    QCOMPARE(prep.work.payloadBytes, 0);

    VCBridge::AuthoringApplyItem applyItem;
    QCOMPARE(applyItem.changedCount, 0);
    QVERIFY(applyItem.changes.isEmpty());
    QVERIFY(applyItem.limitations.isEmpty());

    VCBridge::AuthoringApplyResult apply;
    QCOMPARE(apply.version, 1);
    QCOMPARE(apply.applied, false);
    QCOMPARE(apply.tokenConsumed, false);
    QVERIFY(apply.limitations.isEmpty());
    QCOMPARE(apply.work.hydratedRows, 0);
}

QTEST_MAIN(VCBridgeStructs_Test)
