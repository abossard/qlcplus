/*
  Q Light Controller Plus
  VCAudioTriggersProperties.qml

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
    id: propsRoot
    color: "transparent"
    height: audioTriggerPropsColumn.height

    property VCAudioTriggers widgetRef: null

    property int gridItemsHeight: UISettings.listItemHeight
    // 3-bank trigger labels (low/mid/high) — matches AudioSnapshot::triggers[3]
    // and the multi-resolution mel banks. Volume + beat triggers are rendered
    // separately as dedicated lamps elsewhere in the panel.
    property var bankTriggerNames: [ qsTr("Low"), qsTr("Mid"), qsTr("High") ]
    property var bankTriggerColors: [ "#ff9900", "#33cc66", "#33ccff" ]

    function percentText(value)
    {
        return Math.round(Math.max(0, Math.min(1, value)) * 100) + "%"
    }

    // The per-mapping Activation/Deactivation thresholds popup was removed:
    // setBarThresholds() is a no-op since hysteresis is now defined per band
    // on the AudioProfile (triggerLow/Mid/High High/Low/Hold/Cooldown).

    CustomPopupDialog
    {
        id: renamePopup
        width: mainView.width / 3
        title: qsTr("Rename Audio Profile")

        onAccepted: if (widgetRef) widgetRef.renameCurrentProfile(renameField.text)

        contentItem:
            Column
            {
                width: parent.width
                spacing: 5

                RobotoText
                {
                    label: qsTr("New name")
                }

                TextInput
                {
                    id: renameField
                    width: parent.width
                    height: UISettings.listItemHeight
                    color: "white"
                    text: ""
                    font.pixelSize: UISettings.textSizeDefault
                    selectByMouse: true
                }
            }
    }

    Column
    {
        id: audioTriggerPropsColumn
        width: parent.width
        spacing: 5

        SectionBox
        {
            sectionLabel: qsTr("Audio Profile")

            sectionContents:
                GridLayout
                {
                    width: parent.width
                    columns: 2
                    columnSpacing: 6
                    rowSpacing: 4

                    RobotoText
                    {
                        height: gridItemsHeight
                        label: qsTr("Profile")
                    }

                    CustomComboBox
                    {
                        Layout.fillWidth: true
                        height: gridItemsHeight
                        model: widgetRef ? widgetRef.profileListModel : null
                        textRole: "profileName"
                        currentIndex: {
                            if (!widgetRef || !widgetRef.profileListModel) return 0
                            var m = widgetRef.profileListModel
                            for (var i = 0; i < m.rowCount(); i++) {
                                if (m.data(m.index(i, 0), 0x101) === widgetRef.audioProfileId)
                                    return i
                            }
                            return 0
                        }
                        onActivated: {
                            if (widgetRef && widgetRef.profileListModel) {
                                var m = widgetRef.profileListModel
                                var id = m.data(m.index(currentIndex, 0), 0x101)
                                widgetRef.audioProfileId = id
                            }
                        }
                    }

                    RobotoText
                    {
                        Layout.columnSpan: 2
                        height: gridItemsHeight
                        fontSize: UISettings.textSizeSmall
                        label: qsTr("Changes affect all users of this profile")
                        labelColor: "#888888"
                    }

                    // Profile management buttons
                    Row
                    {
                        Layout.columnSpan: 2
                        Layout.fillWidth: true
                        spacing: 4

                        GenericButton
                        {
                            width: (parent.width - 12) / 4
                            height: gridItemsHeight
                            label: qsTr("Reset")
                            onClicked: if (widgetRef) widgetRef.resetProfileToDefaults()
                        }
                        GenericButton
                        {
                            width: (parent.width - 12) / 4
                            height: gridItemsHeight
                            label: qsTr("Duplicate")
                            onClicked: {
                                if (widgetRef)
                                    widgetRef.duplicateCurrentProfile("")
                            }
                        }
                        GenericButton
                        {
                            width: (parent.width - 12) / 4
                            height: gridItemsHeight
                            label: qsTr("Rename")
                            onClicked: {
                                if (widgetRef)
                                    renamePopup.open()
                            }
                        }
                        GenericButton
                        {
                            width: (parent.width - 12) / 4
                            height: gridItemsHeight
                            label: qsTr("Delete")
                            onClicked: if (widgetRef) widgetRef.deleteCurrentProfile()
                        }
                    }
                }
        }

        // The duplicate top "Envelope & Triggers" section was removed:
        // envelope attack/release live in "QLC+: Response — Envelope" and the
        // per-band Schmitt-trigger grid lives in "QLC+: Triggers" below.

        SectionBox
        {
            sectionLabel: qsTr("aubio: Phase Vocoder")

            sectionContents:
                GridLayout
                {
                    width: parent.width
                    columns: 2
                    columnSpacing: 6
                    rowSpacing: 4

                    RobotoText { height: gridItemsHeight; label: qsTr("Window"); tooltipText: qsTr("FFT window shape. Affects frequency resolution vs time resolution.") }
                    CustomComboBox
                    {
                        Layout.fillWidth: true
                        height: gridItemsHeight
                        property var windowList: ["default","hanning","hamming","blackman","blackman_harris","gaussian","welch","parzen","rectangle"]
                        model: windowList
                        currentIndex: {
                            if (!widgetRef) return 0
                            var idx = windowList.indexOf(widgetRef.windowType)
                            return idx >= 0 ? idx : 0
                        }
                        onActivated: if (widgetRef) widgetRef.setWindowType(windowList[currentIndex])
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Window size"); tooltipText: qsTr("FFT size in samples. Larger = better frequency resolution, more latency.") }
                    RobotoText
                    {
                        Layout.fillWidth: true
                        height: gridItemsHeight
                        label: widgetRef ? widgetRef.windowSize + "" : "--"
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Hop size"); tooltipText: qsTr("Samples between analysis frames. Smaller = faster updates, more CPU.") }
                    RobotoText
                    {
                        Layout.fillWidth: true
                        height: gridItemsHeight
                        label: widgetRef ? widgetRef.hopSize + "" : "--"
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Sample rate"); tooltipText: qsTr("Audio input sample rate from your system.") }
                    RobotoText
                    {
                        Layout.fillWidth: true
                        height: gridItemsHeight
                        label: widgetRef ? widgetRef.sampleRate + " Hz" : "--"
                    }
                }
        }


        SectionBox
        {
            sectionLabel: qsTr("aubio: Mel Filterbank")

            sectionContents:
                GridLayout
                {
                    width: parent.width
                    columns: 2
                    columnSpacing: 6
                    rowSpacing: 4

                    RobotoText { height: gridItemsHeight; label: qsTr("Bands"); tooltipText: qsTr("Mel band count is fixed at 40 in this build (AUBIO_MEL_BANDS).") }
                    RobotoText
                    {
                        Layout.fillWidth: true
                        height: gridItemsHeight
                        label: "40"
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Norm"); tooltipText: qsTr("Normalize filter weights. On = equal energy per band.") }
                    CustomCheckBox
                    {
                        Layout.fillWidth: true
                        enabled: widgetRef !== null
                        checked: widgetRef ? widgetRef.filterbankNorm >= 0.5 : true
                        onToggled: if (widgetRef) widgetRef.setFilterbankNorm(checked ? 1 : 0)
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Power"); tooltipText: qsTr("Exponent on magnitude. 1 = linear, 0.5 = sqrt compression, 2 = squared (energy).") }
                    RowLayout
                    {
                        Layout.fillWidth: true
                        spacing: 6
                        Slider
                        {
                            Layout.fillWidth: true
                            from: 0.5; to: 4.0; stepSize: 0.1
                            enabled: widgetRef !== null
                            value: widgetRef ? widgetRef.filterbankPower : 1.0
                            onPressedChanged: if (!pressed && widgetRef) widgetRef.setFilterbankPower(value)
                        }
                        RobotoText
                        {
                            Layout.preferredWidth: UISettings.bigItemHeight
                            height: gridItemsHeight
                            label: widgetRef ? widgetRef.filterbankPower.toFixed(1) : "--"
                        }
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Mel scale"); tooltipText: qsTr("Mel-scale variant. matt_mel = LedFx 20-15kHz default; htk = HTK style; slaney = librosa/Slaney style.") }
                    CustomComboBox
                    {
                        Layout.fillWidth: true
                        height: gridItemsHeight
                        property var melScales: ["matt_mel", "htk", "slaney"]
                        model: melScales
                        currentIndex: {
                            if (!widgetRef) return 0
                            var idx = melScales.indexOf(widgetRef.melScale)
                            return idx >= 0 ? idx : 0
                        }
                        onActivated: if (widgetRef) widgetRef.setMelScale(melScales[currentIndex])
                    }
                }
        }


        SectionBox
        {
            sectionLabel: qsTr("Mel Processing")

            sectionContents:
                GridLayout
                {
                    width: parent.width
                    columns: 2
                    columnSpacing: 6
                    rowSpacing: 4

                    RobotoText { height: gridItemsHeight; label: qsTr("Post-processing"); tooltipText: qsTr("Master toggle for the LedFx-style mel post-processor (power scaling, Gaussian smoothing, AGC, common/diff filters). When off, mel arrays are passed through raw.") }
                    CustomCheckBox
                    {
                        Layout.fillWidth: true
                        enabled: widgetRef !== null
                        checked: widgetRef ? widgetRef.melPostEnabled : true
                        onToggled: if (widgetRef) widgetRef.setMelPostEnabled(checked)
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Power factor"); tooltipText: qsTr("Exponent applied to mel magnitudes before AGC (LedFx mel_power). 1.0 = linear, 2.0 = squared (default), >2 = more peaky.") }
                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        from: 50; to: 500; suffix: "%"
                        enabled: widgetRef !== null
                        value: widgetRef ? Math.round(widgetRef.melPowerFactor * 100) : 200
                        onValueModified: if (widgetRef) widgetRef.setMelPowerFactor(value / 100)
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Gaussian sigma"); tooltipText: qsTr("Width (in bands) of the Gaussian kernel used to smooth mel across bins (LedFx gaussian_sigma_size). Higher = smoother spectrum.") }
                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        from: 10; to: 1000; suffix: "%"
                        enabled: widgetRef !== null
                        value: widgetRef ? Math.round(widgetRef.melGaussianSigma * 100) : 100
                        onValueModified: if (widgetRef) widgetRef.setMelGaussianSigma(value / 100)
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Smoothing decay"); tooltipText: qsTr("LedFx mel_smoothing alpha_decay. Higher = faster fall.") }
                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        from: 0; to: 100; suffix: "%"
                        enabled: widgetRef !== null
                        value: widgetRef ? Math.round(widgetRef.melSmoothDecay * 100) : 70
                        onValueModified: if (widgetRef) widgetRef.setMelSmoothDecay(value / 100)
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Smoothing rise"); tooltipText: qsTr("LedFx mel_smoothing alpha_rise. Higher = faster attack.") }
                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        from: 0; to: 100; suffix: "%"
                        enabled: widgetRef !== null
                        value: widgetRef ? Math.round(widgetRef.melSmoothRise * 100) : 99
                        onValueModified: if (widgetRef) widgetRef.setMelSmoothRise(value / 100)
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Common decay"); tooltipText: qsTr("LedFx common_filter alpha_decay.") }
                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        from: 0; to: 100; suffix: "%"
                        enabled: widgetRef !== null
                        value: widgetRef ? Math.round(widgetRef.melCommonDecay * 100) : 99
                        onValueModified: if (widgetRef) widgetRef.setMelCommonDecay(value / 100)
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Common rise"); tooltipText: qsTr("LedFx common_filter alpha_rise.") }
                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        from: 0; to: 100; suffix: "%"
                        enabled: widgetRef !== null
                        value: widgetRef ? Math.round(widgetRef.melCommonRise * 100) : 1
                        onValueModified: if (widgetRef) widgetRef.setMelCommonRise(value / 100)
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Diff decay"); tooltipText: qsTr("LedFx diff_filter alpha_decay.") }
                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        from: 0; to: 100; suffix: "%"
                        enabled: widgetRef !== null
                        value: widgetRef ? Math.round(widgetRef.melDiffDecay * 100) : 15
                        onValueModified: if (widgetRef) widgetRef.setMelDiffDecay(value / 100)
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Diff rise"); tooltipText: qsTr("LedFx diff_filter alpha_rise.") }
                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        from: 0; to: 100; suffix: "%"
                        enabled: widgetRef !== null
                        value: widgetRef ? Math.round(widgetRef.melDiffRise * 100) : 99
                        onValueModified: if (widgetRef) widgetRef.setMelDiffRise(value / 100)
                    }
                }
        }


        SectionBox
        {
            sectionLabel: qsTr("Per-Bank AGC (Low / Mid / High)")

            sectionContents:
                GridLayout
                {
                    width: parent.width
                    columns: 2
                    columnSpacing: 6
                    rowSpacing: 4

                    // Low bank
                    RobotoText { height: gridItemsHeight; label: qsTr("Low decay"); tooltipText: qsTr("AGC alpha_decay for the LOW mel bank (LedFx melbank.py:375). Higher = AGC tracks downward faster.") }
                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        from: 0; to: 100; suffix: "%"
                        enabled: widgetRef !== null
                        value: widgetRef ? Math.round(widgetRef.melLowAgcDecay * 100) : 1
                        onValueModified: if (widgetRef) widgetRef.setMelLowAgcDecay(value / 100)
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Low rise"); tooltipText: qsTr("AGC alpha_rise for the LOW mel bank. Higher = AGC tracks upward faster.") }
                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        from: 0; to: 100; suffix: "%"
                        enabled: widgetRef !== null
                        value: widgetRef ? Math.round(widgetRef.melLowAgcRise * 100) : 99
                        onValueModified: if (widgetRef) widgetRef.setMelLowAgcRise(value / 100)
                    }

                    // Mid bank
                    RobotoText { height: gridItemsHeight; label: qsTr("Mid decay"); tooltipText: qsTr("AGC alpha_decay for the MID mel bank.") }
                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        from: 0; to: 100; suffix: "%"
                        enabled: widgetRef !== null
                        value: widgetRef ? Math.round(widgetRef.melMidAgcDecay * 100) : 1
                        onValueModified: if (widgetRef) widgetRef.setMelMidAgcDecay(value / 100)
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Mid rise"); tooltipText: qsTr("AGC alpha_rise for the MID mel bank.") }
                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        from: 0; to: 100; suffix: "%"
                        enabled: widgetRef !== null
                        value: widgetRef ? Math.round(widgetRef.melMidAgcRise * 100) : 99
                        onValueModified: if (widgetRef) widgetRef.setMelMidAgcRise(value / 100)
                    }

                    // High bank
                    RobotoText { height: gridItemsHeight; label: qsTr("High decay"); tooltipText: qsTr("AGC alpha_decay for the HIGH mel bank.") }
                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        from: 0; to: 100; suffix: "%"
                        enabled: widgetRef !== null
                        value: widgetRef ? Math.round(widgetRef.melHighAgcDecay * 100) : 1
                        onValueModified: if (widgetRef) widgetRef.setMelHighAgcDecay(value / 100)
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("High rise"); tooltipText: qsTr("AGC alpha_rise for the HIGH mel bank.") }
                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        from: 0; to: 100; suffix: "%"
                        enabled: widgetRef !== null
                        value: widgetRef ? Math.round(widgetRef.melHighAgcRise * 100) : 99
                        onValueModified: if (widgetRef) widgetRef.setMelHighAgcRise(value / 100)
                    }
                }
        }


        // -----------------------------------------------------------------
        // Per-bank MelPostConfig (non-AGC). Three SectionBoxes mirroring the
        // master "Mel Processing" panel, one per bank. Defaults match
        // MelPostConfig in audiochannelconfig.h:70-94 (LedFx melbank.py:374).
        // Component is reused so each bank renders identically.
        // -----------------------------------------------------------------

        Component
        {
            id: melBankPostEditor
            GridLayout
            {
                width: parent.width
                columns: 2
                columnSpacing: 6
                rowSpacing: 4

                // bankLabel/getEnabled/setEnabled etc. wired by the loader below.
                property string bankLabel: ""
                property var getEnabled
                property var setEnabled
                property var getPower
                property var setPower
                property var getSigma
                property var setSigma
                property var getSmoothDecay
                property var setSmoothDecay
                property var getSmoothRise
                property var setSmoothRise
                property var getCommonDecay
                property var setCommonDecay
                property var getCommonRise
                property var setCommonRise
                property var getDiffDecay
                property var setDiffDecay
                property var getDiffRise
                property var setDiffRise

                RobotoText { height: gridItemsHeight; label: qsTr("Enabled"); tooltipText: qsTr("Master toggle for this bank's post-processor chain.") }
                CustomCheckBox
                {
                    Layout.fillWidth: true
                    enabled: widgetRef !== null
                    checked: widgetRef ? getEnabled() : true
                    onToggled: if (widgetRef) setEnabled(checked)
                }

                RobotoText { height: gridItemsHeight; label: qsTr("Power factor"); tooltipText: qsTr("Per-bank exponent on mel magnitudes (LedFx mel_power). 200% = squared.") }
                CustomSpinBox
                {
                    Layout.fillWidth: true
                    from: 50; to: 500; suffix: "%"
                    enabled: widgetRef !== null
                    value: widgetRef ? Math.round(getPower() * 100) : 200
                    onValueModified: if (widgetRef) setPower(value / 100)
                }

                RobotoText { height: gridItemsHeight; label: qsTr("Gaussian sigma"); tooltipText: qsTr("Per-bank Gaussian smoothing kernel width (LedFx gaussian_sigma_size).") }
                CustomSpinBox
                {
                    Layout.fillWidth: true
                    from: 10; to: 1000; suffix: "%"
                    enabled: widgetRef !== null
                    value: widgetRef ? Math.round(getSigma() * 100) : 100
                    onValueModified: if (widgetRef) setSigma(value / 100)
                }

                RobotoText { height: gridItemsHeight; label: qsTr("Smoothing decay"); tooltipText: qsTr("LedFx mel_smoothing alpha_decay for this bank.") }
                CustomSpinBox
                {
                    Layout.fillWidth: true
                    from: 0; to: 100; suffix: "%"
                    enabled: widgetRef !== null
                    value: widgetRef ? Math.round(getSmoothDecay() * 100) : 70
                    onValueModified: if (widgetRef) setSmoothDecay(value / 100)
                }

                RobotoText { height: gridItemsHeight; label: qsTr("Smoothing rise"); tooltipText: qsTr("LedFx mel_smoothing alpha_rise for this bank.") }
                CustomSpinBox
                {
                    Layout.fillWidth: true
                    from: 0; to: 100; suffix: "%"
                    enabled: widgetRef !== null
                    value: widgetRef ? Math.round(getSmoothRise() * 100) : 99
                    onValueModified: if (widgetRef) setSmoothRise(value / 100)
                }

                RobotoText { height: gridItemsHeight; label: qsTr("Common decay"); tooltipText: qsTr("LedFx common_filter alpha_decay for this bank.") }
                CustomSpinBox
                {
                    Layout.fillWidth: true
                    from: 0; to: 100; suffix: "%"
                    enabled: widgetRef !== null
                    value: widgetRef ? Math.round(getCommonDecay() * 100) : 99
                    onValueModified: if (widgetRef) setCommonDecay(value / 100)
                }

                RobotoText { height: gridItemsHeight; label: qsTr("Common rise"); tooltipText: qsTr("LedFx common_filter alpha_rise for this bank.") }
                CustomSpinBox
                {
                    Layout.fillWidth: true
                    from: 0; to: 100; suffix: "%"
                    enabled: widgetRef !== null
                    value: widgetRef ? Math.round(getCommonRise() * 100) : 1
                    onValueModified: if (widgetRef) setCommonRise(value / 100)
                }

                RobotoText { height: gridItemsHeight; label: qsTr("Diff decay"); tooltipText: qsTr("LedFx diff_filter alpha_decay for this bank.") }
                CustomSpinBox
                {
                    Layout.fillWidth: true
                    from: 0; to: 100; suffix: "%"
                    enabled: widgetRef !== null
                    value: widgetRef ? Math.round(getDiffDecay() * 100) : 15
                    onValueModified: if (widgetRef) setDiffDecay(value / 100)
                }

                RobotoText { height: gridItemsHeight; label: qsTr("Diff rise"); tooltipText: qsTr("LedFx diff_filter alpha_rise for this bank.") }
                CustomSpinBox
                {
                    Layout.fillWidth: true
                    from: 0; to: 100; suffix: "%"
                    enabled: widgetRef !== null
                    value: widgetRef ? Math.round(getDiffRise() * 100) : 99
                    onValueModified: if (widgetRef) setDiffRise(value / 100)
                }
            }
        }

        SectionBox
        {
            sectionLabel: qsTr("Per-Bank Mel Post — Low")
            sectionContents: Loader
            {
                width: parent.width
                sourceComponent: melBankPostEditor
                onLoaded: {
                    item.bankLabel = "low"
                    item.getEnabled = function() { return widgetRef.melLowEnabled }
                    item.setEnabled = function(v) { widgetRef.setMelLowEnabled(v) }
                    item.getPower = function() { return widgetRef.melLowPowerFactor }
                    item.setPower = function(v) { widgetRef.setMelLowPowerFactor(v) }
                    item.getSigma = function() { return widgetRef.melLowGaussianSigma }
                    item.setSigma = function(v) { widgetRef.setMelLowGaussianSigma(v) }
                    item.getSmoothDecay = function() { return widgetRef.melLowSmoothDecay }
                    item.setSmoothDecay = function(v) { widgetRef.setMelLowSmoothDecay(v) }
                    item.getSmoothRise = function() { return widgetRef.melLowSmoothRise }
                    item.setSmoothRise = function(v) { widgetRef.setMelLowSmoothRise(v) }
                    item.getCommonDecay = function() { return widgetRef.melLowCommonDecay }
                    item.setCommonDecay = function(v) { widgetRef.setMelLowCommonDecay(v) }
                    item.getCommonRise = function() { return widgetRef.melLowCommonRise }
                    item.setCommonRise = function(v) { widgetRef.setMelLowCommonRise(v) }
                    item.getDiffDecay = function() { return widgetRef.melLowDiffDecay }
                    item.setDiffDecay = function(v) { widgetRef.setMelLowDiffDecay(v) }
                    item.getDiffRise = function() { return widgetRef.melLowDiffRise }
                    item.setDiffRise = function(v) { widgetRef.setMelLowDiffRise(v) }
                }
            }
        }

        SectionBox
        {
            sectionLabel: qsTr("Per-Bank Mel Post — Mid")
            sectionContents: Loader
            {
                width: parent.width
                sourceComponent: melBankPostEditor
                onLoaded: {
                    item.bankLabel = "mid"
                    item.getEnabled = function() { return widgetRef.melMidEnabled }
                    item.setEnabled = function(v) { widgetRef.setMelMidEnabled(v) }
                    item.getPower = function() { return widgetRef.melMidPowerFactor }
                    item.setPower = function(v) { widgetRef.setMelMidPowerFactor(v) }
                    item.getSigma = function() { return widgetRef.melMidGaussianSigma }
                    item.setSigma = function(v) { widgetRef.setMelMidGaussianSigma(v) }
                    item.getSmoothDecay = function() { return widgetRef.melMidSmoothDecay }
                    item.setSmoothDecay = function(v) { widgetRef.setMelMidSmoothDecay(v) }
                    item.getSmoothRise = function() { return widgetRef.melMidSmoothRise }
                    item.setSmoothRise = function(v) { widgetRef.setMelMidSmoothRise(v) }
                    item.getCommonDecay = function() { return widgetRef.melMidCommonDecay }
                    item.setCommonDecay = function(v) { widgetRef.setMelMidCommonDecay(v) }
                    item.getCommonRise = function() { return widgetRef.melMidCommonRise }
                    item.setCommonRise = function(v) { widgetRef.setMelMidCommonRise(v) }
                    item.getDiffDecay = function() { return widgetRef.melMidDiffDecay }
                    item.setDiffDecay = function(v) { widgetRef.setMelMidDiffDecay(v) }
                    item.getDiffRise = function() { return widgetRef.melMidDiffRise }
                    item.setDiffRise = function(v) { widgetRef.setMelMidDiffRise(v) }
                }
            }
        }

        SectionBox
        {
            sectionLabel: qsTr("Per-Bank Mel Post — High")
            sectionContents: Loader
            {
                width: parent.width
                sourceComponent: melBankPostEditor
                onLoaded: {
                    item.bankLabel = "high"
                    item.getEnabled = function() { return widgetRef.melHighEnabled }
                    item.setEnabled = function(v) { widgetRef.setMelHighEnabled(v) }
                    item.getPower = function() { return widgetRef.melHighPowerFactor }
                    item.setPower = function(v) { widgetRef.setMelHighPowerFactor(v) }
                    item.getSigma = function() { return widgetRef.melHighGaussianSigma }
                    item.setSigma = function(v) { widgetRef.setMelHighGaussianSigma(v) }
                    item.getSmoothDecay = function() { return widgetRef.melHighSmoothDecay }
                    item.setSmoothDecay = function(v) { widgetRef.setMelHighSmoothDecay(v) }
                    item.getSmoothRise = function() { return widgetRef.melHighSmoothRise }
                    item.setSmoothRise = function(v) { widgetRef.setMelHighSmoothRise(v) }
                    item.getCommonDecay = function() { return widgetRef.melHighCommonDecay }
                    item.setCommonDecay = function(v) { widgetRef.setMelHighCommonDecay(v) }
                    item.getCommonRise = function() { return widgetRef.melHighCommonRise }
                    item.setCommonRise = function(v) { widgetRef.setMelHighCommonRise(v) }
                    item.getDiffDecay = function() { return widgetRef.melHighDiffDecay }
                    item.setDiffDecay = function(v) { widgetRef.setMelHighDiffDecay(v) }
                    item.getDiffRise = function() { return widgetRef.melHighDiffRise }
                    item.setDiffRise = function(v) { widgetRef.setMelHighDiffRise(v) }
                }
            }
        }

        // -----------------------------------------------------------------
        // Mel Bank Ranges. Each bank's setter takes (minHz, maxHz, bands)
        // atomically — we read the live triplet from the widget so editing
        // any single field preserves the other two.
        // -----------------------------------------------------------------
        SectionBox
        {
            sectionLabel: qsTr("Mel Bank Ranges (Low / Mid / High)")
            sectionContents:
                GridLayout
                {
                    width: parent.width
                    columns: 4
                    columnSpacing: 6
                    rowSpacing: 4

                    RobotoText { height: gridItemsHeight; label: qsTr("Low") }
                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        from: 0; to: 24000; suffix: " Hz"
                        enabled: widgetRef !== null
                        value: widgetRef ? Math.round(widgetRef.melLowMinHz) : 0
                        onValueModified: if (widgetRef) widgetRef.setMelBankLow(value, widgetRef.melLowMaxHz, widgetRef.melLowBands)
                    }
                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        from: 1; to: 24000; suffix: " Hz"
                        enabled: widgetRef !== null
                        value: widgetRef ? Math.round(widgetRef.melLowMaxHz) : 350
                        onValueModified: if (widgetRef) widgetRef.setMelBankLow(widgetRef.melLowMinHz, value, widgetRef.melLowBands)
                    }
                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        from: 4; to: 256; suffix: " bands"
                        enabled: widgetRef !== null
                        value: widgetRef ? widgetRef.melLowBands : 24
                        onValueModified: if (widgetRef) widgetRef.setMelBankLow(widgetRef.melLowMinHz, widgetRef.melLowMaxHz, value)
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Mid") }
                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        from: 0; to: 24000; suffix: " Hz"
                        enabled: widgetRef !== null
                        value: widgetRef ? Math.round(widgetRef.melMidMinHz) : 20
                        onValueModified: if (widgetRef) widgetRef.setMelBankMid(value, widgetRef.melMidMaxHz, widgetRef.melMidBands)
                    }
                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        from: 1; to: 24000; suffix: " Hz"
                        enabled: widgetRef !== null
                        value: widgetRef ? Math.round(widgetRef.melMidMaxHz) : 2000
                        onValueModified: if (widgetRef) widgetRef.setMelBankMid(widgetRef.melMidMinHz, value, widgetRef.melMidBands)
                    }
                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        from: 4; to: 256; suffix: " bands"
                        enabled: widgetRef !== null
                        value: widgetRef ? widgetRef.melMidBands : 24
                        onValueModified: if (widgetRef) widgetRef.setMelBankMid(widgetRef.melMidMinHz, widgetRef.melMidMaxHz, value)
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("High") }
                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        from: 0; to: 24000; suffix: " Hz"
                        enabled: widgetRef !== null
                        value: widgetRef ? Math.round(widgetRef.melHighMinHz) : 20
                        onValueModified: if (widgetRef) widgetRef.setMelBankHigh(value, widgetRef.melHighMaxHz, widgetRef.melHighBands)
                    }
                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        from: 1; to: 24000; suffix: " Hz"
                        enabled: widgetRef !== null
                        value: widgetRef ? Math.round(widgetRef.melHighMaxHz) : 15000
                        onValueModified: if (widgetRef) widgetRef.setMelBankHigh(widgetRef.melHighMinHz, value, widgetRef.melHighBands)
                    }
                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        from: 4; to: 256; suffix: " bands"
                        enabled: widgetRef !== null
                        value: widgetRef ? widgetRef.melHighBands : 24
                        onValueModified: if (widgetRef) widgetRef.setMelBankHigh(widgetRef.melHighMinHz, widgetRef.melHighMaxHz, value)
                    }
                }
        }


        SectionBox
        {
            sectionLabel: qsTr("aubio: Onset Detection")

            sectionContents:
                Column
                {
                    width: parent.width
                    spacing: 4

                    GridLayout
                    {
                        width: parent.width
                        columns: 2
                        columnSpacing: 6
                        rowSpacing: 4

                        // Global onset parameters (threshold/silence/min interval/delay/
                        // adaptive whitening/compression) were removed: aubio exposes
                        // them only via per-method overrides now. Use the per-method
                        // override editor below to tune each detector individually.
                    }

                    RobotoText
                    {
                        width: parent.width
                        height: gridItemsHeight
                        label: qsTr("Primary method")
                        tooltipText: qsTr("Primary onset method used by aubio for callback-style onset detection. Independent of the per-method enable flags below, which control which descriptors the visualizer plots.")
                    }
                    CustomComboBox
                    {
                        width: parent.width
                        height: gridItemsHeight
                        property var onsetMethods: ["energy","hfc","complex","phase","wphase","specdiff","kl","mkl","specflux"]
                        model: onsetMethods
                        enabled: widgetRef !== null
                        currentIndex: widgetRef ? widgetRef.onsetMethodIndex : 8
                        onActivated: if (widgetRef) widgetRef.setOnsetMethodIndex(currentIndex)
                    }

                    RobotoText
                    {
                        width: parent.width
                        height: gridItemsHeight
                        label: qsTr("Methods")
                        tooltipText: qsTr("Enable individual onset detection functions. The widget combines all enabled methods.")
                    }

                    GridLayout
                    {
                        width: parent.width
                        columns: 3
                        columnSpacing: 6
                        rowSpacing: 4

                        Repeater
                        {
                            model: ["energy","hfc","complex","phase","wphase","specdiff","kl","mkl","specflux"]

                            CustomCheckBox
                            {
                                Layout.fillWidth: true
                                text: modelData
                                enabled: widgetRef !== null
                                checked: widgetRef && widgetRef.onsetMethodsEnabled ? widgetRef.onsetMethodsEnabled[index] === true : false
                                onToggled: if (widgetRef) widgetRef.setOnsetMethodEnabled(index, checked)
                            }
                        }
                    }
                }
        }


        SectionBox
        {
            sectionLabel: qsTr("aubio: Tempo / Beat")

            sectionContents:
                GridLayout
                {
                    width: parent.width
                    columns: 2
                    columnSpacing: 6
                    rowSpacing: 4

                    RobotoText { height: gridItemsHeight; label: qsTr("Threshold"); tooltipText: qsTr("Peak-picking threshold for the beat tracker. Default 0.3.") }
                    RowLayout
                    {
                        Layout.fillWidth: true
                        spacing: 6
                        Slider
                        {
                            Layout.fillWidth: true
                            from: 0; to: 1; stepSize: 0.01
                            enabled: widgetRef !== null
                            value: widgetRef ? widgetRef.tempoThreshold : 0.3
                            onPressedChanged: if (!pressed && widgetRef) widgetRef.setTempoThreshold(value)
                        }
                        RobotoText
                        {
                            Layout.preferredWidth: UISettings.bigItemHeight
                            height: gridItemsHeight
                            label: widgetRef ? widgetRef.tempoThreshold.toFixed(2) : "--"
                        }
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Silence"); tooltipText: qsTr("dBFS gate. Beat detection is suppressed when input level is below this.") }
                    RowLayout
                    {
                        Layout.fillWidth: true
                        spacing: 6
                        Slider
                        {
                            Layout.fillWidth: true
                            from: -90; to: 0; stepSize: 1
                            enabled: widgetRef !== null
                            value: widgetRef ? widgetRef.tempoSilenceDb : -70
                            onPressedChanged: if (!pressed && widgetRef) widgetRef.setTempoSilenceDb(value)
                        }
                        RobotoText
                        {
                            Layout.preferredWidth: UISettings.bigItemHeight
                            height: gridItemsHeight
                            label: widgetRef ? Math.round(widgetRef.tempoSilenceDb) + " dB" : "--"
                        }
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Tatum subdivision"); tooltipText: qsTr("Tatums (sub-beats) per beat. 4 = 16ths, 2 = 8ths, 3 = triplets.") }
                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        from: 1; to: 16
                        enabled: widgetRef !== null
                        value: widgetRef ? widgetRef.tatumSubdivision : 4
                        onValueModified: if (widgetRef) widgetRef.setTatumSubdivision(value)
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Delay"); tooltipText: qsTr("Post-detection delay (ms) for beats. Negative values report earlier.") }
                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        from: -500; to: 500; suffix: " ms"
                        enabled: widgetRef !== null
                        value: widgetRef ? Math.round(widgetRef.tempoDelayMs) : 0
                        onValueModified: if (widgetRef) widgetRef.setTempoDelayMs(value)
                    }
                    RobotoText { height: gridItemsHeight; label: qsTr("Beats per bar"); tooltipText: qsTr("Number of beats per musical bar. Drives the bar-phase dot row in the live widget.") }
                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        from: 1; to: 8
                        enabled: widgetRef !== null
                        value: widgetRef ? widgetRef.beatsPerBar : 4
                        onValueModified: if (widgetRef) widgetRef.setBeatsPerBar(value)
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Pre-emphasis"); tooltipText: qsTr("LedFx-style high-pass pre-emphasis on the tempo input. Boosts transients to improve beat tracking on bass-heavy material.") }
                    CustomCheckBox
                    {
                        Layout.fillWidth: true
                        enabled: widgetRef !== null
                        checked: widgetRef ? widgetRef.preEmphasisEnabled : true
                        onToggled: if (widgetRef) widgetRef.setPreEmphasisEnabled(checked)
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Method"); tooltipText: qsTr("aubio_tempo_new method string. Currently aubio only ships \"default\" — the field is open-ended for future aubio versions.") }
                    CustomComboBox
                    {
                        Layout.fillWidth: true
                        enabled: widgetRef !== null
                        model: ["default"]
                        currentIndex: widgetRef ? Math.max(0, model.indexOf(widgetRef.tempoMethod)) : 0
                        onActivated: if (widgetRef) widgetRef.setTempoMethod(currentText)
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Coast beats"); tooltipText: qsTr("How many beat-periods of silence before BPM starts decaying. Higher = BPM holds longer when music stops.") }
                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        from: 0; to: 64
                        enabled: widgetRef !== null
                        value: widgetRef ? Math.round(widgetRef.coastBeats) : 4
                        onValueModified: if (widgetRef) widgetRef.setCoastBeats(value)
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Decay half-life"); tooltipText: qsTr("Once decay starts, BPM halves every N beats. Lower = faster decay toward the target BPM.") }
                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        from: 5; to: 3200; suffix: "%"
                        enabled: widgetRef !== null
                        value: widgetRef ? Math.round(widgetRef.tempoDecayHalfLifeBeats * 100) : 50
                        onValueModified: if (widgetRef) widgetRef.setTempoDecayHalfLifeBeats(value / 100)
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Decay target BPM"); tooltipText: qsTr("Floor BPM that the tempo decays toward during silence. Must be ≥ 1.") }
                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        from: 1; to: 300; suffix: " BPM"
                        enabled: widgetRef !== null
                        value: widgetRef ? Math.round(widgetRef.tempoDecayTargetBpm) : 1
                        onValueModified: if (widgetRef) widgetRef.setTempoDecayTargetBpm(value)
                    }
                }
        }


        SectionBox
        {
            sectionLabel: qsTr("aubio: Pitch Detection")

            sectionContents:
                GridLayout
                {
                    width: parent.width
                    columns: 2
                    columnSpacing: 6
                    rowSpacing: 4

                    RobotoText { height: gridItemsHeight; label: qsTr("Method"); tooltipText: qsTr("Pitch detection algorithm. yinfft is the most accurate default.") }
                    CustomComboBox
                    {
                        Layout.fillWidth: true
                        height: gridItemsHeight
                        property var methods: ["yinfft", "yin", "yinfast", "fcomb", "mcomb", "schmitt"]
                        model: methods
                        currentIndex: {
                            if (!widgetRef) return 0
                            var idx = methods.indexOf(widgetRef.pitchMethod)
                            return idx >= 0 ? idx : 0
                        }
                        onActivated: if (widgetRef) widgetRef.setPitchMethod(methods[currentIndex])
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Silence"); tooltipText: qsTr("Below this dBFS level the pitch detector reports no pitch.") }
                    RowLayout
                    {
                        Layout.fillWidth: true
                        spacing: 6
                        Slider
                        {
                            Layout.fillWidth: true
                            from: -90; to: 0; stepSize: 1
                            enabled: widgetRef !== null
                            value: widgetRef ? widgetRef.pitchSilenceDb : -40
                            onPressedChanged: if (!pressed && widgetRef) widgetRef.setPitchSilenceDb(value)
                        }
                        RobotoText
                        {
                            Layout.preferredWidth: UISettings.bigItemHeight
                            height: gridItemsHeight
                            label: widgetRef ? Math.round(widgetRef.pitchSilenceDb) + " dB" : "--"
                        }
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Tolerance"); tooltipText: qsTr("Confidence threshold for accepting a pitch estimate (0–1). Higher = stricter.") }
                    RowLayout
                    {
                        Layout.fillWidth: true
                        spacing: 6
                        Slider
                        {
                            Layout.fillWidth: true
                            from: 0; to: 1; stepSize: 0.01
                            enabled: widgetRef !== null
                            value: widgetRef ? widgetRef.pitchTolerance : 0.7
                            onPressedChanged: if (!pressed && widgetRef) widgetRef.setPitchTolerance(value)
                        }
                        RobotoText
                        {
                            Layout.preferredWidth: UISettings.bigItemHeight
                            height: gridItemsHeight
                            label: widgetRef ? widgetRef.pitchTolerance.toFixed(2) : "--"
                        }
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Display unit"); tooltipText: qsTr("Display unit for the live pitch readout. aubio always outputs Hz; this only affects the widget display.") }
                    CustomComboBox
                    {
                        Layout.fillWidth: true
                        height: gridItemsHeight
                        property var units: ["Hz", "midi", "cent", "bin"]
                        model: units
                        currentIndex: {
                            if (!widgetRef) return 0
                            var idx = units.indexOf(widgetRef.pitchUnit)
                            return idx >= 0 ? idx : 0
                        }
                        onActivated: if (widgetRef) widgetRef.setPitchUnit(units[currentIndex])
                    }
                }
        }


        SectionBox
        {
            sectionLabel: qsTr("aubio: Note Detection")

            sectionContents:
                GridLayout
                {
                    width: parent.width
                    columns: 2
                    columnSpacing: 6
                    rowSpacing: 4

                    RobotoText { height: gridItemsHeight; label: qsTr("Silence"); tooltipText: qsTr("dBFS gate. Notes are not emitted when input level is below this.") }
                    RowLayout
                    {
                        Layout.fillWidth: true
                        spacing: 6
                        Slider
                        {
                            Layout.fillWidth: true
                            from: -90; to: 0; stepSize: 1
                            enabled: widgetRef !== null
                            value: widgetRef ? widgetRef.noteSilenceDb : -70
                            onPressedChanged: if (!pressed && widgetRef) widgetRef.setNoteSilenceDb(value)
                        }
                        RobotoText
                        {
                            Layout.preferredWidth: UISettings.bigItemHeight
                            height: gridItemsHeight
                            label: widgetRef ? Math.round(widgetRef.noteSilenceDb) + " dB" : "--"
                        }
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Min interval"); tooltipText: qsTr("Minimum time (ms) between two note-on events.") }
                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        from: 0; to: 500; suffix: " ms"
                        enabled: widgetRef !== null
                        value: widgetRef ? Math.round(widgetRef.noteMinIntervalMs) : 50
                        onValueModified: if (widgetRef) widgetRef.setNoteMinIntervalMs(value)
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Release drop"); tooltipText: qsTr("Drop in dB below the note's peak that triggers a note-off.") }
                    RowLayout
                    {
                        Layout.fillWidth: true
                        spacing: 6
                        Slider
                        {
                            Layout.fillWidth: true
                            from: 0; to: 60; stepSize: 1
                            enabled: widgetRef !== null
                            value: widgetRef ? widgetRef.noteReleaseDropDb : 10
                            onPressedChanged: if (!pressed && widgetRef) widgetRef.setNoteReleaseDropDb(value)
                        }
                        RobotoText
                        {
                            Layout.preferredWidth: UISettings.bigItemHeight
                            height: gridItemsHeight
                            label: widgetRef ? Math.round(widgetRef.noteReleaseDropDb) + " dB" : "--"
                        }
                    }
                }
        }


        SectionBox
        {
            sectionLabel: qsTr("aubio: MFCC")

            sectionContents:
                Column
                {
                    width: parent.width
                    spacing: 6

                    RobotoText
                    {
                        width: parent.width
                        height: gridItemsHeight
                        label: qsTr("About MFCC")
                        tooltipText: qsTr("Mel-Frequency Cepstral Coefficients describe the timbral envelope (shape of the spectrum). Useful as inputs to texture / scene selection — different MFCC fingerprints can switch palettes.")
                    }

                GridLayout
                {
                    width: parent.width
                    columns: 2
                    columnSpacing: 6
                    rowSpacing: 4

                    RobotoText { height: gridItemsHeight; label: qsTr("Power"); tooltipText: qsTr("Exponent applied to magnitudes before the mel transform. 1 = magnitude, 2 = power.") }
                    RowLayout
                    {
                        Layout.fillWidth: true
                        spacing: 6
                        Slider
                        {
                            Layout.fillWidth: true
                            from: 0.5; to: 4.0; stepSize: 0.1
                            enabled: widgetRef !== null
                            value: widgetRef ? widgetRef.mfccPower : 1.0
                            onPressedChanged: if (!pressed && widgetRef) widgetRef.setMfccPower(value)
                        }
                        RobotoText
                        {
                            Layout.preferredWidth: UISettings.bigItemHeight
                            height: gridItemsHeight
                            label: widgetRef ? widgetRef.mfccPower.toFixed(1) : "--"
                        }
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Scale"); tooltipText: qsTr("Output scale on the mel-energy log. Affects coefficient magnitudes; tune to keep coefficients in a usable range.") }
                    RowLayout
                    {
                        Layout.fillWidth: true
                        spacing: 6
                        Slider
                        {
                            Layout.fillWidth: true
                            from: 0.1; to: 10.0; stepSize: 0.1
                            enabled: widgetRef !== null
                            value: widgetRef ? widgetRef.mfccScale : 1.0
                            onPressedChanged: if (!pressed && widgetRef) widgetRef.setMfccScale(value)
                        }
                        RobotoText
                        {
                            Layout.preferredWidth: UISettings.bigItemHeight
                            height: gridItemsHeight
                            label: widgetRef ? widgetRef.mfccScale.toFixed(1) : "--"
                        }
                    }
                } // GridLayout
                } // Column
        }

        SectionBox
        {
            sectionLabel: qsTr("aubio: TSS (Transient/Steady)")

            sectionContents:
                Column
                {
                    width: parent.width
                    spacing: 6

                    RobotoText
                    {
                        width: parent.width
                        height: gridItemsHeight
                        label: qsTr("About TSS")
                        tooltipText: qsTr("Transient/Steady-State separation splits the signal into percussive (transient) vs sustained (steady) components. Useful for separating drum hits from pads.")
                    }

                GridLayout
                {
                    width: parent.width
                    columns: 2
                    columnSpacing: 6
                    rowSpacing: 4

                    RobotoText { height: gridItemsHeight; label: qsTr("Alpha"); tooltipText: qsTr("aubio_tss_set_alpha(): controls how aggressively the steady part is subtracted. Higher = more transient leakage suppression.") }
                    RowLayout
                    {
                        Layout.fillWidth: true
                        spacing: 6
                        Slider
                        {
                            Layout.fillWidth: true
                            from: 0; to: 10; stepSize: 0.1
                            enabled: widgetRef !== null
                            value: widgetRef ? widgetRef.tssAlpha : 3.0
                            onPressedChanged: if (!pressed && widgetRef) widgetRef.setTssAlpha(value)
                        }
                        RobotoText
                        {
                            Layout.preferredWidth: UISettings.bigItemHeight
                            height: gridItemsHeight
                            label: widgetRef ? widgetRef.tssAlpha.toFixed(1) : "--"
                        }
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Beta"); tooltipText: qsTr("aubio_tss_set_beta(): controls how aggressively the transient part is subtracted. Symmetric counterpart to alpha.") }
                    RowLayout
                    {
                        Layout.fillWidth: true
                        spacing: 6
                        Slider
                        {
                            Layout.fillWidth: true
                            from: 0; to: 10; stepSize: 0.1
                            enabled: widgetRef !== null
                            value: widgetRef ? widgetRef.tssBeta : 3.0
                            onPressedChanged: if (!pressed && widgetRef) widgetRef.setTssBeta(value)
                        }
                        RobotoText
                        {
                            Layout.preferredWidth: UISettings.bigItemHeight
                            height: gridItemsHeight
                            label: widgetRef ? widgetRef.tssBeta.toFixed(1) : "--"
                        }
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Threshold"); tooltipText: qsTr("aubio_tss_set_threshold(): magnitude threshold below which bins are forced into the steady part.") }
                    RowLayout
                    {
                        Layout.fillWidth: true
                        spacing: 6
                        Slider
                        {
                            Layout.fillWidth: true
                            from: 0; to: 1; stepSize: 0.01
                            enabled: widgetRef !== null
                            value: widgetRef ? widgetRef.tssThreshold : 0.25
                            onPressedChanged: if (!pressed && widgetRef) widgetRef.setTssThreshold(value)
                        }
                        RobotoText
                        {
                            Layout.preferredWidth: UISettings.bigItemHeight
                            height: gridItemsHeight
                            label: widgetRef ? widgetRef.tssThreshold.toFixed(2) : "--"
                        }
                    }
                } // GridLayout
                } // Column
        }
        SectionBox
        {
            sectionLabel: qsTr("Band Crossovers")

            sectionContents:
                GridLayout
                {
                    width: parent.width
                    columns: 2
                    columnSpacing: 6
                    rowSpacing: 4

                    RobotoText { height: gridItemsHeight; label: qsTr("Beat ≤"); tooltipText: qsTr("Upper Hz for the Beat (kick) slice. Diagnostic — canonical lows/mids/highs come from the engine pipeline.") }
                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        from: 20; to: 1000; suffix: " Hz"
                        enabled: widgetRef !== null
                        value: widgetRef ? Math.round(widgetRef.beatCutoffHz) : 100
                        onValueModified: if (widgetRef) widgetRef.setBeatCutoffHz(value)
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Bass ≤"); tooltipText: qsTr("Upper Hz for the Bass slice (diagnostic, must be > Beat).") }
                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        from: 30; to: 2000; suffix: " Hz"
                        enabled: widgetRef !== null
                        value: widgetRef ? Math.round(widgetRef.bassCutoffHz) : 250
                        onValueModified: if (widgetRef) widgetRef.setBassCutoffHz(value)
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Mids ≤"); tooltipText: qsTr("Upper Hz for the Mids slice (diagnostic, must be > Bass).") }
                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        from: 200; to: 8000; suffix: " Hz"
                        enabled: widgetRef !== null
                        value: widgetRef ? Math.round(widgetRef.midsCutoffHz) : 3000
                        onValueModified: if (widgetRef) widgetRef.setMidsCutoffHz(value)
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Highs ≤"); tooltipText: qsTr("Upper Hz for the Highs slice (diagnostic, must be > Mids).") }
                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        from: 1000; to: 24000; suffix: " Hz"
                        enabled: widgetRef !== null
                        value: widgetRef ? Math.round(widgetRef.highsCutoffHz) : 10000
                        onValueModified: if (widgetRef) widgetRef.setHighsCutoffHz(value)
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Beat decay"); tooltipText: qsTr("LedFx freq_power_filter alpha_decay for the Beat band. Higher = faster release.") }
                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        from: 0; to: 100; suffix: "%"
                        enabled: widgetRef !== null
                        value: widgetRef ? Math.round(widgetRef.freqPowerBeatDecay * 100) : 20
                        onValueModified: if (widgetRef) widgetRef.setFreqPowerBeatDecay(value / 100)
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Beat rise"); tooltipText: qsTr("LedFx freq_power_filter alpha_rise for the Beat band. Higher = faster attack.") }
                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        from: 0; to: 100; suffix: "%"
                        enabled: widgetRef !== null
                        value: widgetRef ? Math.round(widgetRef.freqPowerBeatRise * 100) : 97
                        onValueModified: if (widgetRef) widgetRef.setFreqPowerBeatRise(value / 100)
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Bass decay"); tooltipText: qsTr("LedFx freq_power_filter alpha_decay for the Bass band. Higher = faster release.") }
                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        from: 0; to: 100; suffix: "%"
                        enabled: widgetRef !== null
                        value: widgetRef ? Math.round(widgetRef.freqPowerBassDecay * 100) : 20
                        onValueModified: if (widgetRef) widgetRef.setFreqPowerBassDecay(value / 100)
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Bass rise"); tooltipText: qsTr("LedFx freq_power_filter alpha_rise for the Bass band. Higher = faster attack.") }
                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        from: 0; to: 100; suffix: "%"
                        enabled: widgetRef !== null
                        value: widgetRef ? Math.round(widgetRef.freqPowerBassRise * 100) : 97
                        onValueModified: if (widgetRef) widgetRef.setFreqPowerBassRise(value / 100)
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Mids decay"); tooltipText: qsTr("LedFx freq_power_filter alpha_decay for the Mids band. Higher = faster release.") }
                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        from: 0; to: 100; suffix: "%"
                        enabled: widgetRef !== null
                        value: widgetRef ? Math.round(widgetRef.freqPowerMidsDecay * 100) : 20
                        onValueModified: if (widgetRef) widgetRef.setFreqPowerMidsDecay(value / 100)
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Mids rise"); tooltipText: qsTr("LedFx freq_power_filter alpha_rise for the Mids band. Higher = faster attack.") }
                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        from: 0; to: 100; suffix: "%"
                        enabled: widgetRef !== null
                        value: widgetRef ? Math.round(widgetRef.freqPowerMidsRise * 100) : 97
                        onValueModified: if (widgetRef) widgetRef.setFreqPowerMidsRise(value / 100)
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("High decay"); tooltipText: qsTr("LedFx freq_power_filter alpha_decay for the High band. Higher = faster release.") }
                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        from: 0; to: 100; suffix: "%"
                        enabled: widgetRef !== null
                        value: widgetRef ? Math.round(widgetRef.freqPowerHighDecay * 100) : 20
                        onValueModified: if (widgetRef) widgetRef.setFreqPowerHighDecay(value / 100)
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("High rise"); tooltipText: qsTr("LedFx freq_power_filter alpha_rise for the High band. Higher = faster attack.") }
                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        from: 0; to: 100; suffix: "%"
                        enabled: widgetRef !== null
                        value: widgetRef ? Math.round(widgetRef.freqPowerHighRise * 100) : 97
                        onValueModified: if (widgetRef) widgetRef.setFreqPowerHighRise(value / 100)
                    }
                }
        }
        SectionBox
        {
            sectionLabel: qsTr("QLC+: Response — Envelope")

            sectionContents:
                Column
                {
                    width: parent.width
                    spacing: 4

                    RowLayout
                    {
                        width: parent.width
                        height: gridItemsHeight
                        spacing: 6

                        RobotoText
                        {
                            Layout.preferredWidth: UISettings.bigItemHeight
                            height: gridItemsHeight
                            label: qsTr("Attack")
                            tooltipText: qsTr("Attack time (ms) of the per-band envelope follower. Lower = snappier reaction; higher = smoother but laggier band-power signals driving widget triggers.")
                        }

                        Slider
                        {
                            Layout.fillWidth: true
                            from: 0
                            to: 500
                            stepSize: 1
                            enabled: widgetRef !== null
                            value: widgetRef ? widgetRef.envelopeAttack : 0
                            onMoved: if (widgetRef) widgetRef.setEnvelopeAttack(value)
                        }

                        CustomSpinBox
                        {
                            Layout.preferredWidth: UISettings.bigItemHeight
                            from: 0
                            to: 500
                            suffix: " ms"
                            enabled: widgetRef !== null
                            value: widgetRef ? Math.round(widgetRef.envelopeAttack) : 0
                            onValueModified: if (widgetRef) widgetRef.setEnvelopeAttack(value)
                        }
                    }

                    RowLayout
                    {
                        width: parent.width
                        height: gridItemsHeight
                        spacing: 6

                        RobotoText
                        {
                            Layout.preferredWidth: UISettings.bigItemHeight
                            height: gridItemsHeight
                            label: qsTr("Release")
                            tooltipText: qsTr("Release time (ms) of the per-band envelope follower. Higher = bands hang on longer after a hit; lower = bands fall off faster.")
                        }

                        Slider
                        {
                            Layout.fillWidth: true
                            from: 0
                            to: 2000
                            stepSize: 1
                            enabled: widgetRef !== null
                            value: widgetRef ? widgetRef.envelopeRelease : 0
                            onMoved: if (widgetRef) widgetRef.setEnvelopeRelease(value)
                        }

                        CustomSpinBox
                        {
                            Layout.preferredWidth: UISettings.bigItemHeight
                            from: 0
                            to: 2000
                            suffix: " ms"
                            enabled: widgetRef !== null
                            value: widgetRef ? Math.round(widgetRef.envelopeRelease) : 0
                            onValueModified: if (widgetRef) widgetRef.setEnvelopeRelease(value)
                        }
                    }
                }
        }
        SectionBox
        {
            sectionLabel: qsTr("QLC+: Noise Gate")

            sectionContents:
                GridLayout
                {
                    width: parent.width
                    columns: 2
                    columnSpacing: 6
                    rowSpacing: 4

                    RobotoText { height: gridItemsHeight; label: qsTr("Threshold"); tooltipText: qsTr("RMS level (in dBFS) below which the noise gate closes. Raise to ignore room noise; lower to let quieter audio through.") }
                    RowLayout
                    {
                        Layout.fillWidth: true
                        spacing: 6
                        Slider
                        {
                            Layout.fillWidth: true
                            from: -96; to: 0; stepSize: 1
                            enabled: widgetRef !== null
                            value: widgetRef ? widgetRef.noiseGateThreshold : -54
                            onPressedChanged: if (!pressed && widgetRef) widgetRef.setNoiseGateThreshold(value)
                        }
                        CustomSpinBox
                        {
                            Layout.preferredWidth: UISettings.bigItemHeight
                            from: -96; to: 0; suffix: " dB"
                            enabled: widgetRef !== null
                            value: widgetRef ? Math.round(widgetRef.noiseGateThreshold) : -54
                            onValueModified: if (widgetRef) widgetRef.setNoiseGateThreshold(value)
                        }
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Hold"); tooltipText: qsTr("How long (ms) the gate stays open after the signal drops below threshold. Prevents chatter on percussive material.") }
                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        from: 0; to: 2000; suffix: " ms"
                        enabled: widgetRef !== null
                        value: widgetRef ? Math.round(widgetRef.noiseGateHold) : 120
                        onValueModified: if (widgetRef) widgetRef.setNoiseGateHold(value)
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Input RMS"); tooltipText: qsTr("Live RMS reading of the post-gain input, in dBFS. Use this to set Threshold above your noise floor.") }
                    RowLayout
                    {
                        Layout.fillWidth: true
                        spacing: 6
                        Rectangle
                        {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 10
                            radius: 4
                            color: UISettings.bgStrong
                            border.width: 1; border.color: UISettings.bgLight
                            Rectangle
                            {
                                anchors.left: parent.left
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                width: parent.width * Math.max(0, Math.min(1, (96 + (widgetRef ? widgetRef.rmsDb : -96)) / 96))
                                radius: parent.radius
                                color: widgetRef && widgetRef.noiseGateOpen ? "#33cc66" : "#cc3333"
                            }
                        }
                        RobotoText
                        {
                            Layout.preferredWidth: UISettings.bigItemHeight
                            height: gridItemsHeight
                            label: widgetRef ? widgetRef.rmsDb.toFixed(0) + " dB" : "--"
                        }
                    }
                }
        }
        SectionBox
        {
            sectionLabel: qsTr("QLC+: Triggers")

            sectionContents:
                RowLayout
                {
                    width: parent.width
                    height: gridItemsHeight * 2
                    spacing: 8

                    Repeater
                    {
                        model: 3

                        Column
                        {
                            Layout.fillWidth: true
                            spacing: 4

                            Rectangle
                            {
                                width: 14
                                height: 14
                                radius: 7
                                anchors.horizontalCenter: parent.horizontalCenter
                                color: {
                                    var ts = widgetRef ? widgetRef.triggerStates[index] : null
                                    if (!ts) return "#555555"
                                    if (ts.fired) return "#33ff66"
                                    if (ts.active) return "#33cc66"
                                    if (ts.cooldownMs > 0) return "#cc9933"
                                    return "#555555"
                                }
                                border.width: 1
                                border.color: "#222222"
                            }

                            RobotoText
                            {
                                width: parent.width
                                height: gridItemsHeight
                                label: bankTriggerNames[index]
                                labelColor: bankTriggerColors[index]
                                fontSize: UISettings.textSizeSmall
                                textHAlign: Text.AlignHCenter
                            }
                        }
                    }

                    Column
                    {
                        Layout.fillWidth: true
                        spacing: 4

                        Rectangle
                        {
                            width: 14
                            height: 14
                            radius: 7
                            anchors.horizontalCenter: parent.horizontalCenter
                            color: widgetRef && widgetRef.beatActive ? "#33cc66" : "#555555"
                            border.width: 1
                            border.color: "#222222"
                            Behavior on color { ColorAnimation { duration: 80 } }
                        }

                        RobotoText
                        {
                            width: parent.width
                            height: gridItemsHeight
                            label: qsTr("Beat")
                            textHAlign: Text.AlignHCenter
                        }
                    }

                    GridLayout
                    {
                        Layout.fillWidth: true
                        columns: 5
                        columnSpacing: 6
                        rowSpacing: 4

                        // Header row
                        RobotoText { height: gridItemsHeight; label: qsTr("Band"); fontBold: true }
                        RobotoText
                        {
                            height: gridItemsHeight
                            label: qsTr("High")
                            textHAlign: Text.AlignHCenter
                            tooltipText: qsTr("Upper hysteresis threshold (% of band peak). Band must rise above this to fire a trigger.")
                        }
                        RobotoText
                        {
                            height: gridItemsHeight
                            label: qsTr("Low")
                            textHAlign: Text.AlignHCenter
                            tooltipText: qsTr("Lower hysteresis threshold (% of band peak). Band must fall below this before another trigger can fire.")
                        }
                        RobotoText
                        {
                            height: gridItemsHeight
                            label: qsTr("Hold")
                            textHAlign: Text.AlignHCenter
                            tooltipText: qsTr("Minimum time (ms) the trigger stays active once fired, even if the band drops back below threshold.")
                        }
                        RobotoText
                        {
                            height: gridItemsHeight
                            label: qsTr("Cooldown")
                            textHAlign: Text.AlignHCenter
                            tooltipText: qsTr("Minimum time (ms) between two consecutive triggers from the same band. Use to avoid retriggering on a sustained note.")
                        }

                        // Low band
                        RobotoText { height: gridItemsHeight; label: bankTriggerNames[0]; labelColor: bankTriggerColors[0] }
                        CustomSpinBox
                        {
                            Layout.fillWidth: true
                            from: 0; to: 100; suffix: "%"
                            enabled: widgetRef !== null
                            value: widgetRef ? Math.round(widgetRef.triggerLowHigh * 100) : 0
                            onValueModified: if (widgetRef) widgetRef.setTriggerLowHigh(value / 100)
                        }
                        CustomSpinBox
                        {
                            Layout.fillWidth: true
                            from: 0; to: 100; suffix: "%"
                            enabled: widgetRef !== null
                            value: widgetRef ? Math.round(widgetRef.triggerLowLow * 100) : 0
                            onValueModified: if (widgetRef) widgetRef.setTriggerLowLow(value / 100)
                        }
                        CustomSpinBox
                        {
                            Layout.fillWidth: true
                            from: 0; to: 1000; suffix: " ms"
                            enabled: widgetRef !== null
                            value: widgetRef ? Math.round(widgetRef.triggerLowHold) : 0
                            onValueModified: if (widgetRef) widgetRef.setTriggerLowHold(value)
                        }
                        CustomSpinBox
                        {
                            Layout.fillWidth: true
                            from: 0; to: 2000; suffix: " ms"
                            enabled: widgetRef !== null
                            value: widgetRef ? Math.round(widgetRef.triggerLowCooldown) : 0
                            onValueModified: if (widgetRef) widgetRef.setTriggerLowCooldown(value)
                        }

                        // Mid band
                        RobotoText { height: gridItemsHeight; label: bankTriggerNames[1]; labelColor: bankTriggerColors[1] }
                        CustomSpinBox
                        {
                            Layout.fillWidth: true
                            from: 0; to: 100; suffix: "%"
                            enabled: widgetRef !== null
                            value: widgetRef ? Math.round(widgetRef.triggerMidHigh * 100) : 0
                            onValueModified: if (widgetRef) widgetRef.setTriggerMidHigh(value / 100)
                        }
                        CustomSpinBox
                        {
                            Layout.fillWidth: true
                            from: 0; to: 100; suffix: "%"
                            enabled: widgetRef !== null
                            value: widgetRef ? Math.round(widgetRef.triggerMidLow * 100) : 0
                            onValueModified: if (widgetRef) widgetRef.setTriggerMidLow(value / 100)
                        }
                        CustomSpinBox
                        {
                            Layout.fillWidth: true
                            from: 0; to: 1000; suffix: " ms"
                            enabled: widgetRef !== null
                            value: widgetRef ? Math.round(widgetRef.triggerMidHold) : 0
                            onValueModified: if (widgetRef) widgetRef.setTriggerMidHold(value)
                        }
                        CustomSpinBox
                        {
                            Layout.fillWidth: true
                            from: 0; to: 2000; suffix: " ms"
                            enabled: widgetRef !== null
                            value: widgetRef ? Math.round(widgetRef.triggerMidCooldown) : 0
                            onValueModified: if (widgetRef) widgetRef.setTriggerMidCooldown(value)
                        }

                        // High band
                        RobotoText { height: gridItemsHeight; label: bankTriggerNames[2]; labelColor: bankTriggerColors[2] }
                        CustomSpinBox
                        {
                            Layout.fillWidth: true
                            from: 0; to: 100; suffix: "%"
                            enabled: widgetRef !== null
                            value: widgetRef ? Math.round(widgetRef.triggerHighHigh * 100) : 0
                            onValueModified: if (widgetRef) widgetRef.setTriggerHighHigh(value / 100)
                        }
                        CustomSpinBox
                        {
                            Layout.fillWidth: true
                            from: 0; to: 100; suffix: "%"
                            enabled: widgetRef !== null
                            value: widgetRef ? Math.round(widgetRef.triggerHighLow * 100) : 0
                            onValueModified: if (widgetRef) widgetRef.setTriggerHighLow(value / 100)
                        }
                        CustomSpinBox
                        {
                            Layout.fillWidth: true
                            from: 0; to: 1000; suffix: " ms"
                            enabled: widgetRef !== null
                            value: widgetRef ? Math.round(widgetRef.triggerHighHold) : 0
                            onValueModified: if (widgetRef) widgetRef.setTriggerHighHold(value)
                        }
                        CustomSpinBox
                        {
                            Layout.fillWidth: true
                            from: 0; to: 2000; suffix: " ms"
                            enabled: widgetRef !== null
                            value: widgetRef ? Math.round(widgetRef.triggerHighCooldown) : 0
                            onValueModified: if (widgetRef) widgetRef.setTriggerHighCooldown(value)
                        }
                    }
                }
        }

        SectionBox
        {
            sectionLabel: qsTr("Kick Detection")

            sectionContents:
                GridLayout
                {
                    width: parent.width
                    columns: 2
                    columnSpacing: 6
                    rowSpacing: 4

                    RobotoText { height: gridItemsHeight; label: qsTr("Enabled"); tooltipText: qsTr("Master toggle for the LedFx volume_beat_now kick detector. When off, audio.triggers.kick never fires.") }
                    CustomCheckBox
                    {
                        Layout.fillWidth: true
                        enabled: widgetRef !== null
                        checked: widgetRef ? widgetRef.kickEnabled : true
                        onToggled: if (widgetRef) widgetRef.setKickEnabled(checked)
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Beat max"); tooltipText: qsTr("Upper Hz cutoff for the LedFx volume_beat_now low-mel slice.") }
                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        from: 20; to: 1000; suffix: " Hz"
                        enabled: widgetRef !== null
                        value: widgetRef ? Math.round(widgetRef.kickBeatMaxHz) : 100
                        onValueModified: if (widgetRef) widgetRef.setKickBeatMaxHz(value)
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Min diff"); tooltipText: qsTr("Minimum percent difference over history for a kick.") }
                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        from: 0; to: 500; suffix: "%"
                        enabled: widgetRef !== null
                        value: widgetRef ? Math.round(widgetRef.kickBeatMinPercentDiff * 100) : 50
                        onValueModified: if (widgetRef) widgetRef.setKickBeatMinPercentDiff(value / 100)
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Min amplitude"); tooltipText: qsTr("Minimum LedFx beat power required before a kick can fire.") }
                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        from: 0; to: 1000; suffix: "%"
                        enabled: widgetRef !== null
                        value: widgetRef ? Math.round(widgetRef.kickBeatMinAmplitude * 100) : 50
                        onValueModified: if (widgetRef) widgetRef.setKickBeatMinAmplitude(value / 100)
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("Refractory"); tooltipText: qsTr("Minimum seconds between LedFx kick detections.") }
                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        from: 0; to: 2000; suffix: " ms"
                        enabled: widgetRef !== null
                        value: widgetRef ? Math.round(widgetRef.kickBeatRefractorySec * 1000) : 100
                        onValueModified: if (widgetRef) widgetRef.setKickBeatRefractorySec(value / 1000)
                    }

                    RobotoText { height: gridItemsHeight; label: qsTr("History"); tooltipText: qsTr("Fixed LedFx history deque capacity used in the percent-diff denominator.") }
                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        from: 1; to: 500; suffix: qsTr(" frames")
                        enabled: widgetRef !== null
                        value: widgetRef ? widgetRef.kickBeatHistoryLen : 10
                        onValueModified: if (widgetRef) widgetRef.setKickBeatHistoryLen(value)
                    }
                }
        }

        SectionBox
        {
            sectionLabel: qsTr("QLC+: Volume & Display")

            sectionContents:
                Column
                {
                    width: parent.width
                    spacing: 6

                    GridLayout
                    {
                        width: parent.width
                        columns: 2
                        columnSpacing: 6
                        rowSpacing: 4

                        RobotoText { height: gridItemsHeight; label: qsTr("Smoothing"); tooltipText: qsTr("Time constant (ms) for the smoothed volume readout.") }
                        CustomSpinBox
                        {
                            Layout.fillWidth: true
                            from: 0; to: 500; suffix: " ms"
                            enabled: widgetRef !== null
                            value: widgetRef ? Math.round(widgetRef.volumeSmoothing) : 100
                            onValueModified: if (widgetRef) widgetRef.setVolumeSmoothing(value)
                        }

                        RobotoText { height: gridItemsHeight; label: qsTr("Brightness floor"); tooltipText: qsTr("Minimum normalized output level so the display never goes fully dark.") }
                        RowLayout
                        {
                            Layout.fillWidth: true
                            spacing: 6
                            Slider
                            {
                                Layout.fillWidth: true
                                from: 0; to: 1; stepSize: 0.01
                                enabled: widgetRef !== null
                                value: widgetRef ? widgetRef.brightnessFloor : 0
                                onPressedChanged: if (!pressed && widgetRef) widgetRef.setBrightnessFloor(value)
                            }
                            RobotoText
                            {
                                Layout.preferredWidth: UISettings.bigItemHeight
                                height: gridItemsHeight
                                label: widgetRef ? Math.round(widgetRef.brightnessFloor * 100) + "%" : "--"
                            }
                        }

                        RobotoText { height: gridItemsHeight; label: qsTr("Raw"); tooltipText: qsTr("Live unsmoothed volume.") }
                        RowLayout
                        {
                            Layout.fillWidth: true
                            spacing: 6
                            Rectangle
                            {
                                Layout.fillWidth: true; Layout.preferredHeight: 10; radius: 4
                                color: UISettings.bgStrong; border.width: 1; border.color: UISettings.bgLight
                                Rectangle
                                {
                                    anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom
                                    width: parent.width * Math.max(0, Math.min(1, widgetRef ? widgetRef.volumeRaw : 0))
                                    radius: parent.radius; color: "#33cc66"
                                }
                            }
                            RobotoText
                            {
                                Layout.preferredWidth: UISettings.bigItemHeight
                                height: gridItemsHeight
                                label: widgetRef ? Math.round(widgetRef.volumeRaw * 100) + "%" : "--"
                            }
                        }

                        RobotoText { height: gridItemsHeight; label: qsTr("Normalized"); tooltipText: qsTr("Smoothed and floored volume used for display brightness.") }
                        RowLayout
                        {
                            Layout.fillWidth: true
                            spacing: 6
                            Rectangle
                            {
                                Layout.fillWidth: true; Layout.preferredHeight: 10; radius: 4
                                color: UISettings.bgStrong; border.width: 1; border.color: UISettings.bgLight
                                Rectangle
                                {
                                    anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom
                                    width: parent.width * Math.max(0, Math.min(1, widgetRef ? widgetRef.volumeNormalized : 0))
                                    radius: parent.radius; color: UISettings.selection
                                }
                            }
                            RobotoText
                            {
                                Layout.preferredWidth: UISettings.bigItemHeight
                                height: gridItemsHeight
                                label: widgetRef ? Math.round(widgetRef.volumeNormalized * 100) + "%" : "--"
                            }
                        }
                    }
                }
        }

        SectionBox
        {
            id: audioTriggerProp
            sectionLabel: qsTr("Spectrum Bar Mappings")

            sectionContents:
                GridLayout
                {
                    width: parent.width
                    columns: 2
                    columnSpacing: 5
                    rowSpacing: 4

                    // row 1
                    RobotoText
                    {
                        height: gridItemsHeight
                        label: qsTr("Number of bars")
                    }

                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        height: gridItemsHeight
                        value: widgetRef ? widgetRef.barsNumber - 1 : 0
                        onValueModified: if (widgetRef) widgetRef.barsNumber = value + 1
                    }

                    // row 2
                    ListView
                    {
                        id: barsList
                        Layout.columnSpan: 2
                        Layout.fillWidth: true
                        clip: true
                        //implicitWidth: audioTriggerPropsColumn.width
                        implicitHeight: count * gridItemsHeight
                        boundsBehavior: Flickable.StopAtBounds
                        headerPositioning: ListView.OverlayHeader
                        model: widgetRef ? widgetRef.barsInfo : null

                        property Item currentChecked: null
                        property int currentType: VCAudioTriggers.None

                        header:
                            RowLayout
                            {
                                z: 2
                                width: barsList.width
                                height: gridItemsHeight

                                RobotoText
                                {
                                    width: UISettings.bigItemHeight * 1.8
                                    height: gridItemsHeight
                                    label: qsTr("Name")
                                    color: UISettings.sectionHeader
                                }
                                Rectangle { width: 1; height: gridItemsHeight }

                                RobotoText
                                {
                                    width: UISettings.bigItemHeight + gridItemsHeight + 10
                                    height: gridItemsHeight
                                    label: qsTr("Type")
                                    color: UISettings.sectionHeader
                                }

                                Rectangle { width: 1; height: gridItemsHeight }

                                RobotoText
                                {
                                    Layout.fillWidth: true
                                    height: gridItemsHeight
                                    label: qsTr("Information")
                                    color: UISettings.sectionHeader
                                }
                            }

                        delegate:
                            Row
                            {
                                width: barsList.width
                                height: modelData.type === VCAudioTriggers.FunctionBar ||
                                        modelData.type === VCAudioTriggers.VCWidgetBar ? gridItemsHeight * 2 : gridItemsHeight
                                spacing: 10

                                RobotoText
                                {
                                    width: UISettings.bigItemHeight * 1.8
                                    height: gridItemsHeight
                                    label: modelData.bLabel
                                }
                                CustomComboBox
                                {
                                    width: UISettings.bigItemHeight
                                    height: gridItemsHeight
                                    model: [
                                        { mLabel: qsTr("None"), faIcon: FontAwesome.fa_ban },
                                        { mLabel: qsTr("DMX"), faIcon: FontAwesome.fa_sliders },
                                        { mLabel: qsTr("Function"), faIcon: FontAwesome.fa_cubes },
                                        { mLabel: qsTr("Widget"), faIcon: FontAwesome.fa_table_list }
                                    ]
                                    currentIndex: modelData.type

                                    onActivated:
                                    {
                                        if (widgetRef)
                                        {
                                            widgetRef.selectedBar = modelData.index
                                            widgetRef.setBarType(currentIndex)
                                        }
                                    }
                                }
                                Rectangle
                                {
                                    visible: modelData.type === VCAudioTriggers.None
                                    width: height
                                    height: gridItemsHeight
                                    color: "transparent"
                                }
                                IconButton
                                {
                                    visible: modelData.type !== VCAudioTriggers.None
                                    width: height
                                    height: gridItemsHeight
                                    faSource: FontAwesome.fa_pen_to_square
                                    checkable: true
                                    checked: widgetRef && widgetRef.selectedBar === modelData.index && sideLoader.visible
                                    onCheckedChanged:
                                    {
                                        widgetRef.selectedBar = modelData.index
                                        if (checked)
                                        {
                                            if (!sideLoader.visible)
                                                rightSidePanel.width += UISettings.sidePanelWidth
                                            sideLoader.visible = true
                                            sideLoader.modelProvider = widgetRef
                                            if (modelData.type === VCAudioTriggers.DMXBar)
                                                sideLoader.source = "qrc:/FixtureGroupManager.qml"
                                            else if (modelData.type === VCAudioTriggers.FunctionBar)
                                                sideLoader.source = "qrc:/FunctionManager.qml"
                                            else if (modelData.type === VCAudioTriggers.VCWidgetBar)
                                                sideLoader.source = "qrc:/VCWidgetsList.qml"
                                            barsList.currentChecked = this
                                            barsList.currentType = modelData.type
                                        }
                                        else
                                        {
                                            rightSidePanel.width -= sideLoader.width
                                            sideLoader.source = ""
                                            sideLoader.visible = false
                                            barsList.currentType = VCAudioTriggers.None
                                        }
                                    }
                                }

                                RobotoText
                                {
                                    visible: modelData.type !== VCAudioTriggers.None && modelData.type === VCAudioTriggers.DMXBar
                                    width: UISettings.bigItemHeight * 2
                                    height: gridItemsHeight
                                    clip: false
                                    label: modelData.intVal + " " + qsTr("Channels")
                                }

                                Item
                                {
                                    // Function/Widget bars: just hosts the IconTextEntry below.
                                    // Per-mapping thresholds were removed — hysteresis is now
                                    // configured per band on the AudioProfile.
                                    visible: modelData.type === VCAudioTriggers.FunctionBar ||
                                             modelData.type === VCAudioTriggers.VCWidgetBar
                                    width: UISettings.bigItemHeight * 2
                                    height: gridItemsHeight

                                    IconTextEntry
                                    {
                                        visible: modelData.type === VCAudioTriggers.FunctionBar
                                        y: gridItemsHeight
                                        height: gridItemsHeight
                                        width: parent.width

                                        property QLCFunction func: functionManager.getFunction(modelData.intVal)
                                        tLabel: func ? func.name : ""
                                        functionType: func ? func.type : -1
                                    }

                                    IconTextEntry
                                    {
                                        visible: modelData.type === VCAudioTriggers.VCWidgetBar
                                        y: gridItemsHeight
                                        height: gridItemsHeight
                                        width: parent.width
                                        iSrc: modelData.iconVal ? modelData.iconVal : ""
                                        tLabel: modelData.strVal ? modelData.strVal : ""
                                    }
                                }
                            }

                        Rectangle
                        {
                            id: addFunctionBox
                            visible: barsList.currentType === VCAudioTriggers.FunctionBar
                            anchors.fill: barsList
                            color: addFunctionDrop.containsDrag ? UISettings.activeDropArea : UISettings.bgMedium
                            opacity: 0.9
                            radius: 10

                            RobotoText
                            {
                                id: afText
                                anchors.centerIn: parent
                                label: qsTr("Drop a Function here")
                                labelColor: addFunctionDrop.containsDrag ? UISettings.bgStronger : UISettings.fgMain
                                fontBold: addFunctionDrop.containsDrag ? true : false
                            }

                            DropArea
                            {
                                id: addFunctionDrop
                                anchors.fill: parent

                                keys: [ "function" ]

                                onDropped:
                                {

                                    if (drag.source.hasOwnProperty("fromFunctionManager"))
                                    {
                                        barsList.currentChecked.checked = false
                                        widgetRef.setBarFunction(drag.source.itemsList[0])
                                    }
                                }
                            }
                        }

                        Rectangle
                        {
                            id: addWidgetBox
                            visible: barsList.currentType === VCAudioTriggers.VCWidgetBar
                            anchors.fill: barsList
                            color: addWidgetDrop.containsDrag ? UISettings.activeDropArea : UISettings.bgMedium
                            opacity: 0.9
                            radius: 10

                            RobotoText
                            {
                                anchors.centerIn: parent
                                label: qsTr("Drop a VC Widget here")
                                labelColor: addWidgetDrop.containsDrag ? UISettings.bgStronger : UISettings.fgMain
                                fontBold: addWidgetDrop.containsDrag ? true : false
                            }

                            DropArea
                            {
                                id: addWidgetDrop
                                anchors.fill: parent

                                keys: [ "audiotriggerswidget" ]

                                onDropped:
                                {
                                    if (drag.source.hasOwnProperty("fromVCWidgetsList")
                                            && drag.source.itemsList.length)
                                    {
                                        barsList.currentChecked.checked = false
                                        widgetRef.setBarWidget(drag.source.itemsList[0])
                                    }
                                }
                            }
                        }
                    } // ListView
                } // GridLayout
        } // SectionBox

        SectionBox
        {
            sectionLabel: qsTr("History")

            sectionContents:
                GridLayout
                {
                    width: parent.width
                    columns: 2
                    columnSpacing: 6
                    rowSpacing: 4

                    RobotoText
                    {
                        height: gridItemsHeight
                        label: qsTr("Onset history window")
                        tooltipText: qsTr("Duration of onset history to retain for analysis (seconds). Affects onset rate calculations.")
                    }

                    CustomSpinBox
                    {
                        Layout.fillWidth: true
                        height: gridItemsHeight
                        suffix: " s"
                        from: 1
                        to: 30
                        value: widgetRef ? widgetRef.onsetHistorySeconds : 5
                        onValueModified: if (widgetRef) widgetRef.onsetHistorySeconds = value
                    }
                }
        } // SectionBox

    } // Column
}
