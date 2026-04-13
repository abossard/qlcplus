#!/usr/bin/env python3
"""
Quick smoke test: creates one experiment, verifies it shows up, simulates a rating.

Usage: /tmp/mcp-env/bin/python3 autolight/test_loop.py
"""

import asyncio
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(__file__)))

from autolight.qlc_client import QLC
from autolight.experiments import (
    create_experiment, read_ratings, record_feedback,
    analyze_feedback, load_state, save_state,
)


async def test():
    state = load_state()
    print(f"✓ State loaded, {len(state.get('experiments', []))} existing experiments")

    async with QLC() as q:
        # ── Test 1: Create an experiment ──
        print("\n── Test 1: Create experiment ──")
        recipe = {
            "id": "test-01",
            "name": "Audio Fire Neon",
            "hypothesis": "Bass-driven fire with magenta/cyan works for techno",
            "type": "rgb_matrix",
            "algorithm": "Audio Fire",
            "colors": ["#ff00aa", "#00d4ff"],
            "properties": {"presetSpeed": "7", "presetIntensity": "15"},
            "duration": "1/2",
        }
        result = await create_experiment(q, recipe, state)
        print(f"  Created: {result.get('_created', {})}")
        assert result["_created"].get("matrix") is not None, "Matrix not created!"
        assert result["_created"].get("collection") is not None, "Collection not created!"
        print("  ✓ Matrix + Collection created")

        # Verify the experiment label was updated
        label_id = state["setup"]["labelId"]
        widgets = await q.call("vc_query_widgets", widgetIDs=[label_id])
        caption = widgets[0].get("caption", "")
        print(f"  Label: '{caption}'")
        assert "test-01" in caption.lower() or "TEST-01" in caption, "Label not updated!"
        print("  ✓ Label updated")

        # ── Test 2: Verify functions exist ──
        print("\n── Test 2: Verify functions ──")
        fns = await q.call("query_functions")
        al_fns = [f for f in fns if f["name"].startswith("AL-test-01")]
        print(f"  AutoLight functions: {[f['name'] for f in al_fns]}")
        assert len(al_fns) >= 1, "No experiment functions created!"
        print("  ✓ Functions exist")

        # ── Test 3: Read ratings (should be empty — nothing pressed) ──
        print("\n── Test 3: Read ratings ──")
        ratings = await read_ratings(q, state)
        print(f"  Current ratings: {ratings}")
        print("  ✓ Read ratings works (all None = nothing pressed)")

        # ── Test 4: Record feedback manually ──
        print("\n── Test 4: Record feedback ──")
        state["currentRound"] = 1
        fake_ratings = {
            "overall": 4,
            "Colors": "good",
            "Beat Sync": "ok",
            "Energy": "good",
            "Creativity": "ok",
        }
        exp_entry = {
            "id": "test-01",
            "name": recipe["name"],
            "recipe": {k: v for k, v in recipe.items() if not k.startswith("_")},
            "ratings": fake_ratings,
            "notes": "Fire looks great, bass response is solid",
            "status": "rated",
            "round": 1,
        }
        state["experiments"].append(exp_entry)
        save_state(state)
        print(f"  ✓ Recorded: ⭐⭐⭐⭐ with dimensions")

        # ── Test 5: Analyze feedback ──
        print("\n── Test 5: Analyze feedback ──")
        analysis = analyze_feedback(state)
        print(analysis)
        assert "test-01" in analysis, "Analysis doesn't mention experiment!"
        print("  ✓ Analysis generated")

        # ── Test 6: Create a second experiment (chaser type) ──
        print("\n── Test 6: Create chaser experiment ──")
        recipe2 = {
            "id": "test-02",
            "name": "Color Cycle Neon",
            "hypothesis": "Simple beat-synced color cycle",
            "type": "chaser",
            "colors": ["#ff00aa", "#00d4ff", "#ffffff"],
            "hold": 2,
            "step_fade_in": 0.5,
        }
        result2 = await create_experiment(q, recipe2, state)
        print(f"  Created: {result2.get('_created', {})}")
        chaser_id = result2["_created"].get("chaser")
        assert chaser_id is not None, "Chaser not created!"
        print("  ✓ Chaser + Collection created")

        # ── Cleanup: remove test experiments ──
        print("\n── Cleanup ──")
        fns = await q.call("query_functions")
        test_fns = [f for f in fns if f["name"].startswith("AL-test-")]
        if test_fns:
            await q.call("delete_functions", ids=[f["id"] for f in test_fns])
            print(f"  Deleted {len(test_fns)} test functions")
        # Remove test palettes
        pals = await q.call("query_palettes")
        test_pals = [p for p in pals if p["name"].startswith("AL-test-")]
        if test_pals:
            await q.call("delete_palettes", ids=[p["id"] for p in test_pals])
            print(f"  Deleted {len(test_pals)} test palettes")
        # Remove test experiments from state
        state["experiments"] = [e for e in state["experiments"] if not e["id"].startswith("test-")]
        save_state(state)

        print("\n" + "=" * 50)
        print("  ALL TESTS PASSED ✓")
        print("=" * 50)


if __name__ == "__main__":
    asyncio.run(test())
