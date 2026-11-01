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

#include <memory>
#include "pag/defines.h"
#include "pag/types.h"

namespace tgfx {
class SurfaceReadback;
}  // namespace tgfx

namespace pag {

class PAGSurface;

/**
 * Describes the current state of an asynchronous pixel readback session.
 */
enum class ReadbackStatus {
  /**
   * The pixels are ready and can be read by PAGSurfaceReadback::readPixels().
   */
  Ready,
  /**
   * The pixels have not arrived yet and may still become Ready on a later poll.
   */
  Pending,
  /**
   * The readback has failed permanently; the pixels will never arrive.
   */
  Failed,
};

/**
 * An asynchronous pixel readback session created by PAGSurface::asyncReadPixels(). The pixel data
 * may not be available immediately after creation, because it can involve asynchronous GPU
 * processing: on WebGPU the data arrives through an asynchronous GPU buffer mapping, and even on
 * OpenGL readiness is fence-based, meaning it arrives only after the GPU drains previously queued
 * work. Poll isReady() until it returns true, then call readPixels() to copy the pixels out.
 * The session holds GPU resources until it is destroyed, so do not keep it alive longer than
 * needed.
 */
class PAG_API PAGSurfaceReadback {
 public:
  ~PAGSurfaceReadback();

  /**
   * Returns the width in pixels of the readback data.
   */
  int width() const;

  /**
   * Returns the height in pixels of the readback data.
   */
  int height() const;

  /**
   * Returns true if the pixel data is ready to be read by readPixels(). This method never blocks.
   * Equivalent to status() == ReadbackStatus::Ready.
   */
  bool isReady();

  /**
   * Returns the current state of the readback session. Unlike isReady(), this method
   * distinguishes a permanent failure (ReadbackStatus::Failed) from a not-yet-ready state, so
   * callers can stop polling immediately instead of waiting forever. This method never blocks.
   */
  ReadbackStatus status();

  /**
   * Copies the readback pixels to dstPixels with the specified color type, alpha type and row
   * bytes. Returns false without copying if the pixel data is not ready yet. dstPixels must have
   * room for at least height() * dstRowBytes bytes. Returns true if the pixels are copied.
   * @param colorType Describes how to interpret the components of a pixel.
   * @param alphaType Describes how to interpret the alpha component of a pixel.
   * @param dstPixels The destination pixel buffer to copy into.
   * @param dstRowBytes The number of bytes between subsequent rows of dstPixels.
   */
  bool readPixels(ColorType colorType, AlphaType alphaType, void* dstPixels, size_t dstRowBytes);

 private:
  PAGSurfaceReadback(std::shared_ptr<PAGSurface> pagSurface,
                     std::shared_ptr<tgfx::SurfaceReadback> readback, bool flipY);

  std::shared_ptr<PAGSurface> pagSurface = nullptr;
  std::shared_ptr<tgfx::SurfaceReadback> readback = nullptr;
  bool flipY = false;

  friend class PAGSurface;
};

}  // namespace pag
