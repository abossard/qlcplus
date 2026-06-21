import { parseManualReview, allCheckItems, allTags, filterPlan } from './parser';
import { computeStats, generateReport } from './report';
import {
  loadSession, saveSession, resetSession, startSession, concludeSession,
  uploadScreenshot, updateItemStatus, updateItemNote, addScreenshot,
  getRunId,
} from './store';
import { initKeyboard } from './keyboard';
import type { TestPlan, CheckItem, ReviewSession } from './types';

let fullPlan: TestPlan;
let plan: TestPlan;
let session: ReviewSession;
let flatChecks: CheckItem[] = [];
let currentCheckIndex = 0;
let availableTags = new Set<string>();
let knownTags: string[] = [];

// ── Initialization ──

async function init() {
  const md = await loadMarkdown();
  fullPlan = parseManualReview(md);
  knownTags = allTags(fullPlan);
  availableTags = new Set(knownTags); // default: all hardware available
  plan = fullPlan;
  flatChecks = allCheckItems(plan);

  const runId = getRunId();
  if (runId) {
    session = await loadSession();
    showReviewUI();
  } else {
    session = await loadSession();
    showStartScreen();
  }

  document.getElementById('btn-start')!.addEventListener('click', () => doStart());
}

function showStartScreen() {
  document.getElementById('start-screen')!.style.display = 'flex';
  document.getElementById('review-ui')!.style.display = 'none';
}

async function doStart() {
  const result = await startSession();
  session = result.session;
  showReviewUI();
}

function showReviewUI() {
  document.getElementById('start-screen')!.style.display = 'none';
  document.getElementById('review-ui')!.style.display = '';

  renderTagFilters();
  renderSidebar();
  renderContent();
  updateProgress();
  highlightCurrentCheck();

  initKeyboard({
    'j': () => navigateCheck(1),
    'k': () => navigateCheck(-1),
    'ArrowDown': () => navigateCheck(1),
    'ArrowUp': () => navigateCheck(-1),
    'p': () => setCurrentStatus('pass'),
    'f': () => setCurrentStatus('fail'),
    's': () => setCurrentStatus('skip'),
    'n': () => focusCurrentNote(),
    'Ctrl+e': () => exportReport(),
    'Ctrl+r': () => { if (confirm('Reset all progress?')) doReset(); },
    '?': () => toggleHelp(),
  });

  // Clipboard paste → attach screenshot to the currently focused check item
  document.addEventListener('paste', handlePaste);

  // Toolbar buttons
  document.getElementById('btn-export')!.addEventListener('click', () => exportReport());
  document.getElementById('btn-reset')!.addEventListener('click', () => {
    if (confirm('Reset all progress?')) doReset();
  });
  document.getElementById('btn-conclude')!.addEventListener('click', () => {
    if (confirm('Conclude this review? The report will be finalized.')) doConclude();
  });
  document.getElementById('btn-help')!.addEventListener('click', () => toggleHelp());

  // Help overlay — click to close
  document.getElementById('help-overlay')!.addEventListener('click', () => {
    helpVisible = true; // will toggle to false
    toggleHelp();
  });
}

async function loadMarkdown(): Promise<string> {
  // Try fetching from the repo root
  try {
    const resp = await fetch('/MANUAL_REVIEW.md');
    if (resp.ok) return resp.text();
  } catch { /* fallback below */ }

  // Fallback: try relative paths
  for (const path of ['../../MANUAL_REVIEW.md', '../MANUAL_REVIEW.md', './MANUAL_REVIEW.md']) {
    try {
      const resp = await fetch(path);
      if (resp.ok) return resp.text();
    } catch { /* continue */ }
  }

  return '# No MANUAL_REVIEW.md found\n\nPlace MANUAL_REVIEW.md in the repo root or serve it alongside this app.';
}

// ── Tag Filtering ──

function renderTagFilters() {
  const container = document.getElementById('tag-filters')!;
  if (knownTags.length === 0) {
    container.style.display = 'none';
    return;
  }
  container.innerHTML = '<label>Equipment:</label>';
  for (const tag of knownTags) {
    const checked = availableTags.has(tag) ? 'checked' : '';
    const activeClass = availableTags.has(tag) ? ' active' : '';
    container.innerHTML += `<label class="tag-chip${activeClass}" data-tag="${tag}">
      <input type="checkbox" ${checked} data-tag="${tag}" />${tag}</label>`;
  }
  container.querySelectorAll('input[type="checkbox"]').forEach(cb => {
    cb.addEventListener('change', (e) => {
      const input = e.target as HTMLInputElement;
      const tag = input.dataset.tag!;
      if (input.checked) availableTags.add(tag);
      else availableTags.delete(tag);
      input.parentElement!.classList.toggle('active', input.checked);
      applyTagFilter();
    });
  });
}

function applyTagFilter() {
  plan = filterPlan(fullPlan, availableTags);
  flatChecks = allCheckItems(plan);
  currentCheckIndex = Math.min(currentCheckIndex, Math.max(0, flatChecks.length - 1));
  renderSidebar();
  renderContent();
  updateProgress();
  highlightCurrentCheck();
}

// ── Rendering ──

function renderSidebar() {
  const sidebar = document.getElementById('sidebar')!;
  sidebar.innerHTML = '';

  // Tester name
  const testerDiv = document.createElement('div');
  testerDiv.className = 'tester-input';
  testerDiv.innerHTML = `
    <label>Tester</label>
    <input type="text" id="tester-name" value="${escapeHtml(session.tester)}" placeholder="Your name">
  `;
  sidebar.appendChild(testerDiv);

  const testerInput = testerDiv.querySelector('input')!;
  testerInput.addEventListener('change', () => {
    session.tester = testerInput.value;
    saveSession(session);
  });

  // Sections
  for (const section of plan.sections) {
    const sectionEl = document.createElement('div');
    sectionEl.className = 'sidebar-section';

    const sectionHeader = document.createElement('div');
    sectionHeader.className = 'sidebar-section-header';
    sectionHeader.textContent = `${section.number}. ${section.title}`;
    sectionHeader.addEventListener('click', () => {
      scrollToElement(section.id);
    });
    sectionEl.appendChild(sectionHeader);

    for (const tc of section.cases) {
      const caseEl = document.createElement('div');
      caseEl.className = 'sidebar-case';

      const checks = [...tc.steps.flatMap(s => s.checks), ...tc.tableChecks];
      const statuses = checks.map(c => session.items[c.id]?.status ?? 'pending');
      const icon = statuses.length === 0 ? '📋'
        : statuses.every(s => s === 'pass') ? '✅'
        : statuses.some(s => s === 'fail') ? '❌'
        : statuses.some(s => s !== 'pending') ? '🔶'
        : '⬜';

      caseEl.textContent = `${icon} ${tc.number} ${tc.title}`;
      if (tc.tags.length > 0) {
        for (const tag of tc.tags) {
          const badge = document.createElement('span');
          badge.className = `tag-badge tag-badge-${tag}`;
          badge.textContent = tag;
          caseEl.appendChild(badge);
        }
      }
      caseEl.addEventListener('click', () => {
        scrollToElement(tc.id);
        const firstCheck = checks[0];
        if (firstCheck) {
          currentCheckIndex = flatChecks.findIndex(c => c.id === firstCheck.id);
          highlightCurrentCheck();
        }
      });
      sectionEl.appendChild(caseEl);
    }

    sidebar.appendChild(sectionEl);
  }
}

function renderContent() {
  const content = document.getElementById('content')!;
  content.innerHTML = '';

  for (const section of plan.sections) {
    const sectionEl = document.createElement('div');
    sectionEl.className = 'test-section';
    sectionEl.id = section.id;

    const header = document.createElement('h2');
    header.textContent = `${section.number}. ${section.title}`;
    sectionEl.appendChild(header);

    if (section.contextNote) {
      const note = document.createElement('blockquote');
      note.textContent = section.contextNote;
      sectionEl.appendChild(note);
    }

    for (const tc of section.cases) {
      const caseEl = document.createElement('div');
      caseEl.className = 'test-case';
      caseEl.id = tc.id;

      const caseHeader = document.createElement('h3');
      caseHeader.textContent = `${tc.number} ${tc.title}`;
      if (tc.tags.length > 0) {
        for (const tag of tc.tags) {
          const badge = document.createElement('span');
          badge.className = `tag-badge tag-badge-${tag}`;
          badge.textContent = tag;
          caseHeader.appendChild(badge);
        }
      }
      caseEl.appendChild(caseHeader);

      // Steps
      for (const step of tc.steps) {
        const stepEl = document.createElement('div');
        stepEl.className = `test-step step-${step.kind}`;

        const stepLabel = document.createElement('span');
        stepLabel.className = 'step-label';
        stepLabel.textContent = stepKindLabel(step.kind);
        stepEl.appendChild(stepLabel);

        if (step.text) {
          const stepText = document.createElement('span');
          stepText.className = 'step-text';
          stepText.innerHTML = escapeHtml(step.text);
          stepEl.appendChild(stepText);
        }

        caseEl.appendChild(stepEl);

        // Check items under this step
        for (const check of step.checks) {
          caseEl.appendChild(renderCheckItem(check));
        }
      }

      // Table checks
      if (tc.tableChecks.length > 0) {
        for (const check of tc.tableChecks) {
          caseEl.appendChild(renderCheckItem(check));
        }
      }

      sectionEl.appendChild(caseEl);
    }

    content.appendChild(sectionEl);
  }
}

function renderCheckItem(check: CheckItem): HTMLElement {
  const saved = session.items[check.id];
  const status = saved?.status ?? 'pending';
  const note = saved?.note ?? '';
  const screenshots = saved?.screenshots ?? [];

  const el = document.createElement('div');
  el.className = `check-item status-${status}`;
  el.id = `check-el-${check.id}`;
  el.dataset.checkId = check.id;
  el.tabIndex = 0; // Make focusable for paste events

  el.innerHTML = `
    <div class="check-controls">
      <button class="btn-pass ${status === 'pass' ? 'active' : ''}" title="Pass (p)" data-action="pass">✅</button>
      <button class="btn-fail ${status === 'fail' ? 'active' : ''}" title="Fail (f)" data-action="fail">❌</button>
      <button class="btn-skip ${status === 'skip' ? 'active' : ''}" title="Skip (s)" data-action="skip">⏭</button>
    </div>
    <div class="check-text">${escapeHtml(check.text)}</div>
    <div class="check-meta">
      <input type="text" class="check-note" placeholder="Add note…" value="${escapeHtml(note)}">
      <label class="screenshot-btn" title="Upload screenshot">
        📸
        <input type="file" accept="image/*" class="screenshot-input" multiple>
      </label>
      <span class="paste-hint" title="Click here then Ctrl+V to paste a screenshot">📋 Paste</span>
    </div>
    <div class="check-screenshots">
      ${screenshots.map(s => `<span class="screenshot-tag">📎 ${escapeHtml(s)}</span>`).join('')}
    </div>
  `;

  // Status buttons
  el.querySelectorAll<HTMLButtonElement>('[data-action]').forEach(btn => {
    btn.addEventListener('click', () => {
      const action = btn.dataset.action as CheckItem['status'];
      updateItemStatus(session, check.id, action);
      saveSession(session);
      rerenderCheck(check.id);
      updateProgress();
      renderSidebar();
    });
  });

  // Note input — save on change AND on input (so paste/rerender doesn't lose text)
  const noteInput = el.querySelector<HTMLInputElement>('.check-note')!;
  noteInput.addEventListener('change', () => {
    updateItemNote(session, check.id, noteInput.value);
    saveSession(session);
  });
  noteInput.addEventListener('input', () => {
    updateItemNote(session, check.id, noteInput.value);
  });

  // Screenshot upload via file input
  const fileInput = el.querySelector<HTMLInputElement>('.screenshot-input')!;
  fileInput.addEventListener('change', async () => {
    if (!fileInput.files) return;
    for (const file of Array.from(fileInput.files)) {
      const filenames = await uploadScreenshot(check.id, file);
      for (const fn of filenames) {
        addScreenshot(session, check.id, fn);
      }
    }
    // Capture note before re-render destroys the input
    const currentNote = el.querySelector<HTMLInputElement>('.check-note')?.value ?? '';
    if (currentNote) {
      updateItemNote(session, check.id, currentNote);
    }
    await saveSession(session);
    rerenderCheck(check.id);
  });

  // Paste hint — click to focus this item so Ctrl+V works
  const pasteHint = el.querySelector<HTMLElement>('.paste-hint')!;
  pasteHint.addEventListener('click', (e) => {
    e.stopPropagation();
    currentCheckIndex = flatChecks.findIndex(c => c.id === check.id);
    highlightCurrentCheck();
    el.focus();
  });

  // Per-element paste handler
  el.addEventListener('paste', async (e: ClipboardEvent) => {
    if (!e.clipboardData?.items) return;
    let attached = false;

    for (const item of Array.from(e.clipboardData.items)) {
      if (!item.type.startsWith('image/')) continue;
      const file = item.getAsFile();
      if (!file) continue;

      const ext = item.type.split('/')[1] || 'png';
      const pasteFile = new File([file], `clipboard-${Date.now()}.${ext}`, { type: file.type });
      const filenames = await uploadScreenshot(check.id, pasteFile);
      for (const fn of filenames) {
        addScreenshot(session, check.id, fn);
      }
      attached = true;
    }

    if (attached) {
      e.preventDefault();
      // Capture the current note value before re-rendering destroys the input
      const currentNote = el.querySelector<HTMLInputElement>('.check-note')?.value ?? '';
      if (currentNote) {
        updateItemNote(session, check.id, currentNote);
      }
      await saveSession(session);
      rerenderCheck(check.id);
    }
  });

  // Click to select
  el.addEventListener('click', (e) => {
    if ((e.target as HTMLElement).tagName === 'BUTTON' || (e.target as HTMLElement).tagName === 'INPUT') return;
    if ((e.target as HTMLElement).classList.contains('paste-hint')) return;
    currentCheckIndex = flatChecks.findIndex(c => c.id === check.id);
    highlightCurrentCheck();
  });

  return el;
}

function rerenderCheck(checkId: string) {
  const el = document.getElementById(`check-el-${checkId}`);
  if (!el) return;
  const wasFocused = el.classList.contains('focused');
  const check = flatChecks.find(c => c.id === checkId);
  if (!check) return;
  const parent = el.parentElement!;
  const newEl = renderCheckItem(check);
  if (wasFocused) newEl.classList.add('focused');
  parent.replaceChild(newEl, el);
}

// ── Navigation ──

function navigateCheck(delta: number) {
  const newIndex = currentCheckIndex + delta;
  if (newIndex >= 0 && newIndex < flatChecks.length) {
    currentCheckIndex = newIndex;
    highlightCurrentCheck();
    scrollToElement(`check-el-${flatChecks[currentCheckIndex].id}`);
  }
}

function highlightCurrentCheck() {
  document.querySelectorAll('.check-item').forEach(el => el.classList.remove('focused'));
  if (flatChecks.length > 0) {
    const el = document.getElementById(`check-el-${flatChecks[currentCheckIndex].id}`);
    el?.classList.add('focused');
  }
}

function setCurrentStatus(status: CheckItem['status']) {
  if (flatChecks.length === 0) return;
  const check = flatChecks[currentCheckIndex];
  updateItemStatus(session, check.id, status);
  saveSession(session);
  rerenderCheck(check.id);
  updateProgress();
  renderSidebar();
  // Auto-advance to next
  navigateCheck(1);
}

function focusCurrentNote() {
  if (flatChecks.length === 0) return;
  const el = document.getElementById(`check-el-${flatChecks[currentCheckIndex].id}`);
  el?.querySelector<HTMLInputElement>('.check-note')?.focus();
}

function scrollToElement(id: string) {
  document.getElementById(id)?.scrollIntoView({ behavior: 'smooth', block: 'center' });
}

// ── Progress ──

function updateProgress() {
  const stats = computeStats(plan, session);
  const pct = stats.total > 0 ? Math.round(((stats.total - stats.pending) / stats.total) * 100) : 0;

  const bar = document.getElementById('progress-bar')!;
  bar.style.width = `${pct}%`;
  bar.className = `progress-fill ${stats.fail > 0 ? 'has-failures' : ''}`;

  document.getElementById('progress-text')!.textContent =
    `${stats.pass}✅ ${stats.fail}❌ ${stats.skip}⏭ ${stats.pending}⬜ — ${pct}% complete (${stats.total} items)`;
}

// ── Export ──

async function exportReport() {
  const report = generateReport(plan, session);

  // Build a ZIP-like download: report + screenshots
  // Use the showDirectoryPicker API if available, otherwise download files individually
  if ('showDirectoryPicker' in window) {
    try {
      const dirHandle = await (window as any).showDirectoryPicker({ mode: 'readwrite' });

      // Write report
      const reportFile = await dirHandle.getFileHandle('test-report.md', { create: true });
      const reportWriter = await reportFile.createWritable();
      await reportWriter.write(report);
      await reportWriter.close();

      // Write session data (for AI consumption)
      const sessionFile = await dirHandle.getFileHandle('test-session.json', { create: true });
      const sessionWriter = await sessionFile.createWritable();
      await sessionWriter.write(JSON.stringify(session, null, 2));
      await sessionWriter.close();

      alert(`Report exported!`);
      return;
    } catch (e) {
      // User cancelled or API not supported — fall through to download
      if ((e as Error).name === 'AbortError') return;
    }
  }

  // Fallback: download report as a file
  downloadFile('test-report.md', report, 'text/markdown');
  downloadFile('test-session.json', JSON.stringify(session, null, 2), 'application/json');
}

function downloadFile(name: string, content: string, mime: string) {
  const blob = new Blob([content], { type: mime });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = name;
  a.click();
  URL.revokeObjectURL(url);
}

async function doReset() {
  session = await resetSession();
  renderContent();
  renderSidebar();
  updateProgress();
  currentCheckIndex = 0;
  highlightCurrentCheck();
  showStartScreen();
}

async function doConclude() {
  await concludeSession();
  session = await resetSession();
  renderContent();
  renderSidebar();
  updateProgress();
  currentCheckIndex = 0;
  highlightCurrentCheck();
  showStartScreen();
}

// ── Clipboard Paste ──

function handlePaste(e: ClipboardEvent) {
  // Don't intercept paste into text inputs
  const tag = (e.target as HTMLElement)?.tagName;
  if (tag === 'INPUT' || tag === 'TEXTAREA') return;

  if (!e.clipboardData?.items) return;
  if (flatChecks.length === 0) return;

  const check = flatChecks[currentCheckIndex];
  let attached = false;

  for (const item of Array.from(e.clipboardData.items)) {
    if (!item.type.startsWith('image/')) continue;
    const file = item.getAsFile();
    if (!file) continue;

    const ext = item.type.split('/')[1] || 'png';
    const pasteFile = new File([file], `clipboard-${Date.now()}.${ext}`, { type: file.type });
    uploadScreenshot(check.id, pasteFile).then(filenames => {
      for (const fn of filenames) {
        addScreenshot(session, check.id, fn);
      }
      saveSession(session);
      rerenderCheck(check.id);
      highlightCurrentCheck();
    });
    attached = true;
  }

  if (attached) {
    e.preventDefault();
  }
}

// ── Help ──

let helpVisible = false;

function toggleHelp() {
  helpVisible = !helpVisible;
  const el = document.getElementById('help-overlay')!;
  el.style.display = helpVisible ? 'flex' : 'none';
}

// ── Utilities ──

function escapeHtml(s: string): string {
  return s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
}

function stepKindLabel(kind: string): string {
  const labels: Record<string, string> = {
    'do': '🔧 Do:',
    'verify': '👁 Verify:',
    'prerequisite': '⚡ Prerequisite:',
    'expected': '🎯 Expected:',
    'why-manual': '❓ Why manual:',
    'note': '📝 Note:',
    'text': '',
  };
  return labels[kind] ?? '';
}

// ── Bootstrap ──

document.addEventListener('DOMContentLoaded', init);
