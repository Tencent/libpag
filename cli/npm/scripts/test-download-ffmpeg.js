#!/usr/bin/env node
'use strict';

const assert = require('assert');
const crypto = require('crypto');
const fs = require('fs');
const http = require('http');
const { spawnSync } = require('child_process');
const os = require('os');
const path = require('path');
const {
  downloadFile,
  ensureFFmpeg,
  findCachedFFmpeg,
  findFFmpegOnPath,
  formatDownloadError,
  hasExplicitFFmpeg,
  isMP4Export,
  parseChecksums,
  resolveFFmpegForCLI,
  resolveTarget,
} = require('./download-ffmpeg.js');

const tests = [];
function test(name, body) {
  tests.push({ name, body });
}

function tempDirectory() {
  return fs.mkdtempSync(path.join(os.tmpdir(), 'pagx-ffmpeg-test-'));
}

function fakeInstaller({ archive, checksums, downloads, waitMs = 0 }) {
  return {
    downloadFile: async (url, destination) => {
      downloads.push(url);
      if (waitMs) await new Promise((resolve) => setTimeout(resolve, waitMs));
      fs.writeFileSync(destination, url.endsWith('/SHA256SUMS') ? checksums : archive, { flag: 'wx' });
    },
    extractArchive: async (_archive, destination, platform) => {
      const name = platform === 'win32' ? 'ffmpeg.exe' : 'ffmpeg';
      const executable = path.join(destination, 'bin', name);
      fs.mkdirSync(path.dirname(executable), { recursive: true });
      fs.writeFileSync(executable, 'fake ffmpeg');
      if (platform !== 'win32') fs.chmodSync(executable, 0o755);
    },
  };
}

test('maps every pagx platform and architecture', () => {
  assert.strictEqual(resolveTarget('darwin', 'arm64').assetName, 'ffmpeg-mac-arm64.tar.gz');
  assert.strictEqual(resolveTarget('darwin', 'x64').assetName, 'ffmpeg-mac-x64.tar.gz');
  assert.strictEqual(resolveTarget('linux', 'arm64').assetName, 'ffmpeg-linux-arm64.tar.gz');
  assert.strictEqual(resolveTarget('linux', 'x64').assetName, 'ffmpeg-linux-x64.tar.gz');
  assert.strictEqual(resolveTarget('win32', 'arm64').assetName, 'ffmpeg-win-arm64.zip');
  assert.strictEqual(resolveTarget('win32', 'x64').assetName, 'ffmpeg-win-x64.zip');
  assert.strictEqual(resolveTarget('win32', 'x64').executableName, 'ffmpeg.exe');
  assert.throws(() => resolveTarget('darwin', 'ia32'), /not available/);
});

test('detects MP4 exports that require the ffmpeg installation prompt', () => {
  assert.strictEqual(isMP4Export(['export', '--format', 'mp4', '--input', 'a.pagx']), true);
  assert.strictEqual(isMP4Export(['export', '--input', 'a.pagx', '--output', 'a.MP4']), true);
  assert.strictEqual(isMP4Export(['export', '--output', 'a.mp4', '--format', 'svg']), false);
  assert.strictEqual(isMP4Export(['export', '--format=mp4']), true);
  assert.strictEqual(isMP4Export(['export', '--format', 'mp4', '--help']), false);
  assert.strictEqual(isMP4Export(['render', '--format', 'mp4']), false);
  assert.strictEqual(isMP4Export(['export', '--input', 'a.pagx']), false);
});

test('recognizes explicit paths and existing PATH executables', () => {
  const root = tempDirectory();
  try {
    assert.strictEqual(hasExplicitFFmpeg([], { FFMPEG_BIN: '/tmp/ffmpeg' }), true);
    assert.strictEqual(hasExplicitFFmpeg(['--ffmpeg', '/tmp/ffmpeg'], {}), true);
    assert.strictEqual(hasExplicitFFmpeg([], {}), false);
    const executable = path.join(root, process.platform === 'win32' ? 'ffmpeg.exe' : 'ffmpeg');
    fs.writeFileSync(executable, 'fake');
    if (process.platform !== 'win32') fs.chmodSync(executable, 0o755);
    assert.strictEqual(findFFmpegOnPath({ PATH: root }), executable);
    assert.strictEqual(findFFmpegOnPath({ PATH: path.join(root, 'missing') }), null);
  } finally {
    fs.rmSync(root, { recursive: true, force: true });
  }
});

test('prefers the downloaded ffmpeg over PATH and honors explicit overrides', () => {
  const root = tempDirectory();
  try {
    const target = resolveTarget();
    const pathDir = path.join(root, 'path');
    fs.mkdirSync(pathDir, { recursive: true });
    const pathFFmpeg = path.join(pathDir, target.executableName);
    fs.writeFileSync(pathFFmpeg, 'PATH ffmpeg');

    const cacheRoot = path.join(root, 'cache');
    const cachedFFmpeg = path.join(cacheRoot, 'v1.0.0', target.name, 'bin', target.executableName);
    fs.mkdirSync(path.dirname(cachedFFmpeg), { recursive: true });
    fs.writeFileSync(cachedFFmpeg, 'cached ffmpeg');
    if (target.platform !== 'win32') {
      fs.chmodSync(pathFFmpeg, 0o755);
      fs.chmodSync(cachedFFmpeg, 0o755);
    }

    const options = { cacheRoot, env: { PATH: pathDir } };
    assert.strictEqual(resolveFFmpegForCLI([], options), cachedFFmpeg);
    assert.strictEqual(resolveFFmpegForCLI(['--ffmpeg', '/custom/ffmpeg'], options), null);
    assert.strictEqual(resolveFFmpegForCLI([], { ...options, env: { FFMPEG_BIN: '/custom/ffmpeg' } }), null);

    options.cacheRoot = path.join(root, 'empty-cache');
    assert.strictEqual(resolveFFmpegForCLI([], options), pathFFmpeg);
  } finally {
    fs.rmSync(root, { recursive: true, force: true });
  }
});

test('parses SHA256SUMS in text and binary formats', () => {
  const checksums = parseChecksums(`${'a'.repeat(64)}  ffmpeg-mac-arm64.tar.gz\n${'b'.repeat(64)} *ffmpeg-win-x64.zip\n`);
  assert.strictEqual(checksums.get('ffmpeg-mac-arm64.tar.gz'), 'a'.repeat(64));
  assert.strictEqual(checksums.get('ffmpeg-win-x64.zip'), 'b'.repeat(64));
});

test('downloads through local HTTP redirects', async () => {
  const root = tempDirectory();
  const server = http.createServer((request, response) => {
    if (request.url === '/redirect') {
      response.writeHead(302, { Location: '/file' });
      response.end();
    } else if (request.url === '/file') {
      response.writeHead(200);
      response.end('payload');
    } else {
      response.writeHead(404);
      response.end();
    }
  });
  try {
    await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
    const port = server.address().port;
    const destination = path.join(root, 'payload');
    await downloadFile(`http://127.0.0.1:${port}/redirect`, destination);
    assert.strictEqual(fs.readFileSync(destination, 'utf8'), 'payload');
    await assert.rejects(downloadFile(`http://127.0.0.1:${port}/missing`, path.join(root, 'bad')), /HTTP 404/);
  } finally {
    await new Promise((resolve) => server.close(resolve));
    fs.rmSync(root, { recursive: true, force: true });
  }
});

test('installs atomically and reuses the cache', async () => {
  const root = tempDirectory();
  const archive = Buffer.from('verified archive');
  const digest = crypto.createHash('sha256').update(archive).digest('hex');
  const checksums = `${digest}  ffmpeg-mac-arm64.tar.gz\n`;
  const downloads = [];
  try {
    const options = {
      platform: 'darwin',
      arch: 'arm64',
      cacheRoot: root,
      baseURL: 'https://example.test/v2.0.0',
      env: {},
      ...fakeInstaller({ archive, checksums, downloads }),
    };
    const executable = await ensureFFmpeg(options);
    assert.strictEqual(executable, path.join(root, 'v1.0.0', 'darwin-arm64', 'bin', 'ffmpeg'));
    assert.strictEqual(downloads.length, 2);
    assert.strictEqual(await ensureFFmpeg(options), executable);
    assert.strictEqual(findCachedFFmpeg(options), executable);
    assert.strictEqual(downloads.length, 2);
  } finally {
    fs.rmSync(root, { recursive: true, force: true });
  }
});

test('replaces an interrupted cache installation', async () => {
  const root = tempDirectory();
  const archive = Buffer.from('verified archive');
  const digest = crypto.createHash('sha256').update(archive).digest('hex');
  const checksums = `${digest}  ffmpeg-mac-arm64.tar.gz\n`;
  const downloads = [];
  try {
    const damagedInstall = path.join(root, 'v1.0.0', 'darwin-arm64');
    fs.mkdirSync(path.join(damagedInstall, 'bin'), { recursive: true });
    fs.writeFileSync(path.join(damagedInstall, 'bin', 'ffmpeg'), 'not executable');
    const executable = await ensureFFmpeg({
      platform: 'darwin',
      arch: 'arm64',
      cacheRoot: root,
      baseURL: 'https://example.test',
      env: {},
      ...fakeInstaller({ archive, checksums, downloads }),
    });
    assert.strictEqual(fs.readFileSync(executable, 'utf8'), 'fake ffmpeg');
    assert.strictEqual(downloads.length, 2);
  } finally {
    fs.rmSync(root, { recursive: true, force: true });
  }
});

test('rejects checksum mismatches and cleans the installation lock', async () => {
  const root = tempDirectory();
  const archive = Buffer.from('bad archive');
  const checksums = `${'0'.repeat(64)}  ffmpeg-mac-arm64.tar.gz\n`;
  const downloads = [];
  try {
    const options = {
      platform: 'darwin',
      arch: 'arm64',
      cacheRoot: root,
      baseURL: 'https://example.test',
      env: {},
      ...fakeInstaller({ archive, checksums, downloads }),
    };
    await assert.rejects(ensureFFmpeg(options), /SHA-256 mismatch/);
    assert.strictEqual(fs.existsSync(path.join(root, 'v1.0.0', 'darwin-arm64', 'bin', 'ffmpeg')), false);
    assert.strictEqual(fs.existsSync(path.join(root, 'v1.0.0', 'darwin-arm64.installing')), false);
  } finally {
    fs.rmSync(root, { recursive: true, force: true });
  }
});

test('shares one installation between concurrent callers', async () => {
  const root = tempDirectory();
  const archive = Buffer.from('verified archive');
  const digest = crypto.createHash('sha256').update(archive).digest('hex');
  const checksums = `${digest}  ffmpeg-mac-arm64.tar.gz\n`;
  const downloads = [];
  try {
    const options = {
      platform: 'darwin',
      arch: 'arm64',
      cacheRoot: root,
      baseURL: 'https://example.test',
      env: {},
      ...fakeInstaller({ archive, checksums, downloads, waitMs: 20 }),
    };
    const results = await Promise.all([ensureFFmpeg(options), ensureFFmpeg(options)]);
    assert.strictEqual(results[0], results[1]);
    assert.strictEqual(downloads.length, 2);
  } finally {
    fs.rmSync(root, { recursive: true, force: true });
  }
});

test('postinstall warns about download failures without failing installation', () => {
  const root = tempDirectory();
  try {
    const result = spawnSync(process.execPath, [path.join(__dirname, 'download-ffmpeg.js'), '--postinstall'], {
      encoding: 'utf8',
      timeout: 10000,
      env: {
        ...process.env,
        PAGX_FFMPEG_BASE_URL: 'http://127.0.0.1:1',
        PAGX_FFMPEG_CACHE_DIR: root,
        PAGX_SKIP_FFMPEG_DOWNLOAD: '0',
      },
    });
    assert.strictEqual(result.status, 0);
    assert.match(result.stderr, /optional ffmpeg download failed/);
    assert.match(result.stderr, /failed to install ffmpeg/);
    assert.match(result.stderr, /npm run download:ffmpeg/);
  } finally {
    fs.rmSync(root, { recursive: true, force: true });
  }
});

test('explicit ffmpeg downloads fail when installation fails', () => {
  const root = tempDirectory();
  try {
    const result = spawnSync(process.execPath, [path.join(__dirname, 'download-ffmpeg.js')], {
      encoding: 'utf8',
      timeout: 10000,
      env: {
        ...process.env,
        PAGX_FFMPEG_BASE_URL: 'http://127.0.0.1:1',
        PAGX_FFMPEG_CACHE_DIR: root,
      },
    });
    assert.strictEqual(result.status, 1);
    assert.match(result.stderr, /failed to install ffmpeg/);
  } finally {
    fs.rmSync(root, { recursive: true, force: true });
  }
});

test('download failures explain manual recovery', () => {
  const message = formatDownloadError(new Error('network unavailable'));
  assert.match(message, /network unavailable/);
  assert.match(message, /FFMPEG_BIN/);
  assert.match(message, /--ffmpeg <path>/);
});

(async () => {
  let failures = 0;
  for (const { name, body } of tests) {
    try {
      await body();
      console.log(`ok - ${name}`);
    } catch (error) {
      ++failures;
      console.error(`not ok - ${name}\n${error.stack}`);
    }
  }
  if (failures) process.exitCode = 1;
  else console.log(`${tests.length} ffmpeg downloader tests passed`);
})();
