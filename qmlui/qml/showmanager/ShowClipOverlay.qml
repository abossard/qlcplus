/*
  Q Light Controller Plus
  ShowClipOverlay.qml

  Copyright (c) QLC+ contributors

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
import QtQuick.Shapes

import org.qlcplus.classes 1.0
import "."

/* The clip automation of a Show item, drawn over it from the clip start:
   shading where the clip is not dominant, a marker at every segment start
   (loop wrap, seek, pause, pitch change) and the envelope line */
Item
{
    id: overlayRoot
    objectName: "clipAutomationOverlay"
    clip: true
    visible: drawn.inPoint >= 0 || drawn.envelope.length > 0 || drawn.shaded.length > 0

    property ShowFunction sfRef: null
    /* pixels per unit of the item's own unit */
    property real pixelsPerUnit: 1
    /* pixels the drawing moves while the left edge is dragged, so it stays at its Show times */
    property real shift: 0

    property var drawn: ({ inPoint: -1, boundaries: [], envelope: [], shaded: [] })

    function refresh()
    {
        if (sfRef)
            drawn = showClipEditor.overlay(sfRef)
        else
            drawn = { inPoint: -1, boundaries: [], envelope: [], shaded: [] }
    }

    function px(value)
    {
        return value * pixelsPerUnit
    }

    function envelopeY(level)
    {
        return height - 3 - level * (height - 6)
    }

    /* held before the first keyframe and after the last one */
    function envelopePoints()
    {
        var keyframes = drawn.envelope
        if (keyframes.length === 0)
            return []
        var points = [ Qt.point(-shift, envelopeY(keyframes[0].level)) ]
        for (var i = 0; i < keyframes.length; i++)
            points.push(Qt.point(px(keyframes[i].clipMs), envelopeY(keyframes[i].level)))
        var last = keyframes[keyframes.length - 1]
        points.push(Qt.point(Math.max(width - shift, px(last.clipMs)), envelopeY(last.level)))
        return points
    }

    onSfRefChanged: refresh()

    Connections
    {
        target: overlayRoot.sfRef
        function onClipAutomationChanged() { overlayRoot.refresh() }
        function onDurationChanged() { overlayRoot.refresh() }
    }

    Repeater
    {
        model: overlayRoot.drawn.shaded

        Rectangle
        {
            objectName: "clipShade"
            x: overlayRoot.shift + overlayRoot.px(modelData.fromMs)
            width: overlayRoot.px(modelData.toMs) - overlayRoot.px(modelData.fromMs)
            height: overlayRoot.height
            color: "#80000000"
        }
    }

    Repeater
    {
        model: overlayRoot.drawn.boundaries

        Rectangle
        {
            objectName: "clipBoundary"
            x: overlayRoot.shift + overlayRoot.px(modelData) - width / 2
            width: 2
            height: overlayRoot.height
            color: "#f1c40f"
        }
    }

    Shape
    {
        objectName: "clipEnvelope"
        x: overlayRoot.shift
        visible: overlayRoot.drawn.envelope.length > 0

        ShapePath
        {
            strokeColor: "#2ecc71"
            strokeWidth: 2
            fillColor: "transparent"

            PathPolyline { path: overlayRoot.envelopePoints() }
        }
    }
}
