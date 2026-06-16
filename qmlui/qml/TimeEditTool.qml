/*
  Q Light Controller Plus
  TimeEditTool.qml

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
import "TimeUtils.js" as TimeUtils

import "."

GridLayout
{
    id: toolRoot
    columns: 5
    rows: 4
    columnSpacing: 0
    rowSpacing: 0

    property color buttonsBgColor: "#05438E"
    property int btnFontSize: UISettings.textSizeDefault
    property string title
    property string timeValueString

    property int timeValue: 0

    /* The TAP time counter */
    property double tapTimeValue: 0
    //needed for bpm tapping
    property int tapCount: 0
    property double lastTap: 0
    property var tapHistory: []

    /* If needed, this property can be used to recognize which type
       of speed value is being edited */
    property int speedType

    /* The type of the tempo being edited. Can be Time or Beats */
    property int tempoType: QLCFunction.Time
    property int allowFractions: QLCFunction.NoFractions
    property int currentFraction: 0

    /* Beats-mode count spinner + subdivision selector state.
       The composed value is beatCount * subdivisions[subdivIdx].units. */
    readonly property var subdivisions: [
        { label: "1/1",  units: 1000 },
        { label: "1/2",  units: 500  },
        { label: "1/4",  units: 250  },
        { label: "1/8",  units: 125  },
        { label: "1/16", units: 63   }
    ]
    // Max subdivision index based on allowFractions
    readonly property int maxSubdivIdx: {
        if (allowFractions === QLCFunction.FineFractions) return 4   // 1/16
        if (allowFractions === QLCFunction.AllFractions) return 3    // 1/8
        if (allowFractions === QLCFunction.ByTwoFractions) return 1  // 1/2
        return 0  // NoFractions: whole beats only
    }
    property int beatCount: 1
    property int subdivIdx: 2 // default 1/4
    property bool _suppressBeatsSync: false

    function applyBeatValue()
    {
        if (beatCount < 1)
            beatCount = 1
        if (beatCount > 32)
            beatCount = 32
        if (subdivIdx > maxSubdivIdx)
            subdivIdx = maxSubdivIdx
        _suppressBeatsSync = true
        var subdiv = [1, 2, 4, 8, 16][subdivIdx]
        var v = QLCFunction.musicalBeatValue(beatCount, subdiv)
        updateTime(v, "")
        _suppressBeatsSync = false
    }

    function decomposeBeats(v)
    {
        if (v <= 0)
            return { count: 1, idx: subdivIdx }
        var pt = QLCFunction.beatValueToMusicalPoint(v)
        if (pt.x > 0 && pt.y > 0)
        {
            // Map subdivision value to index: 1→0, 2→1, 4→2, 8→3, 16→4
            var subdivToIdx = { 1: 0, 2: 1, 4: 2, 8: 3, 16: 4 }
            var idx = subdivToIdx[pt.y]
            if (idx !== undefined)
            {
                // Clamp to allowed range
                if (idx > maxSubdivIdx)
                {
                    var coarseUnits = [1000, 500, 250, 125, 63][maxSubdivIdx]
                    var count = Math.max(1, Math.min(32, Math.round(v / coarseUnits)))
                    return { count: count, idx: maxSubdivIdx }
                }
                return { count: Math.min(32, pt.x), idx: idx }
            }
        }
        // Fallback: whole beats
        return { count: Math.max(1, Math.round(v / 1000)), idx: 0 }
    }

    onTimeValueChanged:
    {
        if (_suppressBeatsSync || tempoType !== QLCFunction.Beats)
            return
        if (timeValue <= 0)
            return
        var d = decomposeBeats(timeValue)
        beatCount = d.count
        subdivIdx = d.idx
    }

    onTempoTypeChanged:
    {
        if (tempoType === QLCFunction.Beats && timeValue > 0)
        {
            var d = decomposeBeats(timeValue)
            beatCount = d.count
            subdivIdx = d.idx
        }
    }

    onAllowFractionsChanged:
    {
        if (subdivIdx > maxSubdivIdx)
            subdivIdx = maxSubdivIdx
    }

    /* If needed, this can be the reference index of an item in a list */
    property int indexInList

    signal valueChanged(int val)
    signal tabPressed(bool forward)
    signal closed()

    function show(tX, tY, tTitle, tStrValue, tType)
    {
        tapTimeValue = 0
        tapTimer.stop()
        title = tTitle
        speedType = tType
        timeValueString = tStrValue
        timeValue = TimeUtils.qlcStringToTime(timeValueString, tempoType)
        if (allowFractions !== QLCFunction.NoFractions)
            currentFraction = (timeValue % 1000)

        if (tX >= 0)
            x = tX
        if (tY >= 0)
        {
            y = tY
            if (y + height > mainView.height)
                y = mainView.height - height - UISettings.listItemHeight
        }

        visible = true
        timeBox.selectAndFocus()
    }

    function updateTime(value, string)
    {
        if (value !== -1 && value !== timeValue)
        {
            timeValue = value
            timeValueString = TimeUtils.timeToQlcString(timeValue, tempoType)
            toolRoot.valueChanged(timeValue)
        }
        if (string !== "" && string !== timeValueString)
        {
            var parsed = TimeUtils.qlcStringToTime(string, tempoType)
            if (isNaN(parsed) || parsed < -2)
            {
                // Reject unparseable input: revert displayed string to current value.
                timeValueString = TimeUtils.timeToQlcString(timeValue, tempoType)
                return
            }
            timeValue = parsed
            timeValueString = TimeUtils.timeToQlcString(timeValue, tempoType)
            toolRoot.valueChanged(timeValue)
        }
    }

    Timer
    {
        id: tapTimer
        repeat: true
        running: false
        interval: 500

        onTriggered:
        {
            if (tapButton.border.color == UISettings.bgMedium)
                tapButton.border.color = "#00FF00"
            else
                tapButton.border.color = UISettings.bgMedium
        }
    }

    // title bar + close button
    Rectangle
    {
        height: UISettings.iconSizeDefault
        Layout.fillWidth: true
        Layout.columnSpan: 5
        gradient:
            Gradient
            {
                GradientStop { position: 0; color: UISettings.toolbarStartSub }
                GradientStop { position: 1; color: UISettings.toolbarEnd }
            }

        RobotoText
        {
            id: titleBox
            anchors.fill: parent
            anchors.margins: 3

            label: title
            fontSize: UISettings.textSizeDefault * 0.75
        }
        // allow the tool to be dragged around
        // by holding it on the title bar
        MouseArea
        {
            anchors.fill: parent
            drag.target: toolRoot
        }
        GenericButton
        {
            width: height
            height: parent.height
            anchors.right: parent.right
            border.color: UISettings.bgMedium
            //bgColor: buttonsBgColor
            useFontawesome: true
            label: FontAwesome.fa_xmark

            onClicked:
            {
                tapTimer.stop()
                toolRoot.visible = false
                toolRoot.closed()
            }
        }
    }

    // top row: tap, increase values
    GenericButton
    {
        id: tapButton
        width: UISettings.iconSizeDefault
        Layout.fillHeight: true
        Layout.rowSpan: 2
        bgColor: buttonsBgColor
        fontSize: btnFontSize
        label: qsTr("Tap")

        onClicked: (mouseButton) =>
        {
            /* right click resets the current TAP time */
                if (mouseButton === Qt.RightButton)
                {
                    tapTimer.stop()
                    tapButton.border.color = UISettings.bgMedium
                    lastTap = 0
                    tapHistory = []
                }
                else
                {
                    var currTime = new Date().getTime()

                    if (lastTap != 0 && currTime - lastTap < 1500)
                    {
                        var newTime = currTime - lastTap

                        tapHistory.push(newTime)

                        tapTimeValue = TimeUtils.calculateBPMByTapIntervals(tapHistory)

                        updateTime(tapTimeValue, "")
                        tapTimer.interval = timeValue
                        tapTimer.restart()
                    }
                    else
                    {
                        lastTap = 0
                        tapHistory = []
                    }
                    lastTap = currTime
                }
        }
    }

    GenericButton
    {
        visible: tempoType === QLCFunction.Time
        width: height
        height: UISettings.iconSizeDefault
        border.color: UISettings.bgMedium
        bgColor: buttonsBgColor
        fontSize: btnFontSize
        label: "+M"
        repetition: true
        onClicked: updateTime(timeValue + (60 * 1000), "")
    }

    GenericButton
    {
        visible: tempoType === QLCFunction.Time
        width: height * 1.2
        height: UISettings.iconSizeDefault
        border.color: UISettings.bgMedium
        bgColor: buttonsBgColor
        fontSize: btnFontSize
        label: "+S"
        repetition: true
        onClicked: updateTime(timeValue + 1000, "")
    }

    GenericButton
    {
        visible: tempoType === QLCFunction.Time
        width: height * 1.2
        height: UISettings.iconSizeDefault
        border.color: UISettings.bgMedium
        bgColor: buttonsBgColor
        fontSize: btnFontSize
        label: "+ms"
        repetition: true
        onClicked: updateTime(timeValue + 1, "")
    }

    // === BEATS MODE: Count spinner row (top) ===
    RowLayout
    {
        visible: tempoType === QLCFunction.Beats
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.columnSpan: 4
        Layout.preferredHeight: UISettings.iconSizeDefault
        spacing: 0

        GenericButton
        {
            Layout.fillHeight: true
            Layout.preferredWidth: UISettings.iconSizeDefault
            border.color: UISettings.bgMedium
            bgColor: buttonsBgColor
            fontSize: btnFontSize
            label: "-"
            repetition: true
            onClicked:
            {
                if (beatCount > 1)
                {
                    beatCount--
                    applyBeatValue()
                }
            }
        }

        Rectangle
        {
            Layout.fillHeight: true
            Layout.fillWidth: true
            color: "#444"
            border.color: UISettings.bgMedium

            RobotoText
            {
                anchors.fill: parent
                label: beatCount + " \u00d7 " + subdivisions[subdivIdx].label
                fontSize: btnFontSize
                textHAlign: Text.AlignHCenter
            }
        }

        GenericButton
        {
            Layout.fillHeight: true
            Layout.preferredWidth: UISettings.iconSizeDefault
            border.color: UISettings.bgMedium
            bgColor: buttonsBgColor
            fontSize: btnFontSize
            label: "+"
            repetition: true
            onClicked:
            {
                if (beatCount < 32)
                {
                    beatCount++
                    applyBeatValue()
                }
            }
        }
    }

    // middle row: tap, time value
    Rectangle
    {
        height: UISettings.iconSizeDefault
        color: "#444"
        border.color: UISettings.bgMedium
        Layout.fillWidth: true
        Layout.columnSpan: 4

        CustomTextEdit
        {
            id: timeBox
            anchors.fill: parent
            //anchors.fill: parent
            horizontalAlignment: TextInput.AlignHCenter
            radius: 0
            text: timeValueString
            font.pixelSize: btnFontSize

            onAccepted: updateTime(-1, text)

            Keys.onTabPressed:
            {
                updateTime(-1, text)
                toolRoot.tabPressed(true)
            }
            Keys.onBacktabPressed:
            {
                updateTime(-1, text)
                toolRoot.tabPressed(false)
            }
            Keys.onEscapePressed:
            {
                tapTimer.stop()
                toolRoot.visible = false
                toolRoot.closed()
            }
        }
    }

    // bottom row: infinite, decrease values
    GenericButton
    {
        width: height
        height: UISettings.iconSizeDefault
        border.color: UISettings.bgMedium
        bgColor: buttonsBgColor
        fontSize: btnFontSize
        label: "∞"
        onClicked: updateTime(-2, "")
    }

    GenericButton
    {
        visible: tempoType === QLCFunction.Time
        width: height
        height: UISettings.iconSizeDefault
        border.color: UISettings.bgMedium
        bgColor: buttonsBgColor
        fontSize: btnFontSize
        label: "-M"
        repetition: true
        onClicked:
        {
            if (timeValue < 60000)
                return
            updateTime(timeValue - (60 * 1000), "")
        }
    }

    GenericButton
    {
        visible: tempoType === QLCFunction.Time
        width: height * 1.2
        height: UISettings.iconSizeDefault
        border.color: UISettings.bgMedium
        bgColor: buttonsBgColor
        fontSize: btnFontSize
        label: "-S"
        repetition: true
        onClicked:
        {
            if (timeValue < 1000)
                return
            updateTime(timeValue - 1000, "")
        }
    }

    GenericButton
    {
        visible: tempoType === QLCFunction.Time
        width: height * 1.2
        height: UISettings.iconSizeDefault
        border.color: UISettings.bgMedium
        bgColor: buttonsBgColor
        fontSize: btnFontSize
        label: "-ms"
        repetition: true
        onClicked:
        {
            if (timeValue == 0)
                return
            updateTime(timeValue - 1, "")
        }
    }

    // === BEATS MODE: Subdivision selector row (bottom) ===
    RowLayout
    {
        visible: tempoType === QLCFunction.Beats && maxSubdivIdx > 0
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.columnSpan: 4
        Layout.preferredHeight: UISettings.iconSizeDefault
        spacing: 0

        Repeater
        {
            model: maxSubdivIdx + 1

            GenericButton
            {
                Layout.fillWidth: true
                Layout.fillHeight: true
                border.color: UISettings.bgMedium
                bgColor: index === subdivIdx ? UISettings.highlight : buttonsBgColor
                fontSize: btnFontSize
                label: subdivisions[index].label
                onClicked:
                {
                    subdivIdx = index
                    applyBeatValue()
                }
            }
        }
    }
}
