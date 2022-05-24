import type { PAG } from '../types';

export class BackendContext {
  public static module: PAG;

  public static from(gl: WebGLRenderingContext | WebGL2RenderingContext | BackendContext): BackendContext {
    if (
      gl instanceof WebGLRenderingContext ||
      (window.WebGL2RenderingContext && gl instanceof WebGL2RenderingContext)
    ) {
      const majorVersion = gl instanceof WebGLRenderingContext ? 1 : 2;
      const { GL } = this.module;
      let id = 0;
      if (GL.contexts.length > 0) {
        id = GL.contexts.findIndex((context) => context?.GLctx === gl);
      }
      if (id < 1) {
        id = GL.registerContext(gl, {
          majorVersion: majorVersion,
          minorVersion: 0,
          depth: false,
          stencil: false,
          antialias: false,
        });
        return new BackendContext(id);
      }
      return new BackendContext(id, true);
    } else if (gl instanceof BackendContext) {
      return new BackendContext(gl.handle, true);
    }
    throw new Error('Parameter error!');
  }

  public handle: number;

  private adopted: boolean;
  private isDestroyed = false;
  private oldHandle = 0;

  public constructor(handle: number, adopted = false) {
    this.handle = handle;
    this.adopted = adopted;
  }

  public getContext(): WebGLRenderingContext | null {
    return BackendContext.module.GL.getContext(this.handle)!.GLctx;
  }

  public makeCurrent(): boolean {
    if (this.isDestroyed) return false;
    this.oldHandle = BackendContext.module.GL.currentContext?.handle || 0;
    if (this.oldHandle === this.handle) return true;
    return BackendContext.module.GL.makeContextCurrent(this.handle);
  }

  public clearCurrent() {
    if (this.isDestroyed) return;
    if (this.oldHandle === this.handle) return;
    BackendContext.module.GL.makeContextCurrent(0);
    if (this.oldHandle) {
      BackendContext.module.GL.makeContextCurrent(this.oldHandle);
    }
  }
  /**
   * Register WebGLTexture in EmscriptenGL, And return handle.
   */
  public registerTexture(texture: WebGLTexture) {
    return this.register(BackendContext.module.GL.textures, texture);
  }
  /**
   * Get WebGLTexture by handle.
   */
  public getTexture(handled: number): WebGLTexture | null {
    return BackendContext.module.GL.textures[handled];
  }
  /**
   * Unregister WebGLTexture reference in EmscriptenGL.
   */
  public unregisterTexture(handle: number) {
    BackendContext.module.GL.textures[handle] = null;
  }
  /**
   * Register WebGLFramebuffer in EmscriptenGL, And return handle.
   */
  public registerRenderTarget(framebuffer: WebGLFramebuffer) {
    return this.register(BackendContext.module.GL.framebuffers, framebuffer);
  }
  /**
   * Get WebGLFramebuffer by handle.
   */
  public getRenderTarget(handle: number): WebGLFramebuffer | null {
    return BackendContext.module.GL.framebuffers[handle];
  }
  /**
   * Unregister WebGLTexture reference in EmscriptenGL.
   */
  public unregisterRenderTarget(handle: number) {
    BackendContext.module.GL.framebuffers[handle] = null;
  }

  public destroy(): void {
    if (this.adopted) return;
    BackendContext.module.GL.deleteContext(this.handle);
  }

  private register<T>(table: (T | null)[], item: T): number {
    const handle = BackendContext.module.GL.getNewId(table);
    table[handle] = item;
    return handle;
  }
}
