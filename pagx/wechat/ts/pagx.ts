import './babel';
import * as types from './types';
import createPAG from '../wasm/pagx-viewer';
import { PAGX } from './types';
import { binding } from './binding';

export interface moduleOption {
  /**
   * Link to wasm file.
   */
  locateFile?: (file: string) => string;
}

/**
 * Initialize PAGX webassembly module.
 */
const PAGXInit = (moduleOption: moduleOption = {}): Promise<types.PAGX> =>
  createPAG(moduleOption)
    .then((module: types.PAGX) => {
      binding(module);
      return module;
    })
    .catch((error: any) => {
      console.error(error);
      throw new Error('PAGXInit fail! Please check .wasm file path valid.');
    });

export { PAGXInit, types };