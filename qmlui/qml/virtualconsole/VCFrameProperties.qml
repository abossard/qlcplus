/*
  Q Light Controller Plus
  VCFrameProperties.qml

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
import "."

Rectangle
{
    color: "transparent"
    height: fPropsColumn.height

    property VCFrame widgetRef: null
    property int gridItemsHeight: UISettings.listItemHeight

    Column
    {
        id: fPropsColumn
        width: parent.width
        spacing: 5

        SectionBox
        {
            sectionLabel: qsTr("Header")

            sectionContents:
              GridLayout
              {
                width: parent.width
                columns: 2
                columnSpacing: 5
                rowSpacing: 3

                // row 1
                RobotoText
                {
                    height: gridItemsHeight
                    Layout.fillWidth: true
                    label: qsTr("Show header")
                }

                CustomCheckBox
                {
                    objectName: "showHeaderCheck"
                    implicitWidth: UISettings.iconSizeMedium
                    implicitHeight: implicitWidth
                    checked: widgetRef ? widgetRef.showHeader : false
                    onCheckedChanged: if (widgetRef) widgetRef.showHeader = checked
                }

                // row 2
                RobotoText
                {
                    height: gridItemsHeight
                    Layout.fillWidth: true
                    label: qsTr("Show enable button")
                }

                CustomCheckBox
                {
                    objectName: "showEnableCheck"
                    implicitWidth: UISettings.iconSizeMedium
                    implicitHeight: implicitWidth
                    checked: widgetRef ? widgetRef.showEnable : false
                    onCheckedChanged: if (widgetRef) widgetRef.showEnable = checked
                }
              }
        }

        SectionBox
        {
            visible: widgetRef && widgetRef.type === VCWidget.SoloFrameWidget ? true : false
            sectionLabel: qsTr("Solo Frame Options")

            sectionContents:
              GridLayout
              {
                width: parent.width
                columns: 2
                columnSpacing: 5
                rowSpacing: 3

                // row 1
                RobotoText
                {
                    height: gridItemsHeight
                    Layout.fillWidth: true
                    label: qsTr("Exclude monitored functions")
                }

                CustomCheckBox
                {
                    objectName: "excludeMonitoredFunctionsCheck"
                    implicitWidth: UISettings.iconSizeMedium
                    implicitHeight: implicitWidth
                    checked: widgetRef ? widgetRef.excludeMonitoredFunctions : false
                    onCheckedChanged: if (widgetRef) widgetRef.excludeMonitoredFunctions = checked
                }
              }
        }

        SectionBox
        {
            sectionLabel: qsTr("Pages")

            sectionContents:
              GridLayout
              {
                width: parent.width
                columns: 2
                columnSpacing: 5
                rowSpacing: 3

                // row 1
                RobotoText
                {
                    height: gridItemsHeight
                    Layout.fillWidth: true
                    label: qsTr("Enable pages")
                }

                CustomCheckBox
                {
                    objectName: "multiPageModeCheck"
                    implicitWidth: UISettings.iconSizeMedium
                    implicitHeight: implicitWidth
                    checked: widgetRef ? widgetRef.multiPageMode : false
                    onCheckedChanged: if (widgetRef) widgetRef.multiPageMode = checked
                }

                // row 2
                RobotoText
                {
                    height: gridItemsHeight
                    Layout.fillWidth: true
                    label: qsTr("Circular pages scrolling")
                }

                CustomCheckBox
                {
                    objectName: "pagesLoopCheck"
                    implicitWidth: UISettings.iconSizeMedium
                    implicitHeight: implicitWidth
                    checked: widgetRef ? widgetRef.pagesLoop : false
                    onCheckedChanged: if (widgetRef) widgetRef.pagesLoop = checked
                }

                // row 3
                RowLayout
                {
                    Layout.columnSpan: 2
                    Layout.fillWidth: true

                    RobotoText
                    {
                        height: gridItemsHeight
                        label: qsTr("Pages number")
                    }

                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        height: gridItemsHeight
                        from: 1
                        to: 100
                        value: widgetRef ? widgetRef.totalPagesNumber : 1
                        onValueChanged: if (widgetRef) widgetRef.totalPagesNumber = value
                    }
                }

                GenericButton
                {
                    enabled: widgetRef ? widgetRef.totalPagesNumber > 1 : false
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                    height: gridItemsHeight
                    label: qsTr("Clone first page widgets")
                    onClicked: if (widgetRef) widgetRef.cloneFirstPage()
                }
              }
        }

        SectionBox
        {
            sectionLabel: qsTr("Arrange children")

            sectionContents:
              GridLayout
              {
                id: arrangeGrid
                width: parent.width
                columns: 2
                columnSpacing: 5
                rowSpacing: 3

                /* every change places the children again, or is refused and shown unchanged */
                function arrange(settings)
                {
                    var result = virtualConsole.arrangeFrame(widgetRef.id, settings)
                    if (!result.accepted)
                        virtualConsole.editRejected(result.error)
                    // once the control finished its own change, it follows the frame again
                    Qt.callLater(function() {
                        arrangeModeCombo.currentIndex = Qt.binding(function() { return widgetRef ? widgetRef.arrangeMode : 0 })
                        arrangeGapSpin.value = Qt.binding(function() { return widgetRef ? widgetRef.arrangeGap : 0 })
                        arrangeColumnsSpin.value = Qt.binding(function() { return widgetRef ? widgetRef.arrangeColumns : 1 })
                        arrangeFillCheck.checked = Qt.binding(function() { return widgetRef ? widgetRef.arrangeFill : false })
                    })
                }

                RobotoText
                {
                    height: gridItemsHeight
                    Layout.fillWidth: true
                    label: qsTr("Arrange")
                }

                CustomComboBox
                {
                    id: arrangeModeCombo
                    objectName: "arrangeModeCombo"
                    Layout.fillWidth: true
                    height: gridItemsHeight
                    model: [
                        { mLabel: qsTr("Freely"), mValue: VCFrame.ArrangeNone },
                        { mLabel: qsTr("In a row"), mValue: VCFrame.ArrangeRow },
                        { mLabel: qsTr("In a column"), mValue: VCFrame.ArrangeColumn },
                        { mLabel: qsTr("In a grid"), mValue: VCFrame.ArrangeGrid },
                        { mLabel: qsTr("Wrapping"), mValue: VCFrame.ArrangeFlow }
                    ]
                    currentIndex: widgetRef ? widgetRef.arrangeMode : 0
                    onCurrentIndexChanged:
                        if (widgetRef && currentIndex !== widgetRef.arrangeMode)
                            arrangeGrid.arrange({ "mode": currentIndex })
                }

                RobotoText
                {
                    height: gridItemsHeight
                    Layout.fillWidth: true
                    label: qsTr("Spacing")
                    visible: arrangeModeCombo.currentIndex !== VCFrame.ArrangeNone
                }

                CustomSpinBox
                {
                    id: arrangeGapSpin
                    objectName: "arrangeGapSpin"
                    Layout.fillWidth: true
                    height: gridItemsHeight
                    visible: arrangeModeCombo.currentIndex !== VCFrame.ArrangeNone
                    suffix: "px"
                    from: 0
                    to: 200
                    value: widgetRef ? widgetRef.arrangeGap : 0
                    onValueModified: arrangeGrid.arrange({ "gap": value })
                }

                RobotoText
                {
                    height: gridItemsHeight
                    Layout.fillWidth: true
                    label: qsTr("Columns")
                    visible: arrangeModeCombo.currentIndex === VCFrame.ArrangeGrid
                }

                CustomSpinBox
                {
                    id: arrangeColumnsSpin
                    objectName: "arrangeColumnsSpin"
                    Layout.fillWidth: true
                    height: gridItemsHeight
                    visible: arrangeModeCombo.currentIndex === VCFrame.ArrangeGrid
                    from: 1
                    to: 64
                    value: widgetRef ? widgetRef.arrangeColumns : 1
                    onValueModified: arrangeGrid.arrange({ "columns": value })
                }

                RobotoText
                {
                    height: gridItemsHeight
                    Layout.fillWidth: true
                    label: qsTr("Stretch to fill")
                    visible: arrangeFillCheck.visible
                }

                CustomCheckBox
                {
                    id: arrangeFillCheck
                    objectName: "arrangeFillCheck"
                    implicitWidth: UISettings.iconSizeMedium
                    implicitHeight: implicitWidth
                    visible: arrangeModeCombo.currentIndex !== VCFrame.ArrangeNone &&
                             arrangeModeCombo.currentIndex !== VCFrame.ArrangeFlow
                    checked: widgetRef ? widgetRef.arrangeFill : false
                    onCheckedChanged:
                        if (widgetRef && checked !== widgetRef.arrangeFill)
                            arrangeGrid.arrange({ "fill": checked })
                }
              }
        }

        SectionBox
        {
            sectionLabel: qsTr("Shortcuts")
            visible: widgetRef ? widgetRef.totalPagesNumber > 1 : false

            sectionContents:
              GridLayout
              {
                width: parent.width
                columns: 2
                columnSpacing: 5
                rowSpacing: 3

                // row 1
                RobotoText
                {
                    height: gridItemsHeight
                    Layout.fillWidth: true
                    label: qsTr("Shortcuts")
                }
                CustomComboBox
                {
                    id: shortcutList
                    Layout.fillWidth: true
                    height: gridItemsHeight
                    textRole: ""
                    model: widgetRef ? widgetRef.pageLabels : null
                }

                // row 2
                RobotoText
                {
                    height: gridItemsHeight
                    Layout.fillWidth: true
                    label: qsTr("Shortcut name")
                }
                CustomTextEdit
                {
                    id: shortcutEdit
                    Layout.fillWidth: true
                    text: shortcutList.currentText
                    onTextEdited:
                    {
                        var idx = shortcutList.currentIndex
                        if (widgetRef)
                            widgetRef.setShortcutName(shortcutList.currentIndex, text)
                        // combo model has changed. Restore selected index
                        shortcutList.currentIndex = idx
                    }
                }
              }
        }
    }

}
