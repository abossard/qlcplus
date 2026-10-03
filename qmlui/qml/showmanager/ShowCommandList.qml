/*
  Q Light Controller Plus
  ShowCommandList.qml

  Copyright (c) QLC+ contributors

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
import QtQuick.Controls.Basic

import org.qlcplus.classes 1.0

import "TimeUtils.js" as TimeUtils
import "."

// The Recordings tab of the Show editor: one ordered Time / Control / Action /
// Value list of the selected Show's recorded commands. A click selects, a
// double click edits a time, state or value, Enter commits and Escape
// cancels. Every edit goes through the recorder, also while the Show plays,
// records or follows VDJ Perform, and makes it one undo step.
Rectangle
{
    id: recordingsRoot
    objectName: "recordingsView"
    color: UISettings.bgMedium
    // keys typed in the table are recording keys, in a tab or below the split timeline
    readonly property bool recordingKeyDomain: true

    property var rows: []
    property var rowsById: ({})
    // bumped when row contents change; the shown ids change only with the list
    property int revision: 0
    property var shownIds: []
    // what a row going away shows until the list drops it
    readonly property var emptyRow: ({ time: 0, control: "", detail: "", actionLabel: "", valueText: "",
                                       valueEdit: "", valueField: "" })
    // event and Show ids are unsigned 32 bit: JS numbers hold them exactly,
    // a QML int would not. noId is ShowCommand::InvalidId, never a valid id
    readonly property real noId: 4294967295
    property real rowsShowId: noId
    property var selectedIds: []
    property real anchorId: noId
    property real editingId: noId
    property string editingField: ""
    property string editText: ""
    // what the open draft holds; a recreated row editor starts from it, not from editText
    property string draftText: ""
    // why the last cell edit ended because another recording edit started
    property string draftEndReason: ""
    // the event a draft was opened on: Up and Down go on from it while the list has no current row
    property real keyAnchorId: noId
    property bool draftCommitting: false
    property string filterText: ""
    // an opened passage limits the rows to its events; it is not a selection
    property bool passageActive: false
    property var passageIds: []
    property int filterHiddenCount: 0
    readonly property bool passageEmpty: passageActive && passageIds.length === 0
    property int deletedCount: 0
    property int deletedSerial: 0
    property int stepIndex: 0
    property string timeDeltaText: ""
    property string moveError: ""
    readonly property real stepMs: stepIndex === 3
        ? (/^\d+(\.\d{1,3})?$/.test(timeDeltaText) ? Number(timeDeltaText) * 1000 : 0)
        : (grid !== null && showManager.timeDivision !== Show.Time
           ? stepBars[stepIndex] * grid.beatsPerBar * grid.beatMs : 0)
    readonly property string amountReason: stepMs > 0 && stepMs <= 2147483647 ? ""
        : stepIndex === 3 ? qsTr("Enter a positive time delta in seconds (up to 3 decimals).")
        : qsTr("No musical grid. Choose Time delta (s) in Move by.")
    property Item previousFocus: null
    property alias firstFocus: filterField

    // recorded events stay editable while Perform owns the transport
    readonly property string blockedReason: !showCommandRecorder ? qsTr("No recorder")
                                            : showCommandRecorder.editBlockedReason
    readonly property bool editable: blockedReason === ""
    readonly property var grid: TimeUtils.musicalGrid(showManager.timeDivision === Show.VDJBeat,
                                                      showManager.vdjGridValid, showManager.vdjBeatPeriodMs,
                                                      showManager.vdjGridAnchorMs, showManager.bpmNumber,
                                                      showManager.beatsDivision)
    readonly property var stepBars: [ 1, 0.5, 0.25 ]

    readonly property real timeWidth: width * 0.14
    readonly property real controlWidth: width * 0.46
    readonly property real actionWidth: width * 0.2

    // what Move, Snap and Delete act on, in list order
    readonly property var selectedRows: {
        revision
        return selection().map(function(id) { return rowsById[id] })
                          .filter(function(row) { return row !== undefined })
    }
    readonly property int selectedGroupCount: {
        var ids = selectedIds
        if (!showCommandRecorder || ids.length === 0)
            return 0
        return showCommandRecorder.groups.filter(function(group) {
            return group.eventIds.some(function(id) { return ids.indexOf(id) >= 0 })
        }).length
    }
    readonly property string selectionSummary: selectedRows.length === 0 ? ""
        : qsTr("%n event(s) selected", "", selectedRows.length) + " "
          + qsTr("in %n group(s)", "", selectedGroupCount) + ", "
          + TimeUtils.msToString(selectedRows[0].time) + " – "
          + TimeUtils.msToString(selectedRows[selectedRows.length - 1].time)

    function matches(row, text)
    {
        if (text === "")
            return true
        var needle = text.toLowerCase()
        return [ row.control, row.detail, row.actionLabel, row.valueText ].some(function(field) {
            return String(field).toLowerCase().indexOf(needle) >= 0
        })
    }

    function reload()
    {
        var next = showCommandRecorder ? showCommandRecorder.commands : []
        var showId = next.length ? next[0].showId : showManager.currentShowID
        if (showId !== rowsShowId)
        {
            // an edit, a selection or an Undo prompt belongs to its own Show
            cancelEdit()
            draftEndReason = ""
            selectedIds = []
            anchorId = noId
            deletedCount = 0
            passageActive = false
            passageIds = []
            rowsShowId = showId
        }
        rows = next
        var byId = {}
        rows.forEach(function(row) { byId[row.id] = row })
        rowsById = byId
        revision++
        var ids = rows.map(function(row) { return row.id })
        var kept = passageIds.filter(function(id) { return ids.indexOf(id) >= 0 })
        if (kept.length !== passageIds.length)
            passageIds = kept
        updateShown()
        selectedIds = selectedIds.filter(function(id) { return ids.indexOf(id) >= 0 })
        if (editingId !== noId && ids.indexOf(editingId) < 0)
            endDraft(qsTr("Event %1 was deleted; its cell edit ended").arg(String(editingId)))
        if (showCommandRecorder && showCommandRecorder.lastEditSerial !== deletedSerial)
            deletedCount = 0
    }

    // filtering hides rows; order and data stay the recorder's. A row keeps
    // its delegate, and a cell edit in it, while its event stays shown.
    function updateShown()
    {
        var scoped = passageActive ? rows.filter(function(row) { return passageIds.indexOf(row.id) >= 0 }) : rows
        var ids = scoped.filter(function(row) { return matches(row, filterText) })
                        .map(function(row) { return row.id })
        filterHiddenCount = scoped.length - ids.length
        if (ids.length !== shownIds.length || ids.some(function(id, i) { return id !== shownIds[i] }))
        {
            syncShownRows(ids)
            shownIds = ids
        }
    }

    // the list's rows keyed by event id: removed, moved and inserted, never replaced
    function syncShownRows(ids)
    {
        var wanted = {}
        ids.forEach(function(id) { wanted[id] = true })
        for (var i = shownRows.count - 1; i >= 0; i--)
            if (!wanted[shownRows.get(i).eventId])
                shownRows.remove(i)
        for (var j = 0; j < ids.length; j++)
        {
            if (j < shownRows.count && shownRows.get(j).eventId === ids[j])
                continue
            var from = -1
            for (var k = j + 1; k < shownRows.count; k++)
                if (shownRows.get(k).eventId === ids[j])
                {
                    from = k
                    break
                }
            if (from >= 0)
                shownRows.move(from, j, 1)
            else
                shownRows.insert(j, { eventId: ids[j] })
        }
        // the rows are there at once, as a replaced model's were, not at the next frame
        commandView.forceLayout()
    }

    // limit the rows to exactly these events until showAll(); later regrouping never adds any
    function inspect(ids)
    {
        passageIds = rows.map(function(row) { return row.id })
                         .filter(function(id) { return ids.indexOf(id) >= 0 })
        passageActive = true
        updateShown()
    }

    function showAll()
    {
        passageActive = false
        passageIds = []
        updateShown()
    }

    function visibleIds()
    {
        return shownIds
    }

    // the selection in list order
    function selection()
    {
        return rows.map(function(row) { return row.id })
                   .filter(function(id) { return selectedIds.indexOf(id) >= 0 })
    }

    readonly property string selectionEndsDraft: qsTr("The selection changed, so the cell edit ended without a change")

    function selectMembers(ids, modifiers)
    {
        endDraft(selectionEndsDraft)
        showManager.resetItemsSelection()
        if ((modifiers & Qt.ShiftModifier) && anchorId !== noId)
        {
            var ordered = rows.map(function(row) { return row.id })
            var from = ordered.indexOf(anchorId), first = ordered.indexOf(ids[0])
            var last = ordered.indexOf(ids[ids.length - 1])
            if (from >= 0 && first >= 0 && last >= 0)
            {
                selectedIds = ordered.slice(Math.min(from, first), Math.max(from, last) + 1)
                return
            }
        }
        if (modifiers & (Qt.ControlModifier | Qt.MetaModifier))
        {
            var all = ids.every(function(id) { return selectedIds.indexOf(id) >= 0 })
            selectedIds = all ? selectedIds.filter(function(id) { return ids.indexOf(id) < 0 })
                              : selectedIds.concat(ids.filter(function(id) { return selectedIds.indexOf(id) < 0 }))
        }
        else
            selectedIds = ids.slice()
        anchorId = ids[0]
    }

    function revealMembers(ids)
    {
        // opening from the timeline ends a draft first, so it adds no row
        endDraft(selectionEndsDraft)
        inspect(ids)
        filterField.text = ""
        selectMembers(ids, Qt.NoModifier)
        Qt.callLater(function() {
            commandView.positionViewAtIndex(shownIds.indexOf(ids[0]), ListView.Contain)
            commandView.currentIndex = shownIds.indexOf(ids[0])
            commandView.forceActiveFocus()
        })
    }

    function clickRow(id, modifiers)
    {
        endDraft(selectionEndsDraft)
        showManager.resetItemsSelection()
        commandView.forceActiveFocus()
        if ((modifiers & Qt.ShiftModifier) && anchorId !== noId)
        {
            var ids = visibleIds()
            var from = ids.indexOf(anchorId), to = ids.indexOf(id)
            if (from >= 0 && to >= 0)
            {
                selectedIds = ids.slice(Math.min(from, to), Math.max(from, to) + 1)
                return
            }
        }
        if (modifiers & (Qt.ControlModifier | Qt.MetaModifier))
        {
            var toggled = selectedIds.slice()
            var at = toggled.indexOf(id)
            if (at >= 0)
                toggled.splice(at, 1)
            else
                toggled.push(id)
            selectedIds = toggled
        }
        else
        {
            selectedIds = [ id ]
        }
        anchorId = id
    }

    function fieldAt(x, row)
    {
        if (x < timeWidth)
            return "time"
        if (x >= timeWidth + controlWidth + actionWidth)
            return row.valueField
        return ""
    }

    function beginEdit(row, field)
    {
        // asking again for the cell already being edited keeps its draft
        if (!editable || field === "" || (editingId === row.id && editingField === field))
            return
        cancelEdit()
        // the draft edits the event as it is now: a change to it meanwhile ends the draft with a reason
        if (!showCommandRecorder.beginEditSession(row.showId, [row.id]))
            return
        draftEndReason = ""
        // an explicitly cleared current row: ListView moves the keys onto its current row
        // whenever rows above that come or go, which would take them from the draft
        commandView.currentIndex = -1
        keyAnchorId = row.id
        editText = field === "time" ? (row.time / 1000).toFixed(3) : row.valueEdit
        draftText = editText
        editingField = field
        editingId = row.id
    }

    function closeDraft()
    {
        // the keys stay in the list when its editor goes; a draft ended from elsewhere leaves them there
        var keys = commandView.activeFocus
        editingId = noId
        editingField = ""
        if (keys)
            commandView.forceActiveFocus()
    }

    function cancelEdit()
    {
        if (editingId === noId)
            return
        closeDraft()
        showCommandRecorder.cancelEditSession()
    }

    // the table keeps the reason itself: the publication that ended the draft may
    // clear the recorder's error once it is done
    function endDraft(reason)
    {
        if (editingId === noId)
            return
        if (reason)
            draftEndReason = reason
        closeDraft()
        showCommandRecorder.cancelEditSession(reason)
    }

    function commitEdit(text)
    {
        if (editingId === noId)
            return
        if (text === editText)
        {
            cancelEdit()
            return
        }
        draftCommitting = true
        var committed = showCommandRecorder.commitCellText(editingField, text)
        draftCommitting = false
        // text that is no value keeps the editor open; an event changed meanwhile ends the
        // draft without writing over it. Either way the recorder states the reason
        if (committed || !showCommandRecorder.editSessionActive)
            closeDraft()
    }

    function deleteSelected()
    {
        var ids = selection()
        if (!editable || ids.length === 0)
            return
        if (showCommandRecorder.removeCommands(rowsShowId, ids))
        {
            deletedSerial = showCommandRecorder.lastEditSerial
            deletedCount = ids.length
        }
    }

    function moveSelected(direction, endpoint)
    {
        var ids = selection()
        moveError = endpoint ? qsTr("Recording samples have no scheduling endpoint.")
                            : !editable ? blockedReason : amountReason
        if (moveError !== "" || ids.length === 0)
            return
        showCommandRecorder.moveCommands(rowsShowId, ids, direction, stepMs)
    }

    function snapSelected()
    {
        var ids = selection()
        if (!editable || grid === null || ids.length === 0)
            return
        showCommandRecorder.snapCommands(rowsShowId, ids, grid.beatMs, grid.anchorMs)
    }

    readonly property string copyBlockedReason: selectedIds.length === 0 ? qsTr("Select recorded events to copy") : ""
    readonly property string pasteBlockedReason: !showCommandRecorder || showCommandRecorder.clipboardCount === 0
        ? qsTr("Copy recorded events first")
        : !editable ? blockedReason
        : editingId !== noId && draftText !== editText
        ? qsTr("Press Enter to commit or Escape to cancel the cell edit first") : ""

    // Copy and Paste of the Show Manager's keys, buttons and shortcuts; each
    // returns what to tell the user
    function copySelected()
    {
        var ids = selection()
        if (copyBlockedReason !== "")
            return copyBlockedReason
        if (!showCommandRecorder.copyCommands(rowsShowId, ids))
            return showCommandRecorder.lastError
        return qsTr("Copied %n event(s)", "", ids.length)
    }

    function pasteAtPlayhead()
    {
        if (pasteBlockedReason !== "")
            return pasteBlockedReason
        // refused, nothing here changes
        var ids = showCommandRecorder.pasteCommands(rowsShowId, showManager.currentTime)
        if (ids.length === 0)
            return showCommandRecorder.lastError
        revealMembers(ids)
        var notReady = ids.filter(function(id) {
            return rowsById[id] !== undefined && rowsById[id].controlReady === false
        }).length
        return notReady > 0
            ? qsTr("Pasted %1 event(s); %2 of %1 refer to a control that is not ready").arg(ids.length).arg(notReady)
            : qsTr("Pasted %n event(s)", "", ids.length)
    }

    onFilterTextChanged:
    {
        updateShown()
        var ids = visibleIds()
        selectedIds = selectedIds.filter(function(id) { return ids.indexOf(id) >= 0 })
    }
    onVisibleChanged:
    {
        if (visible)
            reload()
    }
    Component.onCompleted:
    {
        reload()
    }

    Connections
    {
        target: showCommandRecorder
        function onCommandsChanged() { recordingsRoot.reload() }
        // one edit at a time: another one, a Show change or a new workspace ends the draft
        function onEditSessionChanged()
        {
            if (recordingsRoot.editingId === recordingsRoot.noId || recordingsRoot.draftCommitting
                    || showCommandRecorder.editSessionActive)
                return
            var why = showCommandRecorder.editSessionEndReason
            recordingsRoot.draftEndReason = why !== "" ? qsTr("The cell edit ended: %1").arg(why)
                                                       : qsTr("The cell edit ended")
            recordingsRoot.closeDraft()
        }
    }

    // the application routes a plain Delete to the timeline's selection;
    // while this list has the focus, Delete is this list's
    Shortcut
    {
        sequence: "Delete"
        enabled: recordingsRoot.visible && commandView.activeFocus && recordingsRoot.editingId === recordingsRoot.noId
        onActivated: recordingsRoot.deleteSelected()
    }

    // the controls, notices, the passage line (held even while hidden, so the split
    // does not move when it appears) and column titles, plus two rows
    readonly property real minimumHeight: tableLayout.implicitHeight
        + (infoLine.visible ? 0 : UISettings.listItemHeight + tableLayout.spacing)
        + Math.ceil(2 * UISettings.listItemHeight * 1.4) + 8

    ColumnLayout
    {
        id: tableLayout
        anchors.fill: parent
        anchors.margins: 4
        spacing: 4

        RowLayout
        {
            Layout.fillWidth: true
            spacing: 6

            CustomTextEdit
            {
                id: filterField
                KeyNavigation.backtab: recordingsRoot.previousFocus
                KeyNavigation.tab: earlierButton
                objectName: "recordingsFilter"
                implicitWidth: UISettings.bigItemHeight * 2.5
                placeholderText: qsTr("Filter")
                onTextChanged: recordingsRoot.filterText = text
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: "transparent" }

            IconButton
            {
                id: earlierButton
                KeyNavigation.tab: laterButton
                objectName: "moveEarlier"
                width: UISettings.listItemHeight
                height: width
                faSource: FontAwesome.fa_backward
                tooltip: recordingsRoot.selectedRows.length > 0
                         ? qsTr("Move %n selected event(s) earlier (Left)", "", recordingsRoot.selectedRows.length)
                         : qsTr("Move the selection earlier (Left)")
                Accessible.name: recordingsRoot.selectedRows.length > 0
                                 ? qsTr("Move %n selected event(s) earlier", "", recordingsRoot.selectedRows.length)
                                 : qsTr("Move the selection earlier")
                enabled: recordingsRoot.editable && recordingsRoot.amountReason === "" && recordingsRoot.selectedIds.length > 0
                         && !recordingsRoot.passageEmpty
                onClicked: recordingsRoot.moveSelected(-1)
            }

            IconButton
            {
                id: laterButton
                KeyNavigation.tab: snapAction
                objectName: "moveLater"
                width: UISettings.listItemHeight
                height: width
                faSource: FontAwesome.fa_forward
                tooltip: recordingsRoot.selectedRows.length > 0
                         ? qsTr("Move %n selected event(s) later (Right)", "", recordingsRoot.selectedRows.length)
                         : qsTr("Move the selection later (Right)")
                Accessible.name: recordingsRoot.selectedRows.length > 0
                                 ? qsTr("Move %n selected event(s) later", "", recordingsRoot.selectedRows.length)
                                 : qsTr("Move the selection later")
                enabled: recordingsRoot.editable && recordingsRoot.amountReason === "" && recordingsRoot.selectedIds.length > 0
                         && !recordingsRoot.passageEmpty
                onClicked: recordingsRoot.moveSelected(1)
            }

            GenericButton
            {
                id: snapAction
                KeyNavigation.tab: deleteAction
                activeFocusOnTab: true
                readonly property bool showEnterAction: true
                Keys.onPressed: (event) => {
                    if (event.key === Qt.Key_Space || event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                        if (!event.isAutoRepeat)
                            clicked(Qt.LeftButton)
                        event.accepted = true
                    }
                }
                objectName: "snapButton"
                Accessible.id: "snapButton"
                height: UISettings.listItemHeight
                label: qsTr("Snap")
                Accessible.name: recordingsRoot.selectedRows.length > 0
                                 ? qsTr("Snap %n selected event(s)", "", recordingsRoot.selectedRows.length) : label
                tooltip: qsTr("Put the earliest selected event on the nearest beat, the others with it")
                enabled: recordingsRoot.editable && recordingsRoot.grid !== null && recordingsRoot.selectedIds.length > 0
                         && !recordingsRoot.passageEmpty
                onClicked: recordingsRoot.snapSelected()
            }

            IconButton
            {
                id: deleteAction
                KeyNavigation.tab: recordingsRoot.passageActive ? showAllAction
                                   : recordingsRoot.deletedCount > 0 ? undoAction : commandView
                objectName: "deleteButton"
                width: UISettings.listItemHeight
                height: width
                faSource: FontAwesome.fa_trash
                tooltip: recordingsRoot.selectedRows.length > 0
                         ? qsTr("Delete %n selected event(s) (Delete)", "", recordingsRoot.selectedRows.length)
                         : qsTr("Delete the selection (Delete)")
                Accessible.name: recordingsRoot.selectedRows.length > 0
                                 ? qsTr("Delete %n selected event(s)", "", recordingsRoot.selectedRows.length)
                                 : qsTr("Delete the selection")
                enabled: recordingsRoot.editable && recordingsRoot.selectedIds.length > 0 && !recordingsRoot.passageEmpty
                onClicked: recordingsRoot.deleteSelected()
            }
        }

        // an opened passage: its size, what the filter hides of it, and the way
        // back; and what Move, Snap and Delete act on. The line wraps, each text
        // keeps its own width
        Flow
        {
            id: infoLine
            visible: recordingsRoot.passageActive || recordingsRoot.selectionSummary !== ""
            Layout.fillWidth: true
            spacing: 8

            RobotoText
            {
                objectName: "passageSummary"
                visible: recordingsRoot.passageActive
                Accessible.role: Accessible.StaticText
                Accessible.name: label
                height: UISettings.listItemHeight
                textVAlign: Text.AlignVCenter
                label: qsTr("Passage: %n event(s)", "", recordingsRoot.passageIds.length)
                       + (recordingsRoot.filterHiddenCount > 0
                          ? ", " + qsTr("%n hidden by filter", "", recordingsRoot.filterHiddenCount) : "")
                fontSize: UISettings.textSizeDefault * 0.8
            }

            GenericButton
            {
                id: showAllAction
                visible: recordingsRoot.passageActive
                KeyNavigation.backtab: deleteAction
                KeyNavigation.tab: recordingsRoot.deletedCount > 0 ? undoAction : commandView
                activeFocusOnTab: true
                readonly property bool showEnterAction: true
                Keys.onPressed: (event) => {
                    if (event.key === Qt.Key_Space || event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                        if (!event.isAutoRepeat)
                            clicked(Qt.LeftButton)
                        event.accepted = true
                    }
                }
                objectName: "passageShowAll"
                height: UISettings.listItemHeight
                label: qsTr("Show all")
                tooltip: qsTr("Show every recorded event of this Show again")
                onClicked: recordingsRoot.showAll()
            }

            RobotoText
            {
                objectName: "selectionSummary"
                visible: recordingsRoot.selectionSummary !== ""
                Accessible.role: Accessible.StaticText
                Accessible.name: label
                height: UISettings.listItemHeight
                textVAlign: Text.AlignVCenter
                label: recordingsRoot.selectionSummary
                fontSize: UISettings.textSizeDefault * 0.8
            }
        }

        RobotoText
        {
            objectName: "passageEmpty"
            Accessible.role: Accessible.StaticText
            Accessible.name: label
            visible: recordingsRoot.passageEmpty
            Layout.fillWidth: true
            label: qsTr("No event of this passage is left. Use Show all for the whole recording.")
            fontSize: UISettings.textSizeDefault * 0.8
        }

        RobotoText
        {
            objectName: "recordingsBlocked"
            visible: !recordingsRoot.editable
            Layout.fillWidth: true
            label: recordingsRoot.blockedReason
            labelColor: "#f1c40f"
            fontSize: UISettings.textSizeDefault * 0.8
        }

        RobotoText
        {
            objectName: "musicalUnavailable"
            visible: recordingsRoot.grid === null
            Layout.fillWidth: true
            label: qsTr("Musical movement and Snap need BPM markers with a tempo, or a VDJ Beat grid. Times can still be edited.")
            fontSize: UISettings.textSizeDefault * 0.7
        }

        RobotoText
        {
            objectName: "draftEnded"
            visible: recordingsRoot.draftEndReason !== ""
            Layout.fillWidth: true
            label: recordingsRoot.draftEndReason
            labelColor: "#e67e22"
            fontSize: UISettings.textSizeDefault * 0.8
        }

        RobotoText
        {
            objectName: "recordingsError"
            visible: showCommandRecorder && showCommandRecorder.lastError.length > 0
            Layout.fillWidth: true
            label: showCommandRecorder ? showCommandRecorder.lastError : ""
            labelColor: "#e67e22"
            fontSize: UISettings.textSizeDefault * 0.8
        }

        Rectangle
        {
            objectName: "deleteNotice"
            visible: recordingsRoot.deletedCount > 0
            Layout.fillWidth: true
            height: UISettings.listItemHeight
            color: UISettings.bgLight
            radius: 3

            RowLayout
            {
                anchors.fill: parent
                anchors.leftMargin: 6
                spacing: 8

                RobotoText
                {
                    objectName: "deleteNoticeText"
                    label: qsTr("Deleted %n event(s)", "", recordingsRoot.deletedCount)
                    fontSize: UISettings.textSizeDefault * 0.8
                }

                GenericButton
                {
                    id: undoAction
                    // explicit both ways: the one Qt remembers depends on which control tabbed here last
                    KeyNavigation.backtab: recordingsRoot.passageActive ? showAllAction : deleteAction
                    KeyNavigation.tab: commandView
                    activeFocusOnTab: true
                    readonly property bool showEnterAction: true
                    Keys.onPressed: (event) => {
                        if (event.key === Qt.Key_Space || event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                            if (!event.isAutoRepeat)
                                clicked(Qt.LeftButton)
                            event.accepted = true
                        }
                    }
                    objectName: "deleteUndo"
                    height: parent.height - 4
                    label: qsTr("Undo")
                    onClicked:
                    {
                        if (tardis.undoCommandEdit(recordingsRoot.deletedSerial))
                            recordingsRoot.deletedCount = 0
                    }
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: "transparent" }
            }
        }

        // column titles
        Row
        {
            Layout.fillWidth: true
            height: UISettings.listItemHeight * 0.8

            Repeater
            {
                model: [ { t: qsTr("Time"), w: recordingsRoot.timeWidth },
                         { t: qsTr("Control"), w: recordingsRoot.controlWidth },
                         { t: qsTr("Action"), w: recordingsRoot.actionWidth },
                         { t: qsTr("Value"), w: recordingsRoot.width - recordingsRoot.timeWidth
                                                - recordingsRoot.controlWidth - recordingsRoot.actionWidth } ]
                RobotoText
                {
                    required property var modelData
                    width: modelData.w
                    height: parent.height
                    label: modelData.t
                    fontSize: UISettings.textSizeDefault * 0.8
                    fontBold: true
                }
            }
        }

        ListView
        {
            id: commandView
            objectName: "commandView"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            focus: true
            activeFocusOnTab: true
            KeyNavigation.tab: currentItem
            KeyNavigation.backtab: filterField
            boundsBehavior: Flickable.StopAtBounds
            model: ListModel { id: shownRows }
            ScrollBar.vertical: CustomScrollBar { }

            // group commands; an open cell editor keeps its own keys
            Keys.onPressed: (event) =>
            {
                if (recordingsRoot.editingId !== recordingsRoot.noId)
                    return
                var anchor = recordingsRoot.shownIds.indexOf(recordingsRoot.keyAnchorId)
                if ((event.key === Qt.Key_Up || event.key === Qt.Key_Down) && currentIndex < 0 && anchor >= 0)
                {
                    // after a draft: on from the row it was opened on, found by its event
                    currentIndex = Math.max(0, Math.min(count - 1, anchor + (event.key === Qt.Key_Down ? 1 : -1)))
                    if (currentItem)
                        currentItem.forceActiveFocus()
                }
                else if (event.key === Qt.Key_Backspace || event.key === Qt.Key_Delete)
                    recordingsRoot.deleteSelected()
                else if (event.key === Qt.Key_Left)
                    recordingsRoot.moveSelected(-1, event.modifiers & Qt.AltModifier)
                else if (event.key === Qt.Key_Right)
                    recordingsRoot.moveSelected(1, event.modifiers & Qt.AltModifier)
                else if (event.key === Qt.Key_A && (event.modifiers & Qt.ControlModifier))
                    recordingsRoot.selectedIds = recordingsRoot.visibleIds()
                else
                    return
                event.accepted = true
            }

            delegate: Rectangle
            {
                id: rowItem
                objectName: "recordingRow"
                required property real eventId
                required property int index
                property real commandId: eventId
                property var row: { recordingsRoot.revision; return recordingsRoot.rowsById[eventId] || recordingsRoot.emptyRow }
                property bool selected: recordingsRoot.selectedIds.indexOf(eventId) >= 0
                readonly property bool editTarget: recordingsRoot.editingId === commandId
                function focusNextRow()
                {
                    if (index + 1 >= commandView.count)
                    {
                        filterField.forceActiveFocus()
                        return
                    }
                    commandView.currentIndex = index + 1
                    commandView.positionViewAtIndex(index + 1, ListView.Contain)
                    Qt.callLater(function() {
                        if (commandView.currentItem)
                            commandView.currentItem.forceActiveFocus()
                    })
                }
                activeFocusOnTab: true
                border.width: activeFocus ? 2 : 0
                border.color: "#f1c40f"
                KeyNavigation.tab: timeFocus
                KeyNavigation.backtab: filterField
                Keys.onSpacePressed: (event) => {
                    recordingsRoot.clickRow(commandId, event.modifiers)
                    forceActiveFocus()
                }
                onActiveFocusChanged: if (activeFocus)
                    commandView.positionViewAtIndex(index, ListView.Contain)

                width: commandView.width
                height: UISettings.listItemHeight * 1.4
                color: selected ? UISettings.highlight : (index % 2 === 0 ? UISettings.bgLight : UISettings.bgMedium)

                // String() keeps a 32 bit id exact
                Accessible.role: Accessible.ListItem
                Accessible.id: "recordingRow-" + String(commandId)
                Accessible.name: [ qsTr("Event %1").arg(String(commandId)), TimeUtils.msToString(row.time),
                                   row.control, row.detail, row.actionLabel, row.valueText ]
                                 .filter(function(part) { return part !== undefined && String(part) !== "" }).join(", ")
                Accessible.description: editTarget ? qsTr("Being edited") : ""
                Accessible.selectable: true
                Accessible.selected: selected
                Accessible.onPressAction: recordingsRoot.clickRow(rowItem.commandId, Qt.NoModifier)

                // the one row a field edit changes
                Rectangle
                {
                    objectName: "editTargetMark"
                    visible: rowItem.editTarget
                    width: 4
                    height: parent.height
                    color: "#e67e22"
                    z: 1
                }

                MouseArea
                {
                    anchors.fill: parent
                    onClicked: (mouse) => recordingsRoot.clickRow(rowItem.commandId, mouse.modifiers)
                    onDoubleClicked: (mouse) =>
                        recordingsRoot.beginEdit(rowItem.row, recordingsRoot.fieldAt(mouse.x, rowItem.row))
                }

                Row
                {
                    anchors.fill: parent
                    anchors.leftMargin: 4

                    Item
                    {
                        id: timeFocus
                        readonly property bool showF2Editable: true
                        activeFocusOnTab: true
                        KeyNavigation.tab: rowItem.row.valueField !== "" ? valueFocus : null
                        Keys.onTabPressed: {
                            if (rowItem.row.valueField !== "")
                                valueFocus.forceActiveFocus()
                            else
                                rowItem.focusNextRow()
                        }
                        Keys.onPressed: (event) => {
                            if (event.key === Qt.Key_F2) {
                                recordingsRoot.beginEdit(rowItem.row, "time")
                                event.accepted = true
                            }
                        }
                        width: recordingsRoot.timeWidth
                        height: parent.height
                        Rectangle {
                            anchors.fill: parent
                            color: "transparent"
                            border.color: "#f1c40f"
                            border.width: timeFocus.activeFocus ? 2 : 0
                        }

                        RobotoText
                        {
                            objectName: "timeCell"
                            Accessible.role: Accessible.StaticText
                            Accessible.name: label
                            anchors.fill: parent
                            visible: !timeEditor.active
                            label: TimeUtils.msToString(rowItem.row.time)
                            fontSize: UISettings.textSizeDefault * 0.8
                        }
                        Loader
                        {
                            id: timeEditor
                            anchors.fill: parent
                            active: recordingsRoot.editingId === rowItem.commandId && recordingsRoot.editingField === "time"
                            sourceComponent: cellEditor
                        }
                    }

                    Column
                    {
                        width: recordingsRoot.controlWidth
                        anchors.verticalCenter: parent.verticalCenter

                        RobotoText
                        {
                            objectName: "controlCell"
                            Accessible.role: Accessible.StaticText
                            Accessible.name: label
                            width: parent.width
                            height: rowItem.height * 0.55
                            label: rowItem.row.control
                            fontSize: UISettings.textSizeDefault * 0.8
                            fontBold: true
                        }
                        RobotoText
                        {
                            objectName: "controlDetail"
                            Accessible.role: Accessible.StaticText
                            Accessible.name: label
                            width: parent.width
                            height: rowItem.height * 0.4
                            label: rowItem.row.detail
                            fontSize: UISettings.textSizeDefault * 0.6
                        }
                    }

                    RobotoText
                    {
                        objectName: "actionCell"
                        Accessible.role: Accessible.StaticText
                        Accessible.name: label
                        width: recordingsRoot.actionWidth
                        height: parent.height
                        label: rowItem.row.actionLabel
                        fontSize: UISettings.textSizeDefault * 0.8
                    }

                    Item
                    {
                        id: valueFocus
                        readonly property bool showF2Editable: rowItem.row.valueField !== ""
                        activeFocusOnTab: rowItem.row.valueField !== ""
                        Keys.onTabPressed: rowItem.focusNextRow()
                        Keys.onPressed: (event) => {
                            if (event.key === Qt.Key_F2 && rowItem.row.valueField !== "") {
                                recordingsRoot.beginEdit(rowItem.row, rowItem.row.valueField)
                                event.accepted = true
                            }
                        }
                        width: rowItem.width - recordingsRoot.timeWidth - recordingsRoot.controlWidth
                               - recordingsRoot.actionWidth - 4
                        height: parent.height
                        Rectangle {
                            anchors.fill: parent
                            color: "transparent"
                            border.color: "#f1c40f"
                            border.width: valueFocus.activeFocus ? 2 : 0
                        }

                        RobotoText
                        {
                            objectName: "valueCell"
                            Accessible.role: Accessible.StaticText
                            Accessible.name: label
                            anchors.fill: parent
                            visible: !valueEditor.active
                            label: rowItem.row.valueText
                            fontSize: UISettings.textSizeDefault * 0.8
                        }
                        Loader
                        {
                            id: valueEditor
                            anchors.fill: parent
                            active: recordingsRoot.editingId === rowItem.commandId && recordingsRoot.editingField !== ""
                                    && recordingsRoot.editingField !== "time"
                            sourceComponent: cellEditor
                        }
                    }
                }
            }
        }

        RobotoText
        {
            visible: recordingsRoot.rows.length === 0
            Layout.fillWidth: true
            label: qsTr("Nothing recorded yet")
            fontSize: UISettings.textSizeDefault * 0.7
        }
    }

    Component
    {
        id: cellEditor

        CustomTextEdit
        {
            objectName: "cellEditor"
            text: recordingsRoot.draftText
            onTextChanged: recordingsRoot.draftText = text
            Component.onCompleted: selectAndFocus()
            onAccepted: recordingsRoot.commitEdit(text)
            Keys.onEscapePressed: recordingsRoot.cancelEdit()
        }
    }
}
