import path from 'path';
import commonJs from '@rollup/plugin-commonjs';
import esbuild from 'rollup-plugin-esbuild';
import resolve from '@rollup/plugin-node-resolve';
import replaceFunc from './plugin/rollup-plugin-replace';
import alias from '@rollup/plugin-alias';

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
    input: 'src/wechat/pag.ts',
    output: [
      { banner, file: 'demo/wechat-miniprogram/utils/libpag.js', format: 'cjs', exports: 'auto', sourcemap: false },
    ],
    plugins: [
      esbuild({ tsconfig: 'tsconfig.json', minify: false }),
      resolve({ extensions: ['.ts', '.js'] }),
      commonJs(),
      replaceFunc(),
      alias({
        entries: [{ find: '@tgfx', replacement: path.resolve(__dirname, '../../tgfx/web/src') }],
      }),
    ],
  },
];
