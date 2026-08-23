#ifndef MCPINIT_H
#define MCPINIT_H

class QCommandLineParser;
class App;
class Doc;
class VirtualConsole;
class FunctionManager;
class FlowConsole;

/** Add --mcp-http option to the parser */
void mcpAddOptions(QCommandLineParser &parser);

/** Start MCP server based on parsed command line */
void mcpInit(Doc *doc, VirtualConsole *vc, FunctionManager *funcMgr,
             FlowConsole *flowConsole, App *app, const QCommandLineParser &parser);

#endif
