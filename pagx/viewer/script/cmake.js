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

import { fileURLToPath } from 'url';
import { copyFileSync, existsSync, mkdirSync } from 'fs';
import path from 'path';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

process.chdir(__dirname);

process.argv.push("-s");
process.argv.push("../");
process.argv.push("-o");
process.argv.push("./");
process.argv.push("-p");
process.argv.push("web");
process.argv.push("pagx-viewer");

// Use build_tgfx from third_party/tgfx
const buildTgfx = await import("../../../../third_party/tgfx/build_tgfx");

// Copy wasm files to viewer/wasm-mt/ directory
const srcDir = path.resolve(__dirname, 'wasm-mt');
const destDir = path.resolve(__dirname, '../wasm-mt');
if (!existsSync(destDir)) {
    mkdirSync(destDir, { recursive: true });
}
copyFileSync(path.join(srcDir, 'pagx-viewer.js'), path.join(destDir, 'pagx-viewer.js'));
copyFileSync(path.join(srcDir, 'pagx-viewer.wasm'), path.join(destDir, 'pagx-viewer.wasm'));
