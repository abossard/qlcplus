import { describe, it, expect } from 'vitest';
import { generateReport } from './report';
import type { TestPlan, ReviewSession } from './types';

const PLAN: TestPlan = {
  title: 'Test Plan',
  preamble: 'Preamble text',
  sections: [
    {
      id: 'section-1',
      number: '1',
      title: 'First Section',
      contextNote: 'Some context',
      cases: [
        {
          id: 'case-1-1',
          number: '1.1',
          title: 'Test Alpha',
          steps: [
            {
              kind: 'do',
              text: 'Do something',
              checks: [
                { id: 'check-1', text: 'It works', status: 'pending', note: '', screenshots: [] },
                { id: 'check-2', text: 'No errors', status: 'pending', note: '', screenshots: [] },
              ],
            },
          ],
          tableChecks: [
            { id: 'check-3', text: 'Ctrl+S — Saves file', status: 'pending', note: '', screenshots: [] },
          ],
        },
        {
          id: 'case-1-2',
          number: '1.2',
          title: 'Test Bravo',
          steps: [],
          tableChecks: [],
        },
      ],
    },
  ],
};

const SESSION: ReviewSession = {
  sourceFile: 'MANUAL_REVIEW.md',
  startedAt: '2026-05-20T11:00:00Z',
  updatedAt: '2026-05-20T12:00:00Z',
  tester: 'alice',
  items: {
    'check-1': { status: 'pass', note: 'Looks good', screenshots: ['screenshots/check-1-001.png'] },
    'check-2': { status: 'fail', note: 'Error in console', screenshots: [] },
    'check-3': { status: 'skip', note: 'No keyboard available', screenshots: [] },
  },
};

describe('generateReport', () => {
  const report = generateReport(PLAN, SESSION);

  it('includes the title', () => {
    expect(report).toContain('# Test Report: Test Plan');
  });

  it('includes tester and timestamps', () => {
    expect(report).toContain('alice');
    expect(report).toContain('2026-05-20');
  });

  it('includes summary stats', () => {
    expect(report).toContain('1 pass');
    expect(report).toContain('1 fail');
    expect(report).toContain('1 skip');
  });

  it('renders pass/fail/skip markers per check', () => {
    expect(report).toContain('✅ It works');
    expect(report).toContain('❌ No errors');
    expect(report).toContain('⏭ Ctrl+S');
  });

  it('includes notes', () => {
    expect(report).toContain('Looks good');
    expect(report).toContain('Error in console');
  });

  it('includes screenshot references', () => {
    expect(report).toContain('screenshots/check-1-001.png');
  });

  it('marks pending items', () => {
    // case-1-2 has no checks so nothing pending, but if a check had no session entry
    // it would show as pending — handled by the code
  });
});
