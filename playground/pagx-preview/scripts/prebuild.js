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
// artifacts into static/viewer/, and writes info.json so both the server (COOP/COEP headers)
// and the client (dynamic import path) know which variant is in use. Also copies the compiled
// pagx-player esm bundle into static/player/ so the client can import it without going through
// a bundler; pagx-player is workspace-local and has no npm publish target, so the preview owns
// the file copy the same way it owns the pagx-viewer copy.

import fs from 'fs';
import path from 'path';
import { execSync } from 'child_process';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const PREVIEW_DIR = path.dirname(__dirname);
const VIEWER_LIB_DIR = path.resolve(PREVIEW_DIR, '../pagx-viewer/lib');
const PLAYER_LIB_DIR = path.resolve(PREVIEW_DIR, '../pagx-player/lib');
const OUTPUT_DIR = path.join(PREVIEW_DIR, 'static', 'viewer');
const PLAYER_OUTPUT_DIR = path.join(PREVIEW_DIR, 'static', 'player');

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

function copyPagxPlayerArtifacts() {
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
  // Also copy the source map when the debug build was produced; harmless when absent.
  const mapFile = bundleFile + '.map';
  const mapSrc = path.join(PLAYER_LIB_DIR, mapFile);
  if (fs.existsSync(mapSrc)) {
    fs.copyFileSync(mapSrc, path.join(PLAYER_OUTPUT_DIR, mapFile));
    console.log(`  Copied: ${mapFile}`);
  }
}

// Copies the @modelcontextprotocol/ext-apps browser bundle (app-with-deps.js) into static/ext/
// so the MCP widget HTML can import it from the same origin without depending on esm.sh. The
// bundle is self-contained (includes all transitive runtime deps) so no further bundling is
// needed. If the package is not installed (e.g. dev environment without MCP support), the copy
// is skipped with a warning — the widget will fail to load but the rest of pagx-preview works.
function copyExtAppsBundle() {
  const EXT_OUTPUT_DIR = path.join(PREVIEW_DIR, 'static', 'ext');
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

function main() {
  const variant = detectVariant();
  if (!variant) {
    console.error('\npagx-preview prebuild: ERROR: no pagx-viewer build found.');
    console.error(`Looked in: ${VIEWER_LIB_DIR}`);
    console.error('Please build pagx-viewer first (either variant works):');
    console.error(`  cd ${path.dirname(VIEWER_LIB_DIR)} && npm run build:debug       # multi-threaded`);
    console.error(`  cd ${path.dirname(VIEWER_LIB_DIR)} && npm run build:debug:st    # single-threaded\n`);
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

  copyPagxPlayerArtifacts();
  copyExtAppsBundle();
  buildMcpWidgetBundle();

  console.log('pagx-preview prebuild: done.');
}

// Builds the MCP widget bundle: a single minified ES module that inlines app-with-deps.js,
// pagx-player.esm.js, and mcp-widget.js. This bundle is base64-encoded and injected into the
// widget HTML at runtime by the readResource handler so the MCP Apps sandbox iframe does not
// need to load external scripts (which many hosts block).
function buildMcpWidgetBundle() {
  const STATIC_DIR = path.join(PREVIEW_DIR, 'static');
  const widgetSrc = path.join(STATIC_DIR, 'mcp-widget.js');
  if (!fs.existsSync(widgetSrc)) {
    console.log('  Skipped: mcp-widget.js not found (MCP widget bundle not built)');
    return;
  }
  // Create a temp entry with relative imports (esbuild cannot resolve absolute /static/... paths)
  const entryPath = path.join(STATIC_DIR, '_mcp_bundle_entry.js');
  let src = fs.readFileSync(widgetSrc, 'utf8');
  src = src
    .replace(/from '\/static\/ext\/app-with-deps\.js'/g, "from './ext/app-with-deps.js'")
    .replace(/from '\/static\/player\/pagx-player\.esm\.js'/g, "from './player/pagx-player.esm.js'");
  fs.writeFileSync(entryPath, src);
  try {
    const outPath = path.join(STATIC_DIR, 'mcp-widget.bundle.js');
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
