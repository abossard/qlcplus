"""
AutoLight experiment engine — create, run, rate, and iterate LED effects.

Usage:
    from autolight.experiments import create_experiment, read_ratings, record_feedback

    # Create an experiment
    exp = await create_experiment(q, {
        "id": "exp-01",
        "name": "Audio Fire Neon",
        "hypothesis": "Bass-driven fire with magenta/cyan",
        "type": "rgb_matrix",
        "algorithm": "Audio Fire",
        "colors": ["#ff00aa", "#00d4ff"],
        "properties": {"presetSpeed": "7", "presetIntensity": "15"},
    })

    # Read which rating buttons the user pressed
    ratings = await read_ratings(q, state)

    # Record and advance
    record_feedback(state, "exp-01", ratings, notes="loved the bass response")
"""

import json
import os

from .qlc_client import QLC

STATE_FILE = os.path.join(os.path.dirname(__file__), "..", "autolight-state.json")


def load_state() -> dict:
    with open(STATE_FILE) as f:
        return json.load(f)


def save_state(state: dict):
    with open(STATE_FILE, "w") as f:
        json.dump(state, f, indent=2)


async def create_experiment(q: QLC, recipe: dict, state: dict | None = None) -> dict:
    """
    Create a single experiment's effects in QLC+.

    recipe keys:
        id:          str — experiment ID (e.g. "exp-01")
        name:        str — human-readable name
        hypothesis:  str — what we're testing
        type:        str — "rgb_matrix", "script", "chaser", or "collection"
        algorithm:   str — RGB algorithm name (for rgb_matrix type)
        colors:      list[str] — hex colors
        properties:  dict — algorithm-specific properties
        script:      str — JavaScript code (for script type)
        duration:    str — beat duration string (default "1/2")
        fade_in:     str — beat fade in (default "0")

    Returns the recipe dict augmented with created function IDs.
    """
    exp_id = recipe["id"]
    exp_name = recipe["name"]
    exp_type = recipe.get("type", "rgb_matrix")
    colors = recipe.get("colors", ["#ff0000", "#00ff00"])

    created_ids = {}

    # Create color palettes for this experiment
    palette_items = [
        {"name": f"AL-{exp_id}-color-{i}", "type": "Color", "rgb": c}
        for i, c in enumerate(colors)
    ]
    if palette_items:
        await q.call("create_palettes", items=palette_items)

    if exp_type == "rgb_matrix":
        algorithm = recipe.get("algorithm", "Audio Fire")
        props = recipe.get("properties", {})
        duration = recipe.get("duration", "1/2")
        fade_in = recipe.get("fade_in", "0")

        # Query fixture groups to find WLED
        groups = await q.call("query_fixture_groups")
        group_id = groups[0]["id"] if groups else 0

        matrices = await q.call("create_rgb_matrices", items=[{
            "name": f"AL-{exp_id}-{algorithm}",
            "algorithm": algorithm,
            "fixtureGroupID": group_id,
            "startColor": colors[0] if len(colors) > 0 else "#ff0000",
            "endColor": colors[1] if len(colors) > 1 else "#000000",
            "duration": duration,
            "fadeIn": fade_in,
            "properties": props,
            "path": f"AutoLight/Experiments/{exp_id}",
        }])
        created_ids["matrix"] = matrices[0].get("id") if matrices else None

    elif exp_type == "script":
        scripts = await q.call("create_scripts", items=[{
            "name": f"AL-{exp_id}-script",
            "content": recipe.get("script", "Engine.waitTime(1000);"),
            "path": f"AutoLight/Experiments/{exp_id}",
        }])
        created_ids["script"] = scripts[0].get("id") if scripts else None

    elif exp_type == "chaser":
        # Create scenes first, then chain into chaser
        scene_items = []
        for i, c in enumerate(colors):
            scene_items.append({
                "name": f"AL-{exp_id}-step-{i}",
                "fixtureNames": ["WLED*"],
                "paletteNames": [f"AL-{exp_id}-color-{i}", "Full"],
                "path": f"AutoLight/Experiments/{exp_id}",
                "fadeIn": int(recipe.get("scene_fade_in", 200)),
                "fadeOut": int(recipe.get("scene_fade_out", 200)),
            })
        scenes = await q.call("create_scenes", items=scene_items)
        steps = [{"functionName": s["name"], "hold": recipe.get("hold", 2),
                  "fadeIn": recipe.get("step_fade_in", 0.5)}
                 for s in scenes]
        chasers = await q.call("create_chasers", items=[{
            "name": f"AL-{exp_id}-chaser",
            "steps": steps,
            "tempoType": "beats",
            "runOrder": "loop",
            "path": f"AutoLight/Experiments/{exp_id}",
        }])
        created_ids["chaser"] = chasers[0].get("id") if chasers else None

    # Wrap in a collection for single-button activation
    func_ids_for_collection = [v for v in created_ids.values() if v is not None and v != "collection"]
    collections = await q.call("create_collections", items=[{
        "name": f"AL-{exp_id}",
        "functionIDs": func_ids_for_collection,
        "path": f"AutoLight/Experiments/{exp_id}",
    }])
    created_ids["collection"] = collections[0].get("id") if collections else None

    # Update experiment label and bind play button to collection
    if state and "setup" in state:
        updates = [{
            "widgetID": state["setup"]["labelId"],
            "caption": f"{exp_id.upper()}: {exp_name} — {recipe.get('hypothesis', '')}",
        }]
        if created_ids.get("collection") is not None and state["setup"].get("playBtnId"):
            updates.append({
                "widgetID": state["setup"]["playBtnId"],
                "functionID": created_ids["collection"],
            })
        await q.call("vc_update_widgets", items=updates)

    recipe["_created"] = created_ids
    return recipe


async def read_ratings(q: QLC, state: dict) -> dict:
    """
    Read which rating buttons are currently active in the VC.

    Returns:
        {
            "overall": 3,          # 1-5 or None
            "Colors": "good",      # "bad"/"ok"/"good" or None
            "Beat Sync": "ok",
            ...
        }
    """
    setup = state["setup"]
    ratings = {"overall": None}

    # Check overall rating buttons
    btn_ids = list(setup["ratingBtnIds"].values())
    widgets = await q.call("vc_query_widgets", widgetIDs=btn_ids)
    for i, w in enumerate(widgets):
        # A toggle button is "active" if its function is running
        if w.get("functionRunning", False):
            ratings["overall"] = i + 1

    # Check dimension buttons
    for dim_name, dim_ids in setup["dimensionIds"].items():
        ratings[dim_name] = None
        dim_btn_ids = [dim_ids["bad"], dim_ids["ok"], dim_ids["good"]]
        dim_widgets = await q.call("vc_query_widgets", widgetIDs=dim_btn_ids)
        for level, w in zip(["bad", "ok", "good"], dim_widgets):
            if w.get("functionRunning", False):
                ratings[dim_name] = level

    return ratings


def record_feedback(
    state: dict,
    experiment_id: str,
    ratings: dict,
    notes: str = "",
) -> dict:
    """Record ratings for an experiment and save state."""
    exp = next((e for e in state["experiments"] if e["id"] == experiment_id), None)
    if exp is None:
        exp = {"id": experiment_id, "status": "rated"}
        state["experiments"].append(exp)

    exp["ratings"] = ratings
    exp["notes"] = notes
    exp["status"] = "rated"

    save_state(state)
    return exp


def pick_winners(state: dict, top_n: int = 1) -> list[dict]:
    """Pick the top-N experiments by overall rating from the current round."""
    rated = [e for e in state["experiments"]
             if e.get("status") == "rated" and e.get("ratings", {}).get("overall")]
    rated.sort(key=lambda e: e["ratings"]["overall"], reverse=True)
    winners = rated[:top_n]
    state["winners"].extend([w["id"] for w in winners])
    save_state(state)
    return winners


def analyze_feedback(state: dict) -> str:
    """
    Analyze all rated experiments and return a summary with recommendations.
    Useful for feeding back into the AI to generate the next round.
    """
    rated = [e for e in state["experiments"] if e.get("status") == "rated"]
    if not rated:
        return "No experiments rated yet."

    lines = [f"## Round {state['currentRound']} Results\n"]
    for e in rated:
        r = e.get("ratings", {})
        overall = r.get("overall", "?")
        dims = {k: v for k, v in r.items() if k != "overall" and v}
        dims_str = ", ".join(f"{k}={v}" for k, v in dims.items()) if dims else "no dimension ratings"
        lines.append(f"- **{e['id']}** ({e.get('name', '?')}): ⭐{overall}/5 [{dims_str}]")
        if e.get("notes"):
            lines.append(f"  Notes: _{e['notes']}_")
        if e.get("recipe"):
            lines.append(f"  Recipe: {e['recipe'].get('algorithm', '?')}, colors={e['recipe'].get('colors', '?')}")

    # Recommendations
    best = max(rated, key=lambda e: e.get("ratings", {}).get("overall", 0))
    worst = min(rated, key=lambda e: e.get("ratings", {}).get("overall", 0))

    lines.append(f"\n### Winner: {best['id']} (⭐{best['ratings']['overall']})")
    lines.append(f"### Weakest: {worst['id']} (⭐{worst['ratings']['overall']})")

    # Dimension analysis
    dim_scores = {}
    for e in rated:
        for k, v in e.get("ratings", {}).items():
            if k == "overall":
                continue
            score = {"bad": 1, "ok": 2, "good": 3}.get(v, 0)
            if score:
                dim_scores.setdefault(k, []).append((e["id"], score))

    if dim_scores:
        lines.append("\n### Dimension Insights")
        for dim, scores in dim_scores.items():
            avg = sum(s for _, s in scores) / len(scores)
            label = "weak" if avg < 1.5 else "ok" if avg < 2.5 else "strong"
            lines.append(f"- **{dim}**: avg={avg:.1f}/3 ({label})")
            if avg < 2:
                lines.append(f"  → Recommendation: focus next round on improving {dim}")

    return "\n".join(lines)
