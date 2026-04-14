/*
  Q Light Controller Plus
  mcpinit_v5.cpp

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

#include "mcpinit_v5.h"
#include "mcpserver.h"
#include "vcbridgev5.h"
#include "functionmanager.h"

#include <QCommandLineParser>
#include <QPointer>
#include <QDebug>

static QCommandLineOption s_mcpHttpPortOption(QStringList() << "mcp-port",
    "Set MCP HTTP server port (default 9696)",
    "port", "9696");

static QCommandLineOption s_noMcpOption(QStringList() << "no-mcp",
    "Disable the MCP HTTP server");

void mcpAddOptions(QCommandLineParser &parser)
{
    parser.addOption(s_mcpHttpPortOption);
    parser.addOption(s_noMcpOption);
}

void mcpInitV5(Doc *doc, VirtualConsole *vc, FunctionManager *funcMgr, const QCommandLineParser &parser)
{
    if (parser.isSet(s_noMcpOption))
        return;

    VCBridgeV5 *vcBridge = nullptr;
    if (vc)
        vcBridge = new VCBridgeV5(doc, vc);

    // Create lifetime-safe delete callback using QPointer
    DeleteFunctionFn deleteFunc = nullptr;
    if (funcMgr)
    {
        QPointer<FunctionManager> guard(funcMgr);
        deleteFunc = [guard](quint32 id) {
            if (guard)
                guard->deleteFunction(id);
        };
    }

    int port = parser.value(s_mcpHttpPortOption).toInt();
    McpServer *server = new McpServer(doc, vcBridge, std::move(deleteFunc));
    server->startHttp(port);
}
