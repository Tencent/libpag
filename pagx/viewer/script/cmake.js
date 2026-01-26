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
import { createRequire } from 'module';
import path from 'path';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const require = createRequire(import.meta.url);

process.chdir(__dirname);

process.argv.push("-s");
process.argv.push("../");
process.argv.push("-o");
process.argv.push("./");
process.argv.push("-p");
process.argv.push("web");
process.argv.push("pagx-viewer");

// Use vendor_tools from libpag
require("../../../third_party/vendor_tools/lib-build");

// Copy wasm files to viewer/wasm-mt/ directory
const srcDir = path.resolve(__dirname, 'wasm-mt');
const destDir = path.resolve(__dirname, '../wasm-mt');
if (!existsSync(destDir)) {
    mkdirSync(destDir, { recursive: true });
}
copyFileSync(path.join(srcDir, 'pagx-viewer.js'), path.join(destDir, 'pagx-viewer.js'));
copyFileSync(path.join(srcDir, 'pagx-viewer.wasm'), path.join(destDir, 'pagx-viewer.wasm'));

// Copy font files to viewer/fonts/ directory
const fontSrcDir = path.resolve(__dirname, '../../../third_party/tgfx/resources/font');
const fontDestDir = path.resolve(__dirname, '../fonts');
if (!existsSync(fontDestDir)) {
    mkdirSync(fontDestDir, { recursive: true });
}
const fontFiles = ['NotoSansSC-Regular.otf', 'NotoColorEmoji.ttf'];
for (const fontFile of fontFiles) {
    const srcPath = path.join(fontSrcDir, fontFile);
    if (existsSync(srcPath)) {
        copyFileSync(srcPath, path.join(fontDestDir, fontFile));
        console.log(`Copied font: ${fontFile}`);
    }
}
