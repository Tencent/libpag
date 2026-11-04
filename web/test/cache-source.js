// Copyright (C) 2026 Tencent. All rights reserved.
const fs = require('node:fs');
const path = require('node:path');
const { execFileSync } = require('node:child_process');
const { HashText, ValidateBuildInfo, ValidateResult } = require('./artifacts');

const ProjectRoot = path.resolve(__dirname, '../..');

function Git(root, args) {
  return execFileSync('git', ['-C', root, ...args], { encoding: 'utf8', timeout: 30000 }).trim();
}

function SourceMetadata(revision, root = ProjectRoot) {
  if (!/^[a-f0-9]{40}$/.test(revision)) throw new Error('Invalid baseline source revision');
  Git(root, ['merge-base', '--is-ancestor', revision, 'origin/main']);
  const deps = JSON.parse(Git(root, ['show', `${revision}:DEPS`]));
  const tgfx = deps.repos?.common?.find(function isTGFX(repo) { return repo.dir === 'third_party/tgfx'; });
  if (!/^[a-f0-9]{40}$/.test(tgfx?.commit)) throw new Error('Missing pinned TGFX revision');
  const version = execFileSync('git', ['-C', root, 'show', `${revision}:test/baseline/version.json`],
    { encoding: 'utf8', timeout: 30000 });
  return { sourceRevision: revision, tgfxRevision: tgfx.commit, versionHash: HashText(version) };
}

function ValidateSource(info, metadata) {
  if (info.sourceRevision !== metadata.sourceRevision || info.tgfxRevision !== metadata.tgfxRevision) {
    throw new Error('Baseline source does not match the main revision and its pinned TGFX dependency');
  }
}

function ValidateProvenance(cache, provenance, backend, metadata) {
  ValidateBuildInfo(provenance.buildInfo, backend, 'update');
  ValidateResult(provenance.result);
  ValidateSource(provenance.buildInfo, metadata);
  if (provenance.result.exitCode !== 0 || provenance.filter !== '' ||
      !Array.isArray(provenance.errors) || provenance.errors.length !== 0 ||
      provenance.source?.ref !== 'origin/main' || provenance.source?.clean !== true) {
    throw new Error('Baseline must come from a complete successful main run');
  }
  if (HashText(cache.version) !== metadata.versionHash) throw new Error('Baseline source version mismatch');
  for (const name of ['md5', 'version', 'head']) {
    if (provenance.cacheHashes?.[name] !== HashText(cache[name])) {
      throw new Error(`Baseline cache integrity mismatch: ${name}`);
    }
  }
  const head = cache.head.trim();
  if (!/^[a-f0-9]{7,40}$/.test(head) || !metadata.sourceRevision.startsWith(head)) {
    throw new Error('Baseline HEAD mismatch');
  }
}

function CheckUpdateSource(info, root = ProjectRoot) {
  const revision = Git(root, ['rev-parse', 'HEAD']);
  if (revision !== Git(root, ['rev-parse', 'origin/main'])) throw new Error('Update requires origin/main HEAD');
  const metadata = SourceMetadata(revision, root);
  if (info) ValidateSource(info, metadata);
  const dirty = Git(root, ['status', '--porcelain', '--untracked-files=normal']);
  if (dirty) {
    const entries = dirty.split('\n').slice(0, 5).join('\n');
    throw new Error(`Baseline source working tree must be clean:\n${entries}`);
  }
  const tgfx = fs.realpathSync(path.join(root, 'third_party/tgfx'));
  if (tgfx !== path.join(fs.realpathSync(root), 'third_party/tgfx') ||
      Git(tgfx, ['rev-parse', '--show-toplevel']) !== tgfx ||
      Git(tgfx, ['rev-parse', 'HEAD']) !== metadata.tgfxRevision ||
      Git(tgfx, ['status', '--porcelain', '--untracked-files=normal'])) {
    throw new Error('TGFX must be the clean pinned checkout under third_party/tgfx');
  }
  if (HashText(fs.readFileSync(path.join(root, 'test/baseline/version.json'), 'utf8')) !== metadata.versionHash) {
    throw new Error('Working baseline version differs from main');
  }
  return metadata;
}

module.exports = { SourceMetadata, ValidateSource, ValidateProvenance, CheckUpdateSource };

if (require.main === module) {
  try {
    CheckUpdateSource();
  } catch (error) {
    console.error(error.message);
    process.exitCode = 1;
  }
}
