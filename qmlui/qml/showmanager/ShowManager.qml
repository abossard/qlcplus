/*
  Q Light Controller Plus
  ShowManager.qml

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

import "ShortcutUtils.js" as ShortcutUtils
import "TimeUtils.js" as TimeUtils
import "."

Rectangle
{
    id: showMgrContainer
    anchors.fill: parent
    color: "transparent"

    property string contextName: "SHOWMGR"
    readonly property bool showKeyScope: true
    property string keyboardFeedback: ""

    Keys.onPressed: (event) =>
    {
        if ((event.key === Qt.Key_Left || event.key === Qt.Key_Right)
            && (event.modifiers === Qt.NoModifier || event.modifiers === Qt.AltModifier))
        {
            keyboardFeedback = recordings.amountReason
            if (keyboardFeedback === "")
                keyboardFeedback = showManager.shiftSelectedItems(event.key === Qt.Key_Left ? -1 : 1,
                    recordings.stepMs, Boolean(event.modifiers & Qt.AltModifier))
        }
        else if (event.key === Qt.Key_Space && event.modifiers === Qt.NoModifier)
            requestPlayShow()
        else if (event.modifiers === Qt.ControlModifier && event.key === Qt.Key_C)
        {
            if (editorTab === 0 && !recordingLane.activeFocus)
                requestCopyItems()
            else
                keyboardFeedback = qsTr("Recording Copy/Paste is not available.")
        }
        else if (event.modifiers === Qt.ControlModifier && event.key === Qt.Key_V)
        {
            if (editorTab === 0 && !recordingLane.activeFocus)
                requestPasteItems()
            else
                keyboardFeedback = qsTr("Recording Copy/Paste is not available.")
        }
        else if (event.key === Qt.Key_Delete || event.key === Qt.Key_Backspace)
        {
            if (editorTab === 0 && !recordingLane.activeFocus && canEdit)
                showManager.deleteShowItems(showManager.selectedItemRefs())
        }
        else
            return
        event.accepted = true
    }

    property int trackHeight: UISettings.mediumItemHeight
    property int trackWidth: UISettings.bigItemHeight * 1.6

    property real timeScale: showManager.timeScale
    property real tickSize: showManager.tickSize
    property int headerHeight: UISettings.iconSizeMedium
    property real xViewOffset: 0

    property real showID: showManager.currentShowID
    property int selectedTrackIndex: -1
    // 0: the timeline, 1: the recorded commands of the same Show
    property int editorTab: 0
    property var recordingReturnIds: []
    property real recordingReturnShow: -1

    function returnToTimeline()
    {
        editorTab = 0
        if (recordingReturnShow === showID && recordingReturnIds.length)
        {
            recordings.selectedIds = recordingReturnIds.filter(function(id) {
                return recordings.rowsById[id] !== undefined
            })
            Qt.callLater(recordingLane.focusSelection)
        }
    }

    // single QML gate for every mutating affordance; the C++ side enforces
    // the same rule authoritatively (VDJ Perform mode = read only)
    readonly property bool canEdit: showManager.isEditing && !showManager.readOnly

    onShowIDChanged: renderAndCenter()
    Component.onCompleted:
    {
        renderAndCenter()
        forceActiveFocus()
        if (showCommandRecorder)
            showCommandRecorder.setRowsObserved(visible)
    }
    onVisibleChanged: {
        if (showCommandRecorder)
            showCommandRecorder.setRowsObserved(visible)
        if (visible)
            forceActiveFocus()
    }
    Component.onDestruction: if (showCommandRecorder) showCommandRecorder.setRowsObserved(false)
    readonly property bool hasRecordings: showCommandRecorder
        && (showCommandRecorder.groups.length > 0 || recordings.deletedCount > 0)

    Rectangle
    {
        objectName: "showKeyboardFocus"
        property var target: qlcplus.activeFocusItem
        readonly property bool local: {
            for (var item = target; item; item = item.parent)
                if (item === showMgrContainer)
                    return target !== showMgrContainer
            return false
        }
        property rect frame: {
            showContents.contentY; xViewOffset; rightPanel.width; showMgrContainer.width
            if (!target)
                return Qt.rect(0, 0, 0, 0)
            target.x; target.y
            return target.mapToItem(showMgrContainer, Qt.rect(0, 0, target.width, target.height))
        }
        x: frame.x
        y: frame.y
        width: frame.width
        height: frame.height
        visible: local && target.visible
        enabled: false
        color: "transparent"
        border.color: "#f1c40f"
        border.width: 2
        z: 100
        Accessible.ignored: true
    }

    function requestPlayShow()
    {
        if (canEdit)
            showManager.playShow()
    }

    function requestStopShow()
    {
        if (canEdit)
            showManager.stopShow()
    }

    function requestCopyItems()
    {
        if (showManager.selectedItemsCount)
            showManager.copyToClipboard()
    }

    function requestPasteItems()
    {
        if (canEdit && showManager.pasteFromClipboard() === false)
            pasteErrorPopup.open()
    }

    function centerView()
    {
        var cursorX = showManager.timeBasedDivision
                ? TimeUtils.timeToSize(showManager.currentTime, timeScale, tickSize)
                : TimeUtils.timeToBeatPosition(showManager.currentTime, tickSize,
                                               showManager.bpmNumber, showManager.beatsDivision)
        var xPos = cursorX - (timelineHeader.width / 2)
        if (xPos >= 0)
            xViewOffset = xPos
    }

    function followPlayhead()
    {
        if (!timelineHeader || !hdrItem
                || (!showManager.readOnly && (!showManager.isPlaying || showManager.isPaused))
                || timelineHeader.width <= 0)
            return

        var cursorX = hdrItem.cursorPosition
        if (cursorX < xViewOffset || cursorX + 1 > xViewOffset + timelineHeader.width)
            xViewOffset = Math.max(0, cursorX - (timelineHeader.width / 2))
    }

    Connections
    {
        target: showManager
        function onReadOnlyChanged() { Qt.callLater(showMgrContainer.followPlayhead) }
        function onIsPlayingChanged() { Qt.callLater(showMgrContainer.followPlayhead) }
        function onIsPausedChanged() { Qt.callLater(showMgrContainer.followPlayhead) }
        function onSelectedItemsCountChanged(count)
        {
            if (count > 0)
            {
                recordings.selectedIds = []
                if (recordingLane.activeFocus)
                    showMgrContainer.forceActiveFocus()
            }
        }
    }

    function zoomTimeline(zoomIn)
    {
        // In time-based modes a larger timeScale renders smaller content (zoom out).
        // In Beats mode tickSize grows with timeScale, so the relation is inverted.
        // "grow" = whether the visual zoom request needs timeScale to increase.
        var grow = showManager.timeBasedDivision ? !zoomIn : zoomIn
        var step = grow ? (showManager.timeScale >= 1.0 ? 1.0 : 0.1)
                        : (showManager.timeScale > 1.0 ? 1.0 : 0.1)
        var ts = showManager.timeScale + (grow ? step : -step)
        if (ts < 0.1)
            ts = 0.1
        showManager.timeScale = ts
        centerView()
    }

    function renderAndCenter()
    {
        //console.log("Show Manager tick size: " + tickSize + "pixel")
        showManager.renderView(itemsArea.contentItem)
        centerView()
    }

    Shortcut
    {
        sequence: "Space"
        enabled: mainView.currentContext === "SHOWMGR"
                 && !recordingLane.activeFocus
                 && !mainView.shortcutsBlocked()
                 && (qlcplus.accessMask & App.AC_ShowManager)
        onActivated: showMgrContainer.requestPlayShow()
    }

    Shortcut
    {
        sequence: Qt.platform.os === "osx" ? "Meta+Space" : "Ctrl+Space"
        enabled: mainView.currentContext === "SHOWMGR"
                 && !mainView.shortcutsBlocked()
                 && (qlcplus.accessMask & App.AC_ShowManager)
        onActivated: showMgrContainer.requestStopShow()
    }

    Shortcut
    {
        sequence: StandardKey.Copy
        enabled: mainView.currentContext === "SHOWMGR"
                 && !mainView.shortcutsBlocked()
                 && showManager.selectedItemsCount > 0
        onActivated: showMgrContainer.requestCopyItems()
    }

    Shortcut
    {
        sequence: StandardKey.Paste
        enabled: mainView.currentContext === "SHOWMGR"
                 && !mainView.shortcutsBlocked()
                 && (qlcplus.accessMask & App.AC_ShowManager)
        onActivated: showMgrContainer.requestPasteItems()
    }

    Shortcut
    {
        sequence: "Ctrl+]"
        enabled: mainView.currentContext === "SHOWMGR" && !mainView.shortcutsBlocked()
        onActivated:
        {
            if (rightPanel.isOpen)
                rightPanel.animatePanel(false)
            else if (rightPanel.loaderSource !== "")
                rightPanel.animatePanel(true)
        }
    }

    Rectangle
    {
        id: topBar
        width: showMgrContainer.width - rightPanel.width
        height: UISettings.iconSizeDefault * 2
        z: 5
        gradient: Gradient
        {
            GradientStop { position: 0; color: UISettings.toolbarStartSub }
            GradientStop { position: 1; color: UISettings.toolbarEnd }
        }

        RowLayout
        {
            id: topBarRowLayout
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: UISettings.iconSizeDefault
            y: 1

            spacing: 4

            RobotoText { label: qsTr("Name") }

            CustomTextEdit
            {
                width: showMgrContainer.width / 5
                height: parent.height - 10
                text: showManager.showName
                enabled: canEdit

                onTextEdited: showManager.showName = text
            }

            // read-only indicator while VDJ Perform drives this show
            Rectangle
            {
                visible: showManager.readOnly
                width: performBadgeText.width + UISettings.textSizeDefault
                height: parent.height - 10
                radius: height / 4
                color: "#7a5c00"
                border.color: "#f1c40f"
                border.width: 1

                RobotoText
                {
                    id: performBadgeText
                    anchors.centerIn: parent
                    label: qsTr("PERFORM — read only")
                    labelColor: "#f1c40f"
                    fontSize: UISettings.textSizeDefault * 0.7
                }
            }

            RecordControl { Layout.fillHeight: true }

            ButtonGroup { id: editorTabGroup }

            MenuBarEntry
            {
                id: timelineTabButton
                objectName: "timelineTab"
                entryText: qsTr("Timeline")
                checked: showMgrContainer.editorTab === 0
                ButtonGroup.group: editorTabGroup
                focusPolicy: Qt.StrongFocus
                onClicked: showMgrContainer.returnToTimeline()
            }

            MenuBarEntry
            {
                id: recordingsTabButton
                KeyNavigation.backtab: timelineTabButton
                KeyNavigation.tab: editorTab === 1 ? recordings.firstFocus : colPickButton
                objectName: "recordingsTab"
                focusPolicy: Qt.StrongFocus
                entryText: qsTr("Recordings")
                checked: showMgrContainer.editorTab === 1
                ButtonGroup.group: editorTabGroup
                onClicked: showMgrContainer.editorTab = 1
            }

            IconButton
            {
                id: colPickButton
                z: 2
                width: parent.height - 6
                height: width
                imgSource: "qrc:/color.svg"
                checkable: true
                enabled: canEdit
                tooltip: qsTr("Show items color")
                onCheckedChanged: colTool.visible = !colTool.visible

                ColorTool
                {
                    id: colTool
                    parent: mainView
                    x: colPickButton.x
                    y: UISettings.bigItemHeight //colPickButton.y + colPickButton.height
                    z: 15
                    visible: false

                    onToolColorChanged:
                        function(r, g, b, w, a, uv)
                        {
                            showManager.itemsColor = Qt.rgba(r, g, b, 1.0)
                        }
                    onClose: colPickButton.toggle()
                }
            }

            IconButton
            {
                id: lockItem
                z: 2
                width: parent.height - 6
                height: width
                imgSource: "qrc:/lock.svg"
                enabled: canEdit
                counter: showManager.selectedItemsCount

                function checkLockStatus()
                {
                    if (showManager.selectedItemsLocked())
                    {
                        imgSource = "qrc:/unlock.svg"
                        tooltip = qsTr("Unlock the selected items")
                    }
                    else
                    {
                        imgSource = "qrc:/lock.svg"
                        tooltip = qsTr("Lock the selected items")
                    }
                }

                onCounterChanged:
                {
                    checkLockStatus()
                }

                onClicked:
                {
                    var lock = showManager.selectedItemsLocked()
                    if (lock === true)
                        showManager.setSelectedItemsLock(false)
                    else
                        showManager.setSelectedItemsLock(true)
                    checkLockStatus()
                }
            }

            IconButton
            {
                id: gridButton
                z: 2
                width: parent.height - 6
                height: width
                imgSource: "qrc:/grid.svg"
                tooltip: qsTr("Snap to grid")
                checkable: true
                checked: showManager.gridEnabled
                onToggled: showManager.gridEnabled = checked
            }

            IconButton
            {
                id: stretchBtn
                width: parent.height - 6
                height: width
                faSource: FontAwesome.fa_arrows_left_right_to_line
                faColor: "lightyellow"
                tooltip: qsTr("Stretch the original function")
                checkable: true
                checked: showManager.stretchFunctions
                onToggled: showManager.stretchFunctions = checked
            }

            IconButton
            {
                id: removeItem
                z: 2
                width: parent.height - 6
                height: width
                faSource: FontAwesome.fa_minus
                faColor: "crimson"
                tooltip: qsTr("Remove the selected items")
                enabled: canEdit
                counter: showManager.selectedItemsCount
                onClicked:
                {
                    var selNames = showManager.selectedItemNames()
                    //console.log(selNames)
                    deleteItemsPopup.message = qsTr("Are you sure you want to remove the following items?\n" +
                                                    "(Note that the original functions will not be deleted)") + "\n" + selNames
                    deleteItemsPopup.open()
                }

                CustomPopupDialog
                {
                    id: deleteItemsPopup
                    title: qsTr("Delete show items")
                    onAccepted: showManager.deleteShowItems(showManager.selectedItemRefs())
                }
            }

            IconButton
            {
                id: copyBtn
                width: parent.height - 6
                height: width
                faSource: FontAwesome.fa_copy
                faColor: UISettings.fgMain
                tooltip: ShortcutUtils.withShortcut(qsTr("Copy the selected items in the clipboard"), "Ctrl+C")
                counter: showManager.selectedItemsCount
                onClicked: showMgrContainer.requestCopyItems()
            }

            IconButton
            {
                id: pasteBtn
                width: parent.height - 6
                height: width
                faSource: FontAwesome.fa_paste
                faColor: UISettings.fgMain
                tooltip: ShortcutUtils.withShortcut(qsTr("Paste items in the clipboard at cursor position"), "Ctrl+V")
                enabled: canEdit
                counter: showManager.clipboardItemsCount
                onClicked: showMgrContainer.requestPasteItems()

                CustomPopupDialog
                {
                    id: pasteErrorPopup
                    title: qsTr("Paste error")
                    standardButtons: Dialog.Ok
                    message: qsTr("It is not possible to paste the items on the selected track at the current cursor position")
                }
            }

            // filler
            Rectangle
            {
                Layout.fillWidth: true
            }

            RobotoText
            {
                id: timeBox
                radius: height / 5
                border.color: UISettings.fgMedium
                border.width: 1
                leftMargin: UISettings.textSizeDefault
                rightMargin: UISettings.textSizeDefault
                property int currentTime: showManager.currentTime

                label: "00:00:00.00"
                Accessible.role: Accessible.StaticText
                Accessible.name: label
                Accessible.id: "showTime"

                onCurrentTimeChanged:
                {
                    label = TimeUtils.msToStringWithPrecision(currentTime, showManager.isPlaying ? 1 : 2)
                }
            }

            IconButton
            {
                id: playbackBtn
                width: parent.height - 6
                height: width
                faSource: (showManager.isPlaying && !showManager.isPaused) ? FontAwesome.fa_pause : FontAwesome.fa_play
                faColor: UISettings.fgMain
                bgColor: showManager.isPaused ? "green" :
                         (showManager.isPlaying ? "darkorange" : UISettings.bgLight)
                tooltip: ShortcutUtils.withShortcut((showManager.isPlaying && !showManager.isPaused) ? qsTr("Pause") : qsTr("Play or resume"), "Space")
                Accessible.name: (showManager.isPlaying && !showManager.isPaused) ? qsTr("Pause") : qsTr("Play or resume")
                checkable: false
                enabled: canEdit
                onClicked: showMgrContainer.requestPlayShow()
            }
            IconButton
            {
                id: stopBtn
                width: parent.height - 6
                height: width
                faSource: FontAwesome.fa_stop
                faColor: UISettings.fgMain
                bgColor: showManager.isPlaying ? "red" : UISettings.bgLight
                tooltip: ShortcutUtils.withShortcut(qsTr("Stop or rewind"),
                    Qt.platform.os === "osx" ? "Meta+Space" : "Ctrl+Space")
                Accessible.name: qsTr("Stop or rewind")
                checkable: false
                enabled: canEdit
                onClicked: showMgrContainer.requestStopShow()
            }

            // filler
            Rectangle
            {
                Layout.fillWidth: true
            }


            RobotoText
            {
                label: qsTr("Markers")
            }

            CustomComboBox
            {
                model: [
                    { mLabel: qsTr("Time"), mValue: Show.Time },
                    { mLabel: qsTr("VDJ Beat"), mValue: Show.VDJBeat },
                    { mLabel: qsTr("BPM 4/4"), mValue: Show.BPM_4_4 },
                    { mLabel: qsTr("BPM 3/4"), mValue: Show.BPM_3_4 },
                    { mLabel: qsTr("BPM 2/4"), mValue: Show.BPM_2_4 }
                ]
                id: markersCombo
                objectName: "markersCombo"
                enabled: canEdit
                currValue: showManager.timeDivision
                onValueChanged:
                {
                    // BPM beat markers need a tempo: refuse them until a BPM is set.
                    // Time and VDJ Beat are exempt (the VDJ grid comes from the
                    // VirtualDJ database, not from the BPM number).
                    // The revert must be deferred (Qt.callLater) because the combo is
                    // mid-update here (its isUpdating guard would swallow a synchronous
                    // currValue/currentIndex change).
                    if (currValue !== Show.Time && currValue !== Show.VDJBeat
                        && showManager.bpmNumber <= 0)
                    {
                        Qt.callLater(function() { markersCombo.currValue = Show.Time })
                        return
                    }
                    if (currValue !== Show.Time && currValue !== Show.VDJBeat &&
                            showManager.timeBasedDivision &&
                            showManager.hasBeatBasedItems())
                    {
                        beatAlignWarningPopup.pendingDivision = currValue
                        beatAlignWarningPopup.open()
                    }
                    else
                    {
                        showManager.timeDivision = currValue
                    }
                }

                Connections
                {
                    target: showManager
                    function onTimeDivisionChanged(division)
                    {
                        if (markersCombo.currValue !== division)
                            Qt.callLater(function() { markersCombo.currValue = division })
                    }
                }

                CustomPopupDialog
                {
                    id: beatAlignWarningPopup
                    objectName: "beatAlignWarningPopup"
                    title: qsTr("Switch to BPM markers")
                    message: qsTr("Warning: all beat-based functions will be aligned to the nearest beat")
                    standardButtons: Dialog.Ok | Dialog.Cancel

                    property var pendingDivision: Show.Time

                    // the OK/Cancel buttons only emit clicked(role) (see
                    // CustomPopupDialog's footer), while accepted()/rejected()
                    // only fire when confirming with the Enter key, so both
                    // paths must be handled to cover mouse and keyboard
                    onClicked: (role) =>
                    {
                        if (role === Dialog.Ok)
                            showManager.timeDivision = pendingDivision
                        else
                            markersCombo.currValue = showManager.timeDivision
                        close()
                    }
                    onAccepted: showManager.timeDivision = pendingDivision
                    onRejected: markersCombo.currValue = showManager.timeDivision
                }
            }

            ZoomItem
            {
                implicitWidth: UISettings.mediumItemHeight * 1.3
                implicitHeight: parent.height - 2
                fontColor: "#222"

                onZoomOutClicked: showMgrContainer.zoomTimeline(false)
                onZoomInClicked: showMgrContainer.zoomTimeline(true)
            }
        }
        RowLayout
        {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: UISettings.iconSizeDefault
            spacing: 6
            RobotoText { label: qsTr("Move by") }
            CustomComboBox
            {
                id: moveStep
                objectName: "moveStep"
                focusPolicy: Qt.StrongFocus
                Layout.preferredWidth: UISettings.bigItemHeight * 1.7
                model: [ qsTr("Bar"), qsTr("Half bar"), qsTr("Quarter bar"), qsTr("Time delta (s)") ]
                currentIndex: recordings.stepIndex
                onCurrentIndexChanged: {
                    recordings.stepIndex = currentIndex
                    keyboardFeedback = ""
                    recordings.moveError = ""
                }
                KeyNavigation.tab: timeDelta.visible ? timeDelta : null
            }
            CustomTextEdit
            {
                id: timeDelta
                objectName: "moveTimeDelta"
                visible: recordings.stepIndex === 3
                Layout.preferredWidth: UISettings.bigItemHeight
                placeholderText: qsTr("Seconds")
                Accessible.name: qsTr("Move by time delta in seconds")
                text: recordings.timeDeltaText
                onTextChanged: {
                    recordings.timeDeltaText = text
                    keyboardFeedback = ""
                    recordings.moveError = ""
                }
            }
            RobotoText
            {
                objectName: "timelineKeyboardFeedback"
                Layout.fillWidth: true
                label: keyboardFeedback || recordings.moveError || recordings.amountReason
                    || qsTr("Left/Right: move selection. %1: adjust scheduling end.")
                        .arg(ShortcutUtils.display("Alt+Left/Right"))
                labelColor: keyboardFeedback || recordings.moveError || recordings.amountReason
                    ? "#f1c40f" : UISettings.fgMain
                fontSize: UISettings.textSizeDefault * 0.7
            }
        }
    } // top bar

    RightPanel
    {
        id: rightPanel
        x: parent.width - width
        z: 5
        height: parent.height - (bottomPanel.visible ? bottomPanel.height : 0)
        inShowManager: true
    }

    BottomPanel
    {
        id: bottomPanel
        objectName: "bottomPanelItem"
        y: parent.height - height
        z: 8
        visible: false
    }

    // top left area to perform time scaling
    Rectangle
    {
        y: topBar.height
        z: 5
        visible: showMgrContainer.editorTab === 0
        width: trackWidth + verticalDivider.width
        height: showMgrContainer.headerHeight
        color: UISettings.bgStrong

        RowLayout
        {
            anchors.fill: parent

            IconButton
            {
                visible: selectedTrackIndex > 0 ? true : false
                height: parent.height - 2
                width: height
                faSource: FontAwesome.fa_angle_up
                faColor: UISettings.fgMain
                tooltip: qsTr("Move the selected track up")
                onClicked:
                {
                    showManager.moveTrack(selectedTrackIndex, -1)
                    selectedTrackIndex--
                    renderAndCenter()
                }
            }

            IconButton
            {
                visible: selectedTrackIndex < tracksBox.count - 1 ? true : false
                height: parent.height - 2
                width: height
                faSource: FontAwesome.fa_angle_down
                faColor: UISettings.fgMain
                tooltip: qsTr("Move the selected track down")
                onClicked:
                {
                    showManager.moveTrack(selectedTrackIndex, 1)
                    selectedTrackIndex++
                    renderAndCenter()
                }
            }

            // layout filler
            Rectangle
            {
                Layout.fillWidth: true
                color: "transparent"
            }

            Rectangle
            {
                width: verticalDivider.width
                height: parent.height
                color: UISettings.bgLight
            }
        }
    }

    // the timeline header can be flicked horizontally but not vertically
    // so this is kept outside the main vertical flickable
    Flickable
    {
        id: timelineHeader
        x: trackWidth + verticalDivider.width
        y: topBar.height
        z: 4
        visible: showMgrContainer.editorTab === 0
        height: showMgrContainer.headerHeight
        width: showMgrContainer.width - trackWidth - verticalDivider.width - rightPanel.width

        boundsBehavior: Flickable.StopAtBounds
        flickableDirection: Flickable.HorizontalFlick

        contentWidth: hdrItem.width //> width ? hdrItem.width : width
        contentX: xViewOffset

        onContentXChanged: xViewOffset = contentX
        onWidthChanged: showMgrContainer.followPlayhead()

        HeaderAndCursor
        {
            id: hdrItem
            z: 2
            height: parent.height
            visibleWidth: timelineHeader.width
            visibleX: xViewOffset
            headerHeight: showMgrContainer.headerHeight
            cursorHeight: showMgrContainer.height - topBar.height - (bottomPanel.visible ? bottomPanel.height : 0)
            duration: showManager.showDuration
            enabled: canEdit

            // the playhead as an accessible slider in Show milliseconds: Qt
            // reads and writes value, from, to and stepSize
            property real value: currentTime
            readonly property real from: 0
            // the ruler's end; beat divisions store beats, the value is ms
            readonly property real to: Math.max(currentTime, msMode ? Math.max(0, duration) + 300000
                : TimeUtils.posToBeatMs(TimeUtils.beatsToSize(Math.max(0, duration) + 300000, tickSize, beatsDivision),
                                        tickSize, bpmNumber, beatsDivision))
            readonly property real stepSize: showManager.timeBasedDivision ? 1000 : 60000 / Math.max(1, showManager.bpmNumber)
            Accessible.role: Accessible.Slider
            Accessible.name: qsTr("Playhead position")
            Accessible.id: "showPlayheadPosition"
            Accessible.description: TimeUtils.msToString(currentTime)

            // a written value is a seek request; the value always shows the cursor
            onValueChanged:
            {
                if (Math.round(value) === currentTime)
                    return
                showManager.requestSeek(Math.round(value))
                value = Qt.binding(function() { return currentTime })
            }

            onCursorPositionChanged: showMgrContainer.followPlayhead()

            onClicked: (mouseX, mouseY) =>
            {
                if (showManager.readOnly)
                    return
                if (showManager.timeBasedDivision)
                    showManager.requestSeek(TimeUtils.posToMs(mouseX, timeScale, tickSize))
                else
                    showManager.requestSeek(TimeUtils.posToBeatMs(mouseX, tickSize, showManager.bpmNumber, showManager.beatsDivision))
                showManager.resetItemsSelection()
            }
        }
    }

    // the main flickable area containing the tracks list and the Show items
    // this can be flicked only vertically
    Flickable
    {
        id: showContents
        y: topBar.height + headerHeight
        z: 3 // below timelineHeader
        visible: showMgrContainer.editorTab === 0
        width: parent.width - rightPanel.width
        height: showMgrContainer.height - topBar.height - headerHeight - (bottomPanel.visible ? bottomPanel.height : 0)
        clip: true

        boundsBehavior: Flickable.StopAtBounds
        flickableDirection: Flickable.VerticalFlick
        ScrollBar.vertical: CustomScrollBar { }

        contentHeight: totalTracksHeight > height ? totalTracksHeight : height
        //contentWidth: timelineHeader.contentWidth

        property real totalTracksHeight: (tracksBox.count + 2 + (hasRecordings ? 1 : 0)) * trackHeight

        Rectangle
        {
            width: trackWidth
            height: parent.height
            color: UISettings.bgMedium
            z: 2

            Column
            {
                width: trackWidth

                Repeater
                {
                    id: tracksBox
                    width: parent.width
                    model: showManager.tracks

                    delegate:
                        TrackDelegate
                        {
                            width: tracksBox.width
                            height: trackHeight
                            trackRef: modelData
                            isSelected: showMgrContainer.selectedTrackIndex === index ? true : false

                            onTrackSelected: showMgrContainer.selectedTrackIndex = index
                        }
                }
                Rectangle
                {
                    visible: hasRecordings
                    width: trackWidth
                    height: trackHeight
                    color: UISettings.bgMedium
                    RobotoText
                    {
                        anchors.horizontalCenter: parent.horizontalCenter
                        y: 2
                        label: qsTr("Recordings") + " (" + recordings.selectedIds.length + ")"
                    }
                    Row
                    {
                        anchors.bottom: parent.bottom
                        anchors.horizontalCenter: parent.horizontalCenter
                        spacing: 2
                        IconButton
                        {
                            width: UISettings.listItemHeight
                            height: width
                            faSource: FontAwesome.fa_backward
                            Accessible.id: "recordingMoveEarlier"
                            Accessible.name: qsTr("Move selected recordings earlier")
                            tooltip: Accessible.name
                            enabled: recordings.editable && recordings.amountReason === "" && recordings.selectedIds.length > 0
                            onClicked: recordings.moveSelected(-1)
                        }
                        IconButton
                        {
                            width: UISettings.listItemHeight
                            height: width
                            faSource: FontAwesome.fa_forward
                            Accessible.id: "recordingMoveLater"
                            Accessible.name: qsTr("Move selected recordings later")
                            tooltip: Accessible.name
                            enabled: recordings.editable && recordings.amountReason === "" && recordings.selectedIds.length > 0
                            onClicked: recordings.moveSelected(1)
                        }
                        GenericButton
                        {
                            activeFocusOnTab: true
                            Keys.onPressed: (event) => {
                                if (event.key === Qt.Key_Space || event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                                    if (!event.isAutoRepeat)
                                        clicked(Qt.LeftButton)
                                    event.accepted = true
                                }
                            }
                            width: UISettings.listItemHeight * 2
                            height: UISettings.listItemHeight
                            label: qsTr("Snap")
                            Accessible.id: "recordingSnap"
                            enabled: recordings.editable && recordings.grid !== null && recordings.selectedIds.length > 0
                            onClicked: recordings.snapSelected()
                        }
                        IconButton
                        {
                            width: UISettings.listItemHeight
                            height: width
                            faSource: FontAwesome.fa_trash
                            Accessible.id: "recordingDelete"
                            Accessible.name: qsTr("Delete %1 selected recording samples").arg(recordings.selectedIds.length)
                            tooltip: Accessible.name
                            enabled: recordings.editable && recordings.selectedIds.length > 0
                            onClicked: recordings.deleteSelected()
                        }
                    }
                }
            }
        }

        // tracks/timeline vertical divider
        Rectangle
        {
            id: verticalDivider
            x: tracksBox.width
            z: 2
            width: 3
            height: parent.height
            color: UISettings.bgLight
        }

        // the Show items area. This can be flicked horizontally,
        // together with the timelineHeader flickable
        Flickable
        {
            id: itemsArea
            objectName: "showItemsArea"
            x: verticalDivider.x + verticalDivider.width
            z: 1
            width: showMgrContainer.width - trackWidth
            height: parent.height
            clip: true

            boundsBehavior: Flickable.StopAtBounds
            contentHeight: showContents.contentHeight
            contentWidth: timelineHeader.contentWidth
            contentX: xViewOffset
            ScrollBar.horizontal: horScrollBar

            onContentXChanged: xViewOffset = contentX

            MouseArea
            {
                anchors.fill: parent
                enabled: !showManager.readOnly
                onClicked: (mouse) =>
                {
                    if (showManager.timeBasedDivision)
                        showManager.requestSeek(TimeUtils.posToMs(mouse.x, timeScale, tickSize))
                    else
                        showManager.requestSeek(TimeUtils.posToBeatMs(mouse.x, tickSize, showManager.bpmNumber, showManager.beatsDivision))
                    showManager.resetItemsSelection()
                }
            }

            // track divider horizontal lines
            Repeater
            {
                model: tracksBox.count
                delegate:
                    Rectangle
                    {
                        height: 1
                        width: timelineHeader.contentWidth
                        y: (index * trackHeight) + trackHeight - 1
                        color: UISettings.bgLight
                    }
            }

            /** Vertical grid dividers, drawn with the same chunk strategy
              * and the same marker calculations of the timeline header:
              * the Canvas is 3 times the visible area and gets shifted
              * and repainted while flicking horizontally */
            Canvas
            {
                id: gridCanvas
                visible: showManager.gridEnabled
                z: 0
                x: -itemsArea.width
                y: 0
                width: itemsArea.width * 3
                height: itemsArea.contentHeight
                antialiasing: true
                contextType: "2d"

                property real tickSize: showManager.tickSize
                property int beatsDivision: showManager.beatsDivision

                function updatePosition()
                {
                    if (itemsArea.width <= 0)
                        return

                    var chunk = parseInt(xViewOffset / itemsArea.width) * itemsArea.width
                    gridCanvas.x = chunk - itemsArea.width
                    gridCanvas.requestPaint()
                }

                onTickSizeChanged: requestPaint()
                onBeatsDivisionChanged: requestPaint()
                onHeightChanged: requestPaint()
                onVisibleChanged: if (visible) updatePosition()

                Connections
                {
                    target: showMgrContainer

                    function onXViewOffsetChanged()
                    {
                        if (itemsArea.width <= 0)
                            return

                        if (xViewOffset < gridCanvas.x + itemsArea.width ||
                            xViewOffset > gridCanvas.x + (itemsArea.width * 2))
                            gridCanvas.updatePosition()
                    }
                }

                onPaint:
                {
                    var subDividers = showManager.beatsDivision

                    context.globalAlpha = 1.0
                    context.lineWidth = 1
                    context.strokeStyle = UISettings.bgLight
                    context.clearRect(0, 0, width, height)

                    if (tickSize <= 0)
                        return

                    var divNum = width / tickSize
                    var absPos = parseInt((x + width) / tickSize) * tickSize
                    var xPos = absPos - x

                    context.beginPath()

                    // paint dividers from the end to the beginning
                    for (var i = 0; i < divNum; i++)
                    {
                        // don't even bother to paint if we're outside the timeline
                        if (absPos >= 0)
                        {
                            if (subDividers > 1)
                            {
                                var subX = xPos - (tickSize / subDividers)
                                for (var sd = 0; sd < subDividers - 1; sd++)
                                {
                                    context.moveTo(subX, 0)
                                    context.lineTo(subX, height)
                                    subX -= (tickSize / subDividers)
                                }
                            }

                            context.moveTo(xPos, 0)
                            context.lineTo(xPos, height)
                        }
                        absPos -= tickSize
                        xPos -= tickSize
                    }
                    context.closePath()
                    context.stroke()
                }
            }

            /* Snap-to-item guide line */
            Rectangle
            {
                id: snapGuide
                x: showManager.snapGuideX
                y: 0
                z: 10
                width: 1
                height: parent.height
                color: "#00FF00"
                visible: showManager.snapGuideX >= 0
            }

            ShowRecordingLane
            {
                id: recordingLane
                y: tracksBox.count * trackHeight
                height: trackHeight
                width: itemsArea.contentWidth
                z: 3
                visible: hasRecordings
                editor: recordings
                showId: showMgrContainer.showID
                onReveal: (ids) =>
                {
                    showMgrContainer.recordingReturnIds = ids.slice()
                    showMgrContainer.recordingReturnShow = showID
                    showMgrContainer.editorTab = 1
                    recordings.revealMembers(ids)
                }

                Row
                {
                    visible: hasRecordings && (recordings.deletedCount > 0 || showCommandRecorder.lastError.length > 0)
                    x: xViewOffset + 4
                    y: recordingLane.height
                    z: 4
                    spacing: 6
                    RobotoText
                    {
                        Accessible.role: Accessible.StaticText
                        Accessible.id: "recordingEditFeedback"
                        Accessible.name: label
                        label: recordings.deletedCount > 0 ? qsTr("Deleted %n event(s)", "", recordings.deletedCount)
                                                           : showCommandRecorder.lastError
                    }
                    GenericButton
                    {
                        activeFocusOnTab: true
                        Keys.onPressed: (event) => {
                            if (event.key === Qt.Key_Space || event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                                if (!event.isAutoRepeat)
                                    clicked(Qt.LeftButton)
                                event.accepted = true
                            }
                        }
                        visible: recordings.deletedCount > 0
                        height: UISettings.listItemHeight
                        label: qsTr("Undo")
                        Accessible.id: "recordingDeleteUndo"
                        onClicked:
                        {
                            if (tardis.undoCommandEdit(recordings.deletedSerial))
                                recordings.deletedCount = 0
                        }
                    }
                }
            }

            DropArea
            {
                id: newFuncDrop
                x: xViewOffset
                width: showMgrContainer.width - trackWidth
                height: tracksBox.count * trackHeight
                z: 2
                enabled: !showManager.readOnly

                keys: [ "function" ]
                onDropped:
                {
                    console.log("Function items dropped here. x: " + drag.x + " y: " + drag.y)

                    /* Check if the dragging was started from a Function Manager */
                    if (drag.source.hasOwnProperty("fromFunctionManager"))
                    {
                        var trackIdx = (itemsArea.contentY + drag.y) / trackHeight
                        var fTime
                        if (showManager.timeBasedDivision)
                            fTime = TimeUtils.posToMs(itemsArea.contentX + drag.x, timeScale, tickSize)
                        else
                            fTime = TimeUtils.posToBeat(itemsArea.contentX + drag.x, tickSize, showManager.beatsDivision)
                        console.log("Drop on time1: " + fTime)
                        showManager.addItems(itemsArea.contentItem, trackIdx, fTime, drag.source.itemsList)
                    }
/*
                    if (drag.source.funcID !== showID)
                    {
                        showManager.addItem(itemsArea.contentItem, trackIdx, fTime, drag.source.funcID)
                    }
                    else
                    {
                        var args = []
                        actionManager.requestActionPopup(ActionManager.None,
                                                         qsTr("Cannot drag a Show into itself!"),
                                                         ActionManager.OK, args)
                    }
*/
                }
            }

            Rectangle
            {
                id: newTrackBox
                x: xViewOffset
                y: (tracksBox.count + (hasRecordings ? 1 : 0)) * trackHeight
                height: trackHeight
                width: itemsArea.width
                color: "transparent"
                radius: 10

                RobotoText
                {
                    id: ntText
                    visible: false
                    anchors.centerIn: parent
                    label: qsTr("Create a new track")
                }

                DropArea
                {
                    id: funcNewTrackDrop
                    anchors.fill: parent
                    enabled: !showManager.readOnly

                    keys: [ "function" ]

                    states: [
                        State
                        {
                            when: funcNewTrackDrop.containsDrag
                            PropertyChanges
                            {
                                target: newTrackBox
                                color: "#3F00FF00"
                            }
                            PropertyChanges
                            {
                                target: ntText
                                visible: true
                            }
                        }
                    ]

                    onDropped:
                    {
                        console.log("Function item dropped here. x: " + drag.x + " y: " + drag.y)

                        /* Check if the dragging was started from a Function Manager */
                        if (drag.source.hasOwnProperty("fromFunctionManager"))
                        {
                            var fTime

                            if (showManager.timeBasedDivision)
                                fTime = TimeUtils.posToMs(xViewOffset + drag.x, timeScale, tickSize)
                            else
                                fTime = TimeUtils.posToBeat(xViewOffset + drag.x, tickSize, showManager.beatsDivision)

                            console.log("Drop on time2: " + fTime)
                            showManager.addItems(itemsArea.contentItem, -1, fTime, drag.source.itemsList)
                        }
                    }
                }
            }
        } // Flickable (horizontal)
    } // Flickable (vertical)

    Rectangle
    {
        anchors.centerIn: parent
        width: parent.width / 3
        height: UISettings.bigItemHeight
        visible: !showManager.isEditing
        radius: height / 4
        color: UISettings.bgLight

        RobotoText
        {
            anchors.centerIn: parent
            label: qsTr("Create/Edit a Show function or\ndrag a function on the timeline")
        }
    }

    CustomScrollBar
    {
        id: horScrollBar
        visible: showMgrContainer.editorTab === 0
        x: timelineHeader.x
        y: showMgrContainer.height - height
        z: 10
        width: timelineHeader.width
        orientation: Qt.Horizontal
    }
    // the Recordings tab: the recorded commands of the selected Show, with
    // the same REC control, playhead and transport in the top bar
    ShowCommandList
    {
        previousFocus: recordingsTabButton
        id: recordings
        visible: showMgrContainer.editorTab === 1 && showManager.isEditing
        y: topBar.height
        z: 4
        width: showMgrContainer.width - rightPanel.width
        height: showMgrContainer.height - topBar.height - (bottomPanel.visible ? bottomPanel.height : 0)
    }

}
