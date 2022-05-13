import type { PAG } from '../types';

export class PAGGlContext {
  public static module: PAG;

  public static get(gl: WebGLRenderingContext) {
    const { GL } = this.module;
    let id = 0;
    if (GL.contexts.length > 0) {
      id = GL.contexts.findIndex((context) => context?.GLctx === gl);
    }
    if (id < 1) {
      id = GL.registerContext(gl, {
        majorVersion: 1,
        minorVersion: 0,
        depth: false,
        stencil: false,
        antialias: false,
      });
    }
    return new PAGGlContext(id);
  }

  public id: number;

  public constructor(id: number) {
    this.id = id;
  }

  public makeContextCurrent() {
    PAGGlContext.module.GL.makeContextCurrent(this.id);
  }

  public destroy(): void {
    PAGGlContext.module.GL.deleteContext(this.id);
  }
}
