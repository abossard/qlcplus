/*
  Q Light Controller Plus - Unit test
  vc_validation_test.h

  Copyright (C) Massimo Callegari

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

#ifndef VC_VALIDATION_TEST_H
#define VC_VALIDATION_TEST_H

#include <QObject>

class VCValidation_Test final : public QObject
{
    Q_OBJECT

private slots:
    // Type resolution
    void typeFromString_button();
    void typeFromString_allTypes();
    void typeFromString_invalid();
    void typeToString_roundTrip();

    // Field validation — create mode
    void createValidation_buttonFieldsValid();
    void createValidation_sliderFieldsValid();
    void createValidation_frameFieldsValid();
    void createValidation_framePageLabelsValid();
    void createValidation_matrixFieldsValid();
    void createValidation_allTypesAcceptCommonFields();
    void createValidation_buttonFieldOnSlider_rejected();
    void createValidation_sliderFieldOnButton_rejected();
    void createValidation_matrixFieldOnClock_rejected();
    void createValidation_unknownField_rejected();
    void createValidation_errorContainsWidgetType();
    void createValidation_errorContainsInvalidField();
    void createValidation_errorListsAllowedFields();

    // Field validation — update mode
    void updateValidation_buttonFieldsValid();
    void updateValidation_commonFieldsOnAnyType();
    void updateValidation_catchValuesOnSlider_valid();
    void updateValidation_catchValuesOnButton_rejected();
    void updateValidation_multipageModeOnFrame_valid();
    void updateValidation_pageLabelsOnFrame_valid();
    void updateValidation_pageLabelsOnButton_rejected();
    void updateValidation_multipageModeOnLabel_rejected();
    void updateValidation_barsNumberOnAudioTrigger_valid();
    void updateValidation_barsNumberOnButton_rejected();

    // Value validation
    void valueValidation_validAction();
    void valueValidation_invalidAction();
    void valueValidation_validMode();
    void valueValidation_invalidMode();
    void valueValidation_validColor();
    void valueValidation_invalidColor();
    void valueValidation_startupIntensityInRange();
    void valueValidation_startupIntensityOutOfRange();
    void valueValidation_rangeLimitInRange();
    void valueValidation_rangeLimitOutOfRange();
    void valueValidation_validClockType();
    void valueValidation_invalidClockType();
    void valueValidation_validWidgetStyle();
    void valueValidation_invalidWidgetStyle();
    void valueValidation_validFont();
    void valueValidation_invalidFontField();
    void valueValidation_validPosition();
    void valueValidation_positionOutOfRange();
    void valueValidation_invalidType();

    // Full validation pipeline
    void fullValidation_buttonCreateValid();
    void fullValidation_buttonCreateInvalidField();
    void fullValidation_buttonCreateInvalidValue();
    void fullValidation_sliderUpdateValid();
    void fullValidation_sliderUpdateCrossTypeField();

    // parentID / pageIndex exclusivity
    void parentOrPage_frameWithPageIndex_valid();
    void parentOrPage_frameWithParentID_valid();
    void parentOrPage_frameWithBoth_rejected();
    void parentOrPage_frameWithNeither_rejected();
    void parentOrPage_soloframeWithPageIndex_valid();
    void parentOrPage_soloframeWithBoth_rejected();
    void parentOrPage_buttonWithParentID_valid();
    void parentOrPage_buttonWithPageIndex_rejected();
    void parentOrPage_buttonWithNeither_rejected();
};

#endif // VC_VALIDATION_TEST_H
