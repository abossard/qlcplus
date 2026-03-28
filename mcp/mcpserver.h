/*
  Q Light Controller Plus
  mcpserver.h

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

#ifndef MCPSERVER_H
#define MCPSERVER_H

#include <QObject>
#include <QString>
#include <memory>

class Doc;
class VCBridge;
class InputOutputMap;

namespace fastmcpp { namespace tools { class ToolManager; } }
namespace fastmcpp { namespace server { class StreamableHttpServerWrapper; } }

/**
 * MCP (Model Context Protocol) server for QLC+.
 * Exposes lighting show design tools to any MCP-compatible AI agent.
 * Works with both v4 Widget UI and v5 QML UI via VCBridge interface.
 */
class McpServer : public QObject
{
    Q_OBJECT

public:
    McpServer(Doc *doc, VCBridge *vcBridge, QObject *parent = nullptr);
    ~McpServer();

    /** Start MCP server in HTTP mode (non-blocking, runs in background) */
    void startHttp(int port = 9876);
    void stopHttp();

private:
    Doc *m_doc;
    VCBridge *m_vcBridge;
    std::unique_ptr<fastmcpp::tools::ToolManager> m_toolManager;
    std::unique_ptr<fastmcpp::server::StreamableHttpServerWrapper> m_httpServer;
};

#endif // MCPSERVER_H
