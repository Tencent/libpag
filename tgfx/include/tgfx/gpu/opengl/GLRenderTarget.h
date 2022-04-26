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
#include "tgfx/gpu/RenderTarget.h"
#include "tgfx/gpu/opengl/GLFrameBuffer.h"
#include "tgfx/gpu/opengl/GLTexture.h"

namespace tgfx {
class GLInterface;

/**
 * Represents a OpenGL 2D buffer of pixels that can be rendered to.
 */
class GLRenderTarget : public RenderTarget {
 public:
  /**
   * Wraps an OpenGL frame buffer into GLRenderTarget. Please pass the number of samples-per-pixel
   * in the sampleCount parameter if the frame buffer was constructed with MSAA enabled. Caller must
   * ensure the backend frame buffer is valid for the lifetime of returned GLRenderTarget.
   */
  static std::shared_ptr<GLRenderTarget> MakeFrom(Context* context,
                                                  const GLFrameBuffer& frameBuffer, int width,
                                                  int height, ImageOrigin origin,
                                                  int sampleCount = 1);

  /**
   * Wraps an OpenGL frame buffer into GLRenderTarget. Please pass the number of samples-per-pixel
   * in the sampleCount parameter if the frame buffer was constructed with MSAA enabled. The
   * returned GLRenderTarget takes ownership of the backend frame buffer.
   */
  static std::shared_ptr<GLRenderTarget> MakeAdopted(Context* context,
                                                     const GLFrameBuffer& frameBuffer, int width,
                                                     int height, ImageOrigin origin,
                                                     int sampleCount = 1);

  /**
   * Returns the GLFrameBuffer associated with this render target.
   */
  GLFrameBuffer glFrameBuffer() const {
    return renderTargetFBInfo;
  }

 private:
  GLFrameBuffer textureFBInfo = {};
  GLFrameBuffer renderTargetFBInfo = {};
  unsigned msRenderBufferID = 0;
  unsigned textureTarget = 0;
  bool externalTexture = false;

  /**
   * Creates a new render target which uses specified texture as pixel storage. Caller must ensure
   * texture is valid for the lifetime of returned render target.
   */
  static std::shared_ptr<GLRenderTarget> MakeFrom(const GLTexture* texture, int sampleCount = 1);

  GLRenderTarget(int width, int height, ImageOrigin origin, int sampleCount,
                 GLFrameBuffer frameBuffer, unsigned textureTarget = 0);

  void onReleaseGPU() override;

  void clear() const;

  void resolve() const;

  /**
   * Copies a rect of pixels to dstPixels with specified color type, alpha type and row bytes. Copy
   * starts at (srcX, srcY), and does not exceed Surface (width(), height()). Pixels are copied
   * only if pixel conversion is possible. Returns true if pixels are copied to dstPixels.
   */
  bool readPixels(const ImageInfo& dstInfo, void* dstPixels, int srcX = 0, int srcY = 0) const;

  friend class GLSurface;

  friend class GLCanvas;

  friend class Surface;
};
}  // namespace tgfx
