import { getPAGModule } from '../pag-module';
import { isWebGPUBackend } from './backend-context';

/**
 * Minimal structure of a GPUTexture required when wrapping a host texture on the WebGPU backend.
 * lib.dom of TypeScript ~5.0 does not ship WebGPU declarations, so a structural type is used;
 * a real GPUTexture satisfies it.
 */
export interface WebGPUTextureLike {
  readonly format: string;
  /**
   * GPUTexture.usage bitmask. Optional in the structural type, but required by
   * PAGSurface.fromTexture(), which rejects textures without the COPY_SRC flag.
   */
  readonly usage?: number;
}

interface WebGPUObjectManager {
  create: (object: object) => number;
  release: (id: number) => void;
}

const getWebGPURuntime = (): { mgrTexture?: WebGPUObjectManager; TextureFormat?: string[] } | undefined => {
  return getPAGModule()?.WebGPU;
};

export interface ResolvedTextureHandle {
  // Value to pass to the wasm _FromTexture entry.
  textureID: number;
  // WGPUTextureFormat value on the WebGPU backend; 0 on the WebGL backend.
  format: number;
  // Registered WebGPU handle to release via unregisterWebGPUTexture() on destroy; 0 otherwise.
  webGPUHandle: number;
}

/**
 * Resolves the texture argument of PAGSurface.fromTexture()/PAGImage.fromTexture() into wasm-side
 * values. On the WebGL backend only numeric handles registered via BackendContext.registerTexture()
 * are accepted; on the WebGPU backend only GPUTexture objects are accepted and get registered in
 * Emscripten's WebGPU runtime. Throws an apiName-prefixed error describing the exact failure.
 */
export const resolveTextureHandle = (
  apiName: string,
  texture: number | WebGPUTextureLike,
): ResolvedTextureHandle => {
  if (typeof texture === 'number') {
    if (isWebGPUBackend()) {
      throw new Error(`${apiName} requires a GPUTexture on the WebGPU backend.`);
    }
    return { textureID: texture, format: 0, webGPUHandle: 0 };
  }
  const webGPU = getWebGPURuntime();
  if (!webGPU?.mgrTexture || !webGPU.TextureFormat) {
    throw new Error(`${apiName} failed to register the GPUTexture because the WebGPU runtime is unavailable.`);
  }
  // The index of a format name in TextureFormat equals its WGPUTextureFormat value, which is
  // enforced by a static_assert in PAGWasmBindings.cpp.
  const format = webGPU.TextureFormat.indexOf(texture.format);
  if (format <= 0) {
    throw new Error(`${apiName} does not support the GPUTexture format: ${texture.format}.`);
  }
  const handle = webGPU.mgrTexture.create(texture);
  return { textureID: handle, format, webGPUHandle: handle };
};

/**
 * WebGPU backend only. Releases a texture handle previously registered by resolveTextureHandle().
 */
export const unregisterWebGPUTexture = (handle: number): void => {
  const webGPU = getWebGPURuntime();
  if (handle > 0 && webGPU?.mgrTexture) {
    webGPU.mgrTexture.release(handle);
  }
};

// Releases the WebGPU texture registration of wrappers that were garbage-collected without
// calling destroy(), e.g. when a new wrapper from PAGPlayer.getSurface() replaced the original
// one. The registry entry holds a strong reference to the host GPUTexture, so leaking it leaks
// the texture itself. The callback timing is up to the GC; destroy() remains the primary
// release path.
const textureHandleRegistry =
  typeof FinalizationRegistry === 'undefined'
    ? null
    : new FinalizationRegistry<number>((handle) => unregisterWebGPUTexture(handle));

/**
 * WebGPU backend only. Registers the wrapper so its texture handle gets released if the wrapper
 * is garbage-collected without calling destroy(). Pair with untrackWebGPUTextureHandle() when
 * the handle is released explicitly.
 */
export const trackWebGPUTextureHandle = (wrapper: object, handle: number): void => {
  textureHandleRegistry?.register(wrapper, handle, wrapper);
};

/**
 * WebGPU backend only. Cancels the GC-time release registered by trackWebGPUTextureHandle().
 */
export const untrackWebGPUTextureHandle = (wrapper: object): void => {
  textureHandleRegistry?.unregister(wrapper);
};

/**
 * Implemented by wrappers (PAGSurface, PAGImage) whose fromTexture() factory registers a host
 * GPUTexture. The handle is attached by wrapBackendTexture() after construction instead of
 * flowing through the public constructor, so external code cannot inject an arbitrary handle
 * that destroy() would then release by mistake.
 */
export interface WebGPUTextureHolder {
  attachWebGPUTextureHandle: (handle: number) => void;
}

/**
 * Shared implementation of the fromTexture() factories of PAGSurface and PAGImage: resolves the
 * host texture, wraps it into a wasm instance, rolls the registration back on failure, and
 * tracks the wrapper for GC-time release of the WebGPU texture handle.
 */
export const wrapBackendTexture = <T extends WebGPUTextureHolder>(
  apiName: string,
  texture: number | WebGPUTextureLike,
  makeWasmIns: (textureID: number, format: number) => any,
  makeWrapper: (wasmIns: any) => T,
): T => {
  const { textureID, format, webGPUHandle } = resolveTextureHandle(apiName, texture);
  const wasmIns = makeWasmIns(textureID, format);
  if (!wasmIns) {
    if (webGPUHandle > 0) unregisterWebGPUTexture(webGPUHandle);
    throw new Error(`${apiName} failed to wrap the texture ${textureID}!`);
  }
  const wrapper = makeWrapper(wasmIns);
  if (webGPUHandle > 0) {
    wrapper.attachWebGPUTextureHandle(webGPUHandle);
    trackWebGPUTextureHandle(wrapper, webGPUHandle);
  }
  return wrapper;
};
