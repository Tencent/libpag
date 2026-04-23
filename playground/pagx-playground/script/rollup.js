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
import {readFileSync, readdirSync, unlinkSync, copyFileSync, mkdirSync, existsSync} from "node:fs";
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

// Path: playground/pagx-playground/script -> libpag root
const libpagRoot = path.resolve(__dirname, '../../..');
const playgroundRoot = path.resolve(__dirname, '..');
const fileHeaderPath = path.resolve(libpagRoot, '.idea/fileTemplates/includes/PAG File Header.h');
const banner = readFileSync(fileHeaderPath, 'utf-8');
const isRelease = process.env.BUILD_MODE === 'release';

// Clean up old source map files in release mode
if (isRelease) {
    const outputDir = path.resolve(playgroundRoot, 'public');
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

// Copy fonts and other assets after build
function copyAssetsPlugin() {
    return {
        name: 'copy-assets',
        writeBundle() {
            const publicDir = path.resolve(playgroundRoot, 'public');

            // Copy fonts from resources/font
            const fontsDir = path.join(publicDir, 'fonts');
            if (!existsSync(fontsDir)) {
                mkdirSync(fontsDir, { recursive: true });
            }
            const fontFiles = ['NotoSansSC-Regular.otf', 'NotoColorEmoji.ttf'];
            for (const font of fontFiles) {
                const fontSrc = path.resolve(libpagRoot, 'resources/font', font);
                if (existsSync(fontSrc)) {
                    copyFileSync(fontSrc, path.join(fontsDir, font));
                }
            }

            console.log('Assets copied to public/');
        }
    };
}

const plugins = [
    esbuild({tsconfig: path.resolve(__dirname, "../src/tsconfig.json"), minify: isRelease}),
    json(),
    resolve({ extensions: ['.ts', '.js'] }),
    commonJs(),
    alias({
        entries: [
            { find: '@tgfx', replacement: path.resolve(libpagRoot, 'third_party/tgfx/web/src') },
            // { find: 'pagx-viewer', replacement: path.resolve(playgroundRoot, '../pagx-viewer/src/ts/pagx.ts') },
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
    copyAssetsPlugin(),
];

export default [
    {
        input: path.resolve(__dirname, '../src/index.ts'),
        output: {
            banner,
            file: path.resolve(playgroundRoot, 'public/wasm-mt/index.js'),
            format: 'esm',
            sourcemap: !isRelease
        },
        plugins: plugins,
    }
];
