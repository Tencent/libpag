import path from 'path';
import resolve from '@rollup/plugin-node-resolve';
import commonJs from '@rollup/plugin-commonjs';
import json from '@rollup/plugin-json';
import { terser } from 'rollup-plugin-terser';
import esbuild from 'rollup-plugin-esbuild';
import alias from '@rollup/plugin-alias';
import pkg from '../package.json';
import { readFileSync } from 'node:fs';

const fileHeaderPath = path.resolve(__dirname, '../../.idea/fileTemplates/includes/PAG File Header.h');
const banner = readFileSync(fileHeaderPath, 'utf-8');
const arch = process.env.ARCH;
const libPath = (arch === 'wasm-mt' ? "lib-mt" : "lib");
const demoName = (arch === 'wasm-mt'? 'index': 'index-st');

require("./update.pag.import");


const plugins = [
  esbuild({ tsconfig: 'tsconfig.json', minify: false }),
  json(),
  resolve({ extensions: ['.ts', '.js'] }),
  commonJs(),
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
];


const umdConfig = {
  input: 'src/pag.ts',
  output: [
    {
      name: 'libpag',
      banner,
      format: 'umd',
      exports: 'named',
      sourcemap: true,
      file: `${libPath}/libpag.umd.js`,
    },
  ],
  plugins: [...plugins],
};

const umdMinConfig = {
  input: 'src/pag.ts',
  output: [
    {
      name: 'libpag',
      banner,
      format: 'umd',
      exports: 'named',
      sourcemap: true,
      file: `${libPath}/libpag.min.js`,
    },
  ],
  plugins: [...plugins, terser()],
};

export default [
  umdConfig,
  umdMinConfig,
  {
    input: 'src/pag.ts',
    output: [
      { banner, file: `${libPath}/libpag.esm.js`, format: 'esm', sourcemap: true },
      { banner, file: `${libPath}/libpag.cjs.js`, format: 'cjs', exports: 'named', sourcemap: true },
    ],
    plugins: [...plugins],
  },
  {
    input: `demo/${demoName}.ts`,
    output: { banner, file: `demo/${arch}/libpag.js`, format: 'esm', sourcemap: true },
    plugins: plugins,
  },
];
