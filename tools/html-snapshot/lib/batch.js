// Recursive batch driver for `html2pagx`. Replaces the old
// `batch-html2pagx.sh` bash wrapper. The bash wrapper forked a fresh
// `node html2pagx` per file, which meant Chromium was launched once per
// HTML — the dominant cost on small inputs.
//
// This implementation launches one browser, then loops over the matched
// files calling `runHtmlToPagx({ snapshotImpl: in-process })`. The
// in-process snapshot reuses that single browser via the same
// `lib/snapshot-runner.js` path that `server.js` uses for its long-lived
// HTTP service. The pagx binary is still forked per step (it's a C++
// executable; <50 ms of startup, not a bottleneck), but Chromium
// startup is amortised across the whole batch.
//
// Output conventions match the bash wrapper exactly so existing CI
// log-grepping survives:
//   - `Found N non-subset HTML files under: <dir>`
//   - per-file `[idx/total] process <rel>` / `[idx/total] skip <rel>` /
//     `[idx/total] would process <rel>` lines
//   - terminating `Summary: processed=X skipped=Y failed=Z total=W`
//   - exit code 1 when at least one file failed.

'use strict';

const fs = require('fs');
const path = require('path');

const { launchBrowser } = require('./browser-engine');
const { runSnapshot } = require('./snapshot-runner');
const { errMessage } = require('./cli');
const {
  PipelineStepError,
  runHtmlToPagx,
  isHttpUrl,
} = require('./pipeline');

// Walk `root` recursively and collect every regular file whose name ends
// with `.html` or `.htm`, dropping `.subset.html` / `.subset.htm` outputs
// (those are the by-products of a previous run, not new inputs). Returns
// absolute paths sorted lexicographically so progress lines and Summary
// totals are deterministic across runs and across machines.
function collectHtmlInputs(root) {
  const out = [];
  const stack = [root];
  while (stack.length > 0) {
    const dir = stack.pop();
    let entries;
    try {
      entries = fs.readdirSync(dir, { withFileTypes: true });
    } catch (_err) {
      continue;
    }
    for (const entry of entries) {
      const full = path.join(dir, entry.name);
      if (entry.isDirectory()) {
        stack.push(full);
        continue;
      }
      if (!entry.isFile()) continue;
      const lower = entry.name.toLowerCase();
      if (lower.endsWith('.subset.html') || lower.endsWith('.subset.htm')) continue;
      if (lower.endsWith('.html') || lower.endsWith('.htm')) {
        out.push(full);
      }
    }
  }
  out.sort();
  return out;
}

// Translate an absolute input path under `inputRoot` into the directory
// where its outputs should be written. With `outputRoot` set the input
// tree is mirrored under it; without, outputs sit next to the input
// (the bash wrapper's default).
function resolveOutputDir(file, inputRoot, outputRoot) {
  const rel = path.relative(inputRoot, file);
  const relDir = path.dirname(rel);
  if (!outputRoot) {
    return path.dirname(file);
  }
  if (relDir === '.' || relDir === '') return outputRoot;
  return path.join(outputRoot, relDir);
}

// Strip both the extension and a trailing `.htm` from a basename so
// `foo.html` and `foo.htm` both yield `foo`. Matches `runHtmlToPagx`'s
// own derivation, which the bash wrapper implicitly relied on for its
// `--skip-existing` check.
function basenameStem(file) {
  let stem = path.basename(file, path.extname(file));
  if (stem.endsWith('.htm')) stem = stem.slice(0, -'.htm'.length);
  return stem;
}

// Build the in-process snapshot adapter. The returned function has the
// same `(snapshotArgs) => { code, durationMs, stderr }` shape as
// `pipeline.runSnapshot`, but instead of forking `snapshot.js` it calls
// `lib/snapshot-runner.js` against the long-lived `engineHandle`.
//
// Why pass `engineHandle` in via closure rather than re-launching: every
// call shares the one Chromium process, which is the whole point of the
// batch path. The adapter writes the resulting subset HTML to the same
// `output` path the fork variant would have written to, so the rest of
// `runHtmlToPagx` (pagx import → resolve → render) is unaffected.
function makeInProcessSnapshot(engineHandle) {
  return async function inProcessSnapshot(snapshotArgs) {
    const startedAt = Date.now();
    const targetUrl = isHttpUrl(snapshotArgs.input)
      ? snapshotArgs.input
      : `file://${path.resolve(snapshotArgs.input)}`;
    const stderrChunks = [];
    const log = (msg) => { stderrChunks.push(`html-snapshot: ${msg}`); };
    try {
      const result = await runSnapshot(engineHandle, targetUrl, {
        viewportWidth: snapshotArgs.viewportWidth,
        viewportHeight: snapshotArgs.viewportHeight,
        waitMs: snapshotArgs.waitMs,
        selector: snapshotArgs.selector,
        cookies: snapshotArgs.cookies,
        headers: snapshotArgs.headers,
        inlineIconFonts: snapshotArgs.inlineIconFonts,
        downloadFonts: snapshotArgs.downloadFonts,
        fontDir: snapshotArgs.fontDir,
        downloadImages: snapshotArgs.downloadImages,
        imageDir: snapshotArgs.imageDir,
        log,
      });
      fs.writeFileSync(snapshotArgs.output, result.html, 'utf8');
      const stderr = stderrChunks.join('\n');
      if (stderr) process.stderr.write(stderr + '\n');
      return { code: 0, durationMs: Date.now() - startedAt, stdout: '', stderr: '' };
    } catch (err) {
      const stderr = stderrChunks.concat([errMessage(err)]).join('\n');
      return { code: 1, durationMs: Date.now() - startedAt, stdout: '', stderr };
    }
  };
}

// Run the batch.
//
// Required: `inputDir`, `scriptDir`, `pagxBin`, plus the snapshot/render
// flags `runHtmlToPagx` understands. Optional batch-only flags:
//   `outputRoot`     mirror input tree under this directory; default = next to inputs
//   `skipExisting`   skip files whose `.pagx` target already exists
//   `dryRun`         list matched files without launching a browser or running pagx
//   `browserEngine`  forwarded to `launchBrowser`
//
// Returns `{ processed, skipped, failed, total }`. Caller decides exit code.
async function runBatch(opts) {
  const inputDir = opts.inputDir ? path.resolve(opts.inputDir) : '';
  if (!inputDir) throw new Error('runBatch: inputDir is required');
  if (!fs.existsSync(inputDir) || !fs.statSync(inputDir).isDirectory()) {
    throw new Error(`runBatch: input directory not found: ${inputDir}`);
  }
  let outputRoot = opts.outputRoot || '';
  if (outputRoot) {
    fs.mkdirSync(outputRoot, { recursive: true });
    outputRoot = path.resolve(outputRoot);
  }

  const files = collectHtmlInputs(inputDir);
  const total = files.length;
  if (total === 0) {
    process.stdout.write(`No non-subset HTML files found under: ${inputDir}\n`);
    return { processed: 0, skipped: 0, failed: 0, total: 0 };
  }
  process.stdout.write(`Found ${total} non-subset HTML files under: ${inputDir}\n`);

  // Dry run never opens the browser. Mirrors the bash wrapper's dry-run
  // contract (no side effects), with the user-confirmed simplification:
  // we list the would-process files instead of pretending to print
  // a fork-style command line that no longer exists.
  if (opts.dryRun) {
    let processed = 0;
    let skipped = 0;
    files.forEach((file, idx) => {
      const rel = path.relative(inputDir, file);
      const outDir = resolveOutputDir(file, inputDir, outputRoot);
      const stem = basenameStem(file);
      const target = path.join(outDir, `${stem}.pagx`);
      const index = idx + 1;
      if (opts.skipExisting && fs.existsSync(target)) {
        process.stdout.write(`[${index}/${total}] skip      ${rel}\n`);
        skipped++;
        return;
      }
      process.stdout.write(`[${index}/${total}] would process ${rel}\n`);
      processed++;
    });
    process.stdout.write(`Summary: processed=${processed} skipped=${skipped} failed=0 total=${total}\n`);
    return { processed, skipped, failed: 0, total };
  }

  // Launch Chromium once. The handle is shared across every per-file
  // `runHtmlToPagx` call via the in-process snapshot adapter; closing
  // it in the `finally` block guarantees we don't leak the browser
  // process if the batch aborts midway.
  const engineHandle = await launchBrowser({ engine: opts.browserEngine });
  const snapshotImpl = makeInProcessSnapshot(engineHandle);

  let processed = 0;
  let skipped = 0;
  let failed = 0;

  try {
    for (let i = 0; i < total; i++) {
      const file = files[i];
      const rel = path.relative(inputDir, file);
      const index = i + 1;
      const outDir = resolveOutputDir(file, inputDir, outputRoot);
      const stem = basenameStem(file);
      const pagxTarget = path.join(outDir, `${stem}.pagx`);

      if (opts.skipExisting && fs.existsSync(pagxTarget)) {
        process.stdout.write(`[${index}/${total}] skip      ${rel}\n`);
        skipped++;
        continue;
      }

      fs.mkdirSync(outDir, { recursive: true });
      process.stdout.write(`[${index}/${total}] process   ${rel}\n`);

      try {
        await runHtmlToPagx({
          input: file,
          outputDir: outDir,
          outputName: stem,
          pagxBin: opts.pagxBin,
          scriptDir: opts.scriptDir,
          scale: opts.scale,
          doResolve: opts.doResolve,
          doRender: opts.doRender,
          inferFlex: opts.inferFlex,
          keepSubsetHtml: opts.keepSubsetHtml,
          embedFonts: opts.embedFonts,
          downloadFonts: opts.downloadFonts,
          fontDir: opts.fontDir,
          downloadImages: opts.downloadImages,
          imageDir: opts.imageDir,
          browserEngine: opts.browserEngine,
          inlineIconFonts: opts.inlineIconFonts,
          cookies: opts.cookies,
          headers: opts.headers,
          viewportWidth: opts.viewportWidth,
          viewportHeight: opts.viewportHeight,
          waitMs: opts.waitMs,
          selector: opts.selector,
          log: (line) => process.stdout.write(line + '\n'),
          snapshotImpl,
        });
        processed++;
      } catch (err) {
        failed++;
        const msg = err instanceof PipelineStepError
          ? `${err.step} failed`
          : errMessage(err);
        process.stderr.write(`  failed: ${rel}: ${msg}\n`);
      }
    }
  } finally {
    try {
      await engineHandle.browser.close();
    } catch (closeErr) {
      process.stderr.write(`browser close error: ${errMessage(closeErr)}\n`);
    }
  }

  process.stdout.write(`Summary: processed=${processed} skipped=${skipped} failed=${failed} total=${total}\n`);
  return { processed, skipped, failed, total };
}

module.exports = { runBatch, collectHtmlInputs };
