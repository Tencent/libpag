import { PAG } from '../types';

export class PAGGlFrameBuffer {
  public static module: PAG;

  public static create(texture: WebGLTexture) {
    const { GL } = this.module;
    const textureID = GL.getNewId(GL.framebuffers);
    GL.framebuffers[textureID] = texture;
    return new PAGGlFrameBuffer(textureID);
  }
  
  public textureID: number;

  public constructor(textureID: number) {
    this.textureID = textureID;
  }

  public destroy(): void {
    PAGGlFrameBuffer.module.GL.framebuffers[this.textureID] = null;
  }
}
