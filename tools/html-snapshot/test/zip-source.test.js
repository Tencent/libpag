// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
'use strict';
const fs = require('fs');
const os = require('os');
const path = require('path');
const { inspectZip, prepareZip, zipLimits, zipPath, updateCrc32 } = require('../dist/lib/zip-source');
const { writeZip } = require('./archive-helpers');
let dir, archive;
beforeEach(() => { dir = fs.mkdtempSync(path.join(os.tmpdir(), 'zip-source-test-')); archive = path.join(dir, 'site.zip'); });
afterEach(() => fs.rmSync(dir, { recursive: true, force: true }));

test('enumerates every HTML, preserves paths, excludes metadata, and chooses an explicit root', async () => {
  await writeZip(archive, { 'site/': '', 'site/index.html': 'home', 'site/nested/孤立 #%.HTM': 'orphan',
    'site/test.subset.html': 'subset', 'site/data/data.json': '{}', '__MACOSX/._page.html': 'metadata', 'outside.html': 'outside' });
  const plan = await inspectZip(archive, { root: 'site/' });
  expect(plan.pages).toEqual(['index.html', 'nested/孤立 #%.HTM', 'test.subset.html']);
  expect(plan.files).toContain('data/data.json');
  expect((await inspectZip(archive)).pages).toContain('outside.html');
  const source = await prepareZip(archive, { root: 'site' });
  try {
    expect(source.digest).toBe(plan.digest);
    expect(fs.readFileSync(path.join(source.directory, 'nested/孤立 #%.HTM'), 'utf8')).toBe('orphan');
    expect(fs.existsSync(path.join(source.directory, 'outside.html'))).toBe(false);
  } finally { await source.dispose(); }
  expect(fs.existsSync(source.directory)).toBe(false);
});

test.each(['../escape.html', '/absolute.html', 'C:/drive.html', 'a/../b.html', 'a\\b.html',
  'a//b.html', 'a/./b.html', 'a\0b.html', 'a/NUL.html', 'a./b.html'])('rejects unsafe path %s', name => {
  expect(() => zipPath(name)).toThrow(/ZIP path/);
});

test.each([
  { 'a.html': 'a', 'A.html': 'b' },
  { 'é.html': 'a', 'e\u0301.html': 'b' },
  { 'a.html': 'a', 'a.html/b.html': 'b' },
  { 'dir/a.html': 'a', 'DIR/b.html': 'b' },
])('rejects path aliases and file/directory collisions', async entries => {
  await writeZip(archive, entries);
  await expect(inspectZip(archive)).rejects.toThrow(/collision/);
});

test('rejects symlink entries without following them', async () => {
  await writeZip(archive, { 'index.html': 'home', 'link': { data: '/etc/passwd', options: { mode: 0o120777 } } });
  await expect(prepareZip(archive)).rejects.toThrow(/special file/);
});

test('checks CRC including payloads outside the selected root', async () => {
  await writeZip(archive, { 'site/index.html': 'home', 'other.txt': 'outside-data' });
  const bytes = fs.readFileSync(archive);
  bytes[bytes.indexOf(Buffer.from('outside-data'))] ^= 1;
  fs.writeFileSync(archive, bytes);
  await expect(prepareZip(archive, { root: 'site' })).rejects.toThrow(/checksum mismatch: other.txt/);
});

test('CRC is incremental and agrees with the standard check vector', () => {
  expect(updateCrc32(Buffer.from('123456789'))).toBe(0xcbf43926);
  expect(updateCrc32(Buffer.from('56789'), updateCrc32(Buffer.from('1234')))).toBe(0xcbf43926);
  expect(updateCrc32(Buffer.alloc(0))).toBe(0);
});

test('rejects empty, malformed and page-free archives', async () => {
  fs.writeFileSync(archive, 'not a zip');
  await expect(inspectZip(archive)).rejects.toThrow();
  await writeZip(archive, { 'asset.css': 'body{}' });
  await expect(inspectZip(archive)).rejects.toThrow(/No HTML/);
  await writeZip(archive, {});
  await expect(inspectZip(archive)).rejects.toThrow(/No HTML/);
});

test.each([
  [{ archiveBytes: 10 }, /archive size/], [{ entryBytes: 2 }, /uncompressed size/],
  [{ totalBytes: 6 }, /uncompressed size/], [{ entries: 1 }, /entry count/], [{ pages: 1 }, /page count/],
])('enforces limits before materializing pages: %j', async (limits, error) => {
  await writeZip(archive, { 'a.html': 'aaaa', 'b.html': 'bbbb' });
  await expect(inspectZip(archive, { limits })).rejects.toThrow(error);
  await expect(prepareZip(archive, { limits })).rejects.toThrow(error);
});

test('supports cancellation and validates limit configuration', async () => {
  const controller = new AbortController(); controller.abort(new Error('cancelled by caller'));
  await expect(inspectZip(archive, { signal: controller.signal })).rejects.toThrow(/cancelled by caller/);
  await expect(prepareZip(archive, { signal: controller.signal })).rejects.toThrow(/cancelled by caller/);
  expect(() => zipLimits({ entries: 0 })).toThrow(/positive/);
  expect(() => zipLimits({ prepareMs: 2147483648 })).toThrow(/timer range/);
});

test('bounds path expansion and counts directories omitted from the ZIP directory records', async () => {
  for (const name of ['a/'.repeat(128) + 'index.html', 'a'.repeat(256) + '.html', '目录/'.repeat(100) + 'a'.repeat(256)]) {
    expect(() => zipPath(name)).toThrow(/length or depth/);
  }
  await writeZip(archive, { 'a/b/c/index.html': 'home' });
  await expect(inspectZip(archive, { limits: { entries: 3 } })).rejects.toThrow(/implicit directories/);
});

test('fingerprint changes when only a resource changes', async () => {
  await writeZip(archive, { 'index.html': 'same', 'style.css': 'red' });
  const first = await inspectZip(archive);
  await writeZip(archive, { 'index.html': 'same', 'style.css': 'blue' });
  expect((await inspectZip(archive)).digest).not.toBe(first.digest);
});
