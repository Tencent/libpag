import resolve from '@rollup/plugin-node-resolve';
import commonJs from '@rollup/plugin-commonjs';
import json from '@rollup/plugin-json';
import babel from '@rollup/plugin-babel';
import { terser } from 'rollup-plugin-terser';
import esbuild from 'rollup-plugin-esbuild';
import pkg from '../package.json';
import { readFileSync } from 'node:fs';
import path from 'path';

const fileHeaderPath = path.resolve(__dirname, '../../../.idea/fileTemplates/includes/PAG File Header.h');
const banner = readFileSync(fileHeaderPath, 'utf-8');

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
