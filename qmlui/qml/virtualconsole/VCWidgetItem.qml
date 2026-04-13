/*
  Q Light Controller Plus
  VCWidgetItem.qml

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

import org.qlcplus.classes 1.0
import "."

Rectangle
{
    id: wRoot
    x: wObj ? wObj.geometry.x : 0
    y: wObj ? wObj.geometry.y : 0
    width: wObj ? wObj.geometry.width : 100
    height: wObj ? wObj.geometry.height : 100
    color: wObj ? wObj.backgroundColor : "darkgray"
    border.width: 2
    border.color: UISettings.bgLight
    z: wObj ? wObj.zIndex : 0
    visible: wObj ? wObj.isVisible : true

    property VCWidget wObj: null
    property bool isSelected: false
    property int handleSize: Math.min(UISettings.iconSizeMedium, Math.min(height / 2, width / 2))

    Drag.source: wRoot
    Drag.keys: [ "vcwidget" ]
    Drag.active: dragMouseArea.drag.active

    onIsSelectedChanged:
    {
        if (wObj)
            wObj.isEditing = isSelected
    }

    function setCommonProperties(obj)
    {
        if (obj === null)
            return;

        wObj = obj
        if (wObj.isEditing)
        {
            isSelected = true
            virtualConsole.setWidgetSelection(wObj.id, wRoot, isSelected, true)
        }
    }

    function setBgImageMargins(m)
    {
        bgImage.anchors.margins = m
    }

    function snapToGrid()
    {
        if (virtualConsole.snapping)
        {
            var s = virtualConsole.snappingSize
            if (s > 0)
            {
                x = Math.round(x / s) * s
                y = Math.round(y / s) * s
                width = Math.max(Math.round(width / s) * s, s)
                height = Math.max(Math.round(height / s) * s, s)
            }
        }
    }

    function updateGeometry(d)
    {
        d.target = null

        snapToGrid()

        if (height < UISettings.iconSizeMedium)
            height = UISettings.iconSizeMedium
        if (width < UISettings.iconSizeMedium)
            width = UISettings.iconSizeMedium

        wObj.geometry = Qt.rect(x, y, width, height)

        x = Qt.binding(function() { return wObj ? wObj.geometry.x : 0 })
        y = Qt.binding(function() { return wObj ? wObj.geometry.y : 0 })
        width = Qt.binding(function() { return wObj ? wObj.geometry.width : 100 })
        height = Qt.binding(function() { return wObj ? wObj.geometry.height : 100 })
    }

    Image
    {
        id: bgImage
        anchors.fill: parent
        source: wObj && wObj.backgroundImage !== "" ? "file:///" + wObj.backgroundImage : ""
        sourceSize: Qt.size(width, height)
        fillMode: Image.PreserveAspectFit
    }

    // resize area
    Rectangle
    {
        id: resizeLayer
        anchors.fill: parent
        color: "transparent"
        border.width: isSelected ? 3 : 2
        border.color: isSelected ? "yellow" : UISettings.bgLight
        // this must be above the widget root but
        // underneath the widget children (if any)
        z: isSelected ? 99 : 1
        visible: virtualConsole && virtualConsole.editMode && wObj && wObj.allowResize

        // mouse area to select and move the widget
        MouseArea
        {
            id: dragMouseArea
            anchors.fill: parent
            drag.threshold: 10
            preventStealing: true

            property bool dragRemapped: false
            property bool flickingDisabled: false

            onPressed: (mouse) =>
            {
                if (virtualConsole.editMode)
                {
                    isSelected = !isSelected
                    virtualConsole.enableFlicking(false)
                    flickingDisabled = true
                    virtualConsole.setWidgetSelection(wObj.id, wRoot, isSelected, mouse.modifiers & Qt.ControlModifier)
                    drag.target = wRoot
                }

                dragRemapped = false
            }

            onPositionChanged: (mouse) =>
            {
                if (drag.active && drag.target !== null && dragRemapped == false)
                {
                    var remappedPos = wRoot.mapToItem(virtualConsole.currentPageItem(), 0, 0);
                    wObj.geometry = Qt.rect(remappedPos.x, remappedPos.y, wRoot.width, wRoot.height)
                    wRoot.parent = virtualConsole.currentPageItem()
                    dragRemapped = true
                    if (isSelected == false)
                    {
                        isSelected = true
                        virtualConsole.setWidgetSelection(wObj.id, wRoot, isSelected, mouse.modifiers & Qt.ControlModifier)
                    }
                }

                if (drag.active && virtualConsole.snapping)
                {
                    var s = virtualConsole.snappingSize
                    if (s > 0)
                    {
                        wRoot.x = Math.round(wRoot.x / s) * s
                        wRoot.y = Math.round(wRoot.y / s) * s
                    }
                }
            }

            onReleased: (mouse) =>
            {
                if (drag.active && drag.target !== null)
                {
                    // A drag/drop sequence is always performed within a parent frame,
                    // so the new geometry will be calculated by virtualConsole.moveWidget,
                    // invoked by VCFrameItem DropArea
                    wRoot.Drag.drop()
                    drag.target = null
                    dragRemapped = false
                }
                if (flickingDisabled)
                {
                    virtualConsole.enableFlicking(true)
                    flickingDisabled = false
                }

                // Restore bindings broken by the drag mechanism
                // (drag.target directly sets x/y, breaking declarative bindings)
                x = Qt.binding(function() { return wObj ? wObj.geometry.x : 0 })
                y = Qt.binding(function() { return wObj ? wObj.geometry.y : 0 })
                width = Qt.binding(function() { return wObj ? wObj.geometry.width : 100 })
                height = Qt.binding(function() { return wObj ? wObj.geometry.height : 100 })
            }
        }

        // top-left corner
        Image
        {
            id: tlHandle
            rotation: 180
            width: handleSize
            height: width
            source: "qrc:/arrow-corner.svg"
            sourceSize: Qt.size(handleSize, handleSize)
            visible: isSelected && wObj && wObj.allowResize

            MouseArea
            {
                anchors.fill: parent
                cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor

                onPressed:
                {
                    drag.target = tlHandle
                    drag.threshold = 0
                }
                onPositionChanged:
                {
                    if (drag.target == null)
                        return;
                    drag.maximumX = wRoot.width - handleSize
                    drag.maximumY = wRoot.height - handleSize
                    wRoot.x += tlHandle.x
                    wRoot.width -= tlHandle.x
                    wRoot.height -= tlHandle.y
                    wRoot.y += tlHandle.y
                    wRoot.snapToGrid()
                }
                onReleased: wRoot.updateGeometry(drag)
            }
        }
        // top-right corner
        Image
        {
            id: trHandle
            width: handleSize
            height: width
            x: parent.width - handleSize
            rotation: 270
            source: "qrc:/arrow-corner.svg"
            sourceSize: Qt.size(handleSize, handleSize)
            visible: isSelected && wObj && wObj.allowResize

            MouseArea
            {
                anchors.fill: parent
                cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor

                onPressed:
                {
                    drag.target = trHandle
                    drag.threshold = 0
                    drag.minimumX = handleSize
                }
                onPositionChanged:
                {
                    if (drag.target == null)
                        return;
                    drag.maximumY = wRoot.height - handleSize
                    wRoot.width = trHandle.x + trHandle.width
                    wRoot.height -= trHandle.y
                    wRoot.y += trHandle.y
                    wRoot.snapToGrid()
                }
                onReleased: wRoot.updateGeometry(drag)
            }
        }
        // bottom-right corner
        Image
        {
            id: brHandle
            width: handleSize
            height: width
            x: parent.width - handleSize
            y: parent.height - handleSize
            source: "qrc:/arrow-corner.svg"
            sourceSize: Qt.size(handleSize, handleSize)
            visible: isSelected && wObj && wObj.allowResize

            MouseArea
            {
                anchors.fill: parent
                cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                onPressed:
                {
                    drag.target = brHandle
                    drag.threshold = 0
                    drag.minimumX = handleSize
                    drag.minimumY = handleSize
                }
                onPositionChanged:
                {
                    if (drag.target == null)
                        return;
                    wRoot.width = brHandle.x + brHandle.width
                    wRoot.height = brHandle.y + brHandle.height
                    wRoot.snapToGrid()
                }
                onReleased: wRoot.updateGeometry(drag)
            }
        }
        // bottom-left corner
        Image
        {
            id: blHandle
            rotation: 90
            width: handleSize
            height: width
            y: parent.height - handleSize
            source: "qrc:/arrow-corner.svg"
            sourceSize: Qt.size(handleSize, handleSize)
            visible: isSelected && wObj && wObj.allowResize

            MouseArea
            {
                anchors.fill: parent
                cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor

                onPressed:
                {
                    drag.target = blHandle
                    drag.threshold = 0
                    drag.minimumY = handleSize
                }
                onPositionChanged:
                {
                    if (drag.target == null)
                        return;
                    drag.maximumX = wRoot.width - handleSize
                    wRoot.height = blHandle.y + blHandle.height
                    wRoot.x += blHandle.x
                    wRoot.width -= blHandle.x
                    wRoot.snapToGrid()
                }
                onReleased: wRoot.updateGeometry(drag)
            }
        }
    }
}
