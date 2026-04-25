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
    // Canonical 1/16 table — must match engine s_beatSixteenths
    readonly property var sixteenthsTable: [
        0, 63, 125, 188, 250, 313, 375, 438,
        500, 563, 625, 688, 750, 813, 875, 938
    ]
    property int beatCount: 1
    property int subdivIdx: 2 // default 1/4
    property bool _suppressBeatsSync: false

    function applyBeatValue()
    {
        if (beatCount < 1)
            beatCount = 1
        if (beatCount > 32)
            beatCount = 32
        _suppressBeatsSync = true
        // For 1/16 subdivisions, use the canonical table to match the engine
        var v
        if (subdivIdx === 4) {
            var wholeBeats = Math.floor(beatCount / 16)
            var sixteenths = beatCount % 16
            v = wholeBeats * 1000 + sixteenthsTable[sixteenths]
        } else {
            v = beatCount * subdivisions[subdivIdx].units
        }
        updateTime(v, "")
        _suppressBeatsSync = false
    }

    function decomposeBeats(v)
    {
        if (v <= 0)
            return { count: 1, idx: subdivIdx }
        // Try coarse subdivisions first (1/1, 1/2, 1/4, 1/8)
        for (var i = 0; i < 4; i++)
        {
            var u = subdivisions[i].units
            if (u > 0 && v >= u && v % u === 0)
                return { count: v / u, idx: i }
        }
        // Try 1/16: check if fractional part matches the canonical table
        var wholeBeats = Math.floor(v / 1000)
        var frac = v % 1000
        for (var j = 1; j < 16; j++)
        {
            if (frac === sixteenthsTable[j])
                return { count: wholeBeats * 16 + j, idx: 4 }
        }
        if (frac === 0 && wholeBeats > 0)
            return { count: wholeBeats, idx: 0 }
        // Fallback: snap to nearest 1/16
        return { count: Math.max(1, Math.round(v / 63)), idx: 4 }
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
            timeValue = TimeUtils.qlcStringToTime(string, tempoType)
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

        onClicked:
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
        visible: tempoType === QLCFunction.Beats
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.columnSpan: 4
        Layout.preferredHeight: UISettings.iconSizeDefault
        spacing: 0

        Repeater
        {
            model: subdivisions

            GenericButton
            {
                Layout.fillWidth: true
                Layout.fillHeight: true
                border.color: UISettings.bgMedium
                bgColor: index === subdivIdx ? UISettings.highlight : buttonsBgColor
                fontSize: btnFontSize
                label: modelData.label
                onClicked:
                {
                    subdivIdx = index
                    applyBeatValue()
                }
            }
        }
    }
}
