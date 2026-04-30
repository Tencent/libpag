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
 * This script ensures pagx-viewer is built and copies the wasm files to wasm-mt/
 * before the main rollup build starts.
 *
 * Usage:
 *   node script/prebuild.js [--release]
 */

import fs from 'fs';
import path from 'path';
import { execSync } from 'child_process';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const PLAYGROUND_DIR = path.dirname(__dirname);
const PAGX_VIEWER_DIR = path.resolve(PLAYGROUND_DIR, '../pagx-viewer');
const PAGX_VIEWER_LIB_DIR = path.join(PAGX_VIEWER_DIR, 'lib');
const WASM_MT_DIR = path.join(PLAYGROUND_DIR, 'wasm-mt');

const REQUIRED_FILES = ['pagx-viewer.wasm', 'pagx-viewer.esm.js'];

// Source directories/files that should invalidate the build when newer than the artifacts.
const SOURCE_PATHS = [
  path.join(PAGX_VIEWER_DIR, 'src'),
  path.join(PAGX_VIEWER_DIR, 'CMakeLists.txt'),
  path.join(PAGX_VIEWER_DIR, 'package.json'),
];

const isRelease = process.argv.includes('--release');

/**
 * Returns the most recent modification time (in ms) among the given files/directories.
 * Directories are walked recursively. Missing paths are skipped.
 */
function latestMtimeMs(targetPath) {
  let latest = 0;
  const stack = [targetPath];
  while (stack.length > 0) {
    const current = stack.pop();
    let stat;
    try {
      stat = fs.statSync(current);
    } catch {
      continue;
    }
    if (stat.isDirectory()) {
      for (const entry of fs.readdirSync(current)) {
        stack.push(path.join(current, entry));
      }
    } else if (stat.mtimeMs > latest) {
      latest = stat.mtimeMs;
    }
  }
  return latest;
}

/**
 * Check if pagx-viewer artifacts exist and are newer than the sources.
 */
function isPagxViewerBuilt() {
  let oldestArtifactMs = Number.POSITIVE_INFINITY;
  for (const file of REQUIRED_FILES) {
    const filePath = path.join(PAGX_VIEWER_LIB_DIR, file);
    if (!fs.existsSync(filePath)) {
      return false;
    }
    const artifactMs = fs.statSync(filePath).mtimeMs;
    if (artifactMs < oldestArtifactMs) {
      oldestArtifactMs = artifactMs;
    }
  }
  let latestSourceMs = 0;
  for (const sourcePath of SOURCE_PATHS) {
    const ms = latestMtimeMs(sourcePath);
    if (ms > latestSourceMs) {
      latestSourceMs = ms;
    }
  }
  return latestSourceMs <= oldestArtifactMs;
}

/**
 * Build pagx-viewer.
 */
function buildPagxViewer() {
  const buildCmd = isRelease ? 'npm run build:release' : 'npm run build';
  console.log(`Building pagx-viewer (${isRelease ? 'release' : 'debug'})...`);
  console.log(`  Running: ${buildCmd}`);
  try {
    execSync(buildCmd, {
      cwd: PAGX_VIEWER_DIR,
      stdio: 'inherit',
    });
    console.log('  pagx-viewer built successfully.\n');
  } catch (error) {
    console.error('Failed to build pagx-viewer:', error.message);
    process.exit(1);
  }
}

/**
 * Copy wasm files to wasm-mt/.
 */
function copyWasmFiles() {
  console.log('Copying wasm files to wasm-mt/...');

  if (!fs.existsSync(WASM_MT_DIR)) {
    fs.mkdirSync(WASM_MT_DIR, { recursive: true });
  }

  for (const file of REQUIRED_FILES) {
    const src = path.join(PAGX_VIEWER_LIB_DIR, file);
    const dest = path.join(WASM_MT_DIR, file);
    if (fs.existsSync(src)) {
      fs.copyFileSync(src, dest);
      console.log(`  Copied: ${file}`);
    } else {
      console.error(`  ERROR: Source file not found: ${src}`);
      process.exit(1);
    }
  }

  console.log('');
}

function main() {
  console.log('Prebuild: Preparing pagx-viewer wasm files...\n');

  if (!isPagxViewerBuilt()) {
    console.log('pagx-viewer not built or out of date, building now...\n');
    buildPagxViewer();
  } else {
    console.log('pagx-viewer already built and up to date.\n');
  }

  copyWasmFiles();

  console.log('Prebuild complete.\n');
}

main();
