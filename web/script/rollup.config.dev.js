import esbuild from 'rollup-plugin-esbuild';
import resolve from '@rollup/plugin-node-resolve';
import commonJs from '@rollup/plugin-commonjs';
import json from '@rollup/plugin-json';

const banner = `/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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
`;

export default [
  {
    input: 'demo/index.ts',
    output: { banner, file: 'demo/index.js', format: 'esm', sourcemap: true },
    plugins: [esbuild({ tsconfig: 'tsconfig.json', minify: false }), json(), resolve(), commonJs()],
  },
  {
    input: 'demo/worker.ts',
    output: { banner, file: 'demo/worker.js', format: 'esm', sourcemap: true },
    plugins: [esbuild({ tsconfig: 'tsconfig.json', minify: false }), json(), resolve(), commonJs()],
  },
  {
    input: 'src/pag.ts',
    output: { name: 'libpag', banner, file: 'demo/libpag.js', format: 'umd', exports: 'named', sourcemap: true },
    plugins: [esbuild({ tsconfig: 'tsconfig.json', minify: false }), json(), resolve(), commonJs()],
  },
];
