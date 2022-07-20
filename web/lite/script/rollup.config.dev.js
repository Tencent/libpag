import esbuild from 'rollup-plugin-esbuild';
import json from '@rollup/plugin-json';
import resolve from '@rollup/plugin-node-resolve';
import commonJs from '@rollup/plugin-commonjs';
import babel from '@rollup/plugin-babel';
import serve from 'rollup-plugin-serve';

export default {
  input: 'demo/index.ts',
  output: { format: 'umd', exports: 'named', sourcemap: true, file: 'demo/index.js' },
  plugins: [
    esbuild({ tsconfig: 'tsconfig.json', minify: false }),
    json(),
    resolve(),
    commonJs(),
    babel({
      babelHelpers: 'bundled',
      exclude: 'node_modules/**',
    }),
    serve({ host: 'localhost', port: 8080, contentBase: './demo' }),
  ],
};
