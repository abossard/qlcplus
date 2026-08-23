/*
  Q Light Controller Plus
  workspacebridge.h

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

#ifndef WORKSPACEBRIDGE_H
#define WORKSPACEBRIDGE_H

#include <QString>

/**
 * Abstract project-file interface, the workspace counterpart to VCBridge.
 *
 * Opening and saving a project lives in the UI layer (App in qmlui), not in
 * Doc, because it also resets contexts, notifies network clients and updates
 * the recent-files list. This interface keeps mcp/ free of any qmlui include
 * while still letting the MCP tools drive those paths.
 */
class WorkspaceBridge
{
public:
    virtual ~WorkspaceBridge() {}

    /** Absolute path of the current project file, empty when never saved. */
    virtual QString currentFileName() const = 0;

    /** Discard everything and start an empty project. */
    virtual bool newWorkspace() = 0;

    /** Replace the current project with the one at fileName. */
    virtual bool loadWorkspace(const QString &fileName) = 0;

    /** Write the current project to fileName. */
    virtual bool saveWorkspace(const QString &fileName) = 0;
};

#endif // WORKSPACEBRIDGE_H
