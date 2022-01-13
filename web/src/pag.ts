import { PAGFont } from './pag-font';
import { binding } from './binding';
import * as types from './types';
import createPAG from './wasm/libpag';
import { WebAssemblyQueue } from './utils/queue';

/**
 * Initialize pag webassembly module.
 */
const PAGInit = (moduleOption): Promise<types.PAG> =>
  createPAG(moduleOption).then((module: types.PAG) => {
    module.webAssemblyQueue = new WebAssemblyQueue();
    module.webAssemblyQueue.start();
    binding(module);

    PAGFont.registerFallbackFontNames();

    return module;
  });

export { PAGInit, types };
