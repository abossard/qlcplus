import type { ReviewSession, CheckItem } from './types';

const RUN_ID_KEY = 'manual-review-run-id';

// ── Data / Calculations (pure) ──

export function createSession(): ReviewSession {
  return {
    sourceFile: 'MANUAL_REVIEW.md',
    startedAt: new Date().toISOString(),
    updatedAt: new Date().toISOString(),
    tester: '',
    items: {},
  };
}

export function updateItemStatus(
  session: ReviewSession,
  checkId: string,
  status: CheckItem['status'],
): void {
  if (!session.items[checkId]) {
    session.items[checkId] = { status: 'pending', note: '', screenshots: [] };
  }
  session.items[checkId].status = status;
}

export function updateItemNote(
  session: ReviewSession,
  checkId: string,
  note: string,
): void {
  if (!session.items[checkId]) {
    session.items[checkId] = { status: 'pending', note: '', screenshots: [] };
  }
  session.items[checkId].note = note;
}

export function addScreenshot(
  session: ReviewSession,
  checkId: string,
  filename: string,
): void {
  if (!session.items[checkId]) {
    session.items[checkId] = { status: 'pending', note: '', screenshots: [] };
  }
  session.items[checkId].screenshots.push(filename);
}

// ── Actions (side-effectful, API-backed) ──

export function getRunId(): string | null {
  return localStorage.getItem(RUN_ID_KEY);
}

export function setRunId(runId: string): void {
  localStorage.setItem(RUN_ID_KEY, runId);
}

export function clearRunId(): void {
  localStorage.removeItem(RUN_ID_KEY);
}

export async function startSession(): Promise<{ runId: string; session: ReviewSession }> {
  const resp = await fetch('/api/start', { method: 'POST' });
  if (!resp.ok) throw new Error(`Failed to start session: ${resp.statusText}`);
  const data = await resp.json();
  setRunId(data.runId);
  return { runId: data.runId, session: data.session as ReviewSession };
}

export async function loadSession(): Promise<ReviewSession> {
  const runId = getRunId();
  if (!runId) return createSession();

  try {
    const resp = await fetch(`/api/session?runId=${encodeURIComponent(runId)}`);
    if (resp.ok) {
      const data = await resp.json();
      return data.session as ReviewSession;
    }
  } catch { /* fallback below */ }

  return createSession();
}

export async function saveSession(session: ReviewSession): Promise<void> {
  const runId = getRunId();
  if (!runId) return; // No active run, skip

  session.updatedAt = new Date().toISOString();
  await fetch('/api/save', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ runId, session }),
  });
}

export async function uploadScreenshot(checkId: string, file: File): Promise<string[]> {
  const runId = getRunId();
  if (!runId) return [];

  const formData = new FormData();
  formData.append('runId', runId);
  formData.append('checkId', checkId);
  formData.append('file', file);

  const resp = await fetch('/api/screenshot', {
    method: 'POST',
    body: formData,
  });
  if (!resp.ok) throw new Error(`Failed to upload screenshot: ${resp.statusText}`);
  const data = await resp.json();
  return data.filenames as string[];
}

export async function concludeSession(): Promise<void> {
  const runId = getRunId();
  if (!runId) return;

  await fetch('/api/conclude', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ runId }),
  });
  clearRunId();
}

export async function resetSession(): Promise<ReviewSession> {
  clearRunId();
  return createSession();
}
