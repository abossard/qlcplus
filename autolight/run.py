#!/usr/bin/env python3
"""
AutoLight Runner — the full AutoResearch loop.

Usage:
    /tmp/mcp-env/bin/python3 -m autolight.run

Flow:
    1. Setup (create rating VC if not exists)
    2. Briefing (interactive questions)
    3. Generate experiment plan
    4. For each experiment: create → preview → rate → record
    5. Analyze → pick winners → generate next round
    6. Repeat from 4

Each round creates a git branch for safe rollback.
"""

import asyncio
import json
import os
import subprocess
import sys

from .qlc_client import QLC
from .setup import setup, STATE_FILE
from .experiments import (
    create_experiment, read_ratings, record_feedback,
    pick_winners, analyze_feedback, load_state, save_state,
)

# ── Experiment templates ──
# These are the building blocks the AI loop picks from

ALGORITHM_POOL = [
    "Audio Fire", "Audio Plasma", "Audio Spectrum", "Audio Vortex",
    "Audio Blocks", "Audio Strobe", "Audio Energy", "Audio Water",
    "Audio Aurora", "Audio Glitch", "Audio Melt", "Audio Tunnel",
    "Audio Wavelength", "Audio Equalizer", "Audio Chaser", "Audio Shot",
]

COLOR_PALETTES = {
    "neon":   ["#ff00aa", "#00d4ff", "#ffffff"],
    "warm":   ["#ff4400", "#ffaa00", "#ff0066"],
    "cool":   ["#0044ff", "#00ffcc", "#9900ff"],
    "forest": ["#00aa00", "#228b22", "#ffff00"],
    "sunset": ["#ff6600", "#ff0066", "#ffcc00"],
    "ice":    ["#aaddff", "#0088ff", "#ffffff"],
    "fire":   ["#ff0000", "#ff6600", "#ffff00"],
    "purple": ["#9900ff", "#ff00aa", "#6600cc"],
}


def git(cmd: str, cwd: str = None) -> str:
    """Run a git command, return stdout."""
    result = subprocess.run(
        ["git", "--no-pager"] + cmd.split(),
        capture_output=True, text=True,
        cwd=cwd or os.path.dirname(os.path.dirname(__file__)),
    )
    return result.stdout.strip()


def git_branch(name: str):
    """Create and checkout a git branch (from current HEAD)."""
    git(f"checkout -b {name}")


def git_commit(message: str):
    """Stage and commit autolight state files."""
    git("add autolight-state.json")
    git(f"commit -m {message} --allow-empty")


def prompt(question: str, options: list[str] | None = None, default: str = "") -> str:
    """Interactive CLI prompt."""
    if options:
        print(f"\n{question}")
        for i, opt in enumerate(options, 1):
            marker = " (default)" if opt == default else ""
            print(f"  {i}. {opt}{marker}")
        raw = input(f"Choice [1-{len(options)}]: ").strip()
        if not raw and default:
            return default
        try:
            return options[int(raw) - 1]
        except (ValueError, IndexError):
            return default or options[0]
    else:
        raw = input(f"{question} [{default}]: ").strip()
        return raw or default


def briefing_interactive() -> dict:
    """Collect show briefing interactively."""
    print("\n╔══════════════════════════════════╗")
    print("║   AutoLight — Show Briefing      ║")
    print("╚══════════════════════════════════╝\n")

    genre = prompt("Music genre?",
                   ["Techno", "House", "DnB", "Ambient", "Hip-Hop", "Pop", "Rock", "Mixed"],
                   "House")
    energy = prompt("Energy level?",
                    ["Aggressive", "Balanced", "Ambient"],
                    "Balanced")
    palette = prompt("Color palette?",
                     list(COLOR_PALETTES.keys()),
                     "neon")
    reactivity = prompt("Audio reactivity?",
                        ["High", "Medium", "Low", "None"],
                        "Medium")
    bpm = prompt("Typical BPM?", default="120-128")

    dims = prompt("Rating dimensions? (comma-separated, or Enter for defaults)",
                  default="Colors, Beat Sync, Energy, Creativity")
    dimensions = [d.strip() for d in dims.split(",")]

    return {
        "genre": genre,
        "energy": energy,
        "palette": palette,
        "colors": COLOR_PALETTES.get(palette, COLOR_PALETTES["neon"]),
        "reactivity": reactivity,
        "bpm": bpm,
        "dimensions": dimensions,
    }


def generate_round_experiments(briefing: dict, round_num: int,
                               previous_results: str = "") -> list[dict]:
    """
    Generate experiment recipes for a round.

    Round 1: broad exploration (3-4 different algorithms).
    Round 2+: refine based on previous results.
    """
    colors = briefing["colors"]
    reactivity = briefing["reactivity"]
    energy = briefing["energy"]

    # Speed based on energy
    speed = {"Aggressive": "5", "Balanced": "7", "Ambient": "3"}.get(energy, "5")
    intensity = {"Aggressive": "15", "Balanced": "10", "Ambient": "5"}.get(energy, "10")
    duration = {"Aggressive": "1/4", "Balanced": "1/2", "Ambient": "1"}.get(energy, "1/2")

    if round_num == 1:
        # Broad exploration: try different algorithm families
        algos = {
            "High":    ["Audio Fire", "Audio Strobe", "Audio Energy"],
            "Medium":  ["Audio Spectrum", "Audio Plasma", "Audio Blocks"],
            "Low":     ["Audio Water", "Audio Aurora", "Audio Melt"],
            "None":    ["Audio Plasma", "Audio Blocks", "Audio Tunnel"],
        }
        chosen = algos.get(reactivity, algos["Medium"])

        experiments = []
        for i, algo in enumerate(chosen):
            experiments.append({
                "id": f"r{round_num}-{i+1:02d}",
                "name": f"{algo} {briefing['palette'].title()}",
                "hypothesis": f"{algo} with {briefing['palette']} palette at {energy.lower()} energy",
                "type": "rgb_matrix",
                "algorithm": algo,
                "colors": colors[:2],
                "properties": {"presetSpeed": speed, "presetIntensity": intensity},
                "duration": duration,
            })

        # Also add a color chaser for comparison
        experiments.append({
            "id": f"r{round_num}-{len(chosen)+1:02d}",
            "name": f"Color Chaser {briefing['palette'].title()}",
            "hypothesis": f"Simple beat-synced color cycle with {briefing['palette']} palette",
            "type": "chaser",
            "colors": colors,
            "hold": 2,
            "step_fade_in": 0.5,
        })

        return experiments
    else:
        # Later rounds: the caller (AI) should generate these based on analysis
        # For now, return a template that can be filled in
        return [{
            "id": f"r{round_num}-01",
            "name": "Refinement — customize me",
            "hypothesis": "Based on round " + str(round_num - 1) + " feedback",
            "type": "rgb_matrix",
            "algorithm": "Audio Fire",
            "colors": colors[:2],
            "properties": {"presetSpeed": speed},
            "duration": duration,
        }]


async def run_experiment_loop(q: QLC, state: dict, experiments: list[dict]):
    """Run through a list of experiments, collecting ratings for each."""
    for i, recipe in enumerate(experiments):
        exp_id = recipe["id"]
        print(f"\n{'='*50}")
        print(f"  Experiment {i+1}/{len(experiments)}: {exp_id}")
        print(f"  {recipe['name']}")
        print(f"  Hypothesis: {recipe['hypothesis']}")
        print(f"{'='*50}")

        # Create the experiment effects
        recipe = await create_experiment(q, recipe, state)

        # Start the collection
        collection_id = recipe.get("_created", {}).get("collection")
        if collection_id is not None:
            print(f"  ✓ Created (collection ID={collection_id})")
            print(f"  → Effect is active. Play your music!")
        else:
            print(f"  ⚠ Created but no collection — activate manually")

        # Wait for user rating
        print(f"\n  Rate this experiment in QLC+ Virtual Console,")
        print(f"  then press Enter here (or type notes)...")
        notes = input("  Notes (or Enter to skip): ").strip()

        # Read ratings from VC
        ratings = await read_ratings(q, state)
        if ratings["overall"] is None:
            # Ask in CLI as fallback
            try:
                r = int(input("  Overall rating (1-5): ").strip())
                ratings["overall"] = max(1, min(5, r))
            except (ValueError, EOFError):
                ratings["overall"] = 3

        # Record
        recipe_slim = {k: v for k, v in recipe.items() if not k.startswith("_")}
        exp_entry = {
            "id": exp_id,
            "name": recipe["name"],
            "recipe": recipe_slim,
            "ratings": ratings,
            "notes": notes,
            "status": "rated",
            "round": state["currentRound"],
        }
        state["experiments"].append(exp_entry)
        save_state(state)

        stars = "⭐" * (ratings["overall"] or 0)
        print(f"\n  ✓ Recorded: {stars} ({ratings['overall']}/5)")
        dims_str = ", ".join(f"{k}={v}" for k, v in ratings.items()
                            if k != "overall" and v)
        if dims_str:
            print(f"    Dimensions: {dims_str}")


async def main():
    print("╔══════════════════════════════════╗")
    print("║   AutoLight — LED Research Loop  ║")
    print("╚══════════════════════════════════╝")

    # Check if state exists
    if os.path.exists(STATE_FILE):
        state = load_state()
        if state.get("setup"):
            print("✓ Existing AutoLight workspace found")
        else:
            print("Setting up workspace...")
            state = await setup()
    else:
        print("First run — setting up workspace...")
        state = await setup()

    # Briefing
    if not state.get("briefing"):
        briefing = briefing_interactive()
        state["briefing"] = briefing
        save_state(state)
    else:
        briefing = state["briefing"]
        print(f"\n✓ Using existing briefing: {briefing['genre']} / {briefing['energy']} / {briefing['palette']}")
        redo = input("  Redo briefing? (y/N): ").strip().lower()
        if redo == "y":
            briefing = briefing_interactive()
            state["briefing"] = briefing
            save_state(state)

    # ── Main loop: rounds ──
    while True:
        state["currentRound"] += 1
        round_num = state["currentRound"]
        save_state(state)

        print(f"\n{'#'*50}")
        print(f"  ROUND {round_num}")
        print(f"{'#'*50}")

        # Create git branch for this round
        branch_name = f"autolight/round-{round_num}"
        try:
            git_branch(branch_name)
            print(f"  ✓ Git branch: {branch_name}")
        except Exception:
            print(f"  ⚠ Git branch failed (continuing without)")

        # Generate experiments
        previous_analysis = analyze_feedback(state) if round_num > 1 else ""
        if previous_analysis:
            print(f"\n{previous_analysis}")

        experiments = generate_round_experiments(briefing, round_num, previous_analysis)
        print(f"\n  {len(experiments)} experiments planned:")
        for e in experiments:
            print(f"    {e['id']}: {e['name']}")

        # Run experiments
        async with QLC() as q:
            await run_experiment_loop(q, state, experiments)

        # Commit state
        try:
            git_commit(f"autolight: round {round_num} ratings")
        except Exception:
            pass

        # Analyze
        analysis = analyze_feedback(state)
        print(f"\n{analysis}")

        # Pick winners
        winners = pick_winners(state)
        if winners:
            print(f"\n  🏆 Winner: {winners[0]['id']} — {winners[0].get('name', '?')}")

        # Continue?
        cont = input("\n  Start next round? (Y/n/q): ").strip().lower()
        if cont in ("n", "q"):
            break

    print("\n✓ AutoLight session complete!")
    print(f"  {len(state['experiments'])} experiments across {state['currentRound']} rounds")
    print(f"  Winners: {state['winners']}")
    print(f"  State saved to: {STATE_FILE}")


if __name__ == "__main__":
    asyncio.run(main())
