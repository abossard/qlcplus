/*
  Q Light Controller Plus
  tool_registry.h

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

#ifndef TOOL_REGISTRY_H
#define TOOL_REGISTRY_H

#include <QObject>
#include <QThread>
#include <QMetaObject>

namespace fastmcpp { namespace tools { class ToolManager; } }
class Doc;
class VCBridge;

// Each tool file exports one registration function.
void registerQueryTools(fastmcpp::tools::ToolManager &tm, Doc *doc, VCBridge *vcBridge);
void registerFunctionTools(fastmcpp::tools::ToolManager &tm, Doc *doc);
void registerVCTools(fastmcpp::tools::ToolManager &tm, Doc *doc, VCBridge *vcBridge);
void registerIOTools(fastmcpp::tools::ToolManager &tm, Doc *doc);
void registerChannelTools(fastmcpp::tools::ToolManager &tm, Doc *doc);
void registerPrompts(fastmcpp::tools::ToolManager &tm);

// Thread-safe execution helper — runs lambda on Doc's thread
template<typename Func>
auto execOnMainThread(QObject *context, Func &&func) -> decltype(func())
{
    using ReturnType = decltype(func());
    if (QThread::currentThread() == context->thread())
        return func();
    ReturnType result;
    QMetaObject::invokeMethod(context, [&result, &func]() {
        result = func();
    }, Qt::BlockingQueuedConnection);
    return result;
}

#endif // TOOL_REGISTRY_H
