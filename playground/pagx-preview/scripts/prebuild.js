#!/usr/bin/env node
/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

// Detects which pagx-viewer build is available (multi-threaded or single-threaded), copies its
// artifacts into wasm/viewer/, and writes info.json so both the server (COOP/COEP headers)
// and the client (dynamic import path) know which variant is in use. Also copies the compiled
// pagx-player esm bundle into wasm/player/ so the client can import it without going through
// a bundler; pagx-player is workspace-local and has no npm publish target, so the preview owns
// the file copy the same way it owns the pagx-viewer copy.
//
// All generated/copied artifacts live under a single gitignored wasm/ directory (mirroring
// pagx-playground's wasm-mt/ layout) so the source-only static/ dir stays clean and no heavy
// binaries are committed. They are regenerated on demand via `npm run build` and on publish via
// the prepack hook.

import fs from 'fs';
import path from 'path';
import { execSync } from 'child_process';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const PREVIEW_DIR = path.dirname(__dirname);
const VIEWER_DIR = path.resolve(PREVIEW_DIR, '../pagx-viewer');
const PLAYER_DIR = path.resolve(PREVIEW_DIR, '../pagx-player');
const VIEWER_LIB_DIR = path.join(VIEWER_DIR, 'lib');
const PLAYER_LIB_DIR = path.join(PLAYER_DIR, 'lib');
// Single gitignored directory holding every compiled/copied artifact (viewer, player, ext, and
// the MCP widget bundle). Kept out of static/ so the source tree stays free of build outputs.
const GENERATED_DIR = path.join(PREVIEW_DIR, 'wasm');
const OUTPUT_DIR = path.join(GENERATED_DIR, 'viewer');
const PLAYER_OUTPUT_DIR = path.join(GENERATED_DIR, 'player');

// (infix, humanLabel, isMultiThreaded). Order matters: MT is preferred when both are present
// so the preview matches pagx-playground's default flavor.
const VARIANTS = [
  { infix: '', label: 'multi-threaded', multiThreaded: true },
  { infix: '.st', label: 'single-threaded', multiThreaded: false },
];

function detectVariant() {
  for (const variant of VARIANTS) {
    const wasm = path.join(VIEWER_LIB_DIR, `pagx-viewer${variant.infix}.wasm`);
    const glue = path.join(VIEWER_LIB_DIR, `pagx-viewer${variant.infix}.esm.js`);
    if (fs.existsSync(wasm) && fs.existsSync(glue)) {
      return variant;
    }
  }
  return null;
}

function cleanOldArtifacts() {
  if (!fs.existsSync(OUTPUT_DIR)) return;
  for (const entry of fs.readdirSync(OUTPUT_DIR)) {
    if (entry.startsWith('pagx-viewer') || entry === 'info.json') {
      fs.unlinkSync(path.join(OUTPUT_DIR, entry));
    }
  }
}

function copyPagxPlayerArtifacts({ release } = {}) {
  if (!fs.existsSync(PLAYER_LIB_DIR)) {
    console.error('\npagx-preview prebuild: ERROR: pagx-player has not been built.');
    console.error(`Looked in: ${PLAYER_LIB_DIR}`);
    console.error('Please build pagx-player first:');
    console.error(`  cd ${path.dirname(PLAYER_LIB_DIR)} && npm install && npm run build\n`);
    process.exit(1);
  }
  const bundleFile = 'pagx-player.esm.js';
  const src = path.join(PLAYER_LIB_DIR, bundleFile);
  if (!fs.existsSync(src)) {
    console.error(`\npagx-preview prebuild: ERROR: ${bundleFile} not found in ${PLAYER_LIB_DIR}.`);
    console.error('Please rebuild pagx-player:');
    console.error(`  cd ${path.dirname(PLAYER_LIB_DIR)} && npm run build\n`);
    process.exit(1);
  }
  if (!fs.existsSync(PLAYER_OUTPUT_DIR)) {
    fs.mkdirSync(PLAYER_OUTPUT_DIR, { recursive: true });
  }
  // Clean previous copies so an older bundle can't shadow the newly built one.
  for (const entry of fs.readdirSync(PLAYER_OUTPUT_DIR)) {
    if (entry.startsWith('pagx-player')) {
      fs.unlinkSync(path.join(PLAYER_OUTPUT_DIR, entry));
    }
  }
  fs.copyFileSync(src, path.join(PLAYER_OUTPUT_DIR, bundleFile));
  console.log(`  Copied: ${bundleFile}`);
  // Copy the source map only for debug builds (local development). The 2MB+ map is useless in a
  // published/release package — it would only bloat the tarball — so release skips it. The
  // .unlink loop above already removed any stale map from a previous debug run.
  if (!release) {
    const mapFile = bundleFile + '.map';
    const mapSrc = path.join(PLAYER_LIB_DIR, mapFile);
    if (fs.existsSync(mapSrc)) {
      fs.copyFileSync(mapSrc, path.join(PLAYER_OUTPUT_DIR, mapFile));
      console.log(`  Copied: ${mapFile}`);
    }
  }
}

// Copies the @modelcontextprotocol/ext-apps browser bundle (app-with-deps.js) into wasm/ext/
// so the MCP widget HTML can import it from the same origin without depending on esm.sh. The
// bundle is self-contained (includes all transitive runtime deps) so no further bundling is
// needed. If the package is not installed (e.g. dev environment without MCP support), the copy
// is skipped with a warning — the widget will fail to load but the rest of pagx-preview works.
function copyExtAppsBundle() {
  const EXT_OUTPUT_DIR = path.join(GENERATED_DIR, 'ext');
  // Try to locate the ext-apps package via node_modules resolution. The package may be hoisted
  // to a parent node_modules in a monorepo / npm workspace setup, so we walk up from the preview
  // dir rather than hard-coding a single path.
  const candidatePaths = [
    path.join(PREVIEW_DIR, 'node_modules', '@modelcontextprotocol', 'ext-apps'),
    path.join(path.dirname(PREVIEW_DIR), 'node_modules', '@modelcontextprotocol', 'ext-apps'),
    path.join(path.dirname(path.dirname(PREVIEW_DIR)), 'node_modules', '@modelcontextprotocol', 'ext-apps'),
  ];
  const pkgDir = candidatePaths.find((p) => fs.existsSync(path.join(p, 'package.json')));
  if (!pkgDir) {
    console.log('  Skipped: @modelcontextprotocol/ext-apps not installed (MCP widget disabled)');
    return;
  }
  const src = path.join(pkgDir, 'dist', 'src', 'app-with-deps.js');
  if (!fs.existsSync(src)) {
    console.log(`  Skipped: ext-apps bundle not found at ${src}`);
    return;
  }
  if (!fs.existsSync(EXT_OUTPUT_DIR)) {
    fs.mkdirSync(EXT_OUTPUT_DIR, { recursive: true });
  }
  fs.copyFileSync(src, path.join(EXT_OUTPUT_DIR, 'app-with-deps.js'));
  console.log('  Copied: ext/app-with-deps.js');
}

// Builds the upstream pagx-viewer and pagx-player packages in place so a single
// `npm run build` from pagx-preview produces every artifact this script then copies. Without
// --build the script only detects + copies pre-built artifacts (the historical behavior), which
// is what `npm pack` / a clean checkout with committed artifacts relies on.
//
// The viewer defaults to the single-threaded (st) variant because the MCP widget runs inside a
// sandbox iframe with no cross-origin isolation, so SharedArrayBuffer (required by the
// multi-threaded wasm) is unavailable there — st is the only variant that works in every
// consumer. --mt opts into the multi-threaded build, which renders faster but only works in the
// plain browser preview (`pagx-preview file.pagx`), where the server can send COOP/COEP headers;
// in an MCP host the widget then falls back to the browser URL.
function buildUpstreamDependencies({ release, mt }) {
  let viewerScript;
  if (mt) {
    viewerScript = release ? 'build:release' : 'build:debug';
  } else {
    viewerScript = release ? 'build:release:st' : 'build:debug:st';
  }
  const playerScript = release ? 'build:release' : 'build';

  if (!fs.existsSync(VIEWER_DIR)) {
    console.error(`\npagx-preview prebuild: ERROR: pagx-viewer not found at ${VIEWER_DIR}.`);
    process.exit(1);
  }
  if (!fs.existsSync(PLAYER_DIR)) {
    console.error(`\npagx-preview prebuild: ERROR: pagx-player not found at ${PLAYER_DIR}.`);
    process.exit(1);
  }

  // pagx-viewer's wasm build is guarded by a source-hash cache (.pagx-viewer.wasm.md5) that does
  // not distinguish debug from release: after a debug build it will skip recompiling for a
  // release run and reuse the ~100MB debug wasm (full DWARF info). Clean the viewer first on a
  // release build so the wasm is actually recompiled with -O3 and stripped down to a few MB.
  // Debug builds keep the cache to preserve fast incremental rebuilds during development.
  if (release) {
    console.log('pagx-preview prebuild: cleaning pagx-viewer (release: force wasm recompile)...');
    execSync('npm run clean', { cwd: VIEWER_DIR, stdio: 'inherit' });
  }

  console.log(`pagx-preview prebuild: building pagx-viewer (npm run ${viewerScript})...`);
  execSync(`npm run ${viewerScript}`, { cwd: VIEWER_DIR, stdio: 'inherit' });

  console.log(`pagx-preview prebuild: building pagx-player (npm run ${playerScript})...`);
  execSync(`npm run ${playerScript}`, { cwd: PLAYER_DIR, stdio: 'inherit' });
}

function main() {
  const args = process.argv.slice(2);
  const doBuild = args.includes('--build');
  const release = args.includes('--release');
  const mt = args.includes('--mt');
  if (doBuild) {
    buildUpstreamDependencies({ release, mt });
  }

  const variant = detectVariant();
  if (!variant) {
    console.error('\npagx-preview prebuild: ERROR: no pagx-viewer build found.');
    console.error(`Looked in: ${VIEWER_LIB_DIR}`);
    console.error('Build the upstream dependencies automatically:');
    console.error(`  cd ${PREVIEW_DIR} && npm run build            # debug`);
    console.error(`  cd ${PREVIEW_DIR} && npm run build:release    # release`);
    console.error('Or build pagx-viewer manually first (either variant works):');
    console.error(`  cd ${VIEWER_DIR} && npm run build:debug       # multi-threaded`);
    console.error(`  cd ${VIEWER_DIR} && npm run build:debug:st    # single-threaded\n`);
    process.exit(1);
  }

  console.log(`pagx-preview prebuild: detected pagx-viewer (${variant.label}) build.`);

  if (!fs.existsSync(OUTPUT_DIR)) {
    fs.mkdirSync(OUTPUT_DIR, { recursive: true });
  }
  cleanOldArtifacts();

  const files = [`pagx-viewer${variant.infix}.wasm`, `pagx-viewer${variant.infix}.esm.js`];
  for (const file of files) {
    fs.copyFileSync(path.join(VIEWER_LIB_DIR, file), path.join(OUTPUT_DIR, file));
    console.log(`  Copied: ${file}`);
  }

  // Consumed by both the server (to decide whether COOP/COEP headers are required) and the
  // client (to dynamically import the correct glue file).
  const info = {
    variant: variant.multiThreaded ? 'mt' : 'st',
    multiThreaded: variant.multiThreaded,
    glueFile: `pagx-viewer${variant.infix}.esm.js`,
    wasmFile: `pagx-viewer${variant.infix}.wasm`,
  };
  fs.writeFileSync(
    path.join(OUTPUT_DIR, 'info.json'),
    JSON.stringify(info, null, 2) + '\n'
  );
  console.log(`  Wrote:  info.json (${info.variant})`);

  copyPagxPlayerArtifacts({ release });
  copyExtAppsBundle();
  buildMcpWidgetBundle();

  console.log('pagx-preview prebuild: done.');
}

// Builds the MCP widget bundle: a single minified ES module that inlines app-with-deps.js,
// pagx-player.esm.js, and mcp-widget.js. This bundle is base64-encoded and injected into the
// widget HTML at runtime by the readResource handler so the MCP Apps sandbox iframe does not
// need to load external scripts (which many hosts block).
function buildMcpWidgetBundle() {
  const widgetSrc = path.join(PREVIEW_DIR, 'static', 'mcp-widget.js');
  if (!fs.existsSync(widgetSrc)) {
    console.log('  Skipped: mcp-widget.js not found (MCP widget bundle not built)');
    return;
  }
  if (!fs.existsSync(GENERATED_DIR)) {
    fs.mkdirSync(GENERATED_DIR, { recursive: true });
  }
  // Create a temp entry inside GENERATED_DIR with imports rewritten relative to that dir (esbuild
  // cannot resolve absolute /wasm/... URLs). The ext/ and player/ artifacts were copied into
  // GENERATED_DIR earlier in this run, so './ext/...' / './player/...' resolve correctly.
  const entryPath = path.join(GENERATED_DIR, '_mcp_bundle_entry.js');
  let src = fs.readFileSync(widgetSrc, 'utf8');
  src = src
    .replace(/from '\/wasm\/ext\/app-with-deps\.js'/g, "from './ext/app-with-deps.js'")
    .replace(/from '\/wasm\/player\/pagx-player\.esm\.js'/g, "from './player/pagx-player.esm.js'");
  fs.writeFileSync(entryPath, src);
  try {
    const outPath = path.join(GENERATED_DIR, 'mcp-widget.bundle.js');
    execSync(`npx esbuild "${entryPath}" --bundle --format=esm --outfile="${outPath}" --minify`, {
      cwd: PREVIEW_DIR,
      stdio: 'pipe',
    });
    console.log(`  Built:  mcp-widget.bundle.js (${(fs.statSync(outPath).size / 1024).toFixed(0)} KB)`);
  } catch (err) {
    console.log(`  Warning: mcp-widget.bundle.js build failed: ${err.message}`);
    console.log('  The MCP widget will fall back to external script loading.');
  } finally {
    if (fs.existsSync(entryPath)) fs.unlinkSync(entryPath);
  }
}

main();
