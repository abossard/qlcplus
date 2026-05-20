import { describe, it, expect } from 'vitest';
import { parseManualReview } from './parser';

const SAMPLE_MD = `# QLC+ Fork — Manual Review Checklist

This checklist contains ONLY items that require human judgment.

- **Unit tests:** \`cd build && ./engine/test/function_test\`

> Run all automated tests FIRST.

---

## 1. Setup

### 1.1 Start QLC+

\`\`\`bash
cd build
./qmlui/qlcplus-qml -d
\`\`\`

Flags:
- \`-d\` — debug logging to stdout

### 1.2 Load the test workspace

In QLC+:
1. **File → Open**
2. Select \`GARAGE.qxw\`

---

## 2. MCP Server — Composite Tools

> Reachability and handshake are covered by smoke tests. The items below need a running app.

### 2.1 build_show_page

- **Do:** Call with a small spec (one frame, a couple of buttons).
- **Verify in QML:** A new VC page is created with widgets laid out sensibly.
- ☐ Page is created
- ☐ No widget overlap

### 2.2 configure_launchpad

- **Prerequisite:** Novation Launchpad connected.
- **Do:** Call the tool.
- **Verify:** MIDI mappings appear under Inputs/Outputs.
- ☐ LED feedback lights up the expected pads

---

## 3. Keyboard Shortcuts

> No automated QML shortcut tests exist.

### 3.1 Global Shortcuts

Test from ANY context:

| Shortcut | Expected Behavior | Verify |
|----------|-------------------|--------|
| Ctrl+N | Opens save-first prompt | ☐ Prompt appears |
| Ctrl+S | Saves current file | ☐ Saves without dialog |
| Ctrl+Z | Undo last action | ☐ Works globally |

### 3.2 Sort modes

| Action | Expected | Check |
|--------|----------|-------|
| Select "Alphabetical" + ▲ | Songs A→Z | ☐ |
| Toggle to ▼ | Songs Z→A | ☐ |

---

## 4. Known Issues / Limitations

- **WebSocket reconnect:** Takes up to ~5s.
- **Cross-tab sync race:** Brief flicker possible.

---

## 5. Sign-off

| Area | Tester | Date | Pass / Fail | Notes |
|------|--------|------|-------------|-------|
| MCP composite tools | | | | |
| Keyboard shortcuts | | | | |

**Overall:** ☐ Ready to merge ☐ Blockers found
`;

describe('parseManualReview', () => {
  const plan = parseManualReview(SAMPLE_MD);

  it('extracts the title', () => {
    expect(plan.title).toBe('QLC+ Fork — Manual Review Checklist');
  });

  it('extracts preamble text', () => {
    expect(plan.preamble).toContain('human judgment');
  });

  it('parses sections with correct numbers and titles', () => {
    const sectionHeaders = plan.sections.map(s => ({ number: s.number, title: s.title }));
    expect(sectionHeaders).toEqual([
      { number: '1', title: 'Setup' },
      { number: '2', title: 'MCP Server — Composite Tools' },
      { number: '3', title: 'Keyboard Shortcuts' },
      { number: '4', title: 'Known Issues / Limitations' },
      { number: '5', title: 'Sign-off' },
    ]);
  });

  it('extracts context notes from blockquotes', () => {
    expect(plan.sections[1].contextNote).toContain('smoke tests');
    expect(plan.sections[2].contextNote).toContain('No automated');
  });

  it('parses test cases within sections', () => {
    const mcpSection = plan.sections[1];
    expect(mcpSection.cases).toHaveLength(2);
    expect(mcpSection.cases[0].number).toBe('2.1');
    expect(mcpSection.cases[0].title).toBe('build_show_page');
    expect(mcpSection.cases[1].number).toBe('2.2');
    expect(mcpSection.cases[1].title).toBe('configure_launchpad');
  });

  it('parses structured steps (Do, Verify, Prerequisite)', () => {
    const buildCase = plan.sections[1].cases[0];
    const stepKinds = buildCase.steps.map(s => s.kind);
    expect(stepKinds).toContain('do');
    expect(stepKinds).toContain('verify');

    const launchpadCase = plan.sections[1].cases[1];
    const lpKinds = launchpadCase.steps.map(s => s.kind);
    expect(lpKinds).toContain('prerequisite');
  });

  it('parses checkbox items from bullet lines', () => {
    const buildCase = plan.sections[1].cases[0];
    const allChecks = buildCase.steps.flatMap(s => s.checks);
    expect(allChecks).toHaveLength(2);
    expect(allChecks[0].text).toBe('Page is created');
    expect(allChecks[1].text).toBe('No widget overlap');
  });

  it('parses checkbox items from tables', () => {
    const kbSection = plan.sections[2];
    const globalCase = kbSection.cases[0];
    expect(globalCase.tableChecks.length).toBe(3);
    expect(globalCase.tableChecks[0].text).toContain('Ctrl+N');
    expect(globalCase.tableChecks[1].text).toContain('Ctrl+S');
    expect(globalCase.tableChecks[2].text).toContain('Ctrl+Z');
  });

  it('parses tables with different column names (Check vs Verify)', () => {
    const sortCase = plan.sections[2].cases[1];
    expect(sortCase.tableChecks.length).toBe(2);
    expect(sortCase.tableChecks[0].text).toContain('Alphabetical');
  });

  it('assigns unique IDs to all check items', () => {
    const allIds = new Set<string>();
    for (const section of plan.sections) {
      for (const tc of section.cases) {
        for (const step of tc.steps) {
          for (const check of step.checks) {
            expect(allIds.has(check.id)).toBe(false);
            allIds.add(check.id);
          }
        }
        for (const check of tc.tableChecks) {
          expect(allIds.has(check.id)).toBe(false);
          allIds.add(check.id);
        }
      }
    }
    expect(allIds.size).toBeGreaterThan(0);
  });

  it('counts total checkable items', () => {
    let total = 0;
    for (const section of plan.sections) {
      for (const tc of section.cases) {
        total += tc.steps.reduce((sum, s) => sum + s.checks.length, 0);
        total += tc.tableChecks.length;
      }
    }
    // 2 (build_show_page) + 1 (configure_launchpad) + 3 (global shortcuts table) + 2 (sort table) = 8
    expect(total).toBe(8);
  });
});
