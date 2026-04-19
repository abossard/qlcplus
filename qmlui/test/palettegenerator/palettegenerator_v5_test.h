/*
  Q Light Controller Plus - Unit test
  palettegenerator_v5_test.h

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

#ifndef PALETTEGENERATOR_V5_TEST_H
#define PALETTEGENERATOR_V5_TEST_H

#include <QObject>
#include "qlcfixturedefcache.h"

class PaletteGenerator_V5_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    // Capability detection
    void capabilities();
    void capabilitiesPrism();

    // Original types still work
    void createPrimaryColors();
    void createGobos();

    // New color palettes
    void createRainbow();
    void createWarmColors();
    void createCoolColors();

    // Beat-synced chaser timing
    void beatSyncedChasers();

    // Movement chasers
    void panTiltMovementChasers();

    // Prism
    void createPrism();

    // QLCPalette generation
    void colorPalettes();
    void positionPalettes();
    void dimmerPalettes();

private:
    QLCFixtureDefCache m_fixtureDefCache;
};

#endif
