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

// Rollup config for pagx-player.
//
// Output: single ESM bundle at lib/pagx-player.esm.js + typings at lib/pagx-player.d.ts.
// The player interacts with the underlying wasm backend through a structural PlayerView /
// PlayerModule contract declared inside this package (src/pagx-view-types.ts), so the bundle
// carries no runtime or type reference to pagx-viewer at all - hosts inject the actual
// implementation via the `moduleFactory` option. CodeMirror packages are bundled (declared as
// regular dependencies in package.json) so the editor works without asking hosts to pin
// matching CodeMirror versions.

import { nodeResolve } from '@rollup/plugin-node-resolve';
import commonjs from '@rollup/plugin-commonjs';
import esbuild from 'rollup-plugin-esbuild';
import dts from 'rollup-plugin-dts';

const isRelease = process.env.BUILD_MODE === 'release';

export default [
    {
        input: 'src/index.ts',
        output: {
            file: 'lib/pagx-player.esm.js',
            format: 'esm',
            sourcemap: !isRelease,
        },
        plugins: [
            nodeResolve({ browser: true }),
            commonjs(),
            esbuild({
                target: 'es2020',
                minify: isRelease,
                sourceMap: !isRelease,
                tsconfig: './tsconfig.json',
            }),
        ],
    },
    {
        input: 'src/index.ts',
        output: {
            file: 'lib/pagx-player.d.ts',
            format: 'esm',
        },
        plugins: [
            dts({
                tsconfig: './tsconfig.json',
            }),
        ],
    },
];
