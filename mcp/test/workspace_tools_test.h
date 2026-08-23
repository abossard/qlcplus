/*
  Q Light Controller Plus - Unit test
  workspace_tools_test.h

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

#ifndef WORKSPACE_TOOLS_TEST_H
#define WORKSPACE_TOOLS_TEST_H

#include <QObject>

class Doc;

class McpWorkspaceTools_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    // registration
    void withoutBridge_toolsNotRegistered();

    // query_workspace_file
    void queryWorkspaceFile_neverSaved_reportsUnsaved();
    void queryWorkspaceFile_savedAndModified_reported();

    // save_workspace
    void saveWorkspace_withPath_delegatesPath();
    void saveWorkspace_noPath_usesCurrentFile();
    void saveWorkspace_noPathNeverSaved_returnsError();
    void saveWorkspace_bridgeFails_returnsError();
    void saveWorkspace_emptyPath_rejected();
    void saveWorkspace_existingOtherFile_needsOverwrite();
    void saveWorkspace_ownFile_noOverwriteNeeded();
    void saveWorkspace_relativePath_rejected();
    void saveWorkspace_missingDirectory_rejected();

    // load_workspace
    void loadWorkspace_missingFile_returnsErrorWithoutTouchingBridge();
    void loadWorkspace_dirtyDocWithoutFlag_refused();
    void loadWorkspace_dirtyDocWithFlag_loads();
    void loadWorkspace_cleanDoc_loads();
    void loadWorkspace_notAWorkspaceFile_refusedBeforeClearing();
    void loadWorkspace_directory_refused();
    void loadWorkspace_fileUrl_normalisedForBridge();
    void loadWorkspace_pathNotAString_rejected();
    void loadWorkspace_runningFunctions_refused();

    // new_workspace
    void newWorkspace_dirtyDocWithoutFlag_refused();
    void newWorkspace_cleanDoc_resets();
    void newWorkspace_runningFunctions_refused();
    void guardFlags_nonBoolean_rejected_data();
    void guardFlags_nonBoolean_rejected();

    // real file I/O
    void saveThenLoad_roundTripsThroughDisk();

private:
    Doc *m_doc = nullptr;
};

#endif // WORKSPACE_TOOLS_TEST_H
