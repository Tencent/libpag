// Run `pagx import --format html` against a snapshot HTML payload and return
// the resulting PAGX (a UTF-8 XML document) as a string. This module is a
// thin in-memory adapter for `pagx import`: callers hand it an HTML string
// and get the PAGX string back, with temp-file plumbing hidden. It powers
// `server.js` so the HTTP service can return PAGX directly instead of the
// intermediate flat HTML.
//
// The on-disk pipeline (snapshot.js → pagx import → resolve → render) used by
// the html2pagx CLI and eval/run.js lives in `lib/pipeline.js`; that module
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

'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const { spawn } = require('child_process');
const { ChildProcessError } = require('./errors');

// Recover the `<repo-root>/cmake-build-debug/pagx` path the way html2pagx
// does, so callers that don't pass --pagx-bin / $PAGX_BIN still hit the
// canonical local-dev location. We don't verify the file exists here —
// servers may legitimately boot without a pagx build (PAGX-format requests
// then fail at use time with a clear message) — but we do collapse the
// path so logs stay readable.
function defaultPagxBin() {
  if (process.env.PAGX_BIN) return process.env.PAGX_BIN;
  // tools/html-snapshot/lib/pagx-runner.js → repo root is three levels up.
  const repoRoot = path.resolve(__dirname, '..', '..', '..');
  return path.join(repoRoot, 'cmake-build-debug', 'pagx');
}

// Tagged error type so the caller (HTTP handler) can distinguish a pagx
// failure from a snapshot failure and surface a clean 5xx with a
// remediation hint, without losing pagx's stderr trail. Inherits the
// `{ code, stderr }` field initialisation from `ChildProcessError`; the
// override here only sets `name` so `instanceof PagxImportError` and the
// stringified stack still identify the specific failure mode.
class PagxImportError extends ChildProcessError {
  constructor(message, opts) {
    super(message, opts);
    this.name = 'PagxImportError';
  }
}

// Spawn the import command and wait for it to settle. We resolve with
// stderr text (so the caller can log filtered diagnostics) but never fail
// just because pagx printed a warning — only the exit code and the
// existence of the output file matter.
function spawnPagxImport({ pagxBin, htmlPath, outPath, inferFlex }) {
  return new Promise((resolve, reject) => {
    const args = [
      'import',
      '--format', 'html',
      '--input', htmlPath,
      '--output', outPath,
    ];
    // `--html-infer-flex` recovers display:flex containers from the
    // absolute-positioned snapshot. The html2pagx CLI enables it by
    // default; we mirror that here unless the caller passes false.
    if (inferFlex) args.push('--html-infer-flex');

    const child = spawn(pagxBin, args, {
      // stdin: not needed (input is a file path); stdout/stderr piped so we
      // can capture pagx's diagnostics for forwarding into our own log.
      stdio: ['ignore', 'pipe', 'pipe'],
    });

    const stderrChunks = [];
    // We pipe stdout to /dev/null effectively — pagx import only writes a
    // few human-readable lines there ("wrote …") which we don't surface.
    child.stdout.on('data', () => {});
    child.stderr.on('data', (c) => stderrChunks.push(c));

    child.on('error', (err) => {
      // ENOENT (binary missing) and EACCES (not executable) land here. The
      // exit handler will not fire in this case, so produce a final error
      // ourselves so the promise never hangs.
      reject(new PagxImportError(
        `failed to spawn pagx (${pagxBin}): ${err.message}`,
        { stderr: Buffer.concat(stderrChunks).toString('utf8') },
      ));
    });

    child.on('close', (code) => {
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
// (lib/pipeline.js) re-exports this helper so the CLI and the HTTP
// service surface the same diagnostics.
function filterKnownWarnings(stderr) {
  if (!stderr) return '';
  return stderr
    .split('\n')
    .filter((line) => !line.includes('warning: html: position: absolute on text leaf'))
    .join('\n')
    .trim();
}

// Convenience wrapper: drop the snapshot HTML into a per-call temp dir,
// run pagx import, read the resulting PAGX, and clean up. Returns the
// PAGX as a UTF-8 string. The temp dir is removed in `finally` so even
// a failed import never leaks bytes onto $TMPDIR.
//
// `opts`:
//   pagxBin    — absolute path to the `pagx` CLI (required)
//   html       — snapshot HTML payload (required)
//   inferFlex  — whether to pass --html-infer-flex (default: true)
//   log        — optional `(string) => void` for non-fatal diagnostics
//                (e.g. filtered pagx stderr). When omitted, runs silently.
async function runPagxImport({ pagxBin, html, inferFlex = true, log } = {}) {
  if (typeof pagxBin !== 'string' || !pagxBin) {
    throw new PagxImportError('pagxBin is required');
  }
  if (typeof html !== 'string' || !html) {
    throw new PagxImportError('html payload is required');
  }
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'html-snapshot-pagx-'));
  const htmlPath = path.join(dir, 'input.subset.html');
  const pagxPath = path.join(dir, 'output.pagx');
  try {
    fs.writeFileSync(htmlPath, html, 'utf8');
    const { stderr } = await spawnPagxImport({
      pagxBin,
      htmlPath,
      outPath: pagxPath,
      inferFlex,
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
    return fs.readFileSync(pagxPath, 'utf8');
  } finally {
    try {
      fs.rmSync(dir, { recursive: true, force: true });
    } catch (_) {
      // Best-effort. A stale temp dir is annoying but not fatal —
      // the OS cleans $TMPDIR on reboot.
    }
  }
}

module.exports = {
  defaultPagxBin,
  runPagxImport,
  filterKnownWarnings,
  PagxImportError,
};
