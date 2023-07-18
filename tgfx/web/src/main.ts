import { binding } from './binding';
import * as types from './types';
import createTGFX from './wasm/tgfx';

export interface ModuleOption {
  /**
   * Link to wasm file.
   */
  locateFile?: (file: 'tgfx.wasm') => string;
}

const TGFXInit = (moduleOption: ModuleOption = {}): Promise<types.TGFX> =>
  createTGFX(moduleOption)
    .then((module: types.TGFX) => {
      binding(module);
      return module;
    })
    .catch((error: any) => {
      console.error(error);
      throw new Error('TGFXInit fail! Please check .wasm file path valid.');
    });

export { binding, TGFXInit, types };
