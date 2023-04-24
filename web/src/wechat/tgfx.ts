import { ArrayBufferImage } from './array-buffer-image';
import { getCanvas2D, releaseCanvas2D } from './canvas';
import { getSourceSize, isAndroidMiniprogram } from '../tgfx';

import type { EmscriptenGL } from '../types';

type WxOffscreenCanvas = OffscreenCanvas & { isOffscreenCanvas: boolean };

export const uploadToTexture = (
  GL: EmscriptenGL,
  source: TexImageSource | ArrayBufferImage | WxOffscreenCanvas,
  textureID: number,
  alphaOnly: boolean,
) => {
  const gl = GL.currentContext?.GLctx as WebGLRenderingContext;
  gl.bindTexture(gl.TEXTURE_2D, GL.textures[textureID]);
  if (!alphaOnly && (source as WxOffscreenCanvas)?.isOffscreenCanvas) {
    const ctx = (source as WxOffscreenCanvas).getContext('2d') as OffscreenCanvasRenderingContext2D;
    const imgData = ctx.getImageData(0, 0, source.width, source.height);
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
    return;
  }

  gl.pixelStorei(gl.UNPACK_PREMULTIPLY_ALPHA_WEBGL, true);
  if (source instanceof ArrayBufferImage) {
    gl.texImage2D(
      gl.TEXTURE_2D,
      0,
      gl.RGBA,
      source.width,
      source.height,
      0,
      gl.RGBA,
      gl.UNSIGNED_BYTE,
      new Uint8Array(source.buffer),
    );
  } else {
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, source as TexImageSource);
  }

  gl.pixelStorei(gl.UNPACK_PREMULTIPLY_ALPHA_WEBGL, false);
};

export { getSourceSize, isAndroidMiniprogram, getCanvas2D as createCanvas2D, releaseCanvas2D as releaseNativeImage };
