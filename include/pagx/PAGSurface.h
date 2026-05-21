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

namespace pagx {

/**
 * PAGSurface is a render target that PAGFile draws into. It is a thin opaque wrapper around the
 * underlying graphics surface; no platform or rendering backend types are exposed by this header.
 *
 * A PAGSurface is passive: it never advances time or owns a PAGFile, and the same PAGSurface may
 * be drawn into by multiple PAGFile instances in sequence.
 */
class PAGSurface {
 public:
  /**
   * Creates an offscreen surface with the given pixel dimensions. Returns nullptr if the size is
   * invalid or the platform backend is unavailable.
   * @param width  surface width in pixels, must be greater than zero.
   * @param height surface height in pixels, must be greater than zero.
   */
  static std::shared_ptr<PAGSurface> MakeOffscreen(int width, int height);

  virtual ~PAGSurface() = default;

  /**
   * Returns the surface width in pixels.
   */
  int width() const {
    return surfaceWidth;
  }

  /**
   * Returns the surface height in pixels.
   */
  int height() const {
    return surfaceHeight;
  }

 protected:
  PAGSurface(int width, int height) : surfaceWidth(width), surfaceHeight(height) {
  }

 private:
  int surfaceWidth = 0;
  int surfaceHeight = 0;
};

}  // namespace pagx
