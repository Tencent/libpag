///////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
///////////////////////////////////////////////////////////////////////////////////////////////

import type { EmscriptenGL } from '@tgfx/types';

type WxOffscreenCanvas = OffscreenCanvas & { isOffscreenCanvas?: boolean };

/**
 * Creates a new GL texture from a host-decoded image source, registers it with emscripten's
 * GL.textures table, uploads pixels via texImage2D, and generates a full mipmap chain. Returns
 * the GL.textures index of the newly created texture (a positive integer); the wasm side wraps
 * it in a tgfx::BackendTexture and creates a tgfx::Image::MakeAdopted so the tgfx-managed
 * lifetime drives glDeleteTextures when the image is no longer referenced.
 *
 * Mipmaps are always generated. Both thumbnail (typically displayed at fit-zoom which renders
 * the source at <1x scale) and full (rendered <1x at fit and >1x only when the user zooms in
 * beyond the original DPI) benefit from a precomputed chain — the cost is a fixed 1/3 byte
 * overhead per texture, far cheaper than re-decoding the source on every zoom level change.
 *
 * Texture parameters: TEXTURE_2D, RGBA, UNSIGNED_BYTE, LINEAR_MIPMAP_LINEAR for minification,
 * LINEAR for magnification, CLAMP_TO_EDGE on both axes (image-pattern tiling is handled
 * separately by the renderer's sampler state, not by the texture itself).
 *
 * @param GL The emscripten GL bridge object (typically obtained from `module.GL`).
 * @param source The image source. Must be one of:
 *   - WeChat OffscreenCanvas: detected via the `isOffscreenCanvas` discriminator. WeChat's
 *     2D OffscreenCanvas cannot be passed directly to texImage2D as TexImageSource on iOS;
 *     we read pixels through getImageData and upload as a Uint8Array buffer instead.
 *   - Standard web TexImageSource (HTMLImageElement, ImageBitmap, browser OffscreenCanvas,
 *     HTMLCanvasElement, HTMLVideoElement): handed straight to texImage2D.
 * @returns The GL.textures index on success (always > 0), or 0 on failure (no current GL
 *          context, or the source has zero width/height after decoding).
 */
export const createBackendTexture = (
  GL: EmscriptenGL,
  source: TexImageSource | WxOffscreenCanvas,
): number => {
  const ctx = GL.currentContext;
  if (!ctx) {
    return 0;
  }
  const gl = ctx.GLctx as WebGL2RenderingContext;
  const width = (source as { width?: number }).width || 0;
  const height = (source as { height?: number }).height || 0;
  if (width < 1 || height < 1) {
    return 0;
  }

  const tex = gl.createTexture();
  if (!tex) {
    return 0;
  }
  // emscripten requires textures used by GLES code to be registered in GL.textures so the wasm
  // side can refer to them by integer id. The .name property mirrors what
  // GL.createTexture/_glGenTextures would have set.
  const id = GL.getNewId(GL.textures);
  (tex as any).name = id;
  GL.textures[id] = tex;

  gl.bindTexture(gl.TEXTURE_2D, tex);
  gl.pixelStorei(gl.UNPACK_PREMULTIPLY_ALPHA_WEBGL, true);

  const wxCanvas = source as WxOffscreenCanvas;
  if (wxCanvas.isOffscreenCanvas) {
    // WeChat's 2d OffscreenCanvas cannot be used as a TexImageSource directly on iOS. Read
    // back via getImageData; the resulting Uint8ClampedArray is wrapped (no copy) into a
    // Uint8Array view that texImage2D accepts.
    const canvasCtx = wxCanvas.getContext('2d') as OffscreenCanvasRenderingContext2D;
    const imageData = canvasCtx.getImageData(0, 0, width, height);
    gl.texImage2D(
      gl.TEXTURE_2D,
      0,
      gl.RGBA,
      width,
      height,
      0,
      gl.RGBA,
      gl.UNSIGNED_BYTE,
      new Uint8Array(imageData.data.buffer, imageData.data.byteOffset, imageData.data.byteLength),
    );
  } else {
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, source as TexImageSource);
  }

  gl.generateMipmap(gl.TEXTURE_2D);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR_MIPMAP_LINEAR);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
  // Reset the premultiply flag so subsequent uploads driven by other code paths (e.g.
  // tgfx.uploadToTexture) start from the documented default.
  gl.pixelStorei(gl.UNPACK_PREMULTIPLY_ALPHA_WEBGL, false);
  return id;
};

/**
 * Releases a batch of GL textures previously created by createBackendTexture(). The wasm side
 * accumulates retired GL.textures indices in a single std::vector and hands the entire batch
 * across the boundary in one call so wasm↔JS overhead stays at O(1) per draw regardless of
 * how many textures were evicted. For each id this removes the GL.textures entry and asks the
 * driver to free the associated GPU memory; in-flight commands that already reference the
 * texture continue to execute correctly per the WebGL/GLES specification.
 *
 * @param GL The emscripten GL bridge object (typically obtained from `module.GL`).
 * @param textureIds A plain JavaScript Array of GL.textures indices to release. Entries equal
 *   to 0 or pointing at slots that are already empty are silently skipped so the wasm side
 *   never has to special-case partial-failure paths.
 */
export const destroyBackendTextures = (GL: EmscriptenGL, textureIds: number[]): void => {
  const ctx = GL.currentContext;
  if (!ctx) {
    return;
  }
  const gl = ctx.GLctx as WebGL2RenderingContext;
  for (let i = 0; i < textureIds.length; i++) {
    const id = textureIds[i];
    if (!id) {
      continue;
    }
    const tex = GL.textures[id];
    if (!tex) {
      continue;
    }
    gl.deleteTexture(tex);
    // Match the pattern emscripten uses when synthesising _glDeleteTextures: clear both the
    // .name property and the GL.textures slot so a recycled id does not point at a deleted
    // WebGLTexture object.
    GL.textures[id] = null;
  }
};
