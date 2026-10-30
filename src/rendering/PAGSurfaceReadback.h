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
 * An asynchronous pixel readback session created by AsyncReadPixels(). This type is internal to
 * libpag: it is consumed by the Web bindings and tests only, so it is not part of the public
 * header set.
 */
class PAGSurfaceReadback {
 public:
  ~PAGSurfaceReadback();

  // The session wraps a single lockable GPU readback; copying would let two wrappers unlock the
  // same session, so copying is disabled.
  PAGSurfaceReadback(const PAGSurfaceReadback&) = delete;
  PAGSurfaceReadback& operator=(const PAGSurfaceReadback&) = delete;

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
   */
  bool readPixels(ColorType colorType, AlphaType alphaType, void* dstPixels, size_t dstRowBytes);

 private:
  PAGSurfaceReadback(std::shared_ptr<PAGSurface> pagSurface,
                     std::shared_ptr<tgfx::SurfaceReadback> readback, bool flipY);

  std::shared_ptr<PAGSurface> pagSurface = nullptr;
  std::shared_ptr<tgfx::SurfaceReadback> readback = nullptr;
  bool flipY = false;

  friend std::shared_ptr<PAGSurfaceReadback> AsyncReadPixels(std::shared_ptr<PAGSurface> surface);
};

/**
 * Starts an asynchronous readback of the entire surface content and returns a session object.
 * Unlike PAGSurface::readPixels(), this function never blocks the calling thread waiting for the
 * GPU: poll PAGSurfaceReadback::isReady() until it returns true, then copy the pixels out with
 * PAGSurfaceReadback::readPixels(). On the WebGPU backend, where synchronous GPU readback is
 * unsupported, this is the only way to read pixels. The returned session keeps the PAGSurface
 * alive. Returns nullptr if the readback could not be started, which is always the case for
 * canvas-backed surfaces (e.g. created from a web canvas): their frames can only be read back
 * between drawing and presentation, which the PAGPlayer::flush() flow does not expose.
 */
std::shared_ptr<PAGSurfaceReadback> AsyncReadPixels(std::shared_ptr<PAGSurface> surface);

}  // namespace pag
