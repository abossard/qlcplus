/*
  Q Light Controller Plus
  PopupFunctionWizard.qml

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
import QtQuick.Controls.Basic

import org.qlcplus.classes 1.0
import "."

CustomPopupDialog
{
    id: wizardPopup
    width: mainView.width / 1.5
    title: qsTr("Function Wizard")
    standardButtons: Dialog.NoButton

    property int currentStep: 0

    onOpened:
    {
        currentStep = 0
        functionWizardManager.reset()
    }

    contentItem: ColumnLayout
    {
        width: wizardPopup.width - 40
        spacing: 10

        // Step indicator
        RowLayout
        {
            Layout.fillWidth: true
            spacing: 5

            Repeater
            {
                model: [qsTr("1. Fixtures"), qsTr("2. Functions"), qsTr("3. Options")]
                delegate: Rectangle
                {
                    Layout.fillWidth: true
                    height: 35
                    color: currentStep === index ? UISettings.highlight : UISettings.bgMedium
                    radius: 3
                    border.color: UISettings.bgLight
                    border.width: 1

                    Label
                    {
                        anchors.centerIn: parent
                        text: modelData
                        color: currentStep === index ? "white" : UISettings.fgMedium
                        font.family: UISettings.robotoFontName
                        font.pixelSize: UISettings.textSizeDefault
                        font.bold: currentStep === index
                    }
                }
            }
        }

        // Step content area
        StackLayout
        {
            Layout.fillWidth: true
            Layout.minimumHeight: 350
            currentIndex: currentStep

            // Step 0: Fixture selection
            ColumnLayout
            {
                spacing: 5

                RowLayout
                {
                    Layout.fillWidth: true
                    spacing: 10

                    Label
                    {
                        Layout.fillWidth: true
                        text: qsTr("Select the fixtures you want to create functions for.")
                        color: UISettings.fgMain
                        font.family: UISettings.robotoFontName
                        font.pixelSize: UISettings.textSizeDefault
                    }

                    GenericButton
                    {
                        label: qsTr("Select All")
                        onClicked: functionWizardManager.addAllFixtures()
                    }

                    GenericButton
                    {
                        label: qsTr("Clear")
                        onClicked: functionWizardManager.clearFixtures()
                    }
                }

                Rectangle
                {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: UISettings.bgMedium
                    border.color: UISettings.bgLight

                    ListView
                    {
                        id: fixtureListView
                        anchors.fill: parent
                        anchors.margins: 2
                        clip: true
                        model: functionWizardManager ? functionWizardManager.availableFixtures : []

                        delegate: Rectangle
                        {
                            width: fixtureListView.width
                            height: 35
                            color: modelData.selected ? UISettings.highlightPressed : (index % 2 ? UISettings.bgMedium : UISettings.bgLight)

                            RowLayout
                            {
                                anchors.fill: parent
                                anchors.margins: 2
                                spacing: 5

                                CheckBox
                                {
                                    checked: modelData.selected
                                    onToggled:
                                    {
                                        if (checked)
                                            functionWizardManager.addFixtures([modelData.id])
                                        else
                                            functionWizardManager.removeFixture(modelData.id)
                                    }
                                }

                                Label
                                {
                                    Layout.preferredWidth: 30
                                    text: "#" + modelData.id
                                    color: UISettings.fgMedium
                                    font.family: UISettings.robotoFontName
                                    font.pixelSize: UISettings.textSizeDefault
                                }
                                Label
                                {
                                    Layout.fillWidth: true
                                    text: modelData.name
                                    color: "white"
                                    font.family: UISettings.robotoFontName
                                    font.pixelSize: UISettings.textSizeDefault
                                    elide: Text.ElideRight
                                }
                                Label
                                {
                                    Layout.preferredWidth: 200
                                    text: modelData.manufacturer + " - " + modelData.model
                                    color: UISettings.fgMedium
                                    font.family: UISettings.robotoFontName
                                    font.pixelSize: UISettings.textSizeDefault
                                    elide: Text.ElideRight
                                }
                                Label
                                {
                                    Layout.preferredWidth: 50
                                    text: modelData.channels + " ch"
                                    color: UISettings.fgMedium
                                    font.family: UISettings.robotoFontName
                                    font.pixelSize: UISettings.textSizeDefault
                                }
                            }
                        }

                        Label
                        {
                            anchors.centerIn: parent
                            visible: fixtureListView.count === 0
                            text: qsTr("No fixtures patched in this project.\nAdd fixtures first via the Fixture Manager.")
                            color: UISettings.fgMedium
                            font.family: UISettings.robotoFontName
                            font.pixelSize: UISettings.textSizeDefault
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                }
            }

            // Step 1: Capability / Function type selection
            ColumnLayout
            {
                spacing: 5

                Label
                {
                    text: qsTr("Select the function types to create for the selected fixtures.")
                    color: UISettings.fgMain
                    font.family: UISettings.robotoFontName
                    font.pixelSize: UISettings.textSizeDefault
                }

                Rectangle
                {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: UISettings.bgMedium
                    border.color: UISettings.bgLight

                    ListView
                    {
                        id: capsListView
                        anchors.fill: parent
                        anchors.margins: 2
                        clip: true
                        model: functionWizardManager ? functionWizardManager.capabilitiesList : []

                        delegate: Rectangle
                        {
                            width: capsListView.width
                            height: 40
                            color: index % 2 ? UISettings.bgMedium : UISettings.bgLight

                            RowLayout
                            {
                                anchors.fill: parent
                                anchors.margins: 5

                                CheckBox
                                {
                                    id: capCheckBox
                                    checked: modelData.enabled
                                    onToggled: functionWizardManager.setCapabilityEnabled(index, checked)
                                }

                                Label
                                {
                                    Layout.fillWidth: true
                                    text: modelData.name
                                    color: "white"
                                    font.family: UISettings.robotoFontName
                                    font.pixelSize: UISettings.textSizeDefault * 1.1
                                }
                            }
                        }

                        Label
                        {
                            anchors.centerIn: parent
                            visible: capsListView.count === 0
                            text: qsTr("No capabilities detected.\nPlease add fixtures with color, gobo, or shutter channels.")
                            color: UISettings.fgMedium
                            font.family: UISettings.robotoFontName
                            font.pixelSize: UISettings.textSizeDefault
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                }
            }

            // Step 2: Widget & page options
            ColumnLayout
            {
                spacing: 10

                Label
                {
                    text: qsTr("Configure the Virtual Console widget layout.")
                    color: UISettings.fgMain
                    font.family: UISettings.robotoFontName
                    font.pixelSize: UISettings.textSizeDefault
                }

                GridLayout
                {
                    columns: 2
                    columnSpacing: 10
                    rowSpacing: 8
                    Layout.fillWidth: true

                    // Dedicated page
                    CheckBox
                    {
                        id: dedicatedPageCheck
                        checked: functionWizardManager ? functionWizardManager.createDedicatedPage : true
                        onToggled: functionWizardManager.setCreateDedicatedPage(checked)
                    }
                    Label
                    {
                        text: qsTr("Create a dedicated Virtual Console page")
                        color: "white"
                        font.family: UISettings.robotoFontName
                        font.pixelSize: UISettings.textSizeDefault
                    }

                    // Widgets per line
                    Label
                    {
                        text: qsTr("Widgets per line:")
                        color: UISettings.fgMain
                        font.family: UISettings.robotoFontName
                        font.pixelSize: UISettings.textSizeDefault
                    }
                    SpinBox
                    {
                        id: widgetsPerLineSpin
                        from: 1
                        to: 32
                        value: functionWizardManager ? functionWizardManager.widgetsPerLine : 8
                        onValueModified: functionWizardManager.setWidgetsPerLine(value)
                    }

                    // Slider width
                    Label
                    {
                        text: qsTr("Slider width:")
                        color: UISettings.fgMain
                        font.family: UISettings.robotoFontName
                        font.pixelSize: UISettings.textSizeDefault
                    }
                    SpinBox
                    {
                        id: sliderWidthSpin
                        from: 20
                        to: 200
                        value: functionWizardManager ? functionWizardManager.sliderWidth : 60
                        onValueModified: functionWizardManager.setSliderWidth(value)
                    }

                    // Slider height
                    Label
                    {
                        text: qsTr("Slider height:")
                        color: UISettings.fgMain
                        font.family: UISettings.robotoFontName
                        font.pixelSize: UISettings.textSizeDefault
                    }
                    SpinBox
                    {
                        id: sliderHeightSpin
                        from: 40
                        to: 500
                        value: functionWizardManager ? functionWizardManager.sliderHeight : 200
                        onValueModified: functionWizardManager.setSliderHeight(value)
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }

        // Navigation buttons
        RowLayout
        {
            Layout.fillWidth: true
            spacing: 10

            GenericButton
            {
                label: qsTr("Cancel")
                onClicked: wizardPopup.close()
            }

            Item { Layout.fillWidth: true }

            GenericButton
            {
                label: qsTr("Back")
                visible: currentStep > 0
                onClicked: currentStep--
            }

            GenericButton
            {
                label: currentStep < 2 ? qsTr("Next") : qsTr("Create")
                onClicked:
                {
                    if (currentStep < 2)
                    {
                        currentStep++
                    }
                    else
                    {
                        if (functionWizardManager.execute())
                            wizardPopup.close()
                    }
                }
            }
        }
    }
}
