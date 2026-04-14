#!/usr/bin/env python3
"""
DJ Show Builder — Creates a full DJ lighting setup via MCP.
Uses: 2x Varytec Hero 140, 1x Stairville Hz-200 Hazer, 2x Cameo Thunderwash 600 UV
"""

import json
import sys
import time
from urllib.request import Request, urlopen
from urllib.error import URLError, HTTPError


class McpClient:
    def __init__(self, url="http://127.0.0.1:9696/mcp"):
        self.url = url
        self.sid = None
        self._id = 0

    def _rpc(self, method, params):
        self._id += 1
        headers = {"Content-Type": "application/json"}
        if self.sid:
            headers["Mcp-Session-Id"] = self.sid
        req = Request(self.url, json.dumps({
            "jsonrpc": "2.0", "id": self._id,
            "method": method, "params": params
        }).encode(), headers=headers, method="POST")
        try:
            resp = urlopen(req, timeout=15)
        except HTTPError as e:
            body = e.read().decode()
            try:
                return json.loads(body)
            except:
                raise
        if not self.sid:
            self.sid = resp.headers.get("Mcp-Session-Id")
        return json.loads(resp.read())

    def call(self, tool, args):
        r = self._rpc("tools/call", {"name": tool, "arguments": args})
        text = r.get("result", {}).get("content", [{}])[0].get("text", "")
        try:
            return json.loads(text)
        except:
            return text

    def init(self):
        return self._rpc("initialize", {
            "protocolVersion": "2024-11-05",
            "capabilities": {},
            "clientInfo": {"name": "dj-builder", "version": "1.0"}
        })


def main():
    c = McpClient()
    c.init()
    print("🎵 DJ Show Builder — Connected to QLC+ MCP")

    # ── FIXTURES ──────────────────────────────────────────────────
    print("\n🔧 Patching fixtures...")

    r = c.call("patch_fixtures", {"items": [
        {"name": "Hero L", "manufacturer": "Varytec",
         "model": "Hero Spot Wash 140 2in1 RGBW+W",
         "mode": "23 Channel", "universe": 0, "address": 0},
        {"name": "Hero R", "manufacturer": "Varytec",
         "model": "Hero Spot Wash 140 2in1 RGBW+W",
         "mode": "23 Channel", "universe": 0, "address": 23},
        {"name": "Hazer", "manufacturer": "Stairville",
         "model": "Hz-200 DMX",
         "mode": "Default", "universe": 0, "address": 46},
        {"name": "UV L", "manufacturer": "Cameo",
         "model": "Thunderwash 600 UV",
         "mode": "4 Channel", "universe": 0, "address": 48},
        {"name": "UV R", "manufacturer": "Cameo",
         "model": "Thunderwash 600 UV",
         "mode": "4 Channel", "universe": 0, "address": 52},
    ]})
    for f in r:
        print(f"  {f.get('name', '?')}: {f.get('status', '?')} (ID {f.get('id', '?')})")

    # ── QUERY CHANNELS ────────────────────────────────────────────
    print("\n📋 Querying channels...")
    fixtures = c.call("query_fixtures", {})
    fxmap = {f["name"]: f["id"] for f in fixtures}
    print(f"  Fixtures: {list(fxmap.keys())}")

    hero_l = fxmap.get("Hero L", 0)
    hero_r = fxmap.get("Hero R", 1)
    hazer = fxmap.get("Hazer", 2)
    uv_l = fxmap.get("UV L", 3)
    uv_r = fxmap.get("UV R", 4)

    channels = c.call("query_fixture_channels", {"fixtureIDs": [hero_l]})
    # Response is [{channels: [{name, index, ...}, ...]}, ...]
    hero_channels = channels[0]["channels"] if channels else []
    ch = {c_["name"]: c_["index"] for c_ in hero_channels}
    print(f"  Hero channels: {list(ch.keys())}")

    # Channel indices for Hero 140 (23ch mode)
    PAN = ch.get("Pan", 0)
    TILT = ch.get("Tilt", 2)
    PAN_FINE = ch.get("Pan fine", 1)
    TILT_FINE = ch.get("Tilt fine", 3)
    SPEED = ch.get("Pan/Tilt speed", 4)
    SPOT_DIM = ch.get("Spot: Dimmer", 5)
    SPOT_STROBE = ch.get("Spot: Stroboscope", 6)
    WASH_DIM = ch.get("Wash: Dimmer", 13)
    WASH_STROBE = ch.get("Wash: Stroboscope", 14)
    RED = ch.get("Red", 15)
    GREEN = ch.get("Green", 16)
    BLUE = ch.get("Blue", 17)
    WHITE = ch.get("White", 18)
    GOBO = ch.get("Static gobo", 8)
    FOCUS = ch.get("Focus", 11)
    PRISM = ch.get("Prism", 12)

    # UV channels (4ch mode)
    uv_channels = c.call("query_fixture_channels", {"fixtureIDs": [uv_l]})
    uv_hero = uv_channels[0]["channels"] if uv_channels else []
    uv_ch = {c_["name"]: c_["index"] for c_ in uv_hero}
    UV_DIM = uv_ch.get("Dimmer", 0)
    UV_STROBE = uv_ch.get("Strobe", 1)
    UV_DURATION = uv_ch.get("Sound Control Sensitivity / Strobe Duration", uv_ch.get("Duration", 2))
    UV_SOUND = uv_ch.get("Sound Mode", 3)

    # Hazer channels
    hz_channels = c.call("query_fixture_channels", {"fixtureIDs": [hazer]})
    hz_hero = hz_channels[0]["channels"] if hz_channels else []
    hz_ch = {c_["name"]: c_["index"] for c_ in hz_hero}
    HZ_HAZE = hz_ch.get("Haze", 0)
    HZ_FAN = hz_ch.get("Fan Speed", 1)

    # ── SCENES ────────────────────────────────────────────────────
    print("\n🎨 Creating scenes...")

    def hero_vals(fxid, ch_vals):
        """ch_vals: dict of {channel_index: value}"""
        return [{"fixtureID": fxid, "channel": k, "value": v} for k, v in ch_vals.items()]

    def both_heroes(ch_vals):
        return hero_vals(hero_l, ch_vals) + hero_vals(hero_r, ch_vals)

    def both_uvs(ch_vals):
        vals = []
        for fxid in [uv_l, uv_r]:
            vals += [{"fixtureID": fxid, "channel": k, "value": v} for k, v in ch_vals.items()]
        return vals

    scenes = c.call("create_scenes", {"items": [
        # ── WASH COLORS ──
        {"name": "Wash Red",
         "channelValues": both_heroes({WASH_DIM: 255, RED: 255, GREEN: 0, BLUE: 0, WHITE: 0, SPEED: 0})},
        {"name": "Wash Blue",
         "channelValues": both_heroes({WASH_DIM: 255, RED: 0, GREEN: 0, BLUE: 255, WHITE: 0})},
        {"name": "Wash Purple",
         "channelValues": both_heroes({WASH_DIM: 255, RED: 200, GREEN: 0, BLUE: 255, WHITE: 0})},
        {"name": "Wash Teal",
         "channelValues": both_heroes({WASH_DIM: 255, RED: 0, GREEN: 180, BLUE: 220, WHITE: 0})},
        {"name": "Wash Orange",
         "channelValues": both_heroes({WASH_DIM: 255, RED: 255, GREEN: 80, BLUE: 0, WHITE: 0})},
        {"name": "Wash White",
         "channelValues": both_heroes({WASH_DIM: 255, RED: 0, GREEN: 0, BLUE: 0, WHITE: 255})},
        {"name": "Wash Off",
         "channelValues": both_heroes({WASH_DIM: 0, RED: 0, GREEN: 0, BLUE: 0, WHITE: 0})},

        # ── SPOT ──
        {"name": "Spot On",
         "channelValues": both_heroes({SPOT_DIM: 255, SPOT_STROBE: 0, FOCUS: 128})},
        {"name": "Spot Off",
         "channelValues": both_heroes({SPOT_DIM: 0})},
        {"name": "Spot Strobe",
         "channelValues": both_heroes({SPOT_DIM: 255, SPOT_STROBE: 220})},

        # ── POSITION ──
        {"name": "Pos Front",
         "channelValues": both_heroes({PAN: 128, TILT: 128, SPEED: 0})},
        {"name": "Pos Wide",
         "channelValues": hero_vals(hero_l, {PAN: 80, TILT: 100, SPEED: 0})
                        + hero_vals(hero_r, {PAN: 176, TILT: 100, SPEED: 0})},
        {"name": "Pos Floor",
         "channelValues": both_heroes({PAN: 128, TILT: 200, SPEED: 0})},
        {"name": "Pos Up",
         "channelValues": both_heroes({PAN: 128, TILT: 50, SPEED: 0})},
        {"name": "Pos Cross",
         "channelValues": hero_vals(hero_l, {PAN: 180, TILT: 110, SPEED: 0})
                        + hero_vals(hero_r, {PAN: 76, TILT: 110, SPEED: 0})},

        # ── GOBO ──
        {"name": "Gobo Dots",
         "channelValues": both_heroes({GOBO: 10, SPOT_DIM: 255, FOCUS: 128})},
        {"name": "Gobo Star",
         "channelValues": both_heroes({GOBO: 20, SPOT_DIM: 255, FOCUS: 128})},
        {"name": "Gobo Spiral",
         "channelValues": both_heroes({GOBO: 30, SPOT_DIM: 255, FOCUS: 128})},
        {"name": "Gobo Off",
         "channelValues": both_heroes({GOBO: 0})},

        # ── UV ──
        {"name": "UV Full",
         "channelValues": both_uvs({UV_DIM: 255, UV_STROBE: 0})},
        {"name": "UV Strobe",
         "channelValues": both_uvs({UV_DIM: 255, UV_STROBE: 200})},
        {"name": "UV Pulse",
         "channelValues": both_uvs({UV_DIM: 255, UV_STROBE: 80})},
        {"name": "UV Off",
         "channelValues": both_uvs({UV_DIM: 0, UV_STROBE: 0})},

        # ── HAZER ──
        {"name": "Haze Low",
         "channelValues": [{"fixtureID": hazer, "channel": HZ_HAZE, "value": 80},
                           {"fixtureID": hazer, "channel": HZ_FAN, "value": 100}]},
        {"name": "Haze Full",
         "channelValues": [{"fixtureID": hazer, "channel": HZ_HAZE, "value": 255},
                           {"fixtureID": hazer, "channel": HZ_FAN, "value": 200}]},
        {"name": "Haze Off",
         "channelValues": [{"fixtureID": hazer, "channel": HZ_HAZE, "value": 0},
                           {"fixtureID": hazer, "channel": HZ_FAN, "value": 0}]},

        # ── STROBE (wash) ──
        {"name": "Wash Strobe White",
         "channelValues": both_heroes({WASH_DIM: 255, WASH_STROBE: 220, WHITE: 255})},

        # ── BLACKOUT ──
        {"name": "All Off",
         "channelValues": both_heroes({WASH_DIM: 0, SPOT_DIM: 0})
                        + both_uvs({UV_DIM: 0})
                        + [{"fixtureID": hazer, "channel": HZ_HAZE, "value": 0}]},
    ]})
    for s in scenes:
        print(f"  {s.get('name', '?')}: {s.get('status', '?')}")

    # ── CHASERS ───────────────────────────────────────────────────
    print("\n⏩ Creating chasers...")

    chasers = c.call("create_chasers", {"items": [
        {"name": "Color Cycle",
         "steps": ["Wash Red", "Wash Orange", "Wash Purple", "Wash Teal", "Wash Blue"]},
        {"name": "UV Pulse Chase",
         "steps": ["UV Full", "UV Off", "UV Full", "UV Off", "UV Pulse"]},
        {"name": "Spot + Gobo Show",
         "steps": ["Gobo Dots", "Gobo Star", "Gobo Spiral", "Gobo Off"]},
        {"name": "Position Sweep",
         "steps": ["Pos Front", "Pos Wide", "Pos Cross", "Pos Floor", "Pos Up"]},
        {"name": "Strobe Burst",
         "steps": ["Wash Strobe White", "All Off", "UV Strobe", "All Off",
                    "Spot Strobe", "All Off"]},
        {"name": "Build Up",
         "steps": ["Wash Blue", "Wash Purple", "Wash Red", "Wash Strobe White"]},
    ]})
    for ch_ in chasers:
        print(f"  {ch_.get('name', '?')}: {ch_.get('status', '?')}")

    # ── COLLECTIONS ───────────────────────────────────────────────
    print("\n📦 Creating collections...")

    colls = c.call("create_collections", {"items": [
        {"name": "Party Mode", "functionNames": ["Color Cycle", "UV Pulse Chase", "Haze Low"]},
        {"name": "Drop Hit", "functionNames": ["Strobe Burst", "UV Full", "Haze Full"]},
        {"name": "Chill Wash", "functionNames": ["Wash Teal", "Haze Low", "Pos Front"]},
    ]})
    for co in colls:
        print(f"  {co.get('name', '?')}: {co.get('status', '?')}")

    # ── VIRTUAL CONSOLE ───────────────────────────────────────────
    print("\n🖥️  Building Virtual Console...")

    # Create page
    c.call("vc_create_pages", {"items": [{"name": "DJ Control"}]})

    W = 1200  # total width

    # ── Row 1: Main Controls (SoloFrame for wash colors) ──
    c.call("vc_create_widgets", {"items": [
        {"type": "soloframe", "pageIndex": 0,
         "x": 10, "y": 10, "width": 530, "height": 180,
         "caption": "WASH COLOR",
         "headerVisible": True, "enableButtonVisible": True},
    ]})

    # Find the soloframe to put buttons inside
    pages = c.call("vc_query_pages", {})
    sf_id = None
    for p in pages:
        for w in p.get("widgets", []):
            if w.get("type", "").lower() == "soloframe" and w.get("caption") == "WASH COLOR":
                sf_id = w["id"]
    print(f"  WASH COLOR frame: ID {sf_id}")

    if sf_id:
        colors = [
            ("🔴 Red", "Wash Red", "#660000"),
            ("🔵 Blue", "Wash Blue", "#000066"),
            ("💜 Purple", "Wash Purple", "#330066"),
            ("🟢 Teal", "Wash Teal", "#003344"),
            ("🟠 Orange", "Wash Orange", "#663300"),
            ("⚪ White", "Wash White", "#333333"),
            ("⬛ Off", "Wash Off", "#111111"),
        ]
        color_widgets = []
        for i, (label, func, color) in enumerate(colors):
            color_widgets.append({
                "type": "button", "parentID": sf_id,
                "x": 5 + i * 75, "y": 5, "width": 70, "height": 60,
                "caption": label, "functionName": func, "bgColor": color,
                "fgColor": "#ffffff",
            })
        c.call("vc_create_widgets", {"items": color_widgets})
        print(f"  Created {len(colors)} wash color buttons")

    # ── Row 1 right: FX Buttons ──
    fx_items = [
        {"type": "button", "parentID": -1,
         "x": 560, "y": 10, "width": 100, "height": 60,
         "caption": "🌀 Color Cycle", "functionName": "Color Cycle", "bgColor": "#223366"},
        {"type": "button", "parentID": -1,
         "x": 670, "y": 10, "width": 100, "height": 60,
         "caption": "💡 Spot Show", "functionName": "Spot + Gobo Show", "bgColor": "#443300"},
        {"type": "button", "parentID": -1,
         "x": 780, "y": 10, "width": 100, "height": 60,
         "caption": "🎯 Pos Sweep", "functionName": "Position Sweep", "bgColor": "#003333"},
        {"type": "button", "parentID": -1,
         "x": 560, "y": 80, "width": 100, "height": 60,
         "caption": "⚡ STROBE", "functionName": "Strobe Burst",
         "bgColor": "#880000", "action": "flash"},
        {"type": "button", "parentID": -1,
         "x": 670, "y": 80, "width": 100, "height": 60,
         "caption": "🔮 UV Pulse", "functionName": "UV Pulse Chase", "bgColor": "#220044"},
        {"type": "button", "parentID": -1,
         "x": 780, "y": 80, "width": 100, "height": 60,
         "caption": "🔥 Build Up", "functionName": "Build Up", "bgColor": "#662200"},
    ]
    c.call("vc_create_widgets", {"items": fx_items})
    print("  Created 6 FX buttons")

    # ── Row 1 far right: Collections ──
    coll_items = [
        {"type": "button", "parentID": -1,
         "x": 900, "y": 10, "width": 120, "height": 60,
         "caption": "🎉 PARTY MODE", "functionName": "Party Mode",
         "bgColor": "#004400", "fgColor": "#00ff00"},
        {"type": "button", "parentID": -1,
         "x": 900, "y": 80, "width": 120, "height": 60,
         "caption": "💥 DROP HIT", "functionName": "Drop Hit",
         "bgColor": "#440000", "fgColor": "#ff4444", "action": "flash"},
        {"type": "button", "parentID": -1,
         "x": 1040, "y": 10, "width": 120, "height": 60,
         "caption": "🌊 CHILL", "functionName": "Chill Wash",
         "bgColor": "#002233", "fgColor": "#66ccff"},
    ]
    c.call("vc_create_widgets", {"items": coll_items})
    print("  Created 3 collection buttons")

    # ── Row 2: Sliders ──
    sliders = [
        {"type": "slider", "parentID": -1,
         "x": 10, "y": 210, "width": 60, "height": 200,
         "caption": "Spot L", "mode": "level",
         "channels": [{"fixtureID": hero_l, "channel": SPOT_DIM}]},
        {"type": "slider", "parentID": -1,
         "x": 80, "y": 210, "width": 60, "height": 200,
         "caption": "Spot R", "mode": "level",
         "channels": [{"fixtureID": hero_r, "channel": SPOT_DIM}]},
        {"type": "slider", "parentID": -1,
         "x": 150, "y": 210, "width": 60, "height": 200,
         "caption": "Wash L", "mode": "level",
         "channels": [{"fixtureID": hero_l, "channel": WASH_DIM}]},
        {"type": "slider", "parentID": -1,
         "x": 220, "y": 210, "width": 60, "height": 200,
         "caption": "Wash R", "mode": "level",
         "channels": [{"fixtureID": hero_r, "channel": WASH_DIM}]},
        {"type": "slider", "parentID": -1,
         "x": 310, "y": 210, "width": 60, "height": 200,
         "caption": "UV L", "mode": "level",
         "channels": [{"fixtureID": uv_l, "channel": UV_DIM}]},
        {"type": "slider", "parentID": -1,
         "x": 380, "y": 210, "width": 60, "height": 200,
         "caption": "UV R", "mode": "level",
         "channels": [{"fixtureID": uv_r, "channel": UV_DIM}]},
        {"type": "slider", "parentID": -1,
         "x": 470, "y": 210, "width": 60, "height": 200,
         "caption": "Haze", "mode": "level",
         "channels": [{"fixtureID": hazer, "channel": HZ_HAZE}]},
        {"type": "slider", "parentID": -1,
         "x": 540, "y": 210, "width": 60, "height": 200,
         "caption": "Fan", "mode": "level",
         "channels": [{"fixtureID": hazer, "channel": HZ_FAN}]},
    ]
    c.call("vc_create_widgets", {"items": sliders})
    print("  Created 8 dimmer sliders")

    # ── Row 2 right: XY Pad for moving heads ──
    c.call("vc_create_widgets", {"items": [
        {"type": "xypad", "parentID": -1,
         "x": 630, "y": 210, "width": 250, "height": 250,
         "caption": "Pan/Tilt",
         "fixtureIDs": [hero_l, hero_r]},
    ]})
    print("  Created XY Pad for pan/tilt")

    # ── Row 2 far right: Speed Dial ──
    funcs = c.call("query_functions", {})
    chase_ids = [f["id"] for f in funcs if f.get("type") == "Chaser"]
    c.call("vc_create_widgets", {"items": [
        {"type": "speedDial", "parentID": -1,
         "x": 900, "y": 210, "width": 180, "height": 200,
         "caption": "Chase Speed", "functionIDs": chase_ids},
    ]})
    print("  Created speed dial")

    # ── Row 3: Cuelist + UV controls ──
    c.call("vc_create_widgets", {"items": [
        {"type": "cuelist", "parentID": -1,
         "x": 10, "y": 430, "width": 350, "height": 180,
         "caption": "Color Cues", "chaserName": "Color Cycle"},
    ]})
    print("  Created cuelist")

    # ── Labels ──
    c.call("vc_create_widgets", {"items": [
        {"type": "label", "parentID": -1,
         "x": 10, "y": 190, "width": 590, "height": 20,
         "caption": "─── DIMMERS ───", "fgColor": "#666666"},
        {"type": "label", "parentID": -1,
         "x": 630, "y": 190, "width": 250, "height": 20,
         "caption": "─── POSITION ───", "fgColor": "#666666"},
        {"type": "label", "parentID": -1,
         "x": 900, "y": 190, "width": 180, "height": 20,
         "caption": "─── SPEED ───", "fgColor": "#666666"},
    ]})
    print("  Created section labels")

    # ── Blackout button ──
    c.call("vc_create_widgets", {"items": [
        {"type": "button", "parentID": -1,
         "x": 1040, "y": 80, "width": 120, "height": 60,
         "caption": "⬛ BLACKOUT", "action": "blackout",
         "bgColor": "#000000", "fgColor": "#ff0000"},
    ]})
    print("  Created blackout button")

    # ── KEYBOARD SHORTCUTS ──
    print("\n⌨️  Mapping keyboard shortcuts...")
    pages = c.call("vc_query_pages", {})
    shortcuts = {}
    for p in pages:
        for w in p.get("widgets", []):
            cap = w.get("caption", "")
            if "PARTY" in cap: shortcuts["P"] = w["id"]
            elif "DROP" in cap: shortcuts["D"] = w["id"]
            elif "STROBE" in cap: shortcuts["S"] = w["id"]
            elif "BLACKOUT" in cap: shortcuts["B"] = w["id"]
            elif "CHILL" in cap: shortcuts["C"] = w["id"]

    key_items = [{"widgetID": wid, "keySequence": key} for key, wid in shortcuts.items()]
    if key_items:
        c.call("vc_set_key_sequences", {"items": key_items})
        for key, wid in shortcuts.items():
            print(f"  [{key}] → widget {wid}")

    print("\n" + "=" * 50)
    print("🎉 DJ SHOW READY!")
    print("=" * 50)
    print("\nFixtures: 2x Hero 140, 1x Hazer, 2x UV Thunderwash")
    print("Scenes: 27 | Chasers: 6 | Collections: 3")
    print("\nKeyboard shortcuts:")
    print("  [P] Party Mode  [D] Drop Hit  [S] Strobe")
    print("  [C] Chill Wash  [B] Blackout")
    print("\nSwitch to Operate mode (Ctrl+F12) and go! 🎧")


if __name__ == "__main__":
    main()
