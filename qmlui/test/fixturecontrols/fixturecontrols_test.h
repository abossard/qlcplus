/*
  Q Light Controller Plus - Unit test
  fixturecontrols_test.h

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

#ifndef FIXTURECONTROLS_TEST_H
#define FIXTURECONTROLS_TEST_H

#include <QObject>
#include "qlcfixturedefcache.h"

class FixtureControls_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    // Rules, on synthetic modes
    void analyze_data();
    void analyze();

    // Invariants, on every mode of the fixture library
    void library_data();
    void library();

    // Per-control CSV of the whole library, when QLC_FIXTURE_CONTROLS_REPORT is set
    void report();

private:
    QLCFixtureDefCache m_fixtureDefCache;
};

#endif
