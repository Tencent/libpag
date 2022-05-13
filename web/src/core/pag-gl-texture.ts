import { PAG } from '../types';

export class PAGGlTexture {
  public static module: PAG;

  public static create(texture: WebGLTexture) {
    const { GL } = this.module;
    const textureID = GL.getNewId(GL.textures);
    GL.textures[textureID] = texture;
    return new PAGGlTexture(textureID);
  }

  public textureID: number;

  public constructor(textureID: number) {
    this.textureID = textureID;
  }

  public destroy(): void {
    PAGGlTexture.module.GL.textures[this.textureID] = null;
  }
}
