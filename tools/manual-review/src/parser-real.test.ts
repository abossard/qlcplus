import { describe, it, expect } from 'vitest';
import { readFileSync } from 'fs';
import { resolve } from 'path';
import { pathToFileURL } from 'url';
import { execFileSync } from 'child_process';
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

  it.each([
    { number: '50', commit: 'c984562d2',
      cases: ['50.1', '50.2', '50.3', '50.4', '50.5'],
      checks: [2, 2, 2, 2, 1], tags: [['MIDI'], ['MIDI'], ['DMX'], [], []] },
    { number: '49', commit: 'd86decb38',
      cases: ['49.1', '49.2', '49.3'],
      checks: [2, 2, 1], tags: [[], ['DMX'], []] },
    { number: '48', commit: '4e09ecbf0',
      cases: ['48.1', '48.2', '48.3', '48.4'],
      checks: [2, 2, 2, 1], tags: [[], [], [], []] },
    // 47.3 to 47.5 need the set recorded with VirtualDJ in 47.2
    { number: '47', commit: '162a4fe22',
      cases: ['47.1', '47.2', '47.3', '47.4', '47.5', '47.6', '47.7'],
      checks: [3, 3, 3, 2, 1, 2, 1],
      tags: [['VDJ', 'DMX'], ['VDJ'], ['VDJ', 'DMX'], ['VDJ'], ['VDJ'], ['VDJ'], []] },
    { number: '46', commit: '4cd37df23',
      cases: ['46.1', '46.2', '46.3'], checks: [2, 2, 1], tags: [['DMX'], ['DMX'], []] },
    { number: '45', commit: '302a18c98',
      cases: ['45.1', '45.2'], checks: [2, 1], tags: [[], []] },
    { number: '44', commit: 'fb21860d8',
      cases: ['44.1', '44.2', '44.3', '44.4', '44.5', '44.6'],
      checks: [2, 2, 2, 2, 2, 1], tags: [['DMX'], [], ['DMX'], ['DMX'], [], []] },
    { number: '43', commit: '2b447118e',
      cases: ['43.1', '43.2', '43.3', '43.4', '43.5', '43.6'],
      checks: [2, 2, 2, 1, 2, 1], tags: [[], ['MIDI'], [], [], ['DMX'], []] },
  ])('recognizes the section $number ($commit) review cases', ({ number, commit, cases, checks, tags }) => {
    const section = parseManualReview(md).sections.find(s => s.number === number);
    expect(section?.title).toContain(commit);
    expect(section?.cases.map(c => c.number)).toEqual(cases);
    expect(section!.cases.map(c => c.steps.flatMap(s => s.checks).length)).toEqual(checks);
    expect(section!.cases.map(c => c.tags)).toEqual(tags);
    expect(section!.cases.flatMap(c => c.steps.flatMap(s => s.checks)).every(c => c.status === 'pending')).toBe(true);
  });

  it('recognizes the upstream 0eee616b5 integration review cases', () => {
    const section = parseManualReview(md).sections.find(s => s.number === '42');
    expect(section?.title).toContain('0eee616b5');
    expect(section?.cases.map(c => c.number)).toEqual([
      '42.1', '42.2', '42.3', '42.4', '42.5', '42.6',
    ]);
    expect(section!.cases.map(c => c.steps.flatMap(s => s.checks).length)).toEqual([2, 2, 2, 2, 2, 1]);
    expect(section!.cases.map(c => c.tags)).toEqual([['DMX'], ['DMX'], [], [], [], []]);
    expect(section!.cases.flatMap(c => c.steps.flatMap(s => s.checks)).every(c => c.status === 'pending')).toBe(true);
  });

  it.skipIf(skip)('parses without errors', () => {
    const plan = parseManualReview(md);
    expect(plan.title).toBeTruthy();
    expect(plan.sections.length).toBeGreaterThan(0);
  });

  it.skipIf(skip)('finds multiple sections', () => {
    const plan = parseManualReview(md);
    expect(plan.sections.length).toBeGreaterThanOrEqual(5);
  });

  it.skipIf(skip)('recognizes the upstream 30581dd38 review cases', () => {
    const section = parseManualReview(md).sections.find(s => s.number === '29');
    expect(section?.cases.map(c => c.number)).toEqual(['29.1', '29.2', '29.3', '29.4', '29.5']);
    expect(section!.cases.map(c => c.steps.flatMap(s => s.checks).length)).toEqual([3, 2, 3, 1, 1]);
    expect(section!.cases[3].tags).toEqual(['DMX']);
  });

  it('recognizes the upstream 6b2adfd5d channel wizard review cases', () => {
    const section = parseManualReview(md).sections.find(s => s.number === '30');
    expect(section?.title).toContain('6b2adfd5d');
    expect(section?.cases.map(c => c.number)).toEqual(['30.1', '30.2', '30.3']);
    expect(section!.cases.map(c => c.steps.flatMap(s => s.checks).length)).toEqual([3, 2, 1]);
    expect(section!.cases.map(c => c.tags)).toEqual([[], [], []]);
  });

  it('recognizes the upstream 82e541d7a video editor review cases', () => {
    const section = parseManualReview(md).sections.find(s => s.number === '31');
    expect(section?.title).toContain('82e541d7a');
    expect(section?.cases.map(c => c.number)).toEqual(['31.1', '31.2', '31.3']);
    expect(section!.cases.map(c => c.steps.flatMap(s => s.checks).length)).toEqual([2, 3, 1]);
    expect(section!.cases.map(c => c.tags)).toEqual([[], [], []]);
  });

  it('recognizes the upstream 1ccdab841 3D and RGB script review cases', () => {
    const section = parseManualReview(md).sections.find(s => s.number === '32');
    expect(section?.title).toContain('1ccdab841');
    expect(section?.cases.map(c => c.number)).toEqual(['32.1', '32.2', '32.3', '32.4', '32.5']);
    expect(section!.cases.map(c => c.steps.flatMap(s => s.checks).length)).toEqual([3, 2, 3, 2, 1]);
    expect(section!.cases.map(c => c.tags)).toEqual([[], [], [], ['DMX'], []]);
  });

  it('recognizes the upstream 9ddbc62d5 integration review cases', () => {
    const section = parseManualReview(md).sections.find(s => s.number === '33');
    expect(section?.title).toContain('9ddbc62d5');
    expect(section?.cases.map(c => c.number)).toEqual([
      '33.1', '33.2', '33.3', '33.4', '33.5', '33.6', '33.7', '33.8',
    ]);
    expect(section!.cases.map(c => c.steps.flatMap(s => s.checks).length)).toEqual([2, 3, 3, 2, 2, 1, 2, 1]);
    expect(section!.cases.map(c => c.tags)).toEqual([[], [], [], [], [], ['DMX'], [], []]);
  });

  it.each([
    { number: '34', commit: '648072986', cases: ['34.1', '34.2', '34.3'], checks: [2, 2, 2] },
  ])('recognizes upstream $commit packaging review cases', ({ number, commit, cases, checks }) => {
    const section = parseManualReview(md).sections.find(s => s.number === number);
    expect(section?.title).toContain(commit);
    expect(section?.cases.map(c => c.number)).toEqual(cases);
    expect(section!.cases.map(c => c.steps.flatMap(s => s.checks).length)).toEqual(checks);
    expect(section!.cases.map(c => c.tags)).toEqual([[], [], []]);
    expect(section!.cases.flatMap(c => c.steps.flatMap(s => s.checks)).every(c => c.status === 'pending')).toBe(true);
  });

  it.each([
    { number: '35', commit: '928fc14be', cases: ['35.1', '35.2', '35.3', '35.4'],
      checks: [3, 3, 3, 1], tags: [[], ['DMX'], [], []] },
  ])('recognizes upstream $commit audio and XY review cases', ({ number, commit, cases, checks, tags }) => {
    const section = parseManualReview(md).sections.find(s => s.number === number);
    expect(section?.title).toContain(commit);
    expect(section?.cases.map(c => c.number)).toEqual(cases);
    expect(section!.cases.map(c => c.steps.flatMap(s => s.checks).length)).toEqual(checks);
    expect(section!.cases.map(c => c.tags)).toEqual(tags);
    expect(section!.cases.flatMap(c => c.steps.flatMap(s => s.checks)).every(c => c.status === 'pending')).toBe(true);
  });

  it.each([
    { number: '36', cases: ['36.1', '36.2', '36.3', '36.4', '36.5', '36.6'],
      checks: [2, 2, 2, 2, 2, 1], tags: [[], [], ['MIDI'], [], [], []] },
  ])('recognizes section $number VC assignment review cases', ({ number, cases, checks, tags }) => {
    const section = parseManualReview(md).sections.find(s => s.number === number);
    expect(section?.title).toContain('VC assignment');
    expect(section?.cases.map(c => c.number)).toEqual(cases);
    expect(section!.cases.map(c => c.steps.flatMap(s => s.checks).length)).toEqual(checks);
    expect(section!.cases.map(c => c.tags)).toEqual(tags);
    expect(section!.cases.flatMap(c => c.steps.flatMap(s => s.checks)).every(c => c.status === 'pending')).toBe(true);
  });

  it.each([
    { number: '37', commit: 'a977c98b3',
      cases: ['37.1', '37.2', '37.3', '37.4', '37.5', '37.6', '37.7', '37.8'],
      checks: [3, 2, 1, 2, 2, 1, 1, 1], tags: [[], ['DMX'], [], [], ['DMX'], [], [], []] },
  ])('recognizes upstream $commit service and playback review cases', ({ number, commit, cases, checks, tags }) => {
    const section = parseManualReview(md).sections.find(s => s.number === number);
    expect(section?.title).toContain(commit);
    expect(section?.cases.map(c => c.number)).toEqual(cases);
    expect(section!.cases.map(c => c.steps.flatMap(s => s.checks).length)).toEqual(checks);
    expect(section!.cases.map(c => c.tags)).toEqual(tags);
    expect(section!.cases.flatMap(c => c.steps.flatMap(s => s.checks)).every(c => c.status === 'pending')).toBe(true);
  });

  it.each([
    { number: '38', commit: '4c13ab364',
      cases: ['38.1', '38.2', '38.3', '38.4', '38.5', '38.6', '38.7'],
      checks: [2, 2, 2, 1, 2, 2, 1], tags: [[], [], [], [], ['DMX'], [], []] },
  ])('recognizes upstream $commit slider and deployment review cases', ({ number, commit, cases, checks, tags }) => {
    const section = parseManualReview(md).sections.find(s => s.number === number);
    expect(section?.title).toContain(commit);
    expect(section?.cases.map(c => c.number)).toEqual(cases);
    expect(section!.cases.map(c => c.steps.flatMap(s => s.checks).length)).toEqual(checks);
    expect(section!.cases.map(c => c.tags)).toEqual(tags);
    expect(section!.cases.flatMap(c => c.steps.flatMap(s => s.checks)).every(c => c.status === 'pending')).toBe(true);
  });

  it.each([
    { number: '39', commit: '95c323cd0',
      cases: ['39.1', '39.2', '39.3', '39.4', '39.5', '39.6', '39.7'],
      checks: [2, 1, 2, 2, 1, 2, 1], tags: [[], [], [], [], [], [], []] },
  ])('recognizes upstream $commit history and scaling review cases', ({ number, commit, cases, checks, tags }) => {
    const section = parseManualReview(md).sections.find(s => s.number === number);
    expect(section?.title).toContain(commit);
    expect(section?.cases.map(c => c.number)).toEqual(cases);
    expect(section!.cases.map(c => c.steps.flatMap(s => s.checks).length)).toEqual(checks);
    expect(section!.cases.map(c => c.tags)).toEqual(tags);
    expect(section!.cases.flatMap(c => c.steps.flatMap(s => s.checks)).every(c => c.status === 'pending')).toBe(true);
  });

  it.each([
    { number: '40', commit: 'b95e399e5',
      cases: ['40.1', '40.2', '40.3', '40.4', '40.5', '40.6'],
      checks: [2, 2, 2, 1, 2, 1], tags: [[], [], [], [], [], []] },
  ])('recognizes upstream $commit runner and editor review cases', ({ number, commit, cases, checks, tags }) => {
    const section = parseManualReview(md).sections.find(s => s.number === number);
    expect(section?.title).toContain(commit);
    expect(section?.cases.map(c => c.number)).toEqual(cases);
    expect(section!.cases.map(c => c.steps.flatMap(s => s.checks).length)).toEqual(checks);
    expect(section!.cases.map(c => c.tags)).toEqual(tags);
    expect(section!.cases.flatMap(c => c.steps.flatMap(s => s.checks)).every(c => c.status === 'pending')).toBe(true);
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

  it.skipIf(skip)('the review canvas parses section 13b too', () => {
    // the canvas parser is plain ESM outside this package: node runs it on its own
    const canvasParser = pathToFileURL(resolve(__dirname, '../../../.github/extensions/manual-review-canvas/parser.mjs'));
    const script = `import { parseManualReview } from ${JSON.stringify(canvasParser.href)};
      import { readFileSync } from 'fs';
      const section = parseManualReview(readFileSync(${JSON.stringify(mdPath)}, 'utf-8')).sections.find(s => s.number === '13b');
      console.log(section ? section.cases.length : 0);`;
    const cases = Number(execFileSync(process.execPath, ['--input-type=module', '-e', script]).toString().trim());
    expect(cases).toBeGreaterThanOrEqual(4);
  });

  it.skipIf(skip)('gives every VDJ playback case checks of its own', () => {
    const section = parseManualReview(md).sections.find(s => s.number === '12');
    const withoutChecks = section!.cases.filter(c => c.steps.every(s => s.checks.length === 0));
    expect(withoutChecks.map(c => c.number)).toEqual([]);
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
