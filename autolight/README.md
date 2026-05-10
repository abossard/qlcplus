# AutoLight — Agent-Guided LED Effect Research for QLC+

AutoLight is now a **lighting research guide + preset export workflow**, not a standalone experiment runner. The AI agent (Copilot CLI, Claude, or another MCP-capable assistant) is the researcher: it talks with the user, queries QLC+ through MCP tools, creates small effect experiments, asks for preview feedback, and saves the winners.

The full workflow lives in [`docs/lighting-research-guide.md`](../docs/lighting-research-guide.md).

## New Philosophy

- **Guide over runner** — the lighting design process is documented for agents instead of hidden in a Python loop.
- **Agent uses MCP directly** — agents call `query_fixtures`, `query_fixture_groups`, `query_rgb_algorithms`, `create_rgb_matrices`, `create_scenes`, `create_chasers`, `create_collections`, `query_functions`, and `delete_functions` themselves.
- **Human feedback stays central** — the agent asks the user to preview each experiment and explain what works.
- **Experiments are temporary** — use the `EXP-{round}-{letter} {description}` naming pattern so rejected ideas are easy to find and delete.
- **Presets are durable** — winning recipes are saved to `autolight-presets.json` with `autolight/export_winners.py`.

## Recommended Workflow

1. Start QLC+ v5 with the MCP server enabled and fixtures patched.
2. Ask an MCP-capable agent to create or improve a lighting effect.
3. The agent reads `docs/lighting-research-guide.md`.
4. The agent creates 3–5 `EXP-` experiments through QLC+ MCP tools.
5. Preview each experiment in QLC+ and tell the agent which one you prefer and why.
6. Repeat with refinements until a winner is good enough.
7. Export the winners:

```bash
python3 -m autolight.export_winners
```

For non-interactive export, pass ids or exact names:

```bash
python3 -m autolight.export_winners --keep "12,EXP-2-B Audio Fire Slower Fade"
```

The script clones supported winning `EXP-` functions as permanent names, removes the temporary `EXP-` functions, and appends recipes to `autolight-presets.json`.

## What Still Exists

| File | Current role |
|------|--------------|
| `qlc_client.py` | Still useful: small async MCP client used by export tooling. |
| `export_winners.py` | New lightweight preset exporter. |
| `run.py` | Legacy complex research runner; superseded by the agent workflow. |
| `experiments.py` | Legacy experiment engine; superseded by direct MCP calls from the agent. |
| `setup.py` | Legacy VC rating setup; usually unnecessary because the agent asks the user directly in chat. |
| `test_loop.py` | Legacy smoke test; can be simplified around the new export flow later. |

## Legacy Runner

The old Python runner still works for historical workflows, but it is no longer the preferred path. Prefer the agent-led process because it is simpler, easier to adapt during a session, and avoids maintaining a second research engine outside the AI agent.

See `docs/lighting-research-guide.md` for the full effect catalog (45 audio algorithms), genre guide, and experiment templates.

If you use the legacy runner, install its dependencies from `autolight/requirements.txt` and run the existing `python3 -m autolight setup` / `python3 -m autolight` commands.
