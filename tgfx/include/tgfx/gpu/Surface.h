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

namespace tgfx {
/**
 * Describes options of a given Surface. The caller can add some custom options to its subclass and
 * parse them during drawing, which can sometimes optimize the rendering performance (e.g. disabling
 * an expensive feature).
 */
class SurfaceOptions {
 public:
  virtual ~SurfaceOptions() = default;
};

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
                                       bool alphaOnly = false, int sampleCount = 1);

  /**
   * Wraps a render target into Surface. Returns nullptr if renderTarget is nullptr.
   */
  static std::shared_ptr<Surface> MakeFrom(std::shared_ptr<RenderTarget> renderTarget);

  /**
   * Wraps a texture into Surface. A Surface with MSAA enabled is returned if the sampleCount is
   * greater than 1. Returns nullptr if the specified texture is not renderable.
   */
  static std::shared_ptr<Surface> MakeFrom(std::shared_ptr<Texture> texture, int sampleCount = 1);

  explicit Surface(Context* context);

  virtual ~Surface() = default;

  /**
   * Retrieves the context associated with this Surface.
   */
  Context* getContext() const {
    return context;
  }

  /**
   * Returns SurfaceOptions for this surface.
   */
  const SurfaceOptions* options() const {
    return surfaceOptions.get();
  }

  /**
   * Sets the options property for this surface.
   */
  void setOptions(std::unique_ptr<SurfaceOptions> newOptions) {
    surfaceOptions = std::move(newOptions);
  }

  /**
   * Returns the width of this surface.
   */
  virtual int width() const = 0;

  /**
   * Returns the height of this surface.
   */
  virtual int height() const = 0;

  /**
   * Returns the origin of this surface, either ImageOrigin::TopLeft or ImageOrigin::BottomLeft.
   */
  virtual ImageOrigin origin() const = 0;

  /**
   * Retrieves the render target that the surface renders to.
   */
  virtual std::shared_ptr<RenderTarget> getRenderTarget() const = 0;

  /**
   * Retrieves the texture that the surface renders to. Return nullptr if the surface was made from
   * a RenderTarget.
   */
  virtual std::shared_ptr<Texture> getTexture() const = 0;

  /**
   * Returns Canvas that draws into Surface. Subsequent calls return the same Canvas. Canvas
   * returned is managed and owned by Surface, and is deleted when Surface is deleted.
   */
  virtual Canvas* getCanvas() = 0;

  /**
   * Inserts a GPU semaphore that the current GPU-backed API must wait on before executing any more
   * commands on the GPU for this surface. Surface will take ownership of the underlying semaphore
   * and delete it once it has been signaled and waited on. If this call returns false, then the
   * GPU back-end will not wait on the passed semaphore, and the client will still own the
   * semaphore. Returns true if GPU is waiting on the semaphore.
   */
  virtual bool wait(const Semaphore* waitSemaphore) = 0;

  /**
   * Apply all pending changes to the render target immediately. After issuing all commands, the
   * semaphore will be signaled by the GPU. If the signalSemaphore is not null and uninitialized,
   * a new semaphore is created and initializes BackendSemaphore. The caller must delete the
   * semaphore returned in signalSemaphore. BackendSemaphore can be deleted as soon as this function
   * returns. If the back-end API is OpenGL only uninitialized backend semaphores are supported.
   * If false is returned, the GPU back-end did not create or add a semaphore to signal on the GPU;
   * the caller should not instruct the GPU to wait on the semaphore.
   */
  virtual bool flush(Semaphore* signalSemaphore) = 0;

  /**
   * Apply all pending changes to the render target immediately.
   */
  void flush() {
    flush(nullptr);
  }

  /**
   * Copies a rect of pixels to dstPixels with specified ImageInfo. Copy starts at (srcX, srcY), and
   * does not exceed Surface (width(), height()). Pixels are copied only if pixel conversion is
   * possible. Returns true if pixels are copied to dstPixels.
   */
  bool readPixels(const ImageInfo& dstInfo, void* dstPixels, int srcX = 0, int srcY = 0) const;

  /**
   * Evaluates the Surface to see if it overlaps or intersects with the specified point. The point
   * is in the coordinate space of the Surface. This method always checks against the actual pixels
   * of the Surface.
   */
  bool hitTest(float x, float y) const;

 protected:
  Context* context = nullptr;
  
  virtual bool onReadPixels(const ImageInfo& dstInfo, void* dstPixels, int srcX,
                            int srcY) const = 0;

 private:
  std::unique_ptr<SurfaceOptions> surfaceOptions = nullptr;
};
}  // namespace tgfx