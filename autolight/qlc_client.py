"""
AutoLight — QLC+ MCP client library for iterative LED effect research.

Thin async wrapper around the QLC+ MCP server (http://127.0.0.1:9696/mcp).
Every method maps 1:1 to an MCP tool call, returning parsed JSON.

Usage:
    async with QLC() as q:
        fixtures = await q.call("query_fixtures")
        await q.call("create_palettes", items=[...])
"""

import json
from mcp import ClientSession
from mcp.client.streamable_http import streamablehttp_client

MCP_URL = "http://127.0.0.1:9696/mcp"


class QLC:
    """Async context-managed QLC+ MCP client."""

    def __init__(self, url: str = MCP_URL):
        self.url = url
        self._session: ClientSession | None = None

    async def __aenter__(self):
        self._http_ctx = streamablehttp_client(self.url)
        read, write, _ = await self._http_ctx.__aenter__()
        self._session_ctx = ClientSession(read, write)
        self._session = await self._session_ctx.__aenter__()
        await self._session.initialize()
        return self

    async def __aexit__(self, *exc):
        try:
            await self._session_ctx.__aexit__(*exc)
        except Exception:
            pass
        try:
            await self._http_ctx.__aexit__(*exc)
        except Exception:
            pass

    async def call(self, tool: str, **kwargs) -> any:
        """Call an MCP tool. kwargs become the arguments dict. Returns parsed JSON."""
        result = await self._session.call_tool(tool, kwargs if kwargs else {})
        text = result.content[0].text
        try:
            return json.loads(text)
        except json.JSONDecodeError:
            return text
