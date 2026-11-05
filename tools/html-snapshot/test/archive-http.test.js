// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
'use strict';
const fs = require('fs');
const os = require('os');
const path = require('path');
const { parseArchiveQuery, writeArtifactZip } = require('../dist/lib/archive-http');
const { readZip } = require('./archive-helpers');

test('archive HTTP options are explicit, bounded and cannot expose host paths or origins', () => {
  expect(parseArchiveQuery('/snapshot-archive?root=site&format=both&preview=true&previewColumns=2&waitMs=0'))
    .toMatchObject({ archiveRoot: 'site', keepSubsetHtml: true, doRender: true, preview: true, previewColumns: 2, waitMs: 0 });
  for (const query of ['root=a&root=b', 'outputDir=/etc', 'allowOrigin=http://localhost', 'format=html',
    'viewportWidth=NaN', 'waitMs=-1', 'preview=maybe', 'previewColumns=99', 'previewColumns=2']) {
    expect(() => parseArchiveQuery('/snapshot-archive?' + query)).toThrow();
  }
});

test('packages artifacts and relative paths while omitting internal records; enforces limits and cancellation', async () => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'archive-pack-test-'));
  const root = path.join(dir, 'outputs'); fs.mkdirSync(path.join(root, 'nested'), { recursive: true });
  fs.writeFileSync(path.join(root, 'import-manifest.json'), '{}');
  fs.writeFileSync(path.join(root, 'nested/page.html.pagx'), '<pagx/>');
  fs.writeFileSync(path.join(root, 'nested/page.html.import.json'), 'internal');
  fs.mkdirSync(path.join(root, 'user.import.json'));
  fs.writeFileSync(path.join(root, 'user.import.json/.pagx-stage-page.html.pagx'), '<pagx/>');
  fs.mkdirSync(path.join(root, '.pagx-stage-private')); fs.writeFileSync(path.join(root, '.pagx-stage-private/input'), 'private');
  try {
    const target = path.join(dir, 'output.zip');
    await writeArtifactZip(root, target);
    expect([...((await readZip(target)).keys())]).toEqual([
      'import-manifest.json', 'nested/page.html.pagx', 'user.import.json/.pagx-stage-page.html.pagx',
    ]);
    await expect(writeArtifactZip(root, path.join(dir, 'too-large.zip'), { maxBytes: 5 })).rejects.toMatchObject({ status: 413 });
    await expect(writeArtifactZip(root, path.join(dir, 'too-many.zip'), { maxFiles: 1 })).rejects.toMatchObject({ status: 413 });
    const controller = new AbortController(); controller.abort(new Error('cancelled'));
    await expect(writeArtifactZip(root, path.join(dir, 'cancelled.zip'), { signal: controller.signal })).rejects.toThrow('cancelled');
  } finally { fs.rmSync(dir, { recursive: true, force: true }); }
});
