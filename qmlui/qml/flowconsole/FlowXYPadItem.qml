/*
  Q Light Controller Plus
  FlowXYPadItem.qml

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
import "."

FlowWidgetItem
{
    id: xyPadRoot
    property VCXYPad xyPadObj: null
    property point currPosition: xyPadObj ? xyPadObj.currentPosition : Qt.point(0, 0)
    property point horizRange: xyPadObj ? xyPadObj.horizontalRange : Qt.point(0, 255)
    property point vertRange: xyPadObj ? xyPadObj.verticalRange : Qt.point(0, 255)

    Layout.preferredHeight: 250
    Layout.minimumHeight: 150
    clip: true

    onXyPadObjChanged: wObj = xyPadObj

    GridLayout
    {
        anchors.fill: parent
        rowSpacing: 0
        columnSpacing: 0
        columns: 3

        Rectangle { height: UISettings.listItemHeight; width: height; color: "transparent" }

        CustomRangeSlider
        {
            Layout.fillWidth: true
            topPadding: 0; bottomPadding: 0
            from: 0; to: 255; bgColor: "turquoise"
            first.value: horizRange.x; second.value: horizRange.y
            first.onMoved: if (xyPadObj) xyPadObj.horizontalRange = Qt.point(first.value, second.value)
            second.onMoved: if (xyPadObj) xyPadObj.horizontalRange = Qt.point(first.value, second.value)
        }

        Rectangle { height: UISettings.listItemHeight; width: height; color: "transparent" }

        CustomRangeSlider
        {
            Layout.fillHeight: true
            rightPadding: 0; orientation: Qt.Vertical; rotation: 180
            from: 0; to: 255; bgColor: "turquoise"
            first.value: vertRange.x; second.value: vertRange.y
            first.onMoved: if (xyPadObj) xyPadObj.verticalRange = Qt.point(first.value, second.value)
            second.onMoved: if (xyPadObj) xyPadObj.verticalRange = Qt.point(first.value, second.value)
        }

        Rectangle
        {
            id: previewArea
            Layout.fillHeight: true
            Layout.fillWidth: true
            clip: true
            color: UISettings.bgStrong

            Rectangle
            {
                visible: horizRange.x !== 0 || horizRange.y !== 255 || vertRange.x !== 0 || vertRange.y !== 255
                x: (horizRange.x * previewArea.width) / 255.0
                y: (vertRange.x * previewArea.height) / 255.0
                width: ((horizRange.y * previewArea.width) / 255.0) - x
                height: ((vertRange.y * previewArea.height) / 255.0) - y
                color: "darkcyan"; border.width: 1; border.color: "cyan"; opacity: 0.5
            }

            Rectangle
            {
                x: ((currPosition.x * previewArea.width) / 255.0) - (width / 2)
                y: ((currPosition.y * previewArea.height) / 255.0) - (height / 2)
                width: UISettings.iconSizeMedium * 0.5; height: width; radius: width / 2
                color: UISettings.highlight; border.width: 1; border.color: UISettings.highlightPressed
            }

            MouseArea
            {
                anchors.fill: parent
                function getXYPos(mouse) {
                    var x = Math.min(Math.max(mouse.x, 0), previewArea.width)
                    var y = Math.min(Math.max(mouse.y, 0), previewArea.height)
                    return Qt.point((x * 255.0) / previewArea.width, (y * 255.0) / previewArea.height)
                }
                onPressed: (mouse) => { if (xyPadObj) xyPadObj.currentPosition = getXYPos(mouse) }
                onPositionChanged: (mouse) => { if (pressed && xyPadObj) xyPadObj.currentPosition = getXYPos(mouse) }
            }
        }

        CustomSlider
        {
            Layout.fillHeight: true; orientation: Qt.Vertical
            from: 0; to: 255; value: to - currPosition.y
            onMoved: if (xyPadObj) xyPadObj.currentPosition = Qt.point(xSlider.value, to - value)
        }

        Rectangle { height: UISettings.listItemHeight; width: height; color: "transparent" }

        CustomSlider
        {
            id: xSlider
            Layout.fillWidth: true; from: 0; to: 255; value: currPosition.x
            onMoved: if (xyPadObj) xyPadObj.currentPosition = Qt.point(value, 255 - (parent.children[parent.children.length - 3].value))
        }

        Rectangle { height: UISettings.listItemHeight; width: height; color: "transparent" }
    }
}
