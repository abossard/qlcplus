/*
  Q Light Controller Plus
  SongSectionRuler.qml

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
import QtQuick.Controls.Basic

import org.qlcplus.classes 1.0

import "TimeUtils.js" as TimeUtils
import "."

/* The song's sections in a row of their own below the time ruler. It renders
   showManager.analysisSections and only dispatches section edits through
   showManager.editSongSection; the blocks follow the sections in C++ */
Item
{
    id: sectionRuler

    property var sections: showManager.analysisSections
    property real timeScale: showManager.timeScale
    property real tickSize: showManager.tickSize
    property bool msMode: showManager.timeBasedDivision
    property int bpmNumber: showManager.bpmNumber
    property int beatsDivision: showManager.beatsDivision
    readonly property bool editable: !showManager.readOnly
    // the part of the row in view, where a refusal is shown
    property real visibleX: 0
    property real visibleWidth: width

    // a boundary drag in progress: the boundary after dragIndex is drawn at dragMs
    property int dragIndex: -1
    property real dragMs: 0
    // the section the menu acts on
    property int menuIndex: -1
    property string errorText: ""

    readonly property var colors: ({ "drop": "#3987e5", "buildup": "#d95926", "breakdown": "#199e70",
                                     "intro": "#c98500", "outro": "#d55181", "groove": "#008300" })
    // the labels a section can get; an older label (groove) still shows when loaded
    readonly property var labels: edmMapping.labels()

    // new sections end a drag and close the menu: both point at sections by index
    onSectionsChanged:
    {
        dragIndex = -1
        sectionMenu.close()
    }

    function toX(ms)
    {
        return TimeUtils.msToTimelinePos(ms, msMode, timeScale, tickSize, bpmNumber, beatsDivision)
    }

    function toMs(x)
    {
        return msMode ? TimeUtils.posToMs(x, timeScale, tickSize)
                      : TimeUtils.posToBeatMs(x, tickSize, bpmNumber, beatsDivision)
    }

    function startOf(i)
    {
        return dragIndex >= 0 && i === dragIndex + 1 ? dragMs : sections[i].startMs
    }

    function endOf(i)
    {
        return i === dragIndex ? dragMs : sections[i].endMs
    }

    // one section edit; the row is rebuilt from the new sections afterwards
    function dispatch(action, index, timeMs, label)
    {
        errorText = showManager.editSongSection(action, index, Math.round(timeMs), label)
        if (errorText !== "")
            errorTimer.restart()
    }

    function anyEdited()
    {
        for (var i = 0; i < sections.length; i++)
            if (sections[i].edited)
                return true
        return false
    }

    Rectangle
    {
        anchors.fill: parent
        color: UISettings.bgStrong
    }

    Repeater
    {
        model: sectionRuler.sections

        Item
        {
            id: sectionItem
            objectName: "songSection"

            required property int index
            required property var modelData
            readonly property int sectionIndex: index
            readonly property string label: modelData.label
            readonly property bool edited: modelData.edited === true

            x: toX(startOf(index))
            width: Math.max(1, toX(endOf(index)) - x)
            height: sectionRuler.height

            // stronger where the label is more certain; edited sections count as sure
            Rectangle
            {
                anchors.fill: parent
                anchors.margins: 1
                color: colors[sectionItem.label] || "#888888"
                opacity: sectionItem.edited ? 0.8 : 0.35 + 0.45 * sectionItem.modelData.labelConfidence / 100.0
            }

            Rectangle
            {
                anchors.fill: parent
                anchors.margins: 1
                color: "transparent"
                border.width: sectionItem.edited ? 2 : 0
                border.color: "white"
                visible: sectionItem.edited
            }

            Text
            {
                x: UISettings.iconSizeMedium * 0.15
                width: parent.width - 2 * x
                height: parent.height
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
                visible: width > UISettings.iconSizeMedium * 0.5
                color: "white"
                font.family: UISettings.robotoFontName
                font.pixelSize: sectionRuler.height * 0.5
                text: sectionItem.label
            }

            MouseArea
            {
                anchors.fill: parent
                enabled: editable
                onClicked: (mouse) =>
                {
                    menuIndex = sectionItem.index
                    var p = sectionItem.mapToItem(sectionRuler, mouse.x, mouse.y)
                    sectionMenu.x = p.x
                    sectionMenu.y = sectionRuler.height
                    sectionMenu.open()
                }
            }
        }
    }

    // the boundaries between two sections: drag to move, snapped to bars
    Repeater
    {
        model: Math.max(0, sectionRuler.sections.length - 1)

        Rectangle
        {
            id: boundaryItem
            objectName: "songSectionBoundary"

            required property int index
            readonly property int sectionIndex: index

            width: Math.max(6, UISettings.iconSizeMedium * 0.25)
            height: sectionRuler.height
            x: toX(endOf(index)) - width / 2
            z: 2
            color: boundaryMa.pressed || boundaryMa.containsMouse ? UISettings.selection : "transparent"
            opacity: 0.8

            MouseArea
            {
                id: boundaryMa
                anchors.fill: parent
                enabled: editable
                hoverEnabled: true
                preventStealing: true
                cursorShape: Qt.SizeHorCursor

                property real origin: 0

                onPressed: (mouse) =>
                {
                    origin = sectionRuler.sections[boundaryItem.index].endMs
                    dragMs = origin
                    dragIndex = boundaryItem.index
                }
                onPositionChanged: (mouse) =>
                {
                    if (!pressed)
                        return
                    var i = boundaryItem.index
                    var x = boundaryItem.mapToItem(sectionRuler, mouse.x, 0).x
                    // the nearest bar that leaves both sections a length, as the edit snaps
                    var ms = showManager.snapSongTime(toMs(Math.max(0, x)), sectionRuler.sections[i].startMs,
                                                      sectionRuler.sections[i + 1].endMs)
                    if (ms >= 0)
                        dragMs = ms
                }
                onReleased:
                {
                    var i = dragIndex
                    var ms = dragMs
                    var moved = ms !== origin
                    dragIndex = -1
                    if (moved)
                        Qt.callLater(dispatch, "move_boundary", i, ms, "")
                }
                onCanceled: dragIndex = -1
            }
        }
    }

    // a refused edit says why at the left of the part in view
    Rectangle
    {
        id: errorBox
        objectName: "songSectionError"
        readonly property string text: errorText
        z: 3
        x: visibleX + 2
        y: 1
        width: Math.min(errorLabel.implicitWidth + 12, Math.max(0, visibleWidth - 4))
        height: sectionRuler.height - 2
        visible: errorText !== ""
        color: UISettings.bgStronger
        border.color: "#ff9800"
        clip: true

        Text
        {
            id: errorLabel
            x: 6
            anchors.verticalCenter: parent.verticalCenter
            color: "#ff9800"
            font.family: UISettings.robotoFontName
            font.pixelSize: sectionRuler.height * 0.45
            text: errorText
        }

        Timer
        {
            id: errorTimer
            interval: 4000
            onTriggered: errorText = ""
        }
    }

    Popup
    {
        id: sectionMenu
        padding: 0
        focus: true

        readonly property var section: menuIndex >= 0 && menuIndex < sectionRuler.sections.length
                                       ? sectionRuler.sections[menuIndex] : null

        function run(action, timeMs, label)
        {
            var i = menuIndex
            close()
            Qt.callLater(dispatch, action, i, timeMs, label)
        }

        background:
            Rectangle
            {
                color: UISettings.bgStrong
                border.color: UISettings.bgStronger
            }

        Column
        {
            objectName: "songSectionMenu"
            width: UISettings.bigItemHeight * 2.6

            Repeater
            {
                model: sectionRuler.labels

                ContextMenuEntry
                {
                    required property string modelData
                    objectName: "songSectionMenu_label_" + modelData
                    height: UISettings.listItemHeight
                    iconHeight: UISettings.listItemHeight * 0.7
                    faSource: FontAwesome.fa_square
                    faColor: sectionRuler.colors[modelData]
                    entryText: modelData
                    enabled: sectionMenu.section !== null && sectionMenu.section.label !== modelData
                    onClicked: if (enabled) sectionMenu.run("relabel", 0, modelData)
                }
            }

            ContextMenuEntry
            {
                objectName: "songSectionMenu_split"
                height: UISettings.listItemHeight
                iconHeight: UISettings.listItemHeight * 0.7
                faSource: FontAwesome.fa_scissors
                entryText: qsTr("Split at the playhead")
                enabled: sectionMenu.section !== null && showManager.currentTime > sectionMenu.section.startMs
                         && showManager.currentTime < sectionMenu.section.endMs
                onClicked: if (enabled) sectionMenu.run("split", showManager.currentTime, "")
            }

            ContextMenuEntry
            {
                objectName: "songSectionMenu_merge"
                height: UISettings.listItemHeight
                iconHeight: UISettings.listItemHeight * 0.7
                faSource: FontAwesome.fa_object_group
                entryText: qsTr("Merge with the next section")
                enabled: menuIndex >= 0 && menuIndex < sectionRuler.sections.length - 1
                onClicked: if (enabled) sectionMenu.run("merge", 0, "")
            }

            ContextMenuEntry
            {
                objectName: "songSectionMenu_reset"
                height: UISettings.listItemHeight
                iconHeight: UISettings.listItemHeight * 0.7
                faSource: FontAwesome.fa_rotate_left
                entryText: qsTr("Reset this section to the analysis")
                enabled: sectionMenu.section !== null && sectionMenu.section.edited === true
                onClicked: if (enabled) sectionMenu.run("reset", 0, "")
            }

            ContextMenuEntry
            {
                objectName: "songSectionMenu_reset_all"
                height: UISettings.listItemHeight
                iconHeight: UISettings.listItemHeight * 0.7
                faSource: FontAwesome.fa_eraser
                entryText: qsTr("Reset all sections to the analysis")
                enabled: sectionRuler.anyEdited()
                onClicked: if (enabled) sectionMenu.run("reset_all", 0, "")
            }
        }
    }
}
