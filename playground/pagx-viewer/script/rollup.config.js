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

import esbuild from 'rollup-plugin-esbuild';
import resolve from '@rollup/plugin-node-resolve';
import commonJs from '@rollup/plugin-commonjs';
import alias from '@rollup/plugin-alias';
import terser from '@rollup/plugin-terser';
import path from 'path';
import { readFileSync, copyFileSync, mkdirSync, existsSync } from 'node:fs';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const fileHeaderPath = path.resolve(__dirname, '../../../.idea/fileTemplates/includes/PAG File Header.h');
const banner = readFileSync(fileHeaderPath, 'utf-8');

// ARCH selects the wasm output flavor: 'wasm-mt' (multi-threaded) or 'wasm' (single-threaded).
// Both flavors publish to the same lib/ directory; the multi-threaded build keeps the
// canonical filenames (pagx-viewer.umd.js, pagx-viewer.wasm), while the single-threaded
// build adds a `.st` infix (pagx-viewer.st.umd.js, pagx-viewer.st.wasm) to disambiguate.
const SUPPORTED_ARCHS = ['wasm-mt', 'wasm'];
const arch = process.env.ARCH || 'wasm-mt';
if (!SUPPORTED_ARCHS.includes(arch)) {
  throw new Error(`Unsupported ARCH: ${arch}. Expected one of ${SUPPORTED_ARCHS.join(', ')}.`);
}
const nameInfix = arch === 'wasm-mt' ? '' : '.st';
const libDir = path.resolve(__dirname, '../lib');

// Copy the wasm next to the bundle. For the single-threaded build, the wasm is renamed
// with the `.st` infix; the emcc glue's hard-coded `new URL("pagx-viewer.wasm", ...)` is
// rewritten to the matching name later by script/fix-wasm-imports.js.
const copyWasmPlugin = {
  name: 'copy-wasm',
  writeBundle() {
    const wasmDir = path.resolve(__dirname, `../${arch}`);

    if (!existsSync(libDir)) {
      mkdirSync(libDir, { recursive: true });
    }

    // Copy wasm file
    const wasmFile = path.join(wasmDir, 'pagx-viewer.wasm');
    if (existsSync(wasmFile)) {
      copyFileSync(wasmFile, path.join(libDir, `pagx-viewer${nameInfix}.wasm`));
    }
  },
};

const plugins = [
  // MUST stay before resolve()/esbuild() — see PR #3436. Otherwise node-resolve will
  // resolve the `pagx-glue` import to a concrete file before alias rewrites it, and
  // the single-threaded bundle will silently inline the multi-threaded glue.
  alias({
    entries: [
      { find: '@tgfx', replacement: path.resolve(__dirname, '../../../third_party/tgfx/web/src') },
      // Redirect the virtual `pagx-glue` import in pagx.ts to the requested arch's output
      // directory. The same module name is mapped via tsconfig `paths` for type generation.
      {
        find: 'pagx-glue',
        replacement: path.resolve(__dirname, `../${arch}/pagx-viewer`),
      },
    ],
  }),
  esbuild({ tsconfig: path.resolve(__dirname, '../tsconfig.json'), minify: false }),
  resolve({ extensions: ['.ts', '.js'] }),
  commonJs(),
  {
    name: 'preserve-import-meta-url',
    resolveImportMeta(property) {
      if (property === 'url') {
        return 'import.meta.url';
      }
      return null;
    },
  },
];

const input = path.resolve(__dirname, '../src/ts/pagx.ts');

// UMD format (for script tag usage, mounts to window.pagxViewer)
const umdConfig = {
  input,
  output: {
    name: 'pagxViewer',
    banner,
    format: 'umd',
    exports: 'named',
    sourcemap: true,
    file: path.join(libDir, `pagx-viewer${nameInfix}.umd.js`),
  },
  plugins: [...plugins],
};

// UMD minified
const umdMinConfig = {
  input,
  output: {
    name: 'pagxViewer',
    banner,
    format: 'umd',
    exports: 'named',
    sourcemap: true,
    file: path.join(libDir, `pagx-viewer${nameInfix}.min.js`),
  },
  plugins: [...plugins, terser()],
};

// ESM and CJS formats
const moduleConfig = {
  input,
  output: [
    { banner, file: path.join(libDir, `pagx-viewer${nameInfix}.esm.js`), format: 'esm', sourcemap: true },
    { banner, file: path.join(libDir, `pagx-viewer${nameInfix}.cjs.js`), format: 'cjs', exports: 'named', sourcemap: true },
  ],
  plugins: [...plugins, copyWasmPlugin],
};

export default [umdConfig, umdMinConfig, moduleConfig];
