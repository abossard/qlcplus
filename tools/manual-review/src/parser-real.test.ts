import { describe, it, expect } from 'vitest';
import { readFileSync } from 'fs';
import { resolve } from 'path';
import { parseManualReview, countCheckItems, allCheckItems, allTags, filterPlan } from './parser';

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

  // ── Tag-specific tests ──

  it.skipIf(skip)('parses alphanumeric section 4B', () => {
    const plan = parseManualReview(md);
    const section4B = plan.sections.find(s => s.number === '4B');
    expect(section4B).toBeTruthy();
    expect(section4B!.title).toContain('Page');
    expect(section4B!.cases.length).toBeGreaterThanOrEqual(5);
  });

  it.skipIf(skip)('parses alphanumeric section 13b', () => {
    const plan = parseManualReview(md);
    const section13b = plan.sections.find(s => s.number === '13b');
    expect(section13b).toBeTruthy();
    expect(section13b!.title).toContain('VCAnimation');
    expect(section13b!.cases.length).toBeGreaterThanOrEqual(4);
  });

  it.skipIf(skip)('extracts all three tag types', () => {
    const plan = parseManualReview(md);
    const tags = allTags(plan);
    expect(tags).toContain('MIDI');
    expect(tags).toContain('DMX');
    expect(tags).toContain('VDJ');
    expect(tags).toHaveLength(3);
  });

  it.skipIf(skip)('section 12 has VDJ tag and all cases inherit it', () => {
    const plan = parseManualReview(md);
    const section12 = plan.sections.find(s => s.number === '12');
    expect(section12).toBeTruthy();
    expect(section12!.tags).toEqual(['VDJ']);
    for (const tc of section12!.cases) {
      expect(tc.tags).toContain('VDJ');
    }
  });

  it.skipIf(skip)('configure_launchpad case has MIDI tag', () => {
    const plan = parseManualReview(md);
    const mcpSection = plan.sections.find(s => s.title.includes('MCP'));
    expect(mcpSection).toBeTruthy();
    const launchpad = mcpSection!.cases.find(c => c.title.includes('configure_launchpad'));
    expect(launchpad).toBeTruthy();
    expect(launchpad!.tags).toEqual(['MIDI']);
  });

  it.skipIf(skip)('filtering to no tags excludes MIDI/DMX/VDJ cases', () => {
    const plan = parseManualReview(md);
    const filtered = filterPlan(plan, new Set());
    for (const section of filtered.sections) {
      for (const tc of section.cases) {
        expect(tc.tags).toEqual([]);
      }
    }
  });

  it.skipIf(skip)('tags are stripped from displayed titles', () => {
    const plan = parseManualReview(md);
    for (const section of plan.sections) {
      expect(section.title).not.toMatch(/\[MIDI\]|\[DMX\]|\[VDJ\]/);
      for (const tc of section.cases) {
        expect(tc.title).not.toMatch(/\[MIDI\]|\[DMX\]|\[VDJ\]/);
      }
    }
  });
});
