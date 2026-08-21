/*
  Q Light Controller Plus
  VCFrameItem.qml

  Copyright (c) Massimo Callegari

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

import QtQuick
import QtQuick.Layouts

import org.qlcplus.classes 1.0
import "."

VCWidgetItem
{
    id: frameRoot
    property VCFrame frameObj: null
    property bool dropActive: false
    property bool isSolo: false
    property bool isCollapsed: frameObj ? frameObj.isCollapsed : false

    color: dropActive ? UISettings.activeDropArea : (frameObj ? frameObj.backgroundColor : "darkgray")
    clip: true

    onFrameObjChanged:
    {
        setCommonProperties(frameObj)
        if (isSolo)
            frameRoot.border.color = "red"
    }

    onIsCollapsedChanged:
    {
        frameRoot.width = isCollapsed ? UISettings.bigItemHeight * 2 : frameObj.geometry.width
        frameRoot.height = isCollapsed ? UISettings.listItemHeight : frameObj.geometry.height
    }

    // Frame header
    Rectangle
    {
        id: frameHeader
        width: parent.width
        height: UISettings.listItemHeight
        color: "transparent"
        visible: frameObj ? frameObj.showHeader : false

        RowLayout
        {
            x: 1
            y: 1
            height: parent.height - 2
            width: parent.width - 2
            spacing: 1

            // expand/collapse button
            IconButton
            {
                width: height
                height: parent.height
                radius: 0
                border.width: 0
                tooltip: qsTr("Expand/Collapse this frame")
                faSource: checked ? FontAwesome.fa_up_right_and_down_left_from_center : FontAwesome.fa_down_left_and_up_right_to_center
                faColor: UISettings.fgMain
                checkable: true
                checked: isCollapsed
                //checkedColor: bgColor
                onToggled: frameObj.isCollapsed = checked
            }

            // header bar and caption
            Rectangle
            {
                height: parent.height
                //radius: 3
                gradient: Gradient
                {
                    GradientStop { position: 0; color: isSolo ? UISettings.soloFrameHeaderStart : UISettings.frameHeaderStart }
                    GradientStop { position: 1; color: isSolo ? UISettings.soloFrameHeaderEnd : UISettings.frameHeaderEnd }
                }
                Layout.fillWidth: true

                Text
                {
                    x: 2
                    width: parent.width - 4
                    height: parent.height
                    font: frameObj ? frameObj.font : Qt.font({ family: UISettings.robotoFontName })
                    text: frameObj ? frameObj.caption : ""
                    verticalAlignment: Text.AlignVCenter
                    color: frameObj ? frameObj.foregroundColor : "white"
                }
            }

            // enable button
            IconButton
            {
                width: height
                height: parent.height
                radius: 0
                border.width: 0
                checkable: true
                tooltip: qsTr("Enable/Disable this frame")
                faSource: FontAwesome.fa_check
                faColor: "lime"
                imgMargins: 1
                checked: frameObj ? !frameObj.isDisabled : true
                visible: frameObj ? frameObj.showEnable : true
                onToggled: if (frameObj) frameObj.isDisabled = !checked
            }

            // multi page controls
            RowLayout
            {
                visible: frameObj ? frameObj.multiPageMode : false
                height: parent.height
                spacing: 2

                IconButton
                {
                    width: height
                    height: parent.height
                    radius: 0
                    border.width: 0
                    tooltip: qsTr("Previous page")
                    faSource: FontAwesome.fa_angle_left
                    faColor: UISettings.fgMain
                    onClicked: frameObj.gotoPreviousPage()
                }
                CustomComboBox
                {
                    id: pageSelector
                    implicitWidth: Math.max(frameHeader.width * 0.25, UISettings.bigItemHeight)
                    height: parent.height
                    textRole: ""
                    model: frameObj ? frameObj.pageLabels : null
                    currentIndex: frameObj ? frameObj.currentPage : 0
                    onActivated: (index) =>
                    {
                        if (frameObj)
                            frameObj.currentPage = index
                        // binding got  broken, so restore it
                        currentIndex = Qt.binding(function() { return frameObj.currentPage })
                    }
                }
                IconButton
                {
                    x: parent.width - width - 2
                    width: height
                    height: parent.height
                    radius: 0
                    border.width: 0
                    tooltip: qsTr("Next page")
                    faSource: FontAwesome.fa_angle_right
                    faColor: UISettings.fgMain
                    onClicked: frameObj.gotoNextPage()
                }
            }
        }
    }

    /* This DropArea has a dual usage:
     * 1- it is the parent of the frame chidren
     * 2- it is an actual drop area to drag/drop new or existing widgets
     */
    DropArea
    {
        id: dropArea
        anchors.fill: parent
        objectName: frameObj ? "frameDropArea" + frameObj.id : ""
        z: 5 // children must be above the VCWidget resizeLayer

        // Visual grid overlay
        Canvas
        {
            id: gridCanvas
            anchors.fill: parent
            visible: virtualConsole && virtualConsole.editMode &&
                     (virtualConsole.snapping ||
                      (frameObj && frameObj.layoutMode === VCFrame.LayoutGrid))
            z: 0
            opacity: frameObj && frameObj.layoutMode === VCFrame.LayoutGrid ? 1.0 : 0.15

            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()

            Connections
            {
                target: virtualConsole
                function onSnappingSizeChanged() { gridCanvas.requestPaint() }
                function onSnappingChanged() { gridCanvas.requestPaint() }
                function onEditModeChanged() { gridCanvas.requestPaint() }
            }

            Connections
            {
                target: frameObj
                ignoreUnknownSignals: true
                function onLayoutModeChanged() { gridCanvas.requestPaint() }
                function onGridColumnsChanged() { gridCanvas.requestPaint() }
                function onGridRowHeightChanged() { gridCanvas.requestPaint() }
            }

            onPaint:
            {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)

                if (!visible) return

                var gridMode = frameObj && frameObj.layoutMode === VCFrame.LayoutGrid

                if (gridMode)
                {
                    var cols = frameObj.gridColumns > 0 ? frameObj.gridColumns : 12
                    var cellW = width / cols
                    var cellH = frameObj.gridRowHeight > 0 ? frameObj.gridRowHeight : virtualConsole.snappingSize
                    if (cellW < 2 || cellH < 2) return

                    ctx.strokeStyle = Qt.rgba(0.39, 0.59, 1.0, 0.3) // rgba(100,150,255,0.3)
                    ctx.lineWidth = 1.0

                    for (var gi = 1; gi < cols; gi++)
                    {
                        var gx = gi * cellW
                        ctx.beginPath()
                        ctx.moveTo(gx, 0)
                        ctx.lineTo(gx, height)
                        ctx.stroke()
                    }

                    for (var gy = cellH; gy < height; gy += cellH)
                    {
                        ctx.beginPath()
                        ctx.moveTo(0, gy)
                        ctx.lineTo(width, gy)
                        ctx.stroke()
                    }
                    return
                }

                var s = virtualConsole.snappingSize
                if (s < 3) return

                ctx.strokeStyle = Qt.rgba(1, 1, 1, 0.5)
                ctx.lineWidth = 0.5

                // Vertical lines
                for (var x = s; x < width; x += s)
                {
                    ctx.beginPath()
                    ctx.moveTo(x, 0)
                    ctx.lineTo(x, height)
                    ctx.stroke()
                }

                // Horizontal lines
                for (var y = s; y < height; y += s)
                {
                    ctx.beginPath()
                    ctx.moveTo(0, y)
                    ctx.lineTo(width, y)
                    ctx.stroke()
                }
            }
        }

        onEntered: frameRoot.dropActive = true
        onExited: frameRoot.dropActive = false
        onDropped: (drop) =>
        {
            if (frameObj == null || frameRoot.dropActive === false)
                return

            frameRoot.dropActive = false

            if (drag.source.wObj === frameObj)
            {
                console.log("ERROR: Source and target are the same!!!")
                return
            }

            var pos = drag.source.mapToItem(frameRoot, 0, 0)
            console.log("Item dropped in frame " + frameObj.id + " at pos " + pos)

            // TODO: when frameObj.layoutMode === VCFrame.LayoutGrid, call a
            // (not-yet-exposed) relayoutGrid() hook on the C++ VCFrame to
            // re-pack children using GridLayout after drop. Requires additional
            // C++ integration; deferred.

            //console.log("Drop keys: " + drop.keys)
            if (drop.keys[0] === "vcwidget")
            {
                if (drag.source.widgetType)
                {
                    if (drag.source.widgetType === "buttonmatrix" || drag.source.widgetType === "slidermatrix")
                        virtualConsole.requestAddMatrixPopup(frameObj, dropArea, drag.source.widgetType, pos)
                    else
                        frameObj.addWidget(dropArea, drag.source.widgetType, pos)
                }
                else
                {
                    // reparent the QML item first
                    drag.source.parent = dropArea
                    virtualConsole.moveWidget(drag.source.wObj, frameObj, pos)

                }
            }
            else if (drop.keys[0] === "function")
            {
                frameObj.addFunctions(dropArea, drag.source.itemsList, pos, drag.source.modifiers)
            }
            else if (drop.keys[0] === "pasteWidgets")
            {
                frameObj.addWidgetsFromClipboard(dropArea, virtualConsole.clipboardItemsList(), pos);
            }
        }

        keys: [ "vcwidget", "function", "pasteWidgets" ]
    }

    // disable layer
    Rectangle
    {
        y: frameHeader.height
        width: parent.width
        height: parent.height - y
        z: 5
        visible: frameObj ? frameObj.isDisabled : false
        opacity: 0.4
        color: "black"
    }
}
