#!/usr/bin/env node
'use strict';

// Guard executed before `npm publish` (wired via the package's
// `prepublishOnly` script).
//
// The native `pagx` binaries live in `bin/<platform>[-<arch>]/` and are
// gitignored — they must be staged by the release build before publishing.
// Without them the published package contains only the JS launcher and fails
// at runtime for every user, so fail loudly here instead of shipping a broken
// tarball.
//
// Set PAGX_SKIP_BINARY_CHECK=1 to bypass (e.g. a deliberate single-platform
// or test publish). Not recommended for a real release.

const fs = require('fs');
const path = require('path');

const PKG_DIR = path.resolve(__dirname, '..');
const BIN_DIR = path.join(PKG_DIR, 'bin');
const SUPPORTED = ['darwin', 'linux', 'win32'];

function binaryName(platform) {
  return platform === 'win32' ? 'pagx.exe' : 'pagx';
}

// A platform is satisfied when any `bin/<platform>*` directory contains the
// expected executable. This covers both the legacy `<platform>` layout and the
// arch-specific `<platform>-<arch>` layout the launcher prefers.
function findBinaries(platform) {
  if (!fs.existsSync(BIN_DIR)) return [];
  const name = binaryName(platform);
  const matches = [];
  for (const entry of fs.readdirSync(BIN_DIR, { withFileTypes: true })) {
    if (!entry.isDirectory()) continue;
    if (entry.name !== platform && !entry.name.startsWith(platform + '-')) continue;
    const candidate = path.join(BIN_DIR, entry.name, name);
    if (fs.existsSync(candidate)) {
      matches.push(path.relative(PKG_DIR, candidate));
    }
  }
  return matches;
}

function main() {
  if (process.env.PAGX_SKIP_BINARY_CHECK === '1') {
    process.stderr.write('check-binaries: skipped (PAGX_SKIP_BINARY_CHECK=1)\n');
    return;
  }

  const found = [];
  const missing = [];
  for (const platform of SUPPORTED) {
    const matches = findBinaries(platform);
    if (matches.length === 0) {
      missing.push(platform);
    } else {
      found.push(...matches);
    }
  }

  if (found.length > 0) {
    process.stderr.write('check-binaries: found native binaries:\n');
    for (const f of found) process.stderr.write(`  ${f}\n`);
  }

  if (missing.length > 0) {
    process.stderr.write(
      `check-binaries: ERROR: missing native binary for platform(s): ${missing.join(', ')}.\n`,
    );
    process.stderr.write(
      'check-binaries: each supported platform needs an executable at ' +
        'bin/<platform>[-<arch>]/pagx[.exe]. Build and stage the release ' +
        'binaries before `npm publish`.\n',
    );
    process.stderr.write(
      'check-binaries: set PAGX_SKIP_BINARY_CHECK=1 to bypass (not recommended ' +
        'for a real release).\n',
    );
    process.exit(1);
  }

  process.stderr.write('check-binaries: all supported platforms present.\n');
}

main();
