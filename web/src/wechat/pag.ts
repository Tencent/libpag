/* global EmscriptenModule */
import '../utils/polyfills';
import './babel';
import { binding } from './binding';
import * as types from '../types';
import { WebAssemblyQueue } from '../utils/queue';
import createPAG from '../wasm/libpag';
import { clearDirectory } from './file-utils';
import { MP4_CACHE_PATH } from './constant';

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

const clearCache = () => {
  return clearDirectory(MP4_CACHE_PATH);
};

export { PAGInit, types, clearCache };
