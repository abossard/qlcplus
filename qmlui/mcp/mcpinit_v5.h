/*
  Q Light Controller Plus
  mcpinit_v5.h

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

#ifndef MCPINIT_V5_H
#define MCPINIT_V5_H

class QCommandLineParser;
class Doc;
class VirtualConsole;
class FunctionManager;

/** Add --mcp-port and --no-mcp options to the parser */
void mcpAddOptions(QCommandLineParser &parser);

/** Start MCP server with V5 (QML) bridge */
void mcpInitV5(Doc *doc, VirtualConsole *vc, FunctionManager *funcMgr, const QCommandLineParser &parser);

#endif
