/*
  Q Light Controller Plus - Unit test
  vc_bridge_structs_test.h

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

#ifndef VC_BRIDGE_STRUCTS_TEST_H
#define VC_BRIDGE_STRUCTS_TEST_H

#include <QObject>

class VCBridgeStructs_Test final : public QObject
{
    Q_OBJECT

private slots:
    // WidgetDetails defaults
    void widgetDetails_defaultValues();

    // Config struct defaults
    void buttonConfig_allOptionalEmpty();
    void frameConfig_allOptionalEmpty();
    void matrixConfig_allOptionalEmpty();
    void cueListConfig_allOptionalEmpty();
    void clockConfig_allOptionalEmpty();
    void speedDialConfig_allOptionalEmpty();
    void sliderConfig_allOptionalEmpty();

    // Config struct population
    void buttonConfig_setAndRead();
    void frameConfig_setAndRead();
    void clockScheduleInfo_defaults();
    void speedDialFunctionInfo_defaults();
    void fontConfig_setAndRead();
    void xyPadPresetInfo_defaults();
};

#endif // VC_BRIDGE_STRUCTS_TEST_H
