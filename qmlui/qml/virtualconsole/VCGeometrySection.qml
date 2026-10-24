/*
  Q Light Controller Plus
  VCGeometrySection.qml

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

import org.qlcplus.classes 1.0
import "."

/* Position and size of one widget, centering and same size for several */
SectionBox
{
    id: geometrySection
    sectionLabel: qsTr("Position and size")

    property VCWidget widget: null
    property int selectedCount: virtualConsole.selectedWidgetsCount

    /* one side of the widget geometry, in unzoomed pixels */
    component GeometrySpin: CustomSpinBox
    {
        property VCWidget target
        property string side
        Layout.fillWidth: true
        from: 0
        to: 100000
        value: target ? Math.round(target.logicalGeometry[side]) : 0

        onValueModified:
        {
            var rect = target.logicalGeometry
            var edited = Qt.rect(rect.x, rect.y, rect.width, rect.height)
            edited[side] = value
            virtualConsole.editGeometry(target.id, edited, false)
            // a refused edit leaves the widget as it was: show that again
            value = Qt.binding(function() { return target ? Math.round(target.logicalGeometry[side]) : 0 })
        }
    }

    component LabelButton: IconButton
    {
        property string caption
        width: UISettings.iconSizeDefault
        height: width
        bgColor: UISettings.bgLighter

        RobotoText
        {
            anchors.centerIn: parent
            label: parent.caption
            fontSize: UISettings.textSizeDefault * 0.8
        }
    }

    sectionContents:
      GridLayout
      {
        width: parent.width
        columns: 2
        columnSpacing: 5
        rowSpacing: 4

        RobotoText
        {
            height: UISettings.listItemHeight
            label: qsTr("Position")
            visible: selectedCount === 1
        }

        RowLayout
        {
            Layout.fillWidth: true
            visible: selectedCount === 1
            GeometrySpin { objectName: "geometryXSpin"; target: widget; side: "x" }
            GeometrySpin { objectName: "geometryYSpin"; target: widget; side: "y" }
        }

        RobotoText
        {
            height: UISettings.listItemHeight
            label: qsTr("Size")
            visible: selectedCount === 1
        }

        RowLayout
        {
            Layout.fillWidth: true
            visible: selectedCount === 1
            GeometrySpin { objectName: "geometryWidthSpin"; target: widget; side: "width" }
            GeometrySpin { objectName: "geometryHeightSpin"; target: widget; side: "height" }
        }

        RobotoText
        {
            height: UISettings.listItemHeight
            label: qsTr("Center")
            visible: selectedCount > 1
        }

        Flow
        {
            Layout.fillWidth: true
            visible: selectedCount > 1

            IconButton
            {
                width: UISettings.iconSizeDefault
                height: width
                bgColor: UISettings.bgLighter
                imgSource: "qrc:/layout-xcenter.svg"
                tooltip: qsTr("Center the selected widgets horizontally on the last selected one")
                onClicked: virtualConsole.setWidgetsAlignment(widget, Qt.AlignHCenter)
            }
            IconButton
            {
                width: UISettings.iconSizeDefault
                height: width
                bgColor: UISettings.bgLighter
                imgSource: "qrc:/layout-ycenter.svg"
                tooltip: qsTr("Center the selected widgets vertically on the last selected one")
                onClicked: virtualConsole.setWidgetsAlignment(widget, Qt.AlignVCenter)
            }
        }

        RobotoText
        {
            height: UISettings.listItemHeight
            label: qsTr("Same size")
            visible: selectedCount > 1
        }

        Flow
        {
            Layout.fillWidth: true
            visible: selectedCount > 1

            LabelButton
            {
                caption: qsTr("W")
                tooltip: qsTr("Give the selected widgets the width of the last selected one")
                onClicked: vcEditor.setWidgetsSize(widget, Qt.Horizontal)
            }
            LabelButton
            {
                caption: qsTr("H")
                tooltip: qsTr("Give the selected widgets the height of the last selected one")
                onClicked: vcEditor.setWidgetsSize(widget, Qt.Vertical)
            }
            LabelButton
            {
                caption: qsTr("W+H")
                tooltip: qsTr("Give the selected widgets the size of the last selected one")
                onClicked: vcEditor.setWidgetsSize(widget, Qt.Horizontal | Qt.Vertical)
            }
        }
      }
}
