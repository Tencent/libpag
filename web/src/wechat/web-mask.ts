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
    const alphaData = imgData.data.reduce((acc, cur, idx) => {
      if (idx % 4 === 3) acc.push(cur);
      return acc;
    }, [] as number[]);
    gl.pixelStorei(gl.UNPACK_ALIGNMENT, 1);
    gl.texSubImage2D(
      gl.TEXTURE_2D,
      0,
      0,
      0,
      imgData.width,
      imgData.height,
      gl.ALPHA,
      gl.UNSIGNED_BYTE,
      new Uint8Array(alphaData),
    );
  }

  public onDestroy() {
    releaseCanvas2D(this.canvas as OffscreenCanvas);
  }
}
