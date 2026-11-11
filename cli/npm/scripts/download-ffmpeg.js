#!/usr/bin/env node
'use strict';

const crypto = require('crypto');
const fs = require('fs');
const http = require('http');
const https = require('https');
const os = require('os');
const path = require('path');
const { execFile } = require('child_process');
const { pipeline } = require('stream/promises');

const DEFAULT_RELEASE = 'v1.0.0';
const DOWNLOAD_TIMEOUT_MS = 30000;
const LOCK_TIMEOUT_MS = 120000;
const SUPPORTED_TARGETS = new Set([
  'darwin-x64',
  'darwin-arm64',
  'linux-x64',
  'linux-arm64',
  'win32-x64',
  'win32-arm64',
]);

function resolveTarget(platform = process.platform, arch = process.arch) {
  const target = `${platform}-${arch}`;
  if (!SUPPORTED_TARGETS.has(target)) {
    throw new Error(`ffmpeg is not available for ${target}; supported targets: ${Array.from(SUPPORTED_TARGETS).join(', ')}`);
  }
  const builderPlatform = platform === 'darwin' ? 'mac' : platform === 'win32' ? 'win' : 'linux';
  return {
    name: target,
    assetName: `ffmpeg-${builderPlatform}-${arch}.${platform === 'win32' ? 'zip' : 'tar.gz'}`,
    executableName: platform === 'win32' ? 'ffmpeg.exe' : 'ffmpeg',
    platform,
  };
}

function resolvePinnedArtifact(target, manifest) {
  const artifact = manifest.assets && manifest.assets[target.name];
  if (!artifact || artifact.name !== target.assetName ||
      !Number.isSafeInteger(artifact.size) || artifact.size < 0 ||
      !/^[0-9a-f]{64}$/.test(artifact.sha256)) {
    throw new Error(`the pinned ffmpeg manifest has no valid entry for ${target.name}`);
  }
  return artifact;
}

function loadPackagedManifest() {
  const manifestPath = path.join(__dirname, '..', 'ffmpeg-manifest.json');
  try {
    return JSON.parse(fs.readFileSync(manifestPath, 'utf8'));
  } catch (error) {
    if (error.code === 'ENOENT') {
      throw new Error('the generated ffmpeg manifest is missing; run npm run generate:ffmpeg-manifest before packaging');
    }
    throw new Error(`failed to load the generated ffmpeg manifest: ${error.message}`);
  }
}

function defaultCacheRoot(env = process.env) {
  if (env.PAGX_FFMPEG_CACHE_DIR) return path.resolve(env.PAGX_FFMPEG_CACHE_DIR);
  if (process.platform === 'win32') {
    return path.join(env.LOCALAPPDATA || path.join(os.homedir(), 'AppData', 'Local'), 'pagx', 'ffmpeg');
  }
  if (process.platform === 'darwin') return path.join(os.homedir(), 'Library', 'Caches', 'pagx', 'ffmpeg');
  return path.join(env.XDG_CACHE_HOME || path.join(os.homedir(), '.cache'), 'pagx', 'ffmpeg');
}

function isExecutable(filePath, platform = process.platform) {
  try {
    const stat = fs.statSync(filePath);
    if (!stat.isFile()) return false;
    if (platform !== 'win32') fs.accessSync(filePath, fs.constants.X_OK);
    return true;
  } catch {
    return false;
  }
}

function findFFmpegOnPath(env = process.env, platform = process.platform) {
  const pathValue = env.PATH || '';
  const names = platform === 'win32' ? ['ffmpeg.exe', 'ffmpeg'] : ['ffmpeg'];
  for (const directory of pathValue.split(platform === 'win32' ? ';' : ':')) {
    if (!directory) continue;
    for (const name of names) {
      const candidate = path.join(directory, name);
      if (isExecutable(candidate, platform)) return candidate;
    }
  }
  return null;
}

function hasExplicitFFmpeg(args, env = process.env) {
  return Boolean(env.FFMPEG_BIN) || args.some((arg) => arg === '--ffmpeg' || arg.startsWith('--ffmpeg='));
}

function isMP4Export(args) {
  if (args[0] !== 'export' || args.includes('--help') || args.includes('-h')) return false;
  let format = null;
  let output = null;
  for (let i = 1; i < args.length; ++i) {
    if (args[i] === '--format') format = args[++i];
    else if (args[i].startsWith('--format=')) format = args[i].slice('--format='.length);
    else if (args[i] === '--output') output = args[++i];
    else if (args[i].startsWith('--output=')) output = args[i].slice('--output='.length);
  }
  return format ? format === 'mp4' : Boolean(output) && path.extname(output).toLowerCase() === '.mp4';
}

function resolveFFmpegForCLI(args, options = {}) {
  const env = options.env || process.env;
  if (hasExplicitFFmpeg(args, env)) return null;
  return findCachedFFmpeg(options) || findFFmpegOnPath(env, options.platform || process.platform);
}

function findCachedFFmpeg(options = {}) {
  try {
    const target = resolveTarget(options.platform, options.arch);
    const env = options.env || process.env;
    const cacheRoot = options.cacheRoot || defaultCacheRoot(env);
    const executable = path.join(cacheRoot, DEFAULT_RELEASE, target.name, 'bin', target.executableName);
    return isExecutable(executable, target.platform) ? executable : null;
  } catch {
    return null;
  }
}

function releaseBaseURL(env = process.env) {
  if (env.PAGX_FFMPEG_BASE_URL) return env.PAGX_FFMPEG_BASE_URL.replace(/\/$/, '');
  return `https://github.com/libpag/ffmpeg-builder/releases/download/${encodeURIComponent(DEFAULT_RELEASE)}`;
}

function requestResponse(url, redirects, timeoutMs) {
  return new Promise((resolve, reject) => {
    let parsed;
    try {
      parsed = new URL(url);
    } catch (error) {
      reject(error);
      return;
    }
    const transport = parsed.protocol === 'https:' ? https : parsed.protocol === 'http:' ? http : null;
    if (!transport) {
      reject(new Error(`unsupported download protocol: ${parsed.protocol}`));
      return;
    }
    const request = transport.get(parsed, { headers: { 'User-Agent': 'pagx-ffmpeg-downloader' } }, resolve);
    request.setTimeout(timeoutMs, () => request.destroy(new Error(`download timed out: ${url}`)));
    request.on('error', reject);
  });
}

async function downloadFile(url, destination, options = {}) {
  const redirects = options.redirects == null ? 5 : options.redirects;
  const timeoutMs = options.timeoutMs || DOWNLOAD_TIMEOUT_MS;
  const response = await requestResponse(url, redirects, timeoutMs);
  const status = response.statusCode || 0;
  if (status >= 300 && status < 400 && response.headers.location) {
    response.resume();
    if (redirects <= 0) throw new Error(`too many redirects while downloading ${url}`);
    return downloadFile(new URL(response.headers.location, url).toString(), destination, {
      redirects: redirects - 1,
      timeoutMs,
    });
  }
  if (status !== 200) {
    response.resume();
    throw new Error(`download returned HTTP ${status}: ${url}`);
  }
  try {
    await pipeline(response, fs.createWriteStream(destination, { flags: 'wx' }));
  } catch (error) {
    fs.rmSync(destination, { force: true });
    throw new Error(`failed to save ${url}: ${error.message}`);
  }
}

function verifyArchive(archivePath, artifact) {
  const actualSize = fs.statSync(archivePath).size;
  if (actualSize !== artifact.size) {
    throw new Error(`size mismatch for ${artifact.name}: expected ${artifact.size}, got ${actualSize}`);
  }
  const actual = crypto.createHash('sha256').update(fs.readFileSync(archivePath)).digest('hex');
  if (actual !== artifact.sha256) {
    throw new Error(`SHA-256 mismatch for ${artifact.name}: expected ${artifact.sha256}, got ${actual}`);
  }
}

function run(command, args) {
  return new Promise((resolve, reject) => {
    execFile(command, args, { windowsHide: true }, (error, stdout, stderr) => {
      if (error) reject(new Error(stderr.trim() || error.message));
      else resolve(stdout);
    });
  });
}

function validateArchiveEntry(entry) {
  const normalized = entry.replace(/\\/g, '/');
  if (path.posix.isAbsolute(normalized) || /^[A-Za-z]:/.test(normalized)) {
    throw new Error(`unsafe absolute archive entry: ${entry}`);
  }
  if (normalized.split('/').includes('..')) throw new Error(`unsafe archive entry: ${entry}`);
}

async function extractArchive(archivePath, destination, platform = process.platform) {
  fs.mkdirSync(destination, { recursive: true });
  const escapedSource = archivePath.replace(/'/g, "''");
  const listing = platform === 'win32'
    ? await run('powershell.exe', [
        '-NoProfile',
        '-NonInteractive',
        '-Command',
        `Add-Type -AssemblyName System.IO.Compression.FileSystem; $z = [System.IO.Compression.ZipFile]::OpenRead('${escapedSource}'); $z.Entries | ForEach-Object { $_.FullName }; $z.Dispose()`,
      ])
    : await run('tar', ['-tf', archivePath]);
  for (const entry of listing.split(/\r?\n/)) {
    if (entry.trim()) validateArchiveEntry(entry.trim());
  }
  if (platform === 'win32') {
    const escapedDestination = destination.replace(/'/g, "''");
    await run('powershell.exe', [
      '-NoProfile',
      '-NonInteractive',
      '-Command',
      `Expand-Archive -LiteralPath '${escapedSource}' -DestinationPath '${escapedDestination}' -Force`,
    ]);
  } else {
    await run('tar', ['-xpf', archivePath, '-C', destination]);
  }
}

function delay(milliseconds) {
  return new Promise((resolve) => setTimeout(resolve, milliseconds));
}

async function acquireInstallLock(lockPath, executablePath, platform, options) {
  const deadline = Date.now() + (options.lockTimeoutMs || LOCK_TIMEOUT_MS);
  const sleep = options.sleep || delay;
  while (Date.now() < deadline) {
    if (isExecutable(executablePath, platform)) return false;
    try {
      const handle = fs.openSync(lockPath, 'wx');
      fs.closeSync(handle);
      return true;
    } catch (error) {
      if (error.code !== 'EEXIST') throw error;
      if (isExecutable(executablePath, platform)) return false;
      try {
        if (Date.now() - fs.statSync(lockPath).mtimeMs > (options.lockTimeoutMs || LOCK_TIMEOUT_MS)) {
          fs.rmSync(lockPath, { force: true });
        }
      } catch (statError) {
        if (statError.code !== 'ENOENT') throw statError;
      }
      await sleep(250);
    }
  }
  throw new Error('timed out waiting for another ffmpeg installation to finish');
}

async function ensureFFmpeg(options = {}) {
  const target = resolveTarget(options.platform, options.arch);
  const env = options.env || process.env;
  const manifest = options.manifest || loadPackagedManifest();
  const release = manifest.release;
  if (env.PAGX_FFMPEG_RELEASE && env.PAGX_FFMPEG_RELEASE !== release) {
    throw new Error(`custom ffmpeg releases are not supported; use FFMPEG_BIN or --ffmpeg to select a local build (requested ${env.PAGX_FFMPEG_RELEASE}, packaged ${release})`);
  }
  const artifact = resolvePinnedArtifact(target, manifest);
  const cacheRoot = options.cacheRoot || defaultCacheRoot(env);
  const installDir = path.join(cacheRoot, release, target.name);
  const executablePath = path.join(installDir, 'bin', target.executableName);
  if (isExecutable(executablePath, target.platform)) return executablePath;

  fs.mkdirSync(cacheRoot, { recursive: true });
  const lockPath = `${installDir}.installing`;
  fs.mkdirSync(path.dirname(lockPath), { recursive: true });
  const ownsLock = await acquireInstallLock(lockPath, executablePath, target.platform, options);
  if (!ownsLock) return executablePath;

  let stagingDir = null;
  try {
    if (isExecutable(executablePath, target.platform)) return executablePath;
    fs.mkdirSync(path.dirname(installDir), { recursive: true });
    stagingDir = fs.mkdtempSync(path.join(cacheRoot, `.${target.name}-`));
    const archivePath = path.join(stagingDir, artifact.name);
    const extractDir = path.join(stagingDir, 'extracted');
    const baseURL = options.baseURL || releaseBaseURL(env);
    const download = options.downloadFile || downloadFile;
    await download(`${baseURL}/${artifact.name}`, archivePath);
    verifyArchive(archivePath, artifact);
    const extract = options.extractArchive || extractArchive;
    await extract(archivePath, extractDir, target.platform);
    const extractedExecutable = path.join(extractDir, 'bin', target.executableName);
    if (!isExecutable(extractedExecutable, target.platform)) {
      throw new Error(`the downloaded archive does not contain bin/${target.executableName}`);
    }
    if (target.platform !== 'win32') fs.chmodSync(extractedExecutable, 0o755);
    // A previous interrupted installation may have left a directory without a usable executable.
    if (fs.existsSync(installDir)) fs.rmSync(installDir, { recursive: true, force: true });
    fs.renameSync(extractDir, installDir);
    if (!isExecutable(executablePath, target.platform)) {
      throw new Error('ffmpeg installation did not produce an executable');
    }
    return executablePath;
  } catch (error) {
    error.message = `failed to install ffmpeg for ${target.name} (${release}): ${error.message}`;
    throw error;
  } finally {
    if (stagingDir) fs.rmSync(stagingDir, { recursive: true, force: true });
    fs.rmSync(lockPath, { force: true });
  }
}

function formatDownloadError(error) {
  return `pagx: ${error.message}\n` +
    'Check your network, then retry or set PAGX_FFMPEG_BASE_URL to a mirror. The downloader does ' +
    'not read HTTP_PROXY or HTTPS_PROXY. To use an existing installation, set FFMPEG_BIN to the ' +
    'absolute ffmpeg path or pass --ffmpeg <path>.';
}

module.exports = {
  DEFAULT_RELEASE,
  downloadFile,
  ensureFFmpeg,
  findCachedFFmpeg,
  findFFmpegOnPath,
  formatDownloadError,
  hasExplicitFFmpeg,
  isExecutable,
  isMP4Export,
  resolveFFmpegForCLI,
  resolveTarget,
  verifyArchive,
};

if (require.main === module) {
  const postinstall = process.argv.includes('--postinstall');
  if (postinstall && process.env.PAGX_SKIP_FFMPEG_DOWNLOAD === '1') {
    console.log('pagx: skipping the optional ffmpeg download (PAGX_SKIP_FFMPEG_DOWNLOAD=1)');
  } else {
    ensureFFmpeg()
      .then((executablePath) => {
        console.log(postinstall ? `pagx: ffmpeg is ready at ${executablePath}` : executablePath);
      })
      .catch((error) => {
        if (postinstall) {
          console.warn(`pagx: optional ffmpeg download failed; MP4 export is unavailable.\n${formatDownloadError(error)}\n` +
            'Retry with: npm run download:ffmpeg --prefix "$(npm root -g)/@libpag/pagx"');
          return;
        }
        console.error(formatDownloadError(error));
        process.exitCode = 1;
      });
  }
}
