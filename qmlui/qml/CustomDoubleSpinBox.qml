/*
  Q Light Controller Plus
  CustomDoubleSpinBox.qml

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
import QtQuick.Controls.Basic
import "."

CustomSpinBox
{
    id: controlRoot
    property real scale: Math.pow(10, decimals)
    property bool boundedControl: false

    from: boundedControl ? Math.round(realFrom * scale) : realFrom * scale
    to: boundedControl ? Math.round(realTo * scale) : realTo * scale
    value: boundedControl ? Math.round(realValue * scale) : realValue * scale
    stepSize: boundedControl ? Math.max(1, Math.round(realStep * scale)) : realStep * scale
    suffix: "°"

    property real realFrom: 0
    property real realTo: 100
    property real realValue: 0
    property real realStep: 0.5
    property int decimals: 2

    validator: boundedControl ? boundedValidator : unboundedValidator
    DoubleValidator
    {
        id: boundedValidator
        decimals: controlRoot.decimals
        notation: DoubleValidator.StandardNotation
        bottom: Math.min(controlRoot.realFrom, controlRoot.realTo)
        top: Math.max(controlRoot.realFrom, controlRoot.realTo)
    }

    DoubleValidator
    {
        id: unboundedValidator
        bottom: Math.min(controlRoot.from, controlRoot.to)
        top: Math.max(controlRoot.from, controlRoot.to)
    }

    textFromValue: function(value, locale) {
        return Number(value / scale).toLocaleString(locale, 'f', decimals) + suffix
    }

    valueFromText: function(text, locale) {
        return Number.fromLocaleString(locale, text.replace(suffix, "")) * scale
    }

    onValueModified: realValue = value / scale
    onRealValueChanged:
    {
        var targetValue = boundedControl ? Math.round(realValue * scale) : realValue * scale
        if (value !== targetValue)
            value = targetValue
    }
    onScaleChanged:
    {
        var targetValue = boundedControl ? Math.round(realValue * scale) : realValue * scale
        if (value !== targetValue)
            value = targetValue
    }
    onBoundedControlChanged:
    {
        var targetValue = boundedControl ? Math.round(realValue * scale) : realValue * scale
        if (value !== targetValue)
            value = targetValue
    }

    function setValue(newValue)
    {
        value = boundedControl ? Math.round(newValue) : newValue
        realValue = value / scale
    }
}
