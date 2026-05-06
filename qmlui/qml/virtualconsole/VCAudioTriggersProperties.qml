/*
  Q Light Controller Plus
  VCAudioTriggersProperties.qml

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

import org.qlcplus.classes 1.0
import "."

Rectangle
{
    id: propsRoot
    color: "transparent"
    height: audioTriggerPropsColumn.height

    property VCAudioTriggers widgetRef: null

    property int gridItemsHeight: UISettings.listItemHeight
    property var perceptualBandNames: [ qsTr("Sub"), qsTr("Bass"), qsTr("Low Mid"), qsTr("Mid"), qsTr("High") ]
    property var perceptualBandColors: [ "#ff3333", "#ff9900", "#ffdd33", "#33cc66", "#33ccff" ]

    function bandPower(index)
    {
        if (!widgetRef)
            return 0

        switch (index)
        {
        case 0: return widgetRef.subPower
        case 1: return widgetRef.bassPower
        case 2: return widgetRef.lowMidPower
        case 3: return widgetRef.midPower
        case 4: return widgetRef.highPower
        default: return 0
        }
    }

    function percentText(value)
    {
        return Math.round(Math.max(0, Math.min(1, value)) * 100) + "%"
    }

    CustomPopupDialog
    {
        id: thresholdsPopup
        width: mainView.width / 3

        property alias tMin: minThresholdSpin.value
        property alias tMax: maxThresholdSpin.value

        onOpened: maxThresholdSpin.focus = true
        onAccepted: widgetRef.setBarThresholds(tMin, tMax)

        contentItem:
            GridLayout
            {
                width: parent.width
                height: UISettings.iconSizeDefault * rows
                columns: 2
                columnSpacing: 5

                // Row 1
                RobotoText
                {
                    label: qsTr("Activation threshold")
                }

                CustomSpinBox
                {
                    id: maxThresholdSpin
                    Layout.fillWidth: true
                    suffix: "%"
                    from: 5
                    to: 95
                }

                // Row 2
                RobotoText
                {
                    label: qsTr("Deactivation threshold")
                }

                CustomSpinBox
                {
                    id: minThresholdSpin
                    Layout.fillWidth: true
                    suffix: "%"
                    from: 5
                    to: 95
                }
            }
    }

    CustomPopupDialog
    {
        id: renamePopup
        width: mainView.width / 3
        title: qsTr("Rename Audio Profile")

        onAccepted: if (widgetRef) widgetRef.renameCurrentProfile(renameField.text)

        contentItem:
            Column
            {
                width: parent.width
                spacing: 5

                RobotoText
                {
                    label: qsTr("New name")
                }

                TextInput
                {
                    id: renameField
                    width: parent.width
                    height: UISettings.listItemHeight
                    color: "white"
                    text: ""
                    font.pixelSize: UISettings.textSizeDefault
                    selectByMouse: true
                }
            }
    }

    Column
    {
        id: audioTriggerPropsColumn
        width: parent.width
        spacing: 5

        SectionBox
        {
            sectionLabel: qsTr("Audio Profile")

            sectionContents:
                GridLayout
                {
                    width: parent.width
                    columns: 2
                    columnSpacing: 6
                    rowSpacing: 4

                    RobotoText
                    {
                        height: gridItemsHeight
                        label: qsTr("Profile")
                    }

                    CustomComboBox
                    {
                        Layout.fillWidth: true
                        height: gridItemsHeight
                        model: widgetRef ? widgetRef.profileListModel : null
                        textRole: "profileName"
                        currentIndex: {
                            if (!widgetRef || !widgetRef.profileListModel) return 0
                            var m = widgetRef.profileListModel
                            for (var i = 0; i < m.rowCount(); i++) {
                                if (m.data(m.index(i, 0), 0x101) === widgetRef.audioProfileId)
                                    return i
                            }
                            return 0
                        }
                        onActivated: {
                            if (widgetRef && widgetRef.profileListModel) {
                                var m = widgetRef.profileListModel
                                var id = m.data(m.index(currentIndex, 0), 0x101)
                                widgetRef.audioProfileId = id
                            }
                        }
                    }

                    RobotoText
                    {
                        Layout.columnSpan: 2
                        height: gridItemsHeight
                        fontSize: UISettings.textSizeSmall
                        label: qsTr("Changes affect all users of this profile")
                        labelColor: "#888888"
                    }

                    // Profile management buttons
                    Row
                    {
                        Layout.columnSpan: 2
                        Layout.fillWidth: true
                        spacing: 4

                        GenericButton
                        {
                            width: (parent.width - 12) / 4
                            height: gridItemsHeight
                            label: qsTr("Reset")
                            onClicked: if (widgetRef) widgetRef.resetProfileToDefaults()
                        }
                        GenericButton
                        {
                            width: (parent.width - 12) / 4
                            height: gridItemsHeight
                            label: qsTr("Duplicate")
                            onClicked: {
                                if (widgetRef)
                                    widgetRef.duplicateCurrentProfile("")
                            }
                        }
                        GenericButton
                        {
                            width: (parent.width - 12) / 4
                            height: gridItemsHeight
                            label: qsTr("Rename")
                            onClicked: {
                                if (widgetRef)
                                    renamePopup.open()
                            }
                        }
                        GenericButton
                        {
                            width: (parent.width - 12) / 4
                            height: gridItemsHeight
                            label: qsTr("Delete")
                            onClicked: if (widgetRef) widgetRef.deleteCurrentProfile()
                        }
                    }
                }
        }

        SectionBox
        {
            id: audioTriggerProp
            sectionLabel: qsTr("Spectrum Bars")

            sectionContents:
                GridLayout
                {
                    width: parent.width
                    columns: 2
                    columnSpacing: 5
                    rowSpacing: 4

                    // row 1
                    RobotoText
                    {
                        height: gridItemsHeight
                        label: qsTr("Number of bars")
                    }

                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        height: gridItemsHeight
                        value: widgetRef ? widgetRef.barsNumber - 1 : 0
                        onValueModified: if (widgetRef) widgetRef.barsNumber = value + 1
                    }

                    // row 2
                    ListView
                    {
                        id: barsList
                        Layout.columnSpan: 2
                        Layout.fillWidth: true
                        clip: true
                        //implicitWidth: audioTriggerPropsColumn.width
                        implicitHeight: count * gridItemsHeight
                        boundsBehavior: Flickable.StopAtBounds
                        headerPositioning: ListView.OverlayHeader
                        model: widgetRef ? widgetRef.barsInfo : null

                        property Item currentChecked: null
                        property int currentType: VCAudioTriggers.None

                        header:
                            RowLayout
                            {
                                z: 2
                                width: barsList.width
                                height: gridItemsHeight

                                RobotoText
                                {
                                    width: UISettings.bigItemHeight * 1.8
                                    height: gridItemsHeight
                                    label: qsTr("Name")
                                    color: UISettings.sectionHeader
                                }
                                Rectangle { width: 1; height: gridItemsHeight }

                                RobotoText
                                {
                                    width: UISettings.bigItemHeight + gridItemsHeight + 10
                                    height: gridItemsHeight
                                    label: qsTr("Type")
                                    color: UISettings.sectionHeader
                                }

                                Rectangle { width: 1; height: gridItemsHeight }

                                RobotoText
                                {
                                    Layout.fillWidth: true
                                    height: gridItemsHeight
                                    label: qsTr("Information")
                                    color: UISettings.sectionHeader
                                }
                            }

                        delegate:
                            Row
                            {
                                width: barsList.width
                                height: modelData.type === VCAudioTriggers.FunctionBar ||
                                        modelData.type === VCAudioTriggers.VCWidgetBar ? gridItemsHeight * 2 : gridItemsHeight
                                spacing: 10

                                RobotoText
                                {
                                    width: UISettings.bigItemHeight * 1.8
                                    height: gridItemsHeight
                                    label: modelData.bLabel
                                }
                                CustomComboBox
                                {
                                    width: UISettings.bigItemHeight
                                    height: gridItemsHeight
                                    model: [
                                        { mLabel: qsTr("None"), faIcon: FontAwesome.fa_ban },
                                        { mLabel: qsTr("DMX"), faIcon: FontAwesome.fa_sliders },
                                        { mLabel: qsTr("Function"), faIcon: FontAwesome.fa_cubes },
                                        { mLabel: qsTr("Widget"), faIcon: FontAwesome.fa_table_list }
                                    ]
                                    currentIndex: modelData.type

                                    onActivated:
                                    {
                                        if (widgetRef)
                                        {
                                            widgetRef.selectedBar = modelData.index
                                            widgetRef.setBarType(currentIndex)
                                        }
                                    }
                                }
                                Rectangle
                                {
                                    visible: modelData.type === VCAudioTriggers.None
                                    width: height
                                    height: gridItemsHeight
                                    color: "transparent"
                                }
                                IconButton
                                {
                                    visible: modelData.type !== VCAudioTriggers.None
                                    width: height
                                    height: gridItemsHeight
                                    faSource: FontAwesome.fa_pen_to_square
                                    checkable: true
                                    checked: widgetRef && widgetRef.selectedBar === modelData.index && sideLoader.visible
                                    onCheckedChanged:
                                    {
                                        widgetRef.selectedBar = modelData.index
                                        if (checked)
                                        {
                                            if (!sideLoader.visible)
                                                rightSidePanel.width += UISettings.sidePanelWidth
                                            sideLoader.visible = true
                                            sideLoader.modelProvider = widgetRef
                                            if (modelData.type === VCAudioTriggers.DMXBar)
                                                sideLoader.source = "qrc:/FixtureGroupManager.qml"
                                            else if (modelData.type === VCAudioTriggers.FunctionBar)
                                                sideLoader.source = "qrc:/FunctionManager.qml"
                                            else if (modelData.type === VCAudioTriggers.VCWidgetBar)
                                                sideLoader.source = "qrc:/VCWidgetsList.qml"
                                            barsList.currentChecked = this
                                            barsList.currentType = modelData.type
                                        }
                                        else
                                        {
                                            rightSidePanel.width -= sideLoader.width
                                            sideLoader.source = ""
                                            sideLoader.visible = false
                                            barsList.currentType = VCAudioTriggers.None
                                        }
                                    }
                                }

                                RobotoText
                                {
                                    visible: modelData.type !== VCAudioTriggers.None
                                    width: UISettings.bigItemHeight * 2
                                    height: gridItemsHeight
                                    clip: false
                                    color: thresholdsMa.containsMouse ? UISettings.bgLight : "transparent"
                                    label: modelData.type === VCAudioTriggers.DMXBar ?
                                               modelData.intVal + " " + qsTr("Channels") :
                                               qsTr("Thresholds:") + " " + modelData.minThreshold + "% - " + modelData.maxThreshold + "%"

                                    MouseArea
                                    {
                                        id: thresholdsMa
                                        enabled: modelData.type === VCAudioTriggers.FunctionBar ||
                                                 modelData.type === VCAudioTriggers.VCWidgetBar
                                        width: parent.width
                                        height: gridItemsHeight
                                        hoverEnabled: true
                                        onClicked:
                                        {
                                            widgetRef.selectedBar = modelData.index
                                            thresholdsPopup.tMin = Math.round(modelData.minThreshold)
                                            thresholdsPopup.tMax = Math.round(modelData.maxThreshold)
                                            thresholdsPopup.open()
                                        }
                                    }

                                    IconTextEntry
                                    {
                                        visible: modelData.type === VCAudioTriggers.FunctionBar
                                        y: gridItemsHeight
                                        height: gridItemsHeight
                                        width: parent.width

                                        property QLCFunction func: functionManager.getFunction(modelData.intVal)
                                        tLabel: func ? func.name : ""
                                        functionType: func ? func.type : -1
                                    }

                                    IconTextEntry
                                    {
                                        visible: modelData.type === VCAudioTriggers.VCWidgetBar
                                        y: gridItemsHeight
                                        height: gridItemsHeight
                                        width: parent.width
                                        iSrc: modelData.iconVal ? modelData.iconVal : ""
                                        tLabel: modelData.strVal ? modelData.strVal : ""
                                    }
                                }
                            }

                        Rectangle
                        {
                            id: addFunctionBox
                            visible: barsList.currentType === VCAudioTriggers.FunctionBar
                            anchors.fill: barsList
                            color: addFunctionDrop.containsDrag ? UISettings.activeDropArea : UISettings.bgMedium
                            opacity: 0.9
                            radius: 10

                            RobotoText
                            {
                                id: afText
                                anchors.centerIn: parent
                                label: qsTr("Drop a Function here")
                                labelColor: addFunctionDrop.containsDrag ? UISettings.bgStronger : UISettings.fgMain
                                fontBold: addFunctionDrop.containsDrag ? true : false
                            }

                            DropArea
                            {
                                id: addFunctionDrop
                                anchors.fill: parent

                                keys: [ "function" ]

                                onDropped:
                                {
                                    console.log("Function item dropped here. x: " + drag.x + " y: " + drag.y)

                                    if (drag.source.hasOwnProperty("fromFunctionManager"))
                                    {
                                        barsList.currentChecked.checked = false
                                        widgetRef.setBarFunction(drag.source.itemsList[0])
                                    }
                                }
                            }
                        }

                        Rectangle
                        {
                            id: addWidgetBox
                            visible: barsList.currentType === VCAudioTriggers.VCWidgetBar
                            anchors.fill: barsList
                            color: addWidgetDrop.containsDrag ? UISettings.activeDropArea : UISettings.bgMedium
                            opacity: 0.9
                            radius: 10

                            RobotoText
                            {
                                anchors.centerIn: parent
                                label: qsTr("Drop a VC Widget here")
                                labelColor: addWidgetDrop.containsDrag ? UISettings.bgStronger : UISettings.fgMain
                                fontBold: addWidgetDrop.containsDrag ? true : false
                            }

                            DropArea
                            {
                                id: addWidgetDrop
                                anchors.fill: parent

                                keys: [ "audiotriggerswidget" ]

                                onDropped:
                                {
                                    if (drag.source.hasOwnProperty("fromVCWidgetsList")
                                            && drag.source.itemsList.length)
                                    {
                                        barsList.currentChecked.checked = false
                                        widgetRef.setBarWidget(drag.source.itemsList[0])
                                    }
                                }
                            }
                        }
                    } // ListView
                } // GridLayout
        } // SectionBox

        SectionBox
        {
            sectionLabel: qsTr("QLC+: Perceptual Bands")

            sectionContents:
                Column
                {
                    width: parent.width
                    spacing: 4

                    Repeater
                    {
                        model: 5

                        RowLayout
                        {
                            width: parent.width
                            height: gridItemsHeight
                            spacing: 6

                            RobotoText
                            {
                                Layout.preferredWidth: UISettings.bigItemHeight
                                height: gridItemsHeight
                                label: perceptualBandNames[index]
                                labelColor: perceptualBandColors[index]
                            }

                            Rectangle
                            {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 10
                                radius: 4
                                color: UISettings.bgStrong
                                border.width: 1
                                border.color: UISettings.bgLight

                                Rectangle
                                {
                                    anchors.left: parent.left
                                    anchors.top: parent.top
                                    anchors.bottom: parent.bottom
                                    width: parent.width * Math.max(0, Math.min(1, bandPower(index)))
                                    radius: parent.radius
                                    color: perceptualBandColors[index]
                                }
                            }

                            RobotoText
                            {
                                Layout.preferredWidth: UISettings.bigItemHeight
                                height: gridItemsHeight
                                label: percentText(bandPower(index))
                            }
                        }
                    }
                }
        }

        SectionBox
        {
            sectionLabel: qsTr("QLC+: Response — Envelope")

            sectionContents:
                Column
                {
                    width: parent.width
                    spacing: 4

                    RowLayout
                    {
                        width: parent.width
                        height: gridItemsHeight
                        spacing: 6

                        RobotoText
                        {
                            Layout.preferredWidth: UISettings.bigItemHeight
                            height: gridItemsHeight
                            label: qsTr("Attack")
                            tooltipText: qsTr("Attack time (ms) of the per-band envelope follower. Lower = snappier reaction; higher = smoother but laggier band-power signals driving widget triggers.")
                        }

                        Slider
                        {
                            Layout.fillWidth: true
                            from: 0
                            to: 500
                            stepSize: 1
                            enabled: widgetRef !== null
                            value: widgetRef ? widgetRef.envelopeAttack : 0
                            onMoved: if (widgetRef) widgetRef.setEnvelopeAttack(value)
                        }

                        CustomSpinBox
                        {
                            Layout.preferredWidth: UISettings.bigItemHeight
                            from: 0
                            to: 500
                            suffix: " ms"
                            enabled: widgetRef !== null
                            value: widgetRef ? Math.round(widgetRef.envelopeAttack) : 0
                            onValueModified: if (widgetRef) widgetRef.setEnvelopeAttack(value)
                        }
                    }

                    RowLayout
                    {
                        width: parent.width
                        height: gridItemsHeight
                        spacing: 6

                        RobotoText
                        {
                            Layout.preferredWidth: UISettings.bigItemHeight
                            height: gridItemsHeight
                            label: qsTr("Release")
                            tooltipText: qsTr("Release time (ms) of the per-band envelope follower. Higher = bands hang on longer after a hit; lower = bands fall off faster.")
                        }

                        Slider
                        {
                            Layout.fillWidth: true
                            from: 0
                            to: 2000
                            stepSize: 1
                            enabled: widgetRef !== null
                            value: widgetRef ? widgetRef.envelopeRelease : 0
                            onMoved: if (widgetRef) widgetRef.setEnvelopeRelease(value)
                        }

                        CustomSpinBox
                        {
                            Layout.preferredWidth: UISettings.bigItemHeight
                            from: 0
                            to: 2000
                            suffix: " ms"
                            enabled: widgetRef !== null
                            value: widgetRef ? Math.round(widgetRef.envelopeRelease) : 0
                            onValueModified: if (widgetRef) widgetRef.setEnvelopeRelease(value)
                        }
                    }
                }
        }

        SectionBox
        {
            sectionLabel: qsTr("QLC+: Band Grouping")

            sectionContents:
                GridLayout
                {
                    width: parent.width
                    columns: 2
                    columnSpacing: 6
                    rowSpacing: 4

                    RobotoText { height: gridItemsHeight; label: qsTr("Sub ≤"); tooltipText: qsTr("Upper mel-band index for the Sub band (very low frequencies, ~20–60 Hz). Bands at and below this index are routed to Sub.") }
                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        from: 20; to: 5000; suffix: " Hz"
                        enabled: widgetRef !== null
                        value: widgetRef ? Math.round(widgetRef.bandSubMaxHz) : 60
                        onValueModified: if (widgetRef) widgetRef.setBandSubMaxHz(value)
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Bass ≤"); tooltipText: qsTr("Upper mel-band index for the Bass band (kick / low end). Bands above Sub up to this index are routed to Bass.") }
                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        from: 20; to: 5000; suffix: " Hz"
                        enabled: widgetRef !== null
                        value: widgetRef ? Math.round(widgetRef.bandBassMaxHz) : 250
                        onValueModified: if (widgetRef) widgetRef.setBandBassMaxHz(value)
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Low Mid ≤"); tooltipText: qsTr("Upper mel-band index for the Low-Mid band (warmth / lower vocals).") }
                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        from: 20; to: 5000; suffix: " Hz"
                        enabled: widgetRef !== null
                        value: widgetRef ? Math.round(widgetRef.bandLowMidMaxHz) : 500
                        onValueModified: if (widgetRef) widgetRef.setBandLowMidMaxHz(value)
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Mid ≤"); tooltipText: qsTr("Upper mel-band index for the Mid band (vocals / instruments).") }
                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        from: 20; to: 5000; suffix: " Hz"
                        enabled: widgetRef !== null
                        value: widgetRef ? Math.round(widgetRef.bandMidMaxHz) : 2000
                        onValueModified: if (widgetRef) widgetRef.setBandMidMaxHz(value)
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("High ≤"); tooltipText: qsTr("Upper mel-band index for the High band (cymbals / air / sibilance). Everything above this is unused.") }
                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        from: 20; to: 5000; suffix: " Hz"
                        enabled: widgetRef !== null
                        value: widgetRef ? Math.round(widgetRef.bandHighMaxHz) : 5000
                        onValueModified: if (widgetRef) widgetRef.setBandHighMaxHz(value)
                    }
                }
        }

        SectionBox
        {
            sectionLabel: qsTr("QLC+: Input Gain (pre-aubio)")

            sectionContents:
                GridLayout
                {
                    width: parent.width
                    columns: 2
                    columnSpacing: 6
                    rowSpacing: 4

                    RobotoText
                    {
                        height: gridItemsHeight
                        label: qsTr("Input gain")
                        tooltipText: qsTr("Linear gain applied to the captured signal before all DSP. Use this to normalize quiet/loud sources without retuning every threshold.")
                    }
                    RowLayout
                    {
                        Layout.fillWidth: true
                        spacing: 6
                        Slider
                        {
                            Layout.fillWidth: true
                            from: 0.1; to: 4.0; stepSize: 0.1
                            enabled: widgetRef !== null
                            value: widgetRef ? widgetRef.inputGain : 1.6
                            onPressedChanged: if (!pressed && widgetRef) widgetRef.setInputGain(value)
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Linear gain applied to the captured signal before all DSP. " +
                                               "Use this to normalize quiet/loud sources without retuning every threshold.")
                        }
                        RobotoText
                        {
                            Layout.preferredWidth: UISettings.bigItemHeight
                            height: gridItemsHeight
                            label: widgetRef ? widgetRef.inputGain.toFixed(1) + "x" : "--"
                        }
                    }
                }
        }

        SectionBox
        {
            sectionLabel: qsTr("QLC+: Noise Gate")

            sectionContents:
                GridLayout
                {
                    width: parent.width
                    columns: 2
                    columnSpacing: 6
                    rowSpacing: 4

                    RobotoText { height: gridItemsHeight; label: qsTr("State"); tooltipText: qsTr("Whether the noise gate is currently passing audio (open) or muting it (closed).") }
                    Row
                    {
                        spacing: 6
                        height: gridItemsHeight
                        Rectangle
                        {
                            width: 14; height: 14; radius: 7
                            anchors.verticalCenter: parent.verticalCenter
                            color: widgetRef && widgetRef.noiseGateOpen ? "#33cc66" : "#cc3333"
                            border.width: 1; border.color: "#222222"
                        }
                        RobotoText
                        {
                            height: gridItemsHeight
                            label: widgetRef && widgetRef.noiseGateOpen ? qsTr("Open") : qsTr("Closed")
                        }
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Threshold"); tooltipText: qsTr("RMS level (in dBFS) below which the noise gate closes. Raise to ignore room noise; lower to let quieter audio through.") }
                    RowLayout
                    {
                        Layout.fillWidth: true
                        spacing: 6
                        Slider
                        {
                            Layout.fillWidth: true
                            from: -96; to: 0; stepSize: 1
                            enabled: widgetRef !== null
                            value: widgetRef ? widgetRef.noiseGateThreshold : -54
                            onPressedChanged: if (!pressed && widgetRef) widgetRef.setNoiseGateThreshold(value)
                        }
                        CustomSpinBox
                        {
                            Layout.preferredWidth: UISettings.bigItemHeight
                            from: -96; to: 0; suffix: " dB"
                            enabled: widgetRef !== null
                            value: widgetRef ? Math.round(widgetRef.noiseGateThreshold) : -54
                            onValueModified: if (widgetRef) widgetRef.setNoiseGateThreshold(value)
                        }
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Hold"); tooltipText: qsTr("How long (ms) the gate stays open after the signal drops below threshold. Prevents chatter on percussive material.") }
                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        from: 0; to: 2000; suffix: " ms"
                        enabled: widgetRef !== null
                        value: widgetRef ? Math.round(widgetRef.noiseGateHold) : 120
                        onValueModified: if (widgetRef) widgetRef.setNoiseGateHold(value)
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Input RMS"); tooltipText: qsTr("Live RMS reading of the post-gain input, in dBFS. Use this to set Threshold above your noise floor.") }
                    RowLayout
                    {
                        Layout.fillWidth: true
                        spacing: 6
                        Rectangle
                        {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 10
                            radius: 4
                            color: UISettings.bgStrong
                            border.width: 1; border.color: UISettings.bgLight
                            Rectangle
                            {
                                anchors.left: parent.left
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                width: parent.width * Math.max(0, Math.min(1, (96 + (widgetRef ? widgetRef.rmsDb : -96)) / 96))
                                radius: parent.radius
                                color: widgetRef && widgetRef.noiseGateOpen ? "#33cc66" : "#cc3333"
                            }
                        }
                        RobotoText
                        {
                            Layout.preferredWidth: UISettings.bigItemHeight
                            height: gridItemsHeight
                            label: widgetRef ? widgetRef.rmsDb.toFixed(0) + " dB" : "--"
                        }
                    }
                }
        }

        SectionBox
        {
            sectionLabel: qsTr("QLC+: Triggers")

            sectionContents:
                RowLayout
                {
                    width: parent.width
                    height: gridItemsHeight * 2
                    spacing: 8

                    Repeater
                    {
                        model: 5

                        Column
                        {
                            Layout.fillWidth: true
                            spacing: 4

                            Rectangle
                            {
                                width: 14
                                height: 14
                                radius: 7
                                anchors.horizontalCenter: parent.horizontalCenter
                                color: {
                                    var ts = widgetRef ? widgetRef.triggerStates[index] : null
                                    if (!ts) return "#555555"
                                    if (ts.fired) return "#33ff66"
                                    if (ts.active) return "#33cc66"
                                    if (ts.cooldownMs > 0) return "#cc9933"
                                    return "#555555"
                                }
                                border.width: 1
                                border.color: "#222222"
                            }

                            RobotoText
                            {
                                width: parent.width
                                height: gridItemsHeight
                                label: perceptualBandNames[index]
                                labelColor: perceptualBandColors[index]
                                fontSize: UISettings.textSizeSmall
                                textHAlign: Text.AlignHCenter
                            }
                        }
                    }

                    Column
                    {
                        Layout.fillWidth: true
                        spacing: 4

                        Rectangle
                        {
                            width: 14
                            height: 14
                            radius: 7
                            anchors.horizontalCenter: parent.horizontalCenter
                            color: widgetRef && widgetRef.beatActive ? "#33cc66" : "#555555"
                            border.width: 1
                            border.color: "#222222"
                            Behavior on color { ColorAnimation { duration: 80 } }
                        }

                        RobotoText
                        {
                            width: parent.width
                            height: gridItemsHeight
                            label: qsTr("Beat")
                            textHAlign: Text.AlignHCenter
                        }
                    }

                    GridLayout
                    {
                        Layout.fillWidth: true
                        columns: 2
                        columnSpacing: 6
                        rowSpacing: 4

                        RobotoText
                        {
                            height: gridItemsHeight
                            label: qsTr("High")
                            tooltipText: qsTr("Upper hysteresis threshold (% of band peak). Band must rise above this to fire a trigger.")
                        }

                        CustomSpinBox
                        {
                            Layout.fillWidth: true
                            from: 0
                            to: 100
                            suffix: "%"
                            enabled: widgetRef !== null
                            value: widgetRef ? Math.round(widgetRef.triggerHigh * 100) : 0
                            onValueModified: if (widgetRef) widgetRef.setTriggerHighThreshold(value / 100)
                        }

                        RobotoText
                        {
                            height: gridItemsHeight
                            label: qsTr("Low")
                            tooltipText: qsTr("Lower hysteresis threshold (% of band peak). Band must fall below this before another trigger can fire.")
                        }

                        CustomSpinBox
                        {
                            Layout.fillWidth: true
                            from: 0
                            to: 100
                            suffix: "%"
                            enabled: widgetRef !== null
                            value: widgetRef ? Math.round(widgetRef.triggerLow * 100) : 0
                            onValueModified: if (widgetRef) widgetRef.setTriggerLowThreshold(value / 100)
                        }

                        RobotoText
                        {
                            height: gridItemsHeight
                            label: qsTr("Hold")
                            tooltipText: qsTr("Minimum time (ms) the trigger stays active once fired, even if the band drops back below threshold.")
                        }

                        CustomSpinBox
                        {
                            Layout.fillWidth: true
                            from: 0
                            to: 1000
                            suffix: " ms"
                            enabled: widgetRef !== null
                            value: widgetRef ? Math.round(widgetRef.triggerHold) : 0
                            onValueModified: if (widgetRef) widgetRef.setTriggerHold(value)
                        }

                        RobotoText
                        {
                            height: gridItemsHeight
                            label: qsTr("Cooldown")
                            tooltipText: qsTr("Minimum time (ms) between two consecutive triggers from the same band. Use to avoid retriggering on a sustained note.")
                        }

                        CustomSpinBox
                        {
                            Layout.fillWidth: true
                            from: 0
                            to: 2000
                            suffix: " ms"
                            enabled: widgetRef !== null
                            value: widgetRef ? Math.round(widgetRef.triggerCooldown) : 0
                            onValueModified: if (widgetRef) widgetRef.setTriggerCooldown(value)
                        }
                    }
                }
        }

        SectionBox
        {
            sectionLabel: qsTr("aubio: Onset Detection")

            sectionContents:
                GridLayout
                {
                    width: parent.width
                    columns: 2
                    columnSpacing: 6
                    rowSpacing: 4

                    RobotoText { height: gridItemsHeight; label: qsTr("Threshold"); tooltipText: qsTr("aubio_onset_set_threshold(): peak-picking sensitivity for all 9 onset detectors. Lower = more onsets, higher = stricter.") }
                    RowLayout
                    {
                        Layout.fillWidth: true
                        spacing: 6
                        Slider
                        {
                            Layout.fillWidth: true
                            from: 0; to: 1; stepSize: 0.01
                            enabled: widgetRef !== null
                            value: widgetRef ? widgetRef.onsetThreshold : 0.3
                            onPressedChanged: if (!pressed && widgetRef) widgetRef.setOnsetThreshold(value)
                        }
                        RobotoText
                        {
                            Layout.preferredWidth: UISettings.bigItemHeight
                            height: gridItemsHeight
                            label: widgetRef ? widgetRef.onsetThreshold.toFixed(2) : "--"
                        }
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Min Interval"); tooltipText: qsTr("aubio_onset_set_minioi_ms(): minimum time (ms) between consecutive onsets.") }
                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        from: 0; to: 500; suffix: " ms"
                        enabled: widgetRef !== null
                        value: widgetRef ? Math.round(widgetRef.onsetMinInterval) : 50
                        onValueModified: if (widgetRef) widgetRef.setOnsetMinInterval(value)
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Silence"); tooltipText: qsTr("aubio_onset_set_silence(): dBFS gate. Onsets are suppressed when input level is below this.") }
                    RowLayout
                    {
                        Layout.fillWidth: true
                        spacing: 6
                        Slider
                        {
                            Layout.fillWidth: true
                            from: -120; to: 0; stepSize: 1
                            enabled: widgetRef !== null
                            value: widgetRef ? widgetRef.onsetSilenceDb : -70
                            onPressedChanged: if (!pressed && widgetRef) widgetRef.setOnsetSilenceDb(value)
                        }
                        RobotoText
                        {
                            Layout.preferredWidth: UISettings.bigItemHeight
                            height: gridItemsHeight
                            label: widgetRef ? Math.round(widgetRef.onsetSilenceDb) + " dB" : "--"
                        }
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Delay"); tooltipText: qsTr("aubio_onset_set_delay_ms(): post-detection delay before reporting an onset. 0 = aubio default (~50 ms at 44.1 kHz).") }
                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        from: 0; to: 500; suffix: " ms"
                        enabled: widgetRef !== null
                        value: widgetRef ? Math.round(widgetRef.onsetDelayMs) : 0
                        onValueModified: if (widgetRef) widgetRef.setOnsetDelayMs(value)
                    }
                }
        }

        SectionBox
        {
            sectionLabel: qsTr("aubio: Pitch Detection")

            sectionContents:
                GridLayout
                {
                    width: parent.width
                    columns: 2
                    columnSpacing: 6
                    rowSpacing: 4

                    RobotoText { height: gridItemsHeight; label: qsTr("Method"); tooltipText: qsTr("Pitch detection algorithm. yinfft is the most accurate default; yin is faster; schmitt/fcomb/mcomb work better on tonal sources. Changing this rebuilds the pitch detector.") }
                    CustomComboBox
                    {
                        id: pitchMethodCombo
                        Layout.fillWidth: true
                        height: gridItemsHeight
                        property var methods: ["yinfft", "yin", "yinfast", "schmitt", "fcomb", "mcomb"]
                        model: methods
                        currentIndex: {
                            if (!widgetRef) return 0
                            var idx = methods.indexOf(widgetRef.pitchMethod)
                            return idx >= 0 ? idx : 0
                        }
                        onActivated: if (widgetRef) widgetRef.setPitchMethod(methods[currentIndex])
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Silence Threshold"); tooltipText: qsTr("Below this dBFS level the pitch detector reports no pitch. Set near your noise floor to avoid spurious notes during silence.") }
                    RowLayout
                    {
                        Layout.fillWidth: true
                        spacing: 6
                        Slider
                        {
                            Layout.fillWidth: true
                            from: -80; to: 0; stepSize: 1
                            enabled: widgetRef !== null
                            value: widgetRef ? widgetRef.pitchSilenceDb : -40
                            onPressedChanged: if (!pressed && widgetRef) widgetRef.setPitchSilenceDb(value)
                        }
                        RobotoText
                        {
                            Layout.preferredWidth: UISettings.bigItemHeight
                            height: gridItemsHeight
                            label: widgetRef ? Math.round(widgetRef.pitchSilenceDb) + " dB" : "--"
                        }
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Tolerance"); tooltipText: qsTr("Confidence threshold for accepting a pitch estimate (0–1). Higher = stricter, fewer but cleaner notes; lower = more permissive.") }
                    RowLayout
                    {
                        Layout.fillWidth: true
                        spacing: 6
                        Slider
                        {
                            Layout.fillWidth: true
                            from: 0; to: 1; stepSize: 0.01
                            enabled: widgetRef !== null
                            value: widgetRef ? widgetRef.pitchTolerance : 0.7
                            onPressedChanged: if (!pressed && widgetRef) widgetRef.setPitchTolerance(value)
                        }
                        RobotoText
                        {
                            Layout.preferredWidth: UISettings.bigItemHeight
                            height: gridItemsHeight
                            label: widgetRef ? widgetRef.pitchTolerance.toFixed(2) : "--"
                        }
                    }
                }
        }

        SectionBox
        {
            sectionLabel: qsTr("aubio: Tempo Detection")

            sectionContents:
                GridLayout
                {
                    width: parent.width
                    columns: 2
                    columnSpacing: 6
                    rowSpacing: 4

                    RobotoText { height: gridItemsHeight; label: qsTr("Threshold"); tooltipText: qsTr("aubio_tempo_set_threshold(): peak-picking threshold for the beat tracker. Default 0.3.") }
                    RowLayout
                    {
                        Layout.fillWidth: true
                        spacing: 6
                        Slider
                        {
                            Layout.fillWidth: true
                            from: 0; to: 1; stepSize: 0.01
                            enabled: widgetRef !== null
                            value: widgetRef ? widgetRef.tempoThreshold : 0.3
                            onPressedChanged: if (!pressed && widgetRef) widgetRef.setTempoThreshold(value)
                        }
                        RobotoText
                        {
                            Layout.preferredWidth: UISettings.bigItemHeight
                            height: gridItemsHeight
                            label: widgetRef ? widgetRef.tempoThreshold.toFixed(2) : "--"
                        }
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Silence"); tooltipText: qsTr("aubio_tempo_set_silence(): dBFS gate. Beat detection is suppressed when input level is below this.") }
                    RowLayout
                    {
                        Layout.fillWidth: true
                        spacing: 6
                        Slider
                        {
                            Layout.fillWidth: true
                            from: -120; to: 0; stepSize: 1
                            enabled: widgetRef !== null
                            value: widgetRef ? widgetRef.tempoSilenceDb : -90
                            onPressedChanged: if (!pressed && widgetRef) widgetRef.setTempoSilenceDb(value)
                        }
                        RobotoText
                        {
                            Layout.preferredWidth: UISettings.bigItemHeight
                            height: gridItemsHeight
                            label: widgetRef ? Math.round(widgetRef.tempoSilenceDb) + " dB" : "--"
                        }
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Tatum Subdivision"); tooltipText: qsTr("aubio_tempo_set_tatum_signature(): tatums (sub-beats) per beat. 4 = 16ths, 2 = 8ths, 3 = triplets.") }
                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        from: 1; to: 16
                        enabled: widgetRef !== null
                        value: widgetRef ? widgetRef.tatumSubdivision : 4
                        onValueModified: if (widgetRef) widgetRef.setTatumSubdivision(value)
                    }
                }
        }

        SectionBox
        {
            sectionLabel: qsTr("aubio: Mel Filterbank")

            sectionContents:
                GridLayout
                {
                    width: parent.width
                    columns: 2
                    columnSpacing: 6
                    rowSpacing: 4

                    RobotoText { height: gridItemsHeight; label: qsTr("Bands"); tooltipText: qsTr("Mel band count is fixed at 40 in this build (AUBIO_MEL_BANDS). Filters are Slaney-spaced over 0..Nyquist.") }
                    RobotoText
                    {
                        Layout.fillWidth: true
                        height: gridItemsHeight
                        label: "40"
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Norm"); tooltipText: qsTr("aubio_filterbank_set_norm(): 1 = each filter normalized to unit area (default), 0 = raw triangular weights. Toggling rebuilds the filterbank.") }
                    Switch
                    {
                        Layout.fillWidth: true
                        enabled: widgetRef !== null
                        checked: widgetRef ? widgetRef.filterbankNorm >= 0.5 : true
                        onToggled: if (widgetRef) widgetRef.setFilterbankNorm(checked ? 1.0 : 0.0)
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Power"); tooltipText: qsTr("aubio_filterbank_set_power(): input |X|^power before mel weighting. 1 = magnitude, 2 = power (energy). Higher values emphasize louder bands.") }
                    RowLayout
                    {
                        Layout.fillWidth: true
                        spacing: 6
                        Slider
                        {
                            Layout.fillWidth: true
                            from: 0.1; to: 4.0; stepSize: 0.1
                            enabled: widgetRef !== null
                            value: widgetRef ? widgetRef.filterbankPower : 1.0
                            onPressedChanged: if (!pressed && widgetRef) widgetRef.setFilterbankPower(value)
                        }
                        RobotoText
                        {
                            Layout.preferredWidth: UISettings.bigItemHeight
                            height: gridItemsHeight
                            label: widgetRef ? widgetRef.filterbankPower.toFixed(1) : "--"
                        }
                    }
                }
        }

        SectionBox
        {
            sectionLabel: qsTr("aubio: TSS (Transient/Steady)")

            sectionContents:
                GridLayout
                {
                    width: parent.width
                    columns: 2
                    columnSpacing: 6
                    rowSpacing: 4

                    RobotoText { height: gridItemsHeight; label: qsTr("Alpha"); tooltipText: qsTr("aubio_tss_set_alpha(): controls how aggressively the steady part is subtracted. Higher = more transient leakage suppression.") }
                    RowLayout
                    {
                        Layout.fillWidth: true
                        spacing: 6
                        Slider
                        {
                            Layout.fillWidth: true
                            from: 0; to: 10; stepSize: 0.1
                            enabled: widgetRef !== null
                            value: widgetRef ? widgetRef.tssAlpha : 3.0
                            onPressedChanged: if (!pressed && widgetRef) widgetRef.setTssAlpha(value)
                        }
                        RobotoText
                        {
                            Layout.preferredWidth: UISettings.bigItemHeight
                            height: gridItemsHeight
                            label: widgetRef ? widgetRef.tssAlpha.toFixed(1) : "--"
                        }
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Beta"); tooltipText: qsTr("aubio_tss_set_beta(): controls how aggressively the transient part is subtracted. Symmetric counterpart to alpha.") }
                    RowLayout
                    {
                        Layout.fillWidth: true
                        spacing: 6
                        Slider
                        {
                            Layout.fillWidth: true
                            from: 0; to: 10; stepSize: 0.1
                            enabled: widgetRef !== null
                            value: widgetRef ? widgetRef.tssBeta : 3.0
                            onPressedChanged: if (!pressed && widgetRef) widgetRef.setTssBeta(value)
                        }
                        RobotoText
                        {
                            Layout.preferredWidth: UISettings.bigItemHeight
                            height: gridItemsHeight
                            label: widgetRef ? widgetRef.tssBeta.toFixed(1) : "--"
                        }
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Threshold"); tooltipText: qsTr("aubio_tss_set_threshold(): magnitude threshold below which bins are forced into the steady part.") }
                    RowLayout
                    {
                        Layout.fillWidth: true
                        spacing: 6
                        Slider
                        {
                            Layout.fillWidth: true
                            from: 0; to: 1; stepSize: 0.01
                            enabled: widgetRef !== null
                            value: widgetRef ? widgetRef.tssThreshold : 0.25
                            onPressedChanged: if (!pressed && widgetRef) widgetRef.setTssThreshold(value)
                        }
                        RobotoText
                        {
                            Layout.preferredWidth: UISettings.bigItemHeight
                            height: gridItemsHeight
                            label: widgetRef ? widgetRef.tssThreshold.toFixed(2) : "--"
                        }
                    }
                }
        }

        SectionBox
        {
            sectionLabel: qsTr("QLC+: Volume Response")

            sectionContents:
                GridLayout
                {
                    width: parent.width
                    columns: 2
                    columnSpacing: 6
                    rowSpacing: 4

                    RobotoText { height: gridItemsHeight; label: qsTr("Smoothing") }
                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        from: 0; to: 500; suffix: " ms"
                        enabled: widgetRef !== null
                        value: widgetRef ? Math.round(widgetRef.volumeSmoothing) : 100
                        onValueModified: if (widgetRef) widgetRef.setVolumeSmoothing(value)
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Brightness floor") }
                    RowLayout
                    {
                        Layout.fillWidth: true
                        spacing: 6
                        Slider
                        {
                            Layout.fillWidth: true
                            from: 0; to: 1; stepSize: 0.01
                            enabled: widgetRef !== null
                            value: widgetRef ? widgetRef.brightnessFloor : 0
                            onPressedChanged: if (!pressed && widgetRef) widgetRef.setBrightnessFloor(value)
                        }
                        RobotoText
                        {
                            Layout.preferredWidth: UISettings.bigItemHeight
                            height: gridItemsHeight
                            label: widgetRef ? Math.round(widgetRef.brightnessFloor * 100) + "%" : "--"
                        }
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Raw") }
                    RowLayout
                    {
                        Layout.fillWidth: true
                        spacing: 6
                        Rectangle
                        {
                            Layout.fillWidth: true; Layout.preferredHeight: 10; radius: 4
                            color: UISettings.bgStrong; border.width: 1; border.color: UISettings.bgLight
                            Rectangle
                            {
                                anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom
                                width: parent.width * Math.max(0, Math.min(1, widgetRef ? widgetRef.volumeRaw : 0))
                                radius: parent.radius; color: "#33cc66"
                            }
                        }
                        RobotoText
                        {
                            Layout.preferredWidth: UISettings.bigItemHeight
                            height: gridItemsHeight
                            label: widgetRef ? Math.round(widgetRef.volumeRaw * 100) + "%" : "--"
                        }
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Normalized") }
                    RowLayout
                    {
                        Layout.fillWidth: true
                        spacing: 6
                        Rectangle
                        {
                            Layout.fillWidth: true; Layout.preferredHeight: 10; radius: 4
                            color: UISettings.bgStrong; border.width: 1; border.color: UISettings.bgLight
                            Rectangle
                            {
                                anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom
                                width: parent.width * Math.max(0, Math.min(1, widgetRef ? widgetRef.volumeNormalized : 0))
                                radius: parent.radius; color: UISettings.selection
                            }
                        }
                        RobotoText
                        {
                            Layout.preferredWidth: UISettings.bigItemHeight
                            height: gridItemsHeight
                            label: widgetRef ? Math.round(widgetRef.volumeNormalized * 100) + "%" : "--"
                        }
                    }
                }
        }

        SectionBox
        {
            sectionLabel: qsTr("aubio: Spectral (debug)")

            sectionContents:
                GridLayout
                {
                    width: parent.width
                    columns: 2
                    columnSpacing: 6
                    rowSpacing: 4

                    RobotoText
                    {
                        height: gridItemsHeight
                        label: qsTr("Low cut bin")
                    }

                    RobotoText
                    {
                        Layout.fillWidth: true
                        height: gridItemsHeight
                        label: widgetRef ? widgetRef.lowCutBin : "--"
                    }

                    RobotoText
                    {
                        height: gridItemsHeight
                        label: qsTr("High cut bin")
                    }

                    RobotoText
                    {
                        Layout.fillWidth: true
                        height: gridItemsHeight
                        label: widgetRef ? widgetRef.highCutBin : "--"
                    }

                    RobotoText
                    {
                        height: gridItemsHeight
                        label: qsTr("Spectrum bars")
                    }

                    RobotoText
                    {
                        Layout.fillWidth: true
                        height: gridItemsHeight
                        label: widgetRef ? Math.max(0, widgetRef.barsNumber - 1) : "--"
                    }
                }
        }
    } // Column
}
