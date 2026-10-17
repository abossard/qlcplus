import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import QtQml.Models
import org.qlcplus.classes 1.0

import "."

FocusScope {
    id: outline
    objectName: "vcOutline"
    implicitWidth: UISettings.sidePanelWidth
    property var outlineModel: virtualConsole.outlineModel
    property var expandedKeys: []
    property var knownRoots: []
    property string focusedKey: ""
    property string selectionAnchor: ""
    property bool dragging: false
    property bool dragCanceled: false
    property string dropHint: ""
    readonly property bool diagnosticsEligible: visible && width > 0 && height > 0 &&
                                                (qlcplus.accessMask & App.AC_VCEditing)
    readonly property var scopeSummary: outlineModel ? outlineModel.assignmentSummary : ({})
    readonly property int assignmentVisibleRows: assignmentRowsRepeater ? assignmentRowsRepeater.count : 0
    Accessible.role: Accessible.Tree
    Accessible.name: qsTr("Console outline")

    function toggleExpanded(key) {
        focusedKey = key;
        forceActiveFocus();
        var keys = expandedKeys.slice();
        var index = keys.indexOf(key);
        if (index >= 0)
            keys.splice(index, 1);
        else
            keys.push(key);
        expandedKeys = keys;
    }

    function selectKey(key, modifiers) {
        focusedKey = key;
        if (outlineModel)
            outlineModel.inspectionScopeKey = key;
        if (!virtualConsole.editMode)
            return;
        var row = outlineModel.rowData(key);
        if (row.rowKind !== 1)
            return;
        var multi = (modifiers & (Qt.ControlModifier | Qt.MetaModifier)) !== 0;
        var ids = multi ? virtualConsole.selectedIDs.slice() : [];
        if ((modifiers & Qt.ShiftModifier) && selectionAnchor.length) {
            var keys = outlineModel.visibleKeys(expandedKeys);
            var first = keys.indexOf(selectionAnchor);
            var last = keys.indexOf(key);
            if (first >= 0 && last >= 0) {
                for (var i = Math.min(first, last); i <= Math.max(first, last); ++i) {
                    var item = outlineModel.rowData(keys[i]);
                    if (item.rowKind === 1 && ids.indexOf(item.widgetID) < 0)
                        ids.push(item.widgetID);
                }
            } else
                ids.push(row.widgetID);
        } else {
            var selected = ids.indexOf(row.widgetID);
            if (multi && selected >= 0)
                ids.splice(selected, 1);
            else
                ids.push(row.widgetID);
            selectionAnchor = key;
        }
        virtualConsole.selectedIDs = ids;
    }

    function reportResult(result) {
        if (!result.accepted) {
            outlineError.message = result.error;
            outlineError.open();
        }
    }

    function expandNewRoots() {
        if (!outlineModel)
            return;
        var keys = outlineModel.visibleKeys([]);
        var expanded = expandedKeys.slice();
        var known = knownRoots.slice();
        for (var i = 0; i < keys.length; ++i) {
            if (outlineModel.rowData(keys[i]).rowKind === 0 && known.indexOf(keys[i]) < 0) {
                known.push(keys[i]);
                expanded.push(keys[i]);
            }
        }
        knownRoots = known;
        expandedKeys = expanded;
    }

    function reveal(key) {
        var row = outlineModel.rowData(key);
        reportResult(row.rowKind === 2 ? virtualConsole.revealFramePage(row.parentID, row.subpageIndex) : virtualConsole.revealWidget(row.widgetID));
    }

    function ensureExpandedToKey(key) {
        if (!outlineModel || !key || key.length === 0)
            return;
        var row = outlineModel.rowData(key);
        var expanded = expandedKeys.slice();
        while (row.parentKey && row.parentKey.length > 0) {
            if (expanded.indexOf(row.parentKey) < 0)
                expanded.push(row.parentKey);
            row = outlineModel.rowData(row.parentKey);
        }
        expandedKeys = expanded;
    }

    function showAssignmentWidget(row) {
        if (!row || !row.widgetKey || row.widgetKey.length === 0)
            return;
        if (search.text.length > 0)
            search.clear();
        ensureExpandedToKey(row.widgetKey);
        reveal(row.widgetKey);
    }

    function compactMapString(mapValue) {
        if (!mapValue)
            return "-";
        var keys = Object.keys(mapValue);
        if (keys.length === 0)
            return "-";
        keys.sort();
        var pairs = [];
        for (var i = 0; i < keys.length; ++i)
            pairs.push(keys[i] + "=" + mapValue[keys[i]]);
        return pairs.join(", ");
    }

    Keys.onPressed: event => {
        if (!outlineModel)
            return;
        var keys = outlineModel.visibleKeys(expandedKeys);
        var index = keys.indexOf(focusedKey);
        if (event.key === Qt.Key_Up || event.key === Qt.Key_Down) {
            var next = index < 0 ? 0 : Math.max(0, Math.min(keys.length - 1, index + (event.key === Qt.Key_Up ? -1 : 1)));
            if (keys.length) {
                focusedKey = keys[next];
                if (event.modifiers & Qt.ShiftModifier)
                    selectKey(focusedKey, event.modifiers);
            }
            event.accepted = true;
        } else if (event.key === Qt.Key_Left || event.key === Qt.Key_Right) {
            var row = outlineModel.rowData(focusedKey);
            var expanded = expandedKeys.indexOf(focusedKey) >= 0;
            if (event.key === Qt.Key_Right && row.isContainer && !expanded)
                toggleExpanded(focusedKey);
            else if (event.key === Qt.Key_Left && expanded)
                toggleExpanded(focusedKey);
            else if (event.key === Qt.Key_Left && row.parentKey)
                focusedKey = row.parentKey;
            else if (index >= 0 && index + 1 < keys.length)
                focusedKey = keys[index + 1];
            event.accepted = true;
        } else if (event.key === Qt.Key_Space) {
            selectKey(focusedKey, Qt.ControlModifier);
            event.accepted = true;
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            if (focusedKey.length)
                reveal(focusedKey);
            event.accepted = true;
        } else if (event.key === Qt.Key_Escape) {
            dragging = false;
            dragCanceled = true;
            dropHint = "";
            event.accepted = true;
        } else if ((event.modifiers & (Qt.ControlModifier | Qt.MetaModifier)) && event.key === Qt.Key_A) {
            if (virtualConsole.editMode)
                virtualConsole.selectedIDs = outlineModel.visibleWidgetIDs(expandedKeys);
            event.accepted = true;
        } else if (event.key === Qt.Key_Delete || event.key === Qt.Key_Backspace) {
            if (virtualConsole.editMode)
                reportResult(virtualConsole.deleteAuthoringWidgets(virtualConsole.selectedWidgetIDs()));
            event.accepted = true;
        } else if ((event.modifiers & (Qt.ControlModifier | Qt.MetaModifier)) && (event.key === Qt.Key_C || event.key === Qt.Key_V || event.key === Qt.Key_D)) {
            event.accepted = true;
        }
    }

    Connections {
        target: outlineModel
        function onTreeChanged() {
            outline.expandNewRoots();
            if (outline.focusedKey.length && outlineModel.visibleKeys(outline.expandedKeys).indexOf(outline.focusedKey) < 0)
                outline.focusedKey = "";
        }
    }

    Connections {
        target: virtualConsole
        function onEditModeChanged() {
            if (!virtualConsole.editMode) {
                outline.dragging = false;
                outline.dragCanceled = true;
                outline.dropHint = "";
            }
        }
    }

    onDiagnosticsEligibleChanged: {
        if (outlineModel)
            outlineModel.diagnosticsActive = diagnosticsEligible;
    }

    Component.onCompleted: {
        outline.expandNewRoots();
        if (outlineModel)
            outlineModel.diagnosticsActive = diagnosticsEligible;
    }

    Rectangle {
        anchors.fill: parent
        color: UISettings.bgStrong
        border.color: UISettings.bgLighter
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 6
        spacing: 4

        RowLayout {
            Layout.fillWidth: true
            Text {
                text: qsTr("Console outline")
                color: UISettings.fgMain
                font.family: UISettings.robotoFontName
                font.pixelSize: UISettings.textSizeDefault
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            ToolButton {
                objectName: "vcOutlineReveal"
                text: qsTr("Reveal")
                enabled: focusedKey.length > 0
                Accessible.name: qsTr("Reveal focused console element")
                onClicked: reveal(focusedKey)
            }
        }

        TextField {
            id: search
            objectName: "vcOutlineSearch"
            Layout.fillWidth: true
            placeholderText: qsTr("Caption, type or ID")
            text: outlineModel ? outlineModel.filterText : ""
            Accessible.name: qsTr("Search console outline")
            rightPadding: clearSearch.width + 4
            onTextChanged: if (outlineModel)
                outlineModel.filterText = text
            Keys.priority: Keys.AfterItem
            Keys.onPressed: event => {
                if (event.key === Qt.Key_Escape) {
                    clear();
                    event.accepted = true;
                }
            }
            ToolButton {
                id: clearSearch
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                width: parent.height
                height: parent.height
                visible: search.text.length > 0
                text: "x"
                Accessible.name: qsTr("Clear outline search")
                onClicked: {
                    search.clear();
                    search.forceActiveFocus();
                }
            }
        }

        Item {
            objectName: "vcOutlineTree"
            Layout.fillWidth: true
            Layout.fillHeight: true

            Flickable {
                id: treeScroll
                anchors.fill: parent
                clip: true
                interactive: !outline.dragging
                contentWidth: width
                contentHeight: rootBranch.height
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: CustomScrollBar {}

                Loader {
                    id: rootBranch
                    width: treeScroll.width
                    sourceComponent: branchComponent
                    onLoaded: item.initialized = true
                }
            }

            Text {
                objectName: "vcOutlineNoMatches"
                anchors.centerIn: parent
                width: parent.width
                visible: outlineModel && outlineModel.revision > 0 && outlineModel.filteredModel.rowCount() === 0
                text: qsTr("No matching elements")
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                color: UISettings.fgMain
                font.family: UISettings.robotoFontName
                font.pixelSize: UISettings.textSizeDefault
            }
        }

        Rectangle {
            id: assignmentSummaryPane
            objectName: "vcAssignmentSummaryPane"
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(outline.height * 0.45, UISettings.listItemHeight * 9)
            Layout.minimumHeight: UISettings.listItemHeight * 3.5
            color: UISettings.bgMedium
            border.color: UISettings.bgLighter
            radius: 2

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 6
                spacing: 3

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: qsTr("Assignment summary")
                        color: UISettings.fgMain
                        font.family: UISettings.robotoFontName
                        font.pixelSize: UISettings.textSizeDefault * 0.9
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Text {
                        text: qsTr("%1 bindings").arg(scopeSummary.inputBindingCount || 0)
                        color: UISettings.fgMedium
                        font.family: UISettings.robotoFontName
                        font.pixelSize: UISettings.textSizeDefault * 0.75
                    }
                }

                Text {
                    objectName: "vcAssignmentSummaryTotals"
                    Layout.fillWidth: true
                    text: qsTr("Widgets %1 (own %2, descendants %3) • Inputs %4 (own %5, descendants %6)")
                        .arg(scopeSummary.assignedWidgetCount || 0)
                        .arg(scopeSummary.ownAssignedWidgetCount || 0)
                        .arg(scopeSummary.descendantAssignedWidgetCount || 0)
                        .arg(scopeSummary.inputBindingCount || 0)
                        .arg(scopeSummary.ownInputBindingCount || 0)
                        .arg(scopeSummary.descendantInputBindingCount || 0)
                    color: UISettings.fgMedium
                    wrapMode: Text.Wrap
                    font.family: UISettings.robotoFontName
                    font.pixelSize: UISettings.textSizeDefault * 0.72
                }

                Text {
                    objectName: "vcAssignmentSummaryNoRows"
                    Layout.fillWidth: true
                    visible: (scopeSummary.inputBindingCount || 0) === 0
                    text: qsTr("No external assignments in this scope.")
                    color: UISettings.fgMedium
                    wrapMode: Text.Wrap
                    font.family: UISettings.robotoFontName
                    font.pixelSize: UISettings.textSizeDefault * 0.82
                }

                Text {
                    objectName: "vcAssignmentSummaryNoMatches"
                    Layout.fillWidth: true
                    visible: (scopeSummary.inputBindingCount || 0) > 0 &&
                             assignmentVisibleRows === 0 &&
                             outlineModel && (outlineModel.filterText || "").length > 0
                    text: qsTr("No matching assignments for the current search.")
                    color: UISettings.fgMedium
                    wrapMode: Text.Wrap
                    font.family: UISettings.robotoFontName
                    font.pixelSize: UISettings.textSizeDefault * 0.82
                }

                Flickable {
                    id: assignmentScroll
                    objectName: "vcAssignmentSummaryScroll"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    visible: assignmentVisibleRows > 0
                    contentWidth: assignmentRowsColumn.width
                    contentHeight: assignmentRowsColumn.height
                    rightMargin: assignmentSummaryScrollBar.width
                    boundsMovement: Flickable.StopAtBounds
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: CustomScrollBar {
                        id: assignmentSummaryScrollBar
                        objectName: "vcAssignmentSummaryScrollBar"
                        anchors.bottomMargin: assignmentSummaryScrollBarHorizontal.height
                    }
                    ScrollBar.horizontal: CustomScrollBar {
                        id: assignmentSummaryScrollBarHorizontal
                        objectName: "vcAssignmentSummaryScrollBarHorizontal"
                        orientation: Qt.Horizontal
                        anchors.rightMargin: assignmentSummaryScrollBar.width
                    }

                    Column {
                        id: assignmentRowsColumn
                        objectName: "vcAssignmentRowsColumn"
                        readonly property real tablePadding: 4
                        readonly property real tableSpacing: 6
                        readonly property real actionWidth: assignmentActionMetric.implicitWidth
                        readonly property real verticalBarExtent: Math.max(assignmentSummaryScrollBar.width, assignmentSummaryScrollBar.implicitWidth)
                        readonly property real horizontalBarExtent: Math.max(assignmentSummaryScrollBarHorizontal.height, assignmentSummaryScrollBarHorizontal.implicitHeight)
                        readonly property real verticalBarOccupancy: assignmentSummaryScrollBar.visible ? verticalBarExtent : 0
                        readonly property real viewportWidth: Math.max(0, assignmentScroll.width - verticalBarOccupancy)
                        readonly property real minWidgetWidth: assignmentHeaderWidgetMetric.implicitWidth
                        readonly property real minInputWidth: assignmentHeaderInputMetric.implicitWidth
                        readonly property real minFeedbackWidth: assignmentHeaderFeedbackMetric.implicitWidth
                        readonly property real minColumnsWidth: minWidgetWidth + minInputWidth + minFeedbackWidth
                        readonly property real availableColumnsWidth: Math.max(0, viewportWidth - (tablePadding * 2) - actionWidth - (tableSpacing * 3))
                        readonly property bool canFitColumns: availableColumnsWidth >= minColumnsWidth
                        readonly property real extraColumnsWidth: canFitColumns ? (availableColumnsWidth - minColumnsWidth) : 0
                        readonly property real widgetColumnWidth: minWidgetWidth + extraColumnsWidth * 3 / 7
                        readonly property real inputColumnWidth: minInputWidth + extraColumnsWidth * 2 / 7
                        readonly property real feedbackColumnWidth: minFeedbackWidth + extraColumnsWidth * 2 / 7
                        readonly property real tableContentWidth: (tablePadding * 2) + actionWidth + (tableSpacing * 3) +
                                                                 widgetColumnWidth + inputColumnWidth + feedbackColumnWidth
                        width: canFitColumns ? assignmentScroll.width :
                                              Math.max(assignmentScroll.width, tableContentWidth + verticalBarOccupancy)
                        spacing: 4

                        Text {
                            id: assignmentHeaderWidgetMetric
                            visible: false
                            text: qsTr("Widget")
                            font.family: UISettings.robotoFontName
                            font.pixelSize: UISettings.textSizeDefault * 0.75
                            font.bold: true
                        }

                        Text {
                            id: assignmentHeaderInputMetric
                            visible: false
                            text: qsTr("Input")
                            font.family: UISettings.robotoFontName
                            font.pixelSize: UISettings.textSizeDefault * 0.75
                            font.bold: true
                        }

                        Text {
                            id: assignmentHeaderFeedbackMetric
                            visible: false
                            text: qsTr("Feedback")
                            font.family: UISettings.robotoFontName
                            font.pixelSize: UISettings.textSizeDefault * 0.75
                            font.bold: true
                        }

                        ToolButton {
                            id: assignmentActionMetric
                            visible: false
                            text: qsTr("Show")
                        }

                        Rectangle {
                            objectName: "vcAssignmentTableHeader"
                            width: parent.width
                            height: UISettings.listItemHeight
                            color: UISettings.bgLighter
                            border.color: UISettings.bgLight
                            border.width: 1

                            Item {
                                anchors.fill: parent
                                anchors.margins: assignmentRowsColumn.tablePadding

                                Text {
                                    objectName: "vcAssignmentHeaderWidget"
                                    x: 0
                                    width: assignmentRowsColumn.widgetColumnWidth
                                    text: qsTr("Widget")
                                    color: UISettings.fgMain
                                    font.family: UISettings.robotoFontName
                                    font.pixelSize: UISettings.textSizeDefault * 0.75
                                    font.bold: true
                                    elide: Text.ElideRight
                                }

                                Text {
                                    objectName: "vcAssignmentHeaderInput"
                                    x: assignmentRowsColumn.widgetColumnWidth + assignmentRowsColumn.tableSpacing
                                    width: assignmentRowsColumn.inputColumnWidth
                                    text: qsTr("Input")
                                    color: UISettings.fgMain
                                    font.family: UISettings.robotoFontName
                                    font.pixelSize: UISettings.textSizeDefault * 0.75
                                    font.bold: true
                                    elide: Text.ElideRight
                                }

                                Text {
                                    objectName: "vcAssignmentHeaderFeedback"
                                    x: assignmentRowsColumn.widgetColumnWidth + assignmentRowsColumn.inputColumnWidth +
                                       (assignmentRowsColumn.tableSpacing * 2)
                                    width: assignmentRowsColumn.feedbackColumnWidth
                                    text: qsTr("Feedback")
                                    color: UISettings.fgMain
                                    font.family: UISettings.robotoFontName
                                    font.pixelSize: UISettings.textSizeDefault * 0.75
                                    font.bold: true
                                    elide: Text.ElideRight
                                }

                                Item {
                                    x: assignmentRowsColumn.widgetColumnWidth + assignmentRowsColumn.inputColumnWidth +
                                       assignmentRowsColumn.feedbackColumnWidth + (assignmentRowsColumn.tableSpacing * 3)
                                    width: assignmentRowsColumn.actionWidth
                                    height: parent.height
                                }
                            }
                        }

                        Repeater {
                            id: assignmentRowsRepeater
                            model: outlineModel ? outlineModel.assignmentRows : null
                            delegate: Column {
                                objectName: "vcAssignmentSummaryRow"
                                required property var values
                                readonly property var row: values
                                width: assignmentRowsColumn.width
                                spacing: 2

                                Text {
                                    width: parent.width
                                    visible: !!row.deviceGroupStart
                                    text: row.deviceGroupLabel && row.deviceGroupLabel.length ?
                                          row.deviceGroupLabel : qsTr("Unresolved routes")
                                    color: UISettings.highlight
                                    wrapMode: Text.Wrap
                                    font.family: UISettings.robotoFontName
                                    font.pixelSize: UISettings.textSizeDefault * 0.82
                                    font.bold: true
                                }

                                Rectangle {
                                    id: assignmentTableRow
                                    objectName: "vcAssignmentTableRow"
                                    readonly property string detailsText:
                                        qsTr("Widget path: %1\nInput labels: %2 • %3\nOutput labels: %4\nFeedback labels: lower %5 (%6), upper %7 (%8), monitor %9 (%10)\nSource raw: universe %11, packed %12, channel %13, page %14, packed MIDI %15\nInput route: %16 • %17 • %18 • UID %19 • line %20 • source %21\nOutput route: %22 • %23 • UID %24 • line %25\nRoute parameters: input %26 • output %27\nRoute state: input %28, output %29%30\nStored feedback: low %31, high %32, monitor %33, params %34/%35/%36")
                                            .arg(row.widgetPath || "-")
                                            .arg(row.midiMessageLabel || "-")
                                            .arg(row.midiSelectionLabel || "-")
                                            .arg(row.midiOutputSelectionLabel || "-")
                                            .arg(row.midiFeedbackLowerLabel || "-")
                                            .arg(row.midiFeedbackLowerState || "unknown")
                                            .arg(row.midiFeedbackUpperLabel || "-")
                                            .arg(row.midiFeedbackUpperState || "unknown")
                                            .arg(row.midiFeedbackMonitorLabel || "-")
                                            .arg(row.midiFeedbackMonitorState || "unknown")
                                            .arg(row.sourceUniverse !== undefined ? row.sourceUniverse : "-")
                                            .arg(row.sourceChannelRaw !== undefined ? row.sourceChannelRaw : "-")
                                            .arg(row.sourceChannel !== undefined ? row.sourceChannel : "-")
                                            .arg(row.sourcePage !== undefined ? row.sourcePage : "-")
                                            .arg(row.midiPackedChannelIndex !== undefined ? row.midiPackedChannelIndex : "-")
                                            .arg(row.inputDeviceName || "-")
                                            .arg(row.inputProfileName || row.inputPlugin || "-")
                                            .arg(row.sourceLabel || row.sourceIdentity || "-")
                                            .arg(row.inputUID || "-")
                                            .arg(row.inputLine !== undefined ? row.inputLine : "-")
                                            .arg(row.sourceID !== undefined ? row.sourceID : "-")
                                            .arg(row.outputDeviceName || "-")
                                            .arg(row.outputPlugin || "-")
                                            .arg(row.outputUID || "-")
                                            .arg(row.outputLine !== undefined ? row.outputLine : "-")
                                            .arg(compactMapString(row.inputParameters))
                                            .arg(compactMapString(row.outputParameters))
                                            .arg(row.inputRouteAvailability || "unknown")
                                            .arg(row.outputRouteAvailability || "unknown")
                                            .arg(row.sharedAssignment ? (", " + (row.sharedAssignmentLabel || qsTr("Shared physical input address"))) : "")
                                            .arg(row.feedbackLowerValue !== undefined ? row.feedbackLowerValue : "-")
                                            .arg(row.feedbackUpperValue !== undefined ? row.feedbackUpperValue : "-")
                                            .arg(row.feedbackMonitorValue !== undefined ? row.feedbackMonitorValue : "-")
                                            .arg(row.feedbackLowerParams !== undefined ? row.feedbackLowerParams : "-")
                                            .arg(row.feedbackUpperParams !== undefined ? row.feedbackUpperParams : "-")
                                            .arg(row.feedbackMonitorParams !== undefined ? row.feedbackMonitorParams : "-")
                                    width: parent.width
                                    height: Math.max(UISettings.listItemHeight * 1.2,
                                                     assignmentActionMetric.implicitHeight +
                                                     (assignmentRowsColumn.tablePadding * 2))
                                    color: assignmentRowHover.containsMouse || activeFocus ? UISettings.bgLighter : "transparent"
                                    border.color: UISettings.bgLighter
                                    border.width: 1
                                    activeFocusOnTab: true
                                    Accessible.role: Accessible.ListItem
                                    Accessible.name: row.widgetPath || ""
                                    Accessible.description: detailsText
                                    Keys.onReturnPressed: outline.showAssignmentWidget(row)
                                    Keys.onEnterPressed: outline.showAssignmentWidget(row)
                                    ToolTip.visible: assignmentRowHover.containsMouse || activeFocus
                                    ToolTip.text: detailsText
                                    ToolTip.delay: 150
                                    ToolTip.timeout: 30000

                                    MouseArea {
                                        id: assignmentRowHover
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        acceptedButtons: Qt.NoButton
                                    }

                                    Item {
                                        anchors.fill: parent
                                        anchors.margins: assignmentRowsColumn.tablePadding

                                        Text {
                                            objectName: "vcAssignmentCellWidget"
                                            x: 0
                                            width: assignmentRowsColumn.widgetColumnWidth
                                            text: row.widgetPath || "-"
                                            color: UISettings.fgMain
                                            font.family: UISettings.robotoFontName
                                            font.pixelSize: UISettings.textSizeDefault * 0.76
                                            elide: Text.ElideRight
                                        }

                                        Text {
                                            objectName: "vcAssignmentCellInput"
                                            x: assignmentRowsColumn.widgetColumnWidth + assignmentRowsColumn.tableSpacing
                                            width: assignmentRowsColumn.inputColumnWidth
                                            text: qsTr("%1 • %2")
                                                .arg(row.sourceLabel || row.sourceIdentity || "-")
                                                .arg(compactMapString(row.inputParameters))
                                            color: UISettings.fgMedium
                                            font.family: UISettings.robotoFontName
                                            font.pixelSize: UISettings.textSizeDefault * 0.74
                                            elide: Text.ElideRight
                                        }

                                        Text {
                                            objectName: "vcAssignmentCellFeedback"
                                            x: assignmentRowsColumn.widgetColumnWidth + assignmentRowsColumn.inputColumnWidth +
                                               (assignmentRowsColumn.tableSpacing * 2)
                                            width: assignmentRowsColumn.feedbackColumnWidth
                                            text: qsTr("%1 • %2")
                                                .arg(row.midiFeedbackLabel || row.midiOutputSelectionLabel || "-")
                                                .arg(compactMapString(row.outputParameters))
                                            color: UISettings.fgMedium
                                            font.family: UISettings.robotoFontName
                                            font.pixelSize: UISettings.textSizeDefault * 0.74
                                            elide: Text.ElideRight
                                        }

                                        ToolButton {
                                            objectName: "vcAssignmentShowWidget"
                                            x: assignmentRowsColumn.widgetColumnWidth + assignmentRowsColumn.inputColumnWidth +
                                               assignmentRowsColumn.feedbackColumnWidth + (assignmentRowsColumn.tableSpacing * 3)
                                            width: assignmentRowsColumn.actionWidth
                                            text: qsTr("Show")
                                            activeFocusOnTab: true
                                            property string detailsText: assignmentTableRow.detailsText
                                            Accessible.description: assignmentTableRow.detailsText
                                            onClicked: outline.showAssignmentWidget(row)
                                        }
                                    }
                                }
                            }
                        }

                        Item {
                            width: assignmentRowsColumn.width
                            height: assignmentRowsColumn.horizontalBarExtent + assignmentActionMetric.implicitHeight
                        }

                    }
                }
            }
        }

        Timer {
            interval: 30
            repeat: true
            running: outline.dragging
            onTriggered: {
                var p = treeScroll.mapFromItem(outlineDrag.parent, outlineDrag.x + outlineDrag.Drag.hotSpot.x, outlineDrag.y + outlineDrag.Drag.hotSpot.y);
                var edge = UISettings.listItemHeight;
                if (p.x < 0 || p.x > treeScroll.width)
                    return;
                var step = edge / 3;
                var maximum = Math.max(0, treeScroll.contentHeight - treeScroll.height);
                if (p.y >= -edge && p.y < edge)
                    treeScroll.contentY = Math.max(0, treeScroll.contentY - step);
                else if (p.y > treeScroll.height - edge && p.y <= treeScroll.height + edge)
                    treeScroll.contentY = Math.min(maximum, treeScroll.contentY + step);
            }
        }

        Text {
            Layout.fillWidth: true
            text: dropHint.length ? dropHint : qsTr("%1 selected").arg(virtualConsole.selectedWidgetsCount)
            color: dropHint.length ? UISettings.highlight : UISettings.fgMedium
            wrapMode: Text.Wrap
            font.family: UISettings.robotoFontName
            font.pixelSize: UISettings.textSizeDefault * 0.85
        }
    }

    Component {
        id: branchComponent
        Column {
            id: branch
            property string parentKey: ""
            property int depth: 0
            property bool initialized: false
            width: parent ? parent.width : 0

            Repeater {
                model: DelegateModel {
                    model: branch.initialized && outlineModel ? outlineModel.filteredModel : null
                    rootIndex: {
                        if (!outlineModel)
                            return undefined;
                        var revision = outlineModel.revision;
                        return outlineModel.indexForKey(branch.parentKey);
                    }
                    delegate: nodeComponent
                }
            }
            Component {
                id: nodeComponent
                Column {
                    id: node
                    required property string label
                    required property var widgetID
                    required property string typeName
                    required property int rowKind
                    required property int subpageIndex
                    required property var parentID
                    required property bool selected
                    required property string iconSource
                    required property string rowKey
                    required property string parentKey
                    required property bool isContainer
                    // Proxy layout changes temporarily reset DelegateModel.rootIndex to the tree root.
                    property bool belongsToBranch: parentKey === branch.parentKey
                    property bool expanded: search.text.length > 0 || expandedKeys.indexOf(rowKey) >= 0
                    property var dropIntent: null
                    visible: belongsToBranch
                    width: parent ? parent.width : 0

                    function insertion(drop) {
                        var into = rowKind !== 1 || (isContainer && drop.y >= row.height * 0.25 && drop.y <= row.height * 0.75);
                        var after = drop.y > row.height / 2;
                        var owner = into ? (rowKind === 2 ? parentID : widgetID) : parentID;
                        var target = virtualConsole.widget(owner);
                        var page = into ? (rowKind === 2 ? subpageIndex : target.currentPage) : subpageIndex;
                        var before = -1;
                        if (!into) {
                            var siblings = target.orderedChildIDs(page);
                            var index = siblings.indexOf(widgetID);
                            before = after ? (index + 1 < siblings.length ? siblings[index + 1] : -1) : widgetID;
                        }
                        return {
                            targetID: owner,
                            subpage: page,
                            beforeID: before,
                            into: into,
                            after: after,
                            hint: (into ? qsTr("Into %1").arg(label) : (after ? qsTr("After %1").arg(label) : qsTr("Before %1").arg(label))) + " / " + target.caption + " / " + qsTr("Page %1").arg(page + 1)
                        };
                    }

                    Rectangle {
                        id: row
                        objectName: "vcOutlineRow-" + (node.rowKind === 1 ? node.widgetID : node.rowKey)
                        Accessible.id: objectName
                        width: node.width
                        height: UISettings.listItemHeight * 1.3
                        color: node.selected || (node.dropIntent && node.dropIntent.into) ? UISettings.highlight : outline.focusedKey === node.rowKey ? UISettings.bgLighter : "transparent"
                        border.width: outline.activeFocus && !search.activeFocus && outline.focusedKey === node.rowKey ? 1 : 0
                        border.color: UISettings.fgMain
                        Accessible.role: Accessible.TreeItem
                        Accessible.name: node.label + ", " + node.typeName + (node.rowKind === 1 ? ", " + qsTr("ID %1").arg(node.widgetID) : "")
                        Accessible.selectable: node.rowKind === 1
                        Accessible.selected: node.selected
                        Accessible.focusable: true
                        Accessible.focused: outline.activeFocus && !search.activeFocus && outline.focusedKey === node.rowKey
                        Accessible.description: node.isContainer ? (node.expanded ? qsTr("Expanded") : qsTr("Collapsed")) : ""
                        Accessible.onPressAction: {
                            outline.forceActiveFocus();
                            outline.selectKey(node.rowKey, 0);
                        }

                        Row {
                            anchors.fill: parent
                            anchors.leftMargin: branch.depth * 14
                            ToolButton {
                                width: row.height * 0.6
                                height: row.height
                                text: node.isContainer ? (node.expanded ? "-" : "+") : ""
                                enabled: node.isContainer
                                Accessible.name: node.expanded ? qsTr("Collapse %1").arg(node.label) : qsTr("Expand %1").arg(node.label)
                                onClicked: outline.toggleExpanded(node.rowKey)
                            }
                            Image {
                                width: row.height * 0.65
                                height: row.height
                                source: node.iconSource
                                fillMode: Image.PreserveAspectFit
                            }
                            Column {
                                width: Math.max(0, row.width - x - 4)
                                anchors.verticalCenter: parent.verticalCenter
                                Text {
                                    width: parent.width
                                    text: node.label
                                    color: UISettings.fgMain
                                    font.family: UISettings.robotoFontName
                                    font.pixelSize: UISettings.textSizeDefault * 0.9
                                    elide: Text.ElideRight
                                }
                                Text {
                                    width: parent.width
                                    text: node.typeName + (node.rowKind === 1 ? " #" + node.widgetID : "")
                                    color: UISettings.fgMedium
                                    font.family: UISettings.robotoFontName
                                    font.pixelSize: UISettings.textSizeDefault * 0.7
                                    elide: Text.ElideRight
                                }
                            }
                        }

                        MouseArea {
                            id: mouseArea
                            anchors.fill: parent
                            anchors.leftMargin: branch.depth * 14 + row.height * 0.6
                            drag.target: node.rowKind === 1 && virtualConsole.editMode ? outlineDrag : null
                            drag.threshold: 10
                            preventStealing: true
                            onPressed: mouse => {
                                outline.forceActiveFocus();
                                outline.dragCanceled = false;
                                if (!node.selected || mouse.modifiers)
                                    outline.selectKey(node.rowKey, mouse.modifiers);
                                else
                                    outline.focusedKey = node.rowKey;
                                var p = mapToItem(outlineDrag.parent, mouse.x, mouse.y);
                                outlineDrag.x = p.x - outlineDrag.Drag.hotSpot.x;
                                outlineDrag.y = p.y - outlineDrag.Drag.hotSpot.y;
                                outlineDrag.widgetIDs = virtualConsole.selectedIDs;
                            }
                            onClicked: mouse => {
                                if (!mouse.modifiers)
                                    outline.selectKey(node.rowKey, 0);
                            }
                            onDoubleClicked: outline.reveal(node.rowKey)
                            onPositionChanged: mouse => {
                                if (drag.active && !outline.dragCanceled) {
                                    var p = mapToItem(outlineDrag.parent, mouse.x, mouse.y);
                                    outlineDrag.x = p.x - outlineDrag.Drag.hotSpot.x;
                                    outlineDrag.y = p.y - outlineDrag.Drag.hotSpot.y;
                                }
                            }
                            drag.onActiveChanged: outline.dragging = drag.active && !outline.dragCanceled
                            onReleased: {
                                if (outline.dragging)
                                    outlineDrag.Drag.drop();
                                outline.dragging = false;
                                outline.dropHint = "";
                            }
                            onCanceled: {
                                outline.dragging = false;
                                outline.dropHint = "";
                            }
                        }

                        DropArea {
                            anchors.fill: parent
                            keys: ["vcoutline"]
                            onEntered: drag => {
                                node.dropIntent = node.insertion(drag);
                                outline.dropHint = node.dropIntent.hint;
                            }
                            onPositionChanged: drag => {
                                node.dropIntent = node.insertion(drag);
                                outline.dropHint = node.dropIntent.hint;
                            }
                            onExited: {
                                node.dropIntent = null;
                                outline.dropHint = "";
                            }
                            onDropped: drop => {
                                if (!virtualConsole.editMode)
                                    return;
                                var intent = node.insertion(drop);
                                var result = virtualConsole.editStructureBefore(drop.source.widgetIDs, intent.targetID, intent.subpage, intent.beforeID);
                                outline.reportResult(result);
                                node.dropIntent = null;
                                outline.dropHint = "";
                                if (result.accepted)
                                    drop.acceptProposedAction();
                            }
                        }

                        Rectangle {
                            visible: node.dropIntent && !node.dropIntent.into
                            width: parent.width
                            height: 2
                            y: node.dropIntent && node.dropIntent.after ? parent.height - height : 0
                            color: UISettings.highlight
                        }

                        Connections {
                            target: outline
                            function onFocusedKeyChanged() {
                                if (outline.focusedKey !== node.rowKey)
                                    return;
                                var position = row.mapToItem(treeScroll.contentItem, 0, 0);
                                if (position.y < treeScroll.contentY)
                                    treeScroll.contentY = position.y;
                                else if (position.y + row.height > treeScroll.contentY + treeScroll.height)
                                    treeScroll.contentY = position.y + row.height - treeScroll.height;
                            }
                        }
                    }

                    Loader {
                        width: node.width
                        active: node.belongsToBranch && node.expanded && node.isContainer
                        sourceComponent: branchComponent
                        onLoaded: {
                            item.parentKey = Qt.binding(function () {
                                return node.rowKey;
                            });
                            item.depth = Qt.binding(function () {
                                return branch.depth + 1;
                            });
                            item.initialized = true;
                        }
                    }
                }
            }
        }
    }

    Item {
        id: outlineDrag
        parent: outline.Window.window ? outline.Window.window.contentItem : outline
        property var widgetIDs: []
        width: Math.max(100, outline.width * 0.75)
        height: UISettings.listItemHeight
        visible: outline.dragging
        z: 200
        Drag.active: outline.dragging
        Drag.source: outlineDrag
        Drag.keys: ["vcoutline"]
        Drag.hotSpot: Qt.point(8, height / 2)
        Drag.onActiveChanged: UISettings.internalDragActive = Drag.active
        Rectangle {
            anchors.fill: parent
            color: UISettings.bgStrong
            border.color: UISettings.highlight
            Text {
                anchors.centerIn: parent
                text: qsTr("%1 controls").arg(outlineDrag.widgetIDs.length)
                color: UISettings.fgMain
            }
        }
    }

    CustomPopupDialog {
        id: outlineError
        objectName: "vcOutlineError"
        title: qsTr("Cannot edit console hierarchy")
    }
}
