// Run `pagx import --format html` against a snapshot HTML payload and return
// the resulting PAGX (a UTF-8 XML document) as a string. This module is a
// thin in-memory adapter for `pagx import`: callers hand it an HTML string
// and get the PAGX string back, with temp-file plumbing hidden. It powers
// `server.js` so the HTTP service can return PAGX directly instead of the
// intermediate flat HTML.
//
// The on-disk pipeline (snapshot.js → pagx import → resolve → render) used by
// the pagx import CLI and eval/run.js lives in `lib/pipeline.ts`; that module
// also exposes a `filterKnownWarnings` helper that mirrors what we do here
// for stderr cleanup, so the two stay aligned.
//
// The CLI binary is spawned per request — one tiny native invocation against
// a per-request temp file — which keeps the warm-browser model intact (no
// need to rebuild the C++ importer in-process) while still being fast enough
// that PAGX generation fits in the same request as the snapshot itself.
//
// Why temp files: pagx import takes paths only — it does not accept stdin
// for input nor stdout for output (CommandImport.cpp parses --input and
// --output as filesystem paths). We therefore have to materialise both the
// input HTML and the output PAGX on disk; the wrapper guarantees the temp
// directory is removed when the call returns, success or failure.

import { validatePagx } from './artifact-publication';
import * as fs from 'fs';
import * as os from 'os';
import * as path from 'path';
import { runProcess } from './process-runner';
import { ConversionTask, withConversionTask } from './conversion-task';
import { ChildProcessError, type ChildProcessErrorInit } from './errors';
import { prepareSnapshotFontReplacements } from './snapshot-fonts';
import type { FontReplacement } from './snapshot-fonts';
import type { AvailableFont } from './font-matcher';

// Recover the `<repo-root>/cmake-build-debug/pagx` path the way pagx import
// does, so callers that don't pass --pagx-bin / $PAGX_BIN still hit the
// canonical local-dev location. We don't verify the file exists here —
// servers may legitimately boot without a pagx build (PAGX-format requests
// then fail at use time with a clear message) — but we do collapse the
// path so logs stay readable.
export function defaultPagxBin(): string {
  if (process.env.PAGX_BIN) return process.env.PAGX_BIN;
  // tools/html-snapshot/dist/lib/pagx-runner.js → repo root is four levels up
  // (dist/lib/<file> → dist/ → tools/html-snapshot/ → tools/ → repo root).
  // Source layout (lib/<file>) and compiled layout (dist/lib/<file>) both
  // resolve correctly: __dirname is the location of the running .js file.
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
  task?: ConversionTask;
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
  { pagxBin, htmlPath, outPath, fontReplacements = [], task }: SpawnPagxImportArgs,
): Promise<SpawnPagxImportResult> {
  const args = [
    'import',
    '--format', 'html',
    '--input', htmlPath,
    '--output', outPath,
    // `pagx import` resolves import directives by default; the server returns the raw imported
    // PAGX and leaves resolution to the caller (see the response comment in server.js), so opt
    // out here to preserve that contract and avoid work the caller may not want.
    '--no-resolve', '--html-no-snapshot',
  ];
  for (const replacement of fontReplacements) {
    args.push(...(replacement.families.startsWith('-')
    ? [`--font-replacement=${replacement.families}`] : ['--font-replacement', replacement.families]),
    String(replacement.weight),
      replacement.style, replacement.targetFamily, replacement.targetStyle);
  }
  return runProcess(pagxBin, args, { task }).then(result => {
    task?.checkpoint();
    if (result.code !== 0) throw new PagxImportError('pagx import failed', result);
    return { stderr: result.stderr };
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

export interface RunPagxImportOptions {
  task?: ConversionTask;
  pagxBin?: string;
  html?: string;
  log?: ((msg: string) => void) | null;
  availableFonts?: AvailableFont[];
}

// Convenience wrapper: drop the snapshot HTML into a per-call temp dir,
// run pagx import, read the resulting PAGX, and clean up. Returns the
// PAGX as a UTF-8 string. The temp dir is removed in `finally` so even
// a failed import never leaks bytes onto $TMPDIR.
//
// `opts`:
//   pagxBin    — absolute path to the `pagx` CLI (required)
//   html       — snapshot HTML payload (required)
//   log        — optional `(string) => void` for non-fatal diagnostics
//                (e.g. filtered pagx stderr). When omitted, runs silently.
export function runPagxImport(options: RunPagxImportOptions = {}): Promise<string> {
  return withConversionTask(options.task, task => importSnapshot({ ...options, task }));
}

async function importSnapshot({ pagxBin, html, log, availableFonts, task }: RunPagxImportOptions): Promise<string> {
  task!.checkpoint('import');
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
    task!.checkpoint();
    const { stderr } = await spawnPagxImport({
      task,
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
    const result = fs.readFileSync(pagxPath, 'utf8');
    validatePagx(result);
    task!.checkpoint();
    return result;
  } finally {
    task!.deferCleanup(() => fs.promises.rm(dir, { recursive: true, force: true }));
  }
}
