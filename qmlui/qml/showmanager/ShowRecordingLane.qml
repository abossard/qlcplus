import QtQuick
import QtQuick.Controls

import org.qlcplus.classes 1.0
import "TimeUtils.js" as TimeUtils
import "SliderGraph.js" as SliderGraph
import "."

FocusScope
{
    id: lane
    readonly property bool showKeyOwner: true
    readonly property bool recordingKeyDomain: true
    property int showKeyRevision: 0
    property bool restoringFocus: false
    required property var editor
    required property real showId
    property var groups: showCommandRecorder ? showCommandRecorder.groups : []
    property var groupIds: []
    property var groupsById: ({})
    // below a split timeline, choosing objects opens them in the table
    property bool inspectOnSelect: false
    // one row per recorded control below the lane's own row; a view only
    property bool expanded: false
    property real rowHeight: 0
    // the narrowest a recording object is drawn, whatever its time span
    readonly property real minItemWidth: 18
    // the free space objects keep above and below them in their row
    readonly property real itemInset: 3
    readonly property var rowLabels: {
        var labels = []
        if (expanded)
            groups.forEach(function(group) { labels[group.laneIndex] = group.laneLabel })
        return labels
    }
    readonly property int rowCount: rowLabels.length
    function rowTop(index) { return expanded ? (1 + index) * rowHeight : 0 }

    /** The lane rows holding the given samples ({id, ms}) in lane coordinates:
      * { y, height, firstRowY } with firstRowY the row of the earliest sample, or null */
    function selectionRowsRect(samples)
    {
        var laneOf = {}
        groups.forEach(function(group) {
            group.eventIds.forEach(function(id) { laneOf[id] = group.laneIndex || 0 })
        })
        var top = Infinity, bottom = -Infinity, first = null
        samples.forEach(function(sample) {
            if (laneOf[sample.id] === undefined)
                return
            var rowY = rowTop(laneOf[sample.id])
            top = Math.min(top, rowY)
            bottom = Math.max(bottom, rowY + rowHeight)
            if (first === null || sample.ms < first.ms)
                first = { ms: sample.ms, y: rowY }
        })
        return first === null ? null : { y: top, height: bottom - top, firstRowY: first.y }
    }
    signal reveal(var ids)

    /* The one recording pointer gesture: a body drag (0) or a stretch from the end (1)
     * or the start (2) of the exact selection (ShowRetimeKind). It holds the view from
     * press to release. The press freezes the selection in a recorder session; past
     * the drag distance previews come from it as {id: ms} without publishing; release
     * commits the pointer's own request once, or the recorder refuses it whole */
    property bool pointerHeld: false
    property int gestureKind: -1
    property var gestureIds: []
    property bool gestureStarted: false
    property bool gestureMoved: false
    property real gestureRequest: 0
    // the last valid preview, drawn while a refused request is held
    property var previewTimes: null

    // the selection's times, previewed while a gesture holds, and the rows that hold them
    readonly property var spanSamples: {
        var times = previewTimes
        return editor.selectedRows.map(function(row) {
            return { id: row.id, ms: times && times[row.id] !== undefined ? times[row.id] : row.time }
        })
    }
    readonly property var spanRange: {
        if (spanSamples.length === 0)
            return null
        var rows = selectionRowsRect(spanSamples)
        if (rows === null)
            return null
        var start = Infinity, end = -Infinity
        spanSamples.forEach(function(sample) {
            start = Math.min(start, sample.ms)
            end = Math.max(end, sample.ms)
        })
        return { start: start, end: end, y: rows.y, height: rows.height }
    }

    function pxToMs(dx)
    {
        return Math.round(showManager.timeBasedDivision
            ? dx * 1000 * showManager.timeScale / showManager.tickSize
            : TimeUtils.posToBeatMs(dx, showManager.tickSize, showManager.bpmNumber, showManager.beatsDivision))
    }

    // the press: what is selected now is what the gesture edits, whatever is recorded meanwhile
    function armGesture(kind)
    {
        endGesture()
        var ids = editor.selection().slice()
        if (ids.length === 0 || !editor.editable || !showCommandRecorder.beginEditSession(showId, ids))
            return
        gestureKind = kind
        gestureIds = ids
        gestureStarted = true
    }

    // dx: the pointer's distance from its press, in lane px
    function dragGesture(dx)
    {
        if (!gestureStarted || (!gestureMoved && Math.abs(dx) < Qt.styleHints.startDragDistance))
            return
        gestureMoved = true
        gestureRequest = pxToMs(dx)
        var preview = showCommandRecorder.previewRetime(gestureKind, gestureRequest)
        if (preview.times !== undefined)
            previewTimes = preview.times
    }

    function endGesture()
    {
        gestureKind = -1
        gestureIds = []
        gestureStarted = false
        gestureMoved = false
        gestureRequest = 0
        previewTimes = null
    }

    function releaseGesture()
    {
        var started = gestureStarted, kind = gestureKind, request = gestureMoved ? gestureRequest : 0
        endGesture()
        if (!started)
            return
        if (request === 0)
            showCommandRecorder.cancelEditSession()
        else
            showCommandRecorder.commitRetime(kind, request)
    }

    // reason: why the gesture ended, shown by the recorder; none for Escape or a lost grab
    function cancelGesture(reason)
    {
        var started = gestureStarted
        endGesture()
        if (started)
            showCommandRecorder.cancelEditSession(reason || "")
    }

    // the topmost recording object at a lane point, or null
    function objectAt(x, y)
    {
        var found = null
        for (var i = 0; i < objects.count; i++)
        {
            var item = objects.itemAt(i)
            if (item && x >= item.x && x < item.x + item.width && y >= item.y && y < item.y + item.height
                    && (found === null || item.z >= found.z))
                found = item
        }
        return found
    }

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

    // the objects as the one Repeater holds them: global group order
    function orderedObjects()
    {
        var items = []
        for (var i = 0; i < objects.count; i++)
            if (objects.itemAt(i))
                items.push(objects.itemAt(i))
        return items
    }

    function showFocusedItem(item)
    {
        // a pointer press owns its item's motion: revealing it would scroll the
        // view under the pointer and feed that scroll into the drag distance
        if (lane.pointerHeld || item.pointerHolds())
            return
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
    height: (1 + rowCount) * rowHeight
    Accessible.role: Accessible.Grouping
    Accessible.id: "recordingLane-" + String(showId)
    Accessible.name: qsTr("Recordings")

    Keys.onEscapePressed: (event) =>
    {
        if (gestureKind < 0)
        {
            event.accepted = false
            return
        }
        cancelGesture()
    }

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
        function onSelectedIdsChanged()
        {
            lane.showKeyRevision++
            if (!lane.gestureStarted)
                return
            // choosing other events ends the gesture; events deleted meanwhile leave the
            // selection without a choice, and release reports them through the recorder
            var rows = lane.editor.rowsById, selected = lane.editor.selectedIds
            var present = lane.gestureIds.every(function(id) { return rows[id] !== undefined })
            var same = selected.length === lane.gestureIds.length
                && selected.every(function(id) { return lane.gestureIds.indexOf(id) >= 0 })
            if (present && !same)
                lane.cancelGesture(qsTr("The selection changed, so the drag ended without a change"))
        }
    }
    Connections
    {
        target: showCommandRecorder
        // the recorder ended the frozen edit (another edit, a Show change, a new
        // workspace): the preview goes, the reason is the recorder's
        function onEditSessionChanged()
        {
            if (lane.gestureStarted && !showCommandRecorder.editSessionActive)
                lane.endGesture()
        }
    }
    onVisibleChanged: if (!visible) cancelGesture()

    // the part of the lane slider drawings cover: the visible timeline plus a margin
    // on each side, moved in margin steps so a scroll repaints once per margin
    property real drawMarginRatio: 0.25
    readonly property Item horizontalView: parent && parent.parent && parent.parent.contentX !== undefined
                                           ? parent.parent : null
    readonly property Item verticalView: {
        for (var ancestor = parent; ancestor; ancestor = ancestor.parent)
            if (ancestor.totalTracksHeight !== undefined)
                return ancestor
        return null
    }
    readonly property real drawMargin: horizontalView ? Math.max(32, horizontalView.width * drawMarginRatio) : 0
    readonly property real drawLeft: horizontalView
        ? Math.round(horizontalView.contentX / drawMargin) * drawMargin - drawMargin : -Infinity
    readonly property real drawRight: horizontalView ? drawLeft + horizontalView.width + 2 * drawMargin : Infinity
    readonly property real drawTop: verticalView
        ? verticalView.contentY - y - (horizontalView ? horizontalView.y : 0) - rowHeight : -Infinity
    readonly property real drawBottom: verticalView ? drawTop + verticalView.height + 2 * rowHeight : Infinity

    function timeX(ms)
    {
        return projection(0)(ms)
    }

    /** A time to x function over the ruler as it is now, x counted from origin */
    function projection(origin)
    {
        var timeBased = showManager.timeBasedDivision, scale = showManager.timeScale
        var tick = showManager.tickSize, bpm = showManager.bpmNumber, beats = showManager.beatsDivision
        return function(ms) {
            return (timeBased ? TimeUtils.timeToSize(ms, scale, tick)
                              : TimeUtils.timeToBeatPosition(ms, tick, bpm, beats)) - origin
        }
    }

    Repeater
    {
        model: lane.rowCount
        delegate: Rectangle
        {
            required property int index
            y: lane.rowTop(index)
            width: lane.width
            height: 1
            color: UISettings.bgLight
        }
    }

    Repeater
    {
        id: objects
        model: lane.groupIds
        delegate: Rectangle
        {
            id: recording
            readonly property bool showEnterAction: true
            required property var modelData
            required property int index
            readonly property var group: lane.groupsById[modelData] || ({eventIds: [], startTime: 0, endTime: 0,
                id: modelData, control: "", slider: false, valueText: "", actionLabel: "", detail: "", laneIndex: 0})
            readonly property var members: group.eventIds
            readonly property int selectedCount: members.filter(function(id) {
                return lane.editor.selectedIds.indexOf(id) >= 0
            }).length
            readonly property bool selected: selectedCount === members.length
            readonly property string summary: group.control + ", "
                + (group.slider ? qsTr("%1 samples").arg(group.sampleCount) + ", "
                                     + group.valueText + " → " + group.endValueText
                                   : group.actionLabel + " " + group.valueText)
            // the members' times, previewed while the lane's gesture holds some of them
            readonly property var shownSpan: {
                var times = lane.previewTimes
                if (!times || selectedCount === 0)
                    return { start: group.startTime, end: group.endTime }
                var rows = lane.editor.rowsById, start = Infinity, end = -Infinity
                members.forEach(function(id) {
                    var ms = times[id] !== undefined ? times[id] : rows[id] !== undefined ? rows[id].time : group.startTime
                    start = Math.min(start, ms)
                    end = Math.max(end, ms)
                })
                return { start: start, end: end }
            }
            // held from the press handler through the release or cancel handler: pressed
            // reads true live as the press is handled, the binding keeps it true until
            // pressedChanged, which follows released and canceled
            readonly property bool pointerPressed: pointer.pressed
            function pointerHolds() { return pointer.pressed || pointerPressed }

            objectName: "recordingItem"
            x: lane.timeX(shownSpan.start)
            y: lane.rowTop(group.laneIndex) + lane.itemInset
            width: Math.max(lane.minItemWidth, lane.timeX(shownSpan.end) - lane.timeX(shownSpan.start))
            height: lane.rowHeight - 2 * lane.itemInset
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
            onYChanged: if (activeFocus) lane.showFocusedItem(recording)

            Accessible.role: Accessible.ListItem
            Accessible.id: "recordingItem-" + String(lane.showId) + "-" + String(group.id)
            Accessible.name: summary + ", " + TimeUtils.msToString(group.startTime)
                + " – " + TimeUtils.msToString(group.endTime)
            Accessible.description: qsTr("%1 of %2 samples selected. Open to edit individual samples.")
                .arg(selectedCount).arg(members.length) + (simplifiedNote ? " " + simplifiedNote : "")
            Accessible.selectable: true
            Accessible.selected: selected
            Accessible.onPressAction: choose(Qt.NoModifier)

            function choose(modifiers)
            {
                lane.editor.selectMembers(members, modifiers)
                if (lane.inspectOnSelect && lane.editor.selectedIds.length > 0)
                    lane.editor.inspect(lane.editor.selection())
                forceActiveFocus()
            }
            function open()
            {
                // a selection reaching into this object opens as a whole passage
                lane.reveal(selectedCount > 0 ? lane.editor.selection() : members)
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
            // the recorded positions, drawn only while the object meets the drawing window
            readonly property real baseX: lane.timeX(group.startTime)
            readonly property bool drawn: group.slider && baseX + width > lane.drawLeft && baseX < lane.drawRight
                                          && y + height > lane.drawTop && y < lane.drawBottom
            readonly property real plotTop: summaryText.y + summaryText.font.pixelSize * 1.3 + 3
            readonly property real plotHeight: Math.max(0, height - 4 - plotTop)
            readonly property var graphGeometry: graph.item ? graph.item.geometry : null
            readonly property string simplifiedNote: graphGeometry && graphGeometry.simplified
                ? qsTr("Display simplified: where samples share a pixel column only their range is drawn, not their count or order.")
                : ""
            readonly property string graphReadout: {
                if (!group.slider || !pointer.containsMouse || !graph.item)
                    return ""
                var range = graph.item.range
                var nearest = SliderGraph.nearestSample(graph.item.samples,
                                                        group.startTime + pointer.mouseX * 1000 / lane.timeX(1000))
                if (!range || !nearest)
                    return ""
                var percent = function(position) { return Math.round(position * 1000) / 10 + "%" }
                return qsTr("Recorded position %1 – %2").arg(percent(range.min)).arg(percent(range.max)) + "\n"
                    + qsTr("Nearest sample %1: %2").arg(TimeUtils.msToString(nearest.time)).arg(percent(nearest.position))
                    + (simplifiedNote ? "\n" + simplifiedNote : "")
            }
            Loader
            {
                id: graph
                active: recording.drawn
                x: Math.max(0, lane.drawLeft - recording.baseX)
                width: Math.max(0, Math.min(recording.width, lane.drawRight - recording.baseX) - x)
                height: recording.height
                sourceComponent: Canvas
                {
                    objectName: "sliderGraph"
                    enabled: false
                    Accessible.ignored: true
                    antialiasing: false
                    // read again when the groups are, once per commandsChanged
                    readonly property var samples: showCommandRecorder.sliderSamples(lane.showId, recording.members)
                    readonly property var range: SliderGraph.sampleRange(samples)
                    property int fetchCount: 0
                    property int paintCount: 0
                    // missing, unbound or disabled controls: dimmed, still readable on the object
                    readonly property color plotColor: group.controlReady ? "#8fe3ff" : "#c8c8c8"
                    readonly property color selectedColor: "#f1c40f"
                    // what the drawing depends on; any change repaints once, in the next frame
                    readonly property var inputs: [samples, lane.editor.selectedIds, graph.x, width,
                        recording.plotTop, recording.plotHeight, lane.timeX(1000), plotColor]
                    // the step geometry of the last paint, for the object's description
                    property var geometry: null
                    onSamplesChanged: fetchCount++
                    onInputsChanged: requestPaint()
                    onPaint:
                    {
                        paintCount++
                        var selected = {}
                        lane.editor.selectedIds.forEach(function(id) { selected[id] = true })
                        geometry = SliderGraph.stepGeometry(samples, { left: 0, right: width },
                            lane.projection(recording.baseX + graph.x),
                            { top: recording.plotTop, height: recording.plotHeight }, selected)
                        var ctx = getContext("2d")
                        ctx.reset()
                        ctx.lineWidth = 2
                        var holds = geometry.holds, verticals = geometry.verticals, marks = geometry.marks
                        var spans = geometry.selectedSpans, k
                        for (var pass = 0; pass < 2; pass++)
                        {
                            ctx.strokeStyle = pass === 1 ? selectedColor : plotColor
                            ctx.fillStyle = ctx.strokeStyle
                            ctx.beginPath()
                            for (k = 0; k < holds.length; k += 4)
                                if (holds[k + 3] === pass)
                                {
                                    ctx.moveTo(holds[k], holds[k + 2])
                                    ctx.lineTo(holds[k + 1], holds[k + 2])
                                }
                            for (k = 0; k < verticals.length; k += 4)
                                if (verticals[k + 3] === pass)
                                {
                                    ctx.moveTo(verticals[k], verticals[k + 1])
                                    ctx.lineTo(verticals[k], verticals[k + 2])
                                }
                            ctx.stroke()
                            for (k = 0; k < marks.length; k += 3)
                                if (marks[k + 2] === pass)
                                    ctx.fillRect(marks[k] - 2.5, marks[k + 1] - 2.5, 5, 5)
                        }
                        for (k = 0; k < spans.length; k += 3)
                            ctx.fillRect(spans[k] - 1.5, spans[k + 1] - 1.5, 3, spans[k + 2] - spans[k + 1] + 3)
                    }
                }
            }

            Text
            {
                id: summaryText
                anchors.fill: parent
                anchors.margins: 5
                // a slider's summary sits above its drawing
                anchors.bottomMargin: group.slider ? parent.height - 5 - font.pixelSize * 1.3 : 5
                text: recording.summary
                color: UISettings.fgMain
                elide: Text.ElideRight
                verticalAlignment: group.slider ? Text.AlignTop : Text.AlignVCenter
                font.pixelSize: UISettings.textSizeDefault * 0.8
            }
            ToolTip.visible: pointer.containsMouse && !pointer.pressed && !lane.pointerHeld
            ToolTip.text: Accessible.name + "\n" + group.detail + (graphReadout ? "\n" + graphReadout : "")

            // hover and Shift/Ctrl/Meta presses; a plain press belongs to the lane's gesture surface
            MouseArea
            {
                id: pointer
                anchors.fill: parent
                hoverEnabled: true
                onPressed: (mouse) => recording.choose(mouse.modifiers)
                onDoubleClicked: recording.open()
            }
        }
    }

    /* One pointer gesture of the lane: kind 0 drags the selection's body, 2 and 1
     * stretch it from its start or end (ShowRetimeKind). A press focuses the object
     * under it, choosing it first when the body is pressed on an unselected one; a
     * click without movement chooses that object, a double click opens it. The areas
     * live outside the objects, so regrouping that replaces them leaves a held drag */
    component GestureArea: MouseArea
    {
        property int kind: 0
        property Item pressedItem: null
        property real pressX: 0
        property bool moved: false
        preventStealing: true
        Accessible.ignored: true

        onPressed: (mouse) =>
        {
            var at = mapToItem(lane, mouse.x, mouse.y)
            var item = lane.objectAt(at.x, at.y)
            var modifiers = mouse.modifiers & (Qt.ShiftModifier | Qt.ControlModifier | Qt.MetaModifier)
            // Shift, Ctrl and Meta presses choose on the object itself; a grip leaves an
            // unselected object under it to the body, which chooses and drags that object
            if (kind === 0 ? (item === null || modifiers)
                           : (item !== null && (modifiers || item.selectedCount === 0)))
            {
                mouse.accepted = false
                return
            }
            // the press point on screen, taken before choosing or focusing can move
            // anything, anchors the drag in lane coordinates afterwards
            var scene = mapToItem(null, mouse.x, mouse.y)
            lane.pointerHeld = true
            if (kind === 0 && item.selectedCount === 0)
                item.choose(Qt.NoModifier)
            else if (item !== null && item.selectedCount > 0)
                item.forceActiveFocus()
            else
                lane.focusSelection()
            pressedItem = item
            pressX = lane.mapFromItem(null, scene.x, scene.y).x
            moved = false
            lane.armGesture(kind)
        }
        onPositionChanged: (mouse) =>
        {
            var dx = mapToItem(lane, mouse.x, mouse.y).x - pressX
            if (Math.abs(dx) >= Qt.styleHints.startDragDistance)
                moved = true
            lane.dragGesture(dx)
        }
        onReleased:
        {
            lane.releaseGesture()
            lane.pointerHeld = false
        }
        onCanceled:
        {
            lane.cancelGesture()
            lane.pointerHeld = false
        }
        onClicked: if (!moved && pressedItem) pressedItem.choose(Qt.NoModifier)
        onDoubleClicked: if (pressedItem) pressedItem.open()
    }

    GestureArea
    {
        objectName: "recordingGesture"
        anchors.fill: parent
        z: 5
    }

    // the selected span over its rows: the stretch handles at its edges, for a positive span
    Item
    {
        id: span
        objectName: "recordingSpan"
        readonly property var range: lane.spanRange
        visible: range !== null
        x: range ? lane.timeX(range.start) : 0
        y: range ? range.y : 0
        width: range ? lane.timeX(range.end) - lane.timeX(range.start) : 0
        height: range ? range.height : 0
        z: 6

        Rectangle
        {
            anchors.fill: parent
            color: "transparent"
            border.color: "#f1c40f"
            border.width: 1
            opacity: 0.6
            visible: span.width > 0
        }

        Repeater
        {
            model: [ { kind: 2, name: "recordingSpanStart" }, { kind: 1, name: "recordingSpanEnd" } ]
            delegate: GestureArea
            {
                required property var modelData
                kind: modelData.kind
                objectName: modelData.name
                visible: span.width > 0 && lane.editor.editable
                // a grip of fixed size on its selected endpoint, in the band at the
                // bottom of the span: objects may be drawn over either endpoint (from
                // their time to the right, at least minItemWidth wide, or a partly selected
                // group around it), so the grips keep off their middle height instead and
                // reach over the span's lower edge into the free inset, where no object is.
                // Centred on the endpoint, or, on a span too short for both, each on its
                // own side of the middle, still over its endpoint
                x: kind === 2 ? Math.min(-width / 2, span.width / 2 - width)
                              : Math.max(span.width - width / 2, span.width / 2)
                y: span.height - height + lane.itemInset
                width: 12
                height: Math.min(10, span.height / 3)
                cursorShape: Qt.SizeHorCursor

                Rectangle
                {
                    anchors.centerIn: parent
                    width: Math.min(4, parent.width)
                    height: parent.height
                    radius: 2
                    color: "#f1c40f"
                }
            }
        }
    }
}
