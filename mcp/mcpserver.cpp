/*
  Q Light Controller Plus
  mcpserver.cpp

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

#include "mcpserver.h"
#include "tools/tool_registry.h"

#include <fastmcpp/tools/manager.hpp>
#include <fastmcpp/mcp/handler.hpp>
#include <fastmcpp/server/stdio_server.hpp>
#include <fastmcpp/server/streamable_http_server.hpp>

#include <QDebug>

McpServer::McpServer(Doc *doc, VCBridge *vcBridge, QObject *parent)
    : QObject(parent)
    , m_doc(doc)
    , m_vcBridge(vcBridge)
    , m_toolManager(std::make_unique<fastmcpp::tools::ToolManager>())
{
    registerQueryTools(*m_toolManager, m_doc, m_vcBridge);
    registerFunctionTools(*m_toolManager, m_doc);
    registerVCTools(*m_toolManager, m_doc, m_vcBridge);
    registerIOTools(*m_toolManager, m_doc);
    registerChannelTools(*m_toolManager, m_doc);
    registerPrompts(*m_toolManager);
}

McpServer::~McpServer()
{
}

void McpServer::runStdio()
{
    auto handler = fastmcpp::mcp::make_mcp_handler("qlcplus", "5.0.0", *m_toolManager);
    fastmcpp::server::StdioServerWrapper server(handler);
    server.run();
}

void McpServer::startHttp(int port)
{
    auto handler = fastmcpp::mcp::make_mcp_handler("qlcplus", "5.0.0", *m_toolManager);
    m_httpServer = std::make_unique<fastmcpp::server::StreamableHttpServerWrapper>(
        handler, "127.0.0.1", port, "/mcp");
    m_httpServer->start();
    qDebug() << "[MCP] HTTP server started on http://127.0.0.1:" << port << "/mcp";
}

void McpServer::stopHttp()
{
    if (m_httpServer)
    {
        m_httpServer->stop();
        m_httpServer.reset();
        qDebug() << "[MCP] HTTP server stopped";
    }
}
