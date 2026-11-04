// Copyright (C) 2026 Tencent. All rights reserved.
const crypto = require('node:crypto');
const fs = require('node:fs');
const path = require('node:path');

const Backends = ['webgl', 'webgpu'];
const Modes = ['compare', 'update'];
const ArtifactNames = ['pag-test.js', 'pag-test.wasm', 'pag-test.data', 'tgfx.esm.js',
  'ffavc.min.js', 'ffavc.wasm', 'index.html'];

function ValidateBuildInfo(info, backend, mode) {
  if (!info || info.protocol !== 1 || !Backends.includes(backend) || !Modes.includes(mode) ||
      info.backend !== backend || info.mode !== mode || !/^[a-f0-9]{32}$/.test(info.buildId) ||
      !/^[a-f0-9]{40}$/.test(info.sourceRevision) || !/^[a-f0-9]{40}$/.test(info.tgfxRevision)) {
    throw new Error('Build identity does not match the requested backend or mode. Rebuild the tests.');
  }
}

function HashFile(file) {
  const hash = crypto.createHash('sha256');
  const buffer = Buffer.alloc(1024 * 1024);
  const fd = fs.openSync(file, 'r');
  try {
    let count = 0;
    while ((count = fs.readSync(fd, buffer, 0, buffer.length, null)) !== 0) {
      hash.update(buffer.subarray(0, count));
    }
  } finally {
    fs.closeSync(fd);
  }
  return hash.digest('hex');
}

function ReadBuild(directory, backend, mode) {
  const manifest = JSON.parse(fs.readFileSync(path.join(directory, 'build-info.json'), 'utf8'));
  ValidateBuildInfo(manifest, backend, mode);
  for (const name of ArtifactNames) {
    if (manifest.files?.[name] !== HashFile(path.join(directory, name))) {
      throw new Error(`Artifact hash mismatch: ${name}`);
    }
  }
  return manifest;
}

function HashText(text) {
  return crypto.createHash('sha256').update(text).digest('hex');
}

function SnapshotBuild(source, destination, backend, mode) {
  fs.mkdirSync(destination);
  for (const name of [...ArtifactNames, 'build-info.json']) {
    fs.copyFileSync(path.join(source, name), path.join(destination, name));
  }
  return ReadBuild(destination, backend, mode);
}

function RemoveBackup(backup) {
  try {
    fs.rmSync(backup, { recursive: true, force: true });
  } catch (error) {
    console.warn(`Commit succeeded; backup retained at ${backup}: ${error.message}`);
  }
}

// Atomically moves a fully written staging directory into place, restoring the previous
// destination on failure. The caller retains ownership of stage cleanup when this throws.
function ReplaceDirectory(stage, destination) {
  const backup = `${stage}.previous`;
  if (fs.existsSync(destination)) fs.renameSync(destination, backup);
  try {
    fs.renameSync(stage, destination);
  } catch (error) {
    if (fs.existsSync(backup)) fs.renameSync(backup, destination);
    throw error;
  }
  RemoveBackup(backup);
}

function Publish(backend, mode, buildDirectory, webDirectory = __dirname) {
  const info = JSON.parse(fs.readFileSync(path.join(buildDirectory, 'build-info.json'), 'utf8'));
  ValidateBuildInfo(info, backend, mode);
  const root = path.join(webDirectory, 'dist');
  fs.mkdirSync(root, { recursive: true });
  const stage = fs.mkdtempSync(path.join(root, '.publish-'));
  const destination = path.join(root, `${backend}-${mode}`);
  try {
    const binding = fs.readFileSync(path.join(buildDirectory, 'tgfx-binding-path.txt'), 'utf8').trim();
    if (path.basename(binding) !== 'tgfx.esm.js') throw new Error('Invalid TGFX binding path');
    for (const name of ArtifactNames) {
      const source = name === 'tgfx.esm.js' ? binding :
        path.join(name.startsWith('pag-test.') ? buildDirectory : webDirectory, name);
      fs.copyFileSync(source, path.join(stage, name));
    }
    info.files = {};
    for (const name of ArtifactNames) {
      info.files[name] = HashFile(path.join(stage, name));
    }
    fs.writeFileSync(path.join(stage, 'build-info.json'), JSON.stringify(info, null, 2) + '\n');
    ReplaceDirectory(stage, destination);
  } finally {
    fs.rmSync(stage, { recursive: true, force: true });
  }
  return destination;
}

function FlattenStrings(value, prefix = '', result = new Map()) {
  if (!value || typeof value !== 'object' || Array.isArray(value)) {
    throw new Error(`Invalid baseline object: ${prefix}`);
  }
  for (const [key, child] of Object.entries(value)) {
    if (!key || key === '__proto__' || key === 'constructor' || key === 'prototype') {
      throw new Error('Invalid baseline key');
    }
    const name = prefix ? `${prefix}/${key}` : key;
    if (typeof child === 'string' && child.length > 0) result.set(name, child);
    else FlattenStrings(child, name, result);
  }
  return result;
}

function ValidateCache(cache) {
  const versions = FlattenStrings(JSON.parse(cache.version));
  const md5 = FlattenStrings(JSON.parse(cache.md5));
  if (!versions.size || !md5.size) throw new Error('Baseline cache is empty');
  for (const [key, digest] of md5) {
    if (!/^[a-f0-9]{32}$/i.test(digest) || !versions.has(key)) {
      throw new Error(`Incomplete baseline cache: ${key}`);
    }
  }
}

function ValidateResult(result) {
  for (const key of ['exitCode', 'tests', 'failed', 'skipped']) {
    if (!Number.isSafeInteger(result?.[key]) || result[key] < 0) {
      throw new Error(`Invalid test result: ${key}`);
    }
  }
  for (const key of ['compared', 'versionSkipped', 'missingBaseline']) {
    if (!Number.isSafeInteger(result.comparisons?.[key]) || result.comparisons[key] < 0) {
      throw new Error(`Invalid comparison count: ${key}`);
    }
  }
  if (result.tests === 0 || result.skipped === result.tests) {
    throw new Error('No tests executed');
  }
  if (result.failed + result.skipped > result.tests) throw new Error('Inconsistent test counts');
  if (result.exitCode === 0 && (result.failed || result.comparisons.missingBaseline)) {
    throw new Error('Successful result contains failures or missing baselines');
  }
}

module.exports = { ArtifactNames, Backends, Modes, ValidateBuildInfo, HashFile, HashText, ReadBuild,
  SnapshotBuild, RemoveBackup, ReplaceDirectory, Publish, FlattenStrings, ValidateCache, ValidateResult };

if (require.main === module) {
  try {
    const [backend, mode, buildDirectory] = process.argv.slice(2);
    console.log(Publish(backend, mode, path.resolve(buildDirectory)));
  } catch (error) {
    console.error(error.message);
    process.exitCode = 1;
  }
}
