import { getPAGModule, PAGModule } from '../pag-module';
import { WEBGL_CONTEXT_ATTRIBUTES } from '../constant';
import { isInstanceOf } from '../utils/type-utils';

import type { PAG } from '../types';

// The backend is fixed per wasm build, so the result is cached after the first successful read
// to avoid a cross-wasm call on every query (flush() asks once per frame).
let cachedIsWebGPUBackend: boolean | null = null;

/**
 * Returns whether the given module is a WebGPU-backend build, or null when the module does not
 * carry the backend query entry (not initialized yet or not a pag module). Used by PAGInit before
 * the module is registered; everywhere else use isWebGPUBackend().
 */
export const isModuleWebGPUBackend = (module: PAG | undefined): boolean | null => {
  if (!module || typeof module._IsWebGPUBackend !== 'function') return null;
  return module._IsWebGPUBackend();
};

/**
 * Returns true when the loaded wasm module is built with the WebGPU backend. Internal helper
 * for branching backend-specific behavior in the TS layer.
 */
export const isWebGPUBackend = (): boolean => {
  if (cachedIsWebGPUBackend === null) {
    const result = isModuleWebGPUBackend(getPAGModule());
    if (result === null) return false;
    cachedIsWebGPUBackend = result;
  }
  return cachedIsWebGPUBackend;
};

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
  return PAGModule.WebGPU;
};

/**
 * WebGPU backend only. Registers a host GPUTexture in Emscripten's WebGPU runtime and returns the
 * wasm-side texture handle together with the numeric WGPUTextureFormat resolved from
 * GPUTexture.format. Returns null when the WebGPU runtime is unavailable or the format is
 * unknown. Pair every successful registration with unregisterWebGPUTexture() once the wrapping
 * PAGSurface/PAGImage is destroyed.
 */
export const registerWebGPUTexture = (
  texture: WebGPUTextureLike,
): { handle: number; format: number } | null => {
  const webGPU = getWebGPURuntime();
  const format = webGPU?.TextureFormat?.indexOf(texture.format) ?? -1;
  if (!webGPU?.mgrTexture || format <= 0) {
    return null;
  }
  return { handle: webGPU.mgrTexture.create(texture), format };
};

/**
 * WebGPU backend only. Releases a texture handle previously returned by registerWebGPUTexture().
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
 * Emscripten's WebGPU runtime. Throws an apiName-prefixed error on invalid input.
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
  const registered = registerWebGPUTexture(texture);
  if (!registered) {
    throw new Error(`${apiName} failed to register the GPUTexture!`);
  }
  return { textureID: registered.handle, format: registered.format, webGPUHandle: registered.handle };
};

export class BackendContext {
  public static from(gl: WebGLRenderingContext | WebGL2RenderingContext | BackendContext): BackendContext {
    if (gl instanceof BackendContext) {
      return new BackendContext(gl.handle, true);
    } else {
      const majorVersion = isInstanceOf(gl, globalThis.WebGL2RenderingContext) ? 2 : 1;
      const { GL } = PAGModule;
      let id = 0;
      if (GL.contexts.length > 0) {
        id = GL.contexts.findIndex((context) => context?.GLctx === gl);
      }
      if (id < 1) {
        id = GL.registerContext(gl, {
          majorVersion: majorVersion,
          minorVersion: 0,
          ...WEBGL_CONTEXT_ATTRIBUTES,
        });
        return new BackendContext(id);
      }
      return new BackendContext(id, true);
    }
  }

  public handle: number;

  private externallyOwned: boolean;
  private isDestroyed = false;
  private oldHandle = 0;

  public constructor(handle: number, externallyOwned = false) {
    this.handle = handle;
    this.externallyOwned = externallyOwned;
  }

  public getContext(): WebGLRenderingContext | null {
    return PAGModule.GL.getContext(this.handle)!.GLctx;
  }

  public makeCurrent(): boolean {
    if (this.isDestroyed) return false;
    this.oldHandle = PAGModule.GL.currentContext?.handle || 0;
    if (this.oldHandle === this.handle) return true;
    return PAGModule.GL.makeContextCurrent(this.handle);
  }

  public clearCurrent() {
    if (this.isDestroyed) return;
    if (this.oldHandle === this.handle) return;
    PAGModule.GL.makeContextCurrent(0);
    if (this.oldHandle) {
      PAGModule.GL.makeContextCurrent(this.oldHandle);
    }
  }
  /**
   * Register WebGLTexture in EmscriptenGL, And return handle.
   */
  public registerTexture(texture: WebGLTexture) {
    return this.register(PAGModule.GL.textures, texture);
  }
  /**
   * Get WebGLTexture by handle.
   */
  public getTexture(handled: number): WebGLTexture | null {
    return PAGModule.GL.textures[handled];
  }
  /**
   * Unregister WebGLTexture reference in EmscriptenGL.
   */
  public unregisterTexture(handle: number) {
    PAGModule.GL.textures[handle] = null;
  }
  /**
   * Register WebGLFramebuffer in EmscriptenGL, And return handle.
   */
  public registerRenderTarget(framebuffer: WebGLFramebuffer) {
    return this.register(PAGModule.GL.framebuffers, framebuffer);
  }
  /**
   * Get WebGLFramebuffer by handle.
   */
  public getRenderTarget(handle: number): WebGLFramebuffer | null {
    return PAGModule.GL.framebuffers[handle];
  }
  /**
   * Unregister WebGLTexture reference in EmscriptenGL.
   */
  public unregisterRenderTarget(handle: number) {
    PAGModule.GL.framebuffers[handle] = null;
  }

  public destroy(): void {
    if (this.externallyOwned) {
      return;
    }
    PAGModule.GL.deleteContext(this.handle);
  }

  private register<T>(table: (T | null)[], item: T): number {
    const handle = PAGModule.GL.getNewId(table);
    table[handle] = item;
    return handle;
  }
}
