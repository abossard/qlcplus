/*
  Q Light Controller Plus - Unit test
  vceditor_test.h

  Copyright (c) Massimo Callegari

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

#ifndef VCEDITOR_TEST_H
#define VCEDITOR_TEST_H

#include <QObject>
#include <QTemporaryDir>
#include <memory>

class App;
class VirtualConsole;

/** Ids of the standard console built by buildConsole() */
struct ConsoleIds
{
    quint32 page = 0;
    quint32 frame = 0;   // page child, holds a, b and nested
    quint32 a = 0;
    quint32 b = 0;
    quint32 nested = 0;  // frame child, holds inner
    quint32 inner = 0;
    quint32 empty = 0;   // empty page child frame
    quint32 loose = 0;   // page child button
    quint32 onPage2 = 0; // button on the second page
};

class VCEditor_Test : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanupTestCase();

    void editUndoRedoRoundTrip_data();
    void editUndoRedoRoundTrip();
    void liveToggleMakesNoHistory_data();
    void liveToggleMakesNoHistory();
    void clonesStayOnTheirPage();
    void arrangeFrame_data();
    void arrangeFrame();
    void arrangeNested();
    void arrangedStructure_data();
    void arrangedStructure();
    void arrangePanel();
    void widgetMatrix_data();
    void widgetMatrix();
    void arrangeNoOpMakesNoHistory_data();
    void arrangeNoOpMakesNoHistory();
    void newWorkspaceForgetsArrangement();
    void arrangeSettingsPersist();
    void framePageVisibility_data();
    void framePageVisibility();
    void randomEditSequence_data();
    void randomEditSequence();
    void multiSelectionPanel();
    void pageResize_data();
    void pageResize();
    void geometryFields_data();
    void geometryFields();
    void doubleClickOpensProperties();
    void widgetMenu_data();
    void widgetMenu();
    void editKeys_data();
    void editKeys();
    void detachedConsoleKeys_data();
    void detachedConsoleKeys();
    void frameCheckboxes_data();
    void frameCheckboxes();

    void pageSwitchPrunesSelection_data();
    void pageSwitchPrunesSelection();
    void widgetsInRect_data();
    void widgetsInRect();
    void bandSelection_data();
    void bandSelection();
    void keysIgnoredWhileTyping_data();
    void keysIgnoredWhileTyping();
    void pasteRendersIntoFrame();
    void pastePlacement_data();
    void pastePlacement();
    void duplicateKeepsClipboard();
    void newWorkspaceShowsFirstPage();
    void pageDeleteUndo_data();
    void pageDeleteUndo();
    void destroyedDialogReleasesKeys();
    void editErrorsDoNotBlock_data();
    void editErrorsDoNotBlock();
    void functionDropsOnlyWhileEditing_data();
    void functionDropsOnlyWhileEditing();
    void nudgeStep_data();
    void nudgeStep();
    void alignCenters_data();
    void alignCenters();
    void matchSize_data();
    void matchSize();
    void alignToLastSelected_data();
    void alignToLastSelected();

private:
    VirtualConsole *vc() const;
    ConsoleIds buildConsole();
    QByteArray consoleXML() const;
    QObject *rightPanel() const;
    QPoint widgetCenter(quint32 id) const;
    /** Keep the window as an artifact and compare it with its reference
     *  in QLC_VC_GOLDEN_DIR, which QLC_VC_UPDATE_GOLDEN=1 rewrites */
    void checkScreenshot(const QString &name);
    /** Wait until the Tardis thread recorded every queued action, then
     *  leave enough time so the next action starts a new undo step */
    void settleHistory();

private:
    QTemporaryDir m_settings;
    std::unique_ptr<App> m_app;
};

#endif
