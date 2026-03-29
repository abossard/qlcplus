#include "mcpinit.h"
#include "mcpserver.h"
#include "vcbridgev5.h"

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

void mcpInit(Doc *doc, VirtualConsole *vc, FunctionManager *funcMgr, const QCommandLineParser &parser)
{
    if (parser.isSet(s_noMcpOption))
        return;

    VCBridgeV5 *vcBridge = nullptr;
    if (vc)
        vcBridge = new VCBridgeV5(doc, vc);

    int port = parser.value(s_mcpHttpPortOption).toInt();
    McpServer *server = new McpServer(doc, vcBridge, funcMgr);
    server->startHttp(port);
}
