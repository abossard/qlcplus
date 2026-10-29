/*
  Q Light Controller Plus - Unit test
  showclipautomation_test.h

  Copyright (c) QLC+ contributors

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

#ifndef SHOWCLIPAUTOMATION_TEST_H
#define SHOWCLIPAUTOMATION_TEST_H

#include <QObject>

class ShowClipAutomation_Test final : public QObject
{
    Q_OBJECT

private slots:
    // Evaluation
    void songPosition_data();
    void songPosition();
    void inPoint_data();
    void inPoint();
    void level_data();
    void level();
    void dominant_data();
    void dominant();
    void refusedValuesKeepThePreviousOnes_data();
    void refusedValuesKeepThePreviousOnes();

    // Builders the recorder compresses its samples with
    void segmentsFromSamples_data();
    void segmentsFromSamples();
    void segmentsKeepJitteredSamplesWithinTolerance_data();
    void segmentsKeepJitteredSamplesWithinTolerance();
    void envelopeFromSamples_data();
    void envelopeFromSamples();
    void dominanceFromIntervals_data();
    void dominanceFromIntervals();

    // Load & save, on its own and through the ShowFunction hook
    void xmlRoundTrip_data();
    void xmlRoundTrip();
    void xmlUnreadable_data();
    void xmlUnreadable();
    void showFunctionXml_data();
    void showFunctionXml();
};

#endif
