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
 * This script copies the pre-built pagx-viewer wasm/glue files to wasm-mt/
 * before the main rollup build starts. It does NOT build pagx-viewer itself;
 * if the required artifacts are missing, the user must build them first in
 * the pagx-viewer directory.
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

function main() {
  const archLabel = isSingleThreaded ? 'single-threaded' : 'multi-threaded';
  console.log(`Prebuild: Preparing pagx-viewer (${archLabel}) wasm files...\n`);

  copyWasmFiles();

  console.log('Prebuild complete.\n');
}

main();
