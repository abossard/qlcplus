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
#include <functional>

class Doc;
class VCBridge;
class InputOutputMap;

namespace fastmcpp { namespace tools { class ToolManager; } }
namespace fastmcpp { namespace prompts { class PromptManager; } }
namespace fastmcpp { namespace resources { class ResourceManager; } }
namespace fastmcpp { namespace server { class Server; class StreamableHttpServerWrapper; } }

/** Callback to delete a function by ID. Each UI provides its own implementation
 *  to handle undo, sequence cleanup, etc. Falls back to Doc::deleteFunction
 *  if not provided. */
using DeleteFunctionFn = std::function<void(quint32)>;

/**
 * MCP (Model Context Protocol) server for QLC+.
 * Exposes lighting show design tools to any MCP-compatible AI agent.
 * Works with both v4 Widget UI and v5 QML UI via VCBridge interface.
 */
class McpServer : public QObject
{
    Q_OBJECT

public:
    McpServer(Doc *doc, VCBridge *vcBridge, DeleteFunctionFn deleteFunc = nullptr,
              QObject *parent = nullptr);
    ~McpServer();

    /** Start MCP server in HTTP mode (non-blocking, runs in background) */
    void startHttp(int port = 9696);
    void stopHttp();

private:
    Doc *m_doc;
    VCBridge *m_vcBridge;
    DeleteFunctionFn m_deleteFunc;
    std::unique_ptr<fastmcpp::tools::ToolManager> m_toolManager;
    std::unique_ptr<fastmcpp::prompts::PromptManager> m_promptManager;
    std::unique_ptr<fastmcpp::resources::ResourceManager> m_resourceManager;
    std::unique_ptr<fastmcpp::server::Server> m_server;
    std::unique_ptr<fastmcpp::server::StreamableHttpServerWrapper> m_httpServer;
};

#endif // MCPSERVER_H
