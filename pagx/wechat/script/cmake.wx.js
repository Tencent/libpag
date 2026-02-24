#!/usr/bin/env node
/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making tgfx available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the BSD 3-Clause License (the "License"); you may not use this file except
//  in compliance with the License. You may obtain a copy of the License at
//
//      https://opensource.org/licenses/BSD-3-Clause
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

import { fileURLToPath } from 'url';
import path from 'path';
import fs, {copyFileSync} from "fs";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

process.chdir(__dirname);

process.argv.push("-s");
process.argv.push("../");
process.argv.push("-o");
process.argv.push("../");
process.argv.push("-p");
process.argv.push("web");
process.argv.push("pagx-viewer");

await import('./setup.emsdk.wx.js');
await import("../../../third_party/vendor_tools/lib-build");


if (!fs.existsSync("../ts/wasm")) {
    fs.mkdirSync("../ts/wasm", {recursive: true});
}

fs.copyFileSync("../wasm/pagx-viewer.wasm", "../ts/wasm/pagx-viewer.wasm")
