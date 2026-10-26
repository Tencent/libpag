// Run `pagx import --format html` against a snapshot HTML payload and return
// the resulting PAGX (a UTF-8 XML document) as a string. This module is a
// thin in-memory adapter for `pagx import`: callers hand it an HTML string
// and get the PAGX string back, with temp-file plumbing hidden. It powers
// `server.js` so the HTTP service can return PAGX directly instead of the
// intermediate flat HTML.
//
// Evaluation and preview process helpers live in lib/pipeline.ts.
// Full source acquisition and artifact orchestration belong to the native import workflow.
//
// The CLI binary is spawned per request — one tiny native invocation against
// a per-request temp file — which keeps the warm-browser model intact (no
// need to rebuild the C++ importer in-process) while still being fast enough
// that PAGX generation fits in the same request as the snapshot itself.
//
// Why temp files: pagx import takes paths only — it does not accept stdin
// for input nor stdout for output (ImportPlan.cpp parses --input and
// --output as filesystem paths). We therefore have to materialise both the
// input HTML and the output PAGX on disk; the wrapper guarantees the temp
// directory is removed when the call returns, success or failure.

import * as fs from 'fs';
import * as os from 'os';
import * as path from 'path';
import { spawn } from 'child_process';
import { DOMParser, XMLSerializer } from '@xmldom/xmldom';
import { ChildProcessError, type ChildProcessErrorInit } from './errors';
import { prepareSnapshotFontReplacements } from './snapshot-fonts';
import type { FontReplacement } from './snapshot-fonts';
import type { AvailableFont } from './font-matcher';

// Use `<repo-root>/cmake-build-debug/pagx` for development callers that do
// not supply a binary or $PAGX_BIN. We don't verify the file exists here —
// servers may legitimately boot without a pagx build (PAGX-format requests
// then fail at use time with a clear message) — but we do collapse the
// path so logs stay readable.
export function defaultPagxBin(): string {
  if (process.env.PAGX_BIN) return process.env.PAGX_BIN;
  // tools/html-snapshot/dist/lib/pagx-runner.js → repo root is four levels up
  // (dist/lib/<file> → dist/ → tools/html-snapshot/ → tools/ → repo root).
  // This helper executes from the compiled dist/lib directory.
  const repoRoot = path.resolve(__dirname, '..', '..', '..', '..');
  return path.join(repoRoot, 'cmake-build-debug', 'pagx');
}

// Tagged error type so the caller (HTTP handler) can distinguish a pagx
// failure from a snapshot failure and surface a clean 5xx with a
// remediation hint, without losing pagx's stderr trail. Inherits the
// `{ code, stderr }` field initialisation from `ChildProcessError`; the
// override here only sets `name` so `instanceof PagxImportError` and the
// stringified stack still identify the specific failure mode.
export class PagxImportError extends ChildProcessError {
  constructor(message: string, opts?: ChildProcessErrorInit) {
    super(message, opts);
    this.name = 'PagxImportError';
  }
}

interface SpawnPagxImportArgs {
  pagxBin: string;
  htmlPath: string;
  outPath: string;
  fontReplacements?: FontReplacement[];
}

interface SpawnPagxImportResult {
  stderr: string;
}

// Spawn the import command and wait for it to settle. We resolve with
// stderr text (so the caller can log filtered diagnostics) but never fail
// just because pagx printed a warning — only the exit code and the
// existence of the output file matter.
function spawnPagxImport(
  { pagxBin, htmlPath, outPath, fontReplacements = [] }: SpawnPagxImportArgs,
): Promise<SpawnPagxImportResult> {
  return new Promise<SpawnPagxImportResult>((resolve, reject) => {
    const args = [
      'import',
      '--format', 'html',
      '--input', htmlPath,
      '--output', outPath,
      // `pagx import` resolves import directives by default; the server returns the raw imported
      // PAGX and leaves resolution to the caller (see the response comment in server.js), so opt
      // out here to preserve that contract and avoid work the caller may not want.
      '--html-source', 'snapshot', '--images', 'embed', '--fonts', 'external', '--no-resolve',
    ];
    // `--html-source snapshot` marks the payload as an already-rendered subset, so the
    // importer skips its own browser pass. Images are embedded because the HTTP response
    // must survive removal of its temporary output directory. Fonts remain external during
    // import to avoid glyph baking; their file bytes are inlined before returning the response.
    for (const replacement of fontReplacements) {
      args.push('--font-replacement', replacement.families, String(replacement.weight),
        replacement.style, replacement.targetFamily, replacement.targetStyle);
    }
    const child = spawn(pagxBin, args, {
      // stdin: not needed (input is a file path); stdout/stderr piped so we
      // can capture pagx's diagnostics for forwarding into our own log.
      stdio: ['ignore', 'pipe', 'pipe'],
      env: process.env,
    });

    const stderrChunks: Buffer[] = [];
    // We pipe stdout to /dev/null effectively — pagx import only writes a
    // few human-readable lines there ("wrote …") which we don't surface.
    child.stdout?.on('data', () => {});
    child.stderr?.on('data', (c: Buffer) => stderrChunks.push(c));

    child.on('error', (err: Error) => {
      // ENOENT (binary missing) and EACCES (not executable) land here. The
      // exit handler will not fire in this case, so produce a final error
      // ourselves so the promise never hangs.
      reject(new PagxImportError(
        `failed to spawn pagx (${pagxBin}): ${err.message}`,
        { stderr: Buffer.concat(stderrChunks).toString('utf8') },
      ));
    });

    child.on('close', (code: number | null) => {
      const stderr = Buffer.concat(stderrChunks).toString('utf8');
      if (code !== 0) {
        reject(new PagxImportError(
          `pagx import exited with code ${code}`,
          { stderr, code },
        ));
        return;
      }
      resolve({ stderr });
    });
  });
}

// `pagx import --format html` emits one warning per absolute-positioned
// text leaf — the importer's normaliser handles them silently in the
// resulting PAGX, but the warnings still spam stderr at one line per
// element. We strip them here so a 200-element page doesn't produce 200
// lines of noise on every HTTP request. The on-disk pipeline
// (lib/pipeline.ts) re-exports this helper so the CLI and the HTTP
// service surface the same diagnostics.
export function filterKnownWarnings(stderr: string | null | undefined): string {
  if (!stderr) return '';
  return stderr
    .split('\n')
    .filter((line) => !line.includes('warning: html: position: absolute on text leaf'))
    .join('\n')
    .trim();
}

function readPagxWithInlineFonts(pagxPath: string): string {
  const xml = fs.readFileSync(pagxPath, 'utf8');
  const doc = new DOMParser().parseFromString(xml, 'text/xml');
  const fonts = Array.from(doc.getElementsByTagName('Font'));
  const root = fs.realpathSync(path.dirname(pagxPath));
  const embedded = new Map<string, string>();
  let changed = false;
  for (const font of fonts) {
    const file = font.getAttribute('file');
    if (!file) continue;
    const lowerFile = file.toLowerCase();
    if (lowerFile.startsWith('data:') || lowerFile.startsWith('https://') ||
        lowerFile.startsWith('http://')) continue;
    try {
      const resolved = fs.realpathSync(path.resolve(root, file));
      // Only publish files produced inside this import's output directory, including symlinks.
      if (!resolved.startsWith(root + path.sep)) {
        throw new PagxImportError('font resource is outside the import output directory');
      }
      let uri = embedded.get(resolved);
      if (uri === undefined) {
        uri = 'data:application/octet-stream;base64,' + fs.readFileSync(resolved).toString('base64');
        embedded.set(resolved, uri);
      }
      font.setAttribute('file', uri);
      changed = true;
    } catch (error) {
      if (error instanceof PagxImportError) throw error;
      throw new PagxImportError('cannot embed font resource: ' + file);
    }
  }
  return changed ? new XMLSerializer().serializeToString(doc) : xml;
}

export interface RunPagxImportOptions {
  pagxBin?: string;
  html?: string;
  log?: ((msg: string) => void) | null;
  availableFonts?: AvailableFont[];
}

// Convenience wrapper: drop the snapshot HTML into a per-call temp dir,
// run pagx import, inline its local font files, and clean up. Returns the
// PAGX as a UTF-8 string. The temp dir is removed in `finally` so even
// a failed import never leaks bytes onto $TMPDIR.
//
// `opts`:
//   pagxBin    — absolute path to the `pagx` CLI (required)
//   html       — snapshot HTML payload (required)
//   log        — optional `(string) => void` for non-fatal diagnostics
//                (e.g. filtered pagx stderr). When omitted, runs silently.
export async function runPagxImport(
  { pagxBin, html, log, availableFonts }: RunPagxImportOptions = {},
): Promise<string> {
  if (typeof pagxBin !== 'string' || !pagxBin) {
    throw new PagxImportError('pagxBin is required');
  }
  if (typeof html !== 'string' || !html) {
    throw new PagxImportError('html payload is required');
  }
  const prepared = availableFonts === undefined ? undefined : prepareSnapshotFontReplacements(html, availableFonts);
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'html-snapshot-pagx-'));
  const htmlPath = path.join(dir, 'input.subset.html');
  const pagxPath = path.join(dir, 'output.pagx');
  try {
    fs.writeFileSync(htmlPath, html, 'utf8');
    const { stderr } = await spawnPagxImport({
      pagxBin,
      htmlPath,
      outPath: pagxPath,
      fontReplacements: prepared?.replacements,
    });
    if (log) {
      const filtered = filterKnownWarnings(stderr);
      if (filtered) log(`pagx import: ${filtered}`);
    }
    if (!fs.existsSync(pagxPath)) {
      throw new PagxImportError(
        'pagx import did not produce an output file',
        { stderr },
      );
    }
    if (prepared && log) {
      const { replaced, unmatched, missingRequests } = prepared.mapping;
      log(`[font-map] ${replaced} replaced, ${unmatched} unmatched, ${missingRequests} without source requests`);
    }
    return readPagxWithInlineFonts(pagxPath);
  } finally {
    try {
      fs.rmSync(dir, { recursive: true, force: true });
    } catch (_) {
      // Best-effort. A stale temp dir is annoying but not fatal —
      // the OS cleans $TMPDIR on reboot.
    }
  }
}
