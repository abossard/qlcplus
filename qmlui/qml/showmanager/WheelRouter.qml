/*
  Q Light Controller Plus
  WheelRouter.qml

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

/**
 * Takes the scroll gestures of a trackpad (or magic mouse) over its area and
 * turns them into scroll and zoom steps for the view behind it.
 *
 * Two nested Flickables cannot share such a gesture: a Flickable only starts a
 * wheel drag once one axis leads the other by 2x, so a swipe between roughly
 * 27 and 63 degrees off an axis scrolls nothing at all. Here a swipe that is
 * clearly vertical or horizontal stays on that axis, inertia included, so the
 * drift of the fingers does not move the other one, until it really turns
 * diagonal. A diagonal swipe scrolls both.
 *
 * A mouse wheel is left to the Flickables, which animate it. Clicks and hover
 * pass through to whatever is below.
 */

Item
{
    id: router

    /** Pixels the content moves by, with the sign of QWheelEvent::pixelDelta */
    signal scrolled(real dx, real dy)

    /** One zoom step, > 1 zooms in. x and y: the pointer position in this item */
    signal zoomed(real factor, real x, real y)

    /** Cmd + scroll (Ctrl + scroll elsewhere) and the pinch zoom when set */
    property bool zoomEnabled: true

    /** Finger travel, in pixels, needed to tell which way a swipe goes */
    property real lockDistance: 8

    /** How much one axis has to lead the other to lock the swipe to it */
    property real lockRatio: 2

    /** A locked swipe turns free once the other axis has travelled this far,
     *  and at least unlockRatio times as far as the locked one: it is going
     *  diagonal for real, not drifting */
    property real unlockDistance: 24
    property real unlockRatio: 0.4

    /** A zoom step is exp(travel * zoomSpeed) */
    property real zoomSpeed: 0.006

    // "" while undecided, then "x", "y" or "free" (diagonal)
    property string lock: ""
    // travel kept back while the direction is undecided
    property real heldX: 0
    property real heldY: 0
    // net travel of the whole swipe
    property real travelX: 0
    property real travelY: 0

    function reset()
    {
        lock = ""
        heldX = 0
        heldY = 0
        travelX = 0
        travelY = 0
    }

    function handleWheel(wheel)
    {
        var phase = wheel.phase === undefined ? Qt.NoScrollPhase : wheel.phase
        var zooming = zoomEnabled && (wheel.modifiers & Qt.ControlModifier)

        // a mouse wheel notch scrolls through the Flickables behind, which animate it
        if (phase === Qt.NoScrollPhase && !zooming)
        {
            wheel.accepted = false
            return
        }
        wheel.accepted = true

        var dx = wheel.pixelDelta.x
        var dy = wheel.pixelDelta.y
        if (dx === 0 && dy === 0)
        {
            // no pixel data: 120 angle units per notch, and twice the pixels for a trackpad
            var perPixel = phase === Qt.NoScrollPhase ? 120 / 40 : 2
            dx = wheel.angleDelta.x / perPixel
            dy = wheel.angleDelta.y / perPixel
        }

        if (zooming)
        {
            if (dy !== 0)
                zoomed(Math.exp(dy * zoomSpeed), wheel.x, wheel.y)
            return
        }

        if (phase === Qt.ScrollBegin)
            reset()

        travelX += dx
        travelY += dy

        if (lock === "")
        {
            heldX += dx
            heldY += dy
            var ax = Math.abs(heldX)
            var ay = Math.abs(heldY)

            // too short to tell the direction: the travel is applied once it is decided
            if (Math.max(ax, ay) < lockDistance)
            {
                if (phase === Qt.ScrollEnd)
                    reset()
                return
            }

            lock = ay >= lockRatio * ax ? "y" : (ax >= lockRatio * ay ? "x" : "free")
            dx = heldX
            dy = heldY
            heldX = 0
            heldY = 0
        }

        if (lock === "y")
        {
            if (Math.abs(travelX) >= unlockDistance && Math.abs(travelX) >= unlockRatio * Math.abs(travelY))
            {
                lock = "free"
                dx = travelX
            }
            else
                dx = 0
        }
        else if (lock === "x")
        {
            if (Math.abs(travelY) >= unlockDistance && Math.abs(travelY) >= unlockRatio * Math.abs(travelX))
            {
                lock = "free"
                dy = travelY
            }
            else
                dy = 0
        }

        if (dx !== 0 || dy !== 0)
            scrolled(dx, dy)

        if (phase === Qt.ScrollEnd)
            reset()
    }

    MouseArea
    {
        anchors.fill: parent
        acceptedButtons: Qt.NoButton

        onWheel: (wheel) => router.handleWheel(wheel)
    }

    // The pinch of a trackpad, or of two fingers on a touch screen. It is on
    // the Item and not on the MouseArea: a pointer handler makes its Item
    // accept every mouse button, and a MouseArea doing so would swallow all
    // the clicks meant for what is below.
    PinchHandler
    {
        enabled: router.zoomEnabled
        target: null

        property real lastScale: 1

        onActiveChanged: if (active) lastScale = 1

        onActiveScaleChanged:
        {
            if (!active)
                return

            var factor = activeScale / lastScale
            lastScale = activeScale
            if (factor > 0 && factor !== 1)
                router.zoomed(factor, centroid.position.x, centroid.position.y)
        }
    }
}
