#ifndef MCPINIT_H
#define MCPINIT_H

class QCommandLineParser;
class Doc;
class VirtualConsole;

/** Add --mcp-http option to the parser */
void mcpAddOptions(QCommandLineParser &parser);

/** Start MCP server based on parsed command line */
void mcpInit(Doc *doc, VirtualConsole *vc, const QCommandLineParser &parser);

#endif
