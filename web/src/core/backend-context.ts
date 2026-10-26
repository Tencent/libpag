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
