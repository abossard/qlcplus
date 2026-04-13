"""
AutoLight workspace setup — creates rating UI and experiment infrastructure.

Parameterized: pass a briefing dict to tailor the setup.
Idempotent: safe to re-run (all MCP create tools upsert by name).

Usage:
    /tmp/mcp-env/bin/python3 -m autolight.setup
"""

import asyncio
import json
import os
import sys

HEADER_H = 40  # QLC+ frame header height in pixels

from .qlc_client import QLC

STATE_FILE = os.path.join(os.path.dirname(__file__), "..", "autolight-state.json")

# ── Rating definitions ──

OVERALL_RATINGS = [
    {"name": "Rating-1", "label": "⭐",         "rgb": "#ff0000", "desc": "Bad"},
    {"name": "Rating-2", "label": "⭐⭐",       "rgb": "#ff6600", "desc": "Poor"},
    {"name": "Rating-3", "label": "⭐⭐⭐",     "rgb": "#ffff00", "desc": "OK"},
    {"name": "Rating-4", "label": "⭐⭐⭐⭐",   "rgb": "#00ff00", "desc": "Good"},
    {"name": "Rating-5", "label": "⭐⭐⭐⭐⭐", "rgb": "#ffffff", "desc": "Great"},
]

DEFAULT_DIMENSIONS = ["Colors", "Beat Sync", "Energy", "Creativity"]

DIMENSION_LEVELS = [
    {"key": "bad",  "label": "👎 Bad",  "rgb": "#ff0000", "fg": "#ff4444"},
    {"key": "ok",   "label": "😐 OK",   "rgb": "#ffaa00", "fg": "#ffaa00"},
    {"key": "good", "label": "👍 Good", "rgb": "#00ff00", "fg": "#44ff44"},
]


async def setup(
    dimensions: list[str] | None = None,
    extra_questions: list[dict] | None = None,
) -> dict:
    """
    Build the AutoLight rating workspace in the running QLC+ instance.

    Args:
        dimensions: Rating dimensions (default: Colors, Beat Sync, Energy, Creativity).
                    Pass custom ones like ["Warmth", "Movement", "Surprise"].
        extra_questions: Additional per-experiment questions to add as button groups.
                         Each: {"name": "Strobe OK?", "options": ["Yes", "No"]}

    Returns:
        State dict with all widget IDs, saved to autolight-state.json.
    """
    dims = dimensions or DEFAULT_DIMENSIONS

    async with QLC() as q:
        print("✓ Connected to QLC+ MCP")

        # ── Verify fixtures ──
        fixtures = await q.call("query_fixtures")
        print(f"✓ {len(fixtures)} fixtures")
        for f in fixtures:
            print(f"  ID={f['id']} {f['name']} ({f['channels']}ch, {f['heads']} heads)")
        fixture_names = [f["name"] for f in fixtures]

        # ── Clean old functions (only AutoLight ones) ──
        existing = await q.call("query_functions")
        al_funcs = [f for f in existing if f["name"].startswith(("Rating-", "Dim-", "AL-"))]
        if al_funcs:
            print(f"  Cleaning {len(al_funcs)} old AutoLight functions...")
            await q.call("delete_functions", ids=[f["id"] for f in al_funcs])

        # ── 1. Palettes ──
        print("\n── Palettes ──")
        palette_items = []
        for r in OVERALL_RATINGS:
            palette_items.append({"name": r["name"], "type": "Color", "rgb": r["rgb"]})
        for d in DIMENSION_LEVELS:
            palette_items.append({"name": f"Dim-{d['key'].title()}", "type": "Color", "rgb": d["rgb"]})
        palette_items.append({"name": "Full", "type": "Dimmer", "value": 255})
        palette_items.append({"name": "Off",  "type": "Dimmer", "value": 0})

        palettes = await q.call("create_palettes", items=palette_items)
        print(f"  ✓ {len(palettes)} palettes")

        # ── 2. Rating scenes ──
        print("\n── Rating scenes ──")
        scene_items = []
        for r in OVERALL_RATINGS:
            scene_items.append({
                "name": r["name"], "fixtureNames": ["WLED*"],
                "paletteNames": [r["name"], "Full"],
                "path": "AutoLight/Ratings", "fadeIn": 0, "fadeOut": 500,
            })
        for d in DIMENSION_LEVELS:
            scene_items.append({
                "name": f"Dim-{d['key'].title()}", "fixtureNames": ["WLED*"],
                "paletteNames": [f"Dim-{d['key'].title()}", "Full"],
                "path": "AutoLight/Ratings", "fadeIn": 0, "fadeOut": 300,
            })
        scene_items.append({
            "name": "AL-Blackout", "fixtureNames": ["WLED*"],
            "paletteNames": ["Off"],
            "path": "AutoLight/System", "fadeIn": 0, "fadeOut": 0,
        })

        scenes = await q.call("create_scenes", items=scene_items)
        scene_map = {s["name"]: s["id"] for s in scenes}
        print(f"  ✓ {len(scenes)} scenes")

        # ── 3. VC Layout ──
        # All child y-coordinates must be >= HEADER_H to avoid overlapping the frame header.
        # Grid snapping handles fine spacing — no manual padding needed.
        print("\n── VC Layout ──")
        H = HEADER_H
        BTN_H = 50
        DIM_BTN_H = 30
        DIM_ROW_H = 80  # header(40) + button(30) + breathing room, snaps to 80
        RATING_W = 120

        # Top-level frames on page 0
        FRAME_GAP = 5
        row1_h = H + 30                          # Experiment: header + label
        row2_h = H + BTN_H                       # Rating/Transport: header + buttons
        # Dimension SoloFrames get FRAME_GAP between them
        row3_h = H + len(dims) * (DIM_ROW_H + FRAME_GAP)

        # Rating width must be divisible by (5 * gridSnap=5) = 25 for even columns
        rating_w = 625  # 625 / 5 = 125px per star button

        frame_captions = ["Experiment", "Overall Rating", "Transport", "Dimensions"]
        top_frames = await q.call("vc_create_widgets", items=[
            {"type": "frame", "caption": "Experiment",
             "pageIndex": 0, "x": 0, "y": 0, "width": 900, "height": row1_h,
             "headerVisible": True},
            {"type": "soloframe", "caption": "Overall Rating",
             "pageIndex": 0, "x": 0, "y": row1_h, "width": rating_w, "height": row2_h,
             "headerVisible": True},
            {"type": "frame", "caption": "Transport",
             "pageIndex": 0, "x": rating_w, "y": row1_h, "width": 900 - rating_w, "height": row2_h,
             "headerVisible": True},
            {"type": "frame", "caption": "Dimensions",
             "pageIndex": 0, "x": 0, "y": row1_h + row2_h, "width": 900, "height": row3_h,
             "headerVisible": True},
        ])
        fmap = {cap: w["widgetID"] for cap, w in zip(frame_captions, top_frames)}
        print(f"  Frames: {fmap}")

        # Experiment label + play button
        exp_widgets = await q.call("vc_create_widgets", items=[
            {"type": "label",
             "caption": "EXP-00: Ready — start an AutoLight session",
             "parentID": fmap["Experiment"],
             "x": 0, "y": H, "width": 700, "height": 30,
             "fgColor": "#00d4ff", "bgColor": "#1a1a2e"},
            {"type": "button",
             "caption": "▶ Play",
             "parentID": fmap["Experiment"],
             "x": 700, "y": H, "width": 200, "height": 30,
             "action": "toggle",
             "fgColor": "#00ff00", "bgColor": "#003300"},
        ])
        label_id = exp_widgets[0]["widgetID"]
        play_btn_id = exp_widgets[1]["widgetID"]
        print(f"  Label: {label_id}")

        # Overall rating buttons
        rating_btns = await q.call("vc_create_widgets", items=[
            {"type": "button", "caption": r["label"],
             "parentID": fmap["Overall Rating"],
             "x": i * RATING_W, "y": H, "width": RATING_W, "height": BTN_H,
             "functionID": scene_map[r["name"]],
             "action": "toggle",
             "fgColor": r["rgb"], "bgColor": "#1a1a2e"}
            for i, r in enumerate(OVERALL_RATINGS)
        ])
        rating_ids = {f"star{i+1}": w["widgetID"] for i, w in enumerate(rating_btns)}
        print(f"  Rating buttons: {rating_ids}")

        # Transport
        transport = await q.call("vc_create_widgets", items=[
            {"type": "button", "caption": "⏭ Next",
             "parentID": fmap["Transport"],
             "x": 0, "y": H, "width": 140, "height": BTN_H,
             "action": "flash",
             "fgColor": "#00d4ff", "bgColor": "#003366"},
            {"type": "button", "caption": "⏹ Stop All",
             "parentID": fmap["Transport"],
             "x": 140, "y": H, "width": 140, "height": BTN_H,
             "action": "stopall", "stopAllFadeTime": 1000,
             "fgColor": "#ff4444", "bgColor": "#330000"},
        ])
        transport_ids = {"next": transport[0]["widgetID"], "stop": transport[1]["widgetID"]}
        print(f"  Transport: {transport_ids}")

        # Dimension SoloFrames + buttons
        dim_widget_ids = {}
        for row, dim_name in enumerate(dims):
            sf = await q.call("vc_create_widgets", items=[
                {"type": "soloframe", "caption": dim_name,
                 "parentID": fmap["Dimensions"],
                 "x": 0, "y": H + row * (DIM_ROW_H + FRAME_GAP),
                 "width": 900, "height": DIM_ROW_H,
                 "headerVisible": True},
            ])
            sf_id = sf[0]["widgetID"]

            btns = await q.call("vc_create_widgets", items=[
                {"type": "button",
                 "caption": f"{dim_name}: {d['label']}",
                 "parentID": sf_id,
                 "x": j * 150, "y": H, "width": 150, "height": DIM_BTN_H,
                 "action": "toggle",
                 "fgColor": d["fg"], "bgColor": "#1a1a2e"}
                for j, d in enumerate(DIMENSION_LEVELS)
            ])
            dim_widget_ids[dim_name] = {
                "frame": sf_id,
                "bad": btns[0]["widgetID"],
                "ok": btns[1]["widgetID"],
                "good": btns[2]["widgetID"],
            }
            print(f"  {dim_name}: {dim_widget_ids[dim_name]}")

        # Extra question groups (if any)
        extra_ids = {}
        if extra_questions:
            for eq in extra_questions:
                sf = await q.call("vc_create_widgets", items=[
                    {"type": "soloframe", "caption": eq["name"],
                     "parentID": fmap["Dimensions"],
                     "x": 0, "y": H + len(dims) * DIM_ROW_H,
                     "width": 900, "height": DIM_ROW_H,
                     "headerVisible": True},
                ])
                sf_id = sf[0]["widgetID"]
                btns = await q.call("vc_create_widgets", items=[
                    {"type": "button", "caption": f"{eq['name']}: {opt}",
                     "parentID": sf_id,
                     "x": j * 150, "y": H, "width": 150, "height": DIM_BTN_H,
                     "action": "toggle",
                     "fgColor": "#00d4ff", "bgColor": "#1a1a2e"}
                    for j, opt in enumerate(eq["options"])
                ])
                extra_ids[eq["name"]] = {
                    "frame": sf_id,
                    "options": {opt: btns[j]["widgetID"] for j, opt in enumerate(eq["options"])}
                }
                print(f"  Extra Q: {eq['name']}: {extra_ids[eq['name']]}")

        # ── 4. Reflow each frame individually ──
        # Reflow per-frame preserves the side-by-side layout (Rating + Transport)
        # and lets us set button sizes per context. pad=0, no internal gaps.
        print("\n── Reflow ──")
        
        # Rating: exactly 5 buttons, fill the width
        r = await q.call("vc_reflow_frame", frameID=fmap["Overall Rating"],
                         columns=5, buttonHeight=BTN_H, pad=0)
        print(f"  Rating: {r.get('widgetsMoved')} moved")
        
        # Transport: exactly 2 buttons fill width
        r = await q.call("vc_reflow_frame", frameID=fmap["Transport"],
                         columns=2, buttonHeight=BTN_H, pad=0)
        print(f"  Transport: {r.get('widgetsMoved')} moved")
        
        # Each dimension SoloFrame: exactly 3 buttons, fill width
        for dim_name, dim_ids in dim_widget_ids.items():
            r = await q.call("vc_reflow_frame", frameID=dim_ids["frame"],
                             columns=3, buttonHeight=DIM_BTN_H, pad=0)
            print(f"  {dim_name}: {r.get('widgetsMoved')} moved")
        
        # Note: we do NOT reflow the Dimensions container because reflowChildren
        # recurses and would undo our per-frame column settings above.

        # ── 5. Save state ──
        state = {
            "setup": {
                "labelId": label_id,
                "playBtnId": play_btn_id,
                "ratingFrameId": fmap["Overall Rating"],
                "ratingBtnIds": rating_ids,
                "transportIds": transport_ids,
                "dimensionIds": dim_widget_ids,
                "extraQuestionIds": extra_ids,
                "sceneIds": scene_map,
                "dimensions": dims,
            },
            "briefing": None,
            "currentRound": 0,
            "currentExperiment": None,
            "experiments": [],
            "winners": [],
        }
        with open(STATE_FILE, "w") as f:
            json.dump(state, f, indent=2)

        print(f"\n✓ AutoLight workspace ready! State → {STATE_FILE}")
        return state


# ── CLI entry point ──
if __name__ == "__main__":
    # Accept optional dimensions as CLI args
    dims = sys.argv[1:] if len(sys.argv) > 1 else None
    asyncio.run(setup(dimensions=dims))
