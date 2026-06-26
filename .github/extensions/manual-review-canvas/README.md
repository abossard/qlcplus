# Manual Review Canvas

A Copilot CLI **canvas extension** that runs the repo's `MANUAL_REVIEW.md`
checklist directly inside the Copilot side panel — no separate web server, no
`npm install`. It supersedes the standalone runner in `tools/manual-review/`.

## What it does

- Parses `MANUAL_REVIEW.md` (sections / cases / steps / `☐` checkboxes / table rows).
- Interactive iframe: pass / fail / skip per item, notes, screenshots
  (file picker **and** clipboard paste), progress bar, sidebar nav.
- Keyboard shortcuts: `j`/`k` move, `p`/`f`/`s` pass/fail/skip, `n` focus note,
  `Ctrl/⌘+E` export report, `?` help.
- Live updates via Server-Sent Events — when the agent mutates a run, every open
  panel refreshes instantly.
- Agent-callable actions so Copilot can drive or read the run.

## Storage

Runs persist to `<repoRoot>/test-reports/<runId>/` — identical layout and report
format to the old tool, so reports stay compatible:

```
test-reports/
  .active-run                     # pointer to the active run (so reopening rehydrates)
  2025-01-31-143022/
    session.json                  # statuses + notes + screenshot paths
    report.md                     # generated markdown report
    screenshots/                  # uploaded / pasted images
```

`test-reports/` is gitignored. State is keyed by run id (never by canvas
`instanceId`), so closing and reopening the panel resumes the same run.

## Agent actions

| Action | Kind | Purpose |
|--------|------|---------|
| `start` | write | Begin a new run (`{ tester? }`) → returns `runId`. |
| `summary` | read | Counts (pass/fail/skip/pending), verdict, report path. |
| `list_items` | read | Flat checklist with status/note; optional `{ status }` filter. |
| `set_status` | write | `{ checkId, status?, note? }` — status ∈ pass/fail/skip/pending. |
| `get_report` | read | Markdown report for the active run. |
| `conclude` | write | Finalize + clear the active run. |

Use `list_items` to discover `checkId`s, then `set_status` to mark them.

## How it works

- `extension.mjs` — wiring: `joinSession`, `createCanvas`, one loopback HTTP
  server per open panel, the `/api/*` + `/events` routes, durable storage, and
  the agent actions. The repo root is derived from this file's own location
  (`.github/extensions/manual-review-canvas/` → repo root).
- `parser.mjs` — pure port of `tools/manual-review/src/{parser,report}.ts`.
  Produces identical check-item IDs and report output.
- `client.mjs` — `renderHtml()` returns the self-contained iframe (themed with
  app CSS tokens, inline JS, SSE client).

## Relationship to `tools/manual-review/`

This canvas replaces the Vite+Express runner. The old tool is kept for now but is
no longer the recommended path — prefer opening the **Manual Review** canvas.
