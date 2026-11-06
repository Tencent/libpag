// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
'use strict';
const fs = require('fs');
const os = require('os');
const path = require('path');
const { Readable } = require('stream');
const { inspectZip, prepareZip, zipLimits } = require('../dist/lib/zip-source');
const { writeZip } = require('./archive-helpers');
let dir, archive;
beforeEach(() => { dir = fs.mkdtempSync(path.join(os.tmpdir(), 'zip-source-edge-')); archive = path.join(dir, 'site.zip'); });
afterEach(() => { jest.restoreAllMocks(); fs.rmSync(dir, { recursive: true, force: true }); });
function centralEntry(name, change) {
  const bytes = fs.readFileSync(archive);
  let offset = bytes.indexOf(Buffer.from([0x50, 0x4b, 0x01, 0x02]));
  while (offset >= 0 && bytes.readUInt32LE(offset) === 0x02014b50) {
    const length = bytes.readUInt16LE(offset + 28);
    if (bytes.subarray(offset + 46, offset + 46 + length).toString() === name) {
      change(bytes, offset); fs.writeFileSync(archive, bytes); return;
    }
    offset += 46 + length + bytes.readUInt16LE(offset + 30) + bytes.readUInt16LE(offset + 32);
  }
  throw new Error('missing central directory entry: ' + name);
}

test.each([
  ['encrypted', (bytes, offset) => { bytes.writeUInt16LE(1, offset + 8); bytes.writeUInt16LE(8, offset + 10); }, /Encrypted ZIP/],
  ['compression', (bytes, offset) => bytes.writeUInt16LE(99, offset + 10), /Unsupported ZIP compression/],
  ['special file', (bytes, offset) => bytes.writeUInt32LE((0o020600 << 16) >>> 0, offset + 38), /special file/],
])('rejects unsupported %s records before extraction', async (_name, change, message) => {
  await writeZip(archive, { 'index.html': 'home' });
  centralEntry('index.html', change);
  await expect(inspectZip(archive)).rejects.toMatchObject({ status: 400, exitCode: 2, message: expect.stringMatching(message) });
  await expect(prepareZip(archive)).rejects.toThrow(message);
});

test('rejects directory records containing payload data', async () => {
  await writeZip(archive, { 'site/': '', 'site/index.html': 'home' });
  centralEntry('site/', (bytes, offset) => { bytes.writeUInt32LE(1, offset + 20); bytes.writeUInt32LE(1, offset + 24); });
  await expect(inspectZip(archive)).rejects.toThrow(/directory contains file data/);
});

test('extracts deflated entries, ignores Finder metadata, and accepts a dot root', async () => {
  await writeZip(archive, { 'site/index.html': { data: 'compressed home '.repeat(100), options: { compress: true } },
    'site/': '', '.DS_Store': 'metadata', 'site/._index.html': 'metadata', 'asset.txt': '' });
  const source = await prepareZip(archive, { root: '.' });
  try {
    expect(source.root).toBe(''); expect(source.files).toEqual(['asset.txt', 'site/index.html']);
    expect(fs.readFileSync(path.join(source.directory, 'site/index.html'), 'utf8')).toBe('compressed home '.repeat(100));
    expect(fs.readFileSync(path.join(source.directory, 'asset.txt')).length).toBe(0);
  } finally { await source.dispose(); }
});

test('payload failure cleans its private workspace and preserves the source archive', async () => {
  await writeZip(archive, { 'index.html': 'home' });
  centralEntry('index.html', (bytes, offset) => bytes.writeUInt32LE(0, offset + 16));
  const create = fs.promises.mkdtemp;
  let workspace;
  jest.spyOn(fs.promises, 'mkdtemp').mockImplementation(async (...args) => (workspace = await create(...args)));
  await expect(prepareZip(archive)).rejects.toThrow(/checksum mismatch/);
  expect(fs.existsSync(workspace)).toBe(false);
  expect(fs.existsSync(archive)).toBe(true);
});

test.each(['cancel', 'deadline'])('%s interrupts an active copy and removes its workspace', async mode => {
  await writeZip(archive, { 'index.html': 'home' });
  const controller = new AbortController();
  const create = fs.promises.mkdtemp;
  let workspace, entered;
  const started = new Promise(resolve => { entered = resolve; });
  jest.spyOn(fs.promises, 'mkdtemp').mockImplementation(async (...args) => (workspace = await create(...args)));
  const stalled = new Readable({ read() { entered(); } });
  jest.spyOn(fs, 'createReadStream').mockReturnValueOnce(stalled);
  const pending = prepareZip(archive, { signal: controller.signal, limits: { prepareMs: mode === 'deadline' ? 30 : 30000 } });
  const rejected = expect(pending).rejects.toThrow(mode === 'deadline' ? 'ZIP preparation timed out' : 'caller cancelled');
  await started;
  if (mode === 'cancel') controller.abort(new Error('caller cancelled'));
  await rejected;
  expect(stalled.destroyed).toBe(true);
  expect(fs.existsSync(workspace)).toBe(false);
});

test('filesystem failures retain their error code instead of being classified as corrupt ZIPs', async () => {
  await expect(inspectZip(archive)).rejects.toMatchObject({ code: 'ENOENT' });
  const error = Object.assign(new Error('permission denied'), { code: 'EACCES' });
  jest.spyOn(fs.promises, 'stat').mockRejectedValueOnce(error);
  await expect(inspectZip(archive)).rejects.toBe(error);
  await expect(inspectZip(dir)).rejects.toMatchObject({ status: 400 });
});

test('validates environment limits and lets explicit limits override them', () => {
  const original = process.env.HTML_SNAPSHOT_ZIP_ENTRIES;
  try {
    for (const value of ['NaN', '', '-1', '1.5', '9007199254740992']) {
      process.env.HTML_SNAPSHOT_ZIP_ENTRIES = value;
      expect(() => zipLimits()).toThrow(/positive safe integer/);
      expect(zipLimits({ entries: 7 }).entries).toBe(7);
    }
  } finally {
    if (original === undefined) delete process.env.HTML_SNAPSHOT_ZIP_ENTRIES;
    else process.env.HTML_SNAPSHOT_ZIP_ENTRIES = original;
  }
});
