import resolve from '@rollup/plugin-node-resolve';
import commonJs from '@rollup/plugin-commonjs';
import json from '@rollup/plugin-json';
import { terser } from 'rollup-plugin-terser';
import esbuild from 'rollup-plugin-esbuild';

const umdConfig = {
  input: 'src/main.ts',
  output: [
    {
      name: 'tgfx',
      format: 'umd',
      exports: 'named',
      sourcemap: true,
      file: 'lib/tgfx.js',
    },
  ],
  plugins: [
    esbuild({
      tsconfig: 'tsconfig.json',
      minify: false,
    }),
    json(),
    resolve(),
    commonJs(),
  ],
};

const umdMinConfig = {
  input: 'src/main.ts',
  output: [
    {
      name: 'tgfx',
      format: 'umd',
      exports: 'named',
      sourcemap: true,
      file: 'lib/tgfx.min.js',
    },
  ],
  plugins: [
    esbuild({
      tsconfig: 'tsconfig.json',
      minify: false,
    }),
    json(),
    resolve(),
    commonJs(),
    terser(),
  ],
};

export default [
  umdConfig,
  umdMinConfig,
  {
    input: 'src/main.ts',
    output: [
      { file: 'lib/tgfx.esm.js', format: 'esm', sourcemap: true },
      { file: 'lib/tgfx.cjs.js', format: 'cjs', exports: 'auto', sourcemap: true },
    ],
    plugins: [
      esbuild({
        tsconfig: 'tsconfig.json',
        minify: false
      }),
      json(),
      resolve(),
      commonJs()
    ],
  },
];
