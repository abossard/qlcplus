/*
  Q Light Controller Plus - Unit test
  vcpage_input_mode_test.h

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

#ifndef VCPAGE_INPUT_MODE_TEST_H
#define VCPAGE_INPUT_MODE_TEST_H

#include <QObject>

class VCPageInputMode_Test final : public QObject
{
    Q_OBJECT

private slots:
    // String conversion round-trips
    void stringConversion_data();
    void stringConversion();

    // Unknown string defaults to Normal
    void unknownStringDefaultsToNormal();

    // PageInfo struct carries externalInputMode
    void pageInfoHasExternalInputMode();

    // XML round-trip: saveExtraXML writes tag only for non-Normal modes
    void xmlSave_normalOmitsTag();
    void xmlSave_overrideWritesTag();
    void xmlSave_inheritWritesTag();

    // XML round-trip: loadExtraXML parses tag correctly
    void xmlLoad_data();
    void xmlLoad();

    // XML round-trip: missing tag defaults to Normal (backward compat)
    void xmlLoad_missingTagDefaultsToNormal();
};

#endif // VCPAGE_INPUT_MODE_TEST_H
