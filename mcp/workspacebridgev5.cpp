/*
  Q Light Controller Plus
  workspacebridgev5.cpp

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

#include "workspacebridgev5.h"
#include "app.h"

WorkspaceBridgeV5::WorkspaceBridgeV5(App *app)
    : m_app(app)
{
}

QString WorkspaceBridgeV5::currentFileName() const
{
    return m_app->fileName();
}

bool WorkspaceBridgeV5::newWorkspace()
{
    return m_app->newWorkspace();
}

bool WorkspaceBridgeV5::loadWorkspace(const QString &fileName)
{
    return m_app->loadWorkspace(fileName);
}

bool WorkspaceBridgeV5::saveWorkspace(const QString &fileName)
{
    return m_app->saveWorkspace(fileName);
}
