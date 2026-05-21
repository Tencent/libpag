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

namespace pagx {

class Drawable;
class PAGFile;

/**
 * PAGSurface is a render target that PAGFile draws into. It is a thin opaque wrapper around an
 * underlying tgfx::Surface and the GPU device that owns it; no platform or rendering backend
 * types are exposed by this header.
 *
 * A PAGSurface is passive: it never advances time or owns a PAGFile, and the same PAGSurface may
 * be drawn into by multiple PAGFile instances in sequence.
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
  struct Impl;

  explicit PAGSurface(std::unique_ptr<Impl> impl);

  std::unique_ptr<Impl> impl;

  friend class PAGFile;
};

}  // namespace pagx
