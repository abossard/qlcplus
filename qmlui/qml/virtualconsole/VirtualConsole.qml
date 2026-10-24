/*
  Q Light Controller Plus
  VirtualConsole.qml

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
import QtQuick.Window

import org.qlcplus.classes 1.0
import "ShortcutUtils.js" as ShortcutUtils
import "."

Rectangle
{
    id: vcContainer
    anchors.fill: parent
    focus: true

    Keys.enabled: virtualConsole ? virtualConsole.editMode : false
    Keys.onPressed: (event) =>
    {
        // keys a text field leaves unhandled must not edit the selection;
        // a detached console window has no MainView to ask
        if (!virtualConsole || !virtualConsole.editMode
            || (mainView.shortcutsBlocked ? mainView.shortcutsBlocked()
                                          : ShortcutUtils.isTextEditing(Window.activeFocusItem)))
            return

        if (event.modifiers & Qt.ControlModifier)
        {
            if (event.key === Qt.Key_C) {
                virtualConsole.copyToClipboard()
                event.accepted = true
            } else if (event.key === Qt.Key_X) {
                virtualConsole.cutToClipboard()
                event.accepted = true
            } else if (event.key === Qt.Key_V) {
                virtualConsole.pasteFromClipboard()
                event.accepted = true
            } else if (event.key === Qt.Key_D) {
                virtualConsole.duplicateSelection()
                event.accepted = true
            } else if (event.key === Qt.Key_A) {
                virtualConsole.selectAll()
                event.accepted = true
            }
        }
        else if (event.key === Qt.Key_Delete || event.key === Qt.Key_Backspace)
        {
            var result = virtualConsole.deleteAuthoringWidgets(virtualConsole.selectedWidgetIDs())
            if (!result.accepted)
            {
                layoutError.message = result.error
                layoutError.open()
            }
            event.accepted = true
        }
        else if (event.key === Qt.Key_Left || event.key === Qt.Key_Right ||
                 event.key === Qt.Key_Up || event.key === Qt.Key_Down)
        {
            var step = (event.modifiers & Qt.ShiftModifier) ? 5 : 1
            var dx = 0, dy = 0
            if (event.key === Qt.Key_Left) dx = -step
            else if (event.key === Qt.Key_Right) dx = step
            else if (event.key === Qt.Key_Up) dy = -step
            else if (event.key === Qt.Key_Down) dy = step
            virtualConsole.nudgeWidgets(dx, dy)
            event.accepted = true
        }
    }
    color: "transparent"
    objectName: "virtualConsole"

    property string contextName: "VC"
    property int selectedPage: virtualConsole.selectedPage
    property bool docLoaded: qlcplus.docLoaded
    property bool outlineOpen: false
    property real outlineWidth: Math.min(UISettings.sidePanelWidth, UISettings.textSizeDefault * 14, width * 0.18)

    Component.onCompleted: virtualConsole.editMode = false

    VCWidgetMenu { area: vcContainer; panel: rightSidePanel }

    Connections
    {
        target: virtualConsole
        function onEditRejected(error)
        {
            if (vcContainer.visible)
            {
                layoutError.message = error
                layoutError.open()
            }
        }
        // a page deleted or restored at the shown index keeps the index, not the page
        function onPagesCountChanged()
        {
            pageLoader.active = false
            pageLoader.active = true
        }
    }

    Shortcut
    {
        sequence: "Ctrl+]"
        enabled: mainView.currentContext === "VC" && !mainView.shortcutsBlocked()
                 && (qlcplus.accessMask & App.AC_VCEditing)
        onActivated:
        {
            if (rightSidePanel.isOpen)
                rightSidePanel.animatePanel(false)
            else if (rightSidePanel.loaderSource !== "")
                rightSidePanel.animatePanel(true)
        }
    }

    onDocLoadedChanged:
    {
        // force a reload of the selected page
        virtualConsole.selectedPage = 0
        pageLoader.active = false
        pageLoader.active = true
    }

    onSelectedPageChanged:
    {
        pageLoader.source = ""
        if (selectedPage < 0)
            return;
        pageLoader.source = "qrc:/VCPageArea.qml"
    }

    function enableContext(ctx, setChecked)
    {
        console.log("VC enable context " + ctx)

        for (var i = 0; i < pagesRepeater.count; i++)
        {
            console.log("Item " + i + " name: " + pagesRepeater.itemAt(i).contextName)
            if (pagesRepeater.itemAt(i).contextName === ctx)
                pagesRepeater.itemAt(i).visible = true
        }
    }

    function activatePage(pageIndex)
    {
        var pageItem = pagesRepeater.itemAt(pageIndex)
        pageItem.click()
    }

    function requestMatrixPopup(target, mparent, type, pos)
    {
        addMatrixPopup.targetFrame = target
        addMatrixPopup.matrixParent = mparent
        addMatrixPopup.wType = type
        addMatrixPopup.wPos = pos
        addMatrixPopup.open()
    }

    VCRightPanel
    {
        id: rightSidePanel
        visible: qlcplus.accessMask & App.AC_VCEditing
        x: parent.width - width
        z: 5
        height: parent.height
    }

    VCOutline
    {
        id: outlinePane
        objectName: "vcOutlinePane"
        visible: outlineOpen && (qlcplus.accessMask & App.AC_VCEditing)
        width: Math.min(outlineWidth, Math.max(UISettings.iconSizeMedium * 3,
                                              vcContainer.width - rightSidePanel.width - UISettings.iconSizeMedium * 4))
        height: parent.height
        z: 5
    }

    Rectangle
    {
        id: outlineDivider
        visible: outlinePane.visible
        x: outlinePane.width
        width: Math.max(3, screenPixelDensity * UISettings.scalingFactor * 0.9)
        height: parent.height
        color: dividerMouse.containsMouse || dividerMouse.pressed ? UISettings.highlight : UISettings.bgLighter
        z: 6
        MouseArea
        {
            id: dividerMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.SplitHCursor
            property real startWidth: 0
            property real startX: 0
            onPressed: (mouse) =>
            {
                startWidth = outlinePane.width
                startX = mapToItem(vcContainer, mouse.x, mouse.y).x
            }
            onPositionChanged: (mouse) =>
            {
                if (pressed)
                    outlineWidth = Math.max(UISettings.iconSizeMedium * 3,
                                            startWidth + mapToItem(vcContainer, mouse.x, mouse.y).x - startX)
            }
        }
    }

    Rectangle
    {
        id: centerView
        x: outlinePane.visible ? outlinePane.width + outlineDivider.width : 0
        width: Math.max(0, parent.width - x - ((qlcplus.accessMask & App.AC_VCEditing) ? rightSidePanel.width : 0))
        height: parent.height
        color: "transparent"
        clip: true

        Rectangle
        {
            id: vcToolbar
            width: parent.width
            height: UISettings.iconSizeMedium
            z: 10
            gradient: Gradient
            {
                id: vcTbGradient
                GradientStop { position: 0; color: UISettings.toolbarStartSub }
                GradientStop { position: 1; color: UISettings.toolbarEnd }
            }

            RowLayout
            {
                id: rowLayout1
                anchors.fill: parent
                spacing: 5
                ButtonGroup { id: vcToolbarGroup }

                IconButton
                {
                    objectName: "vcOutlineToggle"
                    Accessible.name: qsTr("Show or hide console outline")
                    width: vcToolbar.height
                    height: width
                    visible: qlcplus.accessMask & App.AC_VCEditing
                    imgSource: "qrc:/folder.svg"
                    tooltip: qsTr("Show/Hide console outline")
                    checkable: true
                    checked: outlineOpen
                    onToggled: outlineOpen = checked
                }

                Repeater
                {
                    id: pagesRepeater
                    model: virtualConsole.pagesCount
                    delegate:
                        MenuBarEntry
                        {
                            property VCWidget wObj: virtualConsole.page(index)
                            property string contextName: "PAGE-" + index

                            entryText: wObj ? wObj.caption : qsTr("Page " + index)
                            Accessible.id: "vcPage-" + index
                            mFontSize: UISettings.textSizeDefault
                            //editable: true
                            checked: index === virtualConsole.selectedPage ? true : false
                            checkedColor: UISettings.toolbarSelectionSub
                            bgGradient: vcTbGradient
                            ButtonGroup.group: vcToolbarGroup

                            onCheckedChanged:
                            {
                                if (wObj && checked === true)
                                {
                                    if (wObj.requirePIN())
                                        pinRequestPopup.open()
                                    else
                                        virtualConsole.selectedPage = index
                                }
                            }
                            onRightClicked:
                            {
                                visible = false
                                contextManager.detachContext("PAGE-" + index)
                            }
                            onTextChanged:
                            {
                                if (wObj)
                                    wObj.caption = text
                            }

                            PopupPINRequest
                            {
                                id: pinRequestPopup
                                onAccepted:
                                {
                                    if (virtualConsole.validatePagePIN(index, currentPIN, sessionValidate) === true)
                                    {
                                        virtualConsole.selectedPage = index
                                    }
                                    else
                                    {
                                        pinErrorPopup.open()
                                        // invalidate page selection so nothing
                                        // is displayed on screen
                                        virtualConsole.selectedPage = -1
                                    }
                                }
                            }

                            CustomPopupDialog
                            {
                                id: pinErrorPopup
                                title: qsTr("Error")
                                message: qsTr("Invalid PIN entered")
                            }
                        }
                }

                // filler
                Rectangle { Layout.fillWidth: true }

                RecordControl { Layout.fillHeight: true }

                IconButton
                {
                    width: vcToolbar.height
                    height: width
                    imgSource: "qrc:/grid.svg"
                    tooltip: qsTr("Enable/Disable widgets snapping")
                    checkable: true
                    checked: virtualConsole.snapping
                    onToggled: virtualConsole.snapping = checked
                }

                CustomComboBox
                {
                    id: gridSizeCombo
                    width: UISettings.iconSizeMedium * 1.5
                    height: vcToolbar.height - 4
                    visible: virtualConsole.snapping
                    model: ["5", "10", "15", "20", "25", "30"]
                    currentIndex: {
                        var s = Math.round(virtualConsole.snappingSize)
                        var vals = [5, 10, 15, 20, 25, 30]
                        var idx = vals.indexOf(s)
                        return idx >= 0 ? idx : 2
                    }
                    onCurrentTextChanged:
                    {
                        var val = parseInt(currentText)
                        if (val > 0)
                            virtualConsole.snappingSize = val
                    }
                }

                IconButton
                {
                    width: vcToolbar.height
                    height: width
                    visible: virtualConsole.editMode
                    imgSource: "qrc:/grid.svg"
                    tooltip: qsTr("Preview ordered row and column alignment")
                    onClicked:
                    {
                        var selected = virtualConsole.selectedWidget
                        var target = selected && (selected.type === VCWidget.FrameWidget ||
                                                   selected.type === VCWidget.SoloFrameWidget) ?
                                     selected : virtualConsole.page(selectedPage)
                        var result = virtualConsole.previewLayout(target.id, { allowGrowth: true })
                        layoutPreviewPopup.title = qsTr("Align %1").arg(target.caption)
                        var warnings = result.warnings ? result.warnings.join("\n") : ""
                        layoutPreviewPopup.message = result.accepted ?
                                    qsTr("Align controls in stored order. Control sizes and hierarchy are preserved. Confirm to apply.") +
                                    (warnings.length ? "\n\n" + warnings : "") : result.error
                        layoutPreviewPopup.previewAccepted = result.accepted
                        layoutPreviewPopup.open()
                    }
                    RobotoText
                    {
                        anchors.centerIn: parent
                        label: "A"
                        fontSize: UISettings.textSizeDefault * 0.8
                    }
                }

                ZoomItem
                {
                    objectName: "vcZoomControls"
                    width: UISettings.iconSizeMedium * 2
                    implicitHeight: vcToolbar.height - 2
                    fontColor: UISettings.bgStrong
                    onZoomOutClicked: { virtualConsole.setPageScale(-0.1) }
                    onZoomInClicked: { virtualConsole.setPageScale(0.1) }
                }
            }

        }
        Loader
        {
            id: pageLoader
            z: 0
            anchors.top: vcToolbar.bottom
            width: centerView.width
            height: parent.height - vcToolbar.height
            source: "qrc:/VCPageArea.qml"

            onLoaded:
            {
                // set the page
                pageLoader.item.page = selectedPage
            }
        }
    }

    CustomPopupDialog
    {
        id: layoutPreviewPopup
        objectName: "vcLayoutPreview"
        title: qsTr("Align console controls")
        property bool previewAccepted: false
        onAccepted:
        {
            if (previewAccepted)
            {
                var result = virtualConsole.confirmLayout()
                if (!result.accepted)
                {
                    layoutError.message = result.error
                    layoutError.open()
                }
            }
            else
                virtualConsole.cancelLayout()
        }
        onRejected: virtualConsole.cancelLayout()
        onClosed: virtualConsole.cancelLayout()
    }

    VCEditMessage
    {
        id: layoutError
        objectName: "vcLayoutError"
        title: qsTr("Cannot edit console")
    }


    CustomPopupDialog
    {
        id: addMatrixPopup
        z: 99

        property var matrixParent: null
        property VCFrame targetFrame: null
        property string wType: ""
        property point wPos

        title: qsTr("Widget matrix setup")

        contentItem:
            GridLayout
            {
                width: parent.width
                height: UISettings.iconSizeDefault * rows
                columns: 4
                rowSpacing: 5
                columnSpacing: 5

                // row 1
                RobotoText  { label: qsTr("Columns") }
                CustomSpinBox
                {
                    id: mxColumns
                    from: 1
                    to: 99
                    value: 5
                }

                RobotoText  { label: qsTr("Rows") }
                CustomSpinBox
                {
                    id: mxRows
                    from: 1
                    to: 99
                }

                // row 2
                RobotoText  { label: qsTr("Width") }
                CustomSpinBox
                {
                    id: wWidth
                    from: 1
                    to: 999
                    suffix: "px"
                    value: addMatrixPopup.wType === "buttonmatrix" ? screenPixelDensity * 17 : screenPixelDensity * 15
                }

                RobotoText  { label: qsTr("Height") }
                CustomSpinBox
                {
                    id: wHeight
                    from: 1
                    to: 999
                    suffix: "px"
                    value: addMatrixPopup.wType === "buttonmatrix" ? screenPixelDensity * 17 : screenPixelDensity * 40
                }

                // row 3
                Row
                {
                    Layout.columnSpan: 4
                    Layout.fillWidth: true
                    spacing: 10

                    RobotoText  { label: qsTr("Frame type") }
                    CustomCheckBox
                    {
                        id: normalFrameCheck
                        height: UISettings.iconSizeMedium
                        width: height
                        checked: true
                        autoExclusive: true
                    }
                    RobotoText  { label: qsTr("Normal") }
                    CustomCheckBox
                    {
                        id: soloFrameCheck
                        height: UISettings.iconSizeMedium
                        width: height
                        autoExclusive: true
                    }
                    RobotoText  { label: qsTr("Solo") }
                }
            }

        onAccepted: vcEditor.addWidgetMatrix(targetFrame.id, wType, wPos, Qt.size(mxColumns.value, mxRows.value),
                                             Qt.size(wWidth.value, wHeight.value), soloFrameCheck.checked)
    }
}
