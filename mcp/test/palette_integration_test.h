/*
  Q Light Controller Plus - Unit test
  palette_integration_test.h

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

#ifndef PALETTE_INTEGRATION_TEST_H
#define PALETTE_INTEGRATION_TEST_H

#include <QObject>

class Doc;

class PaletteIntegration_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();

    // QLCPalette CRUD via Doc
    void palette_createDimmer();
    void palette_createColor();
    void palette_createPan();
    void palette_createTilt();
    void palette_createPanTilt();
    void palette_deleteRemovesFromDoc();

    // Scene-palette integration
    void scene_addPaletteRef();
    void scene_addFixtureRegistration();
    void scene_clearPalettesOnUpsert();

    // Fanning
    void fanning_linearType();
    void fanning_sineType();
    void fanning_layoutVariants();
    void fanning_amountRange();

    // functionToJson with palette refs
    void functionToJson_sceneWithPalettes();
    void functionToJson_sceneWithoutPalettes();

    // Palette value storage
    void paletteValue_dimmerRange();
    void paletteValue_colorWithWAUV();
    void paletteValue_panTiltDegrees();

private:
    Doc *m_doc;
};

#endif // PALETTE_INTEGRATION_TEST_H
