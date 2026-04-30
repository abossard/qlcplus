/*
  Q Light Controller Plus
  FlowClockItem.qml

  Copyright (c) Massimo Callegari

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

import QtQuick
import QtQuick.Layouts

import org.qlcplus.classes 1.0
import "TimeUtils.js" as TimeUtils
import "."

FlowWidgetItem
{
    id: clockRoot
    property VCClock clockObj: null
    property var locale: Qt.locale()
    property string timeString: new Date().toLocaleString(locale, "hh:mm:ss")
    property int clockType: clockObj ? clockObj.clockType : VCClock.Clock
    property int clockTimeValue: clockObj ? clockObj.currentTime : 0

    Layout.preferredHeight: 60
    Layout.minimumHeight: 40
    clip: true

    onClockObjChanged: { wObj = clockObj; updateTime() }
    onClockTimeValueChanged: updateTime()
    onClockTypeChanged: updateTime()

    function updateTime()
    {
        switch (clockType)
        {
            case VCClock.Stopwatch:
            case VCClock.Countdown:
                timeString = TimeUtils.msToStringWithPrecision(Math.max(0, clockTimeValue), 1)
                break
            case VCClock.Clock:
                timeString = new Date().toLocaleString(locale, "hh:mm:ss")
                break
        }
    }

    Row
    {
        anchors.fill: parent

        Text
        {
            width: parent.width - enableChk.width
            height: parent.height
            font: clockObj ? clockObj.font : Qt.font({ family: UISettings.robotoFontName })
            text: timeString
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            color: clockObj ? clockObj.foregroundColor : "#111"

            MouseArea
            {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onClicked: (mouse) =>
                {
                    if (clockType === VCClock.Stopwatch || clockType === VCClock.Countdown)
                    {
                        if (mouse.button === Qt.LeftButton)
                        {
                            if (clockType === VCClock.Countdown && clockTimeValue <= 0)
                                return
                            if (clockObj) clockObj.playPauseTimer()
                        }
                        else
                        {
                            if (clockObj) clockObj.resetTimer()
                        }
                    }
                }
            }
        }

        IconButton
        {
            id: enableChk
            anchors.verticalCenter: parent.verticalCenter
            height: Math.min(clockRoot.height, UISettings.iconSizeDefault)
            width: height
            checkable: true
            faSource: FontAwesome.fa_check
            faColor: "lime"
            checked: clockObj ? clockObj.enableSchedule : false
            onToggled: if (clockObj) clockObj.enableSchedule = checked
        }
    }
}
