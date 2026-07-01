#!/usr/bin/env node
'use strict';

// Build the bundled copy of the html-snapshot tool that ships inside the
// @libpag/pagx npm package.
//
// The source of truth is `tools/html-snapshot/` in the repository. This script
// compiles it and copies the runtime-only pieces into `cli/npm/html-snapshot/`
// so that an external user who only `npm install`-ed the package (no git
// checkout) can still run `pagx import` on HTML — the native binary
// shells out to the bundled `snapshot.js` via `PAGX_HTML_SNAPSHOT_BIN`, which
// the package's `bin/pagx.js` wrapper points at `html-snapshot/launch.js`.
//
// What ships:
//   - snapshot.js            (CLI driver, copied verbatim)
//   - ensure-built.js        (preflight helper snapshot.js requires at startup)
//   - dist/                  (compiled TypeScript: dist/lib/*.js)
//   - node_modules/          (the pure-JS runtime deps: opentype.js, wawoff2)
//   - package.json           (generated; declares only the pure-JS deps)
//
// What does NOT ship (installed lazily at first use by launch.js):
//   - puppeteer + its Chromium (~150 MB). Keeping it out of the package keeps
//     `npm install -g @libpag/pagx` light.
//
// `launch.js` is a maintained source file in `cli/npm/html-snapshot/` and is
// intentionally preserved by this script.

const fs = require('fs');
const path = require('path');
const { execFileSync } = require('child_process');

const NPM = process.platform === 'win32' ? 'npm.cmd' : 'npm';

const NPM_PKG_DIR = path.resolve(__dirname, '..'); // cli/npm
const REPO_ROOT = path.resolve(NPM_PKG_DIR, '..', '..');
const TOOL_DIR = path.join(REPO_ROOT, 'tools', 'html-snapshot');
const DEST_DIR = path.join(NPM_PKG_DIR, 'html-snapshot');

// Runtime npm dependencies the snapshot pipeline actually requires (puppeteer
// is deliberately excluded — see header). Versions are read from the tool's
// own package.json so they never drift.
const RUNTIME_DEPS = ['opentype.js', 'wawoff2'];

function say(msg) {
  process.stderr.write(`build-html-snapshot: ${msg}\n`);
}

function die(msg) {
  process.stderr.write(`build-html-snapshot: ERROR: ${msg}\n`);
  process.exit(1);
}

function run(cmd, args, cwd) {
  say(`${cmd} ${args.join(' ')}  (in ${cwd})`);
  execFileSync(cmd, args, { cwd, stdio: 'inherit' });
}

// Recursively delete every *.map file under `dir`. Source maps are never needed
// at runtime; shipping them only bloats the published package. Returns the
// number of files removed.
function stripSourceMaps(dir) {
  if (!fs.existsSync(dir)) return 0;
  let removed = 0;
  for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
    const full = path.join(dir, entry.name);
    if (entry.isDirectory()) {
      removed += stripSourceMaps(full);
    } else if (entry.name.endsWith('.map')) {
      fs.rmSync(full);
      removed += 1;
    }
  }
  return removed;
}

function main() {
  if (!fs.existsSync(TOOL_DIR)) {
    die(`source tool not found at ${TOOL_DIR}`);
  }

  // 1. Install + build the source tool so dist/ is up to date.
  if (!fs.existsSync(path.join(TOOL_DIR, 'node_modules'))) {
    run(NPM, ['ci'], TOOL_DIR);
  }
  run(NPM, ['run', 'build'], TOOL_DIR);

  const distDir = path.join(TOOL_DIR, 'dist');
  if (!fs.existsSync(distDir)) {
    die(`expected build output at ${distDir}`);
  }

  // 2. Reset the generated parts of DEST while preserving the maintained
  //    launch.js source file.
  for (const entry of ['dist', 'node_modules', 'snapshot.js', 'ensure-built.js', 'package.json']) {
    fs.rmSync(path.join(DEST_DIR, entry), { recursive: true, force: true });
  }
  fs.mkdirSync(DEST_DIR, { recursive: true });

  // 3. Copy the CLI driver, its startup preflight helper, and the compiled output.
  //    snapshot.js does `require('./ensure-built')` on its first line, so the helper
  //    must ship alongside it or the installed package fails with MODULE_NOT_FOUND.
  fs.copyFileSync(path.join(TOOL_DIR, 'snapshot.js'), path.join(DEST_DIR, 'snapshot.js'));
  fs.copyFileSync(path.join(TOOL_DIR, 'ensure-built.js'), path.join(DEST_DIR, 'ensure-built.js'));
  fs.cpSync(distDir, path.join(DEST_DIR, 'dist'), { recursive: true });

  // 3a. Strip the compiled TypeScript source maps. Their `sources` point at the
  //     original `.ts` files (e.g. ../../lib/errors.ts) which are not shipped,
  //     so the maps are dead weight for an installed package — drop them.
  say(`stripped ${stripSourceMaps(path.join(DEST_DIR, 'dist'))} TypeScript source map(s) from dist/`);

  // 4. Generate a minimal package.json pinning only the pure-JS runtime deps.
  const toolPkg = JSON.parse(fs.readFileSync(path.join(TOOL_DIR, 'package.json'), 'utf8'));
  const deps = {};
  for (const name of RUNTIME_DEPS) {
    const version = (toolPkg.dependencies && toolPkg.dependencies[name]) || undefined;
    if (!version) {
      die(`runtime dependency '${name}' not found in ${TOOL_DIR}/package.json`);
    }
    deps[name] = version;
  }
  const destPkg = {
    name: 'html-snapshot-bundled',
    version: toolPkg.version || '0.0.0',
    private: true,
    description: 'Bundled html-snapshot runtime shipped inside @libpag/pagx (puppeteer is installed lazily).',
    main: 'snapshot.js',
    dependencies: deps,
  };
  fs.writeFileSync(
    path.join(DEST_DIR, 'package.json'),
    JSON.stringify(destPkg, null, 2) + '\n',
    'utf8',
  );

  // 5. Install the pure-JS runtime deps into DEST/node_modules (no puppeteer,
  //    no dev/optional deps).
  run(NPM, ['install', '--omit=dev', '--omit=optional', '--no-package-lock'], DEST_DIR);

  // Drop the generated lockfile if npm wrote one despite --no-package-lock.
  fs.rmSync(path.join(DEST_DIR, 'package-lock.json'), { force: true });

  // 6. Strip source maps the runtime deps ship (e.g. opentype.js's ~2 MB of
  //    *.map). They are never used at runtime and only bloat the package.
  say(`stripped ${stripSourceMaps(path.join(DEST_DIR, 'node_modules'))} source map(s) from node_modules/`);

  say('done');
}

main();
