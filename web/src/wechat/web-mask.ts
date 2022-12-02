import { WebMask as NativeWebMask } from '../core/web-mask';
import { getCanvas2D, releaseCanvas2D } from './canvas';

import type { EmscriptenGL } from '../types';

export class WebMask extends NativeWebMask {
  public static create(width: number, height: number) {
    const webMask = new WebMask(getCanvas2D(), width, height);
    return webMask;
  }

  public update(GL: EmscriptenGL) {
    const gl = GL.currentContext?.GLctx as WebGLRenderingContext;
    const ctx = this.canvas.getContext('2d') as OffscreenCanvasRenderingContext2D;
    const imgData = ctx.getImageData(0, 0, this.canvas.width, this.canvas.height);
    gl.texImage2D(
      gl.TEXTURE_2D,
      0,
      gl.RGBA,
      imgData.width,
      imgData.height,
      0,
      gl.RGBA,
      gl.UNSIGNED_BYTE,
      new Uint8Array(imgData.data),
    );
  }

  public onDestroy() {
    releaseCanvas2D(this.canvas as OffscreenCanvas);
  }
}
