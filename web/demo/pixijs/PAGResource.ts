import { Resource } from 'pixi.js';

class PAGResource extends Resource {
  static async create(PAG, pagFile) {
    const width = await pagFile.width();
    const height = await pagFile.height();
    const pagResource = new PAGResource(width, height);
    pagResource.pagPlayer = await PAG.PAGPlayer.create();
    await pagResource.pagPlayer.setComposition(pagFile);
    pagResource.module = PAG;
    return pagResource;
  }

  private module;
  private contextID = null;
  private textureID = null;
  private pagPlayer = null;
  private pagSurface = null;

  constructor(width, height) {
    super(width, height);
  }

  async upload(renderer, baseTexture, glTexture) {
    const { width } = this;
    const { height } = this;
    glTexture.width = width;
    glTexture.height = height;

    const { gl } = renderer;

    // 注册 context
    if (this.contextID === null) {
      this.contextID = this.module.GL.registerContext(gl, { majorVersion: 2, minorVersion: 0 });
    }


    if (glTexture.texture.name !== this.textureID) {
      // texture 变化
      if (this.textureID !== null) {
        // 销毁旧的 surface
        this.module.GL.textures[this.textureID] = null;
        this.pagSurface.destroy();
      }
      // 分配内存不然绑定 frameBuffer 会失败
      gl.texImage2D(
        baseTexture.target,
        0,
        baseTexture.format,
        width,
        height,
        0,
        baseTexture.format,
        baseTexture.type,
        null,
      );
      // 注册
      this.textureID = this.module.GL.getNewId(this.module.GL.textures);
      glTexture.texture.name = this.textureID;
      this.module.GL.textures[this.textureID] = glTexture.texture;
      // 生成 surface
      this.module.GL.makeContextCurrent(this.contextID);
      this.pagSurface = await this.module._PAGSurface.FromTexture(this.textureID, width, height, false);
      await this.pagPlayer.setSurface(this.pagSurface);
    }
    await this.pagPlayer.flush();
    renderer.reset();
    return true;
  }

  public async setProgress(progress) {
    await this.pagPlayer.setProgress(progress);
    this.update();
  }
}
