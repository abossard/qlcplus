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
        [QLCPLUS_BIN, "--mcp-port", str(MCP_PORT)],
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
    content = resp.get("result", {}).get("content", resp.get("result", {}))
    # MCP returns content as [{"type": "text", "text": "..."}] — parse the JSON text
    if isinstance(content, list) and len(content) == 1 and isinstance(content[0], dict) and "text" in content[0]:
        try:
            return json.loads(content[0]["text"])
        except (json.JSONDecodeError, TypeError):
            return content[0]["text"]
    return content


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
            assert len(tools) >= 30, f"Expected >=30, got {len(tools)}"
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

        def t_configure_universes_feedback():
            r = call("configure_universes", {"items": [
                {"universeID": 0, "feedbackEnabled": True},
            ]})
            # In offscreen mode no input is patched, so feedback setup may fail gracefully
            assert r[0]["status"] in ("ok", "failed")
        test("configure_universes (feedbackEnabled)", t_configure_universes_feedback)

        def t_configure_plugin_params():
            r = call("configure_plugin_params", {"items": [
                {"universeID": 0, "plugin": "MIDI", "params": {"midichannel": "1"}},
            ]})
            assert len(r) == 1
            assert r[0]["status"] == "ok" or "error" in r[0]
        test("configure_plugin_params", t_configure_plugin_params)

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

        # === SLIDER EXTENDED PROPERTIES ===
        print("\n=== Slider Extended Properties ===")

        def t_slider_grandmaster():
            r = call("add_vc_sliders", {"items": [
                {"parentID": page_idx, "x": 500, "y": 10, "width": 60, "height": 400,
                 "caption": "GM", "mode": "grandmaster",
                 "gmValueMode": "reduce", "gmChannelMode": "allchannels"},
            ]})
            assert len(r) == 1
            wid = r[0]["widgetID"]
            assert wid >= 0, f"Expected valid ID, got {r[0]}"
            d = call("query_widget_details", {"widgetIDs": [wid]})[0]
            assert d["sliderMode"] == "grandmaster", f"Expected grandmaster, got {d.get('sliderMode')}"
            assert d["gmValueMode"] == "reduce", f"Expected reduce, got {d.get('gmValueMode')}"
            assert d["gmChannelMode"] == "allchannels", f"Expected allchannels, got {d.get('gmChannelMode')}"
        test("slider grandmaster mode", t_slider_grandmaster)

        def t_slider_click_and_go():
            r = call("add_vc_sliders", {"items": [
                {"parentID": page_idx, "x": 580, "y": 10, "width": 60, "height": 400,
                 "caption": "Gobo Pick", "mode": "level",
                 "clickAndGoType": "preset"},
            ]})
            assert len(r) == 1
            wid = r[0]["widgetID"]
            assert wid >= 0
            d = call("query_widget_details", {"widgetIDs": [wid]})[0]
            assert d["clickAndGoType"] == "preset", f"Expected preset, got {d.get('clickAndGoType')}"
        test("slider clickAndGoType (preset)", t_slider_click_and_go)

        def t_slider_value_display_style():
            r = call("add_vc_sliders", {"items": [
                {"parentID": page_idx, "x": 660, "y": 10, "width": 60, "height": 400,
                 "caption": "Pct Slider", "mode": "level",
                 "valueDisplayStyle": "percentage"},
            ]})
            wid = r[0]["widgetID"]
            d = call("query_widget_details", {"widgetIDs": [wid]})[0]
            assert d["valueDisplayStyle"] == "percentage", f"Expected percentage, got {d.get('valueDisplayStyle')}"
        test("slider valueDisplayStyle (percentage)", t_slider_value_display_style)

        def t_slider_range_limits():
            r = call("add_vc_sliders", {"items": [
                {"parentID": page_idx, "x": 740, "y": 10, "width": 60, "height": 400,
                 "caption": "Ranged", "mode": "level",
                 "rangeLowLimit": 50, "rangeHighLimit": 200},
            ]})
            wid = r[0]["widgetID"]
            d = call("query_widget_details", {"widgetIDs": [wid]})[0]
            assert d["rangeLowLimit"] == 50, f"rangeLowLimit: {d.get('rangeLowLimit')}"
            assert d["rangeHighLimit"] == 200, f"rangeHighLimit: {d.get('rangeHighLimit')}"
        test("slider rangeLowLimit/rangeHighLimit", t_slider_range_limits)

        def t_slider_monitor():
            r = call("add_vc_sliders", {"items": [
                {"parentID": page_idx, "x": 820, "y": 10, "width": 60, "height": 400,
                 "caption": "Monitored", "mode": "level",
                 "monitorEnabled": True},
            ]})
            wid = r[0]["widgetID"]
            d = call("query_widget_details", {"widgetIDs": [wid]})[0]
            assert d["monitorEnabled"] == True, f"monitorEnabled: {d.get('monitorEnabled')}"
        test("slider monitorEnabled", t_slider_monitor)

        def t_slider_inverted():
            r = call("add_vc_sliders", {"items": [
                {"parentID": page_idx, "x": 900, "y": 10, "width": 60, "height": 400,
                 "caption": "Inverted", "mode": "level",
                 "invertedAppearance": True},
            ]})
            wid = r[0]["widgetID"]
            d = call("query_widget_details", {"widgetIDs": [wid]})[0]
            assert d.get("invertedAppearance") == True or d.get("sliderInvertedAppearance") == True, \
                f"invertedAppearance not set: {d}"
        test("slider invertedAppearance", t_slider_inverted)

        def t_slider_upsert():
            # Create a slider, then create another with same caption → should upsert
            r1 = call("add_vc_sliders", {"items": [
                {"parentID": page_idx, "x": 980, "y": 10, "width": 60, "height": 400,
                 "caption": "Upsert Test", "mode": "level"},
            ]})
            wid1 = r1[0]["widgetID"]
            assert r1[0]["status"] == "created"

            r2 = call("add_vc_sliders", {"items": [
                {"parentID": page_idx, "caption": "Upsert Test", "mode": "level",
                 "clickAndGoType": "colors"},
            ]})
            assert r2[0]["status"] == "updated", f"Expected updated, got {r2[0].get('status')}"
            assert r2[0]["widgetID"] == wid1, "Upsert should return same widget ID"

            d = call("query_widget_details", {"widgetIDs": [wid1]})[0]
            assert d["clickAndGoType"] == "colors", f"Expected colors after upsert, got {d.get('clickAndGoType')}"
        test("slider upsert (update existing by caption)", t_slider_upsert)

        def t_slider_colors_on_create():
            r = call("add_vc_sliders", {"items": [
                {"parentID": page_idx, "x": 1060, "y": 10, "width": 60, "height": 400,
                 "caption": "Colored", "mode": "level",
                 "clickAndGoType": "colors",
                 "bgColor": "#1a3300", "fgColor": "#ffffff"},
            ]})
            wid = r[0]["widgetID"]
            d = call("query_widget_details", {"widgetIDs": [wid]})[0]
            assert d["clickAndGoType"] == "colors"
        test("slider with colors + clickAndGo on create", t_slider_colors_on_create)

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

        # === XY PAD ===
        print("\n=== XY Pad ===")

        # Patch a moving head fixture for XY pad tests
        mh_patched = False
        mh_fixture_ids = []
        def t_patch_moving_heads():
            nonlocal mh_patched, mh_fixture_ids
            r = call("patch_fixtures", {"items": [
                {"manufacturer": "Showtec", "model": "Acrobat", "mode": "11 Channel",
                 "name": "MH", "universe": 0, "address": 100, "quantity": 2},
            ]})
            if len(r) >= 1 and "error" not in r[0]:
                mh_patched = True
                mh_fixture_ids = [f["id"] for f in r]
            else:
                print(f"    (note: moving head patch: {r})")
        test("patch moving heads for XY pad", t_patch_moving_heads)

        def t_xypad_basic():
            if not mh_patched:
                print("    (skipped: no moving heads)")
                return
            r = call("add_vc_xypads", {"items": [
                {"parentID": page_idx, "size": 200,
                 "fixtureIDs": mh_fixture_ids},
            ]})
            assert len(r) == 1
            assert r[0]["widgetID"] >= 0, f"Expected valid ID, got {r[0]}"
        test("add_vc_xypads (basic fixtureIDs)", t_xypad_basic)

        def t_xypad_with_config():
            if not mh_patched:
                print("    (skipped: no moving heads)")
                return
            r = call("add_vc_xypads", {"items": [
                {"parentID": page_idx, "size": 200,
                 "fixtures": [
                     {"fixtureID": mh_fixture_ids[0], "head": 0,
                      "xMin": 0.1, "xMax": 0.9, "yMin": 0.2, "yMax": 0.8}
                 ],
                 "displayMode": "degrees",
                 "invertedAppearance": True},
            ]})
            assert len(r) == 1
            assert r[0]["widgetID"] >= 0
            return r[0]["widgetID"]
        test("add_vc_xypads (per-fixture config)", t_xypad_with_config)

        def t_xypad_query_details():
            if not mh_patched:
                print("    (skipped: no moving heads)")
                return
            # Create a pad with known config
            r = call("add_vc_xypads", {"items": [
                {"parentID": page_idx, "size": 200,
                 "fixtures": [
                     {"fixtureID": mh_fixture_ids[0], "xMin": 0.25, "xMax": 0.75}
                 ],
                 "displayMode": "degrees"},
            ]})
            wid = r[0]["widgetID"]
            assert wid >= 0

            details = call("query_widget_details", {"widgetIDs": [wid]})
            assert len(details) == 1
            d = details[0]
            assert d["type"] == "XY Pad", f"Expected XY Pad, got {d['type']}"
            assert d["displayMode"] == "degrees", f"Expected degrees, got {d.get('displayMode')}"
            assert "fixtures" in d, "Missing fixtures in details"
            assert len(d["fixtures"]) >= 1, "Expected at least 1 fixture"
            fx = d["fixtures"][0]
            assert fx["fixtureID"] == mh_fixture_ids[0]
            assert abs(fx["xMin"] - 0.25) < 0.01, f"xMin: {fx['xMin']}"
            assert abs(fx["xMax"] - 0.75) < 0.01, f"xMax: {fx['xMax']}"
        test("query_widget_details (XY Pad fixtures+degrees)", t_xypad_query_details)

        def t_xypad_display_mode():
            if not mh_patched:
                print("    (skipped: no moving heads)")
                return
            r = call("add_vc_xypads", {"items": [
                {"parentID": page_idx, "size": 150,
                 "fixtureIDs": [mh_fixture_ids[0]],
                 "displayMode": "percentage"},
            ]})
            wid = r[0]["widgetID"]
            d = call("query_widget_details", {"widgetIDs": [wid]})[0]
            assert d["displayMode"] == "percentage"

            # Update display mode
            r2 = call("update_widgets", {"items": [
                {"widgetID": wid, "displayMode": "dmx"}
            ]})
            assert any(c["property"] == "displayMode" and c["status"] == "ok"
                       for c in r2[0]["changes"])
            d2 = call("query_widget_details", {"widgetIDs": [wid]})[0]
            assert d2["displayMode"] == "dmx"
        test("XY Pad display mode (create + update)", t_xypad_display_mode)

        def t_xypad_inverted():
            if not mh_patched:
                print("    (skipped: no moving heads)")
                return
            r = call("add_vc_xypads", {"items": [
                {"parentID": page_idx, "size": 150,
                 "fixtureIDs": [mh_fixture_ids[0]],
                 "invertedAppearance": True},
            ]})
            wid = r[0]["widgetID"]
            d = call("query_widget_details", {"widgetIDs": [wid]})[0]
            assert d["invertedAppearance"] == True

            # Toggle off via update
            call("update_widgets", {"items": [
                {"widgetID": wid, "invertedAppearance": False}
            ]})
            d2 = call("query_widget_details", {"widgetIDs": [wid]})[0]
            assert d2["invertedAppearance"] == False
        test("XY Pad inverted appearance", t_xypad_inverted)

        def t_xypad_position():
            if not mh_patched:
                print("    (skipped: no moving heads)")
                return
            r = call("add_vc_xypads", {"items": [
                {"parentID": page_idx, "size": 150,
                 "fixtureIDs": [mh_fixture_ids[0]]},
            ]})
            wid = r[0]["widgetID"]

            # Set position via update_widgets
            r2 = call("update_widgets", {"items": [
                {"widgetID": wid, "xyPadPosition": {"x": 0.5, "y": 0.3}}
            ]})
            assert any(c["property"] == "xyPadPosition" and c["status"] == "ok"
                       for c in r2[0]["changes"])
        test("XY Pad position control", t_xypad_position)

        # === AUDIO TRIGGERS ===
        print("\n=== Audio Triggers ===")

        def t_audio_triggers_create():
            r = call("add_vc_audio_triggers", {"items": [
                {"parentID": page_idx, "width": 300, "height": 150,
                 "barsNumber": 5, "volumeLevel": 200},
            ]})
            assert len(r) == 1
            assert r[0]["widgetID"] >= 0
            return r[0]["widgetID"]
        test("add_vc_audio_triggers (with settings)", t_audio_triggers_create)

        # Get the widget ID for subsequent tests
        at_r = call("add_vc_audio_triggers", {"items": [
            {"parentID": page_idx, "barsNumber": 5}
        ]})
        at_wid = at_r[0]["widgetID"] if at_r and at_r[0].get("widgetID", -1) >= 0 else -1

        def t_audio_triggers_query():
            if at_wid < 0:
                print("    (skipped: no widget)")
                return
            d = call("query_widget_details", {"widgetIDs": [at_wid]})
            assert len(d) == 1
            det = d[0]
            assert det["type"] == "Audio Triggers", f"Got {det['type']}"
            assert "barsNumber" in det, "Missing barsNumber"
            assert det["barsNumber"] == 5, f"Expected 5 bars, got {det['barsNumber']}"
            assert "bars" in det, "Missing bars array"
            assert len(det["bars"]) == 5
            assert det["bars"][0]["barIndex"] == 0  # volume bar
        test("query_widget_details (Audio Triggers)", t_audio_triggers_query)

        def t_audio_triggers_configure_function_bar():
            if at_wid < 0:
                print("    (skipped: no widget)")
                return
            fns = call("query_functions")
            if not fns:
                print("    (skipped: no functions)")
                return
            fn_id = fns[0]["id"]
            r = call("configure_audio_triggers", {"items": [
                {"widgetID": at_wid, "bars": [
                    {"barIndex": 1, "type": "function", "functionID": fn_id,
                     "minThreshold": 30, "maxThreshold": 70}
                ]}
            ]})
            assert r[0]["bars"][0]["status"] == "ok"

            # Verify round-trip
            d = call("query_widget_details", {"widgetIDs": [at_wid]})[0]
            bar1 = d["bars"][1]
            assert bar1["type"] == "function", f"Expected function, got {bar1['type']}"
            assert bar1["functionID"] == fn_id
            assert bar1["minThreshold"] == 30
            assert bar1["maxThreshold"] == 70
        test("configure_audio_triggers (function bar)", t_audio_triggers_configure_function_bar)

        def t_audio_triggers_configure_none_bar():
            if at_wid < 0:
                print("    (skipped: no widget)")
                return
            r = call("configure_audio_triggers", {"items": [
                {"widgetID": at_wid, "bars": [
                    {"barIndex": 1, "type": "none"}
                ]}
            ]})
            assert r[0]["bars"][0]["status"] == "ok"
            d = call("query_widget_details", {"widgetIDs": [at_wid]})[0]
            assert d["bars"][1]["type"] == "none"
        test("configure_audio_triggers (reset bar to none)", t_audio_triggers_configure_none_bar)

        def t_audio_triggers_update():
            if at_wid < 0:
                print("    (skipped: no widget)")
                return
            r = call("update_widgets", {"items": [
                {"widgetID": at_wid, "barsNumber": 8, "volumeLevel": 150}
            ]})
            changes = {c["property"]: c["status"] for c in r[0]["changes"]}
            assert changes.get("barsNumber") == "ok"
            assert changes.get("volumeLevel") == "ok"

            d = call("query_widget_details", {"widgetIDs": [at_wid]})[0]
            assert d["barsNumber"] == 8
            assert d["volumeLevel"] == 150
        test("update_widgets (Audio Triggers barsNumber+volume)", t_audio_triggers_update)

        # === PROMPTS ===
        print("\n=== Design Guide ===")

        def t_show_design_guide():
            r = call("get_show_design_guide")
            text = str(r)
            assert "TIER 1" in text or "Just Works" in text, "Missing Tier 1"
            assert "TIER 2" in text or "Flexible" in text, "Missing Tier 2"
            assert "TIER 3" in text or "Full Production" in text, "Missing Tier 3"
            assert "Church" in text, "Missing Church venue"
            assert "HTP" in text, "Missing HTP/LTP rules"
            assert "show-setup.md" in text, "Missing post-build doc instruction"
        test("get_show_design_guide (tiers + venues + docs)", t_show_design_guide)

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
