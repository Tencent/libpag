import path from 'path';
import resolve from '@rollup/plugin-node-resolve';
import commonJs from '@rollup/plugin-commonjs';
import json from '@rollup/plugin-json';
import { terser } from 'rollup-plugin-terser';
import esbuild from 'rollup-plugin-esbuild';
import alias from '@rollup/plugin-alias';
import { readFileSync } from 'node:fs';

const fileHeaderPath = path.resolve(__dirname, '../../.idea/fileTemplates/includes/PAG File Header.h');
const banner = readFileSync(fileHeaderPath, 'utf-8');
const { backend, libDir, wasmDir } = require('./targets').resolveTarget(process.env.ARCH, process.env.BACKEND);

require("./update.pag.import");
require("./gen.demo.html");

// PAGInit runs after script evaluation, when document.currentScript is already null.
const captureScriptUrl = `const __pagScriptUrl = typeof document !== 'undefined' && document.currentScript
  ? document.currentScript.src : typeof location !== 'undefined' ? location.href : undefined;`;

const plugins = [
  esbuild({
    tsconfig: 'tsconfig.json',
    minify: false,
    define: {
      __PAG_DEMO_LIB_DIR__: JSON.stringify(libDir),
      __PAG_DEMO_BACKEND__: JSON.stringify(backend),
    },
  }),
  json(),
  resolve({ extensions: ['.ts', '.js'] }),
  commonJs(),
  alias({
    entries: [{ find: '@tgfx', replacement: path.resolve(__dirname, '../../third_party/tgfx/web/src') }],
  }),
  {
    name: 'preserve-import-meta-url',
    resolveImportMeta(property, options) {
      if (property === 'url') {
        if (options.format === 'es') {
          return 'import.meta.url';
        }
        if (options.format === 'umd') {
          return '__pagScriptUrl';
        }
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
      intro: captureScriptUrl,
      exports: 'named',
      sourcemap: true,
      file: `${libDir}/libpag.umd.js`,
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
      intro: captureScriptUrl,
      exports: 'named',
      sourcemap: true,
      file: `${libDir}/libpag.min.js`,
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
      { banner, file: `${libDir}/libpag.esm.js`, format: 'esm', sourcemap: true },
      { banner, file: `${libDir}/libpag.cjs.js`, format: 'cjs', exports: 'named', sourcemap: true },
    ],
    plugins: [...plugins],
  },
  {
    input: 'demo/index.ts',
    output: { banner, file: `demo/${wasmDir}/libpag.js`, format: 'esm', sourcemap: true },
    plugins: plugins,
  },
];
