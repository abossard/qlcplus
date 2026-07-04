/*
  Q Light Controller Plus
  TrackDelegate.qml

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

import org.qlcplus.classes 1.0
import "."

Rectangle
{
    id: trackRoot
    width: 100
    height: UISettings.mediumItemHeight
    clip: true

    color: isSelected ? UISettings.highlight : "#313F4A"

    property Track trackRef: null
    property bool isSelected: false

    signal trackSelected()

    CustomTextInput
    {
        x: 2
        width: parent.width - 4
        height: parent.height
        text: trackRef ? trackRef.name : ""
        wrapMode: TextInput.Wrap
        allowDoubleClick: true
        enabled: !showManager.readOnly

        onTextConfirmed:
            function(text)
            {
                // routed through ShowManager so the readOnly guard applies
                showManager.setTrackName(trackRef, text)
            }
    }

    Rectangle
    {
        width: parent.width
        height: 2
        y: parent.height - 2
        color: "#263039"
    }

    IconButton
    {
        id: soloButton
        x: parent.width - (width * 2) - 6
        y: 2
        z: 2
        width: parent.width / 6
        height: parent.height * 0.3
        bgColor: "#8191A0"
        checkedColor: "yellow"
        imgSource: ""
        checkable: true
        enabled: !showManager.readOnly
        tooltip: qsTr("Solo this track")
        onToggled: showManager.setTrackSolo(trackRef.id, checked)

        RobotoText
        {
            anchors.centerIn: parent
            height: parent.height - 2
            label: "S"
            labelColor: "#3C4A55"
            fontSize: height - 2
            fontBold: true
        }
    }

    IconButton
    {
        id: muteButton
        x: parent.width - width - 2
        y: 2
        z: 2
        width: parent.width / 6
        height: parent.height * 0.3
        bgColor: "#8191A0"
        checkedColor: "red"
        checked: trackRef ? trackRef.mute : false
        imgSource: ""
        checkable: true
        enabled: !showManager.readOnly
        tooltip: qsTr("Mute this track")
        // routed through ShowManager so the readOnly guard applies
        onToggled: showManager.setTrackMute(trackRef, checked)

        RobotoText
        {
            anchors.centerIn: parent
            height: parent.height - 2
            label: "M"
            labelColor: "#3C4A55"
            fontSize: height - 2
            fontBold: true
        }
    }

    IconButton
    {
        id: deleteTrackButton
        x: parent.width - width - 2
        y: parent.height - height - 4
        z: 2
        width: parent.width / 6
        height: parent.height * 0.3
        bgColor: "#8191A0"
        faSource: FontAwesome.fa_minus
        faColor: "#3C4A55"
        enabled: !showManager.readOnly
        tooltip: qsTr("Delete this track")
        onClicked:
        {
            if (trackRef)
            {
                showManager.selectedTrackId = trackRef.id
                deleteTrackPopup.message = qsTr("Are you sure you want to delete this track and all its items?")
                                           + "\n\"" + trackRef.name + "\""
                deleteTrackPopup.open()
            }
        }

        CustomPopupDialog
        {
            id: deleteTrackPopup
            title: qsTr("Delete track")
            onAccepted: showManager.deleteSelectedTrack()
        }
    }

    MouseArea
    {
        anchors.fill: parent
        propagateComposedEvents: true
        onClicked: (mouse) =>
        {
            showManager.selectedTrackId = trackRef.id
            trackRoot.trackSelected()
            mouse.accepted = false
        }
    }
}
