import { describe, it, expect } from 'vitest';
import { parseManualReview, filterPlan, allTags } from './parser';

const SAMPLE_MD = `# QLC+ Fork — Manual Review Checklist

This checklist contains ONLY items that require human judgment.

- **Unit tests:** \`cd build && ./engine/test/function_test\`

> Run all automated tests FIRST.

---

## 1. Setup

### 1.1 Start QLC+

\`\`\`bash
cd build
./qmlui/qlcplus5 -d
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

### 2.2 configure_launchpad [MIDI]

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

## 4B. Page Input [MIDI]

### 4B.1 Normal mode [DMX]

- **Do:** Test input mapping.
- ☐ Input works

### 4B.2 Override mode

- **Do:** Test override.
- ☐ Override works

---

## 12. VDJ Playback [VDJ]

### 12.1 Auto-start

- **Do:** Play a track.
- ☐ Show starts

### 12.2 Seek handling

- **Do:** Seek in VDJ.
- ☐ Show follows

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
      { number: '4B', title: 'Page Input' },
      { number: '12', title: 'VDJ Playback' },
      { number: '5', title: 'Sign-off' },
    ]);
  });

  it('parses alphanumeric section numbers (4B)', () => {
    const section4B = plan.sections.find(s => s.number === '4B');
    expect(section4B).toBeTruthy();
    expect(section4B!.title).toBe('Page Input');
    expect(section4B!.cases).toHaveLength(2);
    expect(section4B!.cases[0].number).toBe('4B.1');
    expect(section4B!.cases[1].number).toBe('4B.2');
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
    // 2 (build_show_page) + 1 (configure_launchpad) + 3 (global shortcuts table)
    // + 2 (sort table) + 1 (4B.1) + 1 (4B.2) + 1 (12.1) + 1 (12.2) = 12
    expect(total).toBe(12);
  });
});

describe('tag parsing', () => {
  const plan = parseManualReview(SAMPLE_MD);

  it('extracts single tag from case header', () => {
    const launchpad = plan.sections[1].cases[1]; // 2.2 configure_launchpad [MIDI]
    expect(launchpad.tags).toEqual(['MIDI']);
    expect(launchpad.title).toBe('configure_launchpad');
  });

  it('extracts tag from section header', () => {
    const section4B = plan.sections.find(s => s.number === '4B')!;
    expect(section4B.tags).toEqual(['MIDI']);
    expect(section4B.title).toBe('Page Input');
  });

  it('inherits section tags to cases (union merge)', () => {
    const section4B = plan.sections.find(s => s.number === '4B')!;
    // 4B.1 has [DMX] + inherits [MIDI] from section
    expect(section4B.cases[0].tags).toContain('MIDI');
    expect(section4B.cases[0].tags).toContain('DMX');
    expect(section4B.cases[0].tags).toHaveLength(2);
    // 4B.2 has no own tag, inherits [MIDI] from section
    expect(section4B.cases[1].tags).toEqual(['MIDI']);
  });

  it('strips tags from case title', () => {
    const section4B = plan.sections.find(s => s.number === '4B')!;
    expect(section4B.cases[0].title).toBe('Normal mode');
  });

  it('sections without tags have empty array', () => {
    expect(plan.sections[0].tags).toEqual([]);
    expect(plan.sections[0].cases[0].tags).toEqual([]);
  });

  it('allTags collects all unique tags', () => {
    const tags = allTags(plan);
    expect(tags).toEqual(['DMX', 'MIDI', 'VDJ']);
  });
});

describe('filterPlan', () => {
  const plan = parseManualReview(SAMPLE_MD);

  it('shows all cases when all tags available', () => {
    const filtered = filterPlan(plan, new Set(['MIDI', 'DMX', 'VDJ']));
    const totalCases = filtered.sections.reduce((sum, s) => sum + s.cases.length, 0);
    const origCases = plan.sections.reduce((sum, s) => sum + s.cases.length, 0);
    expect(totalCases).toBe(origCases);
  });

  it('shows only untagged cases when no tags available', () => {
    const filtered = filterPlan(plan, new Set());
    for (const section of filtered.sections) {
      for (const tc of section.cases) {
        expect(tc.tags).toEqual([]);
      }
    }
    // Should exclude 2.2 [MIDI], 4B.* [MIDI/DMX], 12.* [VDJ]
    const totalCases = filtered.sections.reduce((sum, s) => sum + s.cases.length, 0);
    // Original has: 1.1,1.2 (0 tags), 2.1 (0), 2.2 (MIDI), 3.1,3.2 (0), 4B.1 (MIDI+DMX), 4B.2 (MIDI), 12.1,12.2 (VDJ)
    // untagged = 1.1, 1.2, 2.1, 3.1, 3.2 = 5
    expect(totalCases).toBe(5);
  });

  it('includes MIDI-tagged cases when MIDI is available', () => {
    const filtered = filterPlan(plan, new Set(['MIDI']));
    const caseNumbers = filtered.sections.flatMap(s => s.cases.map(c => c.number));
    expect(caseNumbers).toContain('2.2'); // [MIDI]
    expect(caseNumbers).toContain('4B.2'); // [MIDI] inherited
    expect(caseNumbers).not.toContain('4B.1'); // [MIDI,DMX] — DMX not available
    expect(caseNumbers).not.toContain('12.1'); // [VDJ]
  });

  it('excludes sections with no remaining cases', () => {
    const filtered = filterPlan(plan, new Set());
    const sectionNumbers = filtered.sections.map(s => s.number);
    expect(sectionNumbers).not.toContain('4B'); // all cases need MIDI
    expect(sectionNumbers).not.toContain('12'); // all cases need VDJ
  });

  it('handles multi-tag requirement (needs ALL tags)', () => {
    const filtered = filterPlan(plan, new Set(['MIDI', 'DMX']));
    const caseNumbers = filtered.sections.flatMap(s => s.cases.map(c => c.number));
    expect(caseNumbers).toContain('4B.1'); // [MIDI,DMX] — both available
    expect(caseNumbers).toContain('4B.2'); // [MIDI] — available
    expect(caseNumbers).not.toContain('12.1'); // [VDJ] — not available
  });
});
