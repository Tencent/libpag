import path from 'path';
import commonJs from '@rollup/plugin-commonjs';
import esbuild from 'rollup-plugin-esbuild';
import resolve from '@rollup/plugin-node-resolve';
import replaceFunc from './plugin/rollup-plugin-replace';
import alias from '@rollup/plugin-alias';
import { readFileSync } from 'node:fs';

const fileHeaderPath = path.resolve(__dirname, '../../.idea/fileTemplates/includes/PAG File Header.h');
const banner = readFileSync(fileHeaderPath, 'utf-8');

export default [
  {
    input: 'src/wechat/pag.ts',
    output: [{ banner, file: 'wechat/lib/libpag.js', format: 'cjs', exports: 'auto', sourcemap: true }],
    plugins: [
      esbuild({ tsconfig: 'tsconfig.json', minify: false }),
      resolve({ extensions: ['.ts', '.js'] }),
      commonJs(),
      replaceFunc(),
      alias({
        entries: [{ find: '@tgfx', replacement: path.resolve(__dirname, '../../third_party/tgfx/web/src') }],
      }),
      {
        name: 'preserve-import-meta-url',
        resolveImportMeta(property, options) {
          // Preserve the original behavior of `import.meta.url`.
          if (property === 'url') {
            return 'import.meta.url';
          }
          return null;
        },
      },
    ],
  },
];
