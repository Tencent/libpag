// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
'use strict';
const fs = require('fs');
const os = require('os');
const path = require('path');
const { writeArtifactZip, parseArchiveQuery } = require('../dist/lib/archive-http');
let dir, output, destination;
beforeEach(() => {
  dir = fs.mkdtempSync(path.join(os.tmpdir(), 'archive-packaging-'));
  output = path.join(dir, 'out'); fs.mkdirSync(output);
  destination = path.join(dir, 'result.zip');
});
afterEach(() => { jest.restoreAllMocks(); fs.rmSync(dir, { recursive: true, force: true }); });

test('enforces the response size including ZIP headers and never replaces existing destinations', async () => {
  fs.writeFileSync(path.join(output, 'page.pagx'), '');
  await expect(writeArtifactZip(output, destination, { maxBytes: 10 })).rejects.toMatchObject({ status: 413 });
  fs.writeFileSync(destination, 'preserve');
  await expect(writeArtifactZip(output, destination)).rejects.toMatchObject({ code: 'EEXIST' });
  expect(fs.readFileSync(destination, 'utf8')).toBe('preserve');
});

(process.platform === 'win32' ? test.skip : test)('never follows symbolic links in final artifacts', async () => {
  fs.symlinkSync(__filename, path.join(output, 'linked.js'));
  await expect(writeArtifactZip(output, destination)).rejects.toThrow(/Unexpected non-file output/);
  expect(fs.existsSync(destination)).toBe(false);
});

test('propagates lazy reader failures and closes all opened streams', async () => {
  const file = path.join(output, 'page.pagx'); fs.writeFileSync(file, 'content');
  const create = fs.createReadStream, readers = [];
  jest.spyOn(fs, 'createReadStream').mockImplementation((name, options) => {
    fs.unlinkSync(name);
    const stream = create(name, options); readers.push(stream); return stream;
  });
  await expect(writeArtifactZip(output, destination)).rejects.toMatchObject({ code: 'ENOENT' });
  expect(readers).toHaveLength(1); expect(readers[0].closed).toBe(true);
});

test.each(['viewportWidth=', 'viewportHeight=1.5', 'waitMs=2147483648', 'waitMs=Infinity',
  'preview=true&previewColumns=0', 'render=true&render=false'])('rejects invalid numeric or repeated options: %s', query => {
  expect(() => parseArchiveQuery('/snapshot-archive?' + query)).toThrow();
});

test('preserves explicit false values and selector text', () => {
  expect(parseArchiveQuery('/snapshot-archive?render=false&preview=false&inlineIconFonts=false&captureAnimations=true&scrollReveal=false&selector=%23ready'))
    .toMatchObject({ doRender: false, preview: false, inlineIconFonts: false,
      captureAnimations: true, scrollReveal: false, selector: '#ready', keepSubsetHtml: false });
});
