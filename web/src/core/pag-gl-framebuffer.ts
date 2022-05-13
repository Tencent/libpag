import { PAGGlUnit } from './pag-gl-unit';
import { PAGGlContext } from './pag-gl-context';

export class PAGGlFrameBuffer extends PAGGlUnit {
  public static create(gl: WebGLRenderingContext, framebuffer: WebGLFramebuffer) {
    const { GL } = this.module;
    const id = GL.getNewId(GL.framebuffers);
    GL.framebuffers[id] = framebuffer;
    return new PAGGlFrameBuffer(PAGGlContext.get(gl), id);
  }

  public destroy(): void {
    PAGGlFrameBuffer.module.GL.framebuffers[this.id] = null;
  }
}
