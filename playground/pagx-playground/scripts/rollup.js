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
import json from '@rollup/plugin-json';
import alias from '@rollup/plugin-alias';
import path from "path";
import {readFileSync, readdirSync, unlinkSync, existsSync} from "node:fs";
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

// Path: playground/pagx-playground/scripts -> libpag root
const libpagRoot = path.resolve(__dirname, '../../..');
const playgroundRoot = path.resolve(__dirname, '..');
const fileHeaderPath = path.resolve(libpagRoot, '.idea/fileTemplates/includes/PAG File Header.h');
// The IntelliJ file template is not guaranteed to exist (e.g. in fresh clones or CI minimal
// checkouts). Fall back to an empty banner rather than failing the build.
const banner = existsSync(fileHeaderPath) ? readFileSync(fileHeaderPath, 'utf-8') : '';
const isRelease = process.env.BUILD_MODE === 'release';

// Clean up old source map files in release mode
if (isRelease) {
    const outputDir = path.resolve(playgroundRoot, 'wasm-mt');
    try {
        const files = readdirSync(outputDir);
        for (const file of files) {
            if (file.endsWith('.map')) {
                unlinkSync(path.join(outputDir, file));
            }
        }
    } catch (e) {
        // Directory may not exist yet
    }
}

const plugins = [
    esbuild({tsconfig: path.resolve(__dirname, "../src/tsconfig.json"), minify: isRelease}),
    json(),
    resolve({ extensions: ['.ts', '.js'] }),
    commonJs(),
    alias({
        entries: [
            { find: '@tgfx', replacement: path.resolve(libpagRoot, 'third_party/tgfx/web/src') },
        ],
    }),
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

export default [
    {
        input: path.resolve(__dirname, '../src/index.ts'),
        output: {
            banner,
            file: path.resolve(playgroundRoot, 'wasm-mt/index.js'),
            format: 'esm',
            sourcemap: !isRelease
        },
        plugins: plugins,
    }
];
