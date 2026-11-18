#!/usr/bin/env node
'use strict';

const fs = require('fs');
const https = require('https');
const path = require('path');
const { DEFAULT_RELEASE, resolveTarget } = require('./download-ffmpeg.js');

const TARGETS = [
  ['darwin', 'arm64'],
  ['darwin', 'x64'],
  ['linux', 'arm64'],
  ['linux', 'x64'],
  ['win32', 'arm64'],
  ['win32', 'x64'],
];
const RELEASE_URL = `https://api.github.com/repos/libpag/ffmpeg-builder/releases/tags/${DEFAULT_RELEASE}`;
const MAX_RELEASE_METADATA_BYTES = 4 * 1024 * 1024;

function buildManifest(releaseMetadata) {
  if (!releaseMetadata || releaseMetadata.tag_name !== DEFAULT_RELEASE ||
      !Array.isArray(releaseMetadata.assets)) {
    throw new Error(`GitHub did not return the expected ${DEFAULT_RELEASE} release metadata`);
  }
  const manifest = { release: DEFAULT_RELEASE, assets: {} };
  for (const [platform, arch] of TARGETS) {
    const target = resolveTarget(platform, arch);
    const asset = releaseMetadata.assets.find((candidate) => candidate.name === target.assetName);
    const digest = asset && /^sha256:([0-9a-f]{64})$/.exec(asset.digest || '');
    if (!asset || !Number.isSafeInteger(asset.size) || asset.size < 0 || !digest) {
      throw new Error(`the official release has no verified size and SHA-256 for ${target.assetName}`);
    }
    manifest.assets[target.name] = {
      name: asset.name,
      size: asset.size,
      sha256: digest[1],
    };
  }
  return manifest;
}

function requestReleaseMetadata() {
  return new Promise((resolve, reject) => {
    const headers = {
      Accept: 'application/vnd.github+json',
      'User-Agent': 'pagx-ffmpeg-manifest-generator',
      'X-GitHub-Api-Version': '2022-11-28',
    };
    if (process.env.GITHUB_TOKEN) headers.Authorization = `Bearer ${process.env.GITHUB_TOKEN}`;
    const request = https.get(RELEASE_URL, { headers }, (response) => {
      if (response.statusCode !== 200) {
        response.resume();
        reject(new Error(`GitHub release metadata returned HTTP ${response.statusCode}`));
        return;
      }
      let size = 0;
      const chunks = [];
      response.on('data', (chunk) => {
        size += chunk.length;
        if (size > MAX_RELEASE_METADATA_BYTES) {
          response.destroy(new Error('GitHub release metadata exceeds the size limit'));
          return;
        }
        chunks.push(chunk);
      });
      response.on('end', () => {
        try {
          resolve(JSON.parse(Buffer.concat(chunks).toString('utf8')));
        } catch (error) {
          reject(error);
        }
      });
      response.on('error', reject);
    });
    request.setTimeout(30000, () => request.destroy(new Error('GitHub release metadata request timed out')));
    request.on('error', reject);
  });
}

async function generateManifest() {
  const manifest = buildManifest(await requestReleaseMetadata());
  const destination = path.join(__dirname, '..', 'ffmpeg-manifest.json');
  const temporary = `${destination}.${process.pid}.tmp`;
  try {
    fs.writeFileSync(temporary, `${JSON.stringify(manifest, null, 2)}\n`, { flag: 'wx' });
    fs.renameSync(temporary, destination);
  } finally {
    fs.rmSync(temporary, { force: true });
  }
  console.log(`Generated ${destination} for ${DEFAULT_RELEASE}`);
}

module.exports = { buildManifest, generateManifest };

if (require.main === module) {
  generateManifest().catch((error) => {
    console.error(`generate-ffmpeg-manifest: ERROR: ${error.message}`);
    process.exitCode = 1;
  });
}
