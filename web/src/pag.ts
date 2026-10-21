import './utils/polyfills';
import { PAGBind } from './binding';
import * as types from './types';
import createPAG from './wasm-mt-webgpu/libpag';
import { WebAssemblyQueue } from './utils/queue';

export interface ModuleOption {
  /**
   * Link to wasm file.
   */
  locateFile?: (file: 'libpag.wasm') => string;
  /**
   * WebGPU build only. Provide a host-created GPUDevice so that libpag shares it with the host
   * engine. This is required when wrapping host textures via PAGSurface.fromTexture /
   * PAGImage.fromTexture, because WGPUTexture carries no device back-reference. If omitted,
   * libpag requests a device via navigator.gpu on its own.
   */
  webGPUDevice?: object;
}

// Minimal WebGPU typings. lib.dom of TypeScript ~5.0 does not ship WebGPU declarations.
interface WebGPUAdapterLike {
  requestDevice: () => Promise<object>;
}

interface NavigatorWithGPU extends Navigator {
  gpu?: {
    requestAdapter: () => Promise<WebGPUAdapterLike | null>;
  };
}

const initWebGPUDevice = async (module: types.PAG, webGPUDevice?: object): Promise<void> => {
  if (webGPUDevice) {
    module.preinitializedWebGPUDevice = webGPUDevice;
    return;
  }
  const nav = navigator as NavigatorWithGPU;
  if (typeof navigator === 'undefined' || !nav.gpu) {
    throw new Error('WebGPU is not supported in this browser. Please use the WebGL build of libpag.');
  }
  const adapter = await nav.gpu.requestAdapter();
  if (!adapter) {
    throw new Error('WebGPU adapter request failed. Please use the WebGL build of libpag.');
  }
  module.preinitializedWebGPUDevice = await adapter.requestDevice();
};

/**
 * Initialize pag webassembly module.
 */
const PAGInit = (moduleOption: ModuleOption = {}): Promise<types.PAG> =>
  createPAG(moduleOption)
    .catch((error: any) => {
      console.error(error);
      throw new Error('PAGInit fail! Please check .wasm file path valid.');
    })
    .then(async (module: types.PAG) => {
      if (module._IsWebGPUBackend()) {
        // The GPUDevice must be preinitialized on the JS side before any rendering call,
        // because tgfx picks it up synchronously via emscripten_webgpu_get_device().
        await initWebGPUDevice(module, moduleOption.webGPUDevice);
      }
      PAGBind(module);
      module.webAssemblyQueue = new WebAssemblyQueue();
      module.globalCanvas = new module.GlobalCanvas();
      module.PAGFont.registerFallbackFontNames();
      return module;
    });

export { PAGInit, types };
