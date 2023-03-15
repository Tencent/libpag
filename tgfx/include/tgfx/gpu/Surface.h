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

#include "tgfx/core/Canvas.h"
#include "tgfx/core/ImageInfo.h"
#include "tgfx/gpu/Backend.h"
#include "tgfx/gpu/ImageOrigin.h"
#include "tgfx/gpu/SurfaceOptions.h"

namespace tgfx {
class Canvas;
class Context;
class RenderTarget;

/**
 * The Surface class is responsible for managing the pixels that a Canvas draws into. The Surface
 * takes care of allocating a Canvas that will draw into the surface. Call surface->getCanvas() to
 * use that Canvas (but don't delete it, it is owned by the surface). The Surface always has
 * non-zero dimensions. If there is a request for a new surface, and either of the requested
 * dimensions is zero, then nullptr will be returned.
 */
class Surface {
 public:
  /**
   * Creates a new Surface on GPU indicated by context. Allocates memory for pixels based on the
   * width, height, and color type (alpha only). A Surface with MSAA enabled is returned if the
   * sample count is greater than 1. Return nullptr if the size is invalid or the alpha-only
   * textures are not renderable in the GPU backend.
   */
  static std::shared_ptr<Surface> Make(Context* context, int width, int height,
                                       bool alphaOnly = false, int sampleCount = 1,
                                       bool mipMapped = false,
                                       const SurfaceOptions* options = nullptr);

  /**
   * Wraps a backend render target into Surface. The caller must ensure the renderTarget is valid
   * for the lifetime of the returned Surface. Returns nullptr if the context is nullptr or the
   * renderTarget is invalid.
   */
  static std::shared_ptr<Surface> MakeFrom(Context* context,
                                           const BackendRenderTarget& renderTarget,
                                           ImageOrigin origin,
                                           const SurfaceOptions* options = nullptr);

  /**
   * Wraps a BackendTexture into Surface. The caller must ensure the texture is valid for the
   * lifetime of the returned Surface. If the sampleCount greater than zero, creates an intermediate
   * MSAA Surface which is used for drawing backendTexture. Returns nullptr if the context is
   * nullptr or the texture is not renderable in the GPU backend.
   */
  static std::shared_ptr<Surface> MakeFrom(Context* context, const BackendTexture& backendTexture,
                                           ImageOrigin origin, int sampleCount = 1,
                                           const SurfaceOptions* options = nullptr);

  /**
   * Creates a Surface from the platform-specific hardware buffer. For example, the hardware buffer
   * could be an AHardwareBuffer on the android platform or a CVPixelBufferRef on the apple
   * platform. The returned Surface takes a reference to the hardwareBuffer. Returns nullptr if the
   * context is nullptr or the hardwareBuffer is not renderable.
   */
  static std::shared_ptr<Surface> MakeFrom(Context* context, HardwareBufferRef hardwareBuffer,
                                           int sampleCount = 1,
                                           const SurfaceOptions* options = nullptr);

  virtual ~Surface();

  /**
   * Retrieves the context associated with this Surface.
   */
  Context* getContext() const;

  /**
   * Returns the SurfaceOptions of the Surface.
   */
  const SurfaceOptions* options() const {
    return &surfaceOptions;
  }

  /**
   * Returns the width of this surface.
   */
  int width() const;

  /**
   * Returns the height of this surface.
   */
  int height() const;

  /**
   * Returns the origin of this surface, either SurfaceOrigin::TopLeft or SurfaceOrigin::BottomLeft.
   */
  ImageOrigin origin() const;

  /**
   * Retrieves the backend render target that the surface renders to. The returned
   * BackendRenderTarget should be discarded if the Surface is drawn to or deleted.
   */
  BackendRenderTarget getBackendRenderTarget();

  /**
   * Retrieves the backend texture that the surface renders to. Returns an invalid BackendTexture if
   * the surface has no backend texture. The returned BackendTexture should be discarded if the
   * Surface is drawn to or deleted.
   */
  BackendTexture getBackendTexture();

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
  bool wait(const BackendSemaphore& waitSemaphore);

  /**
   * Apply all pending changes to the render target immediately. After issuing all commands, the
   * semaphore will be signaled by the GPU. If the signalSemaphore is not null and uninitialized,
   * a new semaphore is created and initializes BackendSemaphore. The caller must delete the
   * semaphore returned in signalSemaphore. BackendSemaphore can be deleted as soon as this function
   * returns. If the back-end API is OpenGL only uninitialized backend semaphores are supported.
   * If false is returned, the GPU back-end did not create or add a semaphore to signal on the GPU;
   * the caller should not instruct the GPU to wait on the semaphore.
   */
  bool flush(BackendSemaphore* signalSemaphore = nullptr);

  /**
   * Call to ensure all reads/writes of the surface have been issued to the underlying 3D API.
   * TGFX will correctly order its own draws and pixel operations. This must be used to ensure
   * correct ordering when the surface backing store is accessed outside TGFX (e.g. direct use of
   * the 3D API or a windowing system). Context has additional flush and submit methods that apply
   * to all surfaces and images created from a Context. This is equivalent to calling
   * Surface::flush() followed by Context::submit(syncCpu).
   */
  void flushAndSubmit(bool syncCpu = false);

  /**
   * Returns an Image capturing the Surface contents. Subsequent drawings to the Surface contents
   * are not captured. This method would trigger immediate texture copying if the Surface has no
   * backing texture or the backing texture was allocated externally. For example, the Surface was
   * created from a BackendRenderTarget, a BackendTexture or a HardwareBuffer.
   */
  std::shared_ptr<Image> makeImageSnapshot();

  /**
   * Returns pixel at (x, y) as unpremultiplied color. Some color precision may be lost in the
   * conversion to unpremultiplied color; original pixel data may have additional precision. Returns
   * a transparent color if the point (x, y) is not contained by the Surface bounds.
   * @param x  column index, zero or greater, and less than width()
   * @param y  row index, zero or greater, and less than height()
   * @return   pixel converted to unpremultiplied color
   */
  Color getColor(int x, int y);

  /**
   * Copies a rect of pixels to dstPixels with specified ImageInfo. Copy starts at (srcX, srcY), and
   * does not exceed Surface (width(), height()). Pixels are copied only if pixel conversion is
   * possible. Returns true if pixels are copied to dstPixels.
   */
  bool readPixels(const ImageInfo& dstInfo, void* dstPixels, int srcX = 0, int srcY = 0);

 private:
  std::shared_ptr<RenderTarget> renderTarget = nullptr;
  std::shared_ptr<Texture> texture = nullptr;
  SurfaceOptions surfaceOptions = {};
  bool externalTexture = false;
  Canvas* canvas = nullptr;
  std::shared_ptr<Image> cachedImage = nullptr;

  static std::shared_ptr<Surface> MakeFrom(std::shared_ptr<RenderTarget> renderTarget,
                                           const SurfaceOptions* options = nullptr);

  static std::shared_ptr<Surface> MakeFrom(std::shared_ptr<Texture> texture, int sampleCount = 1,
                                           const SurfaceOptions* options = nullptr);

  Surface(std::shared_ptr<RenderTarget> renderTarget, std::shared_ptr<Texture> texture,
          const SurfaceOptions* options, bool externalTexture = true);

  std::shared_ptr<Texture> getTexture();

  void aboutToDraw(bool discardContent = false);

  friend class DrawingManager;
  friend class Canvas;
  friend class QGLWindow;
};
}  // namespace tgfx