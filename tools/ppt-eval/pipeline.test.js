'use strict';

const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { spawnSync } = require('node:child_process');
const { test } = require('node:test');
const { runCommand } = require('./pipeline');
const { checkAssets } = require('./preflight');

test('large command output retains only the diagnostic tail of each stream', async () => {
  const result = await runCommand(process.execPath, ['-e',
    'process.stdout.write("a".repeat(200000) + "stdout-tail"); process.stderr.write("b".repeat(200000) + "stderr-tail");']);
  assert.equal(result.code, 0);
  assert.equal(Buffer.byteLength(result.stdout), 64 * 1024);
  assert.equal(Buffer.byteLength(result.stderr), 64 * 1024);
  assert.ok(result.stdout.endsWith('stdout-tail'));
  assert.ok(result.stderr.endsWith('stderr-tail'));
});

test('timeouts retain a diagnostic and terminate the process', async () => {
  const result = await runCommand(process.execPath, ['-e', 'setInterval(() => {}, 1000)'], { timeoutMs: 200 });
  assert.equal(result.code, -1);
  assert.equal(result.timedOut, true);
  assert.match(result.stderr, /timed out after 200 ms/);
});

test('LFS preflight checks nested fixtures, declared resources and fonts', (t) => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'ppt-assets-test-'));
  t.after(() => fs.rmSync(dir, { recursive: true, force: true }));
  const paths = ['cases/unit/image.png', 'external.png', 'resources/font/test.otf'];
  for (const file of paths) {
    fs.mkdirSync(path.dirname(path.join(dir, file)), { recursive: true });
    fs.writeFileSync(path.join(dir, file), 'materialized');
  }
  const manifest = { corpora: { sample: { roots: [{ path: 'cases' }], resources: ['external.png'] } } };
  checkAssets(manifest, ['sample'], dir);
  for (const file of paths) {
    fs.writeFileSync(path.join(dir, file), 'version https://git-lfs.github.com/spec/v1\noid sha256:abc\nsize 123\n');
    assert.throws(() => checkAssets(manifest, ['sample'], dir), (error) => {
      assert.ok(error.message.includes(file));
      assert.match(error.message, /git lfs pull/);
      return true;
    });
    fs.writeFileSync(path.join(dir, file), 'materialized');
  }
});

test('asset preflight reports missing inputs with repository-relative paths', (t) => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'ppt-missing-assets-test-'));
  t.after(() => fs.rmSync(dir, { recursive: true, force: true }));
  fs.mkdirSync(path.join(dir, 'resources/font'), { recursive: true });
  const cases = [
    { corpus: { roots: ['cases/missing'] }, missing: 'cases/missing' },
    { corpus: { roots: [{ path: 'cases/missing' }] }, missing: 'cases/missing' },
    { corpus: { resources: ['external/missing.png'] }, missing: 'external/missing.png' },
    { corpus: { decks: [{ inputs: ['slides/missing.pagx'] }] }, missing: 'slides/missing.pagx' },
  ];
  for (const { corpus, missing } of cases) {
    assert.throws(() => checkAssets({ corpora: { sample: corpus } }, ['sample'], dir), {
      message: `asset missing: ${missing}`,
    });
  }
  fs.rmdirSync(path.join(dir, 'resources/font'));
  assert.throws(() => checkAssets({ corpora: { sample: {} } }, ['sample'], dir), {
    message: 'asset missing: resources/font',
  });
});

test('asset preflight terminates ancestor symlink cycles and checks linked assets', (t) => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'ppt-linked-assets-test-'));
  t.after(() => fs.rmSync(dir, { recursive: true, force: true }));
  fs.mkdirSync(path.join(dir, 'cases/unit'), { recursive: true });
  fs.mkdirSync(path.join(dir, 'resources/font'), { recursive: true });
  fs.mkdirSync(path.join(dir, 'external'));
  fs.symlinkSync('..', path.join(dir, 'cases/unit/parent'));
  fs.symlinkSync('../external', path.join(dir, 'cases/linked'));
  fs.writeFileSync(path.join(dir, 'external/image.png'), 'materialized');
  const manifest = { corpora: { sample: { roots: ['cases'], resources: ['external/image.png'] } } };
  checkAssets(manifest, ['sample'], dir);
  fs.writeFileSync(path.join(dir, 'external/image.png'),
    'version https://git-lfs.github.com/spec/v1\noid sha256:abc\nsize 123\n');
  assert.throws(() => checkAssets(manifest, ['sample'], dir), (error) => {
    assert.match(error.message, /Git LFS assets are not materialized/);
    assert.equal(error.message.split('\n').filter((line) => line.endsWith('image.png')).length, 1);
    assert.match(error.message, /git lfs pull/);
    return true;
  });
});

test('PNG fallback rejects scale before executing cases or creating reports', (t) => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'ppt-scale-test-'));
  t.after(() => fs.rmSync(dir, { recursive: true, force: true }));
  const out = path.join(dir, 'out');
  const result = spawnSync(process.execPath, [path.join(__dirname, 'run.js'), '--corpus', 'smoke',
    '--pagx-bin', process.execPath, '--soffice', process.execPath, '--allow-png-fallback',
    '--scale', '2', '--out', out], {
    encoding: 'utf8', env: { ...process.env, PATH: dir, PDF_RASTERIZER_BIN: '' },
  });
  assert.equal(result.status, 1);
  assert.match(result.stderr, /PNG fallback supports only --scale 1/);
  assert.equal(fs.existsSync(out), false);
});
