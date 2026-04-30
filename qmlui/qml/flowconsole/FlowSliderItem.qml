/*
  Q Light Controller Plus
  FlowSliderItem.qml

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
import "."

FlowWidgetItem
{
    id: sliderRoot
    property VCSlider sliderObj: null
    property int sliderValue: sliderObj ? sliderObj.value : 0
    property int sliderMode: sliderObj ? sliderObj.sliderMode : VCSlider.Adjust

    Layout.preferredHeight: 200
    Layout.minimumHeight: 100
    radius: 2

    onSliderObjChanged: wObj = sliderObj

    Gradient
    {
        id: submasterHandleGradient
        GradientStop { position: 0; color: "#4c4c4c" }
        GradientStop { position: 0.45; color: "#2c2c2c" }
        GradientStop { position: 0.50; color: "#000" }
        GradientStop { position: 0.55; color: "#111111" }
        GradientStop { position: 1.0; color: "#131313" }
    }

    Gradient
    {
        id: submasterHandleGradientHover
        GradientStop { position: 0; color: "#6c6c6c" }
        GradientStop { position: 0.45; color: "#4c4c4c" }
        GradientStop { position: 0.50; color: "#ffff00" }
        GradientStop { position: 0.55; color: "#313131" }
        GradientStop { position: 1.0; color: "#333333" }
    }

    Gradient
    {
        id: grandMasterHandleGradient
        GradientStop { position: 0; color: "#A81919" }
        GradientStop { position: 0.45; color: "#DB2020" }
        GradientStop { position: 0.50; color: "#000" }
        GradientStop { position: 0.55; color: "#DB2020" }
        GradientStop { position: 1.0; color: "#A81919" }
    }

    Gradient
    {
        id: grandMasterHandleGradientHover
        GradientStop { position: 0; color: "#DB2020" }
        GradientStop { position: 0.45; color: "#F51C1C" }
        GradientStop { position: 0.50; color: "#FFF" }
        GradientStop { position: 0.55; color: "#F51C1C" }
        GradientStop { position: 1.0; color: "#DB2020" }
    }

    Rectangle
    {
        visible: sliderObj && sliderObj.monitorEnabled
        y: slFader.y
        x: parent.width - width
        height: slFader.height
        width: UISettings.listItemHeight * 0.2
        rotation: sliderObj ? (sliderObj.invertedAppearance ? 0 : 180) : 180
        color: UISettings.bgLight
        border.width: 1
        border.color: UISettings.bgStrong

        Rectangle
        {
            x: 1
            y: 1
            color: "#00FF00"
            height: sliderObj ? parent.height * (sliderObj.monitorValue / 255) : 0
            width: parent.width - 2
        }
    }

    ColumnLayout
    {
        anchors.fill: parent
        spacing: 2

        Text
        {
            Layout.alignment: Qt.AlignHCenter
            height: UISettings.listItemHeight
            font: sliderObj ? sliderObj.font : Qt.font({ family: UISettings.robotoFontName })
            text: sliderObj ? (sliderObj.valueDisplayStyle === VCSlider.DMXValue ?
                               sliderValue : Math.round((sliderValue * 100.0) / 255.0) + "%") : sliderValue
            color: sliderObj ? sliderObj.foregroundColor : "white"
        }

        QLCPlusFader
        {
            id: slFader
            visible: sliderObj ? sliderObj.widgetStyle === VCSlider.WSlider : false
            enabled: visible && sliderObj && !sliderObj.isDisabled
            Layout.alignment: Qt.AlignHCenter
            Layout.fillHeight: true
            width: parent.width
            rotation: sliderObj ? (sliderObj.invertedAppearance ? 180 : 0) : 0
            from: sliderObj ? sliderObj.rangeLowLimit : 0
            to: sliderObj ? sliderObj.rangeHighLimit : 255
            value: sliderValue
            handleGradient: sliderMode === VCSlider.Submaster ? submasterHandleGradient :
                            (sliderMode === VCSlider.GrandMaster ? grandMasterHandleGradient : defaultGradient)
            handleGradientHover: sliderMode === VCSlider.Submaster ? submasterHandleGradientHover :
                                 (sliderMode === VCSlider.GrandMaster ? grandMasterHandleGradientHover : defaultGradientHover)
            trackColor: sliderMode === VCSlider.Submaster ? "#77DD73" : defaultTrackColor

            onMoved: if (sliderObj) sliderObj.value = valueAt(position)
        }

        QLCPlusKnob
        {
            id: slKnob
            visible: sliderObj ? sliderObj.widgetStyle === VCSlider.WKnob : false
            enabled: visible && sliderObj && !sliderObj.isDisabled
            Layout.alignment: Qt.AlignHCenter
            Layout.fillHeight: true
            Layout.fillWidth: true
            from: sliderObj ? sliderObj.rangeLowLimit : 0
            to: sliderObj ? sliderObj.rangeHighLimit : 255
            value: sliderValue

            onMoved: if (sliderObj) sliderObj.value = value
        }

        Text
        {
            id: sliderText
            Layout.fillWidth: true
            height: UISettings.listItemHeight
            font: sliderObj ? sliderObj.font : Qt.font({ family: UISettings.robotoFontName })
            text: sliderObj ? sliderObj.caption : ""
            color: sliderObj ? sliderObj.foregroundColor : "white"
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }

        IconButton
        {
            visible: sliderObj ? sliderObj.monitorEnabled : false
            Layout.alignment: Qt.AlignHCenter
            faSource: FontAwesome.fa_xmark
            faColor: UISettings.bgControl
            bgColor: sliderObj && sliderObj.isOverriding ? "red" : UISettings.bgLight
            onClicked: if (sliderObj) sliderObj.isOverriding = false
        }

        IconButton
        {
            visible: sliderObj ? sliderObj.adjustFlashEnabled : false
            Layout.alignment: Qt.AlignHCenter
            faSource: FontAwesome.fa_star
            faColor: "deepskyblue"
            tooltip: qsTr("Flash the controlled Function")
            onPressed: { if (sliderObj) sliderObj.flashFunction(true) }
            onReleased: { if (sliderObj) sliderObj.flashFunction(false) }
        }
    }

    DropArea
    {
        id: dropArea
        anchors.fill: parent
        z: 2
        keys: [ "function" ]

        onDropped:
        {
            if (drag.source.hasOwnProperty("fromFunctionManager"))
                sliderObj.controlledFunction = drag.source.itemsList[0]
        }

        states: [
            State
            {
                when: dropArea.containsDrag
                PropertyChanges
                {
                    target: sliderRoot
                    color: UISettings.activeDropArea
                }
            }
        ]
    }
}
