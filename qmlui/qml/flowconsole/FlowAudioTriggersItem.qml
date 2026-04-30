/*
  Q Light Controller Plus
  FlowAudioTriggersItem.qml

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
    id: audioTriggerRoot
    property VCAudioTriggers audioTriggerObj: null
    property variant barValues: audioTriggerObj ? audioTriggerObj.audioLevels : null

    Layout.preferredHeight: 150
    Layout.minimumHeight: 80
    clip: true

    onAudioTriggerObjChanged: wObj = audioTriggerObj

    GridLayout
    {
        anchors.fill: parent
        columns: 2; rows: 2

        Rectangle
        {
            id: barsItem
            Layout.fillHeight: true; Layout.fillWidth: true; Layout.rowSpan: 2
            color: "transparent"

            Row
            {
                anchors.fill: parent

                Repeater
                {
                    model: audioTriggerObj ? audioTriggerObj.barsNumber : 0
                    Rectangle
                    {
                        width: barsItem.width / (audioTriggerObj ? audioTriggerObj.barsNumber : 1)
                        height: parent.height
                        color: UISettings.bgStrong; border.width: 1; border.color: UISettings.bgLight

                        Rectangle
                        {
                            anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
                            height: barValues ? parent.height * (Math.max(0, Math.min(255, barValues[index] || 0)) / 255.0) : 0
                            radius: 3
                            color: index === 0 ? "#00FF00" : UISettings.selection
                        }
                    }
                }
            }
        }

        IconButton
        {
            width: height; height: UISettings.iconSizeMedium
            Layout.alignment: Qt.AlignHCenter
            checkable: true
            faSource: FontAwesome.fa_check; faColor: "lime"
            checked: audioTriggerObj ? audioTriggerObj.captureEnabled : false
            onToggled: if (audioTriggerObj) audioTriggerObj.captureEnabled = checked
        }

        QLCPlusFader
        {
            enabled: audioTriggerObj && !audioTriggerObj.isDisabled
            Layout.alignment: Qt.AlignHCenter; Layout.fillHeight: true
            from: 0; to: 100
            value: audioTriggerObj ? audioTriggerObj.volumeLevel : 0
            onMoved: if (audioTriggerObj) audioTriggerObj.volumeLevel = valueAt(position)
        }
    }
}
