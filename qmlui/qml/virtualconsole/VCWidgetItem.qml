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
    property bool containerHandles: false
    property color frameBackgroundColor: wObj ? wObj.backgroundColor : "darkgray"
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

    function canvasViewport()
    {
        var item = wRoot.parent
        while (item)
        {
            if (item.contentItem !== undefined && item.contentX !== undefined)
                return item
            item = item.parent
        }
        return null
    }

    function frameChromeParent()
    {
        var viewport = canvasViewport()
        return viewport ? viewport.contentItem : null
    }

    function frameGeometryScope()
    {
        var page = frameChromeParent(), item = wRoot, path = []
        while (page && item && item !== page) {
            if (item !== wRoot && (!item.visible || item.isCollapsed))
                return []
            path.push({ item: item, x: item.x, y: item.y, width: item.width, height: item.height,
                        scale: item.scale, rotation: item.rotation, opacity: item.opacity,
                        clip: item.clip, collapsed: item !== wRoot && item.isCollapsed,
                        visible: item.visible })
            item = item.parent
        }
        return item ? path : []
    }

    function frameChromeObstacles(path)
    {
        var page = frameChromeParent(), obstacles = []
        function add(item, rect, viewportControl) {
            var transforms = []
            for (var node = item; node && node !== page; node = node.parent)
                transforms.push({ item: node, x: node.x, y: node.y, width: node.width, height: node.height,
                                  rotation: node.rotation, scale: node.scale })
            var bounds = item.mapToItem(wRoot, rect.x, rect.y, rect.width, rect.height)
            for (var parent = item.parent; parent && parent !== page; parent = parent.parent) {
                if (parent.clip) {
                    var clip = parent.mapToItem(wRoot, 0, 0, parent.width, parent.height)
                    var x = Math.max(bounds.x, clip.x), y = Math.max(bounds.y, clip.y)
                    var right = Math.min(bounds.x + bounds.width, clip.x + clip.width)
                    var bottom = Math.min(bounds.y + bounds.height, clip.y + clip.height)
                    bounds = Qt.rect(x, y, Math.max(0, right - x), Math.max(0, bottom - y))
                }
            }
            if (bounds.width > 0 && bounds.height > 0)
                obstacles.push({ rect: bounds, transforms: transforms, viewportControl: viewportControl === true })
        }
        function header(item) {
            if (!item || !item.visible)
                return
            if (item.enabled && ((item.acceptedButtons !== undefined && item.acceptedButtons) ||
                                 (item.pressed !== undefined && item.checkable !== undefined) ||
                                 item.currentIndex !== undefined))
                add(item, Qt.rect(0, 0, item.width, item.height))
            if (item.paintedWidth !== undefined && item.text) {
                var width = Math.min(item.width, item.paintedWidth)
                var height = Math.min(item.height, item.paintedHeight)
                var x = item.horizontalAlignment === Text.AlignRight ? item.width - width :
                        item.horizontalAlignment === Text.AlignHCenter ? (item.width - width) / 2 : 0
                var y = item.verticalAlignment === Text.AlignBottom ? item.height - height :
                        item.verticalAlignment === Text.AlignVCenter ? (item.height - height) / 2 : 0
                add(item, Qt.rect(x, y, width, height))
            }
            for (var i = 0; i < item.children.length; ++i)
                header(item.children[i])
        }
        function widget(item) {
            if (!item || !item.wObj || !item.visible)
                return
            var ancestor = false
            for (var parent = wRoot; parent; parent = parent.parent)
                if (parent === item)
                    ancestor = true
            if (!item.containerHandles || (!ancestor && !item.pageFrame)) {
                add(item, Qt.rect(0, 0, item.width, item.height))
                return
            }
            header(item.frameHeaderItem)
            for (var i = 0; i < item.chromeWidgets.length; ++i)
                widget(item.chromeWidgets[i])
        }
        var root = null
        for (var i = 0; i < path.length; ++i)
            if (path[i].item.pageFrame)
                root = path[i].item
        widget(root)
        var viewport = canvasViewport()
        for (var i = 0; viewport && i < viewport.children.length; ++i) {
            var control = viewport.children[i]
            if (control && control.visible && control.enabled && control.orientation !== undefined &&
                control.position !== undefined && control.size !== undefined && control.pressed !== undefined)
                add(control, Qt.rect(0, 0, control.width, control.height), true)
        }
        return obstacles
    }

    function frameCornerPlacement(left, top, size, path, obstacles)
    {
        var original = Qt.rect(left ? -size : wRoot.width, top ? -size : wRoot.height, size, size)
        var viewport = canvasViewport(), bounds = null
        for (var p = 0; p < path.length; ++p)
            if (path[p].item.pageFrame)
                bounds = path[p].item.mapToItem(wRoot, 0, 0, path[p].width, path[p].height)
        if (!viewport || !bounds)
            return { rect: original, error: "" }
        bounds = Qt.rect(bounds.x - viewport.leftMargin, bounds.y - viewport.topMargin,
                         bounds.width + viewport.leftMargin + viewport.rightMargin,
                         bounds.height + viewport.topMargin + viewport.bottomMargin)
        var gap = 2
        function clear(rect, clearance) {
            if (rect.x < bounds.x || rect.y < bounds.y ||
                rect.x + size > bounds.x + bounds.width || rect.y + size > bounds.y + bounds.height)
                return false
            for (var i = 0; i < obstacles.length; ++i) {
                var obstacle = obstacles[i].rect
                if (rect.x < obstacle.x + obstacle.width + clearance && rect.x + size + clearance > obstacle.x &&
                    rect.y < obstacle.y + obstacle.height + clearance && rect.y + size + clearance > obstacle.y)
                    return false
            }
            return true
        }
        if (clear(original, 0))
            return { rect: original, error: "" }
        var radius = Math.max(size * 4, Math.min(wRoot.width, wRoot.height) / 2)
        var visibleBounds = viewport.contentItem.mapToItem(wRoot, viewport.contentX, viewport.contentY,
                                                          viewport.width, viewport.height)
        var xs = [original.x], ys = [original.y]
        for (var i = 0; i < obstacles.length; ++i) {
            var obstacle = obstacles[i].rect
            var x = left ? obstacle.x - size - gap : obstacle.x + obstacle.width + gap
            var y = top ? obstacle.y - size - gap : obstacle.y + obstacle.height + gap
            if ((left ? x <= original.x : x >= original.x) && Math.abs(x - original.x) <= radius && xs.indexOf(x) < 0)
                xs.push(x)
            if ((top ? y <= original.y : y >= original.y) && Math.abs(y - original.y) <= radius && ys.indexOf(y) < 0)
                ys.push(y)
            if (obstacles[i].viewportControl && original.x < obstacle.x + obstacle.width &&
                original.x + size > obstacle.x && original.y < obstacle.y + obstacle.height &&
                original.y + size > obstacle.y) {
                var slideX = left ? obstacle.x + obstacle.width + gap : obstacle.x - size - gap
                var slideY = top ? obstacle.y + obstacle.height + gap : obstacle.y - size - gap
                if (Math.abs(slideX - original.x) <= radius && xs.indexOf(slideX) < 0)
                    xs.push(slideX)
                if (Math.abs(slideY - original.y) <= radius && ys.indexOf(slideY) < 0)
                    ys.push(slideY)
            }
        }
        xs.sort(function(a, b) { return Math.abs(a - original.x) - Math.abs(b - original.x) })
        ys.sort(function(a, b) { return Math.abs(a - original.y) - Math.abs(b - original.y) })
        var best = null, distance = radius * radius, horizontal = radius, bestVisible = false
        for (var y = 0; y < Math.min(8, ys.length); ++y)
            for (var x = 0; x < Math.min(8, xs.length); ++x) {
                var dx = xs[x] - original.x, dy = ys[y] - original.y
                var candidate = Qt.rect(xs[x], ys[y], size, size), d = dx * dx + dy * dy
                var exterior = (left ? candidate.x + size <= 0 : candidate.x >= wRoot.width) ||
                               (top ? candidate.y + size <= 0 : candidate.y >= wRoot.height)
                var visible = candidate.x >= visibleBounds.x && candidate.y >= visibleBounds.y &&
                              candidate.x + size <= visibleBounds.x + visibleBounds.width &&
                              candidate.y + size <= visibleBounds.y + visibleBounds.height
                if (exterior && d <= radius * radius && clear(candidate, gap) &&
                    (!best || (visible && !bestVisible) || (visible === bestVisible &&
                     (d < distance - 0.000001 ||
                      (Math.abs(d - distance) <= 0.000001 && Math.abs(dx) < horizontal))))) {
                    best = candidate
                    distance = d
                    horizontal = Math.abs(dx)
                    bestVisible = visible
                }
            }
        return { rect: best || original,
                 error: best ? "" : qsTr("No clear resize handle position within the bounded editing area.") }
    }

    function frameCornerFragments(rect, path)
    {
        if (!path.length)
            return []
        var pieces = [{ region: rect, parent: wRoot.parent, level: 1 }]
        for (var i = 1; i < path.length; ++i) {
            var node = path[i]
            if (!node.item || !node.visible || node.collapsed)
                return []
            if (!node.clip)
                continue
            var bounds = node.item.mapToItem(wRoot, 0, 0, node.width, node.height)
            var next = []
            for (var p = 0; p < pieces.length; ++p) {
                var piece = pieces[p], rect = piece.region
                var x = Math.max(rect.x, bounds.x), y = Math.max(rect.y, bounds.y)
                var right = Math.min(rect.x + rect.width, bounds.x + bounds.width)
                var bottom = Math.min(rect.y + rect.height, bounds.y + bounds.height)
                if (piece.level > i) {
                    next.push(piece)
                    continue
                }
                if (right <= x || bottom <= y) {
                    next.push({ region: rect, parent: node.item.parent, level: i + 1 })
                    continue
                }
                next.push({ region: Qt.rect(x, y, right - x, bottom - y),
                            parent: piece.parent, level: piece.level })
                var escaped = [Qt.rect(rect.x, rect.y, rect.width, y - rect.y),
                               Qt.rect(rect.x, bottom, rect.width, rect.y + rect.height - bottom),
                               Qt.rect(rect.x, y, x - rect.x, bottom - y),
                               Qt.rect(right, y, rect.x + rect.width - right, bottom - y)]
                for (var e = 0; e < escaped.length; ++e)
                    if (escaped[e].width > 0 && escaped[e].height > 0)
                        next.push({ region: escaped[e], parent: node.item.parent, level: i + 1 })
            }
            pieces = next
        }
        for (var f = 0; f < pieces.length; ++f) {
            var alpha = 1
            for (var a = 0; a < pieces[f].level; ++a)
                alpha *= path[a].opacity
            pieces[f].opacity = alpha
        }
        return pieces
    }

    function frameCornerZ(target)
    {
        var floor = 0
        for (var s = 0; target && s < target.children.length; ++s) {
            var sibling = target.children[s]
            if (sibling && sibling.wObj)
                floor = Math.min(floor, sibling.z)
        }
        return floor - 1
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
        visible: !wRoot.containerHandles || !virtualConsole.editMode || !wRoot.pageFrame
        source: wObj && wObj.backgroundImage !== "" ? "file:///" + wObj.backgroundImage : ""
        sourceSize: Qt.size(width, height)
        fillMode: Image.PreserveAspectFit
    }

    Item
    {
        id: previewViewport
        objectName: wObj ? "vcGestureViewport-" + wObj.id : "vcGestureViewport"
        parent: wRoot.canvasViewport()
        width: parent ? parent.width : 0
        height: parent ? parent.height : 0
        clip: true
        z: moving || resizing ? 200 : 180
        visible: wRoot.visible && virtualConsole.editMode && (moving || resizing || layoutCandidate !== null)
    }

    Item
    {
        id: frameChrome
        objectName: wObj ? "vcFrameChrome-" + wObj.id : "vcFrameChrome"
        property var geometryScope: wRoot.containerHandles ? wRoot.frameGeometryScope() : []
        property var obstacles: wRoot.containerHandles && wRoot.isSelected && virtualConsole.editMode ?
            wRoot.frameChromeObstacles(geometryScope) : []
        visible: wRoot.containerHandles && wRoot.visible && virtualConsole.editMode &&
                 wRoot.isSelected && geometryScope.length > 0
    }

    Item
    {
        id: frameBackdrop
        objectName: wObj ? "vcFrameBackdrop-" + wObj.id : "vcFrameBackdrop"
        parent: wRoot.containerHandles && wRoot.pageFrame ? wRoot.frameChromeParent() : null
        property var bounds: parent ? wRoot.mapToItem(parent, 0, 0, wRoot.width, wRoot.height) : null
        x: bounds ? bounds.x : 0
        y: bounds ? bounds.y : 0
        width: bounds ? bounds.width : 0
        height: bounds ? bounds.height : 0
        clip: true
        z: -2.5
        visible: wRoot.containerHandles && wRoot.pageFrame && wRoot.visible &&
                 virtualConsole.editMode && bounds !== null && width > 0 && height > 0

        Rectangle
        {
            id: backdropSurface
            width: wRoot.width
            height: wRoot.height
            color: wRoot.pageFrame && !wRoot.dropActive ? "transparent" : wRoot.frameBackgroundColor
            radius: wRoot.radius
            Image
            {
                anchors.fill: parent
                anchors.margins: bgImage.anchors.margins
                source: bgImage.source
                sourceSize: Qt.size(width, height)
                fillMode: bgImage.fillMode
            }
        }
    }

    Item
    {
        id: layoutGhost
        parent: previewViewport
        property point position:
        {
            if (!layoutCandidate || !wObj)
                return Qt.point(0, 0)
            var renderedPage = previewViewport.parent
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
        z: isSelected && !containerHandles ? 99 : 1
        visible: virtualConsole && virtualConsole.editMode && wObj && wObj.allowResize

        MouseArea
        {
            id: dragMouseArea
            parent: resizeLayer
            anchors.fill: parent
            visible: virtualConsole.editMode && wRoot.wObj && wRoot.wObj.allowResize
            preventStealing: true
            acceptedButtons: Qt.LeftButton | Qt.RightButton

            property point pressedScene
            property bool gestureEnabled: false
            property bool startedMoving: false
            property int pressedModifiers: 0

            onPressed: (mouse) =>
            {
                wRoot.forceActiveFocus()
                if (mouse.button === Qt.RightButton)
                {
                    // the menu acts on the selection, so a right click on another widget selects it
                    if (!wRoot.isSelected)
                        virtualConsole.setWidgetSelection(wRoot.wObj.id, wRoot, true, false)
                    vcEditor.widgetMenuRequested(mapToItem(null, mouse.x, mouse.y))
                    return
                }
                var multi = (mouse.modifiers & (Qt.ControlModifier | Qt.MetaModifier)) !== 0
                if (!wRoot.isSelected || multi)
                    virtualConsole.setWidgetSelection(wRoot.wObj.id, wRoot, multi ? !wRoot.isSelected : true, multi)
                wRoot.dragHotSpot = Qt.point(mouse.x, mouse.y)
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
                if (!wRoot.moving && Math.abs(scenePos.x - pressedScene.x) + Math.abs(scenePos.y - pressedScene.y) < 10)
                    return
                if (!wRoot.moving)
                {
                    virtualConsole.setWidgetSelection(wRoot.wObj.id, wRoot, true, true)
                    wRoot.draggedIDs = virtualConsole.selectedIDs
                }
                wRoot.previewAccepted = true
                wRoot.previewError = ""
                wRoot.previewRect = Qt.rect(scenePos.x - wRoot.dragHotSpot.x, scenePos.y - wRoot.dragHotSpot.y, wRoot.width, wRoot.height)
                startedMoving = true
                wRoot.moving = true
            }

            onReleased:
            {
                if (wRoot.moving)
                    gestureGhost.Drag.drop()
                else if (gestureEnabled && !startedMoving && !pressedModifiers)
                    virtualConsole.setWidgetSelection(wRoot.wObj.id, wRoot, true, false)
                gestureEnabled = false
                wRoot.cancelGesture()
            }
            onCanceled:
            {
                gestureEnabled = false
                wRoot.cancelGesture()
            }
            onDoubleClicked: (mouse) =>
            {
                if (mouse.button !== Qt.LeftButton)
                    return
                virtualConsole.setWidgetSelection(wRoot.wObj.id, wRoot, true, false)
                vcEditor.widgetPropertiesRequested()
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
            parent: wRoot.containerHandles ? frameChrome : resizeLayer
            model: 4

            Item
            {
                id: cornerOwner
                required property int index
                objectName: wRoot.wObj ? "vcResizeCorner-" + wRoot.wObj.id + "-" + index : ""
                property bool leftSide: index === 0 || index === 3
                property bool topSide: index < 2
                property var heldPlacement: null
                property bool cornerReady: false
                property string lastClearanceError: ""
                property var placement: heldPlacement || (wRoot.containerHandles ?
                    wRoot.frameCornerPlacement(leftSide, topSide, wRoot.handleSize,
                                               frameChrome.geometryScope, frameChrome.obstacles) : null)
                property real cornerX: wRoot.containerHandles ? placement.rect.x :
                    (leftSide ? 0 : resizeLayer.width - wRoot.handleSize)
                property real cornerY: wRoot.containerHandles ? placement.rect.y :
                    (topSide ? 0 : resizeLayer.height - wRoot.handleSize)
                property var fragments: wRoot.containerHandles ?
                    (placement.error ? [] : wRoot.frameCornerFragments(placement.rect, frameChrome.geometryScope)) :
                    [{ region: Qt.rect(cornerX, cornerY, wRoot.handleSize, wRoot.handleSize),
                       parent: resizeLayer, opacity: 1 }]
                property point startPoint
                property rect startRect
                Component.onCompleted: { cornerReady = true; reportClearance() }
                onPlacementChanged: reportClearance()

                function reportClearance()
                {
                    if (!cornerReady || !wRoot.isSelected || !frameChrome.visible)
                        return
                    var error = placement ? placement.error : ""
                    if (error && error !== lastClearanceError)
                        wRoot.showEditResult({ accepted: false, error: error })
                    lastClearanceError = error
                }

                function begin(area, mouse)
                {
                    if (wRoot.resizing)
                        return
                    if (placement && placement.error) {
                        wRoot.showEditResult({ accepted: false, error: placement.error })
                        return
                    }
                    heldPlacement = placement
                    wRoot.forceActiveFocus()
                    startPoint = area.mapToItem(wRoot.parent, mouse.x, mouse.y)
                    startRect = wRoot.wObj.logicalGeometry
                    wRoot.resizing = true
                    virtualConsole.enableFlicking(false)
                    wRoot.showResizePreview(startRect)
                }

                function move(area, mouse)
                {
                    if (!wRoot.resizing)
                        return
                    var p = area.mapToItem(wRoot.parent, mouse.x, mouse.y)
                    var dx = (p.x - startPoint.x) / wRoot.wObj.scaleFactor
                    var dy = (p.y - startPoint.y) / wRoot.wObj.scaleFactor
                    wRoot.showResizePreview(Qt.rect(startRect.x + (leftSide ? dx : 0),
                                                   startRect.y + (topSide ? dy : 0),
                                                   startRect.width + (leftSide ? -dx : dx),
                                                   startRect.height + (topSide ? -dy : dy)))
                }

                function finish()
                {
                    if (wRoot.resizing)
                        wRoot.showEditResult(virtualConsole.editGeometry(wRoot.wObj.id, wRoot.resizeIntent))
                    wRoot.cancelGesture()
                }

                Connections
                {
                    target: wRoot
                    function onResizingChanged() { if (!wRoot.resizing) cornerOwner.heldPlacement = null }
                }

                Repeater
                {
                    model: cornerOwner.fragments.length
                    onItemRemoved: (index, item) => { item.projectedFragment.parent = null }
                    Item
                    {
                        id: fragmentOwner
                        required property int index
                        property var modelData: cornerOwner.fragments[index]
                        property alias projectedFragment: fragment
                        Item
                        {
                            id: fragment
                            property int index: fragmentOwner.index
                            property var modelData: fragmentOwner.modelData
                            objectName: wRoot.wObj ? "vcResizeFragment-" + wRoot.wObj.id + "-" + cornerOwner.index + "-" + index : ""
                            // Set the visual parent after declarative ownership is established.
                            Component.onCompleted: parent = Qt.binding(function() { return modelData.parent })
                            property point position: wRoot.containerHandles ?
                                wRoot.mapToItem(parent, modelData.region.x, modelData.region.y) :
                                Qt.point(modelData.region.x, modelData.region.y)
                            x: position.x
                            y: position.y
                            width: modelData.region.width
                            height: modelData.region.height
                            clip: true
                            z: wRoot.containerHandles ? wRoot.frameCornerZ(parent) : 0
                            opacity: modelData.opacity
                            visible: wRoot.isSelected && wRoot.wObj && wRoot.wObj.allowResize &&
                                     (!wRoot.containerHandles || frameChrome.visible)

                            Image
                            {
                                objectName: wRoot.wObj && fragment.index === 0 ?
                                    "vcResizeHandle-" + wRoot.wObj.id + "-" + cornerOwner.index : ""
                                property int index: cornerOwner.index
                                property bool leftEdge: cornerOwner.leftSide
                                property bool topEdge: cornerOwner.topSide
                                x: cornerOwner.cornerX - fragment.modelData.region.x
                                y: cornerOwner.cornerY - fragment.modelData.region.y
                                width: wRoot.handleSize
                                height: width
                                rotation: [180, 270, 0, 90][cornerOwner.index]
                                source: "qrc:/arrow-corner.svg"
                                sourceSize: Qt.size(width, height)
                            }

                            MouseArea
                            {
                                anchors.fill: parent
                                cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                                preventStealing: true
                                onPressed: (mouse) => cornerOwner.begin(this, mouse)
                                onPositionChanged: (mouse) => { if (pressed) cornerOwner.move(this, mouse) }
                                onReleased: cornerOwner.finish()
                                onCanceled: wRoot.cancelGesture()
                            }
                        }
                    }
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
