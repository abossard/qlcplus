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
    property var dropPreview: null

    color: dropActive ? UISettings.activeDropArea : (frameObj ? frameObj.backgroundColor : "darkgray")
    clip: true
    width: frameObj ? (isCollapsed ? UISettings.bigItemHeight * 2 : frameObj.geometry.width) : 100
    height: frameObj ? (isCollapsed ? frameHeader.height : frameObj.geometry.height) : 100

    onFrameObjChanged:
    {
        setCommonProperties(frameObj)
        if (isSolo)
            frameRoot.border.color = "red"
    }

    function dropPosition(drop)
    {
        var hotspot = drop.source.wObj ? drop.source.dragHotSpot : Qt.point(0, 0)
        return Qt.point((drop.x - hotspot.x) / frameObj.scaleFactor,
                        (drop.y - hotspot.y) / frameObj.scaleFactor)
    }

    function previewDrop(drop)
    {
        if (!frameObj || !drop.source)
            return
        var pos = dropPosition(drop)
        if (drop.source.wObj)
        {
            if (drop.source.draggedIDs.length > 1)
                dropPreview = virtualConsole.previewSelectionDrop(drop.source.draggedIDs, drop.source.wObj.id,
                                                                  frameObj.id, frameObj.currentPage, pos)
            else
            {
                var rect = drop.source.wObj.logicalGeometry
                dropPreview = virtualConsole.previewDrop(drop.source.wObj.id, frameObj.id, frameObj.currentPage,
                                                         Qt.rect(pos.x, pos.y, rect.width, rect.height))
            }
            drop.source.previewAccepted = dropPreview.accepted
            drop.source.previewError = dropPreview.error
        }
        else if (drop.source.widgetType)
        {
            var type = drop.source.widgetType
            if (type === "buttonmatrix" || type === "slidermatrix")
                type = "Frame"
            dropPreview = virtualConsole.previewCreate(frameObj.id, type, pos, frameObj.currentPage)
        }
        else if (drop.keys.indexOf("function") >= 0)
        {
            var modifiers = drop.source.modifiers
            var functionType = (modifiers & Qt.ShiftModifier) ? "Slider" :
                               (modifiers & Qt.ControlModifier) ? "Cue list" : "Button"
            dropPreview = virtualConsole.previewCreate(frameObj.id, functionType, pos, frameObj.currentPage)
        }
        else
            dropPreview = null
    }

    // Frame header
    Rectangle
    {
        id: frameHeader
        width: parent.width
        height: frameObj ? frameObj.contentRect.y * frameObj.scaleFactor : UISettings.listItemHeight
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
                function onGeometryChanged() { gridCanvas.requestPaint() }
                function onShowHeaderChanged() { gridCanvas.requestPaint() }
            }

            onPaint:
            {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)

                if (!visible || !frameObj) return
                var guide = virtualConsole.snapGuide(frameObj.id)
                if (!guide.enabled) return
                var scale = frameObj.scaleFactor
                var stepX = guide.step.width * scale
                var stepY = guide.step.height * scale
                if (stepX < 2 || stepY < 2) return
                var left = guide.bounds.x * scale
                var top = guide.bounds.y * scale
                var right = left + guide.bounds.width * scale
                var bottom = top + guide.bounds.height * scale
                var originX = guide.origin.x * scale
                var originY = guide.origin.y * scale
                var gridMode = frameObj.layoutMode === VCFrame.LayoutGrid
                ctx.strokeStyle = gridMode ? Qt.rgba(0.39, 0.59, 1.0, 0.3) : Qt.rgba(1, 1, 1, 0.5)
                ctx.lineWidth = gridMode ? 1 : 0.5

                for (var x = originX + Math.ceil((left - originX) / stepX) * stepX; x < right; x += stepX)
                {
                    ctx.beginPath()
                    ctx.moveTo(x, top)
                    ctx.lineTo(x, bottom)
                    ctx.stroke()
                }

                for (var y = originY + Math.ceil((top - originY) / stepY) * stepY; y < bottom; y += stepY)
                {
                    ctx.beginPath()
                    ctx.moveTo(left, y)
                    ctx.lineTo(right, y)
                    ctx.stroke()
                }
            }
        }

        Repeater
        {
            model: dropPreview ? dropPreview.geometries : []
            Rectangle
            {
                required property var modelData
                property rect candidate: modelData.rect
                visible: frameRoot.dropActive
                x: candidate.x * (frameObj ? frameObj.scaleFactor : 1)
                y: candidate.y * (frameObj ? frameObj.scaleFactor : 1)
                width: candidate.width * (frameObj ? frameObj.scaleFactor : 1)
                height: candidate.height * (frameObj ? frameObj.scaleFactor : 1)
                color: "transparent"
                border.width: 2
                border.color: dropPreview && dropPreview.accepted ? UISettings.highlight : "crimson"
                z: 100
            }
        }

        Text
        {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 4
            visible: frameRoot.dropActive && dropPreview && !dropPreview.accepted
            text: dropPreview ? dropPreview.error : ""
            color: "white"
            style: Text.Outline
            styleColor: "black"
            wrapMode: Text.Wrap
            z: 101
        }

        onEntered: (drag) =>
        {
            if (!virtualConsole.editMode)
            {
                drag.accepted = false
                return
            }
            frameRoot.previewDrop(drag)
            if (dropPreview && !dropPreview.targetEligible)
            {
                drag.accepted = false
                frameRoot.dropActive = false
                frameRoot.dropPreview = null
                return
            }
            frameRoot.dropActive = true
        }
        onPositionChanged: (drag) => frameRoot.previewDrop(drag)
        onExited:
        {
            frameRoot.dropActive = false
            frameRoot.dropPreview = null
        }
        onDropped: (drop) =>
        {
            if (!virtualConsole.editMode || !frameObj || !drop.source)
                return

            frameRoot.dropActive = false
            frameRoot.dropPreview = null
            var pos = frameRoot.dropPosition(drop)
            var result
            if (drop.keys[0] === "vcwidget")
            {
                if (drop.source.widgetType)
                {
                    if (drop.source.widgetType === "buttonmatrix" || drop.source.widgetType === "slidermatrix")
                        virtualConsole.requestAddMatrixPopup(frameObj, dropArea, drop.source.widgetType, pos)
                    else
                        result = virtualConsole.createWidget(frameObj.id, drop.source.widgetType, pos, frameObj.currentPage)
                }
                else
                {
                    if (drop.source.draggedIDs.length > 1)
                        result = virtualConsole.editSelectionDrop(drop.source.draggedIDs, drop.source.wObj.id,
                                                                 frameObj.id, frameObj.currentPage, pos)
                    else
                    {
                        var rect = drop.source.wObj.logicalGeometry
                        result = virtualConsole.editDrop(drop.source.wObj.id, frameObj.id, frameObj.currentPage,
                                                        Qt.rect(pos.x, pos.y, rect.width, rect.height))
                    }
                }
            }
            else if (drop.keys[0] === "function")
            {
                result = virtualConsole.createFunctions(frameObj.id, drop.source.itemsList, pos,
                                                        drop.source.modifiers, frameObj.currentPage)
            }
            else if (drop.keys[0] === "pasteWidgets")
            {
                frameObj.addWidgetsFromClipboard(dropArea, virtualConsole.clipboardItemsList(), Qt.point(drop.x, drop.y));
            }
            if (result)
            {
                frameRoot.showEditResult(result)
                if (result.accepted)
                    drop.acceptProposedAction()
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
