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

// Guard for `npm publish` / `npm pack`: verifies that prebuild has staged the pagx-viewer
// artifacts (into wasm/) and the source-side static assets, otherwise the published tarball
// would be broken. Mirrors the intent of cli/npm/scripts/check-binaries.js.

import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const PKG_DIR = path.resolve(__dirname, '..');

const REQUIRED = [
  'static/index.html',
  'static/index.css',
  'static/index.js',
  'static/mcp-widget.html',
  'static/mcp-widget.js',
  'wasm/viewer/info.json',
  'wasm/player/pagx-player.esm.js',
  'static/icons/play.png',
  'static/icons/pause.png',
  'static/icons/previous.png',
  'static/icons/next.png',
];

function main() {
  if (process.env.PAGX_PREVIEW_SKIP_ARTIFACT_CHECK === '1') {
    process.stderr.write('check-artifacts: skipped (PAGX_PREVIEW_SKIP_ARTIFACT_CHECK=1)\n');
    return;
  }

  const missing = REQUIRED.filter((rel) => !fs.existsSync(path.join(PKG_DIR, rel)));
  if (missing.length > 0) {
    process.stderr.write('check-artifacts: ERROR: missing required artifacts:\n');
    for (const rel of missing) process.stderr.write(`  ${rel}\n`);
    process.stderr.write(
      'check-artifacts: run `npm run prebuild` from the pagx-preview directory ' +
      'and make sure pagx-viewer has been built first.\n'
    );
    process.exit(1);
  }

  // Extra: the wasm/glue files referenced by info.json must exist too.
  const infoPath = path.join(PKG_DIR, 'wasm/viewer/info.json');
  const info = JSON.parse(fs.readFileSync(infoPath, 'utf8'));
  for (const rel of [info.wasmFile, info.glueFile]) {
    const abs = path.join(PKG_DIR, 'wasm/viewer', rel);
    if (!fs.existsSync(abs)) {
      process.stderr.write(
        `check-artifacts: ERROR: info.json references ${rel} but the file is missing.\n`
      );
      process.exit(1);
    }
  }

  process.stderr.write('check-artifacts: all required artifacts present.\n');
}

main();
