/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "tgfx/core/ImageInfo.h"
#include "tgfx/gpu/Texture.h"

namespace tgfx {
/**
 * RenderTarget represents a 2D buffer of pixels that can be rendered to.
 */
class RenderTarget : public Resource {
 public:
  /**
   * Wraps a backend renderTarget into RenderTarget. The caller must ensure the backend renderTarget
   * is valid for the lifetime of the returned RenderTarget. Returns nullptr if the context is
   * nullptr or the backend renderTarget is invalid.
   */
  static std::shared_ptr<RenderTarget> MakeFrom(Context* context,
                                                const BackendRenderTarget& renderTarget,
                                                SurfaceOrigin origin);

  /**
   * Creates a new RenderTarget which uses specified Texture as pixel storage. The caller must
   * ensure the texture is valid for the lifetime of the returned RenderTarget.
   */
  static std::shared_ptr<RenderTarget> MakeFrom(const Texture* texture, int sampleCount = 1);

  /**
   * Returns the display width of the render target.
   */
  int width() const {
    return _width;
  }

  /**
   * Returns the display height of the render target.
   */
  int height() const {
    return _height;
  }

  /**
   * Returns the origin of the render target, either SurfaceOrigin::TopLeft or
   * SurfaceOrigin::BottomLeft.
   */
  SurfaceOrigin origin() const {
    return _origin;
  }

  /**
   * Returns the sample count of the render target.
   */
  int sampleCount() const {
    return _sampleCount;
  }

  /**
   * Returns the pixel format of the render target.
   */
  virtual PixelFormat format() const = 0;

  size_t memoryUsage() const override {
    return 0;
  }

  /**
   * Retrieves the backend render target.
   */
  virtual BackendRenderTarget getBackendRenderTarget() const = 0;

  /**
   * Copies a rect of pixels to dstPixels with specified color type, alpha type and row bytes. Copy
   * starts at (srcX, srcY), and does not exceed Surface (width(), height()). Pixels are copied
   * only if pixel conversion is possible. Returns true if pixels are copied to dstPixels.
   */
  virtual bool readPixels(const ImageInfo& dstInfo, void* dstPixels, int srcX = 0,
                          int srcY = 0) const = 0;

  /**
   * Replaces the backing texture of the render target with the specified Texture.
   */
  virtual bool replaceTexture(const Texture* texture) = 0;

 protected:
  RenderTarget(int width, int height, SurfaceOrigin origin, int sampleCount = 1)
      : _width(width), _height(height), _origin(origin), _sampleCount(sampleCount) {
  }

 private:
  virtual const Swizzle& writeSwizzle() const = 0;

  int _width = 0;
  int _height = 0;
  SurfaceOrigin _origin = SurfaceOrigin::TopLeft;
  int _sampleCount = 1;

  friend class DrawOp;
};
}  // namespace tgfx
