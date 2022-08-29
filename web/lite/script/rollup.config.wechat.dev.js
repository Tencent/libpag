import resolve from '@rollup/plugin-node-resolve';
import commonJs from '@rollup/plugin-commonjs';
import babel from '@rollup/plugin-babel';
import { terser } from 'rollup-plugin-terser';
import esbuild from 'rollup-plugin-esbuild';

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

const plugins = (needBabel = false, needTerser = false) => {
  const list = [esbuild({ tsconfig: './tsconfig.json', minify: false }), resolve(), commonJs()];
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

const cjsConfig = {
  input: 'src/wechat/pag.ts',
  output: {
    banner,
    file: 'demo/wechat-miniprogram/utils/pag.js',
    format: 'cjs',
    exports: 'auto',
    sourcemap: true,
  },
  plugins: plugins(false, false),
};

export default [cjsConfig];
