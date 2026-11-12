#!/usr/bin/env node
'use strict';

const assert = require('assert');
const crypto = require('crypto');
const fs = require('fs');
const http = require('http');
const { spawnSync } = require('child_process');
const os = require('os');
const path = require('path');
const { pipeline } = require('stream/promises');
const tar = require('tar');
const yazl = require('yazl');
const { buildManifest } = require('./generate-ffmpeg-manifest.js');
const {
  accountArchiveEntry,
  downloadFile,
  ensureFFmpeg,
  extractArchive,
  findCachedFFmpeg,
  formatDownloadError,
  resolveTarget,
  validateArchiveEntry,
  validateDownloadURL,
  verifyArchive,
} = require('./download-ffmpeg.js');

const tests = [];
function test(name, body) {
  tests.push({ name, body });
}

function tempDirectory() {
  return fs.mkdtempSync(path.join(os.tmpdir(), 'pagx-ffmpeg-test-'));
}

function pinnedManifest(archive) {
  return {
    release: 'v1.0.0',
    assets: {
      'darwin-arm64': {
        name: 'ffmpeg-mac-arm64.tar.gz',
        size: archive.length,
        sha256: crypto.createHash('sha256').update(archive).digest('hex'),
      },
    },
  };
}

function fakeInstaller({ archive, verifiedArchive = archive, downloads, waitMs = 0 }) {
  return {
    manifest: pinnedManifest(verifiedArchive),
    downloadFile: async (url, destination) => {
      downloads.push(url);
      if (waitMs) await new Promise((resolve) => setTimeout(resolve, waitMs));
      fs.writeFileSync(destination, archive, { flag: 'wx' });
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

function fakeReleaseMetadata() {
  const targets = [
    ['darwin', 'arm64'], ['darwin', 'x64'],
    ['linux', 'arm64'], ['linux', 'x64'],
    ['win32', 'arm64'], ['win32', 'x64'],
  ];
  return {
    tag_name: 'v1.0.0',
    assets: targets.map(([platform, arch]) => ({
      name: resolveTarget(platform, arch).assetName,
      size: 1,
      digest: `sha256:${'0'.repeat(64)}`,
    })),
  };
}

async function createZipArchive(archivePath, entries) {
  const zip = new yazl.ZipFile();
  for (const entry of entries) {
    zip.addBuffer(Buffer.from(entry.contents || ''), entry.name, entry.options || {});
  }
  zip.end();
  await pipeline(zip.outputStream, fs.createWriteStream(archivePath, { flags: 'wx' }));
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

test('builds a complete manifest from official release metadata', () => {
  const manifest = buildManifest(fakeReleaseMetadata());
  assert.strictEqual(manifest.release, 'v1.0.0');
  assert.deepStrictEqual(Object.keys(manifest.assets).sort(), [
    'darwin-arm64', 'darwin-x64', 'linux-arm64', 'linux-x64', 'win32-arm64', 'win32-x64',
  ]);
  assert.strictEqual(manifest.assets['darwin-arm64'].sha256, '0'.repeat(64));
  const incomplete = fakeReleaseMetadata();
  incomplete.assets.pop();
  assert.throws(() => buildManifest(incomplete), /no verified size and SHA-256/);
});

test('validates archive size and hash against the pinned manifest', async () => {
  const root = tempDirectory();
  try {
    const archive = Buffer.from('verified archive');
    const archivePath = path.join(root, 'archive');
    fs.writeFileSync(archivePath, archive);
    const artifact = pinnedManifest(archive).assets['darwin-arm64'];
    await verifyArchive(archivePath, artifact);
    await assert.rejects(verifyArchive(archivePath, { ...artifact, size: artifact.size + 1 }),
      /size mismatch/);
    await assert.rejects(verifyArchive(archivePath, { ...artifact, sha256: '0'.repeat(64) }),
      /SHA-256 mismatch/);
  } finally {
    fs.rmSync(root, { recursive: true, force: true });
  }
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

test('enforces HTTPS outside loopback and rejects HTTPS downgrades', () => {
  assert.throws(() => validateDownloadURL('http://example.test/ffmpeg'), /HTTPS is required/);
  assert.throws(() => validateDownloadURL('http://127.0.0.1/file', 'https://example.test/ffmpeg'),
    /HTTPS to HTTP redirect/);
  assert.doesNotThrow(() => validateDownloadURL('http://127.0.0.1/file'));
  assert.doesNotThrow(() => validateDownloadURL('http://[::1]/file'));
});

test('enforces download size limits and the overall deadline', async () => {
  const root = tempDirectory();
  const server = http.createServer((request, response) => {
    if (request.url === '/length') {
      response.writeHead(200, { 'Content-Length': '100' });
      response.end('too large');
    } else if (request.url === '/stream') {
      response.writeHead(200);
      response.end('too large');
    } else if (request.url === '/slow') {
      response.writeHead(200);
      let writes = 0;
      const timer = setInterval(() => {
        response.write('x');
        if (++writes === 4) {
          clearInterval(timer);
          response.end();
        }
      }, 25);
      response.on('close', () => clearInterval(timer));
    } else {
      response.writeHead(404);
      response.end();
    }
  });
  try {
    await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
    const base = `http://127.0.0.1:${server.address().port}`;
    await assert.rejects(downloadFile(`${base}/length`, path.join(root, 'length'), { maxBytes: 4 }),
      /Content-Length/);
    await assert.rejects(downloadFile(`${base}/stream`, path.join(root, 'stream'), { maxBytes: 4 }),
      /byte limit/);
    assert.strictEqual(fs.existsSync(path.join(root, 'stream')), false);
    await assert.rejects(downloadFile(`${base}/slow`, path.join(root, 'slow'), { timeoutMs: 50 }),
      /overall deadline/);
    assert.strictEqual(fs.existsSync(path.join(root, 'slow')), false);
  } finally {
    await new Promise((resolve) => server.close(resolve));
    fs.rmSync(root, { recursive: true, force: true });
  }
});

test('rejects unsafe archive paths and control characters', () => {
  for (const entry of ['/absolute', '../escape', 'C:\\escape', 'line\nbreak']) {
    assert.throws(() => validateArchiveEntry(entry), /unsafe/);
  }
  assert.strictEqual(validateArchiveEntry('./bin/ffmpeg'), 'bin/ffmpeg');
});

test('enforces archive entry count and extracted size budgets', () => {
  assert.throws(() => accountArchiveEntry('large', 64 * 1024 * 1024 + 1, {
    entries: 0,
    totalBytes: 0,
  }), /size limit/);
  const many = { entries: 128, totalBytes: 0 };
  assert.throws(() => accountArchiveEntry('extra', 1, many), /entry count/);
  const large = { entries: 0, totalBytes: 256 * 1024 * 1024 };
  assert.throws(() => accountArchiveEntry('extra', 1, large), /total extracted size/);
});

test('extracts regular tar entries and removes special permission bits', async () => {
  const root = tempDirectory();
  try {
    const source = path.join(root, 'source');
    const executable = path.join(source, 'bin', 'ffmpeg');
    fs.mkdirSync(path.dirname(executable), { recursive: true });
    fs.writeFileSync(executable, 'ffmpeg');
    if (process.platform !== 'win32') fs.chmodSync(executable, 0o4755);
    const archive = path.join(root, 'safe.tar.gz');
    await tar.c({ cwd: source, gzip: true, portable: true, file: archive }, ['bin']);
    const destination = path.join(root, 'extracted');
    await extractArchive(archive, destination, 'darwin');
    assert.strictEqual(fs.readFileSync(path.join(destination, 'bin', 'ffmpeg'), 'utf8'), 'ffmpeg');
    if (process.platform !== 'win32') {
      assert.strictEqual(fs.statSync(path.join(destination, 'bin', 'ffmpeg')).mode & 0o7000, 0);
    }
  } finally {
    fs.rmSync(root, { recursive: true, force: true });
  }
});

test('rejects symbolic links in real tar archives', async () => {
  const root = tempDirectory();
  try {
    const source = path.join(root, 'source');
    fs.mkdirSync(path.join(source, 'bin'), { recursive: true });
    fs.writeFileSync(path.join(source, 'bin', 'ffmpeg'), 'ffmpeg');
    fs.symlinkSync('ffmpeg', path.join(source, 'bin', 'ffmpeg-link'));
    const archive = path.join(root, 'link.tar.gz');
    await tar.c({ cwd: source, gzip: true, portable: true, file: archive }, ['bin']);
    await assert.rejects(extractArchive(archive, path.join(root, 'extracted'), 'darwin'),
      /unsafe archive entry type/);
    fs.unlinkSync(path.join(source, 'bin', 'ffmpeg-link'));
    fs.linkSync(path.join(source, 'bin', 'ffmpeg'), path.join(source, 'bin', 'ffmpeg-hardlink'));
    const hardlinkArchive = path.join(root, 'hardlink.tar.gz');
    await tar.c({ cwd: source, gzip: true, portable: true, file: hardlinkArchive }, ['bin']);
    await assert.rejects(extractArchive(hardlinkArchive, path.join(root, 'hardlink'), 'darwin'),
      /unsafe archive entry type/);
  } finally {
    fs.rmSync(root, { recursive: true, force: true });
  }
});

test('validates real ZIP entries and rejects symlink metadata', async () => {
  const root = tempDirectory();
  try {
    const safeArchive = path.join(root, 'safe.zip');
    await createZipArchive(safeArchive, [{ name: 'bin/ffmpeg.exe', contents: 'ffmpeg' }]);
    const destination = path.join(root, 'safe');
    await extractArchive(safeArchive, destination, 'win32');
    assert.strictEqual(fs.readFileSync(path.join(destination, 'bin', 'ffmpeg.exe'), 'utf8'), 'ffmpeg');

    const linkArchive = path.join(root, 'link.zip');
    await createZipArchive(linkArchive, [{
      name: 'bin/ffmpeg-link',
      contents: 'ffmpeg.exe',
      options: { mode: 0o120777 },
    }]);
    await assert.rejects(extractArchive(linkArchive, path.join(root, 'link'), 'win32'),
      /unsafe ZIP entry type/);
  } finally {
    fs.rmSync(root, { recursive: true, force: true });
  }
});

test('installs atomically and reuses the cache', async () => {
  const root = tempDirectory();
  const archive = Buffer.from('verified archive');
  const downloads = [];
  try {
    const options = {
      platform: 'darwin',
      arch: 'arm64',
      cacheRoot: root,
      baseURL: 'https://example.test/v2.0.0',
      env: {},
      ...fakeInstaller({ archive, downloads }),
    };
    const executable = await ensureFFmpeg(options);
    assert.strictEqual(executable, path.join(root, 'v1.0.0', 'darwin-arm64', 'bin', 'ffmpeg'));
    assert.strictEqual(downloads.length, 1);
    assert.strictEqual(await ensureFFmpeg(options), executable);
    assert.strictEqual(findCachedFFmpeg(options), executable);
    assert.strictEqual(downloads.length, 1);
  } finally {
    fs.rmSync(root, { recursive: true, force: true });
  }
});

test('replaces an interrupted cache installation', async () => {
  const root = tempDirectory();
  const archive = Buffer.from('verified archive');
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
      ...fakeInstaller({ archive, downloads }),
    });
    assert.strictEqual(fs.readFileSync(executable, 'utf8'), 'fake ffmpeg');
    assert.strictEqual(downloads.length, 1);
  } finally {
    fs.rmSync(root, { recursive: true, force: true });
  }
});

test('rejects checksum mismatches and cleans the installation lock', async () => {
  const root = tempDirectory();
  const archive = Buffer.from('invalid archive!');
  const verifiedArchive = Buffer.from('verified archive');
  const downloads = [];
  try {
    const options = {
      platform: 'darwin',
      arch: 'arm64',
      cacheRoot: root,
      baseURL: 'https://example.test',
      env: {},
      ...fakeInstaller({ archive, verifiedArchive, downloads }),
    };
    await assert.rejects(ensureFFmpeg(options), /SHA-256 mismatch/);
    assert.strictEqual(fs.existsSync(path.join(root, 'v1.0.0', 'darwin-arm64', 'bin', 'ffmpeg')), false);
    assert.strictEqual(fs.existsSync(path.join(root, 'v1.0.0', 'darwin-arm64.lock')), false);
  } finally {
    fs.rmSync(root, { recursive: true, force: true });
  }
});

test('shares one installation between concurrent callers', async () => {
  const root = tempDirectory();
  const archive = Buffer.from('verified archive');
  const downloads = [];
  try {
    const options = {
      platform: 'darwin',
      arch: 'arm64',
      cacheRoot: root,
      baseURL: 'https://example.test',
      env: {},
      ...fakeInstaller({ archive, downloads, waitMs: 20 }),
    };
    const results = await Promise.all([ensureFFmpeg(options), ensureFFmpeg(options)]);
    assert.strictEqual(results[0], results[1]);
    assert.strictEqual(downloads.length, 1);
  } finally {
    fs.rmSync(root, { recursive: true, force: true });
  }
});

test('takes over a stale installation lock and releases ownership', async () => {
  const root = tempDirectory();
  const downloads = [];
  try {
    const installDir = path.join(root, 'v1.0.0', 'darwin-arm64');
    const lockPath = `${installDir}.lock`;
    fs.mkdirSync(lockPath, { recursive: true });
    const ownerPath = path.join(lockPath, 'owner');
    fs.writeFileSync(ownerPath, 'stale-owner');
    const old = new Date(Date.now() - 10000);
    fs.utimesSync(ownerPath, old, old);
    await ensureFFmpeg({
      platform: 'darwin',
      arch: 'arm64',
      cacheRoot: root,
      baseURL: 'https://example.test',
      env: {},
      lockStaleMs: 1000,
      lockHeartbeatMs: 100,
      lockTimeoutMs: 2000,
      ...fakeInstaller({ archive: Buffer.from('verified archive'), downloads }),
    });
    assert.strictEqual(downloads.length, 1);
    assert.strictEqual(fs.existsSync(lockPath), false);
  } finally {
    fs.rmSync(root, { recursive: true, force: true });
  }
});

test('heartbeats prevent a live installation from becoming stale', async () => {
  const root = tempDirectory();
  const downloads = [];
  try {
    const options = {
      platform: 'darwin',
      arch: 'arm64',
      cacheRoot: root,
      baseURL: 'https://example.test',
      env: {},
      lockStaleMs: 200,
      lockHeartbeatMs: 50,
      lockTimeoutMs: 3000,
      ...fakeInstaller({ archive: Buffer.from('verified archive'), downloads, waitMs: 600 }),
    };
    const results = await Promise.all([ensureFFmpeg(options), ensureFFmpeg(options)]);
    assert.strictEqual(results[0], results[1]);
    assert.strictEqual(downloads.length, 1);
  } finally {
    fs.rmSync(root, { recursive: true, force: true });
  }
});

test('does not publish or release a lock after ownership changes', async () => {
  const root = tempDirectory();
  const downloads = [];
  try {
    const archive = Buffer.from('verified archive');
    const installDir = path.join(root, 'v1.0.0', 'darwin-arm64');
    const lockPath = `${installDir}.lock`;
    const installer = fakeInstaller({ archive, downloads });
    const download = installer.downloadFile;
    installer.downloadFile = async (url, destination) => {
      await download(url, destination);
      fs.rmSync(lockPath, { recursive: true, force: true });
      fs.mkdirSync(lockPath);
      fs.writeFileSync(path.join(lockPath, 'owner'), 'different-owner');
    };
    await assert.rejects(ensureFFmpeg({
      platform: 'darwin',
      arch: 'arm64',
      cacheRoot: root,
      baseURL: 'https://example.test',
      env: {},
      ...installer,
    }), /no longer owned/);
    assert.strictEqual(fs.existsSync(path.join(installDir, 'bin', 'ffmpeg')), false);
    assert.strictEqual(fs.readFileSync(path.join(lockPath, 'owner'), 'utf8'), 'different-owner');
  } finally {
    fs.rmSync(root, { recursive: true, force: true });
  }
});

test('rejects custom releases before downloading', async () => {
  const root = tempDirectory();
  const downloads = [];
  try {
    await assert.rejects(ensureFFmpeg({
      platform: 'darwin',
      arch: 'arm64',
      cacheRoot: root,
      env: { PAGX_FFMPEG_RELEASE: 'v2.0.0' },
      ...fakeInstaller({ archive: Buffer.from('verified archive'), downloads }),
    }), /custom ffmpeg releases are not supported/);
    assert.strictEqual(downloads.length, 0);
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
  assert.match(message, /PAGX_FFMPEG_BASE_URL/);
  assert.match(message, /does not read HTTP_PROXY or HTTPS_PROXY/);
  assert.match(message, /FFMPEG_BIN/);
  assert.match(message, /--ffmpeg <path>/);
});

async function runTestWithTimeout(name, body) {
  let timer = null;
  try {
    await Promise.race([
      body(),
      new Promise((_, reject) => {
        timer = setTimeout(() => reject(new Error(`test timed out: ${name}`)), 15000);
      }),
    ]);
  } finally {
    clearTimeout(timer);
  }
}

(async () => {
  const generatedManifest = path.join(__dirname, '..', 'ffmpeg-manifest.json');
  const hadGeneratedManifest = fs.existsSync(generatedManifest);
  if (!hadGeneratedManifest) {
    fs.writeFileSync(generatedManifest, `${JSON.stringify(buildManifest(fakeReleaseMetadata()), null, 2)}\n`);
  }
  let failures = 0;
  try {
    for (const { name, body } of tests) {
      try {
        await runTestWithTimeout(name, body);
        console.log(`ok - ${name}`);
      } catch (error) {
        ++failures;
        console.error(`not ok - ${name}\n${error.stack}`);
      }
    }
  } finally {
    if (!hadGeneratedManifest && fs.existsSync(generatedManifest)) {
      fs.unlinkSync(generatedManifest);
    }
  }
  if (failures) process.exitCode = 1;
  else console.log(`${tests.length} ffmpeg downloader tests passed`);
})();
