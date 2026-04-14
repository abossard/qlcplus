#!/usr/bin/env python3
"""
MCP V4 Integration Test Suite

Exercises all MCP tools against a running QLC+ v4 instance via JSON-RPC.
Usage:
    1. Start QLC+ v4:  ./build/main/qlcplus --mcp-port 9696 -d
    2. Run this script: python3 mcp/test/mcp_integration_test.py [--port 9696]
"""

import argparse
import json
import sys
import time
from dataclasses import dataclass, field
from typing import Any, Optional
from urllib.request import Request, urlopen
from urllib.error import URLError


# ── MCP Client ──────────────────────────────────────────────────────────────

class McpClient:
    """Type-safe MCP JSON-RPC client."""

    def __init__(self, base_url: str = "http://127.0.0.1:9696/mcp"):
        self.base_url = base_url
        self.session_id: Optional[str] = None
        self._next_id = 0

    def _rpc(self, method: str, params: dict) -> dict:
        self._next_id += 1
        payload = json.dumps({
            "jsonrpc": "2.0",
            "id": self._next_id,
            "method": method,
            "params": params,
        }).encode()
        headers = {"Content-Type": "application/json"}
        if self.session_id:
            headers["Mcp-Session-Id"] = self.session_id
        req = Request(self.base_url, data=payload, headers=headers, method="POST")
        try:
            resp = urlopen(req, timeout=10)
        except URLError as e:
            if hasattr(e, 'read'):
                body = e.read().decode()
                try:
                    return json.loads(body)
                except json.JSONDecodeError:
                    pass
            raise
        # Capture session ID from response headers
        sid = resp.headers.get("Mcp-Session-Id")
        if sid:
            self.session_id = sid
        return json.loads(resp.read())

    def initialize(self) -> dict:
        return self._rpc("initialize", {
            "protocolVersion": "2024-11-05",
            "capabilities": {},
            "clientInfo": {"name": "integration-test", "version": "1.0"},
        })

    def list_tools(self) -> list[dict]:
        resp = self._rpc("tools/list", {})
        return resp.get("result", {}).get("tools", [])

    def call_tool(self, name: str, arguments: dict) -> Any:
        """Call an MCP tool and return parsed result (or raise on error)."""
        resp = self._rpc("tools/call", {"name": name, "arguments": arguments})
        if "error" in resp:
            raise McpError(f"RPC error: {resp['error']}")
        content = resp.get("result", {}).get("content", [])
        if not content:
            return None
        text = content[0].get("text", "")
        try:
            parsed = json.loads(text)
        except json.JSONDecodeError:
            return text
        # Check for tool-level errors
        if isinstance(parsed, dict) and "error" in parsed:
            raise McpError(parsed["error"])
        if isinstance(parsed, list) and parsed and isinstance(parsed[0], dict) and "error" in parsed[0]:
            raise McpError(parsed[0]["error"])
        return parsed


class McpError(Exception):
    pass


# ── Test Runner ─────────────────────────────────────────────────────────────

@dataclass
class TestResult:
    name: str
    passed: bool
    detail: str = ""

@dataclass
class TestSuite:
    results: list[TestResult] = field(default_factory=list)

    def record(self, name: str, passed: bool, detail: str = ""):
        self.results.append(TestResult(name, passed, detail))

    @property
    def passed(self) -> int:
        return sum(1 for r in self.results if r.passed)

    @property
    def failed(self) -> int:
        return sum(1 for r in self.results if not r.passed)

    def print_report(self):
        print("\n" + "=" * 60)
        print("TEST RESULTS")
        print("=" * 60)
        for r in self.results:
            icon = "✅" if r.passed else "❌"
            line = f"  {icon} {r.name}"
            if r.detail:
                line += f"  ({r.detail})"
            print(line)
        print("=" * 60)
        print(f"  PASS: {self.passed}  FAIL: {self.failed}  TOTAL: {len(self.results)}")
        print("=" * 60)


def run_test(suite: TestSuite, name: str, fn):
    """Run a test function, catch exceptions, record result."""
    try:
        fn()
        suite.record(name, True)
    except McpError as e:
        suite.record(name, False, str(e)[:120])
    except Exception as e:
        suite.record(name, False, f"{type(e).__name__}: {e}"[:120])


# ── Tests ───────────────────────────────────────────────────────────────────

def test_initialize(c: McpClient, s: TestSuite):
    def t():
        resp = c.initialize()
        info = resp.get("result", {}).get("serverInfo", {})
        assert info.get("name") == "qlcplus", f"unexpected server: {info}"
    run_test(s, "initialize", t)


def test_list_tools(c: McpClient, s: TestSuite):
    def t():
        tools = c.list_tools()
        names = [t["name"] for t in tools]
        assert len(names) >= 40, f"expected 40+ tools, got {len(names)}"
        # Spot-check key tools exist
        for expected in ["patch_fixtures", "create_scenes", "vc_create_widgets", "query_fixtures"]:
            assert expected in names, f"missing tool: {expected}"
    run_test(s, "list_tools (50 tools)", t)


def test_patch_fixtures(c: McpClient, s: TestSuite):
    def t():
        result = c.call_tool("patch_fixtures", {"items": [
            {"name": "RGB", "manufacturer": "Generic", "model": "Generic RGB",
             "mode": "RGB", "universe": 0, "address": 0, "quantity": 4},
        ]})
        assert isinstance(result, list), f"expected list, got {type(result)}"
        assert result[0].get("status") in ("created", "patched", "existing"), f"unexpected: {result[0]}"
    run_test(s, "patch_fixtures (4 RGB)", t)


def test_query_fixtures(c: McpClient, s: TestSuite):
    def t():
        result = c.call_tool("query_fixtures", {})
        assert isinstance(result, list), f"expected list, got {type(result)}"
        assert len(result) >= 1, f"expected 1+ fixtures, got {len(result)}"
    run_test(s, "query_fixtures", t)


def test_query_fixture_channels(c: McpClient, s: TestSuite):
    def t():
        result = c.call_tool("query_fixture_channels", {"fixtureIDs": [0]})
        assert isinstance(result, list), f"expected list"
        assert len(result) >= 1, "expected at least 1 channel"
    run_test(s, "query_fixture_channels", t)


def test_create_scenes(c: McpClient, s: TestSuite):
    def t():
        # Use channel 0 (Red) from RGB fixtures
        result = c.call_tool("create_scenes", {"items": [
            {"name": "All Red", "channelValues": [
                {"fixtureID": 0, "channel": 0, "value": 255},
                {"fixtureID": 1, "channel": 0, "value": 255},
                {"fixtureID": 2, "channel": 0, "value": 255},
                {"fixtureID": 3, "channel": 0, "value": 255},
            ]},
            {"name": "All Blue", "channelValues": [
                {"fixtureID": 0, "channel": 2, "value": 255},
                {"fixtureID": 1, "channel": 2, "value": 255},
                {"fixtureID": 2, "channel": 2, "value": 255},
                {"fixtureID": 3, "channel": 2, "value": 255},
            ]},
            {"name": "All Off", "channelValues": [
                {"fixtureID": 0, "channel": 0, "value": 0},
                {"fixtureID": 1, "channel": 0, "value": 0},
                {"fixtureID": 2, "channel": 0, "value": 0},
                {"fixtureID": 3, "channel": 0, "value": 0},
            ]},
        ]})
        assert len(result) == 3, f"expected 3 scenes, got {len(result)}"
        for r in result:
            assert r.get("status") in ("created", "updated", "existing"), f"unexpected: {r}"
    run_test(s, "create_scenes (3 scenes)", t)


def test_create_chasers(c: McpClient, s: TestSuite):
    def t():
        result = c.call_tool("create_chasers", {"items": [
            {"name": "RGB Chase", "steps": ["All Red", "All Blue", "All Off"]},
        ]})
        assert result[0].get("status") in ("created", "updated", "existing"), f"unexpected: {result[0]}"
    run_test(s, "create_chasers", t)


def test_create_collections(c: McpClient, s: TestSuite):
    def t():
        result = c.call_tool("create_collections", {"items": [
            {"name": "All On", "functionNames": ["All Red"]},
        ]})
        assert result[0].get("status") in ("created", "updated", "existing"), f"unexpected: {result[0]}"
    run_test(s, "create_collections", t)


def test_query_functions(c: McpClient, s: TestSuite):
    def t():
        result = c.call_tool("query_functions", {})
        assert isinstance(result, list)
        names = [f.get("name") for f in result]
        assert "All Red" in names, f"missing scene 'All Red'"
        assert "RGB Chase" in names, f"missing chaser"
    run_test(s, "query_functions", t)


def test_query_universes(c: McpClient, s: TestSuite):
    def t():
        result = c.call_tool("query_universes", {})
        assert isinstance(result, list)
        assert len(result) >= 1
    run_test(s, "query_universes", t)


def test_configure_universes(c: McpClient, s: TestSuite):
    def t():
        result = c.call_tool("configure_universes", {"items": [
            {"universeID": 0, "name": "Main Stage"},
        ]})
    run_test(s, "configure_universes", t)


def test_create_palettes(c: McpClient, s: TestSuite):
    def t():
        result = c.call_tool("create_palettes", {"items": [
            {"name": "Red Wash", "type": "Color", "rgb": "#FF0000"},
        ]})
        assert result[0].get("status") in ("created", "updated", "existing")
    run_test(s, "create_palettes", t)


def test_delete_functions(c: McpClient, s: TestSuite):
    def t():
        # Create a throwaway, then delete it
        created = c.call_tool("create_scenes", {"items": [
            {"name": "Throwaway", "channelValues": [
                {"fixtureID": 0, "channel": 0, "value": 1},
            ]},
        ]})
        fid = created[0].get("id")
        assert fid is not None, "no ID returned"
        result = c.call_tool("delete_functions", {"ids": [fid]})
        assert result[0].get("status") == "deleted"
    run_test(s, "delete_functions", t)


# ── VC Tools ────────────────────────────────────────────────────────────────

def test_vc_create_pages(c: McpClient, s: TestSuite):
    def t():
        result = c.call_tool("vc_create_pages", {"items": [
            {"name": "MCP Test Page"},
        ]})
        assert result[0].get("status") in ("created", "existing")
    run_test(s, "vc_create_pages", t)


def test_vc_query_pages(c: McpClient, s: TestSuite):
    def t():
        result = c.call_tool("vc_query_pages", {})
        assert isinstance(result, (list, dict))
    run_test(s, "vc_query_pages", t)


def test_vc_create_button(c: McpClient, s: TestSuite):
    def t():
        result = c.call_tool("vc_create_widgets", {"items": [
            {"type": "button", "parentID": -1,
             "x": 10, "y": 10, "width": 120, "height": 60,
             "caption": "Run Chase", "functionName": "RGB Chase"},
        ]})
        assert result[0].get("status") in ("created", "updated", "existing"), f"unexpected: {result[0]}"
    run_test(s, "vc_create_widgets (button)", t)


def test_vc_create_slider(c: McpClient, s: TestSuite):
    def t():
        result = c.call_tool("vc_create_widgets", {"items": [
            {"type": "slider", "parentID": -1,
             "x": 150, "y": 10, "width": 60, "height": 200,
             "caption": "Master Dim", "mode": "level",
             "channels": [
                 {"fixtureID": 0, "channel": 0},
                 {"fixtureID": 1, "channel": 0},
                 {"fixtureID": 2, "channel": 0},
                 {"fixtureID": 3, "channel": 0},
             ]},
        ]})
        assert result[0].get("status") in ("created", "updated", "existing"), f"unexpected: {result[0]}"
    run_test(s, "vc_create_widgets (slider)", t)


def test_vc_create_frame(c: McpClient, s: TestSuite):
    def t():
        result = c.call_tool("vc_create_widgets", {"items": [
            {"type": "frame", "pageIndex": 0,
             "x": 230, "y": 10, "width": 250, "height": 200,
             "caption": "Controls"},
        ]})
        assert result[0].get("status") in ("created", "updated", "existing")
    run_test(s, "vc_create_widgets (frame)", t)


def test_vc_create_label(c: McpClient, s: TestSuite):
    def t():
        result = c.call_tool("vc_create_widgets", {"items": [
            {"type": "label", "parentID": -1,
             "x": 10, "y": 80, "width": 120, "height": 30,
             "caption": "MCP V4 Test"},
        ]})
        assert result[0].get("status") in ("created", "updated", "existing")
    run_test(s, "vc_create_widgets (label)", t)


def test_vc_create_cuelist(c: McpClient, s: TestSuite):
    def t():
        result = c.call_tool("vc_create_widgets", {"items": [
            {"type": "cuelist", "parentID": -1,
             "x": 10, "y": 230, "width": 200, "height": 150,
             "caption": "My CueList", "chaserName": "RGB Chase"},
        ]})
        assert result[0].get("status") in ("created", "updated", "existing")
    run_test(s, "vc_create_widgets (cuelist)", t)


def test_vc_create_speeddial(c: McpClient, s: TestSuite):
    def t():
        # Get function ID for RGB Chase
        funcs = c.call_tool("query_functions", {})
        chase_id = next((f["id"] for f in funcs if f.get("name") == "RGB Chase"), None)
        fids = [chase_id] if chase_id is not None else []
        result = c.call_tool("vc_create_widgets", {"items": [
            {"type": "speedDial", "parentID": -1,
             "x": 500, "y": 10, "width": 150, "height": 150,
             "caption": "Speed", "functionIDs": fids},
        ]})
        assert result[0].get("status") in ("created", "updated", "existing")
    run_test(s, "vc_create_widgets (speedDial)", t)


def test_vc_create_clock(c: McpClient, s: TestSuite):
    def t():
        result = c.call_tool("vc_create_widgets", {"items": [
            {"type": "clock", "parentID": -1,
             "x": 500, "y": 170, "width": 150, "height": 50,
             "caption": "Clock"},
        ]})
        assert result[0].get("status") in ("created", "updated", "existing")
    run_test(s, "vc_create_widgets (clock)", t)


def _find_widget_id(pages, widget_type):
    """Recursively find a widget ID by type from vc_query_pages result."""
    if not isinstance(pages, list):
        return None
    for page in pages:
        for w in page.get("widgets", []):
            if w.get("type", "").lower() == widget_type.lower():
                return w.get("id")
            for child in w.get("children", w.get("widgets", [])):
                if child.get("type", "").lower() == widget_type.lower():
                    return child.get("id")
    return None


def test_vc_query_widgets(c: McpClient, s: TestSuite):
    def t():
        pages = c.call_tool("vc_query_pages", {})
        widget_ids = []
        if isinstance(pages, list):
            for page in pages:
                for w in page.get("widgets", []):
                    widget_ids.append(w.get("id"))
        assert len(widget_ids) > 0, f"no widgets found in pages"
        result = c.call_tool("vc_query_widgets", {"widgetIDs": widget_ids[:10]})
        assert isinstance(result, list), f"expected list, got {type(result)}"
    run_test(s, "vc_query_widgets", t)


def test_vc_update_widgets(c: McpClient, s: TestSuite):
    def t():
        pages = c.call_tool("vc_query_pages", {})
        button_id = _find_widget_id(pages, "button")
        assert button_id is not None, f"no button widget found. Pages: {json.dumps(pages)[:300]}"
        result = c.call_tool("vc_update_widgets", {"items": [
            {"widgetID": button_id, "caption": "🎵 Chase!", "bgColor": "#003366"},
        ]})
        # Response can be {"status": "ok"} or {"changes": [...]}
        r = result[0]
        ok = r.get("status") in ("updated", "ok") or "changes" in r
        assert ok, f"unexpected: {r}"
    run_test(s, "vc_update_widgets (caption+color)", t)


def test_vc_detect_overlaps(c: McpClient, s: TestSuite):
    def t():
        result = c.call_tool("vc_detect_overlaps", {"pageIndex": 0})
        # Result can be empty list (no overlaps) or list of overlaps
        assert isinstance(result, (list, dict))
    run_test(s, "vc_detect_overlaps", t)


def test_vc_delete_widgets(c: McpClient, s: TestSuite):
    def t():
        created = c.call_tool("vc_create_widgets", {"items": [
            {"type": "label", "parentID": -1,
             "x": 700, "y": 700, "width": 80, "height": 30,
             "caption": "DELETE ME"},
        ]})
        wid = created[0].get("id") or created[0].get("widgetID")
        assert wid is not None and wid >= 0, f"no valid widget ID returned: {created}"
        result = c.call_tool("vc_delete_widgets", {"ids": [wid]})
    run_test(s, "vc_delete_widgets", t)


def test_vc_set_key_sequences(c: McpClient, s: TestSuite):
    def t():
        pages = c.call_tool("vc_query_pages", {})
        button_id = _find_widget_id(pages, "button")
        assert button_id is not None, f"no button found. Pages: {json.dumps(pages)[:300]}"
        result = c.call_tool("vc_set_key_sequences", {"items": [
            {"widgetID": button_id, "keySequence": "Space"},
        ]})
    run_test(s, "vc_set_key_sequences (Space→button)", t)


# ── Main ────────────────────────────────────────────────────────────────────

def wait_for_server(url: str, timeout: int = 30):
    """Poll until MCP server responds."""
    from urllib.error import HTTPError
    print(f"Connecting to {url} ", end="", flush=True)
    for _ in range(timeout):
        try:
            req = Request(url, data=b'{}', headers={"Content-Type": "application/json"}, method="POST")
            urlopen(req, timeout=2)
            print(" connected!")
            return True
        except HTTPError:
            # 400/405 etc = server is up, just rejecting bad request
            print(" connected!")
            return True
        except (URLError, ConnectionRefusedError, OSError):
            print(".", end="", flush=True)
            time.sleep(1)
    print(" TIMEOUT")
    return False


def main():
    parser = argparse.ArgumentParser(description="MCP V4 Integration Tests")
    parser.add_argument("--port", type=int, default=9696, help="MCP server port")
    parser.add_argument("--host", default="127.0.0.1", help="MCP server host")
    args = parser.parse_args()

    url = f"http://{args.host}:{args.port}/mcp"

    if not wait_for_server(url):
        print("ERROR: MCP server not responding. Start QLC+ first:")
        print("  ./build/main/qlcplus --mcp-port 9696 -d")
        sys.exit(1)

    client = McpClient(url)
    suite = TestSuite()

    # Phase 1: Connection
    print("\n── Connection ──")
    test_initialize(client, suite)
    test_list_tools(client, suite)

    # Phase 2: Engine tools
    print("\n── Engine Tools ──")
    test_patch_fixtures(client, suite)
    test_query_fixtures(client, suite)
    test_query_fixture_channels(client, suite)
    test_create_scenes(client, suite)
    test_create_chasers(client, suite)
    test_create_collections(client, suite)
    test_create_palettes(client, suite)
    test_query_functions(client, suite)
    test_query_universes(client, suite)
    test_configure_universes(client, suite)
    test_delete_functions(client, suite)

    # Phase 3: VC tools
    print("\n── VC Tools (VCBridgeV4) ──")
    test_vc_create_pages(client, suite)
    test_vc_query_pages(client, suite)
    test_vc_create_button(client, suite)
    test_vc_create_slider(client, suite)
    test_vc_create_frame(client, suite)
    test_vc_create_label(client, suite)
    test_vc_create_cuelist(client, suite)
    test_vc_create_speeddial(client, suite)
    test_vc_create_clock(client, suite)
    test_vc_query_widgets(client, suite)
    test_vc_update_widgets(client, suite)
    test_vc_detect_overlaps(client, suite)
    test_vc_delete_widgets(client, suite)
    test_vc_set_key_sequences(client, suite)

    # Report
    suite.print_report()

    if suite.failed > 0:
        print("\n⚠️  Some tests failed. Check the app console for crash logs.")
        print("   The app should still be running — check Virtual Console for created widgets.")
    else:
        print("\n🎉 All tests passed!")
        print("   Now check the QLC+ Virtual Console tab:")
        print("   - You should see: Button, Slider, Frame, Label, CueList, SpeedDial, Clock")
        print("   - Switch to Operate mode (Ctrl+F12) and click 'Run Chase' button")
        print("   - Press Space to trigger the button via keyboard shortcut")

    sys.exit(1 if suite.failed > 0 else 0)


if __name__ == "__main__":
    main()
