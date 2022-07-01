/* global EmscriptenModule */

import { binding } from './binding';
import * as types from './types';
import createPAG from './wasm/libpag';
import { WebAssemblyQueue } from './utils/queue';
import { version } from '../package.json';

export interface moduleOption {
  /**
   * Link to wasm file.
   */
  locateFile?: (file: string) => string;
}

/**
 * Initialize pag webassembly module.
 */
const PAGInit = (moduleOption: moduleOption = {}): Promise<types.PAG> =>
  createPAG(moduleOption)
    .then((module: types.PAG) => {
      binding(module);
      module.webAssemblyQueue = new WebAssemblyQueue();
      module.globalCanvas = new module.GlobalCanvas();
      module.PAGFont.registerFallbackFontNames();
      return module;
    })
    .catch((error: any) => {
      console.error(error);
      throw new Error('PAGInit fail! Please check .wasm file path valid.');
    });

export { PAGInit, types, version };
