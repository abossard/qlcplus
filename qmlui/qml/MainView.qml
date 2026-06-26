/*
  Q Light Controller Plus
  MainView.qml

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
    id: mainView
    visible: true
    width: 800
    height: 600
    anchors.fill: parent
    color: UISettings.bgMedium

    property string currentContext: ""
    property int popupCount: 0

    Component.onCompleted:
    {
        UISettings.sidePanelWidth = Math.min(width / 3, UISettings.bigItemHeight * 5)

        var ctx = "FIXANDFUNC"
        // handle Kiosk mode on startup
        if (qlcplus.accessMask === App.AC_VCControl)
            ctx = "VC"
        enableContext(ctx, true)
    }
    onWidthChanged: UISettings.sidePanelWidth = Math.min(width / 3, UISettings.bigItemHeight * 5)

    function enableContext(ctx, setChecked)
    {
        var item = null

        if (ctx === "FIXANDFUNC")
            item = fnfEntry
        else if (ctx === "VC")
            item = vcEntry
        else if (ctx === "SDESK")
            item = sdEntry
        else if (ctx === "SHOWMGR")
            item = smEntry
        else if (ctx === "IOMGR")
            item = ioEntry
        else if (ctx === "FC")
            item = fcEntry
        else if (ctx === "DJMGR")
            item = songEntry

        if (item)
        {
            item.visible = true
            if (setChecked)
                item.checked = true
            return true
        }
        return false
    }

    function switchToContext(ctx, qmlRes)
    {
        if (currentContext === ctx)
            return

        if (enableContext(ctx, true) === true)
        {
            currentContext = ctx
            // show toolbar only if not in kiosk mode
            if (qlcplus.accessMask !== App.AC_VCControl)
                mainToolbar.visible = true
        }
        else
        {
            mainToolbar.visible = false
            currentContext = ""
        }

        if (ctx === "FIXANDFUNC")
            fixAndFuncLoader.active = true
        else if (qmlRes)
            otherViewLoader.source = qmlRes
    }

    function setDimScreen(enable)
    {
        dimScreen.visible = enable
    }

    function openAccessRequest(clientName)
    {
        clientAccessPopup.clientName = clientName
        clientAccessPopup.open()
    }

    function saveProject()
    {
        actionsMenu.handleSaveAction()
    }

    function saveBeforeExit()
    {
        //actionsMenu.open()
        actionsMenu.saveBeforeExit()
    }

    function loadResource(qmlRes)
    {
        if (qmlRes === fnfEntry.ctxRes)
        {
            currentContext = fnfEntry.ctxName
            fnfEntry.checked = true
            fixAndFuncLoader.active = true
        }
        else
        {
            currentContext = "RESOURCE"
            otherViewLoader.source = qmlRes
        }
    }

    function isTextEditingActive()
    {
        return ShortcutUtils.isTextEditing(Window.activeFocusItem)
    }

    function shortcutsBlocked()
    {
        return isTextEditingActive()
            || mainView.popupCount > 0
            || actionsMenu.opened
            || dimScreen.visible
    }

    function projectShortcutsAllowed()
    {
        return !shortcutsBlocked()
            && qlcplus.accessMask !== App.AC_VCControl
    }

    FontLoader
    {
        source: "qrc:/RobotoCondensed-Regular.ttf"
    }

    // Load the "FontAwesome" font for the monochrome icons
    FontLoader
    {
        id: faFontLoader
        source: "qrc:/FontAwesome7-Free-Solid-900.otf"
        onStatusChanged:
        {
            if (status === FontLoader.Ready)
                UISettings.fontAwesomeFontName = faFontLoader.name
        }
    }

    Rectangle
    {
        id: mainToolbar
        visible: qlcplus.accessMask & App.AC_VCControl ? false : true // this is kiosk mode
        width: parent.width
        height: UISettings.iconSizeDefault
        z: 50
        gradient: Gradient
        {
            GradientStop { position: 0; color: UISettings.toolbarStartMain }
            GradientStop { position: 1; color: UISettings.toolbarEnd }
        }

        RowLayout
        {
            spacing: 5
            anchors.fill: parent

            ButtonGroup { id: menuBarGroup }

            MenuBarEntry
            {
                id: actEntry
                Layout.alignment: Qt.AlignTop
                imgSource: "qrc:/qlcplus.svg"
                entryText: qsTr("Actions")
                onPressed: actionsMenu.open()
                autoExclusive: false
                checkable: false

                Image
                {
                    visible: qlcplus.docModified
                    source: "qrc:/filesave.svg"
                    x: 1
                    y: parent.height - height - 1
                    height: parent.height / 3
                    width: height
                    sourceSize: Qt.size(width, height)
                }
            }
            MenuBarEntry
            {
                id: fnfEntry
                property string ctxName: "FIXANDFUNC"
                Layout.alignment: Qt.AlignTop
                property string ctxRes: "qrc:/FixturesAndFunctions.qml"

                //visible: qlcplus.accessMask & App.AC_FunctionEditing
                imgSource: "qrc:/editor.svg"
                entryText: qsTr("Fixtures & Functions")
                tooltip: ShortcutUtils.withShortcut(qsTr("Fixtures & Functions"), "Alt+1")
                checked: false
                ButtonGroup.group: menuBarGroup
                onCheckedChanged:
                {
                    if (checked === true)
                        switchToContext(fnfEntry.ctxName, fnfEntry.ctxRes)
                }
            }
            MenuBarEntry
            {
                id: vcEntry
                Layout.alignment: Qt.AlignTop
                property string ctxName: "VC"
                property string ctxRes: "qrc:/VirtualConsole.qml"

                visible: qlcplus.accessMask & App.AC_VCControl
                imgSource: "qrc:/virtualconsole.svg"
                entryText: qsTr("Virtual Console")
                tooltip: ShortcutUtils.withShortcut(qsTr("Virtual Console"), "Alt+2")
                ButtonGroup.group: menuBarGroup
                onCheckedChanged:
                {
                    if (checked === true)
                        switchToContext(vcEntry.ctxName, vcEntry.ctxRes)
                }
                onRightClicked:
                {
                    vcEntry.visible = false
                    contextManager.detachContext("VC")
                }
            }
            MenuBarEntry
            {
                id: sdEntry
                Layout.alignment: Qt.AlignTop
                property string ctxName: "SDESK"
                property string ctxRes: "qrc:/SimpleDesk.qml"

                visible: qlcplus.accessMask & App.AC_SimpleDesk
                imgSource: "qrc:/simpledesk.svg"
                entryText: qsTr("Simple Desk")
                tooltip: ShortcutUtils.withShortcut(qsTr("Simple Desk"), "Alt+3")
                ButtonGroup.group: menuBarGroup
                onCheckedChanged:
                {
                    if (checked === true)
                        switchToContext(sdEntry.ctxName, sdEntry.ctxRes)
                }
                onRightClicked:
                {
                    sdEntry.visible = false
                    contextManager.detachContext("SDESK")
                }
            }
            MenuBarEntry
            {
                id: smEntry
                Layout.alignment: Qt.AlignTop
                property string ctxName: "SHOWMGR"
                property string ctxRes: "qrc:/ShowManager.qml"

                visible: qlcplus.accessMask & App.AC_ShowManager
                imgSource: "qrc:/showmanager.svg"
                entryText: qsTr("Show Manager")
                tooltip: ShortcutUtils.withShortcut(qsTr("Show Manager"), "Alt+4")
                ButtonGroup.group: menuBarGroup
                onCheckedChanged:
                {
                    if (checked === true)
                        switchToContext(smEntry.ctxName, smEntry.ctxRes)
                }
                onRightClicked:
                {
                    smEntry.visible = false
                    contextManager.detachContext("SHOWMGR")
                }
            }
            MenuBarEntry
            {
                id: ioEntry
                Layout.alignment: Qt.AlignTop
                property string ctxName: "IOMGR"
                property string ctxRes: "qrc:/InputOutputManager.qml"

                visible: qlcplus.accessMask & App.AC_InputOutput
                imgSource: "qrc:/inputoutput.svg"
                entryText: qsTr("Input/Output")
                tooltip: ShortcutUtils.withShortcut(qsTr("Input/Output"), "Alt+5")
                ButtonGroup.group: menuBarGroup
                onCheckedChanged:
                {
                    if (checked === true)
                        switchToContext(ioEntry.ctxName, ioEntry.ctxRes)
                }
                onRightClicked:
                {
                    ioEntry.visible = false
                    contextManager.detachContext("IOMGR")
                }
            }
            MenuBarEntry
            {
                id: fcEntry
                Layout.alignment: Qt.AlignTop
                property string ctxName: "FC"
                property string ctxRes: "qrc:/FlowConsole.qml"

                visible: qlcplus.accessMask & App.AC_VCControl
                imgSource: "qrc:/grid.svg"
                entryText: qsTr("Flow Console")
                tooltip: ShortcutUtils.withShortcut(qsTr("Flow Console"), "Alt+6")
                ButtonGroup.group: menuBarGroup
                onCheckedChanged:
                {
                    if (checked === true)
                        switchToContext(fcEntry.ctxName, fcEntry.ctxRes)
                }
                onRightClicked:
                {
                    fcEntry.visible = false
                    contextManager.detachContext("FC")
                }
            }
            MenuBarEntry
            {
                id: songEntry
                Layout.alignment: Qt.AlignTop
                property string ctxName: "DJMGR"
                property string ctxRes: "qrc:/DjManager.qml"

                visible: qlcplus.accessMask & App.AC_ShowManager
                imgSource: "qrc:/showmanager.svg"
                entryText: qsTr("DJ Manager")
                tooltip: ShortcutUtils.withShortcut(qsTr("DJ Manager"), "Alt+7")
                ButtonGroup.group: menuBarGroup
                onCheckedChanged:
                {
                    if (checked === true)
                        switchToContext(songEntry.ctxName, songEntry.ctxRes)
                }
                onRightClicked:
                {
                    songEntry.visible = false
                    contextManager.detachContext("DJMGR")
                }
            }
            Rectangle
            {
                // acts like an horizontal spacer
                Layout.fillWidth: true
                implicitHeight: parent.height
                color: "transparent"
            }

            // ################## DMX DUMP ##################
            IconButton
            {
                id: sceneDump
                z: 2
                implicitWidth: UISettings.iconSizeDefault
                implicitHeight: UISettings.iconSizeDefault
                Layout.alignment: Qt.AlignTop
                bgColor: "transparent"
                imgSource: "qrc:/dmxdump.svg"
                imgMargins: 10
                tooltip: qsTr("Dump DMX values on a Scene")
                counter: (qlcplus.accessMask & App.AC_FunctionEditing)

                property string bubbleLabel: {
                    if (currentContext === sdEntry.ctxName)
                        return simpleDesk ? simpleDesk.dumpValuesCount : ""
                    else
                        return contextManager ? contextManager.dumpValuesCount : ""
                }

                function updateDumpVariables()
                {
                    if (currentContext === sdEntry.ctxName)
                    {
                        dmxDumpDialog.capabilityMask = simpleDesk ? simpleDesk.dumpChannelMask : 0
                        dmxDumpDialog.channelSetMask = simpleDesk ? simpleDesk.dumpChannelMask : 0
                    }
                    else
                    {
                        dmxDumpDialog.capabilityMask = fixtureManager ? fixtureManager.capabilityMask : 0
                        dmxDumpDialog.channelSetMask = contextManager ? contextManager.dumpChannelMask : 0
                    }
                }

                // channel count bubble
                Rectangle
                {
                    x: -3
                    y: parent.height - height + 3
                    width: sceneDump.width * 0.4
                    height: width
                    color: "red"
                    border.width: 1
                    border.color: UISettings.fgMain
                    radius: 3
                    clip: true
                    visible: sceneDump.bubbleLabel !== "0" ? true : false

                    RobotoText
                    {
                        anchors.centerIn: parent
                        height: parent.height * 0.7
                        label: sceneDump.bubbleLabel
                        fontSize: height
                    }
                }

                MouseArea
                {
                    id: dumpDragArea
                    anchors.fill: parent
                    drag.target: dumpDragItem
                    drag.threshold: 10

                    onClicked: (mouse) =>
                    {
                        sceneDump.updateDumpVariables()
                        dmxDumpDialog.open()
                        dmxDumpDialog.focusEditItem()
                    }

                    property bool dragActive: drag.active

                    onDragActiveChanged:
                    {
                        console.log("Drag active changed: " + dragActive)
                        if (dragActive == false)
                        {
                            dumpDragItem.Drag.drop()
                            dumpDragItem.parent = sceneDump
                            dumpDragItem.x = 0
                            dumpDragItem.y = 0
                        }
                        else
                        {
                            dumpDragItem.parent = mainView
                        }

                        dumpDragItem.Drag.active = dragActive
                    }
                }

                Item
                {
                    id: dumpDragItem
                    z: 99
                    visible: dumpDragArea.drag.active

                    Drag.source: dumpDragItem
                    Drag.keys: [ "dumpValues" ]

                    function itemDropped(id, name)
                    {
                        console.log("Dump values dropped on " + id)
                        functionManager.selectFunctionID(id, false)
                        sceneDump.updateDumpVariables()
                        dmxDumpDialog.sceneName = name
                        dmxDumpDialog.existingScene = true
                        dmxDumpDialog.open()
                        dmxDumpDialog.focusEditItem()
                    }

                    Rectangle
                    {
                        width: UISettings.iconSizeMedium
                        height: width
                        radius: width / 4
                        color: "red"

                        RobotoText
                        {
                            anchors.centerIn: parent
                            label: sceneDump.bubbleLabel
                        }
                    }
                }

                PopupDMXDump
                {
                    id: dmxDumpDialog
                    implicitWidth: Math.min(UISettings.bigItemHeight * 4, mainView.width / 3)

                    onAccepted:
                    {
                        if (currentContext === sdEntry.ctxName)
                        {
                            simpleDesk.dumpDmxChannels(sceneName, getChannelsMask(), existingScene && func ? func.id : -1, nonZeroOnly)
                        }
                        else
                        {
                            contextManager.dumpDmxChannels(getChannelsMask(), sceneName, existingScene && func ? func.id : -1,
                                                           allChannels, nonZeroOnly);
                        }
                    }
                }
            }

            // spacer
            Rectangle
            {
                width: UISettings.iconSizeDefault / 2
                color: "transparent"
            }

            // ################## BEATS ##################
            RobotoText
            {
                label: "BPM: " + (ioManager.bpmNumber > 0 ? ioManager.bpmNumber : qsTr("Off"))
                color: gsMouseArea.containsMouse ? UISettings.bgLight : "transparent"
                fontSize: UISettings.textSizeDefault
                Layout.alignment: Qt.AlignTop
                implicitWidth: width
                implicitHeight: parent.height

                MouseArea
                {
                    id: gsMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: beatSelectionPanel.visible = !beatSelectionPanel.visible
                }
                BeatGeneratorsPanel
                {
                    id: beatSelectionPanel
                    parent: mainView
                    y: mainToolbar.height
                    x: beatIndicator.x - width
                    z: 51
                    visible: false
                }
            }
            Rectangle
            {
                id: beatIndicator
                implicitWidth: height
                implicitHeight: parent.height * 0.5
                Layout.alignment: Qt.AlignVCenter
                radius: height / 2
                border.width: 2
                border.color: UISettings.bgMedium
                color: UISettings.fgMedium

                ColorAnimation on color
                {
                    id: cAnim
                    from: "#00FF00"
                    to: UISettings.fgMedium
                    // half the duration of the current BPM
                    duration: ioManager.bpmNumber ? 30000 / ioManager.bpmNumber : 200
                    running: false
                }

                Connections
                {
                    id: beatSignal
                    target: ioManager
                    function onBeat()
                    {
                        cAnim.restart()
                    }
                }
            }

            // spacer
            Rectangle
            {
                width: UISettings.iconSizeDefault / 2
                color: "transparent"
            }

            // ################## OPEN WEB CONTROL ##################
            IconButton
            {
                id: webControlBtn
                visible: networkManager.webAccessPort > 0
                implicitWidth: UISettings.iconSizeDefault
                implicitHeight: UISettings.iconSizeDefault
                Layout.alignment: Qt.AlignTop
                bgColor: "transparent"
                imgSource: "qrc:/network.svg"
                imgMargins: 8
                tooltip: qsTr("Open DMX Web Control")
                onClicked: Qt.openUrlExternally("http://localhost:" + networkManager.webAccessPort + "/vc/")
            }

            // ################## STOP ALL FUNCTIONS ##################
            IconButton
            {
                id: stopAllButton
                implicitWidth: UISettings.iconSizeDefault
                implicitHeight: UISettings.iconSizeDefault
                Layout.alignment: Qt.AlignTop
                enabled: runningCount ? true : false
                bgColor: "transparent"
                faSource: FontAwesome.fa_octagon
                faColor: "red"
                tooltip: ShortcutUtils.withShortcut(qsTr("Stop all the running functions"),
                    Qt.platform.os === "osx" ? "Meta+Shift+Esc" : "Ctrl+Shift+Esc")

                onClicked: actionsMenu.handleStopAllAction()

                property int runningCount: qlcplus.runningFunctionsCount

                onRunningCountChanged: console.log("Functions running: " + runningCount)

                RobotoText
                {
                    anchors.centerIn: parent
                    height: parent.height * 0.2
                    fontSize: height
                    label: "STOP"
                }

                Rectangle
                {
                    x: parent.width / 2
                    y: parent.height / 2
                    width: parent.width * 0.4
                    height: width
                    color: UISettings.highlight
                    border.width: 1
                    border.color: UISettings.fgMain
                    radius: 3
                    clip: true
                    visible: stopAllButton.runningCount

                    RobotoText
                    {
                        anchors.centerIn: parent
                        height: parent.height * 0.7
                        label: stopAllButton.runningCount
                        fontSize: height
                    }
                }
            }

        } // end of RowLayout
    } // end of mainToolbar

    // Persistent Fixtures & Functions view — survives context switches
    Loader
    {
        id: fixAndFuncLoader
        source: "qrc:/FixturesAndFunctions.qml"
        active: false
        visible: currentContext === "FIXANDFUNC"
        width: parent.width
        height: parent.height - (mainToolbar.visible ? mainToolbar.height : 0)
        y: mainToolbar.visible ? mainToolbar.height : 0
    }

    // Swapping loader for all other contexts
    Loader
    {
        id: otherViewLoader
        active: visible
        visible: currentContext !== "FIXANDFUNC" && currentContext !== ""
        width: parent.width
        height: parent.height - (mainToolbar.visible ? mainToolbar.height : 0)
        y: mainToolbar.visible ? mainToolbar.height : 0
    }

    PopupNetworkConnect { id: clientAccessPopup }

    /** Menu to open/load/save a project */
    ActionsMenu
    {
        id: actionsMenu
        x: 1
        y: actEntry.height + 1
        visible: false
        z: visible ? 99 : 0
    }

    Shortcut
    {
        sequence: StandardKey.New
        enabled: mainView.projectShortcutsAllowed()
        onActivated: actionsMenu.handleNewAction()
    }

    Shortcut
    {
        sequence: StandardKey.Open
        enabled: mainView.projectShortcutsAllowed()
        onActivated: actionsMenu.handleOpenAction()
    }

    Shortcut
    {
        sequence: StandardKey.Save
        enabled: mainView.projectShortcutsAllowed()
        onActivated: actionsMenu.handleSaveAction()
    }

    Shortcut
    {
        sequence: StandardKey.Undo
        enabled: !mainView.shortcutsBlocked()
        onActivated: actionsMenu.handleUndoAction()
    }

    Shortcut
    {
        sequence: StandardKey.Redo
        enabled: !mainView.shortcutsBlocked()
        onActivated: actionsMenu.handleRedoAction()
    }

    Shortcut
    {
        sequence: Qt.platform.os === "osx" ? "Meta+Shift+Esc" : "Ctrl+Shift+Esc"
        enabled: !mainView.shortcutsBlocked()
                 && (qlcplus.accessMask & App.AC_VCControl)
                 && qlcplus.runningFunctionsCount > 0
        onActivated: actionsMenu.handleStopAllAction()
    }

    Shortcut
    {
        sequences: ["F11", "Ctrl+F11"]
        enabled: !mainView.shortcutsBlocked()
        onActivated: actionsMenu.handleFullscreenAction()
    }

    Shortcut { sequence: "Alt+1"; enabled: !mainView.shortcutsBlocked() && fnfEntry.visible; onActivated: fnfEntry.checked = true }
    Shortcut { sequence: "Alt+2"; enabled: !mainView.shortcutsBlocked() && vcEntry.visible; onActivated: vcEntry.checked = true }
    Shortcut { sequence: "Alt+3"; enabled: !mainView.shortcutsBlocked() && sdEntry.visible; onActivated: sdEntry.checked = true }
    Shortcut { sequence: "Alt+4"; enabled: !mainView.shortcutsBlocked() && smEntry.visible; onActivated: smEntry.checked = true }
    Shortcut { sequence: "Alt+5"; enabled: !mainView.shortcutsBlocked() && ioEntry.visible; onActivated: ioEntry.checked = true }
    Shortcut { sequence: "Alt+6"; enabled: !mainView.shortcutsBlocked() && fcEntry.visible; onActivated: fcEntry.checked = true }

    Shortcut
    {
        sequence: "Ctrl+PgDown"
        enabled: !mainView.shortcutsBlocked()
        onActivated:
        {
            var entries = [fnfEntry, vcEntry, sdEntry, smEntry, ioEntry, fcEntry].filter(function(e) { return e.visible })
            var idx = -1
            for (var i = 0; i < entries.length; i++) { if (entries[i].checked) { idx = i; break } }
            if (idx >= 0) entries[(idx + 1) % entries.length].checked = true
        }
    }

    Shortcut
    {
        sequence: "Ctrl+PgUp"
        enabled: !mainView.shortcutsBlocked()
        onActivated:
        {
            var entries = [fnfEntry, vcEntry, sdEntry, smEntry, ioEntry, fcEntry].filter(function(e) { return e.visible })
            var idx = -1
            for (var i = 0; i < entries.length; i++) { if (entries[i].checked) { idx = i; break } }
            if (idx >= 0) entries[(idx - 1 + entries.length) % entries.length].checked = true
        }
    }

    /* Rectangle covering the whole window to
     * have a dimmered background for popups */
    Rectangle
    {
        id: dimScreen
        anchors.fill: parent
        visible: false
        z: 99
        color: Qt.rgba(0, 0, 0, 0.5)
    }

    //PopupDisclaimer { }
}
