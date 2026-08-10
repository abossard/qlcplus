/*
  Q Light Controller Plus
  HUEMatrixEditor.qml

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
import QtQuick.Controls
import QtQuick.Dialogs

import org.qlcplus.classes 1.0

import "TimeUtils.js" as TimeUtils
import "."

Rectangle
{
    id: rgbmeContainer
    //anchors.fill: parent
    color: "transparent"

    property int functionID: -1
    property var algoColors: hueMatrixEditor ? hueMatrixEditor.algoColors : null

    signal requestView(int ID, string qmlSrc, bool back)


    TimeEditTool
    {
        id: timeEditTool

        parent: mainView
        z: 99
        x: rightSidePanel.x - width
        visible: false
        tempoType: hueMatrixEditor.tempoType

        onValueChanged:
        {
            if (speedType == QLCFunction.FadeIn)
                hueMatrixEditor.fadeInSpeed = val
            else if (speedType == QLCFunction.Hold)
                hueMatrixEditor.holdSpeed = val
            else if (speedType == QLCFunction.FadeOut)
                hueMatrixEditor.fadeOutSpeed = val
        }
    }

    ColorTool
    {
        id: colorTool
        x: -width - (UISettings.iconSizeDefault * 1.25)
        y: UISettings.bigItemHeight
        visible: false
        closeOnSelect: true

        property int colorIndex: -1
        property Item previewBtn

        function showTool(index, button)
        {
            colorIndex = index
            previewBtn = button
            currentRGB = hueMatrixEditor.colorAtIndex(colorIndex)
            visible = true
        }

        function hide()
        {
            visible = false
            colorIndex = -1
            previewBtn = null
        }

        onToolColorChanged:
            function(r, g, b, w, a, uv)
            {
                hueMatrixEditor.setColorAtIndex(colorIndex, Qt.rgba(r, g, b, 1.0))
            }
        onClose: visible = false
    }

    EditorTopBar
    {
        id: topBar
        text: hueMatrixEditor.functionName
        onTextChanged: hueMatrixEditor.functionName = text

        onBackClicked:
        {
            var prevID = hueMatrixEditor.previousID
            requestView(prevID, functionManager.getEditorResource(prevID), true)
        }
    }

    Flickable
    {
        id: editorFlickable
        x: 5
        y: topBar.height + 2
        width: parent.width - 10
        height: parent.height - y

        contentHeight: editorColumn.height
        boundsBehavior: Flickable.StopAtBounds

        Component.onCompleted: console.log("Flickable height: " + height + ", Grid height: " + editorColumn.height + ", parent height: " + parent.height)

        Column
        {
            id: editorColumn
            width: parent.width
            spacing: 2

            property int itemsHeight: UISettings.listItemHeight
            property int firstColumnWidth: 0
            property int colWidth: parent.width - (sbar.visible ? sbar.width : 0)

            //onHeightChanged: editorFlickable.contentHeight = height //console.log("Grid layout height changed: " + height)

            function checkLabelWidth(w)
            {
                firstColumnWidth = Math.max(w, firstColumnWidth)
            }

            // row 1
            RowLayout
            {
                width: editorColumn.colWidth

                RobotoText
                {
                    label: qsTr("Fixture Group")
                    height: editorColumn.itemsHeight
                    onWidthChanged:
                    {
                        editorColumn.checkLabelWidth(width)
                        width = Qt.binding(function() { return editorColumn.firstColumnWidth })
                    }
                }
                CustomComboBox
                {
                    Layout.fillWidth: true
                    height: editorColumn.itemsHeight
                    model: fixtureGroupEditor.groupsListModel
                    currValue: hueMatrixEditor.fixtureGroup
                    onValueChanged: (value) => hueMatrixEditor.fixtureGroup = value
                }
            }

            // row 2
            RGBMatrixPreview
            {
                width: editorColumn.width
                matrixSize: hueMatrixEditor.previewSize
                matrixData: hueMatrixEditor.previewData
                maximumHeight: rgbmeContainer.height / 3
            }


            GridLayout
            {
                width: editorColumn.colWidth
                columns: 3

                // row 3
                RobotoText
                {
                    id: patternLabel
                    label: qsTr("Pattern")
                    height: editorColumn.itemsHeight
                    onWidthChanged:
                    {
                        editorColumn.checkLabelWidth(width)
                        width = Qt.binding(function() { return editorColumn.firstColumnWidth })
                    }
                }
                CustomComboBox
                {
                    id: algoCombo
                    Layout.fillWidth: true
                    height: editorColumn.itemsHeight
                    textRole: ""
                    model: hueMatrixEditor.algorithms
                    currentIndex: hueMatrixEditor.algorithmIndex
                    onDisplayTextChanged:
                    {
                        hueMatrixEditor.algorithmIndex = currentIndex
                        paramSection.sectionContents = null
                        if (displayText === "Text")
                            paramSection.sectionContents = textAlgoComponent
                        else if (displayText === "Image")
                            paramSection.sectionContents = imageAlgoComponent
                        else
                            paramSection.sectionContents = scriptAlgoComponent
                    }
                }

                Row
                {
                    height: editorColumn.itemsHeight
                    spacing: 4

                    IconButton
                    {
                        width: UISettings.listItemHeight
                        height: width
                        imgSource: "qrc:/sequence.svg"
                        tooltip: qsTr("Save this matrix to a sequence")
                        onClicked: hueMatrixEditor.saveToSequence()
                    }
                }

                // row 4
                RobotoText
                {
                    label: qsTr("Blend mode")
                    height: editorColumn.itemsHeight
                    onWidthChanged:
                    {
                        editorColumn.checkLabelWidth(width)
                        width = Qt.binding(function() { return editorColumn.firstColumnWidth })
                    }
                }
                CustomComboBox
                {
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                    height: editorColumn.itemsHeight
                    model: [
                        { mLabel: qsTr("Default (HTP)") },
                        { mLabel: qsTr("Mask") },
                        { mLabel: qsTr("Additive") },
                        { mLabel: qsTr("Subtractive") }
                    ]

                    currentIndex: hueMatrixEditor.blendMode
                    onCurrentIndexChanged: hueMatrixEditor.blendMode = currentIndex
                }

                // row 5
                RobotoText
                {
                    label: qsTr("Color mode")
                    height: editorColumn.itemsHeight
                    onWidthChanged:
                    {
                        editorColumn.checkLabelWidth(width)
                        width = Qt.binding(function() { return editorColumn.firstColumnWidth })
                    }
                }
                CustomComboBox
                {
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                    height: editorColumn.itemsHeight
                    model: [
                        { mLabel: qsTr("Default (RGB)") },
                        { mLabel: qsTr("White") },
                        { mLabel: qsTr("Amber") },
                        { mLabel: qsTr("UV") },
                        { mLabel: qsTr("Dimmer") },
                        { mLabel: qsTr("Shutter") },
                        { mLabel: qsTr("RGBW (Accurate)") },
                        { mLabel: qsTr("RGBW (Brighter)") }
                    ]

                    currentIndex: hueMatrixEditor.controlMode
                    onCurrentIndexChanged: hueMatrixEditor.controlMode = currentIndex
                }

                // Brightness
                RobotoText
                {
                    label: qsTr("Brightness")
                    height: editorColumn.itemsHeight
                    onWidthChanged:
                    {
                        editorColumn.checkLabelWidth(width)
                        width = Qt.binding(function() { return editorColumn.firstColumnWidth })
                    }
                }
                CustomDoubleSpinBox
                {
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                    height: editorColumn.itemsHeight
                    decimals: 2
                    realFrom: 0
                    realTo: 1000
                    realStep: 0.01
                    realValue: hueMatrixEditor.brightness
                    onRealValueChanged: hueMatrixEditor.brightness = realValue
                }

                // Rotation
                RobotoText
                {
                    label: qsTr("Rotation")
                    height: editorColumn.itemsHeight
                    onWidthChanged:
                    {
                        editorColumn.checkLabelWidth(width)
                        width = Qt.binding(function() { return editorColumn.firstColumnWidth })
                    }
                }
                CustomComboBox
                {
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                    height: editorColumn.itemsHeight
                    model: [
                        { mLabel: qsTr("None") },
                        { mLabel: qsTr("90\u00B0 CW") },
                        { mLabel: qsTr("180\u00B0") },
                        { mLabel: qsTr("270\u00B0 CW") }
                    ]
                    currentIndex: hueMatrixEditor.rotation
                    onCurrentIndexChanged: hueMatrixEditor.rotation = currentIndex
                }

                // Mirror
                RobotoText
                {
                    label: qsTr("Mirror")
                    height: editorColumn.itemsHeight
                    onWidthChanged:
                    {
                        editorColumn.checkLabelWidth(width)
                        width = Qt.binding(function() { return editorColumn.firstColumnWidth })
                    }
                }
                CustomComboBox
                {
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                    height: editorColumn.itemsHeight
                    model: [
                        { mLabel: qsTr("Off") },
                        { mLabel: qsTr("Horizontal") },
                        { mLabel: qsTr("Vertical") },
                        { mLabel: qsTr("Both") }
                    ]
                    currentIndex: hueMatrixEditor.mirror
                    onCurrentIndexChanged: hueMatrixEditor.mirror = currentIndex
                }

                // Mirror Blend (only visible when mirror is active)
                RobotoText
                {
                    visible: hueMatrixEditor.mirror > 0
                    label: qsTr("Mirror blend")
                    height: editorColumn.itemsHeight
                    onWidthChanged:
                    {
                        editorColumn.checkLabelWidth(width)
                        width = Qt.binding(function() { return editorColumn.firstColumnWidth })
                    }
                }
                CustomComboBox
                {
                    visible: hueMatrixEditor.mirror > 0
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                    height: editorColumn.itemsHeight
                    model: [
                        { mLabel: qsTr("Flip") },
                        { mLabel: qsTr("Max") },
                        { mLabel: qsTr("Average") },
                        { mLabel: qsTr("Additive") }
                    ]
                    currentIndex: hueMatrixEditor.mirrorBlend
                    onCurrentIndexChanged: hueMatrixEditor.mirrorBlend = currentIndex
                }

                // Beat Transform - Effect
                RobotoText
                {
                    label: qsTr("Beat effect")
                    height: editorColumn.itemsHeight
                    onWidthChanged:
                    {
                        editorColumn.checkLabelWidth(width)
                        width = Qt.binding(function() { return editorColumn.firstColumnWidth })
                    }
                }
                CustomComboBox
                {
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                    height: editorColumn.itemsHeight
                    model: [
                        { mLabel: qsTr("Off") },
                        { mLabel: qsTr("Mirror") },
                        { mLabel: qsTr("Color Invert") },
                        { mLabel: qsTr("Blackout") },
                        { mLabel: qsTr("Whiteout") }
                    ]
                    currentIndex: hueMatrixEditor.beatEffect
                    onCurrentIndexChanged: hueMatrixEditor.beatEffect = currentIndex
                }

                // Beat Transform - Selection (only visible when effect != Off)
                RobotoText
                {
                    visible: hueMatrixEditor.beatEffect > 0
                    label: qsTr("Beat selection")
                    height: editorColumn.itemsHeight
                    onWidthChanged:
                    {
                        editorColumn.checkLabelWidth(width)
                        width = Qt.binding(function() { return editorColumn.firstColumnWidth })
                    }
                }
                CustomComboBox
                {
                    visible: hueMatrixEditor.beatEffect > 0
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                    height: editorColumn.itemsHeight
                    model: [
                        { mLabel: qsTr("All on downbeat") },
                        { mLabel: qsTr("Walk") },
                        { mLabel: qsTr("Random") }
                    ]
                    currentIndex: hueMatrixEditor.beatSelection
                    onCurrentIndexChanged: hueMatrixEditor.beatSelection = currentIndex
                }

                // Beat Transform - Orientation (only visible when effect != Off)
                RobotoText
                {
                    visible: hueMatrixEditor.beatEffect > 0
                    label: qsTr("Beat orientation")
                    height: editorColumn.itemsHeight
                    onWidthChanged:
                    {
                        editorColumn.checkLabelWidth(width)
                        width = Qt.binding(function() { return editorColumn.firstColumnWidth })
                    }
                }
                CustomComboBox
                {
                    visible: hueMatrixEditor.beatEffect > 0
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                    height: editorColumn.itemsHeight
                    model: [
                        { mLabel: qsTr("Rows") },
                        { mLabel: qsTr("Columns") }
                    ]
                    currentIndex: hueMatrixEditor.beatOrientation
                    onCurrentIndexChanged: hueMatrixEditor.beatOrientation = currentIndex
                }


                // row 6: dynamic color pickers (one per algoColorsCount)
                RobotoText
                {
                    id: colorLabel
                    label: qsTr("Colors")
                    visible: hueMatrixEditor.algoColorsCount > 0
                    height: editorColumn.itemsHeight
                    onWidthChanged:
                    {
                        editorColumn.checkLabelWidth(width)
                        width = Qt.binding(function() { return editorColumn.firstColumnWidth })
                    }
                }

                Flow
                {
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                    spacing: 6
                    visible: hueMatrixEditor.algoColorsCount > 0

                    Repeater
                    {
                        model: Math.min(hueMatrixEditor.algoColorsCount, 5)

                        delegate: Row
                        {
                            spacing: 2

                            Rectangle
                            {
                                id: colorBtn
                                width: UISettings.iconSizeDefault * 2
                                height: editorColumn.itemsHeight
                                radius: 5
                                border.color: colorMA.containsMouse ? "white" : UISettings.bgLight
                                border.width: 2
                                color: rgbmeContainer.algoColors && hueMatrixEditor.hasColorAtIndex(index)
                                       ? rgbmeContainer.algoColors[index] : "transparent"

                                MouseArea
                                {
                                    id: colorMA
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onClicked:
                                    {
                                        if (colorTool.visible)
                                            colorTool.hide()
                                        else
                                            colorTool.showTool(index, colorBtn)
                                    }
                                }
                            }
                            IconButton
                            {
                                width: UISettings.listItemHeight
                                height: width
                                faSource: FontAwesome.fa_xmark
                                faColor: "darkred"
                                tooltip: qsTr("Reset color %1").arg(index + 1)
                                visible: index > 0
                                onClicked: hueMatrixEditor.resetColorAtIndex(index)
                            }
                        }
                    }
                }
            }

            SectionBox
            {
                id: paramSection
                width: editorColumn.colWidth - 5
                visible: sectionContents ? true : false

                sectionLabel: qsTr("Parameters")
                sectionContents: null
            }

            SectionBox
            {
                id: speedSection
                width: editorColumn.colWidth - 5
                isExpanded: false
                sectionLabel: qsTr("Speed")
                sectionContents:
                    GridLayout
                    {
                        width: parent.width
                        columns: 3
                        columnSpacing: 5
                        rowSpacing: 4

                        function showTimeTool(item, titleLabel, timeLabel, type)
                        {
                            timeEditTool.allowFractions = QLCFunction.FineFractions
                            timeEditTool.show(-1, item.mapToItem(mainView, 0, 0).y - timeEditTool.height,
                                              titleLabel, timeLabel, type)
                        }

                        // Row 1
                        RobotoText
                        {
                            id: fiLabel
                            label: qsTr("Steps fade in")
                            height: UISettings.listItemHeight
                        }

                        Rectangle
                        {
                            Layout.fillWidth: true
                            height: UISettings.listItemHeight
                            color: UISettings.bgMedium

                            RobotoText
                            {
                                id: fiTimeLabel
                                x: 3
                                height: parent.height
                                label: TimeUtils.timeToQlcString(hueMatrixEditor.fadeInSpeed, hueMatrixEditor.tempoType)
                            }
                            MouseArea
                            {
                                anchors.fill: parent
                                onDoubleClicked: showTimeTool(this, fiLabel.label, fiTimeLabel.label, QLCFunction.FadeIn)
                            }
                        }

                        IconButton
                        {
                            width: height
                            height: UISettings.listItemHeight
                            faSource: FontAwesome.fa_clock
                            faColor: UISettings.fgMain
                            onClicked: showTimeTool(this, fiLabel.label, fiTimeLabel.label, QLCFunction.FadeIn)
                        }

                        // Row 2
                        RobotoText
                        {
                            id: hLabel
                            height: UISettings.listItemHeight
                            label: qsTr("Steps hold")
                        }

                        Rectangle
                        {
                            Layout.fillWidth: true
                            height: UISettings.listItemHeight
                            color: UISettings.bgMedium

                            RobotoText
                            {
                                id: hTimeLabel
                                x: 3
                                height: parent.height
                                label: TimeUtils.timeToQlcString(hueMatrixEditor.holdSpeed, hueMatrixEditor.tempoType)
                            }
                            MouseArea
                            {
                                anchors.fill: parent
                                onDoubleClicked: showTimeTool(this, hLabel.label, hTimeLabel.label, QLCFunction.Hold)
                            }
                        }

                        IconButton
                        {
                            width: height
                            height: UISettings.listItemHeight
                            faSource: FontAwesome.fa_clock
                            faColor: UISettings.fgMain
                            onClicked: showTimeTool(this, hLabel.label, hTimeLabel.label, QLCFunction.Hold)
                        }

                        // Row 3
                        RobotoText
                        {
                            id: foLabel
                            height: UISettings.listItemHeight
                            label: qsTr("Steps fade out")
                        }

                        Rectangle
                        {
                            Layout.fillWidth: true
                            height: UISettings.listItemHeight
                            color: UISettings.bgMedium

                            RobotoText
                            {
                                id: foTimeLabel
                                x: 3
                                height: parent.height
                                label: TimeUtils.timeToQlcString(hueMatrixEditor.fadeOutSpeed, hueMatrixEditor.tempoType)
                            }
                            MouseArea
                            {
                                anchors.fill: parent
                                onDoubleClicked: showTimeTool(this, foLabel.label, foTimeLabel.label, QLCFunction.FadeOut)
                            }
                        }

                        IconButton
                        {
                            width: height
                            height: UISettings.listItemHeight
                            faSource: FontAwesome.fa_clock
                            faColor: UISettings.fgMain
                            onClicked: showTimeTool(this, foLabel.label, foTimeLabel.label, QLCFunction.FadeOut)
                        }

                        // Row 4
                        RobotoText
                        {
                            id: ttLabel
                            height: UISettings.listItemHeight
                            label: qsTr("Tempo type")
                        }
                        CustomComboBox
                        {
                            Layout.columnSpan: 2
                            Layout.fillWidth: true
                            height: UISettings.listItemHeight
                            model: [
                                { mLabel: qsTr("Time"), mValue: QLCFunction.Time },
                                { mLabel: qsTr("Beats"), mValue: QLCFunction.Beats }
                            ]

                            currValue: hueMatrixEditor.tempoType
                            onValueChanged: hueMatrixEditor.tempoType = value
                        }
                        Item
                        {
                            width: UISettings.listItemHeight
                            height: width
                        }

                        // Row 5
                        RobotoText
                        {
                            label: qsTr("Step duration")
                            height: UISettings.listItemHeight
                        }
                        Rectangle
                        {
                            Layout.fillWidth: true
                            height: UISettings.listItemHeight
                            color: UISettings.bgMedium

                            RobotoText
                            {
                                x: 3
                                height: parent.height
                                label: {
                                    var dur = hueMatrixEditor.duration
                                    if (dur === -2) return "∞"
                                    return TimeUtils.timeToQlcString(dur, hueMatrixEditor.tempoType)
                                }
                            }
                        }
                        Item
                        {
                            width: UISettings.listItemHeight
                            height: width
                        }
                    }
            }

            SectionBox
            {
                id: directionSection
                width: editorColumn.colWidth - 5
                sectionLabel: qsTr("Order and direction")
                sectionContents:
                    GridLayout
                    {
                        width: parent.width
                        columns: 4
                        columnSpacing: 4
                        rowSpacing: 4

                        // Row 1
                        IconPopupButton
                        {
                            model: [
                                { mLabel: qsTr("Loop"), faIcon: FontAwesome.fa_retweet, mValue: QLCFunction.Loop },
                                { mLabel: qsTr("Single Shot"), faIcon: FontAwesome.fa_right_long, mValue: QLCFunction.SingleShot },
                                { mLabel: qsTr("Ping Pong"), faIcon: FontAwesome.fa_right_left, mValue: QLCFunction.PingPong }
                            ]

                            currValue: hueMatrixEditor.runOrder
                            onValueChanged: hueMatrixEditor.runOrder = value
                        }
                        RobotoText
                        {
                            label: qsTr("Run Order")
                            Layout.fillWidth: true
                        }

                        IconPopupButton
                        {
                            model: [
                                { mLabel: qsTr("Forward"), faIcon: FontAwesome.fa_angles_right, mValue: QLCFunction.Forward },
                                { mLabel: qsTr("Backward"), faIcon: FontAwesome.fa_angles_left, mValue: QLCFunction.Backward }
                            ]

                            currValue: hueMatrixEditor.direction
                            onValueChanged: hueMatrixEditor.direction = value
                        }
                        RobotoText
                        {
                            label: qsTr("Direction")
                            Layout.fillWidth: true
                        }

                    } // GridLayout
            }
        } // Column
        ScrollBar.vertical: CustomScrollBar { id: sbar }
    } // Flickable

    /* *************************************************************
     * Here starts all the Algorithm-specific Component definitions,
     * loaded at runtime depending on the selected algorithm
     * *********************************************************** */

    /* *************************************************************
     * **************** Text Algorithm parameters **************** */
    Component
    {
        id: textAlgoComponent
        GridLayout
        {
            columns: 2
            columnSpacing: 5

            // Row 1
            RobotoText
            {
                height: UISettings.listItemHeight
                label: qsTr("Text")
            }

            Rectangle
            {
                Layout.fillWidth: true
                height: editorColumn.itemsHeight
                color: "transparent"

                Rectangle
                {
                    height: parent.height
                    width: parent.width - fontButton.width - 5
                    radius: 3
                    color: UISettings.bgMedium
                    border.color: "#222"

                    TextInput
                    {
                        id: algoTextEdit
                        anchors.fill: parent
                        anchors.margins: 4
                        anchors.verticalCenter: parent.verticalCenter
                        text: hueMatrixEditor.algoText
                        font.family: hueMatrixEditor.algoTextFont.font.family
                        font.bold: hueMatrixEditor.algoTextFont.font.bold
                        font.italic: hueMatrixEditor.algoTextFont.font.italic
                        font.pixelSize: UISettings.textSizeDefault * 0.8
                        color: "white"

                        onTextEdited: hueMatrixEditor.algoText = text
                    }
                }
                IconButton
                {
                    id: fontButton
                    width: UISettings.iconSizeMedium
                    height: width
                    anchors.right: parent.right
                    faSource: FontAwesome.fa_font
                    faColor: "lightcyan"

                    onClicked: fontDialog.visible = true

                    FontDialog
                    {
                        id: fontDialog
                        title: qsTr("Please choose a font")
                        selectedFont: hueMatrixEditor.algoTextFont
                        visible: false

                        onAccepted:
                        {
                            console.log("Selected font: " + selectedFont)
                            hueMatrixEditor.algoTextFont = selectedFont
                        }
                    }
                }
            }

            // Row 2
            RobotoText
            {
                height: UISettings.listItemHeight
                label: qsTr("Animation")
            }
            CustomComboBox
            {
                Layout.fillWidth: true
                height: editorColumn.itemsHeight
                model: [
                    { mLabel: qsTr("Letters") },
                    { mLabel: qsTr("Horizontal") },
                    { mLabel: qsTr("Vertical") }
                ]

                currentIndex: hueMatrixEditor.animationStyle
                onCurrentIndexChanged: hueMatrixEditor.animationStyle = currentIndex
            }

            // Row 3
            RobotoText
            {
                height: UISettings.listItemHeight
                label: qsTr("Offset")
            }
            Rectangle
            {
                Layout.fillWidth: true
                height: editorColumn.itemsHeight
                color: "transparent"

                Row
                {
                    id: toffRow
                    spacing: 20
                    anchors.fill: parent

                    property size algoOffset: hueMatrixEditor.algoOffset

                    RobotoText { height: UISettings.listItemHeight; label: qsTr("X") }
                    CustomSpinBox
                    {
                        height: parent.height
                        from: -255
                        to: 255
                        value: toffRow.algoOffset.width
                        onValueModified:
                        {
                            var newOffset = toffRow.algoOffset
                            newOffset.width = value
                            hueMatrixEditor.algoOffset = newOffset
                        }
                    }

                    RobotoText { height: UISettings.listItemHeight; label: qsTr("Y") }
                    CustomSpinBox
                    {
                        height: parent.height
                        from: -255
                        to: 255
                        value: toffRow.algoOffset.height
                        onValueModified:
                        {
                            var newOffset = toffRow.algoOffset
                            newOffset.height = value
                            hueMatrixEditor.algoOffset = newOffset
                        }
                    }
                }
            }
        }
    }

    /* *************************************************************
     * **************** Image Algorithm parameters *************** */
    Component
    {
        id: imageAlgoComponent

        GridLayout
        {
            id: imageAlgoGrid
            columns: 2
            columnSpacing: 5

            // Row 1
            RobotoText
            {
                height: UISettings.listItemHeight
                label: qsTr("Image")
            }
            Rectangle
            {
                Layout.fillWidth: true
                height: editorColumn.itemsHeight
                color: "transparent"

                Rectangle
                {
                    height: parent.height
                    width: parent.width - imgButton.width - 5
                    radius: 3
                    color: UISettings.bgMedium
                    border.color: UISettings.bgStrong
                    clip: true

                    TextInput
                    {
                        id: algoTextEdit
                        anchors.fill: parent
                        anchors.margins: 4
                        anchors.verticalCenter: parent.verticalCenter
                        text: hueMatrixEditor.algoImagePath
                        font.pixelSize: UISettings.textSizeDefault
                        color: "white"

                        onTextEdited: hueMatrixEditor.algoImagePath = text
                    }
                }
                IconButton
                {
                    id: imgButton
                    width: UISettings.iconSizeMedium
                    height: width
                    anchors.right: parent.right
                    faSource: FontAwesome.fa_image
                    faColor: "lightyellow"
                    tooltip: qsTr("Set a custom background")

                    onClicked: fileDialog.visible = true

                    FileDialog
                    {
                        id: fileDialog
                        visible: false
                        title: qsTr("Select an image")
                        nameFilters: [ "Image files (*.png *.bmp *.jpg *.jpeg *.gif)", "All files (*)" ]

                        onAccepted: hueMatrixEditor.algoImagePath = fileDialog.selectedFile
                    }
                }
            }

            // Row 2
            RobotoText
            {
                height: UISettings.listItemHeight
                label: qsTr("Animation")
            }
            CustomComboBox
            {
                Layout.fillWidth: true
                height: editorColumn.itemsHeight
                model: [
                    { mLabel: qsTr("Static") },
                    { mLabel: qsTr("Horizontal") },
                    { mLabel: qsTr("Vertical") },
                    { mLabel: qsTr("Animation") }
                ]

                currentIndex: hueMatrixEditor.animationStyle
                onCurrentIndexChanged: hueMatrixEditor.animationStyle = currentIndex
            }

            // Row 3
            RobotoText
            {
                height: UISettings.listItemHeight
                label: qsTr("Offset")
            }
            Rectangle
            {
                Layout.fillWidth: true
                height: editorColumn.itemsHeight
                color: "transparent"

                Row
                {
                    id: ioffRow
                    spacing: 20
                    anchors.fill: parent

                    property size algoOffset: hueMatrixEditor.algoOffset

                    RobotoText { height: UISettings.listItemHeight; label: qsTr("X") }
                    CustomSpinBox
                    {
                        height: parent.height
                        from: -255
                        to: 255
                        value: ioffRow.algoOffset.width
                        onValueModified:
                        {
                            var newOffset = ioffRow.algoOffset
                            newOffset.width = value
                            hueMatrixEditor.algoOffset = newOffset
                        }
                    }

                    RobotoText { height: UISettings.listItemHeight; label: qsTr("Y") }
                    CustomSpinBox
                    {
                        height: parent.height
                        from: -255
                        to: 255
                        value: ioffRow.algoOffset.height
                        onValueModified:
                        {
                            var newOffset = ioffRow.algoOffset
                            newOffset.height = value
                            hueMatrixEditor.algoOffset = newOffset
                        }
                    }
                }
            }
        }
    }

    /* ************************************************************ */
    /* ***************  Script Algorithm parameters *************** */
    Component
    {
        id: scriptAlgoComponent

        GridLayout
        {
            id: scriptAlgoGrid
            columns: 2
            columnSpacing: 5

            function addLabel(text)
            {
                labelComponent.createObject(scriptAlgoGrid,
                               {"propName": text });
                if (labelComponent.status !== Component.Ready)
                    console.log("Label component is not ready !!")
            }

            function addComboBox(propName, model, currentIndex)
            {
                comboComponent.createObject(scriptAlgoGrid,
                               {"propName": propName, "model": model, "currentIndex": currentIndex });
                if (comboComponent.status !== Component.Ready)
                    console.log("Combo component is not ready !!")
            }

            function addSpinBox(propName, min, max, currentValue)
            {
                spinComponent.createObject(scriptAlgoGrid,
                              {"propName": propName, "from": min, "to": max, "value": currentValue });
                if (spinComponent.status !== Component.Ready)
                    console.log("Spin component is not ready !!")
            }

            function addDoubleSpinBox(propName, currentValue)
            {
                doubleSpinComponent.createObject(scriptAlgoGrid,
                              {"propName": propName, "realValue": currentValue });
                if (spinComponent.status !== Component.Ready)
                    console.log("Double spin component is not ready !!")
            }

            function addTextEdit(propName, currentText)
            {
                textEditComponent.createObject(scriptAlgoGrid,
                               {"propName": propName, "text": currentText });
                if (comboComponent.status !== Component.Ready)
                    console.log("TextEdit component is not ready !!")
            }

            Component.onCompleted:
            {
                hueMatrixEditor.createScriptObjects(scriptAlgoGrid)
            }
        }
    }

    // Script algorithm text label
    Component
    {
        id: labelComponent

        RobotoText
        {
            implicitHeight: UISettings.listItemHeight
            implicitWidth: width
            property string propName

            label: propName
        }
    }

    // Script algorithm combo box property
    Component
    {
        id: comboComponent

        CustomComboBox
        {
            Layout.fillWidth: true
            property string propName

            onCurrentTextChanged: hueMatrixEditor.setScriptStringProperty(propName, currentText)
        }
    }

    // Script algorithm spin box property
    Component
    {
        id: spinComponent

        CustomSpinBox
        {
            Layout.fillWidth: true
            property string propName

            onValueModified: hueMatrixEditor.setScriptIntProperty(propName, value)
        }
    }

    // Script algorithm float box property
    Component
    {
        id: doubleSpinComponent

        CustomDoubleSpinBox
        {
            Layout.fillWidth: true
            property string propName

            decimals: 3
            suffix: ""
            onRealValueChanged: hueMatrixEditor.setScriptFloatProperty(propName, realValue)
        }
    }

    // Script algorithm combo box property
    Component
    {
        id: textEditComponent

        CustomTextEdit
        {
            Layout.fillWidth: true
            property string propName

            onTextEdited: hueMatrixEditor.setScriptStringProperty(propName, text)
        }
    }
}
