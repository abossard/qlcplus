#!/usr/bin/env python3
"""
Interactive Launchpad Mini MK3 Test Protocol for QLC+ MCP.
Walks through each feature step by step, asking for user feedback.

Usage: python3 tests/mcp/test_launchpad.py [--port 9696]
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
    try:
        with urllib.request.urlopen(req, timeout=10) as resp:
            SESSION_ID = resp.headers.get("Mcp-Session-Id", SESSION_ID)
            return json.loads(resp.read())
    except Exception as e:
        return {"error": str(e)}


def call(tool, args=None):
    r = mcp_post("tools/call", {"name": tool, "arguments": args or {}})
    if "error" in r and isinstance(r["error"], dict):
        print(f"  ⚠️  Tool error: {r['error'].get('message', r['error'])}")
        return None
    content = r.get("result", {}).get("content", [])
    if content and isinstance(content, list) and len(content) > 0:
        text = content[0].get("text", "")
        try:
            return json.loads(text)
        except (json.JSONDecodeError, TypeError):
            return text
    return content


def ask(question):
    print(f"\n  ❓ {question}")
    return input("  → ").strip()


def ok(question):
    r = ask(f"{question} (y/n)")
    return r.lower() in ("y", "yes", "1", "ok", "true")


def step(num, title):
    print(f"\n{'='*60}")
    print(f"  STEP {num}: {title}")
    print(f"{'='*60}")


def main():
    print("🎛️  Launchpad Mini MK3 — Interactive Test Protocol")
    print("=" * 60)
    print("Make sure QLC+ is running with --mcp-http 9696")
    print("and the Launchpad is connected via USB.")
    print()

    # --- Step 0: Connect MCP ---
    step(0, "Connect to QLC+ MCP")
    r = mcp_post("initialize", {
        "protocolVersion": "2024-11-05",
        "capabilities": {},
        "clientInfo": {"name": "launchpad-test", "version": "1.0"},
    })
    if "error" in r:
        print(f"  ❌ Cannot connect to MCP: {r['error']}")
        print(f"  Start QLC+ with: ./build-mcp/qmlui/qlcplus-qml --mcp-http 9696")
        return
    print(f"  ✅ Connected: {r.get('result', {}).get('serverInfo', {})}")

    # --- Step 1: Find MIDI devices ---
    step(1, "Discover MIDI Devices")
    devices = call("query_midi_devices")
    if not devices:
        print("  ❌ No plugins returned")
        return

    midi = None
    for plugin in devices:
        if plugin.get("plugin") == "MIDI":
            midi = plugin
            break

    if not midi:
        print("  ❌ MIDI plugin not loaded! Check PlugIns/ directory.")
        return

    print(f"  MIDI inputs:")
    for inp in midi.get("inputs", []):
        marker = " ← LAUNCHPAD" if "Launchpad" in inp["name"] else ""
        print(f"    Line {inp['line']}: {inp['name']}{marker}")
    print(f"  MIDI outputs:")
    for out in midi.get("outputs", []):
        marker = " ← LAUNCHPAD" if "Launchpad" in out["name"] else ""
        print(f"    Line {out['line']}: {out['name']}{marker}")

    lp_inputs = [i for i in midi["inputs"] if "Launchpad" in i["name"]]
    lp_outputs = [o for o in midi["outputs"] if "Launchpad" in o["name"]]

    if not lp_inputs:
        print("  ❌ Launchpad not found in MIDI inputs!")
        return

    print(f"\n  Found {len(lp_inputs)} Launchpad input(s), {len(lp_outputs)} output(s)")

    lp_line = ask(f"Which MIDI line for the Launchpad? (try {lp_inputs[0]['line']} or {lp_inputs[-1]['line']})")
    lp_line = int(lp_line) if lp_line else lp_inputs[0]["line"]

    # --- Step 2: Configure Universe ---
    step(2, "Configure Universe for Launchpad")
    uni_id = int(ask("Which universe to use for Launchpad? (0-3, default 3)") or "3")

    print(f"  Setting universe {uni_id + 1} input to MIDI line {lp_line}...")
    r = call("configure_universes", {"items": [
        {"universeID": uni_id, "name": "Launchpad", "inputPlugin": "MIDI", "inputLine": lp_line}
    ]})
    print(f"  Input: {r}")

    print(f"  Setting universe {uni_id + 1} output to MIDI line {lp_line}...")
    r = call("configure_universes", {"items": [
        {"universeID": uni_id, "outputPlugin": "MIDI", "outputLine": lp_line}
    ]})
    print(f"  Output: {r}")

    if not ok("In QLC+ I/O Manager, is the Launchpad now assigned to that universe?"):
        alt_line = ask("Try a different line number?")
        if alt_line:
            lp_line = int(alt_line)
            call("configure_universes", {"items": [
                {"universeID": uni_id, "inputPlugin": "MIDI", "inputLine": lp_line}
            ]})
            call("configure_universes", {"items": [
                {"universeID": uni_id, "outputPlugin": "MIDI", "outputLine": lp_line}
            ]})

    # --- Step 3: Set Input Profile ---
    step(3, "Set Input Profile")
    print("  Setting profile to 'Novation Launchpad Mini MK3'...")
    r = call("set_input_profile", {"items": [
        {"universeID": uni_id, "profileName": "Novation Launchpad Mini MK3"}
    ]})
    print(f"  Result: {r}")

    # --- Step 4: Programmer Mode ---
    step(4, "Enter Programmer Mode")
    print("  The MIDI Init Message needs to be set in QLC+ GUI:")
    print("  1. Input/Output Manager → click on the Launchpad universe")
    print("  2. Find MIDI config (gear icon on the output)")
    print("  3. Set Init Message → 'Novation Launchpad Mini MK3 Developer Mode'")
    print()
    print("  OR manually on the Launchpad:")
    print("  1. Hold Session button (top-left)")
    print("  2. Tap the orange pad in the bottom row")
    print("  3. Release Session")

    if not ok("Is the Launchpad now in Programmer Mode? (pads should be dark)"):
        print("  ⚠️  Continuing anyway — pads may not respond correctly.")

    # --- Step 5: Test Input ---
    step(5, "Test MIDI Input")
    print("  Press any pad on the Launchpad now.")
    if ok("Do you see input activity in QLC+ Input/Output Manager?"):
        print("  ✅ MIDI input working!")
    else:
        print("  ❌ No input. Check:")
        print("     - Is the correct Launchpad device assigned?")
        print("     - Try the other Launchpad MIDI port")
        if not ok("Want to continue anyway?"):
            return

    # --- Step 6: Patch Fixtures ---
    step(6, "Patch Fixtures")
    print("  Patching 4 Generic RGB pars...")
    r = call("patch_fixtures", {"items": [
        {"manufacturer": "Generic", "model": "Generic RGB", "mode": "RGB Dimmer",
         "name": "Par", "universe": 0, "address": 1, "quantity": 4}
    ]})
    if r:
        print(f"  ✅ Patched {len(r)} fixtures")
    else:
        print("  ⚠️  Fixture patching failed (library might not be loaded)")
        print("  Continuing — scenes will be created but won't have channel values.")

    # --- Step 7: Create Scenes ---
    step(7, "Create Color Scenes")
    r = call("create_scenes", {"items": [
        {"name": "Red", "fixtureIDs": [0, 1, 2, 3], "color": {"r": 255, "g": 0, "b": 0}, "intensity": 255},
        {"name": "Blue", "fixtureIDs": [0, 1, 2, 3], "color": {"r": 0, "g": 0, "b": 255}, "intensity": 255},
        {"name": "Green", "fixtureIDs": [0, 1, 2, 3], "color": {"r": 0, "g": 255, "b": 0}, "intensity": 255},
        {"name": "White", "fixtureIDs": [0, 1, 2, 3], "color": {"r": 255, "g": 255, "b": 255}, "intensity": 255},
    ]})
    scene_ids = [s["id"] for s in r] if r else [0, 1, 2, 3]
    print(f"  ✅ Created scenes: {r}")

    # --- Step 8: Create VC Buttons ---
    step(8, "Create Virtual Console Buttons")
    colors = ["Red", "Blue", "Green", "White"]
    buttons = call("add_vc_buttons", {"items": [
        {"parentID": 0, "x": 10 + i * 110, "y": 10, "width": 100, "height": 50,
         "functionID": scene_ids[i], "caption": colors[i], "action": "toggle"}
        for i in range(4)
    ]})
    if buttons:
        btn_ids = [b["widgetID"] for b in buttons]
        print(f"  ✅ Created buttons: {btn_ids}")
    else:
        print("  ⚠️  Button creation returned no data")
        btn_ids = []

    if ok("Do you see 4 buttons (Red, Blue, Green, White) in the Virtual Console?"):
        print("  ✅ Buttons visible!")
    else:
        print("  ⚠️  Buttons may be on a different page. Check all VC pages.")

    # --- Step 9: Map Pads to Buttons ---
    step(9, "Map Launchpad Pads to Buttons")
    print("  You need to map each button manually via QLC+ auto-detect:")
    print()
    for i, color in enumerate(colors):
        print(f"  {i+1}. Click the [{color}] button in VC")
        print(f"     → Properties → External Input → Learn/Auto-detect")
        print(f"     → Press pad at row 1, column {i+1} on the Launchpad")
        print()

    if not ok("Did you map all 4 buttons to Launchpad pads?"):
        print("  Skipping pad test.")
    else:
        # --- Step 10: Test Pad → Scene ---
        step(10, "Test: Pad Triggers Scene")
        print("  Press each of the 4 mapped pads on the Launchpad.")
        if ok("Do the scenes toggle on/off in QLC+?"):
            print("  ✅ 🎉 Pad → Scene triggering works!")
        else:
            print("  ❌ Scenes not triggering. Debug:")
            print("     - Check External Input mapping on each button")
            print("     - Check universe/channel matches")

        # --- Step 11: Test LED Feedback ---
        step(11, "Test: LED Feedback")
        print("  When you press a pad and the scene activates,")
        print("  does the Launchpad pad LED change color?")
        if ok("Do pad LEDs light up when scenes are active?"):
            print("  ✅ 🎉🎉 Full bidirectional control working!")
        else:
            print("  ❌ No LED feedback. Check:")
            print("     - Feedback/Output enabled in I/O Manager")
            print("     - Init Message set to MK3 Developer Mode")
            print("     - Correct Launchpad port for output")

    # --- Summary ---
    print()
    print("=" * 60)
    print("  TEST PROTOCOL COMPLETE")
    print("=" * 60)
    print()

    fns = call("query_functions")
    fixtures = call("query_fixtures")
    print(f"  Fixtures: {len(fixtures) if fixtures else 0}")
    print(f"  Functions: {len(fns) if fns else 0}")
    print()
    print("  Next steps:")
    print("  - Add more scenes (chasers, EFX)")
    print("  - Map more rows on the Launchpad")
    print("  - Set LED feedback colors via configure_vc_feedback")
    print()


if __name__ == "__main__":
    if len(sys.argv) > 2 and sys.argv[1] == "--port":
        MCP_URL = f"http://127.0.0.1:{sys.argv[2]}/mcp"
    main()
