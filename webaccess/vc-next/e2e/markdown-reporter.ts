// Custom Playwright reporter that generates a markdown document with
// embedded screenshots for every test.  Each test's attachments (images)
// are linked with relative paths so the report is viewable on GitHub
// or any markdown renderer.

import type {
  Reporter,
  FullConfig,
  Suite,
  TestCase,
  TestResult,
  FullResult,
} from '@playwright/test/reporter';
import fs from 'node:fs';
import path from 'node:path';

interface Options {
  /** Output markdown file path (relative to project root). */
  outputFile?: string;
}

interface TestEntry {
  title: string;
  suiteName: string;
  file: string;
  line: number;
  status: string;
  duration: number;
  error?: string;
  attachments: { name: string; relPath: string; contentType: string }[];
  retry: number;
}

const STATUS_ICON: Record<string, string> = {
  passed: '✅',
  failed: '❌',
  timedOut: '⏱️',
  skipped: '⏭️',
  interrupted: '🛑',
};

export default class MarkdownReporter implements Reporter {
  private outputFile: string;
  private entries: TestEntry[] = [];
  private startTime = 0;

  constructor(options: Options = {}) {
    this.outputFile = options.outputFile ?? 'e2e/screenshots/TEST_REPORT.md';
  }

  onBegin(_config: FullConfig, _suite: Suite) {
    this.startTime = Date.now();
  }

  onTestEnd(test: TestCase, result: TestResult) {
    // Only record the final attempt (highest retry).
    const existing = this.entries.find(
      (e) => e.file === test.location.file && e.line === test.location.line,
    );
    if (existing && existing.retry >= result.retry) return;
    if (existing) {
      // Replace with later retry.
      const idx = this.entries.indexOf(existing);
      this.entries.splice(idx, 1);
    }

    const outputDir = path.dirname(path.resolve(this.outputFile));
    const attachments: TestEntry['attachments'] = [];

    for (const att of result.attachments) {
      if (att.contentType?.startsWith('image/') && att.path) {
        // Prefer the screenshot in the same directory as the report (cleaner relative paths).
        const localCandidate = path.join(outputDir, `${att.name}.png`);
        const src = fs.existsSync(localCandidate) ? localCandidate : att.path;
        const relPath = path.relative(outputDir, src);
        attachments.push({ name: att.name, relPath, contentType: att.contentType });
      }
    }

    const suiteTitles: string[] = [];
    let parent = test.parent;
    while (parent) {
      if (parent.title) suiteTitles.unshift(parent.title);
      parent = parent.parent;
    }

    this.entries.push({
      title: test.title,
      suiteName: suiteTitles.join(' › '),
      file: path.relative(process.cwd(), test.location.file),
      line: test.location.line,
      status: result.status,
      duration: result.duration,
      error: result.error?.message,
      attachments,
      retry: result.retry,
    });
  }

  onEnd(result: FullResult) {
    const elapsed = ((Date.now() - this.startTime) / 1000).toFixed(1);
    const passed = this.entries.filter((e) => e.status === 'passed').length;
    const failed = this.entries.filter((e) => e.status === 'failed').length;
    const skipped = this.entries.filter((e) => e.status === 'skipped').length;
    const total = this.entries.length;

    const lines: string[] = [];

    // Header
    lines.push('# 🎛️ DMX Web Control — Test Report');
    lines.push('');
    lines.push(`> Generated: **${new Date().toISOString().slice(0, 19).replace('T', ' ')}**  `);
    lines.push(`> Duration: **${elapsed}s** | Status: **${result.status}**  `);
    lines.push(`> Tests: **${passed}** passed, **${failed}** failed, **${skipped}** skipped — **${total}** total`);
    lines.push('');

    // Summary table
    lines.push('## Summary');
    lines.push('');
    lines.push('| Status | Test | Duration |');
    lines.push('|--------|------|----------|');
    for (const e of this.entries) {
      const icon = STATUS_ICON[e.status] ?? '❓';
      const imgs = e.attachments.length > 0 ? ` 📸×${e.attachments.length}` : '';
      lines.push(`| ${icon} | ${e.title}${imgs} | ${e.duration}ms |`);
    }
    lines.push('');

    // Group by suite
    const suites = new Map<string, TestEntry[]>();
    for (const e of this.entries) {
      const key = e.suiteName || 'Ungrouped';
      if (!suites.has(key)) suites.set(key, []);
      suites.get(key)!.push(e);
    }

    lines.push('---');
    lines.push('');

    for (const [suiteName, tests] of suites) {
      lines.push(`## ${suiteName}`);
      lines.push('');

      for (const t of tests) {
        const icon = STATUS_ICON[t.status] ?? '❓';
        lines.push(`### ${icon} ${t.title}`);
        lines.push('');
        lines.push(`- **File:** \`${t.file}:${t.line}\``);
        lines.push(`- **Duration:** ${t.duration}ms`);
        if (t.retry > 0) lines.push(`- **Retries:** ${t.retry}`);
        lines.push('');

        if (t.error) {
          lines.push('<details><summary>Error</summary>');
          lines.push('');
          lines.push('```');
          lines.push(t.error.split('\n').slice(0, 10).join('\n'));
          lines.push('```');
          lines.push('</details>');
          lines.push('');
        }

        if (t.attachments.length > 0) {
          for (const att of t.attachments) {
            lines.push(`**${att.name}**`);
            lines.push('');
            lines.push(`![${att.name}](${att.relPath})`);
            lines.push('');
          }
        }

        lines.push('---');
        lines.push('');
      }
    }

    // Write
    const dir = path.dirname(this.outputFile);
    if (dir) fs.mkdirSync(dir, { recursive: true });
    fs.writeFileSync(this.outputFile, lines.join('\n'));
  }
}
