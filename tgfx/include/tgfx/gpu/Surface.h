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
#include "tgfx/gpu/Canvas.h"
#include "tgfx/gpu/RenderTarget.h"
#include "tgfx/gpu/Semaphore.h"
#include "tgfx/gpu/SurfaceOptions.h"

namespace tgfx {

class Canvas;

class Context;

/**
 * Surface is responsible for managing the pixels that a canvas draws into. Surface takes care of
 * allocating a Canvas that will draw into the surface. Call surface->getCanvas() to use that
 * canvas (but don't delete it, it is owned by the surface). Surface always has non-zero dimensions.
 * If there is a request for a new surface, and either of the requested dimensions are zero,
 * then nullptr will be returned.
 */
class Surface {
 public:
  /**
   * Creates a new Surface on GPU indicated by context. Allocates memory for pixels, based on the
   * width, height and color type (alphaOnly). A Surface with MSAA enabled is returned if the
   * sampleCount is greater than 1. Return nullptr if alphaOnly is not supported or the size is
   * zero.
   */
  static std::shared_ptr<Surface> Make(Context* context, int width, int height,
                                       bool alphaOnly = false, int sampleCount = 1,
                                       bool mipMapped = false,
                                       const SurfaceOptions* options = nullptr);

  /**
   * Wraps a render target into Surface. Returns nullptr if renderTarget is nullptr.
   */
  static std::shared_ptr<Surface> MakeFrom(std::shared_ptr<RenderTarget> renderTarget,
                                           const SurfaceOptions* options = nullptr);

  /**
   * Wraps a texture into Surface. A Surface with MSAA enabled is returned if the sampleCount is
   * greater than 1. Returns nullptr if the specified texture is not renderable.
   */
  static std::shared_ptr<Surface> MakeFrom(std::shared_ptr<Texture> texture, int sampleCount = 1,
                                           const SurfaceOptions* options = nullptr);

  virtual ~Surface();

  /**
   * Retrieves the context associated with this Surface.
   */
  Context* getContext() const {
    return renderTarget->getContext();
  }

  /**
   * Returns the SurfaceOptions of the Surface.
   */
  const SurfaceOptions* options() const {
    return &surfaceOptions;
  }

  /**
   * Returns the width of this surface.
   */
  int width() const {
    return renderTarget->width();
  }

  /**
   * Returns the height of this surface.
   */
  int height() const {
    return renderTarget->height();
  }

  /**
   * Returns the origin of this surface, either ImageOrigin::TopLeft or ImageOrigin::BottomLeft.
   */
  ImageOrigin origin() const {
    return renderTarget->origin();
  }

  /**
   * Retrieves the render target that the surface renders to.
   */
  std::shared_ptr<RenderTarget> getRenderTarget();

  /**
   * Retrieves the texture that the surface renders to. Return nullptr if the surface was made from
   * a RenderTarget.
   */
  std::shared_ptr<Texture> getTexture();

  /**
   * Returns Canvas that draws into Surface. Subsequent calls return the same Canvas. Canvas
   * returned is managed and owned by Surface, and is deleted when Surface is deleted.
   */
  Canvas* getCanvas();

  /**
   * Inserts a GPU semaphore that the current GPU-backed API must wait on before executing any more
   * commands on the GPU for this surface. Surface will take ownership of the underlying semaphore
   * and delete it once it has been signaled and waited on. If this call returns false, then the
   * GPU back-end will not wait on the passed semaphore, and the client will still own the
   * semaphore. Returns true if GPU is waiting on the semaphore.
   */
  bool wait(const Semaphore* waitSemaphore);

  /**
   * Apply all pending changes to the render target immediately. After issuing all commands, the
   * semaphore will be signaled by the GPU. If the signalSemaphore is not null and uninitialized,
   * a new semaphore is created and initializes BackendSemaphore. The caller must delete the
   * semaphore returned in signalSemaphore. BackendSemaphore can be deleted as soon as this function
   * returns. If the back-end API is OpenGL only uninitialized backend semaphores are supported.
   * If false is returned, the GPU back-end did not create or add a semaphore to signal on the GPU;
   * the caller should not instruct the GPU to wait on the semaphore.
   */
  bool flush(Semaphore* signalSemaphore = nullptr);

  /**
   * Copies a rect of pixels to dstPixels with specified ImageInfo. Copy starts at (srcX, srcY), and
   * does not exceed Surface (width(), height()). Pixels are copied only if pixel conversion is
   * possible. Returns true if pixels are copied to dstPixels.
   */
  bool readPixels(const ImageInfo& dstInfo, void* dstPixels, int srcX = 0, int srcY = 0);

  /**
   * Evaluates the Surface to see if it overlaps or intersects with the specified point. The point
   * is in the coordinate space of the Surface. This method always checks against the actual pixels
   * of the Surface.
   */
  bool hitTest(float x, float y);

 protected:
  Surface(std::shared_ptr<RenderTarget> renderTarget, std::shared_ptr<Texture> texture,
          const SurfaceOptions* options);

  virtual bool onReadPixels(const ImageInfo& dstInfo, void* dstPixels, int srcX, int srcY) = 0;

  std::shared_ptr<RenderTarget> renderTarget = nullptr;
  std::shared_ptr<Texture> texture = nullptr;
  bool requiresManualMSAAResolve = false;

 private:
  SurfaceOptions surfaceOptions = {};
  Canvas* canvas = nullptr;

  friend class DrawingManager;

  friend class Canvas;
};
}  // namespace tgfx