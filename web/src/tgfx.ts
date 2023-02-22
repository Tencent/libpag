import { getCanvas2D, releaseCanvas2D } from './utils/canvas';
import { malloc } from './utils/buffer';
import { IPHONE } from './utils/ua';

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

export const readImagePixels = async (module: PAG, bytes: ArrayBuffer, width: number, height: number) => {
  const image = await createImageFromBytes(bytes);
  if (!image) return null;
  const canvas = getCanvas2D();
  canvas.width = width;
  canvas.height = height;
  const ctx = canvas.getContext('2d') as CanvasRenderingContext2D | OffscreenCanvasRenderingContext2D | null;
  if (!ctx) return null;
  ctx.drawImage(image, 0, 0, width, height);
  const { data } = ctx.getImageData(0, 0, width, height);
  releaseCanvas2D(canvas);
  if (data.length === 0) return null;
  return malloc(module, data);
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

export const uploadToTexture = (GL: EmscriptenGL, source: TexImageSource | OffscreenCanvas, textureID: number) => {
  const gl = GL.currentContext?.GLctx as WebGLRenderingContext;
  gl.bindTexture(gl.TEXTURE_2D, GL.textures[textureID]);
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

export const getBytesFromPath = async (module: PAG, path: string) => {
  const buffer = await fetch(path).then((res) => res.arrayBuffer());
  return malloc(module, buffer);
};

export const isIphone = () => IPHONE;
