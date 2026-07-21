/*
  Q Light Controller Plus
  FlowCueListItem.qml

  Copyright (c) Massimo Callegari

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.Basic

import org.qlcplus.classes 1.0
import "."

FlowWidgetItem
{
    id: cueListRoot
    property VCCueList cueListObj: null
    property int contentWidth: width - (sideFaderLayout.visible ? sideFaderLayout.width : 0)
    property int buttonsLayout: cueListObj ? cueListObj.playbackLayout : VCCueList.PlayPauseStop
    property int playbackStatus: cueListObj ? cueListObj.playbackStatus : VCCueList.Stopped
    property int sideFaderMode: cueListObj ? cueListObj.sideFaderMode : VCCueList.None

    Layout.preferredHeight: 300
    Layout.minimumHeight: 150
    clip: true

    onCueListObjChanged: wObj = cueListObj

    onPlaybackStatusChanged:
    {
        if (cueListObj && cueListObj.playbackLayout === VCCueList.PlayPauseStop)
        {
            playbackBtn.bgColor = playbackStatus === VCCueList.Playing ? "darkorange" :
                                  playbackStatus === VCCueList.Paused ? "green" : UISettings.bgLight
            stopBtn.bgColor = playbackStatus === VCCueList.Stopped ? UISettings.bgLight : "red"
        }
        else
        {
            playbackBtn.bgColor = playbackStatus === VCCueList.Stopped ? UISettings.bgLight : "red"
            stopBtn.bgColor = playbackStatus === VCCueList.Paused ? "darkorange" : UISettings.bgLight
        }
    }

    ColumnLayout
    {
        id: sideFaderLayout
        visible: sideFaderMode !== VCCueList.None
        height: parent.height
        width: UISettings.iconSizeDefault * 1.2

        RobotoText
        {
            height: UISettings.listItemHeight
            width: parent.width
            textHAlign: Text.AlignHCenter
            label: "" + sideFader.value + (sideFaderMode === VCCueList.Crossfade ? "%" : "")
        }

        QLCPlusFader
        {
            id: sideFader
            Layout.fillHeight: true
            Layout.alignment: Qt.AlignHCenter
            from: 0
            to: sideFaderMode === VCCueList.Crossfade ? 100 : 255
            value: cueListObj ? cueListObj.sideFaderLevel : 0
            onMoved: if (cueListObj) cueListObj.sideFaderLevel = value
        }
    }

    ColumnLayout
    {
        width: contentWidth
        height: parent.height
        x: sideFaderLayout.visible ? sideFaderLayout.width : 0
        spacing: 2

        ChaserWidget
        {
            id: chWidget
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: cueListObj ? cueListObj.stepsList : null
            playbackIndex: cueListObj ? cueListObj.playbackIndex : -1
            nextIndex: sideFaderMode === VCCueList.Crossfade && cueListObj ? cueListObj.nextStepIndex : -1
            isRunning: playbackStatus === VCCueList.Playing

            onIndexChanged: (index) => {
                if (cueListObj) cueListObj.playbackIndex = index
            }
            onNoteTextChanged: (index, text) => {
                if (cueListObj) cueListObj.setStepNote(index, text)
            }
            onAddFunctions: (list, index) => {
                if (cueListObj) cueListObj.addFunctions(list, index)
            }
            onEnterPressed: (index) => {
                if (cueListObj) cueListObj.playCurrentStep()
            }
        }

        Row
        {
            height: UISettings.iconSizeMedium

            IconButton
            {
                id: playbackBtn
                width: contentWidth / 4
                height: UISettings.iconSizeMedium
                enabled: cueListObj && !cueListObj.isDisabled
                faSource: (cueListObj && cueListObj.playbackLayout === VCCueList.PlayPauseStop) ?
                           (playbackStatus === VCCueList.Stopped || playbackStatus === VCCueList.Paused ?
                            FontAwesome.fa_play : FontAwesome.fa_pause) :
                           (playbackStatus === VCCueList.Stopped ? FontAwesome.fa_play : FontAwesome.fa_stop)
                faColor: UISettings.fgMain
                onClicked: if (cueListObj) cueListObj.playClicked()
            }
            IconButton
            {
                id: stopBtn
                width: contentWidth / 4
                height: UISettings.iconSizeMedium
                enabled: cueListObj && !cueListObj.isDisabled
                faSource: (cueListObj && cueListObj.playbackLayout === VCCueList.PlayStopPause) ? FontAwesome.fa_pause : FontAwesome.fa_stop
                faColor: UISettings.fgMain
                onClicked: if (cueListObj) cueListObj.stopClicked()
            }
            IconButton
            {
                width: contentWidth / 4
                height: UISettings.iconSizeMedium
                enabled: cueListObj && !cueListObj.isDisabled
                faSource: FontAwesome.fa_circle_left
                faColor: "lightcyan"
                onClicked: if (cueListObj) cueListObj.previousClicked()
            }
            IconButton
            {
                width: contentWidth / 4
                height: UISettings.iconSizeMedium
                enabled: cueListObj && !cueListObj.isDisabled
                faSource: FontAwesome.fa_circle_right
                faColor: "lightcyan"
                onClicked: if (cueListObj) cueListObj.nextClicked()
            }
        }
    }
}
