/*
  Q Light Controller Plus - Unit test
  uimanager_preset_test.cpp

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

#include <QTest>
#include <QColor>
#include <QSignalSpy>

#include "uimanager_preset_test.h"
#include "uimanager.h"

void UiManagerPreset_Test::presetNamesContainsClassic()
{
    UiManager mgr(nullptr, nullptr);
    QStringList names = mgr.presetNames();
    QVERIFY(names.contains("Classic"));
    QVERIFY(names.indexOf("Classic") == 0); // Classic is always first
}

void UiManagerPreset_Test::presetNamesContainsVSCodeDark()
{
    UiManager mgr(nullptr, nullptr);
    QStringList names = mgr.presetNames();
    QVERIFY(names.contains("VS Code Dark"));
}

void UiManagerPreset_Test::applyPresetSetsCurrentPreset()
{
    UiManager mgr(nullptr, nullptr);
    QCOMPARE(mgr.currentPreset(), QString("Classic"));

    mgr.setCurrentPreset("VS Code Dark");
    QCOMPARE(mgr.currentPreset(), QString("VS Code Dark"));
}

void UiManagerPreset_Test::applyPresetEmitsSignal()
{
    UiManager mgr(nullptr, nullptr);
    QSignalSpy spy(&mgr, &UiManager::currentPresetChanged);

    mgr.setCurrentPreset("VS Code Dark");
    QCOMPARE(spy.count(), 1);

    /* Setting same value again should NOT emit */
    mgr.setCurrentPreset("VS Code Dark");
    QCOMPARE(spy.count(), 1);

    mgr.setCurrentPreset("Classic");
    QCOMPARE(spy.count(), 2);
}

QTEST_MAIN(UiManagerPreset_Test)
