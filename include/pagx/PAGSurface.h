/////////////////////////////////////////////////////////////////////////////////////////////////
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
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstddef>
#include <memory>
#include "pag/gpu.h"

namespace pagx {

class Drawable;
class PAGScene;

/**
 * PAGSurface is a render target that PAGScene draws into. It is a thin opaque wrapper around an
 * underlying tgfx::Surface and the GPU device that owns it; no platform or rendering backend
 * types are exposed by this header.
 *
 * A PAGSurface is passive: it never advances time or owns a PAGScene, and the same PAGSurface may
 * be drawn into by multiple PAGScene instances in sequence.
 */
class PAGSurface {
 public:
  /**
   * Creates an offscreen surface with the given pixel dimensions. Returns nullptr if the size is
   * invalid or no GPU backend is available on the calling thread.
   * @param width  surface width in pixels, must be greater than zero.
   * @param height surface height in pixels, must be greater than zero.
   */
  static std::shared_ptr<PAGSurface> MakeOffscreen(int width, int height);

  /**
   * Creates a PAGSurface backed by the given Drawable. The Drawable is retained for the lifetime
   * of the returned PAGSurface. Returns nullptr if drawable is null. Drawable is an opaque
   * extension point: platform-specific subclasses (e.g. canvas-backed drawables on the web) live
   * in the pagx implementation tree and are obtained via their respective factory entry points.
   * @param drawable the platform-specific render target adapter.
   */
  static std::shared_ptr<PAGSurface> MakeFrom(std::shared_ptr<Drawable> drawable);

  /**
   * Creates a PAGSurface from the specified backend texture. The texture must not be bound to any
   * framebuffer. The PAGSurface uses the GPU context on the calling thread directly via
   * GLDevice::Current(). Returns nullptr if the texture is invalid or no GPU context is available.
   * @param texture the backend texture to render into.
   * @param origin  the origin of the texture coordinate system.
   */
  static std::shared_ptr<PAGSurface> MakeFrom(const pag::BackendTexture& texture,
                                              pag::ImageOrigin origin);

  /**
   * Creates a PAGSurface from the specified backend render target. The PAGSurface uses the GPU
   * context on the calling thread directly via GLDevice::Current(). Returns nullptr if the render
   * target is invalid or no GPU context is available.
   * @param renderTarget the backend render target to render into.
   * @param origin       the origin of the render target coordinate system.
   */
  static std::shared_ptr<PAGSurface> MakeFrom(const pag::BackendRenderTarget& renderTarget,
                                              pag::ImageOrigin origin);

  ~PAGSurface();

  PAGSurface(const PAGSurface&) = delete;
  PAGSurface& operator=(const PAGSurface&) = delete;

  /**
   * Returns the surface width in pixels.
   */
  int width() const;

  /**
   * Returns the surface height in pixels.
   */
  int height() const;

  /**
   * Copies pixels from the surface into dstPixels. Pixels are written as RGBA_8888 with
   * premultiplied alpha and rowBytes equal to the row stride of the destination buffer.
   * The destination buffer must hold at least height() * dstRowBytes bytes, with
   * dstRowBytes >= width() * 4. Returns true on success.
   * @param dstPixels   pointer to destination buffer, must not be null.
   * @param dstRowBytes number of bytes per destination row.
   */
  bool readPixels(void* dstPixels, size_t dstRowBytes);

 private:
  explicit PAGSurface(std::shared_ptr<Drawable> drawable);

  std::shared_ptr<Drawable> drawable = nullptr;

  friend class PAGScene;
};

}  // namespace pagx
