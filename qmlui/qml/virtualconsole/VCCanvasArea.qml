/*
  Q Light Controller Plus
  VCCanvasArea.qml

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

import "."

/* The empty page under the widgets: a click clears the selection, dragging pans
   the page, and in edit mode Shift+drag selects the widgets a band touches */
MouseArea
{
    id: canvasArea
    z: -3
    anchors.fill: parent

    /** Index of the console page shown */
    property int pageIndex: -1
    property bool banding: false
    property point bandStart

    onPressed: (mouse) =>
    {
        banding = virtualConsole.editMode && (mouse.modifiers & Qt.ShiftModifier) !== 0
        preventStealing = banding
        bandStart = Qt.point(mouse.x, mouse.y)
        selectionBand.x = mouse.x
        selectionBand.y = mouse.y
        selectionBand.width = 0
        selectionBand.height = 0
    }
    onPositionChanged: (mouse) =>
    {
        if (!banding)
            return
        selectionBand.x = Math.min(bandStart.x, mouse.x)
        selectionBand.y = Math.min(bandStart.y, mouse.y)
        selectionBand.width = Math.abs(mouse.x - bandStart.x)
        selectionBand.height = Math.abs(mouse.y - bandStart.y)
    }
    onReleased: (mouse) =>
    {
        if (!banding)
            return
        banding = false
        preventStealing = false
        var vcPageObj = virtualConsole.page(pageIndex)
        var scale = vcPageObj.scaleFactor
        var ids = vcEditor.widgetsInRect(vcPageObj.id,
                      Qt.rect(selectionBand.x / scale, selectionBand.y / scale,
                              selectionBand.width / scale, selectionBand.height / scale))
        if (mouse.modifiers & (Qt.ControlModifier | Qt.MetaModifier))
            ids = virtualConsole.selectedIDs.concat(ids)
        virtualConsole.selectedIDs = ids
    }
    onClicked: (mouse) =>
    {
        if ((mouse.modifiers & Qt.ShiftModifier) === 0)
            virtualConsole.resetWidgetSelection()
    }

    Rectangle
    {
        id: selectionBand
        // drawn over the widgets, which are siblings of this area
        parent: canvasArea.parent
        z: 100
        visible: canvasArea.banding
        color: Qt.rgba(UISettings.highlight.r, UISettings.highlight.g, UISettings.highlight.b, 0.2)
        border.width: 1
        border.color: UISettings.highlight
    }
}
