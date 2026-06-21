/*
  Q Light Controller Plus - Unit test
  vcrecordpanel_test.h

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

#ifndef VCRECORDPANEL_TEST_H
#define VCRECORDPANEL_TEST_H

#include <QObject>

class Doc;

class VCRecordPanel_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void cleanup();

    // DmxCapture tests
    void captureAllFixtures_nullDoc();
    void captureAllFixtures_noFixtures();
    void captureAllFixtures_withFixtures();
    void captureAllFixtures_nonZeroOnly();

    // Scene creation through Doc
    void sceneCreation_basic();
    void sceneCreation_autoNaming();

    // Chaser creation through Doc
    void chaserCreation_basic();
    void chaserCreation_withSteps();

    // Full recording flow: create chaser, add scenes as steps
    void recordingFlow_chaserWithScenes();

    // XML round-trip for VCRecordPanel tags
    void xmlRoundTrip_data();
    void xmlRoundTrip();

private:
    Doc *m_doc;
};

#endif // VCRECORDPANEL_TEST_H
