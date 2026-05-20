import { describe, it, expect } from 'vitest';
import { readFileSync } from 'fs';
import { resolve } from 'path';
import { parseManualReview, countCheckItems, allCheckItems } from './parser';

describe('parseManualReview with real MANUAL_REVIEW.md', () => {
  const mdPath = resolve(__dirname, '../../../MANUAL_REVIEW.md');
  let md: string;

  try {
    md = readFileSync(mdPath, 'utf-8');
  } catch {
    md = '';
  }

  // Skip if file doesn't exist (CI environments)
  const skip = md.length === 0;

  it.skipIf(skip)('parses without errors', () => {
    const plan = parseManualReview(md);
    expect(plan.title).toBeTruthy();
    expect(plan.sections.length).toBeGreaterThan(0);
  });

  it.skipIf(skip)('finds multiple sections', () => {
    const plan = parseManualReview(md);
    expect(plan.sections.length).toBeGreaterThanOrEqual(5);
  });

  it.skipIf(skip)('finds checkable items', () => {
    const plan = parseManualReview(md);
    const total = countCheckItems(plan);
    expect(total).toBeGreaterThanOrEqual(20);
  });

  it.skipIf(skip)('all check items have unique IDs', () => {
    const plan = parseManualReview(md);
    const items = allCheckItems(plan);
    const ids = items.map(i => i.id);
    const uniqueIds = new Set(ids);
    expect(uniqueIds.size).toBe(ids.length);
  });

  it.skipIf(skip)('finds the Song Manager section', () => {
    const plan = parseManualReview(md);
    const songSection = plan.sections.find(s => s.title.includes('Song Manager'));
    expect(songSection).toBeTruthy();
    expect(songSection!.cases.length).toBeGreaterThanOrEqual(3);
  });

  it.skipIf(skip)('finds keyboard shortcuts table checks', () => {
    const plan = parseManualReview(md);
    const kbSection = plan.sections.find(s => s.title.includes('Keyboard'));
    expect(kbSection).toBeTruthy();
    const totalTableChecks = kbSection!.cases.reduce(
      (sum, tc) => sum + tc.tableChecks.length, 0
    );
    expect(totalTableChecks).toBeGreaterThanOrEqual(10);
  });
});
