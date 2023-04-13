import { getCanvas2D, releaseCanvas2D, isOffscreenCanvas } from './utils/canvas';
import { writeBufferToWasm } from './utils/buffer';
import { BitmapImage } from './core/bitmap-image';

import type { EmscriptenGL, PAG } from './types';
import type { wx } from './wechat/interfaces';

declare const wx: wx;

export const createImage = (source: string) => {
  return new Promise<HTMLImageElement | null>((resolve) => {
    const image = new Image();
    image.onload = function () {
      resolve(image);
    };
    image.onerror = function () {
      console.error('image create from bytes error.');
      resolve(null);
    };
    image.src = source;
  });
};

export const createImageFromBytes = (bytes: ArrayBuffer) => {
  const blob = new Blob([bytes], { type: 'image/*' });
  return createImage(URL.createObjectURL(blob));
};

export const readImagePixels = (module: PAG, image: CanvasImageSource, width: number, height: number) => {
  if (!image) {
    return null;
  }
  const canvas = getCanvas2D(width, height);
  const ctx = canvas.getContext('2d') as CanvasRenderingContext2D | OffscreenCanvasRenderingContext2D | null;
  if (!ctx) {
    return null;
  }
  ctx.drawImage(image, 0, 0, width, height);
  const { data } = ctx.getImageData(0, 0, width, height);
  releaseCanvas2D(canvas);
  if (data.length === 0) {
    return null;
  }
  return writeBufferToWasm(module, data);
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

export const uploadToTexture = (
  GL: EmscriptenGL,
  source: TexImageSource | OffscreenCanvas | BitmapImage,
  textureID: number,
  alphaOnly: boolean,
) => {
  let renderSource = source instanceof BitmapImage ? source.bitmap : source;
  if (!renderSource) return;
  const gl = GL.currentContext?.GLctx as WebGLRenderingContext;
  gl.bindTexture(gl.TEXTURE_2D, GL.textures[textureID]);
  if (alphaOnly) {
    gl.pixelStorei(gl.UNPACK_ALIGNMENT, 1);
    gl.texSubImage2D(gl.TEXTURE_2D, 0, 0, 0, gl.ALPHA, gl.UNSIGNED_BYTE, renderSource);
  } else {
    gl.pixelStorei(gl.UNPACK_ALIGNMENT, 4);
    gl.pixelStorei(gl.UNPACK_PREMULTIPLY_ALPHA_WEBGL, true);
    gl.texSubImage2D(gl.TEXTURE_2D, 0, 0, 0, gl.RGBA, gl.UNSIGNED_BYTE, renderSource);
    gl.pixelStorei(gl.UNPACK_PREMULTIPLY_ALPHA_WEBGL, false);
  }
};

export const isAndroidMiniprogram = () => {
  if (typeof wx !== 'undefined' && wx.getSystemInfoSync) {
    return wx.getSystemInfoSync().platform === 'android';
  }
};

export const releaseNativeImage = (source: TexImageSource | OffscreenCanvas) => {
  if (source instanceof ImageBitmap) {
    source.close();
  } else if (isOffscreenCanvas(source) || source instanceof HTMLCanvasElement) {
    releaseCanvas2D(source as OffscreenCanvas | HTMLCanvasElement);
  }
};

export const getBytesFromPath = async (module: PAG, path: string) => {
  const buffer = await fetch(path).then((res) => res.arrayBuffer());
  return writeBufferToWasm(module, buffer);
};

export { getCanvas2D as createCanvas2D };
