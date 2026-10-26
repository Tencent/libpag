// Copyright (C) 2026 Tencent. All rights reserved.
'use strict';
const fs = require('fs');
const os = require('os');
const path = require('path');
const { runPagxImport } = require('../dist/lib/pagx-runner');

let dir;
beforeEach(() => { dir = fs.mkdtempSync(path.join(os.tmpdir(), 'font-replacement-')); });
afterEach(() => fs.rmSync(dir, { recursive: true, force: true }));

const availableFonts = [{ fontName: { family: 'SOURCE', style: 'SemiBold' } }];
const html = '<html><body><span style="font-family:Source,sans-serif;font-weight:650">A</span></body></html>';

// A stand-in `pagx` that records its argv (NUL separated so empty values survive),
// copies the HTML it was handed and writes a PAGX document to --output. The copy
// proves the catalog is matched against the snapshot instead of rewriting it.
function fakePagx() {
  const argvPath = path.join(dir, 'argv.bin');
  const copiedPath = path.join(dir, 'import-input.html');
  const bin = path.join(dir, 'fake-pagx');
  fs.writeFileSync(bin, [
    '#!/bin/sh',
    `printf '%s\\0' "$@" > ${JSON.stringify(argvPath)}`,
    'out=""',
    'input=""',
    'while [ "$#" -gt 0 ]; do',
    '  if [ "$1" = "--output" ]; then out="$2"; shift; fi',
    '  if [ "$1" = "--input" ]; then input="$2"; shift; fi',
    '  shift',
    'done',
    `cp "$input" ${JSON.stringify(copiedPath)}`,
    'printf \'<pagx><Layer/></pagx>\' > "$out"',
  ].join('\n'));
  fs.chmodSync(bin, 0o755);
  return { bin, argvPath, copiedPath };
}

function readArgv(argvPath) {
  return fs.readFileSync(argvPath, 'utf8').split('\0').slice(0, -1);
}

function replacementArgs(argv) {
  const offset = argv.indexOf('--font-replacement');
  return offset < 0 ? [] : argv.slice(offset + 1, offset + 6);
}

test('replaces catalogued families before native import without rewriting the snapshot', async () => {
  const fake = fakePagx();
  const logs = [];
  await runPagxImport({ pagxBin: fake.bin, html, availableFonts, log: (m) => logs.push(m) });
  const argv = readArgv(fake.argvPath);
  expect(replacementArgs(argv)).toEqual(['Source,sans-serif', '650', 'normal', 'SOURCE', 'SemiBold']);
  expect(fs.readFileSync(fake.copiedPath, 'utf8')).toBe(html);
  expect(fs.readFileSync(fake.copiedPath, 'utf8')).not.toContain('data-pagx-font-request');
  expect(logs.some((line) => line === '[font-map] 1 replaced, 0 unmatched, 0 without source requests'))
    .toBe(true);
});

test('without a catalog no replacement is requested', async () => {
  const fake = fakePagx();
  const logs = [];
  await runPagxImport({ pagxBin: fake.bin, html, log: (m) => logs.push(m) });
  expect(replacementArgs(readArgv(fake.argvPath))).toEqual([]);
  expect(fs.readFileSync(fake.copiedPath, 'utf8')).toBe(html);
  expect(logs.some((line) => line.startsWith('[font-map]'))).toBe(false);
});

test('rejects a malformed catalog before spawning native conversion', async () => {
  const fake = fakePagx();
  await expect(runPagxImport({ pagxBin: fake.bin, html, availableFonts: [{}] }))
    .rejects.toThrow(/INVALID_AVAILABLE_FONTS/);
  expect(fs.existsSync(fake.argvPath)).toBe(false);
});

test('keys the replacement on the computed request and targets the cut the snapshot declared', async () => {
  const fake = fakePagx();
  const declared = '<html><head><style>@font-face{font-family:"Cinzel Decorative";' +
    'src:url("data:font/woff2;base64,AA");font-weight:700;font-style:normal}</style></head>' +
    '<body><span style="font-family:\'Cinzel Decorative\'">A</span></body></html>';
  await runPagxImport({
    pagxBin: fake.bin,
    html: declared,
    availableFonts: [
      { fontName: { family: 'Cinzel Decorative', style: 'Regular' } },
      { fontName: { family: 'Cinzel Decorative', style: 'Bold' } },
    ],
  });
  expect(replacementArgs(readArgv(fake.argvPath)))
    .toEqual(["'Cinzel Decorative'", '400', 'normal', 'Cinzel Decorative', 'Bold']);
});
