import { releaseCanvas2D } from './canvas';
import { getSourceSize, isAndroidMiniprogram } from '../tgfx';

import type { EmscriptenGL } from '../types';

export const uploadToTexture = (
  GL: EmscriptenGL,
  source: TexImageSource | OffscreenCanvas | { buffer: ArrayBuffer; width: number; height: number },
  textureID: number
) => {
  const gl = GL.currentContext?.GLctx as WebGLRenderingContext;
  gl.bindTexture(gl.TEXTURE_2D, GL.textures[textureID]);
  gl.pixelStorei(gl.UNPACK_PREMULTIPLY_ALPHA_WEBGL, true);
  if (source.hasOwnProperty('buffer')) {
    // ArrayBuffer ImageData
    gl.texImage2D(
      gl.TEXTURE_2D,
      0,
      gl.RGBA,
      source.width,
      source.height,
      0,
      gl.RGBA,
      gl.UNSIGNED_BYTE,
      new Uint8Array((source as { buffer: ArrayBuffer }).buffer),
    );
  } else {
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, source as TexImageSource);
  }

  gl.pixelStorei(gl.UNPACK_PREMULTIPLY_ALPHA_WEBGL, false);
};

export { getSourceSize, isAndroidMiniprogram, releaseCanvas2D as releaseNativeImage };
