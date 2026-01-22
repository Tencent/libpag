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

import esbuild from 'rollup-plugin-esbuild';
import resolve from '@rollup/plugin-node-resolve';
import commonJs from '@rollup/plugin-commonjs';
import json from '@rollup/plugin-json';
import alias from '@rollup/plugin-alias';
import path from "path";
import { readFileSync } from "node:fs";
import { fileURLToPath } from 'url';
import replaceFunc from './plugin/rollup-plugin-replace';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const fileHeaderPath = path.resolve(__dirname, '../../../.idea/fileTemplates/includes/PAG File Header.h');
const banner = readFileSync(fileHeaderPath, 'utf-8');
const isRelease = process.env.BUILD_MODE === 'release';

const plugins = [
  esbuild({ tsconfig: path.resolve(__dirname, "../tsconfig.json"), minify: isRelease }),
  json(),
  alias({
    entries: [{ find: '@tgfx', replacement: path.resolve(__dirname, '../../../third_party/tgfx/web/src') }],
  }),
  resolve({ extensions: ['.mjs', '.js', '.ts', '.json'] }),
  commonJs(),
  replaceFunc(),
  {
    name: 'preserve-import-meta-url',
    resolveImportMeta(property, options) {
      // Preserve the original behavior of `import.meta.url`.
      if (property === 'url') {
        return 'import.meta.url';
      }
      return null;
    },
  },
];

export default [
  {
    input: path.resolve(__dirname, '../ts/pagx.ts'),
    output: {
      banner,
      file: path.resolve(__dirname, '../ts/wasm/pagx-viewer.js'),
      format: 'esm',
      sourcemap: !isRelease
    },
    plugins: plugins,
  },
  {
    input: path.resolve(__dirname, '../ts/gesture-manager.ts'),
    output: {
      banner,
      file: path.resolve(__dirname, '../wx_demo/utils/gesture-manager.js'),
      format: 'esm',
      sourcemap: !isRelease
    },
    plugins: plugins,
  }
];