import { defineConfig, type Plugin } from 'vite';
import { resolve } from 'path';
import { readFileSync, mkdirSync, writeFileSync, existsSync, readdirSync } from 'fs';
import type { IncomingMessage, ServerResponse } from 'http';

const REPO_ROOT = resolve(__dirname, '../..');
const REPORTS_DIR = resolve(REPO_ROOT, 'test-reports');

/** Serve MANUAL_REVIEW.md from repo root at /MANUAL_REVIEW.md */
function serveManualReview(): Plugin {
  const mdPath = resolve(REPO_ROOT, 'MANUAL_REVIEW.md');
  return {
    name: 'serve-manual-review',
    configureServer(server) {
      server.middlewares.use((req, res, next) => {
        if (req.url === '/MANUAL_REVIEW.md') {
          try {
            const content = readFileSync(mdPath, 'utf-8');
            res.setHeader('Content-Type', 'text/markdown; charset=utf-8');
            res.end(content);
          } catch {
            res.statusCode = 404;
            res.end('MANUAL_REVIEW.md not found');
          }
          return;
        }
        next();
      });
    },
  };
}

// ── Helpers ──

function readBody(req: IncomingMessage): Promise<Buffer> {
  return new Promise((resolve, reject) => {
    const chunks: Buffer[] = [];
    req.on('data', (c: Buffer) => chunks.push(c));
    req.on('end', () => resolve(Buffer.concat(chunks)));
    req.on('error', reject);
  });
}

function jsonResponse(res: ServerResponse, status: number, data: unknown) {
  res.writeHead(status, { 'Content-Type': 'application/json' });
  res.end(JSON.stringify(data));
}

function makeRunId(): string {
  const d = new Date();
  const pad = (n: number) => String(n).padStart(2, '0');
  return `${d.getFullYear()}-${pad(d.getMonth() + 1)}-${pad(d.getDate())}-${pad(d.getHours())}${pad(d.getMinutes())}${pad(d.getSeconds())}`;
}

function generateReportFromSession(session: Record<string, unknown>): string {
  const items = (session.items ?? {}) as Record<string, { status?: string; note?: string; screenshots?: string[] }>;
  const counts = { pass: 0, fail: 0, skip: 0, pending: 0 };
  for (const item of Object.values(items)) {
    const s = item.status ?? 'pending';
    if (s in counts) counts[s as keyof typeof counts]++;
  }
  const total = Object.keys(items).length;
  const result = counts.fail > 0 ? '❌ BLOCKERS FOUND' : counts.pending > 0 ? '⚠️ INCOMPLETE' : '✅ ALL PASSED';

  const lines: string[] = [
    `# Test Report`,
    '',
    `| Field | Value |`,
    `|-------|-------|`,
    `| Source | \`${session.sourceFile ?? 'MANUAL_REVIEW.md'}\` |`,
    `| Tester | ${session.tester || '(not set)'} |`,
    `| Started | ${session.startedAt} |`,
    `| Updated | ${session.updatedAt} |`,
    '',
    '## Summary',
    '',
    `| Status | Count |`,
    `|--------|-------|`,
    `| ✅ Pass | ${counts.pass} |`,
    `| ❌ Fail | ${counts.fail} |`,
    `| ⏭ Skip | ${counts.skip} |`,
    `| ⬜ Pending | ${counts.pending} |`,
    `| **Total** | **${total}** |`,
    '',
    `**Result: ${result}**`,
    '',
    '---',
    '',
    '## Details',
    '',
  ];

  for (const [checkId, item] of Object.entries(items)) {
    const icon = item.status === 'pass' ? '✅' : item.status === 'fail' ? '❌' : item.status === 'skip' ? '⏭' : '⬜';
    lines.push(`- ${icon} \`${checkId}\``);
    if (item.note) lines.push(`  - 💬 ${item.note}`);
    if (item.screenshots?.length) {
      for (const ss of item.screenshots) lines.push(`  - 📸 ![screenshot](${ss})`);
    }
  }

  return lines.join('\n');
}

// ── Multipart parser (minimal, for screenshot upload) ──

interface ParsedFile {
  fieldname: string;
  filename: string;
  mimetype: string;
  data: Buffer;
}

function parseMultipart(buf: Buffer, contentType: string): { fields: Record<string, string>; files: ParsedFile[] } {
  const boundaryMatch = contentType.match(/boundary=(?:"([^"]+)"|([^\s;]+))/);
  if (!boundaryMatch) return { fields: {}, files: [] };
  const boundary = boundaryMatch[1] || boundaryMatch[2];
  const boundaryBuf = Buffer.from(`--${boundary}`);

  const fields: Record<string, string> = {};
  const files: ParsedFile[] = [];

  // Split by boundary
  let start = buf.indexOf(boundaryBuf);
  while (start !== -1) {
    start += boundaryBuf.length;
    // Skip \r\n after boundary
    if (buf[start] === 0x0d && buf[start + 1] === 0x0a) start += 2;
    // Check for -- (end boundary)
    if (buf[start] === 0x2d && buf[start + 1] === 0x2d) break;

    const nextBoundary = buf.indexOf(boundaryBuf, start);
    if (nextBoundary === -1) break;

    const part = buf.subarray(start, nextBoundary);
    // Find header/body separator (\r\n\r\n)
    const headerEnd = part.indexOf('\r\n\r\n');
    if (headerEnd === -1) { start = nextBoundary; continue; }

    const headerStr = part.subarray(0, headerEnd).toString('utf-8');
    // Remove trailing \r\n from body (before next boundary)
    let body = part.subarray(headerEnd + 4);
    if (body.length >= 2 && body[body.length - 2] === 0x0d && body[body.length - 1] === 0x0a) {
      body = body.subarray(0, body.length - 2);
    }

    const nameMatch = headerStr.match(/name="([^"]+)"/);
    const filenameMatch = headerStr.match(/filename="([^"]+)"/);
    const mimeMatch = headerStr.match(/Content-Type:\s*(\S+)/i);

    if (nameMatch && filenameMatch) {
      files.push({
        fieldname: nameMatch[1],
        filename: filenameMatch[1],
        mimetype: mimeMatch?.[1] ?? 'application/octet-stream',
        data: body,
      });
    } else if (nameMatch) {
      fields[nameMatch[1]] = body.toString('utf-8');
    }

    start = nextBoundary;
  }

  return { fields, files };
}

/** API routes for filesystem-backed review sessions */
function apiPlugin(): Plugin {
  return {
    name: 'api-plugin',
    configureServer(server) {
      server.middlewares.use(async (req: IncomingMessage, res: ServerResponse, next: () => void) => {
        const url = req.url ?? '';

        // POST /api/start — create a new test run folder
        if (req.method === 'POST' && url === '/api/start') {
          try {
            const runId = makeRunId();
            const runDir = resolve(REPORTS_DIR, runId);
            mkdirSync(resolve(runDir, 'screenshots'), { recursive: true });

            const session = {
              sourceFile: 'MANUAL_REVIEW.md',
              startedAt: new Date().toISOString(),
              updatedAt: new Date().toISOString(),
              tester: '',
              items: {},
            };
            writeFileSync(resolve(runDir, 'session.json'), JSON.stringify(session, null, 2));
            writeFileSync(resolve(runDir, 'report.md'), generateReportFromSession(session));
            jsonResponse(res, 200, { runId, session });
          } catch (e) {
            jsonResponse(res, 500, { error: String(e) });
          }
          return;
        }

        // POST /api/save — update session.json + regenerate report.md
        if (req.method === 'POST' && url === '/api/save') {
          try {
            const body = JSON.parse((await readBody(req)).toString('utf-8'));
            const { runId, session } = body;
            if (!runId || !session) { jsonResponse(res, 400, { error: 'runId and session required' }); return; }

            const runDir = resolve(REPORTS_DIR, runId);
            if (!existsSync(runDir)) { jsonResponse(res, 404, { error: 'Run not found' }); return; }

            session.updatedAt = new Date().toISOString();
            writeFileSync(resolve(runDir, 'session.json'), JSON.stringify(session, null, 2));
            writeFileSync(resolve(runDir, 'report.md'), generateReportFromSession(session));
            jsonResponse(res, 200, { ok: true });
          } catch (e) {
            jsonResponse(res, 500, { error: String(e) });
          }
          return;
        }

        // POST /api/screenshot — upload screenshot file
        if (req.method === 'POST' && url === '/api/screenshot') {
          try {
            const contentType = req.headers['content-type'] ?? '';
            const buf = await readBody(req);
            const { fields, files } = parseMultipart(buf, contentType);
            const runId = fields.runId || fields.runid;
            const checkId = fields.checkId || fields.checkid;

            if (!runId || !checkId) { jsonResponse(res, 400, { error: 'runId and checkId required' }); return; }
            if (files.length === 0) { jsonResponse(res, 400, { error: 'No file uploaded' }); return; }

            const runDir = resolve(REPORTS_DIR, runId);
            if (!existsSync(runDir)) { jsonResponse(res, 404, { error: 'Run not found' }); return; }

            const ssDir = resolve(runDir, 'screenshots');
            mkdirSync(ssDir, { recursive: true });

            const filenames: string[] = [];
            for (const file of files) {
              const ext = file.filename.split('.').pop() || 'png';
              const name = `${checkId}-${Date.now()}.${ext}`;
              writeFileSync(resolve(ssDir, name), file.data);
              filenames.push(`screenshots/${name}`);
            }

            jsonResponse(res, 200, { filenames });
          } catch (e) {
            jsonResponse(res, 500, { error: String(e) });
          }
          return;
        }

        // POST /api/conclude — finalize the report
        if (req.method === 'POST' && url === '/api/conclude') {
          try {
            const body = JSON.parse((await readBody(req)).toString('utf-8'));
            const { runId } = body;
            if (!runId) { jsonResponse(res, 400, { error: 'runId required' }); return; }

            const runDir = resolve(REPORTS_DIR, runId);
            if (!existsSync(runDir)) { jsonResponse(res, 404, { error: 'Run not found' }); return; }

            const session = JSON.parse(readFileSync(resolve(runDir, 'session.json'), 'utf-8'));
            session.concludedAt = new Date().toISOString();
            writeFileSync(resolve(runDir, 'session.json'), JSON.stringify(session, null, 2));
            writeFileSync(resolve(runDir, 'report.md'), generateReportFromSession(session));
            jsonResponse(res, 200, { ok: true });
          } catch (e) {
            jsonResponse(res, 500, { error: String(e) });
          }
          return;
        }

        // GET /api/session?runId=... — load session from disk
        if (req.method === 'GET' && url.startsWith('/api/session')) {
          try {
            const params = new URL(url, 'http://localhost').searchParams;
            const runId = params.get('runId');
            if (!runId) { jsonResponse(res, 400, { error: 'runId required' }); return; }

            const sessionPath = resolve(REPORTS_DIR, runId, 'session.json');
            if (!existsSync(sessionPath)) { jsonResponse(res, 404, { error: 'Session not found' }); return; }

            const session = JSON.parse(readFileSync(sessionPath, 'utf-8'));
            jsonResponse(res, 200, { session });
          } catch (e) {
            jsonResponse(res, 500, { error: String(e) });
          }
          return;
        }

        // GET /api/screenshots/<runId>/<filename> — serve screenshot files
        if (req.method === 'GET' && url.startsWith('/api/screenshots/')) {
          try {
            const parts = url.replace('/api/screenshots/', '').split('/');
            if (parts.length < 2) { jsonResponse(res, 400, { error: 'Invalid path' }); return; }
            const runId = parts[0];
            const filename = parts.slice(1).join('/');
            const filePath = resolve(REPORTS_DIR, runId, 'screenshots', filename);
            if (!existsSync(filePath)) { res.writeHead(404); res.end('Not found'); return; }

            const data = readFileSync(filePath);
            const ext = filename.split('.').pop()?.toLowerCase();
            const mimeMap: Record<string, string> = { png: 'image/png', jpg: 'image/jpeg', jpeg: 'image/jpeg', gif: 'image/gif', webp: 'image/webp' };
            res.writeHead(200, { 'Content-Type': mimeMap[ext ?? ''] ?? 'application/octet-stream' });
            res.end(data);
          } catch (e) {
            res.writeHead(500); res.end(String(e));
          }
          return;
        }

        // DELETE /api/reset — clear test-reports (for E2E testing)
        if (req.method === 'DELETE' && url === '/api/reset') {
          try {
            const { rmSync } = await import('fs');
            if (existsSync(REPORTS_DIR)) {
              for (const entry of readdirSync(REPORTS_DIR)) {
                if (entry === '.gitkeep') continue;
                rmSync(resolve(REPORTS_DIR, entry), { recursive: true, force: true });
              }
            }
            jsonResponse(res, 200, { ok: true });
          } catch (e) {
            jsonResponse(res, 500, { error: String(e) });
          }
          return;
        }

        next();
      });
    },
  };
}

export default defineConfig({
  root: '.',
  plugins: [serveManualReview(), apiPlugin()],
  server: {
    port: 5173,
    open: true,
  },
  resolve: {
    alias: {
      '@': resolve(__dirname, 'src'),
    },
  },
});
