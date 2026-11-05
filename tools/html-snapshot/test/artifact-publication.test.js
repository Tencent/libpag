// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
'use strict';
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { PNG } = require('pngjs');
const { ArtifactPublication, validatePagx } = require('../dist/lib/artifact-publication');
const { ConversionTask, ConversionTimeoutError } = require('../dist/lib/conversion-task');
const { runHtmlToPagx, convertToArtifacts } = require('../dist/lib/pipeline');
const { runImportRequest } = require('../dist/lib/import-request');

let directory;
beforeEach(() => { directory = fs.mkdtempSync(path.join(os.tmpdir(), 'pagx-publish-test-')); });
afterEach(() => { jest.restoreAllMocks(); fs.rmSync(directory, { recursive: true, force: true }); });
const valid = '<pagx width="8" height="8"><Layer/></pagx>';

function nativeStub(mode) {
  const executable = path.join(directory, 'native.js');
  const png = PNG.sync.write(new PNG({ width: 8, height: 8 })).toString('base64');
  fs.writeFileSync(executable, `#!${process.execPath}
const fs = require('node:fs');
const args = process.argv.slice(2), command = args[0], output = args[args.indexOf('--output') + 1];
const mode = ${JSON.stringify(mode)};
if (command === 'import') fs.writeFileSync(output, mode === 'invalid-pagx' ? '<html/>' : ${JSON.stringify(valid)});
if (command === 'embed' || (mode === 'preview-timeout' && command === 'render' && args[1].endsWith('sheet.pagx'))) {
  setInterval(() => {}, 1000);
} else if (command === 'render') fs.writeFileSync(output, mode === 'invalid-png' ? 'PNG' : Buffer.from(${JSON.stringify(png)}, 'base64'));
`);
  fs.chmodSync(executable, 0o755);
  return executable;
}

test.each(['invalid-pagx', 'invalid-png', 'embed-timeout'])('%s preserves every prior output and removes staging after exit', async mode => {
  const input = path.join(directory, 'page.html');
  fs.writeFileSync(input, '<html/>');
  for (const suffix of ['pagx', 'png', 'subset.html']) fs.writeFileSync(path.join(directory, `result.${suffix}`), `old ${suffix}`);
  const task = new ConversionTask({ totalMs: 650, settleMs: 100 });
  try {
    const operation = runHtmlToPagx({ task, input, outputDir: directory, outputName: 'result',
      pagxBin: nativeStub(mode), scriptDir: directory, embedFonts: mode === 'embed-timeout',
      snapshotImpl: async options => {
        fs.writeFileSync(options.output, '<html><body>new</body></html>');
        return { code: 0, durationMs: 0, stdout: '', stderr: '' };
      } });
    if (mode === 'embed-timeout') await expect(operation).rejects.toBeInstanceOf(ConversionTimeoutError);
    else await expect(operation).rejects.toThrow(/invalid PAGX|PNG/);
  } finally { expect(await task.finish()).toBe(true); }
  for (const suffix of ['pagx', 'png', 'subset.html']) expect(fs.readFileSync(path.join(directory, `result.${suffix}`), 'utf8')).toBe(`old ${suffix}`);
  expect(fs.readdirSync(directory).some(file => file.startsWith('.pagx-stage-'))).toBe(false);
});

test('a requested preview shares the deadline and cannot replace outputs before it completes', async () => {
  const input = path.join(directory, 'source.svg');
  fs.writeFileSync(input, '<svg width="8" height="8"/>');
  for (const extension of ['pagx', 'png']) fs.writeFileSync(path.join(directory, `result.${extension}`), `old ${extension}`);
  const task = new ConversionTask({ totalMs: 800, settleMs: 100 });
  try {
    await expect(runImportRequest({ version: 1, task, input, format: 'svg',
      outputDir: directory, outputName: 'result', pagxBin: nativeStub('preview-timeout'),
      doRender: true, preview: true }, path.resolve(__dirname, '..'))).rejects.toBeInstanceOf(ConversionTimeoutError);
  } finally { expect(await task.finish()).toBe(true); }
  for (const extension of ['pagx', 'png']) expect(fs.readFileSync(path.join(directory, `result.${extension}`), 'utf8')).toBe(`old ${extension}`);
});

test.each(['io-error', 'deadline'])('%s during publication rolls back files already replaced', async mode => {
  let now = 0;
  jest.spyOn(performance, 'now').mockImplementation(() => now);
  const task = new ConversionTask({ totalMs: 1000, settleMs: 100 });
  const publication = new ArtifactPublication(task, directory);
  const png = path.join(directory, 'output.png'), pagx = path.join(directory, 'output.pagx');
  fs.writeFileSync(png, 'old png'); fs.writeFileSync(pagx, 'old pagx');
  for (const target of [png, pagx]) {
    const staged = path.join(publication.directory, path.basename(target));
    fs.writeFileSync(staged, 'new'); publication.add(staged, target);
  }
  const rename = fs.renameSync;
  jest.spyOn(fs, 'renameSync').mockImplementation((from, to) => {
    if (mode === 'io-error' && to === pagx && !path.basename(from).startsWith('backup-')) throw new Error('write failed');
    rename(from, to);
    if (mode === 'deadline' && to === png) now = 1001;
  });
  try { expect(() => publication.commit()).toThrow(); }
  finally { publication.dispose(); await task.finish(); }
  expect(fs.readFileSync(png, 'utf8')).toBe('old png');
  expect(fs.readFileSync(pagx, 'utf8')).toBe('old pagx');
});

test.each(['', '<html/>', '<pagx width="NaN" height="8"/>', '<pagx width="8" height="8"><Layer></pagx>'])('rejects malformed/native error output %s', xml => {
  expect(() => validatePagx(xml)).toThrow();
});

test('an incomplete rollback retains the old bytes for recovery', async () => {
  const task = new ConversionTask();
  const publication = new ArtifactPublication(task, directory);
  for (const extension of ['png', 'pagx']) {
    const target = path.join(directory, `output.${extension}`);
    const staged = path.join(publication.directory, `output.${extension}`);
    fs.writeFileSync(target, `old ${extension}`); fs.writeFileSync(staged, 'new'); publication.add(staged, target);
  }
  const rename = fs.renameSync;
  jest.spyOn(fs, 'renameSync').mockImplementation((from, to) => {
    if (to.endsWith('.pagx') || path.basename(from).startsWith('backup-')) throw new Error('filesystem unavailable');
    rename(from, to);
  });
  expect(() => publication.commit()).toThrow(`backups retained in ${publication.directory}`);
  publication.dispose(); await task.finish();
  expect(fs.readFileSync(path.join(publication.directory, 'backup-0'), 'utf8')).toBe('old png');
});

const native = process.env.PAGX_TEST_BIN || path.resolve(__dirname, '../../../cmake-build-debug/pagx');
(fs.existsSync(native) ? test : test.skip)('external images are staged, rendered and retained immutably across replacement', async () => {
  const sourceDir = path.join(directory, 'source'), outputDir = path.join(directory, 'output');
  fs.mkdirSync(sourceDir); fs.mkdirSync(outputDir);
  const input = path.join(sourceDir, 'input.svg'), image = path.join(directory, 'image.png');
  fs.writeFileSync(input, '<svg xmlns="http://www.w3.org/2000/svg" width="8" height="8"><image href="../image.png" width="8" height="8"/></svg>');
  const references = [];
  for (const color of [[240, 30, 20, 255], [10, 20, 230, 255]]) {
    const pixels = new PNG({ width: 8, height: 8 });
    for (let i = 0; i < pixels.data.length; i += 4) pixels.data.set(color, i);
    const bytes = PNG.sync.write(pixels);
    fs.writeFileSync(image, bytes);
    const result = await convertToArtifacts({ input, outputDir, outputName: 'result', format: 'svg', pagxBin: native, doRender: true }, input);
    const doc = validatePagx(fs.readFileSync(result.pagx, 'utf8'));
    const source = doc.getElementsByTagName('Image')[0].getAttribute('source');
    expect(source).toMatch(/^\.pagx-assets\/[a-f0-9]{64}\.png$/);
    const target = path.join(outputDir, source);
    expect(fs.readFileSync(target)).toEqual(bytes);
    references.push([target, bytes]);
    const rendered = PNG.sync.read(fs.readFileSync(result.png));
    expect([...rendered.data.subarray(0, 4)]).toEqual(color);
  }
  expect(references[0][0]).not.toBe(references[1][0]);
  for (const [file, bytes] of references) expect(fs.readFileSync(file)).toEqual(bytes);
}, 20000);
