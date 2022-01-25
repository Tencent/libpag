import resolve from '@rollup/plugin-node-resolve';
import commonJs from '@rollup/plugin-commonjs';
import json from '@rollup/plugin-json';
import { terser } from 'rollup-plugin-terser';
import esbuild from 'rollup-plugin-esbuild';
import jscc from 'rollup-plugin-jscc';
import replaceFunc from './plugin/rollup-plugin-replace';

import pkg from '../package.json';

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

const umdConfig = {
  input: 'src/pag.ts',
  output: [
    {
      name: 'libpag',
      banner,
      format: 'umd',
      exports: 'named',
      sourcemap: true,
      file: pkg.browser,
    },
  ],
  plugins: [esbuild({ tsconfig: 'tsconfig.json', minify: false }), json(), resolve(), commonJs()],
};

const umdMinConfig = {
  input: 'src/pag.ts',
  output: [
    {
      name: 'libpag',
      banner,
      format: 'umd',
      exports: 'named',
      sourcemap: true,
      file: 'lib/libpag.min.js',
    },
  ],
  plugins: [esbuild({ tsconfig: 'tsconfig.json', minify: false }), json(), resolve(), commonJs(), terser()],
};

export default [
  umdConfig,
  umdMinConfig,
  {
    input: 'src/pag.ts',
    output: [
      { banner, file: pkg.module, format: 'esm', sourcemap: true },
      { banner, file: pkg.main, format: 'cjs', exports: 'auto', sourcemap: true },
    ],
    plugins: [esbuild({ tsconfig: 'tsconfig.json', minify: false }), resolve(), commonJs()],
  },
  {
    input: 'src/pag.ts',
    output: [{ banner, file: 'lib/libpag.wx.js', format: 'cjs', exports: 'auto', sourcemap: true }],
    plugins: [
      jscc({ values: { _WECHAT: 1 } }),
      esbuild({ tsconfig: 'tsconfig.json', minify: false }),
      resolve(),
      commonJs(),
      replaceFunc(),
    ],
  },
];
