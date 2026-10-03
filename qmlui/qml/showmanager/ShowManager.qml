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
    readonly property bool recordingKeyDomain: {
        if (editorTab === 1 || recordings.selectedIds.length > 0)
            return true
        for (var item = qlcplus.activeFocusItem; item && item !== showMgrContainer; item = item.parent)
            if (item.recordingKeyDomain === true)
                return true
        return false
    }

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
            copyInDomain()
        else if (event.modifiers === Qt.ControlModifier && event.key === Qt.Key_V)
            pasteInDomain()
        else if (event.key === Qt.Key_Delete || event.key === Qt.Key_Backspace)
        {
            if (!recordingKeyDomain && canEdit)
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
    // the one horizontal view of the ruler and the lanes: commands write it, a user scroll
    // of either Flickable writes its contentX and feeds it back; both follow it only here
    property real xViewOffset: 0
    // the width of that view: the timeline columns left of the right panel. The ruler and
    // the lanes are exactly this wide, so every reveal, fit and clamp measures what is seen
    readonly property real timelineViewportWidth: width - trackWidth - verticalDivider.width - rightPanel.width
    onXViewOffsetChanged:
    {
        if (itemsArea.contentX !== xViewOffset)
            itemsArea.contentX = xViewOffset
        if (timelineHeader.contentX !== xViewOffset)
            timelineHeader.contentX = xViewOffset
    }

    property real showID: showManager.currentShowID
    property int selectedTrackIndex: -1
    // Qt's Text and Text|List Tab policies (macOS) skip the timeline's buttons and
    // recordings; there one Show-local owner walks the timeline's stops instead
    readonly property bool reducedTimelineTraversal: timelineShown && (tracksBox.count > 0 || hasRecordings)
        && (Qt.styleHints.tabFocusBehavior === Qt.TabFocusTextControls
            || Qt.styleHints.tabFocusBehavior === (Qt.TabFocusTextControls | Qt.TabFocusListControls))

    // the timeline's stops in order, read from the current items: every track's
    // controls, the Recordings header top to bottom, then its objects in global group order
    function timelineStops()
    {
        var stops = []
        var add = function(item) {
            if (item && item.visible && item.enabled)
                stops.push(item)
        }
        for (var t = 0; t < tracksBox.count; t++)
        {
            var track = tracksBox.itemAt(t)
            for (var control = track ? track.firstControl : null; control;
                 control = control === track.lastControl ? null : control.KeyNavigation.tab)
                add(control)
        }
        if (hasRecordings)
        {
            add(lanesToggle)
            for (var i = 0; i < recordingActions.children.length; i++)
                add(recordingActions.children[i])
            recordingLane.orderedObjects().forEach(add)
            add(recordingUndo)
        }
        return stops
    }

    // the one ordered-neighbour decision over [lead..., stops..., exit];
    // null leaves the step to Qt's own chain
    function orderedNeighbor(sequence, current, forward)
    {
        var index = forward ? sequence.indexOf(current) : sequence.lastIndexOf(current)
        if (index < 0 || index === (forward ? sequence.length - 1 : 0))
            return null
        return sequence[index + (forward ? 1 : -1)]
    }

    // the Split's boundary before the timeline: the colour button while it takes focus,
    // else the Split tab itself, whose Tab then goes on by the owner or Qt's own chain
    readonly property Item splitTimelineBefore: colPickButton.enabled ? colPickButton : splitTabButton

    // the explicit route through the timeline: its lead, its stops, then its exit. Only the
    // lead's last control and the exit are the segment's neighbours, so no other control
    // enters it. In the Split the table's filter follows the timeline and the tab bar's side
    // leads in. Elsewhere Qt's reduced policies skip the toolbar's buttons: from the tab bar's
    // side the toolbar's KeyNavigation chain leads on to Move by, then the seconds field and
    // the Show name, the controls that take focus now
    function timelineLead()
    {
        if (editorTab === 2)
            return [splitTimelineBefore]
        var lead = [splitTimelineBefore]
        for (var item = markersCombo; item; item = item === moveStep ? null : item.KeyNavigation.tab)
            lead.push(item)
        lead.push(timeDelta, showName)
        return lead.filter(function(control) { return control.visible && control.enabled })
    }
    readonly property Item timelineExit: editorTab === 2 ? recordings.firstFocus : timelineTabButton

    function timelineTraversalTarget(current, forward)
    {
        var stops = timelineStops()
        if (stops.length === 0)
            return null
        return orderedNeighbor(timelineLead().concat(stops, [timelineExit]), current, forward)
    }

    // the one vertical reveal: a stop that takes focus, by any route, scrolls into the
    // timeline's viewport; a key that leaves focus where it is scrolls nothing
    function revealInTimeline(control)
    {
        var top = control.mapToItem(showContents.contentItem, 0, 0).y
        var offset = showContents.contentY
        if (top < offset)
            offset = top
        else if (top + control.height > offset + showContents.height)
            offset = top + control.height - showContents.height
        showContents.contentY = Math.max(0, Math.min(offset, showContents.contentHeight - showContents.height))
    }
    readonly property Item focusedItem: qlcplus.activeFocusItem
    onFocusedItemChanged: if (timelineShown && focusedItem && timelineStops().indexOf(focusedItem) >= 0)
                              revealInTimeline(focusedItem)

    // the focused Show control hands Tab and Shift+Tab to this one owner first;
    // every other key, and every step it leaves to Qt, passes on unchanged
    Item
    {
        id: timelineTraversal
        Keys.onPressed: (event) =>
        {
            if ((event.key !== Qt.Key_Tab && event.key !== Qt.Key_Backtab)
                || (event.modifiers & (Qt.ControlModifier | Qt.AltModifier)))
                return
            var forward = event.key === Qt.Key_Tab && !(event.modifiers & Qt.ShiftModifier)
            var target = timelineTraversalTarget(qlcplus.activeFocusItem, forward)
            if (!target)
                return
            target.forceActiveFocus(forward ? Qt.TabFocusReason : Qt.BacktabFocusReason)
            event.accepted = true
        }
    }
    readonly property Item timelineKeySource: reducedTimelineTraversal && keyboardFocusOutline.local
        ? qlcplus.activeFocusItem : null
    Binding
    {
        target: timelineKeySource ? timelineKeySource.Keys : null
        property: "forwardTo"
        value: timelineTraversal
        when: timelineKeySource !== null
        restoreMode: Binding.RestoreBindingOrValue
    }
    // 0: the timeline, 1: the recorded commands of the same Show, 2: both,
    // the timeline above the same Recordings table
    property int editorTab: 0
    // the Recordings lane as one row per recorded control; a view only, for the editor session
    property bool lanesExpanded: false
    readonly property bool timelineShown: editorTab !== 1
    readonly property bool recordingsShown: editorTab !== 0
    readonly property real editorBottom: height - (bottomPanel.visible ? bottomPanel.height : 0)
    // the split halves the editor, leaving the table its controls and two rows
    // where the timeline's ruler and one track leave room for them
    readonly property real timelineFloor: topBar.height + headerHeight + trackHeight
    readonly property real timelineBottom: editorTab === 2
        ? Math.max(timelineFloor,
                   Math.min(topBar.height + Math.round((editorBottom - topBar.height) / 2),
                            editorBottom - recordings.minimumHeight))
        : editorBottom
    property var recordingReturnIds: []
    property real recordingReturnShow: -1

    function returnToTimeline()
    {
        // only leaving the Recordings tab returns to what was opened from the timeline
        var fromRecordings = editorTab === 1
        editorTab = 0
        if (fromRecordings && recordingReturnShow === showID && recordingReturnIds.length)
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

    onShowIDChanged: { resetSelectionNavigationView(); renderAndCenter() }
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
        id: keyboardFocusOutline
        objectName: "showKeyboardFocus"
        property var target: qlcplus.activeFocusItem
        readonly property bool local: {
            for (var item = target; item; item = item.parent)
                if (item === showMgrContainer)
                    return target !== showMgrContainer
            return false
        }
        // the target's rectangle, cut to every clipping ancestor: the outline
        // never paints past the pane that shows the target (the split's table)
        property rect frame: {
            showContents.contentY; xViewOffset; rightPanel.width; showMgrContainer.width; showMgrContainer.height
            if (!target)
                return Qt.rect(0, 0, 0, 0)
            target.x; target.y; target.width; target.height
            var r = target.mapToItem(showMgrContainer, Qt.rect(0, 0, target.width, target.height))
            for (var a = target.parent; a && a !== showMgrContainer; a = a.parent)
            {
                // read so that moving or scrolling any ancestor recomputes the frame
                a.x; a.y; a.width; a.height; a.contentX; a.contentY
                if (!a.clip)
                    continue
                var c = a.mapToItem(showMgrContainer, Qt.rect(0, 0, a.width, a.height))
                var left = Math.max(r.x, c.x), top = Math.max(r.y, c.y)
                var right = Math.min(r.x + r.width, c.x + c.width), bottom = Math.min(r.y + r.height, c.y + c.height)
                r = Qt.rect(left, top, Math.max(0, right - left), Math.max(0, bottom - top))
            }
            return r
        }
        x: frame.x
        y: frame.y
        width: frame.width
        height: frame.height
        visible: local && target.visible && frame.width > 0 && frame.height > 0
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

    // one Copy/Paste for the keys, the toolbar and the shortcuts: recorded
    // events while the recordings own the keys, clips otherwise
    function copyInDomain()
    {
        if (recordingKeyDomain)
            keyboardFeedback = recordings.copySelected()
        else
            requestCopyItems()
    }

    function pasteInDomain()
    {
        if (recordingKeyDomain)
            keyboardFeedback = recordings.pasteAtPlayhead()
        else
            requestPasteItems()
    }

    function centerView()
    {
        var cursorX = showManager.timeBasedDivision
                ? TimeUtils.timeToSize(showManager.currentTime, timeScale, tickSize)
                : TimeUtils.timeToBeatPosition(showManager.currentTime, tickSize,
                                               showManager.bpmNumber, showManager.beatsDivision)
        var xPos = cursorX - (timelineViewportWidth / 2)
        if (xPos >= 0)
            xViewOffset = xPos
    }

    // the timeline's reach past authored and recorded content, raised only by Fit
    // selection: a view in ruler units, reset whenever a ruler unit changes meaning
    property real revealedRulerEnd: 0
    // why the last Fit selection could not show the whole selection; "" otherwise
    property string selectionNavigationStatus: ""
    readonly property real recordedRulerEnd: {
        if (!showCommandRecorder)
            return 0
        // the int extent is negative from 2^31 ms on; the exact group times are not
        var end = Math.max(0, showCommandRecorder.extent)
        showCommandRecorder.groups.forEach(function(group) { end = Math.max(end, group.endTime) })
        return showManager.timeBasedDivision ? end : TimeUtils.msToBeatsMs(end, showManager.bpmNumber)
    }
    readonly property real rulerDuration: Math.min(TimeUtils.selectionNavigationBound,
        Math.max(showManager.showDuration, recordedRulerEnd, revealedRulerEnd))

    function resetSelectionNavigationView()
    {
        revealedRulerEnd = 0
        selectionNavigationStatus = ""
    }
    Connections
    {
        target: showManager
        function onTimeDivisionChanged() { showMgrContainer.revealedRulerEnd = 0 }
        function onBpmNumberChanged() { showMgrContainer.revealedRulerEnd = 0 }
    }
    Connections
    {
        // a workspace load or clear leaves no current Show first
        target: showManager
        function onIsEditingChanged() { showMgrContainer.resetSelectionNavigationView() }
    }
    Connections
    {
        target: recordings
        function onSelectedIdsChanged() { showMgrContainer.selectionNavigationStatus = "" }
    }

    /** The owning selection, ruler and access as plain values; with the view,
      * also the pixels of the selection and of the timeline's viewport */
    function selectionNavigationSnapshot(withView)
    {
        var msRuler = showManager.timeBasedDivision, bpm = showManager.bpmNumber
        var snapshot = {
            showId: showManager.isEditing ? showID : -1,
            domain: !showManager.isEditing ? "none" : recordingKeyDomain ? "recordings" : "clips",
            recordings: { samples: [], tailPx: recordingLane.minItemWidth, rows: null },
            clips: { items: [] },
            ruler: { msRuler: msRuler, bpm: bpm, beatsDivision: showManager.beatsDivision,
                     vdjBeat: showManager.timeDivision === Show.VDJBeat, vdjGridValid: showManager.vdjGridValid,
                     vdjBeatPeriodMs: showManager.vdjBeatPeriodMs },
            access: { readOnly: showManager.readOnly, performState: vdjBridge ? vdjBridge.performState : 0 }
        }
        var content = showContents.contentItem
        if (snapshot.domain === "recordings")
        {
            snapshot.recordings.samples = recordings.selectedRows.map(function(row) {
                return withView ? { id: row.id, ms: row.time, x: recordingLane.timeX(row.time) }
                                : { id: row.id, ms: row.time }
            })
            var rows = withView && recordingLane.visible
                ? recordingLane.selectionRowsRect(snapshot.recordings.samples) : null
            if (rows)
            {
                var dy = recordingLane.mapToItem(content, 0, 0).y
                snapshot.recordings.rows = { y: rows.y + dy, height: rows.height, firstRowY: rows.firstRowY + dy }
            }
        }
        else if (snapshot.domain === "clips" && showManager.selectedItemsCount > 0)
        {
            var refs = showManager.selectedItemRefs()
            var shown = itemsArea.contentItem.children
            for (var i = 0; i < shown.length; i++)
            {
                var item = shown[i]
                if (item.sfRef === undefined || !item.funcRef || refs.indexOf(item.sfRef) < 0)
                    continue
                var clip = { startTime: item.sfRef.startTime, duration: item.sfRef.duration,
                             isBeats: item.funcRef.tempoType === QLCFunction.Beats }
                if (withView)
                {
                    clip.x = item.x
                    clip.width = item.width
                    clip.y = item.mapToItem(content, 0, 0).y
                    clip.height = item.height
                }
                snapshot.clips.items.push(clip)
            }
        }
        if (withView)
            snapshot.view = {
                timeScale: showManager.timeScale, minScale: 0.1,
                unitPx: recordingLane.timeX(msRuler ? 1e6 : TimeUtils.beatsMsToMs(1e6, bpm)) / 1e6,
                rulerDuration: rulerDuration, revealedRulerEnd: revealedRulerEnd,
                x: itemsArea.contentX, headerX: timelineHeader.contentX, width: timelineViewportWidth,
                y: showContents.contentY, height: showContents.height, contentHeight: showContents.contentHeight
            }
        return snapshot
    }
    readonly property var selectionNavigationPlan: TimeUtils.planSelectionNavigation(selectionNavigationSnapshot(false))

    /** The one effect of Fit selection (the view) and Go to selection start (a seek),
      * from a fresh plan: an unavailable action does nothing */
    function applySelectionNavigation(kind)
    {
        var plan = TimeUtils.planSelectionNavigation(selectionNavigationSnapshot(true))
        if (kind === "go")
        {
            if (plan.go.available)
                showManager.requestSeek(plan.go.ms)
            return
        }
        if (!plan.fit.available)
            return
        showManager.timeScale = plan.fit.timeScale
        revealedRulerEnd = Math.max(revealedRulerEnd, plan.fit.rulerEnd)
        xViewOffset = plan.fit.xViewOffset
        showContents.contentY = plan.fit.contentY
        selectionNavigationStatus = TimeUtils.selectionNavigationFitResult(selectionNavigationSnapshot(true)).reason
    }

    function followPlayhead()
    {
        // a recording pointer gesture holds the view until release; the playhead runs on
        if (!timelineHeader || !hdrItem || recordingLane.pointerHeld
                || (!showManager.readOnly && (!showManager.isPlaying || showManager.isPaused))
                || timelineViewportWidth <= 0)
            return

        var cursorX = hdrItem.cursorPosition
        if (cursorX < xViewOffset || cursorX + 1 > xViewOffset + timelineViewportWidth)
            xViewOffset = Math.max(0, cursorX - (timelineViewportWidth / 2))
    }

    Connections
    {
        target: showManager
        function onReadOnlyChanged() { Qt.callLater(showMgrContainer.followPlayhead) }
        function onIsPlayingChanged() { Qt.callLater(showMgrContainer.followPlayhead) }
        function onIsPausedChanged() { Qt.callLater(showMgrContainer.followPlayhead) }
        function onSelectedItemsCountChanged(count)
        {
            showMgrContainer.selectionNavigationStatus = ""
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
                 && (recordingKeyDomain ? recordings.selectedIds.length > 0 : showManager.selectedItemsCount > 0)
        onActivated: showMgrContainer.copyInDomain()
    }

    Shortcut
    {
        sequence: StandardKey.Paste
        enabled: mainView.currentContext === "SHOWMGR"
                 && !mainView.shortcutsBlocked()
                 && (qlcplus.accessMask & App.AC_ShowManager)
        onActivated: showMgrContainer.pasteInDomain()
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
        height: toolbarFlow.height + UISettings.iconSizeDefault
        z: 5
        gradient: Gradient
        {
            GradientStop { position: 0; color: UISettings.toolbarStartSub }
            GradientStop { position: 1; color: UISettings.toolbarEnd }
        }

        Flow
        {
            id: toolbarFlow
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            spacing: 4

            RobotoText { label: qsTr("Name") }

            CustomTextEdit
            {
                id: showName
                width: showMgrContainer.width / 5
                height: UISettings.iconSizeDefault - 10
                text: showManager.showName
                enabled: canEdit

                onTextEdited: showManager.showName = text
            }

            // read-only indicator while VDJ Perform drives this show
            Rectangle
            {
                visible: showManager.readOnly
                width: performBadgeText.width + UISettings.textSizeDefault
                height: UISettings.iconSizeDefault - 10
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

            RecordControl
            {
                width: Math.min(implicitWidth, parent.width)
                height: UISettings.iconSizeDefault
            }

            ButtonGroup { id: editorTabGroup }

            MenuBarEntry
            {
                id: timelineTabButton
                height: UISettings.iconSizeDefault
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
                height: UISettings.iconSizeDefault
                KeyNavigation.backtab: timelineTabButton
                KeyNavigation.tab: splitTabButton
                objectName: "recordingsTab"
                focusPolicy: Qt.StrongFocus
                entryText: qsTr("Recordings")
                checked: showMgrContainer.editorTab === 1
                ButtonGroup.group: editorTabGroup
                onClicked:
                {
                    // the tab itself opens the whole recording; a passage opens from the timeline
                    showMgrContainer.editorTab = 1
                    recordings.showAll()
                }
            }

            MenuBarEntry
            {
                id: splitTabButton
                height: UISettings.iconSizeDefault
                KeyNavigation.backtab: recordingsTabButton
                KeyNavigation.tab: editorTab === 1 ? recordings.firstFocus
                                   : splitTimelineBefore !== splitTabButton ? splitTimelineBefore : null
                objectName: "splitTab"
                focusPolicy: Qt.StrongFocus
                entryText: qsTr("Split")
                Accessible.name: qsTr("Timeline above Recordings")
                checked: showMgrContainer.editorTab === 2
                ButtonGroup.group: editorTabGroup
                onClicked:
                {
                    // from the Timeline tab its selection opens below; an open table, or a
                    // table holding a draft, keeps its rows: switching view never re-scopes a draft
                    var fromTimeline = showMgrContainer.editorTab === 0
                    showMgrContainer.editorTab = 2
                    if (fromTimeline && recordings.selectedIds.length > 0 && recordings.editingId === recordings.noId)
                        recordings.inspect(recordings.selection())
                }
            }

            IconButton
            {
                id: colPickButton
                z: 2
                width: UISettings.iconSizeDefault - 6
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
                width: UISettings.iconSizeDefault - 6
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
                width: UISettings.iconSizeDefault - 6
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
                width: UISettings.iconSizeDefault - 6
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
                width: UISettings.iconSizeDefault - 6
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
                objectName: "copyButton"
                width: UISettings.iconSizeDefault - 6
                height: width
                faSource: FontAwesome.fa_copy
                faColor: UISettings.fgMain
                tooltip: !recordingKeyDomain
                    ? ShortcutUtils.withShortcut(qsTr("Copy the selected items in the clipboard"), "Ctrl+C")
                    : recordings.copyBlockedReason
                      || ShortcutUtils.withShortcut(qsTr("Copy the selected recorded events"), "Ctrl+C")
                enabled: !recordingKeyDomain || recordings.copyBlockedReason === ""
                counter: recordingKeyDomain ? recordings.selectedIds.length : showManager.selectedItemsCount
                onClicked: showMgrContainer.copyInDomain()
            }

            IconButton
            {
                id: pasteBtn
                objectName: "pasteButton"
                width: UISettings.iconSizeDefault - 6
                height: width
                faSource: FontAwesome.fa_paste
                faColor: UISettings.fgMain
                tooltip: !recordingKeyDomain
                    ? ShortcutUtils.withShortcut(qsTr("Paste items in the clipboard at cursor position"), "Ctrl+V")
                    : recordings.pasteBlockedReason
                      || ShortcutUtils.withShortcut(qsTr("Paste the copied recorded events at the cursor"), "Ctrl+V")
                enabled: recordingKeyDomain ? recordings.pasteBlockedReason === "" : canEdit
                counter: recordingKeyDomain ? (showCommandRecorder ? showCommandRecorder.clipboardCount : 0)
                                            : showManager.clipboardItemsCount
                onClicked: showMgrContainer.pasteInDomain()

                CustomPopupDialog
                {
                    id: pasteErrorPopup
                    title: qsTr("Paste error")
                    standardButtons: Dialog.Ok
                    message: qsTr("It is not possible to paste the items on the selected track at the current cursor position")
                }
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
                width: UISettings.iconSizeDefault - 6
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
                width: UISettings.iconSizeDefault - 6
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

            Row
            {
                height: UISettings.iconSizeDefault
                spacing: 4

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
                    KeyNavigation.tab: zoomOutButton
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

                Row
                {
                    width: UISettings.mediumItemHeight * 2.6
                    height: UISettings.iconSizeDefault - 2

                    IconButton
                    {
                        id: zoomOutButton
                        KeyNavigation.tab: zoomInButton
                        width: parent.width / 4
                        height: parent.height
                        focusPolicy: Qt.StrongFocus
                        faSource: FontAwesome.fa_magnifying_glass_minus
                        faColor: "#222"
                        Accessible.name: qsTr("Zoom out")
                        tooltip: Accessible.name
                        onClicked: showMgrContainer.zoomTimeline(false)
                    }
                    IconButton
                    {
                        id: zoomInButton
                        KeyNavigation.tab: fitSelectionButton
                        width: parent.width / 4
                        height: parent.height
                        focusPolicy: Qt.StrongFocus
                        faSource: FontAwesome.fa_magnifying_glass_plus
                        faColor: "#222"
                        Accessible.name: qsTr("Zoom in")
                        tooltip: Accessible.name
                        onClicked: showMgrContainer.zoomTimeline(true)
                    }
                    IconButton
                    {
                        id: fitSelectionButton
                        objectName: "fitSelectionButton"
                        KeyNavigation.tab: goToSelectionStartButton
                        KeyNavigation.backtab: zoomInButton
                        width: parent.width / 4
                        height: parent.height
                        focusPolicy: Qt.StrongFocus
                        faSource: FontAwesome.fa_expand
                        faColor: "#222"
                        enabled: selectionNavigationPlan.fit.available
                        Accessible.name: qsTr("Fit selection")
                        Accessible.description: selectionNavigationStatus || selectionNavigationPlan.fit.reason
                        tooltip: Accessible.name
                        onClicked: showMgrContainer.applySelectionNavigation("fit")
                    }
                    IconButton
                    {
                        id: goToSelectionStartButton
                        objectName: "goToSelectionStartButton"
                        KeyNavigation.tab: moveStep
                        KeyNavigation.backtab: fitSelectionButton
                        width: parent.width / 4
                        height: parent.height
                        focusPolicy: Qt.StrongFocus
                        faSource: FontAwesome.fa_backward_step
                        faColor: "#222"
                        enabled: selectionNavigationPlan.go.available
                        Accessible.name: qsTr("Go to selection start")
                        Accessible.description: selectionNavigationPlan.go.reason
                        tooltip: Accessible.name
                        onClicked: showMgrContainer.applySelectionNavigation("go")
                    }
                }

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
                KeyNavigation.backtab: goToSelectionStartButton
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
            // in this fixed-height row, so a Fit result never reflows the toolbar
            RobotoText
            {
                objectName: "selectionNavigationStatus"
                visible: label !== ""
                label: selectionNavigationStatus
                labelColor: "#f1c40f"
                fontSize: UISettings.textSizeDefault * 0.7
                Accessible.role: Accessible.StaticText
                Accessible.name: label
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
        visible: showMgrContainer.timelineShown
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
        objectName: "timelineHeader"
        x: trackWidth + verticalDivider.width
        y: topBar.height
        z: 4
        visible: showMgrContainer.timelineShown
        height: showMgrContainer.headerHeight
        width: timelineViewportWidth

        boundsBehavior: Flickable.StopAtBounds
        flickableDirection: Flickable.HorizontalFlick

        contentWidth: hdrItem.width //> width ? hdrItem.width : width

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
            cursorHeight: showMgrContainer.timelineBottom - topBar.height
            duration: showMgrContainer.rulerDuration
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
        visible: showMgrContainer.timelineShown
        width: parent.width - rightPanel.width
        height: showMgrContainer.timelineBottom - topBar.height - headerHeight
        clip: true

        boundsBehavior: Flickable.StopAtBounds
        flickableDirection: Flickable.VerticalFlick
        ScrollBar.vertical: CustomScrollBar { }

        contentHeight: totalTracksHeight > height ? totalTracksHeight : height
        //contentWidth: timelineHeader.contentWidth

        property real totalTracksHeight: (tracksBox.count + 2) * trackHeight + (hasRecordings ? recordingLane.height : 0)

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
                            previousControl: index > 0 && tracksBox.itemAt(index - 1)
                                ? tracksBox.itemAt(index - 1).lastControl : null
                            nextControl: index + 1 < tracksBox.count && tracksBox.itemAt(index + 1)
                                ? tracksBox.itemAt(index + 1).firstControl : null

                            onTrackSelected: showMgrContainer.selectedTrackIndex = index
                        }
                }
                Rectangle
                {
                    id: recordingHeader
                    readonly property bool recordingKeyDomain: true
                    visible: hasRecordings
                    width: trackWidth
                    height: recordingLane.height
                    color: UISettings.bgMedium
                    RobotoText
                    {
                        objectName: "recordingsTitle"
                        // centred in the space left of the lanes toggle, never under it;
                        // a count too long for that space shrinks the whole title
                        x: 2
                        width: lanesToggle.x - 4
                        textHAlign: Text.AlignHCenter
                        y: 2
                        label: qsTr("Recordings") + " (" + recordings.selectedIds.length + ")"
                        readonly property real fit: (width - 2) / Math.max(1, titleMetrics.advanceWidth)
                        fontSize: fit < 1 ? Math.floor(UISettings.textSizeDefault * fit) : UISettings.textSizeDefault
                        TextMetrics
                        {
                            id: titleMetrics
                            font.family: UISettings.robotoFontName
                            font.pixelSize: UISettings.textSizeDefault
                            text: qsTr("Recordings") + " (" + recordings.selectedIds.length + ")"
                        }
                    }
                    GenericButton
                    {
                        id: lanesToggle
                        readonly property bool showEnterAction: true
                        activeFocusOnTab: true
                        Keys.onPressed: (event) => {
                            if (event.key === Qt.Key_Space || event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                                if (!event.isAutoRepeat)
                                    clicked(Qt.LeftButton)
                                event.accepted = true
                            }
                        }
                        objectName: "recordingLanesToggle"
                        x: parent.width - width - 2
                        y: 2
                        width: UISettings.listItemHeight
                        height: UISettings.listItemHeight
                        useFontawesome: true
                        label: FontAwesome.fa_list
                        bgColor: showMgrContainer.lanesExpanded ? UISettings.highlight : UISettings.bgControl
                        Accessible.id: "recordingLanesToggle"
                        Accessible.name: showMgrContainer.lanesExpanded ? qsTr("Show recordings in one lane")
                                                                        : qsTr("Show one lane per control")
                        Accessible.checkable: true
                        Accessible.checked: showMgrContainer.lanesExpanded
                        tooltip: Accessible.name
                        onClicked: showMgrContainer.lanesExpanded = !showMgrContainer.lanesExpanded
                    }
                    Repeater
                    {
                        model: recordingLane.rowCount
                        delegate: Rectangle
                        {
                            required property int index
                            readonly property int laneIndex: index
                            readonly property string label: recordingLane.rowLabels[index] || ""
                            objectName: "recordingLaneRow"
                            y: recordingLane.rowTop(index)
                            width: trackWidth
                            height: recordingLane.rowHeight
                            color: "transparent"
                            border.color: UISettings.bgLight
                            Accessible.role: Accessible.StaticText
                            Accessible.name: label
                            RobotoText
                            {
                                x: 4
                                width: parent.width - 8
                                height: parent.height
                                wrapText: true
                                fontSize: UISettings.textSizeDefault * 0.8
                                label: parent.label
                            }
                        }
                    }
                    Row
                    {
                        id: recordingActions
                        y: trackHeight - height
                        anchors.horizontalCenter: parent.horizontalCenter
                        spacing: 2
                        IconButton
                        {
                            width: UISettings.listItemHeight
                            height: width
                            faSource: FontAwesome.fa_backward
                            Accessible.id: "recordingMoveEarlier"
                            Accessible.name: recordings.selectedRows.length > 0
                                ? qsTr("Move %n selected event(s) earlier", "", recordings.selectedRows.length)
                                : qsTr("Move selected recordings earlier")
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
                            Accessible.name: recordings.selectedRows.length > 0
                                ? qsTr("Move %n selected event(s) later", "", recordings.selectedRows.length)
                                : qsTr("Move selected recordings later")
                            tooltip: Accessible.name
                            enabled: recordings.editable && recordings.amountReason === "" && recordings.selectedIds.length > 0
                            onClicked: recordings.moveSelected(1)
                        }
                        GenericButton
                        {
                            readonly property bool showEnterAction: true
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
                            Accessible.name: recordings.selectedRows.length > 0
                                ? qsTr("Snap %n selected event(s)", "", recordings.selectedRows.length) : label
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
            width: timelineViewportWidth
            height: parent.height
            clip: true

            boundsBehavior: Flickable.StopAtBounds
            contentHeight: showContents.contentHeight
            contentWidth: timelineHeader.contentWidth
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
                onWidthChanged: updatePosition()
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
                rowHeight: trackHeight
                expanded: showMgrContainer.lanesExpanded
                width: itemsArea.contentWidth
                z: 3
                visible: hasRecordings
                editor: recordings
                inspectOnSelect: showMgrContainer.editorTab === 2
                showId: showMgrContainer.showID
                onPointerHeldChanged: if (!pointerHeld) Qt.callLater(showMgrContainer.followPlayhead)
                onReveal: (ids) =>
                {
                    // the split already shows the table below
                    if (showMgrContainer.editorTab !== 2)
                    {
                        showMgrContainer.recordingReturnIds = ids.slice()
                        showMgrContainer.recordingReturnShow = showID
                        showMgrContainer.editorTab = 1
                    }
                    recordings.revealMembers(ids)
                }

                Row
                {
                    readonly property bool recordingKeyDomain: true
                    visible: hasRecordings && (recordings.deletedCount > 0 || showCommandRecorder.lastError.length > 0
                                               || recordings.selectionSummary !== "")
                    x: xViewOffset + 4
                    // below the grips, which reach the lane's free inset past its edge
                    y: recordingLane.height + recordingLane.itemInset
                    z: 4
                    spacing: 6
                    RobotoText
                    {
                        objectName: "timelineSelectionSummary"
                        visible: recordings.selectionSummary !== ""
                        Accessible.role: Accessible.StaticText
                        Accessible.name: label
                        label: recordings.selectionSummary
                    }
                    RobotoText
                    {
                        visible: recordings.deletedCount > 0 || showCommandRecorder.lastError.length > 0
                        Accessible.role: Accessible.StaticText
                        Accessible.id: "recordingEditFeedback"
                        Accessible.name: label
                        label: recordings.deletedCount > 0 ? qsTr("Deleted %n event(s)", "", recordings.deletedCount)
                                                           : showCommandRecorder.lastError
                    }
                    GenericButton
                    {
                        readonly property bool showEnterAction: true
                        activeFocusOnTab: true
                        Keys.onPressed: (event) => {
                            if (event.key === Qt.Key_Space || event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                                if (!event.isAutoRepeat)
                                    clicked(Qt.LeftButton)
                                event.accepted = true
                            }
                        }
                        id: recordingUndo
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
                objectName: "newFunctionDrop"
                x: xViewOffset
                width: timelineViewportWidth
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
                objectName: "newTrackBox"
                x: xViewOffset
                y: tracksBox.count * trackHeight + (hasRecordings ? recordingLane.height : 0)
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
        visible: showMgrContainer.timelineShown
        x: timelineHeader.x
        y: (showMgrContainer.editorTab === 2 ? showMgrContainer.timelineBottom : showMgrContainer.height) - height
        z: 10
        width: timelineHeader.width
        orientation: Qt.Horizontal
    }
    // the Recordings tab: the recorded commands of the selected Show, with
    // the same REC control, playhead and transport in the top bar
    // the Recordings tab, or the lower half of the split: one table either way
    ShowCommandList
    {
        // below the timeline, Shift+Tab goes back through the timeline
        previousFocus: showMgrContainer.editorTab === 2 ? null : splitTabButton
        id: recordings
        visible: showMgrContainer.recordingsShown && showManager.isEditing
        y: showMgrContainer.editorTab === 2 ? showMgrContainer.timelineBottom : topBar.height
        z: 4
        width: showMgrContainer.width - rightPanel.width
        height: showMgrContainer.editorBottom - y
    }

}
