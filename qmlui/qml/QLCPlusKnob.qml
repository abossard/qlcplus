/*
  Q Light Controller Plus
  QLCPlusKnob.qml

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
import QtQuick.Controls
import QtQuick.Controls.Basic

import "CanvasDrawFunctions.js" as DrawFuncs
import "."

Dial
{
    id: control
    implicitHeight: 50
    implicitWidth: 50
    height: implicitHeight
    width: implicitWidth
    clip: true

    from: 0
    to: 255
    stepSize: 1.0
    wheelEnabled: false

    property bool drawOuterLevel: true
    property real knobSize: Math.min(width, height)

    /** Base color of the knob body. When set to a non-gray color, the dial
     *  body is tinted accordingly (used e.g. for R/G/B knobs) */
    property color knobColor: "#808080"

    /** Color of the level progress arc */
    property color levelColor: "#00FF00"

    onPositionChanged: kCanvas.requestPaint()
    onHeightChanged: kCanvas.requestPaint()
    onWidthChanged: kCanvas.requestPaint()
    onKnobColorChanged: kCanvas.requestPaint()
    onLevelColorChanged: kCanvas.requestPaint()

    /** Finger travel, in pixels, that turns the knob through its whole range
     *  with a trackpad */
    property real wheelTravel: 500

    // A mouse wheel notch turns the knob by one step. A trackpad swipe is a
    // stream of small pixel deltas followed by an inertia tail, and the page
    // can slide a knob under a resting pointer in the middle of a swipe, so:
    // only a swipe that began over this knob turns it, the turn follows the
    // finger travel instead of the number of events, and the inertia tail is
    // ignored.
    property bool wheelOwned: false
    property real wheelRemainder: 0

    Timer
    {
        id: wheelOwnerTimer
        interval: 350
        onTriggered:
        {
            control.wheelOwned = false
            control.wheelRemainder = 0
        }
    }

    MouseArea
    {
        anchors.fill: parent
        acceptedButtons: Qt.NoButton

        onWheel: (wheel) =>
        {
            if (!control.enabled)
            {
                wheel.accepted = false
                return
            }

            var previousValue = control.value

            if (wheel.phase === undefined || wheel.phase === Qt.NoScrollPhase)
            {
                var rawSteps = wheel.angleDelta.y / 120.0
                if (rawSteps === 0 && wheel.pixelDelta.y !== 0)
                    rawSteps = wheel.pixelDelta.y > 0 ? 1 : -1

                var steps = rawSteps > 0 ? Math.ceil(rawSteps) : Math.floor(rawSteps)
                if (steps > 0)
                {
                    for (var i = 0; i < steps; i++)
                        control.increase()
                }
                else if (steps < 0)
                {
                    for (var j = 0; j < -steps; j++)
                        control.decrease()
                }
            }
            else
            {
                if (wheel.phase === Qt.ScrollBegin)
                    control.wheelOwned = true

                // content sliding under a resting pointer keeps scrolling the page
                if (!control.wheelOwned)
                {
                    wheel.accepted = false
                    return
                }

                wheelOwnerTimer.restart()

                if (wheel.phase === Qt.ScrollMomentum)
                {
                    wheel.accepted = true
                    return
                }

                // stepped like the notch above, so a value bound by the owner of
                // the knob stays bound
                var travel = wheel.pixelDelta.y !== 0 ? wheel.pixelDelta.y : wheel.angleDelta.y / 2
                var stepsInRange = (control.to - control.from) / (control.stepSize > 0 ? control.stepSize : 1)
                control.wheelRemainder += travel * stepsInRange / control.wheelTravel
                var whole = control.wheelRemainder > 0 ? Math.floor(control.wheelRemainder) : Math.ceil(control.wheelRemainder)
                if (whole !== 0)
                {
                    control.wheelRemainder -= whole
                    for (var k = 0; k < Math.abs(whole); k++)
                    {
                        if (whole > 0)
                            control.increase()
                        else
                            control.decrease()
                    }
                }

                if (wheel.phase === Qt.ScrollEnd)
                {
                    control.wheelOwned = false
                    control.wheelRemainder = 0
                }
            }

            if (control.value !== previousValue)
                control.moved()

            // Keep wheel control local to the knob and avoid page Flickable scrolling.
            wheel.accepted = true
        }
    }

    background: Canvas {
        id: kCanvas
        width: control.knobSize
        height: control.knobSize
        anchors.centerIn: parent
        antialiasing: true
        contextType: "2d"

        onPaint:
        {
            if (!context)
                return

            var startAngle = 90 + 40
            var arcWidth = 4
            var darkShade = Qt.darker(control.knobColor, 1.13).toString()
            var lightShade = Qt.lighter(control.knobColor, 1.07).toString()
            var outerGrad = context.createLinearGradient(0, 0, 0, height)
            var innerGrad = context.createLinearGradient(0, 0, 0, height)
            outerGrad.addColorStop(0, darkShade)
            outerGrad.addColorStop(1, lightShade)
            innerGrad.addColorStop(0, lightShade)
            innerGrad.addColorStop(1, darkShade)

            context.globalAlpha = 1.0
            context.clearRect(0, 0, width, height)

            context.beginPath()
            context.fillStyle = outerGrad
            context.arc(width / 2, height / 2, (width * 0.5) - arcWidth, 0, 2 * Math.PI)
            context.fill()
            context.closePath()

            context.beginPath()
            context.fillStyle = innerGrad
            context.arc(width / 2, height / 2, (width * 0.35), 0, 2 * Math.PI)
            context.fill()
            context.closePath()

            if (drawOuterLevel)
            {
                context.beginPath()
                context.strokeStyle = control.levelColor.toString()
                context.lineWidth = arcWidth
                context.arc(width / 2, height / 2, (width / 2) - (arcWidth / 2),
                            DrawFuncs.degToRad(startAngle),
                            DrawFuncs.degToRad(startAngle + (control.position * 280)))
                context.stroke()
                context.closePath()
            }
        }
    }

    handle: Rectangle {
        id: handleItem
        x: background.x + background.width / 2 - handle.width / 2
        y: background.y + background.height / 2 - handle.height / 2
        width: control.knobSize / 7
        height: width
        color: control.pressed ? "#17a81a" : "#21be2b"
        border.width: 1
        border.color: UISettings.bgStrong
        radius: height / 2
        antialiasing: true
        opacity: control.enabled ? 1 : 0.3
        transform: [
            Translate {
                y: -Math.min(control.background.width, control.background.height) * 0.35 + handleItem.height / 2
            },
            Rotation {
                angle: control.angle
                origin.x: handleItem.width / 2
                origin.y: handleItem.height / 2
            }
        ]
    }
}
