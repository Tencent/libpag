'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const {
  defaultPagxBin,
  runPagxImport,
  PagxImportError,
} = require('../dist/lib/pagx-runner');

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

  test('rejects when called with no options at all (default {} applies)', async () => {
    await expect(runPagxImport())
      .rejects.toThrow(/pagxBin is required/);
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

  test('does not log when every stderr warning is filtered away', async () => {
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'pagx-allfiltered-'));
    const wrapper = path.join(dir, 'pagx');
    // Emits only a known/filtered warning, so `filterKnownWarnings` returns ''
    // and the `if (filtered)` guard skips the log call entirely.
    fs.writeFileSync(wrapper, [
      '#!/bin/sh',
      'out=""',
      'while [ "$#" -gt 0 ]; do',
      '  if [ "$1" = "--output" ]; then out="$2"; shift; fi',
      '  shift',
      'done',
      'echo "warning: html: position: absolute on text leaf" 1>&2',
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
      expect(logs).toEqual([]);
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

describe('runPagxImport — font resource lifetime', () => {
  let dir;
  beforeEach(() => {
    dir = fs.mkdtempSync(path.join(os.tmpdir(), 'pagx-font-response-'));
  });
  afterEach(() => fs.rmSync(dir, { recursive: true, force: true }));

  function wrapperFor(xml, fonts = {}, symlink = false) {
    const wrapper = path.join(dir, 'pagx');
    fs.writeFileSync(wrapper, [
      '#!' + process.execPath,
      "const fs = require('fs'); const path = require('path');",
      "const out = process.argv[process.argv.indexOf('--output') + 1];",
      "const root = path.dirname(out);",
      'fs.writeFileSync(' + JSON.stringify(path.join(dir, 'output-path')) + ', out);',
      'const fonts = ' + JSON.stringify(fonts) + ';',
      'for (const [name, base64] of Object.entries(fonts)) {',
      '  const target = path.join(root, name);',
      '  fs.mkdirSync(path.dirname(target), { recursive: true });',
      "  fs.writeFileSync(target, Buffer.from(base64, 'base64'));",
      '}',
      symlink ? 'fs.symlinkSync(' + JSON.stringify(path.join(dir, 'secret.otf')) + ', path.join(root, "link.otf"));' : '',
      'fs.writeFileSync(out, ' + JSON.stringify(xml) + ');',
    ].join('\n'));
    fs.chmodSync(wrapper, 0o755);
    return wrapper;
  }

  function expectOutputRemoved() {
    const out = fs.readFileSync(path.join(dir, 'output-path'), 'utf8');
    expect(fs.existsSync(path.dirname(out))).toBe(false);
  }

  test('embeds exact font bytes and keeps aliases and unresolved imports after cleanup', async () => {
    const bytes = Buffer.from([0, 1, 2, 127, 128, 255]);
    const xml = '<pagx><Resources><Font id="first" fontFamily="A &amp; B" file="output.fonts/a.otf"/>' +
      '<Font id="second" fontFamily="Alias" file="output.fonts/a.otf"/></Resources>' +
      '<Layer import="remaining.svg"/><Text fontFamily="Alias" fontStyle="Bold"/></pagx>';
    const result = await runPagxImport({pagxBin: wrapperFor(xml, {'output.fonts/a.otf': bytes.toString('base64')}), html: '<div/>'});
    const {DOMParser} = require('@xmldom/xmldom');
    const doc = new DOMParser().parseFromString(result, 'text/xml');
    const fonts = Array.from(doc.getElementsByTagName('Font'));
    expect(fonts).toHaveLength(2);
    for (const font of fonts) {
      const uri = font.getAttribute('file');
      expect(uri).toMatch(/^data:.*;base64,/);
      expect(Buffer.from(uri.slice(uri.indexOf(',') + 1), 'base64')).toEqual(bytes);
    }
    expect(fonts[0].getAttribute('fontFamily')).toBe('A & B');
    expect(fonts[1].getAttribute('fontFamily')).toBe('Alias');
    expect(doc.getElementsByTagName('Layer')[0].getAttribute('import')).toBe('remaining.svg');
    expect(doc.getElementsByTagName('Text')[0].getAttribute('fontStyle')).toBe('Bold');
    expectOutputRemoved();
  });

  test('leaves inline fonts and remote font references unchanged', async () => {
    const xml = '<pagx><Font file="data:font/woff2;base64,AAEC"/>' +
      '<Font file="https://fonts.example/font.woff2"/><Font id="glyphs"/></pagx>';
    expect(await runPagxImport({pagxBin: wrapperFor(xml), html: '<div/>'})).toBe(xml);
    expectOutputRemoved();
  });

  test('rejects missing local fonts instead of returning broken references', async () => {
    await expect(runPagxImport({pagxBin: wrapperFor('<pagx><Font file="missing.otf"/></pagx>'), html: '<div/>'}))
      .rejects.toBeInstanceOf(PagxImportError);
    expectOutputRemoved();
  });

  test('rejects font paths outside the output directory', async () => {
    const secret = path.join(dir, 'secret.otf');
    fs.writeFileSync(secret, 'must not be returned');
    await expect(runPagxImport({pagxBin: wrapperFor('<pagx><Font file="' + secret + '"/></pagx>'), html: '<div/>'}))
      .rejects.toBeInstanceOf(PagxImportError);
    expectOutputRemoved();
  });

  test('rejects symlink font paths escaping the output directory', async () => {
    fs.writeFileSync(path.join(dir, 'secret.otf'), 'must not be returned');
    await expect(runPagxImport({pagxBin: wrapperFor('<pagx><Font file="link.otf"/></pagx>', {}, true), html: '<div/>'}))
      .rejects.toBeInstanceOf(PagxImportError);
    expectOutputRemoved();
  });
});
