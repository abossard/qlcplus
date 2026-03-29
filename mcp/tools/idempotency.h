/*
  Q Light Controller Plus
  idempotency.h

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

#ifndef MCP_IDEMPOTENCY_H
#define MCP_IDEMPOTENCY_H

#include "doc.h"
#include "function.h"
#include "fixture.h"
#include "fixturegroup.h"

#include <QString>

namespace mcp {

/** Find an existing function by name and type. Returns nullptr if not found. */
inline Function* findFunction(Doc *doc, const QString &name, Function::Type type)
{
    for (Function *fn : doc->functions())
    {
        if (fn->name() == name && fn->type() == type)
            return fn;
    }
    return nullptr;
}

/** Find an existing fixture by name, universe, and address. */
inline Fixture* findFixture(Doc *doc, const QString &name, int universe, int address)
{
    for (Fixture *fxi : doc->fixtures())
    {
        if (fxi->name() == name &&
            (int)fxi->universe() == universe &&
            (int)fxi->address() == address)
            return fxi;
    }
    return nullptr;
}

/** Find an existing fixture group by name. */
inline FixtureGroup* findFixtureGroup(Doc *doc, const QString &name)
{
    for (FixtureGroup *group : doc->fixtureGroups())
    {
        if (group->name() == name)
            return group;
    }
    return nullptr;
}

} // namespace mcp

#endif // MCP_IDEMPOTENCY_H
