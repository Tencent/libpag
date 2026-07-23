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

/**
 * Prebuild script for pagx-playground.
 *
 * This script copies two sets of pre-built artifacts into wasm-mt/ before the main rollup
 * build starts:
 *   1. pagx-viewer wasm + glue ESM (from ../pagx-viewer/lib/) - provides the wasm runtime.
 *   2. pagx-player ESM bundle    (from ../pagx-player/lib/)  - provides the player component
 *      that the playground imports as `pagx-player` (aliased in rollup.js).
 *
 * The script does NOT build either component itself; it only copies. If the required
 * artifacts are missing the script fails with a message directing the user to build the
 * missing component in its own directory.
 *
 * Usage:
 *   node scripts/prebuild.js                  # multi-threaded (default), debug
 *   node scripts/prebuild.js --arch st        # single-threaded, debug
 *   node scripts/prebuild.js --release        # multi-threaded, release
 *   node scripts/prebuild.js --arch st --release  single-threaded, release
 */

import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const PLAYGROUND_DIR = path.dirname(__dirname);
const PAGX_VIEWER_DIR = path.resolve(PLAYGROUND_DIR, '../pagx-viewer');
const PAGX_VIEWER_LIB_DIR = path.join(PAGX_VIEWER_DIR, 'lib');
const PAGX_PLAYER_DIR = path.resolve(PLAYGROUND_DIR, '../pagx-player');
const PAGX_PLAYER_LIB_DIR = path.join(PAGX_PLAYER_DIR, 'lib');
const OUTPUT_DIR = path.join(PLAYGROUND_DIR, 'wasm-mt');

// Parse CLI arguments: --arch <mt|st> selects the wasm flavor (default: auto-detect).
const CLI_ARGS = process.argv.slice(2);
function getArgValue(name) {
  const index = CLI_ARGS.indexOf(name);
  return index !== -1 ? CLI_ARGS[index + 1] : null;
}
const archArg = getArgValue('--arch');
const isRelease = CLI_ARGS.includes('--release');

// Auto-detect which arch artifacts exist in pagx-viewer/lib/.
// If neither exists, fall back to the default (multi-threaded) and let
// copyWasmFiles() report the error with helpful instructions.
function detectArch() {
  const mtExists = fs.existsSync(path.join(PAGX_VIEWER_LIB_DIR, 'pagx-viewer.wasm'));
  const stExists = fs.existsSync(path.join(PAGX_VIEWER_LIB_DIR, 'pagx-viewer.st.wasm'));
  if (archArg === 'st') return true;
  if (archArg === 'mt') return false;
  // No explicit --arch: prefer whichever exists. If both exist, pick multi-threaded.
  if (stExists && !mtExists) return true;
  return false;
}

const isSingleThreaded = detectArch();

// The single-threaded variant adds a `.st` infix so its glue/wasm don't collide
// with the multi-threaded ones.
const artifactInfix = isSingleThreaded ? '.st' : '';
const REQUIRED_FILES = [`pagx-viewer${artifactInfix}.wasm`, `pagx-viewer${artifactInfix}.esm.js`];
// When switching arch, remove the opposite-arch artifacts from the output dir
// so a stale wasm/glue of the wrong flavor can't linger.
const STALE_NAMES = isSingleThreaded
  ? ['pagx-viewer.wasm', 'pagx-viewer.esm.js']
  : ['pagx-viewer.st.wasm', 'pagx-viewer.st.esm.js'];

/**
 * Copy wasm files to wasm-mt/.
 */
function copyWasmFiles() {
  console.log('Copying wasm files to wasm-mt/...');

  if (!fs.existsSync(OUTPUT_DIR)) {
    fs.mkdirSync(OUTPUT_DIR, { recursive: true });
  }

  // Remove artifacts from the opposite arch before copying.
  for (const stale of STALE_NAMES) {
    const stalePath = path.join(OUTPUT_DIR, stale);
    if (fs.existsSync(stalePath)) {
      fs.unlinkSync(stalePath);
      console.log(`  Removed stale: ${stale}`);
    }
  }

  for (const file of REQUIRED_FILES) {
    const src = path.join(PAGX_VIEWER_LIB_DIR, file);
    const dest = path.join(OUTPUT_DIR, file);
    if (fs.existsSync(src)) {
      fs.copyFileSync(src, dest);
      console.log(`  Copied: ${file}`);
    } else {
      console.error(`\nERROR: Required artifact not found: ${src}`);
      console.error('Please build pagx-viewer first:');
      let buildCmd;
      if (isSingleThreaded) {
        buildCmd = isRelease ? 'npm run build:release:st' : 'npm run build:debug:st';
      } else {
        buildCmd = isRelease ? 'npm run build:release' : 'npm run build:debug';
      }
      console.error(`  cd ${PAGX_VIEWER_DIR} && ${buildCmd}\n`);
      process.exit(1);
    }
  }

  console.log('');
}

/**
 * Copy the pre-built pagx-player ESM bundle into wasm-mt/ so rollup can inline it via the
 * `pagx-player` alias. pagx-player is a sibling in-tree component package; the playground
 * consumes its build artifact from ../pagx-player/lib/ directly instead of resolving through
 * the npm registry. If the artifact is missing, we fail with a helpful message pointing at
 * the pagx-player build script.
 */
function copyPlayerFiles() {
  console.log('Copying pagx-player ESM bundle to wasm-mt/...');
  if (!fs.existsSync(OUTPUT_DIR)) {
    fs.mkdirSync(OUTPUT_DIR, { recursive: true });
  }
  const src = path.join(PAGX_PLAYER_LIB_DIR, 'pagx-player.esm.js');
  const dest = path.join(OUTPUT_DIR, 'pagx-player.esm.js');
  if (fs.existsSync(src)) {
    fs.copyFileSync(src, dest);
    console.log(`  Copied: pagx-player.esm.js`);
  } else {
    // The pagx-player bundle is not tracked by git (lib/ is gitignored). Direct the user to
    // build it locally rather than second-guessing why the file is missing.
    console.error(`\nERROR: Required artifact not found: ${src}`);
    console.error('Please build pagx-player first:');
    const playerBuildCmd = isRelease ? 'npm run build:release' : 'npm run build';
    console.error(`  cd ${PAGX_PLAYER_DIR} && npm install && ${playerBuildCmd}\n`);
    process.exit(1);
  }
  console.log('');
}

function main() {
  const archLabel = isSingleThreaded ? 'single-threaded' : 'multi-threaded';
  console.log(
    `Prebuild: Preparing pagx-viewer (${archLabel}) wasm files and pagx-player bundle...\n`,
  );

  copyWasmFiles();
  copyPlayerFiles();

  console.log('Prebuild complete.\n');
}

main();
