# AGENTS.md

Instructions for AI agents working on this repository. This is the canonical reference for code intelligence tooling.

## MCP Servers

This repo configures two MCP servers (see `.mcp.json`):

| Server | Transport | Purpose |
|--------|-----------|---------|
| `qlcplus` | HTTP (`localhost:9696/mcp`) | Live QLC+ workspace: fixtures, functions, widgets, DMX |
| `codegraph` | stdio (`codegraph serve --mcp`) | Code intelligence: symbol search, call graphs, impact analysis |

## CodeGraph Tools

The codegraph index covers **1,530 files**, **24,650 symbols**, and **51,757 edges**. Use these tools instead of grep/glob for symbol-level questions.

### Tool preference order

```
codegraph_context    → START HERE for any "how does X work" question
codegraph_files      → START HERE for file/folder exploration (replaces glob)
codegraph_explore    → inspect source of several symbols surfaced by context
codegraph_search     → quick symbol lookup by name (locations only)
codegraph_node       → detail on ONE symbol (signature, docstring, optionally source)
codegraph_callers    → who calls this symbol?
codegraph_callees    → what does this symbol call?
codegraph_impact     → what breaks if I change this symbol?
codegraph_status     → is the index healthy?
grep / glob          → FALLBACK: literal text, regex patterns, non-code files
```

### Tool catalog

#### `codegraph_context` — Primary entry point

Call this **first** for any architecture, feature, or bug-context question. Composes search + node + callers + callees and returns entry points, related symbols, and key code in **one call**. Usually sufficient to answer without further tools.

```
codegraph_context(task: "how does the MasterTimer thread process running functions")
```

**Note:** Provides CODE context, not product requirements. Still clarify UX/edge cases with the user.

#### `codegraph_files` — File structure explorer

**Required** for file/folder exploration. Returns a tree view of all indexed files with metadata (language, symbol count). Much faster than glob/find.

```
codegraph_files(path: "mcp/tools", pattern: "*.cpp")
```

Use this FIRST when exploring project structure. Only fall back to glob for non-indexed files (configs, resources, etc.).

#### `codegraph_explore` — Multi-symbol source viewer

Returns source for several related symbols grouped by file in one capped call. **Strongly prefer** over chaining multiple `codegraph_node` or `view` calls — each separate call re-reads the whole context, so 8 node calls cost far more than 1 explore.

```
codegraph_explore(query: "execOnMainThread validateFields tool_registry")
```

**Budget:** At most **2 calls** per task for this project (1,530 files indexed). Use after `codegraph_context` when you need actual source of surfaced symbols.

**Important:** Query with specific symbol/file/code terms, NOT natural-language sentences. Run `codegraph_search` first to find names if needed.

#### `codegraph_search` — Quick symbol search

Returns locations only (no code). Use for targeted symbol lookups when you already know what you're looking for.

```
codegraph_search(query: "VCBridge", kind: "class")
```

Supports kind filters: `function`, `method`, `class`, `interface`, `type`, `variable`, `route`, `component`.

#### `codegraph_node` — Single symbol detail

Get location, signature, docstring for one symbol. Set `includeCode: true` for source — a function returns its body; a class returns a compact member outline (fields + signatures + line numbers).

```
codegraph_node(symbol: "execOnMainThread", includeCode: true)
```

**Avoid chaining** multiple node calls. Use `codegraph_explore` for several related symbols instead.

#### `codegraph_callers` / `codegraph_callees` — Call graph

Find what calls a symbol (callers) or what a symbol calls (callees). Essential for understanding dependencies and change impact.

```
codegraph_callers(symbol: "Doc::addFixture")
codegraph_callees(symbol: "registerMcpTools")
```

#### `codegraph_impact` — Change impact analysis

Analyze what code could be affected by modifying a symbol. Traverses dependency edges to the specified depth (default: 2).

```
codegraph_impact(symbol: "VCWidget::setCaption", depth: 3)
```

Use before making changes to shared interfaces or base classes.

#### `codegraph_status` — Index health

Check index statistics (files, nodes, edges, DB size). Use to verify the index is up to date.

### When to use grep/glob instead

- **Literal text search** across non-code files (configs, XML, YAML, translations)
- **Regex patterns** that codegraph doesn't support (e.g., matching specific string literals)
- **Files not in the index** (build output, generated files, binary resources)
- **Content search** where you need surrounding context lines (`-A`, `-B`, `-C` flags)

### Rules of thumb

- **Answer directly — don't delegate exploration.** For "how does X work" / architecture / trace questions, answer with 2–3 codegraph calls: `codegraph_context` first, then ONE `codegraph_explore` for the source of the symbols it surfaces. Don't spawn a file-reading sub-agent or run a grep + read loop — codegraph already did that work.
- **Trust codegraph results.** They come from a full AST parse. Do NOT re-verify them with grep — that's slower, less accurate, and wastes context.
- **Don't grep first** when looking up a symbol by name. `codegraph_search` is faster and returns kind + location + signature in one call.
- **Index lag:** the file watcher debounces ~500ms behind writes; don't re-query immediately after editing a file in the same turn.

### Anti-patterns

| Don't | Do instead |
|-------|------------|
| Chain 5+ `codegraph_node` calls | One `codegraph_explore` call |
| Use `glob` for project structure | `codegraph_files` |
| Chain `codegraph_search` + `codegraph_node` for context | One `codegraph_context` call |
| Ask codegraph natural-language queries in `explore` | Use specific symbol/file terms |
| Skip `codegraph_context` and go straight to search | Start with `codegraph_context` |
| Use grep to find function definitions | `codegraph_search` with kind filter |
| Re-verify codegraph results with grep | Trust the AST parse |
| Assume codegraph has the answer for string literals | Fall back to grep for text search |
| Spawn explore sub-agents for architecture questions | Answer directly with 2–3 codegraph calls |

## QLC+ MCP Tools

The QLC+ MCP server (`localhost:9696/mcp`) exposes ~47 tools for live workspace management. See `CLAUDE.md` and `.github/copilot-instructions.md` for the full coding patterns and architecture guide.

Key categories:
- **Query tools**: fixtures, functions, pages, widgets, universes, palettes
- **Create tools**: widgets, functions, fixtures
- **Update tools**: widget properties, function parameters
- **I/O tools**: universe configuration, plugin management
