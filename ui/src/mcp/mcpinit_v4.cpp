/*
  Q Light Controller Plus
  mcpinit_v4.cpp

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

#include "mcpinit_v4.h"
#include "mcpserver.h"
#include "vcbridgev4.h"
#include "doc.h"
#include "function.h"

#include <QCommandLineParser>
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

void mcpInitV4(Doc *doc, VirtualConsole *vc, FunctionManager *funcMgr, const QCommandLineParser &parser)
{
    Q_UNUSED(funcMgr);

    if (parser.isSet(s_noMcpOption))
        return;

    VCBridgeV4 *vcBridge = nullptr;
    if (vc)
        vcBridge = new VCBridgeV4(doc, vc);

    // V4's FunctionManager has no public single-function delete method,
    // so we use Doc::deleteFunction directly. This skips sequence bound-scene
    // cleanup but is safe for MCP usage.
    DeleteFunctionFn deleteFunc = [doc](quint32 id) {
        Function *f = doc->function(id);
        if (f && f->isRunning())
            f->stopAndWait();
        doc->deleteFunction(id);
    };

    int port = parser.value(s_mcpHttpPortOption).toInt();
    McpServer *server = new McpServer(doc, vcBridge, std::move(deleteFunc));
    server->startHttp(port);
}
