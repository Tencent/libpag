#!/usr/bin/env node
'use strict';

const assert = require('assert');
const crypto = require('crypto');
const fs = require('fs');
const http = require('http');
const { spawnSync } = require('child_process');
const os = require('os');
const path = require('path');
const { buildManifest } = require('./generate-ffmpeg-manifest.js');
const {
  downloadFile,
  ensureFFmpeg,
  findCachedFFmpeg,
  findFFmpegOnPath,
  formatDownloadError,
  hasExplicitFFmpeg,
  isMP4Export,
  resolveFFmpegForCLI,
  resolveTarget,
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

test('validates archive size and hash against the pinned manifest', () => {
  const root = tempDirectory();
  try {
    const archive = Buffer.from('verified archive');
    const archivePath = path.join(root, 'archive');
    fs.writeFileSync(archivePath, archive);
    const artifact = pinnedManifest(archive).assets['darwin-arm64'];
    verifyArchive(archivePath, artifact);
    assert.throws(() => verifyArchive(archivePath, { ...artifact, size: artifact.size + 1 }),
      /size mismatch/);
    assert.throws(() => verifyArchive(archivePath, { ...artifact, sha256: '0'.repeat(64) }),
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
    assert.strictEqual(fs.existsSync(path.join(root, 'v1.0.0', 'darwin-arm64.installing')), false);
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
        await body();
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
