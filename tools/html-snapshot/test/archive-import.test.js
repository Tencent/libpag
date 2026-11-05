// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
'use strict';
const fs = require('fs');
const os = require('os');
const path = require('path');
const { preflightArchiveImport, runArchiveImport } = require('../dist/lib/archive-import');
const { validateImportRequest } = require('../dist/lib/import-request');
const { portableWebsiteSnapshot } = require('../dist/lib/website-snapshot');
const { websiteOrigins } = require('../dist/lib/website-proxy');
const { writeZip } = require('./archive-helpers');
let dir, opts;
beforeEach(async () => {
  dir = fs.mkdtempSync(path.join(os.tmpdir(), 'archive-import-test-'));
  opts = { version: 1, format: 'html', input: path.join(dir, 'site.zip'), outputDir: path.join(dir, 'out'),
    pagxBin: '/missing', scriptDir: path.resolve(__dirname, '..'), log: () => {} };
  await writeZip(opts.input, { 'index.html': 'home', 'nested/page.htm': 'other', 'index.htm': 'distinct' });
});
afterEach(() => fs.rmSync(dir, { recursive: true, force: true }));

test('plans stable names without flattening or initializing outputs/browser', async () => {
  const plan = await preflightArchiveImport(opts);
  expect(plan.inputs.map(item => path.relative(opts.outputDir, path.join(item.outputDir, item.outputName + '.pagx'))))
    .toEqual(['index.htm.pagx', 'index.html.pagx', path.join('nested', 'page.htm.pagx')]);
  expect((await runArchiveImport({ ...opts, dryRun: true })).processed).toBe(3);
  expect(fs.existsSync(opts.outputDir)).toBe(false);
});

test('rejects multiple results with -o and output aliases of the input', async () => {
  await expect(preflightArchiveImport({ ...opts, outputDir: undefined, outputFile: path.join(dir, 'one.pagx') }))
    .rejects.toThrow(/Multiple ZIP pages/);
  await writeZip(opts.input, { 'index.html': 'home' });
  const alias = path.join(dir, 'alias.pagx'); fs.linkSync(opts.input, alias);
  await expect(preflightArchiveImport({ ...opts, outputDir: undefined, outputFile: alias })).rejects.toThrow(/collision/);
});

test('validates all planned file/directory and manifest collisions', async () => {
  for (const entries of [
    { 'index.html': 'a', 'index.html.pagx/b.html': 'b' },
    { 'index.html': 'a', 'import-manifest.json/b.html': 'b' },
    { 'index.html': 'a', 'index.html.fonts': 'not-html', 'index.html.fonts/x.html': 'b' },
  ]) {
    await writeZip(opts.input, entries);
    await expect(preflightArchiveImport(opts)).rejects.toThrow(/collision/);
  }
});

test('rejects unsupported CLI combinations at the shared request boundary', () => {
  for (const extra of [{ htmlNoSnapshot: true }, { scenario: 'scenario.json' }, { format: 'svg' }, { outputName: 'all' }]) {
    expect(() => validateImportRequest({ ...opts, ...extra })).toThrow();
  }
  expect(websiteOrigins(['https://cdn.example/', 'https://cdn.example'])).toEqual(['https://cdn.example']);
  for (const origin of ['file:///tmp', 'https://cdn.example/path', 'http://user:pass@host', 'https://cdn.example?q=1']) {
    expect(() => websiteOrigins([origin])).toThrow();
  }
});

test('retained snapshots inline captured files and remove uncaptured resources', () => {
  const imageDir = path.join(dir, 'images'); fs.mkdirSync(imageDir);
  const file = path.join(imageDir, 'image.png'); fs.writeFileSync(file, 'captured');
  const warnings = [];
  const html = portableWebsiteSnapshot(`<html><body><img src="${file}"/><img src="/etc/passwd"/>` +
    '<div style="background-image:url(http://127.0.0.1:123/missing.png)"></div><svg><use href="#local"/></svg></body></html>', imageDir, warnings);
  expect(html).toContain('data:image/png;base64,Y2FwdHVyZWQ=');
  expect(html).not.toContain('/etc/passwd'); expect(html).not.toContain('http://127.0.0.1:123');
  expect(html).toContain('href="#local"'); expect(warnings).toHaveLength(2);
});

test('sanitizes nested SVG and stylesheet resources and preserves captured webfont bytes', () => {
  const font = path.join(dir, 'local.ttf'); fs.writeFileSync(font, 'font');
  const svg = '<svg xmlns="http://www.w3.org/2000/svg"><image href="file:///etc/image.png"/></svg>';
  const warnings = [];
  const html = portableWebsiteSnapshot('<html><head><style>@font-face{src:url("http://site/font.ttf")}</style></head><body>' +
    `<img src="data:image/svg+xml;base64,${Buffer.from(svg).toString('base64')}"/></body></html>`, undefined, warnings,
    new Map([['http://site/font.ttf', font]]));
  expect(html).toContain('data:font/ttf;base64,Zm9udA==');
  expect(html).not.toContain('http://site');
  const encoded = /data:image\/svg\+xml;base64,([^"]+)/.exec(html)[1];
  expect(Buffer.from(encoded, 'base64').toString()).not.toContain('file:///etc');
  expect(warnings).toHaveLength(1);
});

(process.platform === 'win32' ? test.skip : test)('checks output aliases through existing symlink directories', async () => {
  await writeZip(opts.input, { 'a/index.html': 'a', 'b/index.html': 'b' });
  fs.mkdirSync(path.join(opts.outputDir, 'a'), { recursive: true });
  fs.symlinkSync('a', path.join(opts.outputDir, 'b'));
  await expect(preflightArchiveImport(opts)).rejects.toThrow(/collision/);
});
