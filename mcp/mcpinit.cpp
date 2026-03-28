#include "mcpinit.h"
#include "mcpserver.h"
#include "vcbridgev5.h"

#include <QCommandLineParser>
#include <QDebug>

static QCommandLineOption s_mcpHttpOption(QStringList() << "mcp-http",
    "Start MCP HTTP server on specified port (default 9876)",
    "port", "9876");

void mcpAddOptions(QCommandLineParser &parser)
{
    parser.addOption(s_mcpHttpOption);
}

void mcpInit(Doc *doc, VirtualConsole *vc, const QCommandLineParser &parser)
{
    VCBridgeV5 *vcBridge = nullptr;
    if (vc)
        vcBridge = new VCBridgeV5(doc, vc);

    if (parser.isSet(s_mcpHttpOption))
    {
        int port = parser.value(s_mcpHttpOption).toInt();
        McpServer *server = new McpServer(doc, vcBridge);
        server->startHttp(port);
    }
}
