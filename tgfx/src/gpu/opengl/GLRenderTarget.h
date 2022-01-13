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

#include "GLState.h"
#include "GLTexture.h"
#include "image/ImageInfo.h"

namespace pag {
/**
 * GLRenderTarget represents a 2D buffer of pixels that can be rendered to.
 */
class GLRenderTarget : public Resource {
 public:
  /**
   * Wraps a BackendRenderTarget into GLRenderTarget. Caller must ensure the BackendRenderTarget is
   * valid for the lifetime of returned render target.
   */
  static std::shared_ptr<GLRenderTarget> MakeFrom(Context* context,
                                                  const BackendRenderTarget& renderTarget,
                                                  ImageOrigin origin);

  /**
   * Creates a new render target which uses specified texture as pixel storage. Caller must ensure
   * texture is valid for the lifetime of returned render target.
   */
  static std::shared_ptr<GLRenderTarget> MakeFrom(Context* context, GLTexture* texture,
                                                  int sampleCount = 1);

  /**
   * Returns the display width of this render target.
   */
  int width() const {
    return _width;
  }

  /**
   * Returns the display height of this render target.
   */
  int height() const {
    return _height;
  }

  /**
   * Returns the origin of this render target, either ImageOrigin::TopLeft or
   * ImageOrigin::BottomLeft.
   */
  ImageOrigin origin() const {
    return _origin;
  }

  GLFrameBufferInfo getGLInfo() const {
    return renderTargetFBInfo;
  }

  int sampleCount() const {
    return _sampleCount;
  }

  bool usesMSAA() const {
    return _sampleCount > 1;
  }

  /**
   * Replacing all pixels with transparent color.
   */
  void clear(const GLInterface* gl) const;

  /**
   * Copies a rect of pixels to dstPixels with specified color type, alpha type and row bytes. Copy
   * starts at (srcX, srcY), and does not exceed Surface (width(), height()). Pixels are copied
   * only if pixel conversion is possible. Returns true if pixels are copied to dstPixels.
   */
  bool readPixels(Context* context, const ImageInfo& dstInfo, void* dstPixels, int srcX = 0,
                  int srcY = 0) const;

  void resolve(Context* context) const;

 protected:
  void onRelease(Context* context) override;

 private:
  int _width = 0;
  int _height = 0;
  ImageOrigin _origin = ImageOrigin::TopLeft;
  int _sampleCount = 1;
  GLFrameBufferInfo textureFBInfo = {};
  GLFrameBufferInfo renderTargetFBInfo = {};
  unsigned msRenderBufferID = 0;
  unsigned textureTarget = 0;

  GLRenderTarget(int width, int height, ImageOrigin origin, GLFrameBufferInfo info,
                 unsigned textureTarget = 0);

  friend class GLSurface;
};
}  // namespace pag
