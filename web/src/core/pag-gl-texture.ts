import { PAGGlUnit } from './pag-gl-unit';
import { PAGGlContext } from './pag-gl-context';

export class PAGGlTexture extends PAGGlUnit {
  public static create(gl: WebGLRenderingContext, texture: WebGLTexture) {
    const { GL } = this.module;
    const id = GL.getNewId(GL.textures);
    GL.textures[id] = texture;
    return new PAGGlTexture(PAGGlContext.get(gl), id);
  }

  public destroy(): void {
    PAGGlTexture.module.GL.textures[this.id] = null;
  }
}
