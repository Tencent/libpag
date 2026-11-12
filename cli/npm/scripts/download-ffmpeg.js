#!/usr/bin/env node
'use strict';

const crypto = require('crypto');
const fs = require('fs');
const http = require('http');
const https = require('https');
const os = require('os');
const path = require('path');
const { Transform } = require('stream');
const { pipeline } = require('stream/promises');
const tar = require('tar');
const yauzl = require('yauzl');

const DEFAULT_RELEASE = 'v1.0.0';
const DOWNLOAD_TIMEOUT_MS = 120000;
const SOCKET_TIMEOUT_MS = 30000;
const MAX_DOWNLOAD_BYTES = 64 * 1024 * 1024;
const LOCK_TIMEOUT_MS = 120000;
const LOCK_STALE_MS = 300000;
const LOCK_HEARTBEAT_MS = 10000;
const MAX_ARCHIVE_ENTRIES = 128;
const MAX_ARCHIVE_ENTRY_BYTES = 64 * 1024 * 1024;
const MAX_EXTRACTED_BYTES = 256 * 1024 * 1024;
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

function isLoopbackHostname(hostname) {
  const normalized = hostname.replace(/^\[|\]$/g, '').toLowerCase();
  if (normalized === 'localhost' || normalized === '::1') return true;
  const match = /^127(?:\.(\d{1,3})){3}$/.exec(normalized);
  if (!match) return false;
  return normalized.split('.').every((part) => Number(part) >= 0 && Number(part) <= 255);
}

function validateDownloadURL(url, previousURL) {
  const parsed = new URL(url);
  const localHTTP = parsed.protocol === 'http:' && isLoopbackHostname(parsed.hostname);
  if (parsed.protocol !== 'https:' && !localHTTP) {
    throw new Error(`refusing insecure download URL; HTTPS is required outside loopback: ${url}`);
  }
  if (previousURL && new URL(previousURL).protocol === 'https:' && parsed.protocol !== 'https:') {
    throw new Error(`refusing HTTPS to HTTP redirect: ${previousURL} -> ${url}`);
  }
  return parsed;
}

function requestResponse(parsed, deadline) {
  return new Promise((resolve, reject) => {
    const remaining = deadline - Date.now();
    if (remaining <= 0) {
      reject(new Error(`download exceeded its overall deadline: ${parsed.toString()}`));
      return;
    }
    let deadlineError = null;
    const transport = parsed.protocol === 'https:' ? https : http;
    const request = transport.get(parsed, {
      headers: { 'User-Agent': 'pagx-ffmpeg-downloader' },
    }, (response) => resolve({
      response,
      cancel: () => clearTimeout(deadlineTimer),
      getDeadlineError: () => deadlineError,
    }));
    const deadlineTimer = setTimeout(() => {
      deadlineError = new Error(`download exceeded its overall deadline: ${parsed.toString()}`);
      request.destroy(deadlineError);
    }, remaining);
    request.setTimeout(Math.min(SOCKET_TIMEOUT_MS, remaining), () => {
      request.destroy(new Error(`download timed out while idle: ${parsed.toString()}`));
    });
    request.on('error', (error) => {
      clearTimeout(deadlineTimer);
      reject(error);
    });
  });
}

function createByteLimiter(maxBytes, url) {
  let received = 0;
  return new Transform({
    transform(chunk, _encoding, callback) {
      received += chunk.length;
      if (received > maxBytes) {
        callback(new Error(`download exceeds the ${maxBytes}-byte limit: ${url}`));
        return;
      }
      callback(null, chunk);
    },
  });
}

async function downloadFile(url, destination, options = {}) {
  const redirects = options.redirects == null ? 5 : options.redirects;
  const deadline = options.deadline || Date.now() + (options.timeoutMs || DOWNLOAD_TIMEOUT_MS);
  const maxBytes = options.maxBytes == null ? MAX_DOWNLOAD_BYTES : options.maxBytes;
  const parsed = validateDownloadURL(url, options.previousURL);
  const { response, cancel, getDeadlineError } = await requestResponse(parsed, deadline);
  try {
    const status = response.statusCode || 0;
    if (status >= 300 && status < 400 && response.headers.location) {
      response.destroy();
      if (redirects <= 0) throw new Error(`too many redirects while downloading ${url}`);
      const nextURL = new URL(response.headers.location, url).toString();
      validateDownloadURL(nextURL, url);
      return downloadFile(nextURL, destination, {
        ...options,
        redirects: redirects - 1,
        deadline,
        previousURL: url,
      });
    }
    if (status !== 200) {
      response.destroy();
      throw new Error(`download returned HTTP ${status}: ${url}`);
    }
    const contentLength = Number(response.headers['content-length']);
    if (Number.isFinite(contentLength) && contentLength > maxBytes) {
      response.destroy();
      throw new Error(`download exceeds the ${maxBytes}-byte Content-Length limit: ${url}`);
    }
    try {
      await pipeline(
        response,
        createByteLimiter(maxBytes, url),
        fs.createWriteStream(destination, { flags: 'wx' }),
      );
    } catch (error) {
      fs.rmSync(destination, { force: true });
      const deadlineError = getDeadlineError();
      if (deadlineError) throw deadlineError;
      throw new Error(`failed to save ${url}: ${error.message}`);
    }
  } finally {
    cancel();
  }
}

async function verifyArchive(archivePath, artifact) {
  const actualSize = fs.statSync(archivePath).size;
  if (actualSize !== artifact.size) {
    throw new Error(`size mismatch for ${artifact.name}: expected ${artifact.size}, got ${actualSize}`);
  }
  const hash = crypto.createHash('sha256');
  await pipeline(fs.createReadStream(archivePath), hash);
  const actual = hash.digest('hex');
  if (actual !== artifact.sha256) {
    throw new Error(`SHA-256 mismatch for ${artifact.name}: expected ${artifact.sha256}, got ${actual}`);
  }
}

function validateArchiveEntry(entry) {
  if (typeof entry !== 'string' || /[\0-\x1f\x7f]/.test(entry)) {
    throw new Error(`unsafe control character in archive entry: ${entry}`);
  }
  const normalized = entry.replace(/\\/g, '/');
  if (path.posix.isAbsolute(normalized) || /^[A-Za-z]:/.test(normalized)) {
    throw new Error(`unsafe absolute archive entry: ${entry}`);
  }
  if (normalized.split('/').includes('..')) throw new Error(`unsafe archive entry: ${entry}`);
  const clean = path.posix.normalize(normalized).replace(/\/+$/, '');
  return clean === '.' ? '' : clean;
}

function resolveArchivePath(destination, entry) {
  const normalized = validateArchiveEntry(entry);
  const root = path.resolve(destination);
  const target = normalized ? path.resolve(root, ...normalized.split('/')) : root;
  if (target !== root && !target.startsWith(`${root}${path.sep}`)) {
    throw new Error(`archive entry escapes the extraction directory: ${entry}`);
  }
  return target;
}

function accountArchiveEntry(entry, size, budget) {
  const normalized = validateArchiveEntry(entry);
  if (!normalized) return normalized;
  if (!Number.isSafeInteger(size) || size < 0 || size > MAX_ARCHIVE_ENTRY_BYTES) {
    throw new Error(`archive entry exceeds the size limit: ${entry}`);
  }
  budget.entries += 1;
  budget.totalBytes += size;
  if (budget.entries > MAX_ARCHIVE_ENTRIES || budget.totalBytes > MAX_EXTRACTED_BYTES) {
    throw new Error('archive exceeds the entry count or total extracted size limit');
  }
  return normalized;
}

function sanitizeExtractedTree(destination) {
  const stat = fs.lstatSync(destination);
  if (stat.isSymbolicLink() || (!stat.isFile() && !stat.isDirectory())) {
    throw new Error(`unsafe extracted file type: ${destination}`);
  }
  if (process.platform !== 'win32') fs.chmodSync(destination, stat.isDirectory() ? 0o755 : 0o644);
  if (!stat.isDirectory()) return;
  for (const entry of fs.readdirSync(destination)) {
    sanitizeExtractedTree(path.join(destination, entry));
  }
}

async function extractTarArchive(archivePath, destination) {
  const budget = { entries: 0, totalBytes: 0 };
  let failure = null;
  await tar.x({
    file: archivePath,
    cwd: destination,
    strict: true,
    preservePaths: false,
    preserveOwner: false,
    noMtime: true,
    filter: (entryPath, entry) => {
      if (failure) return false;
      try {
        if (entry.type !== 'File' && entry.type !== 'Directory') {
          throw new Error(`unsafe archive entry type ${entry.type}: ${entryPath}`);
        }
        accountArchiveEntry(entryPath, entry.size, budget);
        return true;
      } catch (error) {
        failure = error;
        return false;
      }
    },
  });
  if (failure) throw failure;
}

function openZipEntry(zip, entry) {
  return new Promise((resolve, reject) => {
    zip.openReadStream(entry, (error, stream) => {
      if (error) reject(error);
      else resolve(stream);
    });
  });
}

async function extractZipEntry(zip, entry, destination, budget) {
  const unixMode = (entry.externalFileAttributes >>> 16) & 0xffff;
  const fileType = unixMode & 0xf000;
  const isDirectory = entry.fileName.endsWith('/') || fileType === 0x4000;
  if (fileType !== 0 && fileType !== 0x4000 && fileType !== 0x8000) {
    throw new Error(`unsafe ZIP entry type: ${entry.fileName}`);
  }
  const normalized = accountArchiveEntry(entry.fileName, entry.uncompressedSize, budget);
  const target = resolveArchivePath(destination, entry.fileName);
  if (!normalized) return;
  if (isDirectory) {
    fs.mkdirSync(target, { recursive: true, mode: 0o755 });
    return;
  }
  fs.mkdirSync(path.dirname(target), { recursive: true, mode: 0o755 });
  const input = await openZipEntry(zip, entry);
  await pipeline(input, fs.createWriteStream(target, { flags: 'wx', mode: 0o644 }));
}

async function extractZipArchive(archivePath, destination) {
  const zip = await new Promise((resolve, reject) => {
    yauzl.open(archivePath, {
      lazyEntries: true,
      decodeStrings: true,
      validateEntrySizes: true,
    }, (error, value) => {
      if (error) reject(error);
      else resolve(value);
    });
  });
  const budget = { entries: 0, totalBytes: 0 };
  try {
    await new Promise((resolve, reject) => {
      zip.on('entry', (entry) => {
        extractZipEntry(zip, entry, destination, budget)
          .then(() => zip.readEntry())
          .catch(reject);
      });
      zip.on('end', resolve);
      zip.on('error', reject);
      zip.readEntry();
    });
  } finally {
    zip.close();
  }
}

async function extractArchive(archivePath, destination, platform = process.platform) {
  fs.mkdirSync(destination, { recursive: true, mode: 0o755 });
  if (platform === 'win32') await extractZipArchive(archivePath, destination);
  else await extractTarArchive(archivePath, destination);
  sanitizeExtractedTree(destination);
}

function lockOwnerPath(lockPath) {
  return path.join(lockPath, 'owner');
}

function readLockOwner(lockPath) {
  const ownerPath = lockOwnerPath(lockPath);
  const stat = fs.statSync(ownerPath);
  return {
    token: fs.readFileSync(ownerPath, 'utf8'),
    dev: stat.dev,
    ino: stat.ino,
    mtimeMs: stat.mtimeMs,
  };
}

function matchesLockOwner(lockPath, expected) {
  try {
    const actual = readLockOwner(lockPath);
    return actual.token === expected.token && actual.dev === expected.dev && actual.ino === expected.ino;
  } catch {
    return false;
  }
}

function ownsInstallLock(lock) {
  try {
    const stat = fs.fstatSync(lock.handle);
    return matchesLockOwner(lock.path, { token: lock.token, dev: stat.dev, ino: stat.ino });
  } catch {
    return false;
  }
}

function assertInstallLockOwner(lock) {
  if (!ownsInstallLock(lock)) throw new Error('the ffmpeg installation lock is no longer owned by this process');
}

function createInstallLock(lockPath, update) {
  const token = crypto.randomBytes(16).toString('hex');
  let created = false;
  let handle = null;
  try {
    fs.mkdirSync(lockPath, { mode: 0o700 });
    created = true;
    handle = fs.openSync(lockOwnerPath(lockPath), 'wx', 0o600);
    fs.writeFileSync(handle, token, 'utf8');
    const lock = { path: lockPath, token, handle, heartbeat: null };
    lock.heartbeat = setInterval(() => {
      try {
        const now = new Date();
        fs.futimesSync(handle, now, now);
      } catch {
        // Publishing verifies the owner again, so a failed heartbeat cannot authorize a publish.
      }
    }, Math.max(1, update));
    lock.heartbeat.unref();
    return lock;
  } catch (error) {
    if (handle !== null) fs.closeSync(handle);
    if (created) fs.rmSync(lockPath, { recursive: true, force: true });
    throw error;
  }
}

function takeStaleInstallLock(lockPath, stale) {
  let expected = null;
  try {
    expected = readLockOwner(lockPath);
  } catch (error) {
    if (error.code === 'ENOENT') return;
    throw error;
  }
  if (Date.now() - expected.mtimeMs <= stale) return;
  const quarantine = `${lockPath}.stale-${crypto.randomBytes(8).toString('hex')}`;
  try {
    fs.renameSync(lockPath, quarantine);
  } catch (error) {
    if (error.code === 'ENOENT') return;
    throw error;
  }
  if (matchesLockOwner(quarantine, expected)) {
    fs.rmSync(quarantine, { recursive: true, force: true });
    return;
  }
  try {
    if (!fs.existsSync(lockPath)) fs.renameSync(quarantine, lockPath);
  } catch {
    // A different lock must never be deleted; leave the quarantined directory for diagnosis.
  }
}

function releaseInstallLock(lock) {
  clearInterval(lock.heartbeat);
  const releasePath = `${lock.path}.release-${lock.token}`;
  try {
    assertInstallLockOwner(lock);
    fs.renameSync(lock.path, releasePath);
    if (!ownsInstallLock({ ...lock, path: releasePath })) {
      try {
        if (!fs.existsSync(lock.path)) fs.renameSync(releasePath, lock.path);
      } catch {
        // Do not delete a lock whose ownership changed during release.
      }
      throw new Error('the ffmpeg installation lock changed ownership during release');
    }
    fs.rmSync(releasePath, { recursive: true, force: true });
  } finally {
    fs.closeSync(lock.handle);
  }
}

function delay(milliseconds) {
  return new Promise((resolve) => setTimeout(resolve, milliseconds));
}

async function acquireInstallLock(installDir, executablePath, platform, options) {
  const timeout = options.lockTimeoutMs == null ? LOCK_TIMEOUT_MS : options.lockTimeoutMs;
  const stale = options.lockStaleMs == null ? LOCK_STALE_MS : options.lockStaleMs;
  const update = options.lockHeartbeatMs == null ? LOCK_HEARTBEAT_MS : options.lockHeartbeatMs;
  const lockPath = `${installDir}.lock`;
  const deadline = Date.now() + timeout;
  while (Date.now() < deadline) {
    if (isExecutable(executablePath, platform)) return null;
    try {
      return createInstallLock(lockPath, update);
    } catch (error) {
      if (error.code !== 'EEXIST') throw error;
    }
    if (isExecutable(executablePath, platform)) return null;
    takeStaleInstallLock(lockPath, stale);
    await delay(Math.max(1, Math.min(250, deadline - Date.now())));
  }
  if (isExecutable(executablePath, platform)) return null;
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

  fs.mkdirSync(path.dirname(installDir), { recursive: true });
  const releaseLock = await acquireInstallLock(installDir, executablePath, target.platform, options);
  if (!releaseLock) return executablePath;

  let stagingDir = null;
  let result = null;
  let failure = null;
  try {
    if (isExecutable(executablePath, target.platform)) {
      result = executablePath;
    } else {
      stagingDir = fs.mkdtempSync(path.join(cacheRoot, `.${target.name}-`));
      const archivePath = path.join(stagingDir, artifact.name);
      const extractDir = path.join(stagingDir, 'extracted');
      const baseURL = options.baseURL || releaseBaseURL(env);
      const download = options.downloadFile || downloadFile;
      await download(`${baseURL}/${artifact.name}`, archivePath, { maxBytes: artifact.size });
      await verifyArchive(archivePath, artifact);
      const extract = options.extractArchive || extractArchive;
      await extract(archivePath, extractDir, target.platform);
      const extractedExecutable = path.join(extractDir, 'bin', target.executableName);
      if (!isExecutable(extractedExecutable, target.platform)) {
        throw new Error(`the downloaded archive does not contain bin/${target.executableName}`);
      }
      if (target.platform !== 'win32') fs.chmodSync(extractedExecutable, 0o755);
      assertInstallLockOwner(releaseLock);
      if (fs.existsSync(installDir)) fs.rmSync(installDir, { recursive: true, force: true });
      fs.renameSync(extractDir, installDir);
      if (!isExecutable(executablePath, target.platform)) {
        throw new Error('ffmpeg installation did not produce an executable');
      }
      result = executablePath;
    }
  } catch (error) {
    error.message = `failed to install ffmpeg for ${target.name} (${release}): ${error.message}`;
    failure = error;
  }
  try {
    if (stagingDir) fs.rmSync(stagingDir, { recursive: true, force: true });
  } catch (error) {
    if (!failure) failure = error;
  }
  try {
    releaseInstallLock(releaseLock);
  } catch (error) {
    if (!failure) failure = error;
  }
  if (failure) throw failure;
  return result;
}

function formatDownloadError(error) {
  return `pagx: ${error.message}\n` +
    'Check your network, then retry or set PAGX_FFMPEG_BASE_URL to a mirror. The downloader does ' +
    'not read HTTP_PROXY or HTTPS_PROXY. To use an existing installation, set FFMPEG_BIN to the ' +
    'absolute ffmpeg path or pass --ffmpeg <path>.';
}

module.exports = {
  DEFAULT_RELEASE,
  accountArchiveEntry,
  downloadFile,
  ensureFFmpeg,
  extractArchive,
  findCachedFFmpeg,
  formatDownloadError,
  isExecutable,
  resolveTarget,
  validateArchiveEntry,
  validateDownloadURL,
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
