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
#include <fastmcpp/prompts/manager.hpp>
#include <fastmcpp/resources/manager.hpp>
#include <fastmcpp/server/server.hpp>
#include <fastmcpp/mcp/handler.hpp>
#include <fastmcpp/server/streamable_http_server.hpp>

#include <QDebug>

McpServer::McpServer(Doc *doc, VCBridge *vcBridge, FunctionManager *funcMgr,
                     FlowConsole *flowConsole, QObject *parent)
    : QObject(parent)
    , m_doc(doc)
    , m_vcBridge(vcBridge)
    , m_funcMgr(funcMgr)
    , m_flowConsole(flowConsole)
    , m_toolManager(std::make_unique<fastmcpp::tools::ToolManager>())
    , m_promptManager(std::make_unique<fastmcpp::prompts::PromptManager>())
    , m_resourceManager(std::make_unique<fastmcpp::resources::ResourceManager>())
    , m_server(std::make_unique<fastmcpp::server::Server>(
          "qlcplus", "5.0.0",
          std::nullopt, // website_url
          std::nullopt, // icons
          std::string(  // server instructions
              "QLC+ lighting controller MCP server.\n\n"
              "Workflow: query_fixtures -> query_fixture_channels -> create_scenes -> "
              "create_chasers -> create_collections -> create_scripts -> build_show_page.\n\n"
              "Rules:\n"
              "- Always call query_fixture_channels before create_scenes to discover channel indices.\n"
              "- Each DMX channel belongs to exactly ONE layer (mood/position/dimmer/strobe/texture).\n"
              "- All create tools are idempotent (upsert by name) — safe to call repeatedly.\n"
              "- Use tempoType \"beats\" for DJ/club shows.\n"
              "- Scenes must only set channels for their layer (wash = color+dimmer only, position = pan+tilt only).\n"
              "- create_scripts accepts raw JavaScript (content field) — use for automation, randomness, easing, "
              "audio-reactive effects, BPM sync, state machines. Scripts are validated before saving.\n\n"
              "Fixture intelligence:\n"
              "- query_fixtures returns 'type' (Moving Head, LED Bar, Dimmer, etc.) and 'capabilities' "
              "(RGBW, ContinuousTiltRotation, Pan/Tilt, UV, Amber, etc.).\n"
              "- 'headMap' array shows per-head channel indices and rgbChannels — use this for multi-head fixtures.\n"
              "- RGBW fixtures: White channel is INDEPENDENT from RGB. Set W channel separately; "
              "picking white via RGB gives R=G=B=255 but does NOT drive the W channel.\n"
              "- ContinuousTiltRotation/ContinuousPanRotation: the tilt/pan channel has mixed behavior — "
              "part of the DMX range is fixed position, part is continuous rotation (CW/CCW). "
              "Check capability presets (RotationClockwise*, RotationCounterClockwise*) for the rotation ranges.\n"
              "- Channels with controlByte='fine' are the LSB of a 16-bit pair; the MSB is the 'coarse' channel "
              "with the same group. Combine for precise positioning.\n"
              "- headIndex on channels tells which head (LED pixel) a channel belongs to.\n\n"
              "DJ phase workflow — use SoloFrame multipage for phase-based shows:\n"
              "  1. Create a SoloFrame with multipageMode=true, totalPages=N, pageLabels=[\"Intro\",\"Build\",\"Drop\",...].\n"
              "  2. Create each child with childPageIndex set to its target phase page.\n"
              "  3. Query childPageIndex to verify placement; authoring never changes the visible current page.\n"
              "  4. Map next/previous page to MIDI via vc_map_inputs with sourceName \"nextPage\"/\"previousPage\".\n"
              "  5. Map direct page jumps with sourceName \"page0\", \"page1\", etc.\n"
              "  6. Additional source names: \"enable\" (toggle frame), \"collapse\" (toggle collapsed).\n"
              "  7. Multiple SoloFrames can share the same phase structure — all switch pages together via MIDI.\n\n"
              "Use MCP prompts for guided workflows: design_dj_show, debug_channel_conflict, setup_launchpad."
          )))
{
    registerQueryTools(*m_toolManager, m_doc, m_vcBridge);
    registerFunctionTools(*m_toolManager, m_doc, m_funcMgr);
    registerVCCreateTools(*m_toolManager, m_doc, m_vcBridge);
    registerVCUpdateTools(*m_toolManager, m_doc, m_vcBridge);
    registerVCInputTools(*m_toolManager, m_doc, m_vcBridge);
    registerVCLayoutTools(*m_toolManager, m_doc, m_vcBridge);
    registerIOTools(*m_toolManager, m_doc);
    registerChannelTools(*m_toolManager, m_doc);
    registerPaletteTools(*m_toolManager, m_doc);
    registerFlowTools(*m_toolManager, m_doc, m_flowConsole);
    registerPrompts(*m_promptManager, m_doc, m_vcBridge);
}

McpServer::~McpServer()
{
}

void McpServer::startHttp(int port)
{
    auto handler = fastmcpp::mcp::make_mcp_handler(
        "qlcplus", "5.0.0", *m_server, *m_toolManager, *m_resourceManager, *m_promptManager);
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
