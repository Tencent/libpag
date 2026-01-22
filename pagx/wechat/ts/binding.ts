import { PAGX } from './types';
import { View } from './pagx-view';
import { TGFXBind } from '@tgfx/wechat/binding';
import type { EmscriptenGL, WindowColorSpace } from '@tgfx/types';

/**
 * Set color space for WebGL context.
 * This function is required by WebGLDevice but missing in TGFXBind for WeChat.
 */
const setColorSpace = (GL: EmscriptenGL, colorSpace: WindowColorSpace): boolean => {
  // WindowColorSpace: None=0, SRGB=1, DisplayP3=2, Others=3
  if (colorSpace === 3) { // WindowColorSpace.Others
    return false;
  }
  const gl = GL.currentContext?.GLctx as WebGLRenderingContext;
  if ('drawingBufferColorSpace' in gl) {
    if (colorSpace === 0 || colorSpace === 1) { // WindowColorSpace.None or SRGB
      (gl as any).drawingBufferColorSpace = 'srgb';
    } else { // WindowColorSpace.DisplayP3
      (gl as any).drawingBufferColorSpace = 'display-p3';
    }
    return true;
  } else if (colorSpace === 2) { // WindowColorSpace.DisplayP3
    return false;
  }
  return true;
};

/**
 * Binding pag js module on pag webassembly module.
 */
export const binding = (module: PAGX) => {
  TGFXBind(module);
  module.View = View;
  module.tgfx.setColorSpace = setColorSpace;
};
