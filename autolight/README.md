# AutoLight — Iterative LED Effect Research for QLC+

AutoLight is a Python CLI tool that uses the QLC+ MCP server to run structured
A/B-style experiments on LED effects. It automates the **create → preview →
rate → iterate** loop for finding the best-looking effects for your fixture
setup.

## Prerequisites

| Requirement | Details |
|-------------|---------|
| **QLC+ (v5 QML)** | Built with `-Dqmlui=ON -Dmcp_server=ON` and running. MCP server auto-starts on `http://localhost:9696/mcp`. |
| **Patched fixtures** | At least one universe with fixtures patched (e.g. WLED strips, LED pars). AutoLight targets fixtures matching `WLED*` by default. |
| **Python 3.10+** | For `match` statements and `X | Y` union type hints. |
| **MCP SDK** | `pip install mcp` — the only dependency. |

## Quick Start

```bash
# 1. Install dependencies (one-time)
python3 -m venv .venv
source .venv/bin/activate
pip install -r autolight/requirements.txt

# 2. Make sure QLC+ is running with fixtures patched
#    (MCP server should be listening on http://localhost:9696/mcp)

# 3. Create the rating UI in QLC+ Virtual Console
python3 -m autolight setup

# 4. Start the research loop
python3 -m autolight
```

If you prefer an isolated temp venv:
```bash
python3 -m venv /tmp/mcp-env
/tmp/mcp-env/bin/pip install -r autolight/requirements.txt
/tmp/mcp-env/bin/python3 -m autolight setup
/tmp/mcp-env/bin/python3 -m autolight
```

## How It Works

```
┌─────────────────────────────────────────────────┐
│                  AutoLight Loop                 │
│                                                 │
│  1. Setup ──► rating UI in QLC+ Virtual Console │
│  2. Briefing ──► genre, energy, palette, BPM    │
│  3. Generate ──► 3-4 experiments per round      │
│  4. Preview ──► effects run live on fixtures     │
│  5. Rate ──► 1-5 stars + dimension feedback     │
│  6. Analyze ──► pick winners, refine next round │
│  └──────────────────── repeat ───────────────┘  │
└─────────────────────────────────────────────────┘
```

### Step 1: Setup

`python3 -m autolight setup` creates a rating workspace in the QLC+ Virtual
Console:

- **Experiment frame** — label showing current experiment name + hypothesis
- **Overall Rating** (solo-frame) — 5 star buttons (⭐ to ⭐⭐⭐⭐⭐), only one active at a time
- **Transport** — Play/Stop buttons bound to the current experiment
- **Dimensions** — per-dimension solo-frames (Colors, Beat Sync, Energy, Creativity)
  with Bad/OK/Good buttons

All widgets are idempotent — re-running setup is safe.

### Step 2: Briefing

Interactive CLI questionnaire:

| Question | Options | Default |
|----------|---------|---------|
| Music genre | Techno, House, DnB, Ambient, Hip-Hop, Pop, Rock, Mixed | House |
| Energy level | Aggressive, Balanced, Ambient | Balanced |
| Color palette | neon, warm, cool, forest, sunset, ice, fire, purple | neon |
| Audio reactivity | High, Medium, Low, None | Medium |
| Typical BPM | free text | 120-128 |
| Rating dimensions | comma-separated | Colors, Beat Sync, Energy, Creativity |

### Step 3: Experiments

Each round generates 3–4 experiments based on the briefing. Round 1 does broad
exploration across algorithm families; later rounds refine based on ratings.

Experiment types:
- **rgb_matrix** — Audio-reactive RGB Matrix with algorithm, colors, speed, intensity
- **chaser** — Color cycle chaser with beat-synced steps
- **script** — Custom JavaScript script
- **collection** — Wraps any experiment for single-button activation

### Step 4: Rate

Preview each experiment live on your fixtures while music plays. Rate in the
QLC+ Virtual Console (click star buttons + dimension buttons), then press
Enter in the CLI. If no VC rating is detected, you can type a 1–5 rating
in the terminal as a fallback.

### Step 5: Iterate

After all experiments in a round are rated, AutoLight:
1. Analyzes dimension scores to find strengths and weaknesses
2. Picks the winner(s)
3. Creates a git branch (`autolight/round-N`) for safe rollback
4. Generates the next round's experiments based on feedback

## Custom Dimensions

Pass custom rating dimensions to match your use case:

```bash
# CLI args
python3 -m autolight setup Warmth Movement Surprise Complexity

# Or answer the briefing prompt:
#   "Rating dimensions? (comma-separated, or Enter for defaults)"
#   > Warmth, Movement, Surprise, Complexity
```

## Smoke Test

Verify your setup works end-to-end without manual interaction:

```bash
python3 autolight/test_loop.py
```

This creates a test experiment, verifies functions were created in QLC+,
reads ratings, records fake feedback, runs analysis, and cleans up.

## Architecture

| File | Purpose |
|------|---------|
| `__main__.py` | Entry point for `python -m autolight` — routes to setup or run |
| `qlc_client.py` | Async MCP client — wraps `ClientSession` with `streamablehttp_client` |
| `setup.py` | Creates rating VC widgets (star buttons, dimension solo-frames, transport) |
| `experiments.py` | Create/rate/analyze experiments — `create_experiment()`, `read_ratings()`, `pick_winners()` |
| `run.py` | Main CLI loop — briefing → rounds → experiments → analysis |
| `test_loop.py` | Smoke test — creates experiment, verifies, reads ratings, cleans up |

## State File

All session state is persisted in `autolight-state.json` (repo root). Structure:

```json
{
  "setup": { "labelId": 42, "ratingBtnIds": {...}, ... },
  "briefing": { "genre": "House", "energy": "Balanced", ... },
  "currentRound": 2,
  "experiments": [
    { "id": "r1-01", "name": "Audio Fire Neon", "ratings": { "overall": 4, ... } }
  ],
  "winners": ["r1-01"]
}
```

## Color Palettes

Built-in palettes available in the briefing:

| Name | Colors |
|------|--------|
| neon | `#ff00aa` `#00d4ff` `#ffffff` |
| warm | `#ff4400` `#ffaa00` `#ff0066` |
| cool | `#0044ff` `#00ffcc` `#9900ff` |
| forest | `#00aa00` `#228b22` `#ffff00` |
| sunset | `#ff6600` `#ff0066` `#ffcc00` |
| ice | `#aaddff` `#0088ff` `#ffffff` |
| fire | `#ff0000` `#ff6600` `#ffff00` |
| purple | `#9900ff` `#ff00aa` `#6600cc` |
