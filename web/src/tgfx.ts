import { releaseCanvas2D } from './utils/canvas';

import type { EmscriptenGL } from './types';
import type { wx } from './wechat/interfaces';

declare const wx: wx;

export const createImageFromBytes = (bytes: ArrayBuffer) => {
  const blob = new Blob([bytes], { type: 'image/*' });
  return new Promise<HTMLImageElement | null>((resolve) => {
    const image = new Image();
    image.onload = function () {
      resolve(image);
    };
    image.onerror = function () {
      console.error('image create from bytes error.');
      resolve(null);
    };
    image.src = URL.createObjectURL(blob);
  });
};

export const createImageFromPath = (path: string) => {
  return new Promise<HTMLImageElement | null>((resolve) => {
    const image = new Image();
    image.onload = function () {
      resolve(image);
    };
    image.onerror = function () {
      console.error(`image create from path error: ${path}`);
      resolve(null);
    };
    image.src = path;
  });
};

export const hasWebpSupport = () => {
  try {
    return document.createElement('canvas').toDataURL('image/webp', 0.5).indexOf('data:image/webp') === 0;
  } catch (err) {
    return false;
  }
};

export const getSourceSize = (source: TexImageSource | OffscreenCanvas) => {
  if (globalThis.HTMLVideoElement && source instanceof HTMLVideoElement) {
    return {
      width: source.videoWidth,
      height: source.videoHeight,
    };
  }
  return { width: source.width, height: source.height };
};

export const uploadTexImageSource = (GL: EmscriptenGL, source: TexImageSource | OffscreenCanvas) => {
  const gl = GL.currentContext?.GLctx as WebGLRenderingContext;
  gl.pixelStorei(gl.UNPACK_PREMULTIPLY_ALPHA_WEBGL, true);
  gl.pixelStorei(gl.UNPACK_ALIGNMENT, 4);
  gl.texSubImage2D(gl.TEXTURE_2D, 0, 0, 0, gl.RGBA, gl.UNSIGNED_BYTE, source);
  gl.pixelStorei(gl.UNPACK_PREMULTIPLY_ALPHA_WEBGL, false);
};

export const isAndroidMiniprogram = () => {
  if (typeof wx !== 'undefined' && wx.getSystemInfoSync) {
    return wx.getSystemInfoSync().platform === 'android';
  }
};

export const releaseNativeImage = (source: TexImageSource | OffscreenCanvas) => {
  if (source instanceof ImageBitmap) {
    source.close();
  } else if (source instanceof OffscreenCanvas || source instanceof HTMLCanvasElement) {
    releaseCanvas2D(source);
  }
};
