/*
  Q Light Controller Plus - Unit test
  gridlayout_test.h

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

#ifndef GRIDLAYOUT_TEST_H
#define GRIDLAYOUT_TEST_H

#include <QObject>

class GridLayout_Test final : public QObject
{
    Q_OBJECT

private slots:
    void compactVertical_data();
    void compactVertical();

    void resolveCollisions_data();
    void resolveCollisions();

    void pixelsToCells_data();
    void pixelsToCells();

    void cellsToPixels_data();
    void cellsToPixels();

    void roundtripConversion();

    void cellsOverlap_data();
    void cellsOverlap();

    void cellWidth_basic();
};

#endif // GRIDLAYOUT_TEST_H
