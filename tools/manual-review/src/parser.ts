import type { TestPlan, TestSection, TestCase, TestStep, CheckItem } from './types';

/**
 * Extract `[TAG]` suffixes from a title string.
 * Returns { cleanTitle, tags } where tags are uppercase bracketed words.
 */
function extractTags(raw: string): { cleanTitle: string; tags: string[] } {
  const tags: string[] = [];
  const cleanTitle = raw.replace(/\s*\[([A-Z][A-Z0-9]*)\]/g, (_, tag) => {
    tags.push(tag);
    return '';
  }).trim();
  return { cleanTitle, tags };
}

/**
 * Parse a MANUAL_REVIEW.md file into a structured TestPlan.
 *
 * Recognises:
 *  - ## N. Title         → sections (N can be alphanumeric, e.g. 4B, 13b)
 *  - ### N.M Title       → test cases
 *  - > blockquote        → context notes
 *  - - **Do:** / **Verify:** / **Prerequisite:** / **Expected:** / **Why manual:** → steps
 *  - - ☐ text            → check items
 *  - | col | ... | ☐ ... | → table check items
 *  - [TAG] suffix        → hardware requirement tags (e.g. [MIDI], [DMX], [VDJ])
 */
export function parseManualReview(markdown: string): TestPlan {
  const lines = markdown.split('\n');
  const plan: TestPlan = { title: '', preamble: '', sections: [] };
  let preambleLines: string[] = [];
  let inPreamble = true;

  let currentSection: TestSection | null = null;
  let currentCase: TestCase | null = null;
  let currentStep: TestStep | null = null;
  let caseCheckCounter = 0;
  let inCodeBlock = false;

  for (let i = 0; i < lines.length; i++) {
    const line = lines[i];

    // Track code blocks to avoid parsing their content
    if (line.trimStart().startsWith('```')) {
      inCodeBlock = !inCodeBlock;
      continue;
    }
    if (inCodeBlock) continue;

    // # Title (H1)
    const h1 = line.match(/^# (.+)/);
    if (h1) {
      plan.title = h1[1].trim();
      inPreamble = true;
      continue;
    }

    // ## N. Title (section) — N can be alphanumeric (e.g. 4B, 13b)
    const h2 = line.match(/^## (\w+)\.\s+(.+)/);
    if (h2) {
      inPreamble = false;
      flushCase(currentCase, currentStep, currentSection);
      currentCase = null;
      currentStep = null;
      const { cleanTitle, tags } = extractTags(h2[2]);
      currentSection = makeSection(h2[1], cleanTitle, tags);
      plan.sections.push(currentSection);
      continue;
    }

    // ### N.M Title (test case) — N.M can be alphanumeric (e.g. 4B.1, 13b.2)
    const h3 = line.match(/^### (\w+\.\w+)\s+(.+)/);
    if (h3 && currentSection) {
      flushCase(currentCase, currentStep, currentSection);
      currentStep = null;
      const { cleanTitle, tags } = extractTags(h3[2]);
      const mergedTags = [...new Set([...currentSection.tags, ...tags])];
      currentCase = makeCase(currentSection.number, h3[1], cleanTitle, mergedTags);
      caseCheckCounter = 0;
      continue;
    }

    // Still in preamble (before first ##)
    if (inPreamble && !currentSection) {
      if (line.trim() && !line.startsWith('#')) {
        preambleLines.push(line);
      }
      continue;
    }

    if (!currentSection) continue;

    // > Blockquote → context note (only if no current case)
    const bq = line.match(/^>\s*(.*)/);
    if (bq && !currentCase) {
      currentSection.contextNote += (currentSection.contextNote ? ' ' : '') + bq[1].trim();
      continue;
    }

    // Table row with ☐ → table check item
    if (line.includes('|') && line.includes('☐') && currentCase) {
      const cells = line.split('|').map(c => c.trim()).filter(c => c.length > 0);
      // Find the cell with ☐ and build text from the other cells
      const checkCellIndex = cells.findIndex(c => c.includes('☐'));
      const textCells = cells.filter((_, idx) => idx !== checkCellIndex);
      const checkText = textCells.join(' — ');
      // Also include any text after ☐ in the check cell
      const checkCellText = cells[checkCellIndex]?.replace('☐', '').trim();
      const fullText = checkCellText ? `${checkText} — ${checkCellText}` : checkText;

      currentCase.tableChecks.push(makeCheckItem(currentCase.id, ++caseCheckCounter, fullText));
      continue;
    }

    // Skip table header/separator rows
    if (line.match(/^\|[-\s|:]+\|$/)) continue;
    if (line.match(/^\|.*\|$/) && !line.includes('☐')) continue;

    // - ☐ text → check item (attached to current step)
    const checkbox = line.match(/^[-*]\s*☐\s+(.*)/);
    if (checkbox && currentCase) {
      const check = makeCheckItem(currentCase.id, ++caseCheckCounter, checkbox[1].trim());
      if (currentStep) {
        currentStep.checks.push(check);
      } else {
        // Orphan checkbox — create an implicit verify step
        currentStep = { kind: 'verify', text: '', checks: [check] };
        currentCase.steps.push(currentStep);
      }
      continue;
    }

    // - **Do:** / **Verify:** etc → structured step
    const stepMatch = line.match(/^[-*]\s+\*\*(\w[\w\s]*?)(?:\s+\w+)?:\*\*\s*(.*)/);
    if (stepMatch && currentCase) {
      const kind = classifyStepKind(stepMatch[1]);
      currentStep = { kind, text: stepMatch[2].trim(), checks: [] };
      currentCase.steps.push(currentStep);
      continue;
    }
  }

  // Flush the last case
  flushCase(currentCase, currentStep, currentSection);
  plan.preamble = preambleLines.join('\n');

  // Post-process: ensure every case with steps has at least one check item.
  // Cases that only have Do/Verify prose but no ☐ lines get an auto-generated
  // "Verified" check so the user can still mark them pass/fail.
  for (const section of plan.sections) {
    for (const tc of section.cases) {
      const hasChecks = tc.tableChecks.length > 0 ||
        tc.steps.some(s => s.checks.length > 0);
      const hasSteps = tc.steps.length > 0;
      if (!hasChecks && hasSteps) {
        tc.tableChecks.push(makeCheckItem(tc.id, 1, `${tc.number} ${tc.title} — verified`));
      }
    }
  }

  return plan;
}

function makeSection(number: string, title: string, tags: string[]): TestSection {
  return {
    id: `section-${number}`,
    number,
    title: title.trim(),
    tags,
    contextNote: '',
    cases: [],
  };
}

function makeCase(sectionNumber: string, number: string, title: string, tags: string[]): TestCase {
  return {
    id: `s${sectionNumber}-case-${number.replace('.', '-')}`,
    number,
    title: title.trim(),
    tags,
    steps: [],
    tableChecks: [],
  };
}

function makeCheckItem(caseId: string, caseCounter: number, text: string): CheckItem {
  return {
    id: `${caseId}-chk-${caseCounter}`,
    text,
    status: 'pending',
    note: '',
    screenshots: [],
  };
}

function classifyStepKind(label: string): TestStep['kind'] {
  const lower = label.toLowerCase().trim();
  if (lower.startsWith('do')) return 'do';
  if (lower.startsWith('verify')) return 'verify';
  if (lower.startsWith('prerequisite') || lower.startsWith('precondition')) return 'prerequisite';
  if (lower.startsWith('expected')) return 'expected';
  if (lower.startsWith('why')) return 'why-manual';
  if (lower.startsWith('note')) return 'note';
  return 'text';
}

function flushCase(
  currentCase: TestCase | null,
  _currentStep: TestStep | null,
  currentSection: TestSection | null,
): void {
  if (currentCase && currentSection) {
    currentSection.cases.push(currentCase);
  }
}

/** Count all checkable items in a test plan. */
export function countCheckItems(plan: TestPlan): number {
  let total = 0;
  for (const section of plan.sections) {
    for (const tc of section.cases) {
      total += tc.steps.reduce((sum, s) => sum + s.checks.length, 0);
      total += tc.tableChecks.length;
    }
  }
  return total;
}

/** Get a flat list of all check items in the plan. */
export function allCheckItems(plan: TestPlan): CheckItem[] {
  const items: CheckItem[] = [];
  for (const section of plan.sections) {
    for (const tc of section.cases) {
      for (const step of tc.steps) {
        items.push(...step.checks);
      }
      items.push(...tc.tableChecks);
    }
  }
  return items;
}

/** Collect all unique tags from the plan. */
export function allTags(plan: TestPlan): string[] {
  const tags = new Set<string>();
  for (const section of plan.sections) {
    for (const tag of section.tags) tags.add(tag);
    for (const tc of section.cases) {
      for (const tag of tc.tags) tags.add(tag);
    }
  }
  return [...tags].sort();
}

/**
 * Filter a plan by available equipment tags.
 * Cases with no tags (app-only) are always included.
 * Cases whose tags are ALL present in `availableTags` are included.
 * Cases with any tag NOT in `availableTags` are excluded.
 * Sections with no remaining cases are excluded.
 */
export function filterPlan(plan: TestPlan, availableTags: Set<string>): TestPlan {
  const sections: TestSection[] = [];
  for (const section of plan.sections) {
    const filteredCases = section.cases.filter(tc =>
      tc.tags.length === 0 || tc.tags.every(t => availableTags.has(t))
    );
    if (filteredCases.length > 0) {
      sections.push({ ...section, cases: filteredCases });
    }
  }
  return { ...plan, sections };
}
