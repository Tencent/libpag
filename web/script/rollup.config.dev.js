import path from 'path';
import esbuild from 'rollup-plugin-esbuild';
import resolve from '@rollup/plugin-node-resolve';
import commonJs from '@rollup/plugin-commonjs';
import json from '@rollup/plugin-json';
import alias from '@rollup/plugin-alias';
import { readFileSync } from 'node:fs';

const fileHeaderPath = path.resolve(__dirname, '../../.idea/fileTemplates/includes/PAG File Header.h');
const banner = readFileSync(fileHeaderPath, 'utf-8');

const plugins = [
  esbuild({ tsconfig: "tsconfig.json", minify: false }),
  json(),
  resolve({ extensions: ['.ts', '.js'] }),
  commonJs(),
  alias({
    entries: [{ find: '@tgfx', replacement: path.resolve(__dirname, '../../third_party/tgfx/web/src') }],
  }),
];

export default [
  {
    input: 'demo/index.ts',
    output: { banner, file: 'demo/index.js', format: 'esm', sourcemap: true },
    plugins: plugins,
  },
  {
    input: 'demo/worker.ts',
    output: { banner, file: 'demo/worker.js', format: 'esm', sourcemap: true },
    plugins: plugins,
  },
  {
    input: 'src/pag.ts',
    output: { name: 'libpag', banner, file: 'demo/libpag.js', format: 'umd', exports: 'named', sourcemap: true },
    plugins: plugins,
  },
];
