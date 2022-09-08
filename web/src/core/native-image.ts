import { EmscriptenGL } from '../types';
import { isCanvas, releaseCanvas2D } from '../utils/canvas';

export class NativeImage {
  public static async createFromBytes(bytes: ArrayBuffer) {
    const blob = new Blob([bytes], { type: 'image/*' });
    return new Promise<NativeImage | null>((resolve) => {
      const image = new Image();
      image.onload = function () {
        resolve(new NativeImage(image));
      };
      image.onerror = function () {
        console.error('image create from bytes error.');
        resolve(null);
      };
      image.src = URL.createObjectURL(blob);
    });
  }

  public static async createFromPath(path: string) {
    return new Promise<NativeImage | null>((resolve) => {
      const image = new Image();
      image.onload = function () {
        resolve(new NativeImage(image));
      };
      image.onerror = function () {
        console.error(`image create from path error: ${path}`);
        resolve(null);
      };
      image.src = path;
    });
  }

  private source: TexImageSource | OffscreenCanvas;
  private reuse: boolean;

  public constructor(source: TexImageSource | OffscreenCanvas, reuse = false) {
    this.source = source;
    this.reuse = reuse;
  }

  public width(): number {
    return window.HTMLVideoElement && this.source instanceof HTMLVideoElement
      ? this.source.videoWidth
      : this.source.width;
  }

  public height(): number {
    return window.HTMLVideoElement && this.source instanceof HTMLVideoElement
      ? this.source.videoHeight
      : this.source.height;
  }

  public upload(GL: EmscriptenGL) {
    const gl = GL.currentContext?.GLctx as WebGLRenderingContext;
    gl.pixelStorei(gl.UNPACK_PREMULTIPLY_ALPHA_WEBGL, true);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, this.source);
  }

  public onDestroy() {
    if (this.reuse && isCanvas(this.source)) {
      releaseCanvas2D(this.source as HTMLCanvasElement | OffscreenCanvas);
    }
  }
}
