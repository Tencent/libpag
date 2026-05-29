'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const {
  defaultPagxBin,
  runPagxImport,
  PagxImportError,
} = require('../lib/pagx-runner');

describe('defaultPagxBin', () => {
  const saved = process.env.PAGX_BIN;
  afterEach(() => {
    if (saved === undefined) delete process.env.PAGX_BIN;
    else process.env.PAGX_BIN = saved;
  });

  test('honours $PAGX_BIN when set', () => {
    process.env.PAGX_BIN = '/custom/pagx';
    expect(defaultPagxBin()).toBe('/custom/pagx');
  });

  test('falls back to the canonical cmake-build-debug path', () => {
    delete process.env.PAGX_BIN;
    const bin = defaultPagxBin();
    expect(bin.endsWith(path.join('cmake-build-debug', 'pagx'))).toBe(true);
    expect(path.isAbsolute(bin)).toBe(true);
  });
});

describe('PagxImportError', () => {
  test('captures stderr and code', () => {
    const err = new PagxImportError('boom', { stderr: 'detail', code: 3 });
    expect(err).toBeInstanceOf(Error);
    expect(err.name).toBe('PagxImportError');
    expect(err.stderr).toBe('detail');
    expect(err.code).toBe(3);
  });

  test('defaults stderr to empty and code to null', () => {
    const err = new PagxImportError('boom');
    expect(err.stderr).toBe('');
    expect(err.code).toBeNull();
  });
});

describe('runPagxImport — input validation', () => {
  test('rejects a missing pagxBin', async () => {
    await expect(runPagxImport({ html: '<div></div>' }))
      .rejects.toThrow(/pagxBin is required/);
  });

  test('rejects a missing html payload', async () => {
    await expect(runPagxImport({ pagxBin: '/bin/pagx' }))
      .rejects.toThrow(/html payload is required/);
  });
});

describe('runPagxImport — spawn behaviour', () => {
  test('surfaces a spawn failure for a non-existent binary', async () => {
    const missing = path.join(os.tmpdir(), 'definitely-not-a-real-pagx-bin');
    await expect(runPagxImport({ pagxBin: missing, html: '<div></div>' }))
      .rejects.toBeInstanceOf(PagxImportError);
  });

  test('cleans up the temp dir even when the import fails', async () => {
    const before = fs.readdirSync(os.tmpdir())
      .filter((n) => n.startsWith('html-snapshot-pagx-')).length;
    const missing = path.join(os.tmpdir(), 'definitely-not-a-real-pagx-bin');
    await runPagxImport({ pagxBin: missing, html: '<div></div>' }).catch(() => {});
    const after = fs.readdirSync(os.tmpdir())
      .filter((n) => n.startsWith('html-snapshot-pagx-')).length;
    expect(after).toBeLessThanOrEqual(before);
  });

  test('returns PAGX and filters known warnings via a wrapper binary', async () => {
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'pagx-wrap-'));
    const wrapper = path.join(dir, 'pagx');
    // A shell wrapper that ignores the `import --format html --input X`
    // arguments, writes a PAGX document to the path following --output, and
    // emits one filtered + one real warning on stderr.
    fs.writeFileSync(wrapper, [
      '#!/bin/sh',
      'out=""',
      'while [ "$#" -gt 0 ]; do',
      '  if [ "$1" = "--output" ]; then out="$2"; shift; fi',
      '  shift',
      'done',
      'echo "warning: html: position: absolute on text leaf" 1>&2',
      'echo "real warning: keep me" 1>&2',
      'printf "<pag/>" > "$out"',
    ].join('\n'));
    fs.chmodSync(wrapper, 0o755);
    const logs = [];
    try {
      const pagx = await runPagxImport({
        pagxBin: wrapper,
        html: '<div>hi</div>',
        log: (m) => logs.push(m),
      });
      expect(pagx).toBe('<pag/>');
      const joined = logs.join('\n');
      expect(joined).toMatch(/real warning: keep me/);
      expect(joined).not.toMatch(/position: absolute on text leaf/);
    } finally {
      fs.rmSync(dir, { recursive: true, force: true });
    }
  });

  test('errors when the binary exits 0 but writes no output file', async () => {
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'pagx-noout-'));
    const wrapper = path.join(dir, 'pagx');
    fs.writeFileSync(wrapper, ['#!/bin/sh', 'exit 0'].join('\n'));
    fs.chmodSync(wrapper, 0o755);
    try {
      await expect(runPagxImport({ pagxBin: wrapper, html: '<div></div>' }))
        .rejects.toThrow(/did not produce an output file/);
    } finally {
      fs.rmSync(dir, { recursive: true, force: true });
    }
  });
});
