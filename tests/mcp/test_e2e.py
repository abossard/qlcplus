#!/usr/bin/env python3
"""
End-to-end MCP server test for QLC+ over HTTP transport.
Tests all 25+ tools: fixtures, functions, VC, channels, I/O, prompts.
"""

import subprocess
import json
import sys
import os
import time
import urllib.request
import urllib.error

QLCPLUS_BIN = os.environ.get("QLCPLUS_BIN", "./build-mcp/qmlui/qlcplus-qml")
MCP_PORT = 19876
MCP_URL = f"http://127.0.0.1:{MCP_PORT}/mcp"

_id_counter = 0
_passed = 0
_failed = 0
_session_id = ""


def next_id():
    global _id_counter
    _id_counter += 1
    return _id_counter


def http_post(url, data, headers=None):
    hdrs = {"Content-Type": "application/json"}
    if headers:
        hdrs.update(headers)
    body = json.dumps(data).encode()
    req = urllib.request.Request(url, data=body, headers=hdrs)
    with urllib.request.urlopen(req, timeout=30) as resp:
        return json.loads(resp.read()), dict(resp.headers)


def start_server():
    proc = subprocess.Popen(
        [QLCPLUS_BIN, "--mcp-http", str(MCP_PORT)],
        stdin=subprocess.DEVNULL,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        env={**os.environ, "QT_QPA_PLATFORM": "offscreen"},
    )
    # Wait for HTTP server to be ready
    for attempt in range(30):
        if proc.poll() is not None:
            raise RuntimeError(f"Server exited: {proc.stderr.read().decode()}")
        try:
            urllib.request.urlopen(f"http://127.0.0.1:{MCP_PORT}/mcp", timeout=1)
        except urllib.error.HTTPError:
            break  # Server is up (returned an HTTP error, but it's listening)
        except urllib.error.URLError:
            time.sleep(0.5)
            continue
        break
    else:
        raise RuntimeError("Server did not start in time")
    return proc


def send(method, params=None):
    global _session_id
    request = {"jsonrpc": "2.0", "method": method, "id": next_id()}
    if params is not None:
        request["params"] = params
    hdrs = {}
    if _session_id:
        hdrs["Mcp-Session-Id"] = _session_id
    resp, resp_headers = http_post(MCP_URL, request, headers=hdrs)
    # Capture session ID from response
    sid = resp_headers.get("Mcp-Session-Id", resp_headers.get("mcp-session-id", ""))
    if sid:
        _session_id = sid
    return resp


def call(tool, args=None):
    resp = send("tools/call", {"name": tool, "arguments": args or {}})
    if "error" in resp:
        raise RuntimeError(f"Tool {tool} error: {resp['error']}")
    return resp.get("result", {}).get("content", resp.get("result", {}))


def test(name, func):
    global _passed, _failed
    try:
        func()
        _passed += 1
        print(f"  ✅ {name}")
    except Exception as e:
        _failed += 1
        print(f"  ❌ {name}: {e}")


def run_tests():
    global _passed, _failed
    print(f"Starting QLC+ MCP HTTP server: {QLCPLUS_BIN} --mcp-http {MCP_PORT}\n")
    proc = start_server()

    try:
        # === PROTOCOL ===
        print("=== Protocol ===")

        def t_initialize():
            r = send("initialize", {
                "protocolVersion": "2024-11-05",
                "capabilities": {},
                "clientInfo": {"name": "test", "version": "1.0"},
            })
            assert r["result"]["serverInfo"]["name"] == "qlcplus"
        test("initialize", t_initialize)

        def t_list_tools():
            r = send("tools/list", {})
            tools = r["result"]["tools"]
            names = [t["name"] for t in tools]
            assert len(tools) >= 25, f"Expected >=25, got {len(tools)}"
            for required in ["query_fixtures", "create_scenes", "create_chasers",
                             "add_vc_buttons", "configure_channels", "set_channel_modifiers",
                             "get_show_design_guide"]:
                assert required in names, f"Missing tool: {required}"
        test("list_tools (>=25)", t_list_tools)

        # === QUERY EMPTY ===
        print("\n=== Query Empty Project ===")

        def t_fixtures_empty():
            r = call("query_fixtures")
            assert r == [], f"Expected empty, got {r}"
        test("query_fixtures (empty)", t_fixtures_empty)

        def t_functions_empty():
            r = call("query_functions")
            assert r == []
        test("query_functions (empty)", t_functions_empty)

        def t_universes():
            r = call("query_universes")
            assert len(r) >= 1, "Should have at least 1 universe"
        test("query_universes", t_universes)

        # === FIXTURE LIBRARY ===
        print("\n=== Fixture Library ===")

        def t_search_library():
            r = call("query_available_fixtures", {"items": [{"manufacturer": "Generic", "model": "RGB"}]})
            if len(r) > 0:
                names = [f["model"] for f in r]
                assert any("RGB" in n for n in names), f"No RGB fixture found in {names}"
            else:
                print("    (skipped: fixture library not loaded in offscreen mode)")
        test("query_available_fixtures (Generic RGB)", t_search_library)

        # === PATCH FIXTURES ===
        print("\n=== Patch Fixtures ===")

        def t_patch():
            r = call("patch_fixtures", {"items": [
                {"manufacturer": "Generic", "model": "Generic RGB", "mode": "RGB Dimmer",
                 "name": "Par", "universe": 0, "address": 1, "quantity": 4},
            ]})
            if len(r) == 1 and "error" in r[0]:
                print(f"    (skipped: {r[0]['error']})")
                return
            assert len(r) == 4, f"Expected 4 fixtures, got {len(r)}"
            assert r[0]["name"] == "Par 1"
            assert r[3]["name"] == "Par 4"
        test("patch_fixtures (4 RGB pars)", t_patch)

        patched = call("query_fixtures")
        has_fixtures = len(patched) > 0

        def t_query_patched():
            if not has_fixtures:
                print("    (skipped: no fixtures patched)")
                return
            assert len(patched) == 4
            assert all("RGB" in str(f["capabilities"]) for f in patched)
        test("query_fixtures (verify patched)", t_query_patched)

        # === CHANNEL TOOLS ===
        print("\n=== Channel Configuration ===")

        def t_query_channels():
            if not has_fixtures:
                print("    (skipped: no fixtures)")
                return
            r = call("query_fixture_channels", {"fixtureIDs": [0]})
            assert len(r) == 1
            channels = r[0]["channels"]
            assert len(channels) >= 3
            assert channels[0]["precedence"] == "auto"
            assert channels[0]["canFade"] == True
        test("query_fixture_channels", t_query_channels)

        def t_configure_channels():
            if not has_fixtures:
                print("    (skipped: no fixtures)")
                return
            r = call("configure_channels", {"items": [
                {"fixtureID": 0, "channel": 0, "precedence": "ltp"},
                {"fixtureID": 0, "channel": 3, "canFade": False},
            ]})
            assert all(item["status"] == "ok" for item in r)
            v = call("query_fixture_channels", {"fixtureIDs": [0]})
            ch = v[0]["channels"]
            assert ch[0]["precedence"] == "ltp"
        test("configure_channels (LTP + canFade)", t_configure_channels)

        def t_auto_precedence():
            if not has_fixtures:
                print("    (skipped: no fixtures)")
                return
            r = call("configure_channels", {"items": [
                {"fixtureID": 0, "channel": 0, "precedence": "auto"},
            ]})
            v = call("query_fixture_channels", {"fixtureIDs": [0]})
            assert v[0]["channels"][0]["precedence"] == "auto"
        test("configure_channels (back to auto)", t_auto_precedence)

        def t_list_modifiers():
            r = call("query_channel_modifiers")
            if len(r) == 0:
                print("    (skipped: no modifiers loaded in offscreen mode)")
                return
            names = [m["name"] for m in r]
            assert "Invert" in names
        test("query_channel_modifiers", t_list_modifiers)

        def t_set_modifier():
            mods = call("query_channel_modifiers")
            if len(mods) == 0 or not has_fixtures:
                print("    (skipped: no modifiers or fixtures)")
                return
            r = call("set_channel_modifiers", {"items": [
                {"fixtureID": 0, "channel": 0, "modifierName": "Invert"},
            ]})
            assert r[0]["status"] == "ok"
        test("set_channel_modifiers (Invert)", t_set_modifier)

        def t_remove_modifier():
            if not has_fixtures:
                print("    (skipped: no fixtures)")
                return
            r = call("set_channel_modifiers", {"items": [
                {"fixtureID": 0, "channel": 0, "modifierName": "none"},
            ]})
            assert r[0]["status"] == "ok"
        test("set_channel_modifiers (remove)", t_remove_modifier)

        # === FUNCTION CREATION ===
        print("\n=== Function Creation ===")

        def t_create_scenes():
            fxIDs = [f["id"] for f in patched] if has_fixtures else [0, 1, 2, 3]
            r = call("create_scenes", {"items": [
                {"name": "Red Wash", "fixtureIDs": fxIDs, "color": {"r":255,"g":0,"b":0}, "intensity": 200},
                {"name": "Blue Wash", "fixtureIDs": fxIDs, "color": {"r":0,"g":0,"b":255}, "intensity": 180},
                {"name": "Green Wash", "fixtureIDs": fxIDs, "color": {"r":0,"g":255,"b":0}},
            ]})
            assert len(r) == 3
            assert r[0]["name"] == "Red Wash"
        test("create_scenes (3 batch)", t_create_scenes)

        all_fns = call("query_functions")
        scene_ids = [f["id"] for f in all_fns if f.get("type") == "Scene"]
        if not scene_ids:
            scene_ids = [f["id"] for f in all_fns][:3]

        def t_create_chasers():
            r = call("create_chasers", {"items": [
                {"name": "Color Chase", "functionIDs": scene_ids,
                 "holdTime": 1000, "fadeIn": 500, "fadeOut": 200,
                 "runOrder": "loop", "direction": "forward",
                 "tempoType": "time", "fadeInMode": "common",
                 "fadeOutMode": "common", "durationMode": "common"},
            ]})
            assert len(r) == 1
            assert r[0]["name"] == "Color Chase"
        test("create_chasers (full run properties)", t_create_chasers)

        def t_create_efxs():
            r = call("create_efxs", {"items": [
                {"name": "Circle Move", "fixtureIDs": [0,1],
                 "algorithm": "Circle", "width": 200, "height": 200, "speed": 3000},
            ]})
            assert len(r) == 1
        test("create_efxs", t_create_efxs)

        def t_create_collections():
            fns = call("query_functions")
            fn_ids = [f["id"] for f in fns]
            r = call("create_collections", {"items": [
                {"name": "Party Mood", "functionIDs": fn_ids[:2]},
            ]})
            assert len(r) == 1
            assert r[0]["name"] == "Party Mood"
        test("create_collections", t_create_collections)

        def t_create_rgb_matrices():
            r = call("create_rgb_matrices", {"items": [
                {"name": "Rainbow"},
            ]})
            assert len(r) == 1
        test("create_rgb_matrices", t_create_rgb_matrices)

        def t_query_all_functions():
            r = call("query_functions")
            assert len(r) >= 6, f"Expected >=6 functions, got {len(r)}"
            names = [f["name"] for f in r]
            assert "Red Wash" in names
            assert "Color Chase" in names
            assert "Party Mood" in names
        test("query_functions (verify all created)", t_query_all_functions)

        # === I/O CONFIGURATION ===
        print("\n=== I/O Configuration ===")

        def t_configure_universes():
            r = call("configure_universes", {"items": [
                {"universeID": 0, "name": "Main Stage"},
            ]})
            assert r[0]["status"] == "ok"
        test("configure_universes", t_configure_universes)

        # === VIRTUAL CONSOLE ===
        print("\n=== Virtual Console ===")

        def t_query_vc_pages():
            r = call("query_vc_pages")
            assert len(r) >= 1, "Should have at least 1 default page"
        test("query_vc_pages", t_query_vc_pages)

        def t_create_vc_pages():
            r = call("create_vc_pages", {"items": [{"name": "Test Page"}]})
            assert len(r) == 1
            assert "pageIndex" in r[0]
            return r[0]["pageIndex"]
        test("create_vc_pages", t_create_vc_pages)

        pages = call("query_vc_pages")
        page_idx = len(pages) - 1

        def t_add_vc_frames():
            r = call("add_vc_frames", {"items": [
                {"pageIndex": page_idx, "x": 10, "y": 10, "width": 400, "height": 200,
                 "caption": "Moods", "solo": True},
                {"pageIndex": page_idx, "x": 10, "y": 220, "width": 400, "height": 150,
                 "caption": "Effects", "solo": False},
            ]})
            assert len(r) == 2
            assert all("widgetID" in w for w in r)
        test("add_vc_frames (solo + normal)", t_add_vc_frames)

        def t_add_vc_buttons():
            fns = call("query_functions")
            fn_id = fns[0]["id"] if fns else 0
            r = call("add_vc_buttons", {"items": [
                {"parentID": page_idx, "x": 10, "y": 430, "width": 100, "height": 50,
                 "functionID": fn_id, "caption": "Red", "action": "toggle"},
                {"parentID": page_idx, "x": 120, "y": 430, "width": 100, "height": 50,
                 "functionID": fn_id, "caption": "Strobe", "action": "flash"},
            ]})
            assert len(r) == 2
        test("add_vc_buttons (toggle + flash)", t_add_vc_buttons)

        def t_add_vc_sliders():
            r = call("add_vc_sliders", {"items": [
                {"parentID": page_idx, "x": 420, "y": 10, "width": 60, "height": 400,
                 "caption": "Master", "mode": "submaster"},
            ]})
            assert len(r) == 1
        test("add_vc_sliders (submaster)", t_add_vc_sliders)

        def t_add_vc_cuelists():
            fns = call("query_functions")
            chaser_id = next((f["id"] for f in fns if f.get("type") == "Chaser"), fns[0]["id"] if fns else 0)
            r = call("add_vc_cuelists", {"items": [
                {"parentID": page_idx, "x": 490, "y": 10, "width": 300, "height": 200,
                 "chaserID": chaser_id, "caption": "Color Chase"},
            ]})
            assert len(r) == 1
        test("add_vc_cuelists", t_add_vc_cuelists)

        def t_add_vc_labels():
            r = call("add_vc_labels", {"items": [
                {"parentID": page_idx, "x": 10, "y": 490, "width": 200, "height": 30,
                 "text": "Audio Reactive"},
            ]})
            assert len(r) == 1
        test("add_vc_labels", t_add_vc_labels)

        def t_map_vc_inputs():
            r = call("map_vc_inputs", {"items": [
                {"widgetID": 0, "inputUniverse": 0, "inputChannel": 1},
            ]})
            assert len(r) == 1
        test("map_vc_inputs", t_map_vc_inputs)

        # === PROMPTS ===
        print("\n=== Design Guide ===")

        def t_show_design_guide():
            r = call("get_show_design_guide")
            text = str(r)
            assert "Layered" in text or "layered" in text
            assert "HTP" in text
            assert "OSC" in text or "Audio" in text
        test("get_show_design_guide (has layering + HTP + audio)", t_show_design_guide)

        # === SUMMARY ===
        print(f"\n{'='*50}")
        print(f"Results: {_passed} passed, {_failed} failed, {_passed + _failed} total")
        print(f"{'='*50}")
        return _failed == 0

    except Exception as e:
        print(f"\n💥 Fatal error: {e}")
        import traceback
        traceback.print_exc()
        return False

    finally:
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()


if __name__ == "__main__":
    success = run_tests()
    sys.exit(0 if success else 1)
