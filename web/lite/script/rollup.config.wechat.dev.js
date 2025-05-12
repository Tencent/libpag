import resolve from '@rollup/plugin-node-resolve';
import commonJs from '@rollup/plugin-commonjs';
import babel from '@rollup/plugin-babel';
import { terser } from 'rollup-plugin-terser';
import esbuild from 'rollup-plugin-esbuild';
import { readFileSync } from 'node:fs';
import path from 'path';

const fileHeaderPath = path.resolve(__dirname, '../../../.idea/fileTemplates/includes/PAG File Header.h');
const banner = readFileSync(fileHeaderPath, 'utf-8');

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
