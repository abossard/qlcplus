/*
  Q Light Controller Plus
  FlowSpeedDialItem.qml

  Copyright (c) Massimo Callegari

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import org.qlcplus.classes 1.0
import "TimeUtils.js" as TimeUtils
import "."

FlowWidgetItem
{
    id: speedRoot
    property VCSpeedDial speedObj: null
    property int vMask: speedObj ? speedObj.visibilityMask : VCSpeedDial.Nothing
    property color activeColor: "green"
    property int currTime: speedObj ? speedObj.currentTime : 0

    Layout.preferredHeight: 250
    Layout.minimumHeight: 120
    clip: true

    onSpeedObjChanged: wObj = speedObj

    onCurrTimeChanged:
    {
        var value = currTime
        var h = Math.floor(value / 3600000); value -= (h * 3600000)
        var m = Math.floor(value / 60000); value -= (m * 60000)
        var s = Math.floor(value / 1000); value -= (s * 1000)
        hoursSpin.value = h; minutesSpin.value = m; secondsSpin.value = s; msSpin.value = value
    }

    function updateTime()
    {
        var newTime = 0
        if (hoursSpin.visible) newTime += hoursSpin.value * 3600000
        if (minutesSpin.visible) newTime += minutesSpin.value * 60000
        if (secondsSpin.visible) newTime += secondsSpin.value * 1000
        if (msSpin.visible) newTime += msSpin.value
        speedObj.currentTime = newTime
    }

    GridLayout
    {
        anchors.fill: parent
        columns: tapButton.visible ? 6 : 4

        Text
        {
            visible: speedObj && speedObj.caption.length
            Layout.columnSpan: parent.columns
            Layout.fillWidth: true
            height: UISettings.listItemHeight
            font: speedObj ? speedObj.font : Qt.font({ family: UISettings.robotoFontName })
            text: speedObj ? speedObj.caption : ""
            color: speedObj ? speedObj.foregroundColor : "white"
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        QLCPlusKnob
        {
            Layout.columnSpan: tapButton.visible ? 4 : parent.columns
            Layout.rowSpan: tapButton.visible ? 2 : 1
            Layout.fillWidth: true; Layout.fillHeight: true
            visible: vMask & VCSpeedDial.Dial
            drawOuterLevel: false; from: 0; to: 1000; wrap: true
            property int lastValue: 0; property int threshold: 50
            onMoved: {
                var diff = value - lastValue
                if (diff > threshold) diff = -stepSize
                else if (diff < -threshold) diff = stepSize
                lastValue = value
                if (speedObj) speedObj.currentTime += diff
            }
        }

        Repeater
        {
            model: ["1/16", "1/8", "1/4", "1/2"]
            GenericButton
            {
                Layout.fillWidth: true; Layout.fillHeight: true
                label: modelData; visible: vMask & VCSpeedDial.Beats
                property var factors: [VCSpeedDial.OneSixteenth, VCSpeedDial.OneEighth, VCSpeedDial.OneFourth, VCSpeedDial.Half]
                bgColor: speedObj && speedObj.currentFactor === factors[index] ? activeColor : UISettings.bgControl
                onClicked: if (speedObj) speedObj.currentFactor = factors[index]
            }
        }

        GenericButton
        {
            id: tapButton
            Layout.columnSpan: 2; Layout.rowSpan: 2; Layout.fillHeight: true
            label: "TAP"; visible: vMask & VCSpeedDial.Tap
            onClicked: (mouseButton) => { if (speedObj) { mouseButton === Qt.RightButton ? speedObj.resetTap() : speedObj.tap() } }
        }

        Repeater
        {
            model: ["2", "4", "8", "16"]
            GenericButton
            {
                Layout.fillWidth: true; Layout.fillHeight: true
                label: modelData; visible: vMask & VCSpeedDial.Beats
                property var factors: [VCSpeedDial.Two, VCSpeedDial.Four, VCSpeedDial.Eight, VCSpeedDial.Sixteen]
                bgColor: speedObj && speedObj.currentFactor === factors[index] ? activeColor : UISettings.bgControl
                onClicked: if (speedObj) speedObj.currentFactor = factors[index]
            }
        }

        RowLayout
        {
            visible: vMask & VCSpeedDial.Hours | vMask & VCSpeedDial.Minutes | vMask & VCSpeedDial.Seconds | vMask & VCSpeedDial.Milliseconds
            Layout.columnSpan: parent.columns; Layout.fillWidth: true

            CustomSpinBox { id: hoursSpin; Layout.fillWidth: true; visible: vMask & VCSpeedDial.Hours; from: 0; to: 999; suffix: "h"; onValueModified: updateTime() }
            CustomSpinBox { id: minutesSpin; Layout.fillWidth: true; visible: vMask & VCSpeedDial.Minutes; from: 0; to: 59; suffix: "m"; onValueModified: updateTime() }
            CustomSpinBox { id: secondsSpin; Layout.fillWidth: true; visible: vMask & VCSpeedDial.Seconds; from: 0; to: 59; suffix: "s"; onValueModified: updateTime() }
            CustomSpinBox { id: msSpin; Layout.fillWidth: true; visible: vMask & VCSpeedDial.Milliseconds; from: 0; to: 999; suffix: "ms"; onValueModified: updateTime() }
        }
    }

    DropArea
    {
        id: dropArea
        anchors.fill: parent; z: 2; keys: [ "function" ]
        onDropped: {
            if (drag.source.hasOwnProperty("fromFunctionManager"))
                for (var i = 0; i < drag.source.itemsList.length; i++)
                    speedObj.addFunction(drag.source.itemsList[i])
        }
        states: [ State { when: dropArea.containsDrag; PropertyChanges { target: speedRoot; color: UISettings.activeDropArea } } ]
    }
}
