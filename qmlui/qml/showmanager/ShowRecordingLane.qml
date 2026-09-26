import QtQuick
import QtQuick.Controls

import org.qlcplus.classes 1.0
import "TimeUtils.js" as TimeUtils
import "."

FocusScope
{
    id: lane
    readonly property bool showKeyOwner: true
    property int showKeyRevision: 0
    property bool restoringFocus: false
    required property var editor
    required property real showId
    property var groups: showCommandRecorder ? showCommandRecorder.groups : []
    property var groupIds: []
    property var groupsById: ({})
    property int revision: 0
    signal reveal(var ids)

    function focusSelection()
    {
        for (var i = 0; i < objects.count; i++)
        {
            var item = objects.itemAt(i)
            if (item && item.selectedCount > 0)
            {
                item.forceActiveFocus()
                return
            }

        }
    }

    function showFocusedItem(item)
    {
        var horizontal = lane.parent ? lane.parent.parent : null
        if (horizontal && horizontal.contentX !== undefined)
        {
            if (item.x < horizontal.contentX)
                horizontal.contentX = item.x
            else if (item.x + Math.min(item.width, horizontal.width) > horizontal.contentX + horizontal.width)
                horizontal.contentX = Math.max(0, item.x + Math.min(item.width, horizontal.width) - horizontal.width)
        }
        for (var ancestor = lane.parent; ancestor; ancestor = ancestor.parent)
            if (ancestor.totalTracksHeight !== undefined)
            {
                var top = item.mapToItem(ancestor.contentItem, 0, 0).y
                if (top < ancestor.contentY)
                    ancestor.contentY = top
                else if (top + item.height > ancestor.contentY + ancestor.height)
                    ancestor.contentY = top + item.height - ancestor.height
                break
            }
    }

    objectName: "recordingLane"
    Accessible.role: Accessible.Grouping
    Accessible.id: "recordingLane-" + String(showId)
    Accessible.name: qsTr("Recordings")

    onGroupsChanged:
    {
        var restoreFocus = activeFocus
        restoringFocus = restoreFocus
        var byId = {}
        groups.forEach(function(group) { byId[group.id] = group })
        groupsById = byId
        var ids = groups.map(function(group) { return group.id })
        if (ids.length !== groupIds.length || ids.some(function(id, i) { return id !== groupIds[i] }))
            groupIds = ids
        if (restoreFocus)
            Qt.callLater(function() {
                for (var i = 0; i < objects.count; i++)
                {
                    var item = objects.itemAt(i)
                    if (item && item.selectedCount > 0)
                    {
                        item.forceActiveFocus()
                        break
                    }
                }
                restoringFocus = false
            })
    }
    Connections
    {
        target: lane.editor
        function onSelectedIdsChanged() { lane.showKeyRevision++ }
    }
    Connections
    {
        target: showCommandRecorder
        function onCommandsChanged() { lane.revision++ }
        function onEditGateChanged() { lane.revision++ }
    }
    onShowIdChanged: revision++
    onVisibleChanged: revision++
    Connections
    {
        target: showManager
        function onReadOnlyChanged() { lane.revision++ }
        function onTimeScaleChanged() { lane.revision++ }
        function onTimeDivisionChanged() { lane.revision++ }
        function onBpmNumberChanged() { lane.revision++ }
        function onBeatsDivisionChanged() { lane.revision++ }
    }

    function timeX(ms)
    {
        return showManager.timeBasedDivision
            ? TimeUtils.timeToSize(ms, showManager.timeScale, showManager.tickSize)
            : TimeUtils.timeToBeatPosition(ms, showManager.tickSize, showManager.bpmNumber, showManager.beatsDivision)
    }

    Repeater
    {
        id: objects
        model: lane.groupIds
        delegate: Rectangle
        {
            id: recording
            required property var modelData
            required property int index
            readonly property var group: lane.groupsById[modelData] || ({eventIds: [], startTime: 0, endTime: 0,
                id: modelData, control: "", slider: false, valueText: "", actionLabel: "", detail: ""})
            readonly property var members: group.eventIds
            readonly property int selectedCount: members.filter(function(id) {
                return lane.editor.selectedIds.indexOf(id) >= 0
            }).length
            readonly property bool selected: selectedCount === members.length
            readonly property string summary: group.control + ", "
                + (group.slider ? qsTr("%1 samples").arg(group.sampleCount) + ", "
                                     + group.valueText + " → " + group.endValueText
                                   : group.actionLabel + " " + group.valueText)
            property real previewDelta: 0

            objectName: "recordingItem"
            x: lane.timeX(group.startTime) + previewDelta
            y: 3
            width: Math.max(18, lane.timeX(group.endTime) - lane.timeX(group.startTime))
            height: lane.height - 6
            color: selected ? UISettings.highlight : UISettings.bgLight
            border.color: selectedCount > 0 || activeFocus ? "#f1c40f" : UISettings.bgMedium
            border.width: selectedCount > 0 || activeFocus ? 2 : 1
            radius: 3
            z: activeFocus || selectedCount > 0 ? 2 : 1
            activeFocusOnTab: true
            onActiveFocusChanged: if (activeFocus) {
                if (!lane.restoringFocus)
                    lane.showKeyRevision++
                lane.showFocusedItem(recording)
            }
            onXChanged: if (activeFocus) lane.showFocusedItem(recording)

            Accessible.role: Accessible.ListItem
            Accessible.id: "recordingItem-" + String(lane.showId) + "-" + String(group.id)
            Accessible.name: summary + ", " + TimeUtils.msToString(group.startTime)
                + " – " + TimeUtils.msToString(group.endTime)
            Accessible.description: qsTr("%1 of %2 samples selected. Open to edit individual samples.")
                .arg(selectedCount).arg(members.length)
            Accessible.selectable: true
            Accessible.selected: selected
            Accessible.onPressAction: choose(Qt.NoModifier)

            function choose(modifiers)
            {
                lane.editor.selectMembers(members, modifiers)
                forceActiveFocus()
            }
            function open()
            {
                lane.reveal(selectedCount > 0 ? members.filter(function(id) {
                    return lane.editor.selectedIds.indexOf(id) >= 0
                }) : members)
            }

            Keys.onPressed: (event) =>
            {
                if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter)
                    open()
                else if (event.key === Qt.Key_Space)
                    choose(event.modifiers)
                else if (event.key === Qt.Key_Left)
                    lane.editor.moveSelected(-1, event.modifiers & Qt.AltModifier)
                else if (event.key === Qt.Key_Right)
                    lane.editor.moveSelected(1, event.modifiers & Qt.AltModifier)
                else if (event.key === Qt.Key_Backspace || event.key === Qt.Key_Delete)
                    lane.editor.deleteSelected()
                else
                    return
                event.accepted = true
            }

            Shortcut
            {
                sequence: "Delete"
                enabled: recording.activeFocus && lane.visible
                onActivated: lane.editor.deleteSelected()
            }

            Rectangle
            {
                x: 0
                width: 2
                height: parent.height
                color: "#f1c40f"
            }
            Text
            {
                anchors.fill: parent
                anchors.margins: 5
                text: recording.summary
                color: UISettings.fgMain
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
                font.pixelSize: UISettings.textSizeDefault * 0.8
            }
            ToolTip.visible: pointer.containsMouse && !pointer.pressed
            ToolTip.text: Accessible.name + "\n" + group.detail

            MouseArea
            {
                id: pointer
                anchors.fill: parent
                hoverEnabled: true
                preventStealing: true
                property real pressX: 0
                property real capturedShow: 0
                property var capturedIds: []
                property int capturedRevision: -1
                property bool moved: false

                onPressed: (mouse) =>
                {
                    if (recording.selectedCount === 0 || (mouse.modifiers & (Qt.ShiftModifier | Qt.ControlModifier | Qt.MetaModifier)))
                        recording.choose(mouse.modifiers)
                    else
                        recording.forceActiveFocus()
                    pressX = mapToItem(lane, mouse.x, mouse.y).x
                    capturedShow = lane.showId
                    capturedIds = lane.editor.selection().slice()
                    capturedRevision = lane.revision
                    moved = false
                }
                onPositionChanged: (mouse) =>
                {
                    if (!pressed || !lane.editor.editable || capturedRevision !== lane.revision)
                        return
                    var delta = mapToItem(lane, mouse.x, mouse.y).x - pressX
                    if (Math.abs(delta) >= Qt.styleHints.startDragDistance)
                        moved = true
                    if (moved)
                        recording.previewDelta = delta
                }
                onReleased:
                {
                    var dx = recording.previewDelta
                    recording.previewDelta = 0
                    if (!moved || capturedRevision !== lane.revision || !lane.editor.editable
                            || capturedShow !== lane.showId)
                        return
                    var selection = lane.editor.selection()
                    if (selection.length !== capturedIds.length
                            || selection.some(function(id, i) { return id !== capturedIds[i] }))
                        return
                    var delta = showManager.timeBasedDivision
                        ? dx * 1000 * showManager.timeScale / showManager.tickSize
                        : TimeUtils.posToBeatMs(dx, showManager.tickSize, showManager.bpmNumber, showManager.beatsDivision)
                    var ms = Math.round(delta)
                    if (ms !== 0)
                        showCommandRecorder.moveCommands(capturedShow, capturedIds, ms < 0 ? -1 : 1, Math.abs(ms))
                }
                onCanceled: recording.previewDelta = 0
                onClicked: (mouse) =>
                {
                    if (!moved && capturedRevision === lane.revision
                            && !(mouse.modifiers & (Qt.ShiftModifier | Qt.ControlModifier | Qt.MetaModifier)))
                        recording.choose(mouse.modifiers)
                }
                onDoubleClicked: recording.open()
                Connections
                {
                    target: lane
                    function onRevisionChanged() { recording.previewDelta = 0 }
                }
            }
        }
    }
}
