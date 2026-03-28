#!/usr/bin/env python3
"""
Launchpad Mini MK3 — Full Demo Show Builder
Creates a complete show via QLC+ MCP with all Launchpad features:
- 8 color moods (Row 7, SoloFrame)
- 4 effect chasers (Row 6)
- 4 EFX movements (Row 5)
- Blackout + Stop All (Row 8)
- LED feedback with all 3 modes (static/flashing/pulsing)

Usage: python3 tests/mcp/build_launchpad_demo.py
Requires: QLC+ running with --mcp-http 9696
"""

import urllib.request
import json
import sys
import time

MCP_URL = "http://127.0.0.1:9696/mcp"
SESSION_ID = None
_id = 0


def next_id():
    global _id
    _id += 1
    return _id


def mcp_post(method, params=None):
    global SESSION_ID
    body = {"jsonrpc": "2.0", "method": method, "id": next_id()}
    if params:
        body["params"] = params
    headers = {"Content-Type": "application/json"}
    if SESSION_ID:
        headers["Mcp-Session-Id"] = SESSION_ID
    req = urllib.request.Request(MCP_URL, json.dumps(body).encode(), headers)
    with urllib.request.urlopen(req, timeout=10) as resp:
        SESSION_ID = resp.headers.get("Mcp-Session-Id", SESSION_ID)
        return json.loads(resp.read())


def call(tool, args=None):
    r = mcp_post("tools/call", {"name": tool, "arguments": args or {}})
    content = r.get("result", {}).get("content", [])
    if content and isinstance(content, list):
        return json.loads(content[0].get("text", "[]"))
    return content


def main():
    print("🎛️  Launchpad Mini MK3 — Full Demo Show Builder")
    print("=" * 55)

    # Initialize
    mcp_post("initialize", {
        "protocolVersion": "2024-11-05",
        "capabilities": {},
        "clientInfo": {"name": "launchpad-demo", "version": "1.0"},
    })
    print("✅ Connected to QLC+ MCP\n")

    # === Step 1: Configure Universe ===
    print("=== Step 1: Configure Launchpad Universe ===")
    # Find Launchpad
    devices = call("query_midi_devices")
    midi = next((p for p in devices if p["plugin"] == "MIDI"), None)
    if not midi:
        print("❌ MIDI plugin not found!")
        return

    lp_lines = [i for i in midi["inputs"] if "Launchpad" in i["name"]]
    if not lp_lines:
        print("❌ Launchpad not connected!")
        return

    # Pick the SECOND (last) Launchpad port
    lp_line = lp_lines[-1]["line"]
    print(f"  Found Launchpad on MIDI line {lp_line}")

    # Set input (no output!)
    call("configure_universes", {"items": [
        {"universeID": 3, "name": "Launchpad", "inputPlugin": "MIDI", "inputLine": lp_line}
    ]})
    print("  ✅ Universe 4 input = MIDI Launchpad")

    # Set profile
    call("set_input_profile", {"items": [
        {"universeID": 3, "profileName": "Novation Launchpad Mini MK3"}
    ]})
    print("  ✅ Profile = Novation Launchpad Mini MK3")
    print()
    print("  ⚠️  MANUAL STEPS REQUIRED:")
    print("  1. Enable Feedback in I/O Manager (checkbox)")
    print("  2. Set Init Message → 'Novation Launchpad Mini MK3 Developer Mode'")
    print("  3. Enter Programmer Mode (hold Session → orange pad → release)")
    print()

    # === Step 2: Patch Fixtures ===
    print("=== Step 2: Patch Fixtures ===")
    fixtures = call("patch_fixtures", {"items": [
        {"manufacturer": "Generic", "model": "Generic RGB", "mode": "RGB Dimmer",
         "name": "Par", "universe": 0, "address": 1, "quantity": 8}
    ]})
    print(f"  ✅ Patched {len(fixtures)} RGB pars")

    # === Step 3: Create Color Mood Scenes (Row 7) ===
    print("\n=== Step 3: Create 8 Color Moods ===")
    fx_ids = list(range(8))
    moods = call("create_scenes", {"items": [
        {"name": "Warm Red",     "fixtureIDs": fx_ids, "color": {"r": 255, "g": 50,  "b": 0},   "intensity": 255},
        {"name": "Deep Blue",    "fixtureIDs": fx_ids, "color": {"r": 0,   "g": 30,  "b": 255}, "intensity": 200},
        {"name": "Forest Green", "fixtureIDs": fx_ids, "color": {"r": 0,   "g": 200, "b": 50},  "intensity": 200},
        {"name": "White Wash",   "fixtureIDs": fx_ids, "color": {"r": 255, "g": 255, "b": 255}, "intensity": 255},
        {"name": "Amber",        "fixtureIDs": fx_ids, "color": {"r": 255, "g": 150, "b": 0},   "intensity": 220},
        {"name": "Purple Haze",  "fixtureIDs": fx_ids, "color": {"r": 150, "g": 0,   "b": 255}, "intensity": 200},
        {"name": "Cyan Ice",     "fixtureIDs": fx_ids, "color": {"r": 0,   "g": 200, "b": 200}, "intensity": 200},
        {"name": "Hot Pink",     "fixtureIDs": fx_ids, "color": {"r": 255, "g": 20,  "b": 150}, "intensity": 220},
    ]})
    mood_ids = [m["id"] for m in moods]
    for m in moods:
        print(f"  Scene {m['id']}: {m['name']}")

    # === Step 4: Create Effect Chasers (Row 6) ===
    print("\n=== Step 4: Create 4 Effect Chasers ===")

    # Color cycle chaser (all moods)
    chasers = call("create_chasers", {"items": [
        {"name": "Color Cycle", "functionIDs": mood_ids, "holdTime": 2000,
         "fadeIn": 1000, "runOrder": "loop", "direction": "forward"},
        {"name": "Fast Strobe", "functionIDs": [mood_ids[0], mood_ids[3]],
         "holdTime": 100, "fadeIn": 0, "runOrder": "loop"},
        {"name": "Ping Pong", "functionIDs": mood_ids[:4], "holdTime": 500,
         "fadeIn": 300, "runOrder": "pingpong"},
        {"name": "Random Flash", "functionIDs": mood_ids, "holdTime": 300,
         "fadeIn": 0, "runOrder": "random"},
    ]})
    chaser_ids = [c["id"] for c in chasers]
    for c in chasers:
        print(f"  Chaser {c['id']}: {c['name']}")

    # === Step 5: Create EFX Movements (Row 5) ===
    print("\n=== Step 5: Create 4 EFX Patterns ===")
    efxs = call("create_efxs", {"items": [
        {"name": "Circle", "fixtureIDs": fx_ids[:4], "algorithm": "Circle",
         "width": 200, "height": 200, "speed": 5000},
        {"name": "Figure 8", "fixtureIDs": fx_ids[:4], "algorithm": "Eight",
         "width": 180, "height": 180, "speed": 4000},
        {"name": "Diamond", "fixtureIDs": fx_ids[:4], "algorithm": "Diamond",
         "width": 150, "height": 150, "speed": 3000},
        {"name": "Line Sweep", "fixtureIDs": fx_ids[:4], "algorithm": "Line",
         "width": 255, "height": 50, "speed": 2000},
    ]})
    efx_ids = [e["id"] for e in efxs]
    for e in efxs:
        print(f"  EFX {e['id']}: {e['name']}")

    # === Step 6: Create Collections (Phases) ===
    print("\n=== Step 6: Create Phase Collections ===")
    collections = call("create_collections", {"items": [
        {"name": "Chill Mode", "functionIDs": [mood_ids[1], efx_ids[0]]},
        {"name": "Party Mode", "functionIDs": [chaser_ids[0], efx_ids[1]]},
    ]})
    for c in collections:
        print(f"  Collection {c['id']}: {c['name']}")

    # === Step 7: Create Virtual Console ===
    print("\n=== Step 7: Build Virtual Console ===")

    # Create page
    pages = call("create_vc_pages", {"items": [{"name": "Launchpad Demo"}]})
    page_idx = pages[0]["pageIndex"]
    print(f"  ✅ Page created (index {page_idx})")

    # Create SoloFrame for moods
    mood_frame = call("add_vc_frames", {"items": [
        {"pageIndex": page_idx, "x": 10, "y": 10, "width": 900, "height": 100,
         "caption": "Color Moods (SoloFrame)", "solo": True}
    ]})
    mood_frame_id = mood_frame[0]["widgetID"]
    print(f"  ✅ Mood SoloFrame (widget {mood_frame_id})")

    # Create mood buttons
    mood_names = ["Warm Red", "Deep Blue", "Forest Green", "White",
                  "Amber", "Purple", "Cyan", "Hot Pink"]
    mood_buttons = call("add_vc_buttons", {"items": [
        {"parentID": mood_frame_id, "x": 5 + i * 110, "y": 30, "width": 100, "height": 60,
         "functionID": mood_ids[i], "caption": mood_names[i], "action": "toggle"}
        for i in range(8)
    ]})
    mood_btn_ids = [b["widgetID"] for b in mood_buttons]
    print(f"  ✅ 8 mood buttons: {mood_btn_ids}")

    # Create FX frame
    fx_frame = call("add_vc_frames", {"items": [
        {"pageIndex": page_idx, "x": 10, "y": 120, "width": 500, "height": 100,
         "caption": "Effects", "solo": False}
    ]})
    fx_frame_id = fx_frame[0]["widgetID"]

    # FX buttons (flash mode for strobe)
    fx_names = ["Color Cycle", "Strobe", "Ping Pong", "Random"]
    fx_actions = ["toggle", "flash", "toggle", "toggle"]
    fx_buttons = call("add_vc_buttons", {"items": [
        {"parentID": fx_frame_id, "x": 5 + i * 120, "y": 30, "width": 110, "height": 60,
         "functionID": chaser_ids[i], "caption": fx_names[i], "action": fx_actions[i]}
        for i in range(4)
    ]})
    fx_btn_ids = [b["widgetID"] for b in fx_buttons]
    print(f"  ✅ 4 effect buttons: {fx_btn_ids}")

    # EFX buttons
    efx_buttons = call("add_vc_buttons", {"items": [
        {"parentID": page_idx, "x": 10 + i * 120, "y": 230, "width": 110, "height": 60,
         "functionID": efx_ids[i], "caption": efxs[i]["name"], "action": "toggle"}
        for i in range(4)
    ]})
    efx_btn_ids = [b["widgetID"] for b in efx_buttons]
    print(f"  ✅ 4 EFX buttons: {efx_btn_ids}")

    # Master slider
    slider = call("add_vc_sliders", {"items": [
        {"parentID": page_idx, "x": 520, "y": 120, "width": 60, "height": 200,
         "caption": "Master", "mode": "submaster"}
    ]})
    print(f"  ✅ Master slider: {slider[0]['widgetID']}")

    # Labels
    call("add_vc_labels", {"items": [
        {"parentID": page_idx, "x": 10, "y": 300, "width": 200, "height": 30,
         "text": "Row 7: Moods | Row 6: FX | Row 5: EFX"},
    ]})

    # === Step 8: Set LED Feedback Colors ===
    print("\n=== Step 8: Configure LED Feedback ===")
    # Mood buttons: match scene colors
    #   Red=10, Blue=50, Green=26, White=6, Amber=16, Purple=62, Cyan=38, Pink=68
    mood_colors_idle =   [14, 54, 30, 2,  20, 66, 42, 72]
    mood_colors_active = [10, 50, 26, 6,  16, 62, 38, 68]

    feedback_items = []
    for i in range(8):
        feedback_items.append({
            "widgetID": mood_btn_ids[i],
            "idleValue": mood_colors_idle[i],
            "activeValue": mood_colors_active[i],
            "ledMode": "static"
        })

    # FX buttons: green for chasers, red for strobe (flashing!)
    fx_colors =  [26, 10, 26, 26]
    fx_modes =   ["static", "flashing", "pulsing", "pulsing"]
    for i in range(4):
        feedback_items.append({
            "widgetID": fx_btn_ids[i],
            "idleValue": 30,
            "activeValue": fx_colors[i],
            "ledMode": fx_modes[i]
        })

    # EFX buttons: cyan, pulsing
    for i in range(4):
        feedback_items.append({
            "widgetID": efx_btn_ids[i],
            "idleValue": 42,
            "activeValue": 38,
            "ledMode": "pulsing"
        })

    result = call("configure_vc_feedback", {"items": feedback_items})
    print(f"  ✅ Configured {len(feedback_items)} LED feedback entries")

    # === Summary ===
    print("\n" + "=" * 55)
    print("🎉 Demo Show Complete!")
    print("=" * 55)

    fns = call("query_functions")
    print(f"\n  Functions: {len(fns)}")
    for f in fns:
        print(f"    {f['id']:2d}: [{f['type']:10s}] {f['name']}")

    print(f"\n  Launchpad Pad Mapping (manual auto-detect required):")
    print(f"    Row 8: [not mapped yet — Blackout, Stop All]")
    print(f"    Row 7: 8 mood buttons → {mood_btn_ids}")
    print(f"    Row 6: 4 effect buttons → {fx_btn_ids}")
    print(f"    Row 5: 4 EFX buttons → {efx_btn_ids}")
    print(f"\n  LED modes used: static, flashing, pulsing")
    print(f"\n  Next: Map each VC button to a Launchpad pad via auto-detect")


if __name__ == "__main__":
    main()
