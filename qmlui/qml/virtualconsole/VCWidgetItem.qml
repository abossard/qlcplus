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
    property bool isSelected: wObj ? virtualConsole.selectedIDs.indexOf(wObj.id) >= 0 : false
    property int handleSize: Math.min(UISettings.iconSizeMedium, Math.min(height / 2.5, width / 2.5))
    property point dragHotSpot: Qt.point(0, 0)
    property var draggedIDs: []
    property bool moving: false
    property bool resizing: false
    property rect resizeIntent: Qt.rect(0, 0, 0, 0)
    property rect previewRect: Qt.rect(0, 0, 0, 0)
    property bool previewAccepted: true
    property string previewError: ""
    property var layoutCandidate:
    {
        if (!wObj)
            return null
        var plan = virtualConsole.layoutPreview
        for (var i = 0; i < plan.length; ++i)
            if (plan[i].id === wObj.id)
                return plan[i]
        return null
    }

    function setCommonProperties(obj)
    {
        wObj = obj
    }

    function setBgImageMargins(m)
    {
        bgImage.anchors.margins = m
    }

    function showEditResult(result)
    {
        if (!result.accepted)
        {
            editError.message = result.error
            editError.open()
        }
    }

    function cancelGesture()
    {
        moving = false
        resizing = false
        virtualConsole.enableFlicking(true)
    }

    function showResizePreview(rect)
    {
        resizeIntent = rect
        var result = virtualConsole.previewGeometry(wObj.id, rect)
        previewAccepted = result.accepted
        previewError = result.error
        var candidate = result.accepted && result.geometries.length ? result.geometries[0].rect : rect
        var scale = wObj.scaleFactor
        var scenePos = wRoot.parent.mapToItem(gestureGhost.parent, candidate.x * scale, candidate.y * scale)
        previewRect = Qt.rect(scenePos.x, scenePos.y, candidate.width * scale, candidate.height * scale)
    }

    Keys.onEscapePressed: (event) =>
    {
        if (moving || resizing)
        {
            cancelGesture()
            event.accepted = true
        }
    }

    onVisibleChanged: if (!visible && (moving || resizing)) cancelGesture()
    Component.onDestruction: if (moving || resizing) virtualConsole.enableFlicking(true)
    Connections
    {
        target: virtualConsole
        function onEditModeChanged()
        {
            if (!virtualConsole.editMode)
                wRoot.cancelGesture()
        }
    }

    Image
    {
        id: bgImage
        anchors.fill: parent
        source: wObj && wObj.backgroundImage !== "" ? "file:///" + wObj.backgroundImage : ""
        sourceSize: Qt.size(width, height)
        fillMode: Image.PreserveAspectFit
    }

    Item
    {
        id: previewViewport
        parent: virtualConsole.selectedPage >= 0 ? virtualConsole.currentPageItem() : null
        width: parent ? parent.width : 0
        height: parent ? parent.height : 0
        clip: true
        z: moving || resizing ? 200 : 180
        visible: wRoot.visible && virtualConsole.editMode && (moving || resizing || layoutCandidate !== null)
    }

    Item
    {
        id: layoutGhost
        parent: previewViewport
        property point position:
        {
            if (!layoutCandidate || !wObj)
                return Qt.point(0, 0)
            var renderedPage = virtualConsole.currentPageItem()
            if (!renderedPage || !renderedPage.contentItem)
                return Qt.point(0, 0)
            return Qt.point(layoutCandidate.pageRect.x * wObj.scaleFactor - renderedPage.contentX,
                            layoutCandidate.pageRect.y * wObj.scaleFactor - renderedPage.contentY)
        }
        x: position.x
        y: position.y
        width: layoutCandidate && wObj ? layoutCandidate.pageRect.width * wObj.scaleFactor : 0
        height: layoutCandidate && wObj ? layoutCandidate.pageRect.height * wObj.scaleFactor : 0
        visible: layoutCandidate && wRoot.visible && virtualConsole.editMode &&
                 layoutCandidate.pageIndex === virtualConsole.selectedPage
        z: 180
        Rectangle
        {
            anchors.fill: parent
            color: wObj ? wObj.backgroundColor : UISettings.bgStrong
            opacity: 0.65
            border.color: UISettings.highlight
            border.width: 2
            Text
            {
                anchors.fill: parent
                anchors.margins: 4
                text: wObj ? wObj.caption : ""
                color: UISettings.fgMain
                font.family: UISettings.robotoFontName
                font.pixelSize: UISettings.textSizeDefault
                elide: Text.ElideRight
                wrapMode: Text.Wrap
            }
        }
    }

    Item
    {
        id: gestureGhost
        parent: previewViewport
        x: wRoot.previewRect.x
        y: wRoot.previewRect.y
        width: wRoot.previewRect.width
        height: wRoot.previewRect.height
        z: 200
        visible: wRoot.moving || wRoot.resizing

        Drag.active: wRoot.moving
        Drag.source: wRoot
        Drag.keys: [ "vcwidget" ]
        Drag.hotSpot: wRoot.dragHotSpot
        Drag.onActiveChanged: UISettings.internalDragActive = Drag.active

        Rectangle
        {
            anchors.fill: parent
            color: wRoot.wObj ? wRoot.wObj.backgroundColor : UISettings.bgStrong
            opacity: 0.7
            border.width: 2
            border.color: wRoot.previewAccepted ? UISettings.highlight : "crimson"

            Text
            {
                anchors.fill: parent
                anchors.margins: 4
                text: wRoot.wObj ? wRoot.wObj.caption : ""
                color: UISettings.fgMain
                font.family: UISettings.robotoFontName
                font.pixelSize: UISettings.textSizeDefault
                wrapMode: Text.Wrap
                elide: Text.ElideRight
            }
        }
    }

    Rectangle
    {
        id: resizeLayer
        anchors.fill: parent
        color: "transparent"
        border.width: isSelected ? 3 : 2
        border.color: isSelected ? "yellow" : UISettings.bgLight
        z: isSelected ? 99 : 1
        visible: virtualConsole && virtualConsole.editMode && wObj && wObj.allowResize

        MouseArea
        {
            id: dragMouseArea
            anchors.fill: parent
            preventStealing: true

            property point pressedScene
            property bool gestureEnabled: false
            property bool startedMoving: false
            property int pressedModifiers: 0

            onPressed: (mouse) =>
            {
                wRoot.forceActiveFocus()
                var multi = (mouse.modifiers & (Qt.ControlModifier | Qt.MetaModifier)) !== 0
                if (!isSelected || multi)
                    virtualConsole.setWidgetSelection(wObj.id, wRoot, multi ? !isSelected : true, multi)
                dragHotSpot = Qt.point(mouse.x, mouse.y)
                pressedScene = mapToItem(gestureGhost.parent, mouse.x, mouse.y)
                gestureEnabled = true
                startedMoving = false
                pressedModifiers = mouse.modifiers
                virtualConsole.enableFlicking(false)
            }

            onPositionChanged: (mouse) =>
            {
                if (!pressed || !gestureEnabled)
                    return
                var scenePos = mapToItem(gestureGhost.parent, mouse.x, mouse.y)
                if (!moving && Math.abs(scenePos.x - pressedScene.x) + Math.abs(scenePos.y - pressedScene.y) < 10)
                    return
                if (!moving)
                {
                    virtualConsole.setWidgetSelection(wObj.id, wRoot, true, true)
                    draggedIDs = virtualConsole.selectedIDs
                }
                previewAccepted = true
                previewError = ""
                previewRect = Qt.rect(scenePos.x - dragHotSpot.x, scenePos.y - dragHotSpot.y, wRoot.width, wRoot.height)
                startedMoving = true
                moving = true
            }

            onReleased:
            {
                if (moving)
                    gestureGhost.Drag.drop()
                else if (gestureEnabled && !startedMoving && !pressedModifiers)
                    virtualConsole.setWidgetSelection(wObj.id, wRoot, true, false)
                gestureEnabled = false
                cancelGesture()
            }
            onCanceled:
            {
                gestureEnabled = false
                cancelGesture()
            }

            Connections
            {
                target: wRoot
                function onMovingChanged()
                {
                    if (!wRoot.moving)
                        dragMouseArea.gestureEnabled = false
                }
            }
        }

        Repeater
        {
            model: 4

            Image
            {
                id: corner
                required property int index
                property bool leftEdge: index === 0 || index === 3
                property bool topEdge: index < 2
                width: handleSize
                height: width
                x: leftEdge ? 0 : resizeLayer.width - width
                y: topEdge ? 0 : resizeLayer.height - height
                rotation: [180, 270, 0, 90][index]
                source: "qrc:/arrow-corner.svg"
                sourceSize: Qt.size(handleSize, handleSize)
                visible: isSelected && wObj && wObj.allowResize

                MouseArea
                {
                    anchors.fill: parent
                    cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                    preventStealing: true
                    property point startPoint
                    property rect startRect

                    onPressed: (mouse) =>
                    {
                        wRoot.forceActiveFocus()
                        startPoint = mapToItem(wRoot.parent, mouse.x, mouse.y)
                        startRect = wObj.logicalGeometry
                        resizing = true
                        virtualConsole.enableFlicking(false)
                        showResizePreview(startRect)
                    }

                    onPositionChanged: (mouse) =>
                    {
                        if (!pressed || !resizing)
                            return
                        var p = mapToItem(wRoot.parent, mouse.x, mouse.y)
                        var dx = (p.x - startPoint.x) / wObj.scaleFactor
                        var dy = (p.y - startPoint.y) / wObj.scaleFactor
                        showResizePreview(Qt.rect(startRect.x + (corner.leftEdge ? dx : 0),
                                                  startRect.y + (corner.topEdge ? dy : 0),
                                                  startRect.width + (corner.leftEdge ? -dx : dx),
                                                  startRect.height + (corner.topEdge ? -dy : dy)))
                    }

                    onReleased:
                    {
                        if (resizing)
                            showEditResult(virtualConsole.editGeometry(wObj.id, resizeIntent))
                        cancelGesture()
                    }
                    onCanceled: cancelGesture()
                }
            }
        }
    }

    CustomPopupDialog
    {
        id: editError
        objectName: "vcEditError"
        title: qsTr("Cannot edit control")
    }
}
