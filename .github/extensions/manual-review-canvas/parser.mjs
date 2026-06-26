// Pure parsing + reporting for MANUAL_REVIEW.md.
// Ported from tools/manual-review/src/{parser,report}.ts so the canvas produces
// identical check-item IDs and report output, keeping test-reports/ compatible.

/**
 * Parse a MANUAL_REVIEW.md file into a structured plan.
 * Recognises:
 *   ## N. Title            -> sections
 *   ### N.M Title          -> test cases
 *   > blockquote           -> context notes (section-level)
 *   - **Do:** / **Verify:** -> steps
 *   - ☐ text               -> check items
 *   | col | ... | ☐ ... |  -> table check items
 */
export function parseManualReview(markdown) {
    const lines = markdown.split("\n");
    const plan = { title: "", preamble: "", sections: [] };
    const preambleLines = [];
    let inPreamble = true;

    let currentSection = null;
    let currentCase = null;
    let currentStep = null;
    let caseCheckCounter = 0;
    let inCodeBlock = false;

    const flushCase = () => {
        if (currentCase && currentSection) currentSection.cases.push(currentCase);
    };

    for (let i = 0; i < lines.length; i++) {
        const line = lines[i];

        if (line.trimStart().startsWith("```")) {
            inCodeBlock = !inCodeBlock;
            continue;
        }
        if (inCodeBlock) continue;

        const h1 = line.match(/^# (.+)/);
        if (h1) {
            plan.title = h1[1].trim();
            inPreamble = true;
            continue;
        }

        const h2 = line.match(/^## (\d+)\.\s+(.+)/);
        if (h2) {
            inPreamble = false;
            flushCase();
            currentCase = null;
            currentStep = null;
            currentSection = makeSection(h2[1], h2[2]);
            plan.sections.push(currentSection);
            continue;
        }

        const h3 = line.match(/^### (\d+\.\d+)\s+(.+)/);
        if (h3 && currentSection) {
            flushCase();
            currentStep = null;
            currentCase = makeCase(currentSection.number, h3[1], h3[2]);
            caseCheckCounter = 0;
            continue;
        }

        if (inPreamble && !currentSection) {
            if (line.trim() && !line.startsWith("#")) preambleLines.push(line);
            continue;
        }

        if (!currentSection) continue;

        const bq = line.match(/^>\s*(.*)/);
        if (bq && !currentCase) {
            currentSection.contextNote +=
                (currentSection.contextNote ? " " : "") + bq[1].trim();
            continue;
        }

        // Table row containing ☐ -> table check item
        if (line.includes("|") && line.includes("☐") && currentCase) {
            const cells = line.split("|").map((c) => c.trim()).filter((c) => c.length > 0);
            const checkCellIndex = cells.findIndex((c) => c.includes("☐"));
            const textCells = cells.filter((_, idx) => idx !== checkCellIndex);
            const checkText = textCells.join(" — ");
            const checkCellText = cells[checkCellIndex]?.replace("☐", "").trim();
            const fullText = checkCellText ? `${checkText} — ${checkCellText}` : checkText;
            currentCase.tableChecks.push(makeCheckItem(currentCase.id, ++caseCheckCounter, fullText));
            continue;
        }

        // Skip table header / separator rows
        if (line.match(/^\|[-\s|:]+\|$/)) continue;
        if (line.match(/^\|.*\|$/) && !line.includes("☐")) continue;

        // - ☐ text -> check item attached to current step
        const checkbox = line.match(/^[-*]\s*☐\s+(.*)/);
        if (checkbox && currentCase) {
            const check = makeCheckItem(currentCase.id, ++caseCheckCounter, checkbox[1].trim());
            if (currentStep) {
                currentStep.checks.push(check);
            } else {
                currentStep = { kind: "verify", text: "", checks: [check] };
                currentCase.steps.push(currentStep);
            }
            continue;
        }

        // - **Do:** / **Verify:** etc -> structured step
        const stepMatch = line.match(/^[-*]\s+\*\*(\w[\w\s]*?)(?:\s+\w+)?:\*\*\s*(.*)/);
        if (stepMatch && currentCase) {
            const kind = classifyStepKind(stepMatch[1]);
            currentStep = { kind, text: stepMatch[2].trim(), checks: [] };
            currentCase.steps.push(currentStep);
            continue;
        }
    }

    flushCase();
    plan.preamble = preambleLines.join("\n");

    // Cases with prose but no ☐ get an auto "verified" check so they're markable.
    for (const section of plan.sections) {
        for (const tc of section.cases) {
            const hasChecks =
                tc.tableChecks.length > 0 || tc.steps.some((s) => s.checks.length > 0);
            const hasSteps = tc.steps.length > 0;
            if (!hasChecks && hasSteps) {
                tc.tableChecks.push(makeCheckItem(tc.id, 1, `${tc.number} ${tc.title} — verified`));
            }
        }
    }

    return plan;
}

function makeSection(number, title) {
    return { id: `section-${number}`, number, title: title.trim(), contextNote: "", cases: [] };
}

function makeCase(sectionNumber, number, title) {
    return {
        id: `s${sectionNumber}-case-${number.replace(".", "-")}`,
        number,
        title: title.trim(),
        steps: [],
        tableChecks: [],
    };
}

function makeCheckItem(caseId, caseCounter, text) {
    return { id: `${caseId}-chk-${caseCounter}`, text, status: "pending", note: "", screenshots: [] };
}

function classifyStepKind(label) {
    const lower = label.toLowerCase().trim();
    if (lower.startsWith("do")) return "do";
    if (lower.startsWith("verify")) return "verify";
    if (lower.startsWith("prerequisite") || lower.startsWith("precondition")) return "prerequisite";
    if (lower.startsWith("expected")) return "expected";
    if (lower.startsWith("why")) return "why-manual";
    if (lower.startsWith("note")) return "note";
    return "text";
}

/** Flat list of every check item in document order. */
export function allCheckItems(plan) {
    const items = [];
    for (const section of plan.sections) {
        for (const tc of section.cases) {
            for (const step of tc.steps) items.push(...step.checks);
            items.push(...tc.tableChecks);
        }
    }
    return items;
}

/** Flat list enriched with section/case context (for agent list_items). */
export function flatChecksWithContext(plan) {
    const items = [];
    for (const section of plan.sections) {
        for (const tc of section.cases) {
            const checks = [...tc.steps.flatMap((s) => s.checks), ...tc.tableChecks];
            for (const check of checks) {
                items.push({
                    id: check.id,
                    text: check.text,
                    section: `${section.number}. ${section.title}`,
                    case: `${tc.number} ${tc.title}`,
                });
            }
        }
    }
    return items;
}

/** Compute pass/fail/skip/pending stats from a plan + session. */
export function computeStats(plan, session) {
    let total = 0, pass = 0, fail = 0, skip = 0, pending = 0;
    for (const section of plan.sections) {
        for (const tc of section.cases) {
            const checks = [...tc.steps.flatMap((s) => s.checks), ...tc.tableChecks];
            for (const check of checks) {
                total++;
                const status = session.items?.[check.id]?.status ?? "pending";
                if (status === "pass") pass++;
                else if (status === "fail") fail++;
                else if (status === "skip") skip++;
                else pending++;
            }
        }
    }
    return { total, pass, fail, skip, pending };
}

export function verdict(stats) {
    if (stats.fail > 0) return "❌ BLOCKERS FOUND";
    if (stats.pending > 0) return "⚠️ INCOMPLETE";
    return "✅ ALL PASSED";
}

const STATUS_ICON = { pass: "✅", fail: "❌", skip: "⏭", pending: "⬜" };

/** Generate a markdown test report (mirrors tools/manual-review report.ts). */
export function generateReport(plan, session) {
    const stats = computeStats(plan, session);
    const lines = [];
    lines.push(`# Test Report: ${plan.title}`);
    lines.push("");
    lines.push(`| Field | Value |`);
    lines.push(`|-------|-------|`);
    lines.push(`| Source | \`${session.sourceFile ?? "MANUAL_REVIEW.md"}\` |`);
    lines.push(`| Tester | ${session.tester || "(not set)"} |`);
    lines.push(`| Started | ${session.startedAt} |`);
    lines.push(`| Updated | ${session.updatedAt} |`);
    if (session.concludedAt) lines.push(`| Concluded | ${session.concludedAt} |`);
    lines.push("");
    lines.push("## Summary");
    lines.push("");
    lines.push(`| Status | Count |`);
    lines.push(`|--------|-------|`);
    lines.push(`| ✅ Pass | ${stats.pass} |`);
    lines.push(`| ❌ Fail | ${stats.fail} |`);
    lines.push(`| ⏭ Skip | ${stats.skip} |`);
    lines.push(`| ⬜ Pending | ${stats.pending} |`);
    lines.push(`| **Total** | **${stats.total}** |`);
    lines.push("");
    lines.push(`**Result: ${verdict(stats)}**`);
    lines.push("");
    lines.push("---");
    lines.push("");

    for (const section of plan.sections) {
        lines.push(`## ${section.number}. ${section.title}`);
        lines.push("");
        for (const tc of section.cases) {
            lines.push(`### ${tc.number} ${tc.title}`);
            lines.push("");
            const allChecks = [...tc.steps.flatMap((s) => s.checks), ...tc.tableChecks];
            if (allChecks.length === 0) {
                lines.push("_No checkable items._");
                lines.push("");
                continue;
            }
            for (const check of allChecks) {
                const saved = session.items?.[check.id];
                const status = saved?.status ?? "pending";
                const icon = STATUS_ICON[status] ?? "⬜";
                lines.push(`- ${icon} ${check.text}`);
                if (saved?.note) lines.push(`  - 💬 ${saved.note}`);
                if (saved?.screenshots?.length) {
                    for (const ss of saved.screenshots) lines.push(`  - 📸 ![screenshot](${ss})`);
                }
            }
            lines.push("");
        }
    }
    return lines.join("\n");
}
