/*
  Q Light Controller Plus
  AudioAnalysis.qml

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
import QtQuick.Dialogs

import org.qlcplus.classes 1.0

Rectangle
{
    id: analysisContainer
    objectName: "audioAnalysisPage"
    anchors.fill: parent
    color: "transparent"
    property string contextName: "AUDIOANALYSIS"

    readonly property var statusColors: ({ "queued": UISettings.fgMedium, "running": UISettings.highlight,
                                           "done": "#2ecc71", "error": "#e74c3c" })

    FolderDialog
    {
        id: folderDialog
        title: qsTr("Choose a folder with MP3 files")
        onAccepted: audioAnalysis.start(selectedFolder)
    }

    ColumnLayout
    {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        RowLayout
        {
            Layout.fillWidth: true
            spacing: 10

            GenericButton
            {
                objectName: "chooseFolderButton"
                label: qsTr("Analyze folder…")
                enabled: !audioAnalysis.running && audioAnalysis.toolProblem === ""
                onClicked: folderDialog.open()
            }
            GenericButton
            {
                label: qsTr("Stop")
                visible: audioAnalysis.running
                onClicked: audioAnalysis.cancel()
            }
            GenericButton
            {
                objectName: "openReviewButton"
                label: qsTr("Open rating site")
                enabled: audioAnalysis.toolProblem === ""
                onClicked: audioAnalysis.openReview()
            }
            GenericButton
            {
                label: qsTr("Show results folder")
                onClicked: audioAnalysis.revealOutput()
            }
            Item { Layout.fillWidth: true }
        }

        RobotoText
        {
            Layout.fillWidth: true
            visible: audioAnalysis.toolProblem !== ""
            label: audioAnalysis.toolProblem
            labelColor: "#e74c3c"
            wrapText: true
        }

        RobotoText
        {
            Layout.fillWidth: true
            label: audioAnalysis.folder === "" ? qsTr("Pick a folder: every MP3 in it (and its subfolders) gets beats, bars, sections, drops and stem levels. Tracks analyzed before are skipped.")
                                               : audioAnalysis.folder
            labelColor: UISettings.fgMedium
            wrapText: true
        }

        // overall progress
        ColumnLayout
        {
            Layout.fillWidth: true
            visible: audioAnalysis.total > 0
            spacing: 4

            Rectangle
            {
                objectName: "analysisProgress"
                Layout.fillWidth: true
                height: 8
                radius: 4
                color: UISettings.bgMedium

                Rectangle
                {
                    width: parent.width * (audioAnalysis.done + audioAnalysis.failed) / Math.max(1, audioAnalysis.total)
                    height: parent.height
                    radius: 4
                    color: UISettings.highlight
                }
            }
            RobotoText
            {
                Layout.fillWidth: true
                label: qsTr("%1 of %2 done, %3 failed").arg(audioAnalysis.done).arg(audioAnalysis.total).arg(audioAnalysis.failed)
                       + (audioAnalysis.currentTitle !== "" ? "  ·  " + audioAnalysis.currentTitle + " (" + audioAnalysis.currentStage + ")" : "")
            }
        }

        RobotoText
        {
            Layout.fillWidth: true
            visible: audioAnalysis.lastMessage !== ""
            label: audioAnalysis.lastMessage
            labelColor: UISettings.fgMedium
            fontSize: UISettings.textSizeDefault * 0.85
        }

        ListView
        {
            id: trackList
            objectName: "analysisTrackList"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: audioAnalysis.tracks
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: CustomScrollBar { }

            delegate: Rectangle
            {
                width: trackList.width
                height: UISettings.listItemHeight * (model.error !== "" || model.summary !== "" ? 1.6 : 1)
                color: index % 2 ? UISettings.bgMedium : "transparent"

                Rectangle
                {
                    id: dot
                    width: 10
                    height: 10
                    radius: 5
                    anchors.left: parent.left
                    anchors.leftMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    color: analysisContainer.statusColors[model.status] || UISettings.fgMedium
                }

                Column
                {
                    anchors.left: dot.right
                    anchors.leftMargin: 10
                    anchors.right: bpmText.left
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter

                    Text
                    {
                        width: parent.width
                        text: model.title + (model.status === "running" && model.stage !== "" ? "  ·  " + model.stage + "…" : "")
                        color: UISettings.fgMain
                        font.family: UISettings.robotoFontName
                        font.pixelSize: UISettings.textSizeDefault
                        elide: Text.ElideRight
                    }
                    Text
                    {
                        width: parent.width
                        visible: text !== ""
                        text: model.error !== "" ? model.error : model.summary
                        color: model.error !== "" ? "#e74c3c" : UISettings.fgMedium
                        font.family: UISettings.robotoFontName
                        font.pixelSize: UISettings.textSizeDefault * 0.8
                        elide: Text.ElideRight
                    }
                }

                Text
                {
                    id: bpmText
                    anchors.right: parent.right
                    anchors.rightMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    text: model.bpm > 0 ? model.bpm.toFixed(1) + " BPM  ·  " + (model.seconds < 0.5 ? qsTr("already analyzed") : model.seconds.toFixed(0) + " s") : ""
                    color: UISettings.fgMedium
                    font.family: UISettings.robotoFontName
                    font.pixelSize: UISettings.textSizeDefault * 0.85
                }
            }
        }
    }
}
