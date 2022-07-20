import resolve from '@rollup/plugin-node-resolve';
import commonJs from '@rollup/plugin-commonjs';
import json from '@rollup/plugin-json';
import babel from '@rollup/plugin-babel';
import { terser } from 'rollup-plugin-terser';
import esbuild from 'rollup-plugin-esbuild';

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

const name = 'libpag';
const plugins = (needBabel = false, needTerser = false) => {
  const list = [esbuild({ tsconfig: './tsconfig.json', minify: false }), json(), resolve(), commonJs()];
  if (needBabel) {
    list.push(
      babel({
        babelHelpers: 'bundled',
        exclude: 'node_modules/**',
        extensions: ['.js', '.ts'],
      }),
    );
  }
  if (needTerser) {
    list.push(terser());
  }
  return list;
};

const umdConfig = {
  input: 'src/pag.ts',
  output: [
    {
      name,
      banner,
      format: 'umd',
      exports: 'named',
      sourcemap: true,
      file: pkg.browser,
    },
  ],
  plugins: plugins(true, false),
};

const umdMinConfig = {
  input: 'src/pag.ts',
  output: {
    name,
    banner,
    format: 'umd',
    exports: 'named',
    sourcemap: true,
    file: 'lib/pag.min.js',
  },
  plugins: plugins(false, true),
};

const umdBabelConfig = {
  input: 'src/pag.ts',
  output: {
    name,
    banner,
    format: 'umd',
    exports: 'named',
    sourcemap: true,
    file: 'lib/pag.babel.js',
  },
  plugins: plugins(true, true),
};

export default [
  {
    input: 'src/pag.ts',
    output: [
      { banner, file: pkg.main, format: 'cjs', exports: 'auto', sourcemap: true },
      { banner, file: pkg.module, format: 'esm', sourcemap: true },
    ],
    plugins: plugins(),
  },
  umdConfig,
  umdMinConfig,
  umdBabelConfig,
];
