# Manual Review Runner

Interactive web-based walkthrough for the QLC+ manual review checklist.

## Quick Start

```bash
cd tools/manual-review
npm install
npm run dev
```

Opens `http://localhost:5173` — the app loads `MANUAL_REVIEW.md` from the repo root automatically.

## Keyboard Shortcuts

| Key | Action |
|-----|--------|
| `j` / `↓` | Next check item |
| `k` / `↑` | Previous check item |
| `p` | Pass |
| `f` | Fail |
| `s` | Skip |
| `n` | Focus note field |
| `Ctrl+E` | Export report |
| `Ctrl+R` | Reset session |
| `?` | Toggle help |

## Workflow

1. Start the app, enter your name in the sidebar
2. Navigate through test cases using sidebar or keyboard
3. For each check item: **Pass** / **Fail** / **Skip**, add notes, attach screenshots
4. Progress auto-saves to localStorage
5. **Export Report** → writes `test-report.md`, `test-session.json`, and `screenshots/` to a directory you choose

## Report Output

The exported report includes:
- `test-report.md` — Markdown summary with ✅❌⏭ markers, notes, and screenshot references
- `test-session.json` — Machine-readable session data (for AI consumption)
- `screenshots/` — Attached evidence images

Commit the report directory to `test-reports/` in the repo for traceability.

## Tests

```bash
npm test
```
