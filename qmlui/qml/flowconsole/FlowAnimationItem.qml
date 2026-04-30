/*
  Q Light Controller Plus
  FlowAnimationItem.qml

  Copyright (c) Massimo Callegari

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import org.qlcplus.classes 1.0
import "."

FlowWidgetItem
{
    id: animationRoot
    property VCAnimation animationObj: null

    Layout.preferredHeight: 150
    Layout.minimumHeight: 80
    clip: true

    onAnimationObjChanged: wObj = animationObj

    GridLayout
    {
        anchors.fill: parent
        columns: levelFader.visible ? 2 : 1

        QLCPlusFader
        {
            id: levelFader
            Layout.fillHeight: true; Layout.rowSpan: 3
            from: 0; to: 255
            visible: animationObj ? animationObj.visibilityMask & VCAnimation.Fader : true
            value: animationObj ? animationObj.faderLevel : 0
            onValueChanged: if (animationObj) animationObj.faderLevel = value
        }

        Text
        {
            Layout.fillWidth: true
            visible: animationObj ? animationObj.visibilityMask & VCAnimation.Label : true
            font: animationObj ? animationObj.font : Qt.font({ family: UISettings.robotoFontName })
            text: animationObj ? animationObj.caption : ""
            verticalAlignment: Text.AlignVCenter; horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap; lineHeight: 0.8
            color: animationObj ? animationObj.foregroundColor : "#111"
        }

        RowLayout
        {
            Layout.fillWidth: true; height: UISettings.iconSizeDefault

            Repeater
            {
                model: animationObj ? animationObj.colorCount : 0
                delegate: Rectangle
                {
                    required property int index
                    width: UISettings.iconSizeDefault; height: width; radius: 5
                    border.color: UISettings.bgLight; border.width: 2
                    color: animationObj && animationObj.colors.length > index ? animationObj.colors[index] : "transparent"
                }
            }
        }

        CustomComboBox
        {
            Layout.fillWidth: true; height: UISettings.listItemHeight
            visible: animationObj ? animationObj.visibilityMask & VCAnimation.PresetCombo : true
            textRole: ""
            model: animationObj ? animationObj.algorithms : null
            currentIndex: animationObj ? animationObj.algorithmIndex : 0
            onActivated: (index) => { if (animationObj) animationObj.algorithmIndex = currentIndex }
        }
    }

    DropArea
    {
        id: dropArea
        anchors.fill: parent; z: 2; keys: [ "function" ]
        onDropped: { if (drag.source.hasOwnProperty("fromFunctionManager")) animationObj.functionID = drag.source.itemsList[0] }
        states: [ State { when: dropArea.containsDrag; PropertyChanges { target: animationRoot; color: UISettings.activeDropArea } } ]
    }
}
