/*
  Q Light Controller Plus
  VCWidgetMenu.qml

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
import QtQuick.Controls

import "ShortcutUtils.js" as ShortcutUtils
import "."

/* The menu of the selected widgets, and their properties on double-click */
Popup
{
    id: widgetMenu
    objectName: "vcWidgetMenu"
    padding: 0

    /** The console item the menu opens over */
    property Item area
    /** The side panel that shows the widget properties */
    property Item panel

    function showProperties()
    {
        panel.loaderSource = "qrc:/VCWidgetProperties.qml"
        panel.animatePanel(true)
    }

    Connections
    {
        target: vcEditor
        function onWidgetPropertiesRequested() { widgetMenu.showProperties() }
        function onWidgetMenuRequested(scenePos)
        {
            var at = area.mapFromItem(null, scenePos.x, scenePos.y)
            widgetMenu.x = Math.min(at.x, area.width - widgetMenu.width)
            widgetMenu.y = Math.min(at.y, area.height - widgetMenu.height)
            widgetMenu.open()
        }
    }

    background: Rectangle { color: UISettings.bgStrong; border.color: UISettings.bgLight }

    Column
    {
        // one width for all entries, whatever their text
        width: UISettings.textSizeDefault * 10

        Repeater
        {
            model: [
                { name: "Properties", label: qsTr("Properties"), shortcut: "" },
                { name: "Cut", label: qsTr("Cut"), shortcut: "Ctrl+X" },
                { name: "Copy", label: qsTr("Copy"), shortcut: "Ctrl+C" },
                { name: "Paste", label: qsTr("Paste"), shortcut: "Ctrl+V" },
                { name: "Duplicate", label: qsTr("Duplicate"), shortcut: "Ctrl+D" },
                { name: "Delete", label: qsTr("Delete"), shortcut: "Del" }
            ]

            ContextMenuEntry
            {
                objectName: "vcWidgetMenu" + modelData.name
                entryText: modelData.label
                shortcutText: ShortcutUtils.display(modelData.shortcut)
                visible: modelData.name !== "Paste" || virtualConsole.clipboardItemsCount > 0
                onClicked:
                {
                    widgetMenu.close()
                    switch (modelData.name)
                    {
                        case "Properties": widgetMenu.showProperties(); break
                        case "Cut": virtualConsole.cutToClipboard(); break
                        case "Copy": virtualConsole.copyToClipboard(); break
                        case "Paste": virtualConsole.pasteFromClipboard(); break
                        case "Duplicate": virtualConsole.duplicateSelection(); break
                        case "Delete":
                            var result = virtualConsole.deleteAuthoringWidgets(virtualConsole.selectedIDs)
                            if (!result.accepted)
                                virtualConsole.editRejected(result.error)
                        break
                    }
                }
            }
        }
    }
}
