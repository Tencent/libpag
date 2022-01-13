export class NativeImage {
  public static async createFromBytes(bytes) {
    const blob = new Blob([bytes], { type: 'image/*' });
    return new Promise((resolve) => {
      const image = new Image();
      image.onload = function () {
        resolve(new NativeImage(image));
      };
      image.src = URL.createObjectURL(blob);
    });
  }

  private readonly source: TexImageSource;

  public constructor(source) {
    this.source = source;
  }

  public width(): number {
    return this.source instanceof HTMLVideoElement ? this.source.videoWidth : this.source.width;
  }

  public height(): number {
    return this.source instanceof HTMLVideoElement ? this.source.videoHeight : this.source.height;
  }

  public upload(GL) {
    const gl = GL.currentContext.GLctx as WebGLRenderingContext;
    gl.pixelStorei(gl.UNPACK_PREMULTIPLY_ALPHA_WEBGL, true);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, this.source);
  }
}
