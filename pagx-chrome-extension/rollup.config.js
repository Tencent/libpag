import esbuild from 'rollup-plugin-esbuild';
import resolve from '@rollup/plugin-node-resolve';
import commonJs from '@rollup/plugin-commonjs';
import alias from '@rollup/plugin-alias';
import path from 'path';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

export default {
  input: path.resolve(__dirname, 'viewer/viewer.ts'),
  output: {
    file: path.resolve(__dirname, 'viewer/viewer.js'),
    format: 'esm',
    sourcemap: false
  },
  plugins: [
    esbuild({
      tsconfig: path.resolve(__dirname, 'tsconfig.json'),
      minify: true
    }),
    resolve({ extensions: ['.ts', '.js'] }),
    commonJs(),
    alias({
      entries: [
        { find: '@tgfx', replacement: path.resolve(__dirname, '../third_party/tgfx/web/src') }
      ],
    }),
  ],
};
