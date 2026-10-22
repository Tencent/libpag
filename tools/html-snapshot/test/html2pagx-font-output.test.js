// Copyright (C) 2026 Tencent. All rights reserved.
'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const { spawnSync } = require('child_process');

test.each(['single', 'batch', 'url'])('protects a font-map input from %s inventory output', mode => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'html2pagx-font-output-'));
  try {
    const html = path.join(dir, 'page.html');
    const map = path.join(dir, 'page.fonts.json');
    fs.writeFileSync(html, '<html/>');
    const rules = JSON.stringify(require('./fixtures/font-map.json'));
    fs.writeFileSync(map, rules);
    const args = mode === 'batch' ? ['--batch', dir] : mode === 'url'
      ? ['https://example.test/page', '--output-name', 'page', '--output-dir', dir] : [html];
    const result = spawnSync(process.execPath, [path.resolve(__dirname, '../html2pagx'),
      ...args, '--font-map', map], { encoding: 'utf8' });
    expect(result.status).toBe(2);
    expect(result.stderr).toContain('--font-map input would be overwritten');
    expect(fs.readFileSync(map, 'utf8')).toBe(rules);
    expect(fs.existsSync(path.join(dir, 'page.pagx'))).toBe(false);
  } finally {
    fs.rmSync(dir, { recursive: true, force: true });
  }
});
