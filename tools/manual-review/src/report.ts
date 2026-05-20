import type { TestPlan, ReviewSession, ReviewStats } from './types';

/** Compute stats from a plan + session. */
export function computeStats(plan: TestPlan, session: ReviewSession): ReviewStats {
  let total = 0, pass = 0, fail = 0, skip = 0, pending = 0;

  for (const section of plan.sections) {
    for (const tc of section.cases) {
      const checks = [
        ...tc.steps.flatMap(s => s.checks),
        ...tc.tableChecks,
      ];
      for (const check of checks) {
        total++;
        const saved = session.items[check.id];
        const status = saved?.status ?? 'pending';
        if (status === 'pass') pass++;
        else if (status === 'fail') fail++;
        else if (status === 'skip') skip++;
        else pending++;
      }
    }
  }

  return { total, pass, fail, skip, pending };
}

const STATUS_ICON: Record<string, string> = {
  pass: '✅',
  fail: '❌',
  skip: '⏭',
  pending: '⬜',
};

/** Generate a markdown test report. */
export function generateReport(plan: TestPlan, session: ReviewSession): string {
  const stats = computeStats(plan, session);
  const lines: string[] = [];

  lines.push(`# Test Report: ${plan.title}`);
  lines.push('');
  lines.push(`| Field | Value |`);
  lines.push(`|-------|-------|`);
  lines.push(`| Source | \`${session.sourceFile}\` |`);
  lines.push(`| Tester | ${session.tester || '(not set)'} |`);
  lines.push(`| Started | ${session.startedAt} |`);
  lines.push(`| Completed | ${session.updatedAt} |`);
  lines.push('');
  lines.push('## Summary');
  lines.push('');
  lines.push(`| Status | Count |`);
  lines.push(`|--------|-------|`);
  lines.push(`| ✅ Pass | ${stats.pass} pass |`);
  lines.push(`| ❌ Fail | ${stats.fail} fail |`);
  lines.push(`| ⏭ Skip | ${stats.skip} skip |`);
  lines.push(`| ⬜ Pending | ${stats.pending} pending |`);
  lines.push(`| **Total** | **${stats.total}** |`);
  lines.push('');
  lines.push(`**Result: ${stats.fail > 0 ? '❌ BLOCKERS FOUND' : stats.pending > 0 ? '⚠️ INCOMPLETE' : '✅ ALL PASSED'}**`);
  lines.push('');
  lines.push('---');
  lines.push('');

  for (const section of plan.sections) {
    lines.push(`## ${section.number}. ${section.title}`);
    lines.push('');

    for (const tc of section.cases) {
      lines.push(`### ${tc.number} ${tc.title}`);
      lines.push('');

      const allChecks = [
        ...tc.steps.flatMap(s => s.checks),
        ...tc.tableChecks,
      ];

      if (allChecks.length === 0) {
        lines.push('_No checkable items._');
        lines.push('');
        continue;
      }

      for (const check of allChecks) {
        const saved = session.items[check.id];
        const status = saved?.status ?? 'pending';
        const icon = STATUS_ICON[status] ?? '⬜';
        lines.push(`- ${icon} ${check.text}`);
        if (saved?.note) {
          lines.push(`  - 💬 ${saved.note}`);
        }
        if (saved?.screenshots?.length) {
          for (const ss of saved.screenshots) {
            lines.push(`  - 📸 ![screenshot](${ss})`);
          }
        }
      }
      lines.push('');
    }
  }

  return lines.join('\n');
}
