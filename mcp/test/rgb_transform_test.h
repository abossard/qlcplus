/*
  Q Light Controller Plus - Unit test
  rgb_transform_test.h

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

#ifndef RGB_TRANSFORM_TEST_H
#define RGB_TRANSFORM_TEST_H

#include <QObject>

class RGBTransform_Test final : public QObject
{
    Q_OBJECT

private slots:
    // Rotation tests on non-square grid
    void rotation0_identity();
    void rotation90_nonSquare();
    void rotation180_nonSquare();
    void rotation270_nonSquare();

    // Rotation round-trip: 4x 90° = identity
    void rotation_roundTrip();

    // Mirror tests
    void mirrorHorizontal_flip();
    void mirrorVertical_flip();
    void mirrorHorizontal_max();
    void mirrorVertical_max();
    void mirrorHorizontalAndVertical_differ();

    // Combined rotation + mirror
    void rotation90_withMirror();

    // Setter validation
    void settersClamping();

    // MirrorBlend string conversion
    void mirrorBlend_stringConversion();
};

#endif // RGB_TRANSFORM_TEST_H
