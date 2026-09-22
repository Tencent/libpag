#!/usr/bin/env node
'use strict';

const crypto = require('crypto');
const fs = require('fs');
const os = require('os');
const path = require('path');
const { spawn } = require('child_process');
const { pathToFileURL } = require('url');

const DEFAULT_TIMEOUT_MS = 120000;

function runCommand(command, args, options = {}) {
  const timeoutMs = options.timeoutMs || DEFAULT_TIMEOUT_MS;
  return new Promise((resolve) => {
    const startedAt = Date.now();
    const child = spawn(command, args, {
      cwd: options.cwd,
      env: options.env || process.env,
      detached: process.platform !== 'win32',
      stdio: ['ignore', 'pipe', 'pipe'],
    });
    let stdout = '';
    let stderr = '';
    let finished = false;
    let timedOut = false;
    const finish = (code) => {
      if (finished) return;
      finished = true;
      clearTimeout(timer);
      resolve({
        code,
        stdout,
        stderr: stderr + (timedOut ? `\ntimed out after ${timeoutMs} ms` : ''),
        durationMs: Date.now() - startedAt,
        timedOut,
      });
    };
    const terminate = () => {
      timedOut = true;
      try {
        if (process.platform === 'win32') child.kill('SIGKILL');
        else process.kill(-child.pid, 'SIGKILL');
      } catch (_) {
        try { child.kill('SIGKILL'); } catch (_) { /* best effort */ }
      }
    };
    const timer = setTimeout(terminate, timeoutMs);
    child.stdout.on('data', (chunk) => { stdout += chunk; });
    child.stderr.on('data', (chunk) => { stderr += chunk; });
    child.on('error', (error) => {
      stderr += `${error.message}\n`;
      finish(-1);
    });
    child.on('close', (code) => finish(timedOut ? -1 : code));
  });
}

function findExecutable(explicit, names, extraCandidates = []) {
  const candidates = [];
  if (explicit) candidates.push(explicit);
  for (const name of names) {
    if (path.isAbsolute(name)) candidates.push(name);
    else {
      for (const dir of (process.env.PATH || '').split(path.delimiter)) {
        if (dir) candidates.push(path.join(dir, name));
      }
    }
  }
  candidates.push(...extraCandidates);
  for (const candidate of candidates) {
    try {
      if (candidate && fs.statSync(candidate).isFile()) return candidate;
    } catch (_) { /* keep searching */ }
  }
  return '';
}

function findSoffice(explicit) {
  return findExecutable(explicit || process.env.SOFFICE_BIN, ['soffice'], [
    '/Applications/LibreOffice.app/Contents/MacOS/soffice',
    '/opt/homebrew/bin/soffice',
    '/usr/local/bin/soffice',
  ]);
}

function findPdfRasterizer(explicit) {
  const requested = explicit || process.env.PDF_RASTERIZER_BIN || '';
  if (requested) {
    const found = findExecutable(requested, []);
    if (!found) throw new Error(`PDF rasterizer not found: ${requested}`);
    const base = path.basename(found).toLowerCase();
    if (!base.includes('pdftocairo') && !base.includes('pdftoppm')) {
      throw new Error(`unsupported PDF rasterizer '${found}' (expected pdftocairo or pdftoppm)`);
    }
    return found;
  }
  return findExecutable('', ['pdftocairo', 'pdftoppm']);
}

function commandError(prefix, result) {
  const detail = (result.stderr || result.stdout || '').trim();
  return new Error(`${prefix}${detail ? `: ${detail}` : ''}`);
}

function isUnresolvedImportError(result) {
  const detail = `${result.stderr || ''}\n${result.stdout || ''}`.toLowerCase();
  return detail.includes('unresolved import directive') && detail.includes('pagx resolve');
}

async function resolveToTemp(pagxBin, input, timeoutMs) {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'pagx-ppt-resolve-'));
  const output = path.join(dir, path.basename(input));
  const result = await runCommand(
    pagxBin,
    ['resolve', input, '--output', output, '--images', 'embed'],
    { timeoutMs },
  );
  if (result.code !== 0 || !fs.existsSync(output)) {
    fs.rmSync(dir, { recursive: true, force: true });
    throw commandError('pagx resolve failed', result);
  }
  return {
    file: output,
    cleanup: () => fs.rmSync(dir, { recursive: true, force: true }),
  };
}

async function withResolveFallback(pagxBin, input, timeoutMs, operation) {
  let result = await operation(input);
  if (result.code === 0 || !isUnresolvedImportError(result)) return result;
  const resolved = await resolveToTemp(pagxBin, input, timeoutMs);
  try {
    result = await operation(resolved.file);
    return result;
  } finally {
    resolved.cleanup();
  }
}

async function renderPagx({ pagxBin, input, output, scale, timeoutMs }) {
  fs.mkdirSync(path.dirname(output), { recursive: true });
  return withResolveFallback(pagxBin, input, timeoutMs, (source) => runCommand(
    pagxBin,
    ['render', '--format', 'png', '--scale', String(scale), '--output', output, source],
    { timeoutMs },
  ));
}

async function exportPptx({ pagxBin, inputs, output, exportArgs, timeoutMs }) {
  fs.mkdirSync(path.dirname(output), { recursive: true });
  const runExport = (sources) => {
    const args = ['export'];
    for (const input of sources) args.push('--input', input);
    args.push('--output', output, '--format', 'pptx', ...exportArgs);
    return runCommand(pagxBin, args, { timeoutMs });
  };
  if (inputs.length !== 1) {
    let result = await runExport(inputs);
    if (result.code === 0 || !isUnresolvedImportError(result)) return result;
    const resolvedInputs = [];
    try {
      for (const input of inputs) resolvedInputs.push(await resolveToTemp(pagxBin, input, timeoutMs));
      result = await runExport(resolvedInputs.map((resolved) => resolved.file));
      return result;
    } finally {
      for (const resolved of resolvedInputs) resolved.cleanup();
    }
  }
  return withResolveFallback(pagxBin, inputs[0], timeoutMs, (source) => runCommand(
    pagxBin,
    ['export', '--input', source, '--output', output, '--format', 'pptx', ...exportArgs],
    { timeoutMs },
  ));
}

function numberedPngs(dir, prefix) {
  const escaped = prefix.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
  const pattern = new RegExp(`^${escaped}-(\\d+)\\.png$`, 'i');
  return fs.readdirSync(dir)
    .map((name) => {
      const match = name.match(pattern);
      return match ? { page: Number(match[1]), path: path.join(dir, name) } : null;
    })
    .filter(Boolean)
    .sort((a, b) => a.page - b.page);
}

async function libreOfficeConvert({ soffice, input, format, outDir, timeoutMs, profileDir }) {
  fs.mkdirSync(outDir, { recursive: true });
  fs.mkdirSync(profileDir, { recursive: true });
  const profileUrl = pathToFileURL(profileDir).href;
  return runCommand(soffice, [
    `-env:UserInstallation=${profileUrl}`,
    '--headless', '--nologo', '--nodefault', '--nolockcheck', '--norestore',
    '--convert-to', format,
    '--outdir', outDir,
    input,
  ], { timeoutMs });
}

async function rasterizePptx({ soffice, pdfRasterizer, pptx, outputDir, timeoutMs, scale = 1 }) {
  const temp = fs.mkdtempSync(path.join(os.tmpdir(), 'pagx-ppt-render-'));
  try {
    const pdfResult = await libreOfficeConvert({
      soffice,
      input: pptx,
      format: 'pdf:impress_pdf_Export',
      outDir: temp,
      timeoutMs,
      profileDir: path.join(temp, 'pdf-profile'),
    });
    const pdf = path.join(temp, `${path.basename(pptx, path.extname(pptx))}.pdf`);
    if (pdfResult.code !== 0 || !fs.existsSync(pdf)) {
      throw commandError('LibreOffice PPTX-to-PDF conversion failed', pdfResult);
    }
    fs.copyFileSync(pdf, path.join(outputDir, 'export.pdf'));

    if (pdfRasterizer) {
      const prefix = path.join(temp, 'slide');
      const args = ['-png', '-r', String(96 * scale), pdf, prefix];
      const rasterResult = await runCommand(pdfRasterizer, args, { timeoutMs });
      if (rasterResult.code !== 0) {
        throw commandError(`${path.basename(pdfRasterizer)} failed`, rasterResult);
      }
      const pages = numberedPngs(temp, 'slide');
      if (!pages.length) throw new Error(`${path.basename(pdfRasterizer)} produced no PNG pages`);
      const outputs = [];
      for (const page of pages) {
        const destination = path.join(outputDir, `actual-${page.page}.png`);
        fs.copyFileSync(page.path, destination);
        outputs.push(destination);
      }
      return { pages: outputs, mode: path.basename(pdfRasterizer) };
    }

    // Compatibility fallback for developer machines without Poppler. It is
    // intentionally limited to a one-slide case; CI should install pdftocairo
    // or pdftoppm so every slide is rasterized explicitly at 96 DPI.
    if (scale !== 1) {
      throw new Error('LibreOffice PNG fallback supports only --scale 1');
    }
    const pngDir = path.join(temp, 'png');
    const pngResult = await libreOfficeConvert({
      soffice,
      input: pptx,
      format: 'png',
      outDir: pngDir,
      timeoutMs,
      profileDir: path.join(temp, 'png-profile'),
    });
    const produced = path.join(pngDir, `${path.basename(pptx, path.extname(pptx))}.png`);
    if (pngResult.code !== 0 || !fs.existsSync(produced)) {
      throw commandError('LibreOffice PPTX-to-PNG fallback failed', pngResult);
    }
    const destination = path.join(outputDir, 'actual-1.png');
    fs.copyFileSync(produced, destination);
    return { pages: [destination], mode: 'soffice-png' };
  } finally {
    fs.rmSync(temp, { recursive: true, force: true });
  }
}

function sha256File(filePath) {
  return crypto.createHash('sha256').update(fs.readFileSync(filePath)).digest('hex');
}

async function toolVersion(command, args = ['--version']) {
  const result = await runCommand(command, args, { timeoutMs: 15000 });
  if (result.code !== 0) return '';
  return `${result.stdout || ''}\n${result.stderr || ''}`.trim().split(/\r?\n/)[0];
}

module.exports = {
  exportPptx,
  findPdfRasterizer,
  findSoffice,
  rasterizePptx,
  renderPagx,
  runCommand,
  sha256File,
  toolVersion,
};
