import { PAGModule } from '../pag-module';
import { WEBGL_CONTEXT_ATTRIBUTES } from '../constant';

export class BackendContext {
  public static from(gl: WebGLRenderingContext | WebGL2RenderingContext | BackendContext): BackendContext {
    if (gl instanceof BackendContext) {
      return new BackendContext(gl.handle, true);
    } else {
      const majorVersion = globalThis.WebGL2RenderingContext && gl instanceof globalThis.WebGL2RenderingContext ? 2 : 1;
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

  private adopted: boolean;
  private isDestroyed = false;
  private oldHandle = 0;

  public constructor(handle: number, adopted = false) {
    this.handle = handle;
    this.adopted = adopted;
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
    if (this.adopted) return;
    PAGModule.GL.deleteContext(this.handle);
  }

  private register<T>(table: (T | null)[], item: T): number {
    const handle = PAGModule.GL.getNewId(table);
    table[handle] = item;
    return handle;
  }
}
