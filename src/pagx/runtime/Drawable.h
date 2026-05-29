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
#include "tgfx/core/Surface.h"
#include "tgfx/gpu/Device.h"

namespace pagx {

/**
 * Drawable is the platform-extension point that PAGSurface uses to talk to the underlying GPU
 * backend. Subclasses adapt platform-specific render targets (offscreen pixel buffers, web
 * canvases, native windows, hardware buffers, etc.) into the (Device, Surface) pair that the
 * tgfx renderer needs.
 *
 * This header lives inside the pagx implementation tree: public API only sees Drawable through
 * an opaque forward declaration in PAGSurface.h. Subclass authors must include this header to
 * implement the protocol; callers must obtain Drawable instances from each subclass's factory
 * (e.g. GPUDrawable::FromCanvasID on web).
 */
class Drawable {
 public:
  virtual ~Drawable() = default;

  virtual int width() const = 0;
  virtual int height() const = 0;

  /**
   * Returns the GPU device backing this drawable. May be lazily constructed; the returned device
   * remains valid for the lifetime of the drawable. Returns nullptr if the drawable is not yet
   * ready (e.g. zero-sized canvas) or the platform backend is unavailable.
   */
  virtual std::shared_ptr<tgfx::Device> getDevice() = 0;

  /**
   * Acquires a tgfx::Surface to render into. The context must be the one obtained from
   * getDevice()->lockContext() on the calling thread. Implementations may cache the surface
   * across calls and re-create it after updateSize(). Returns nullptr if the drawable cannot
   * provide a renderable surface.
   */
  virtual std::shared_ptr<tgfx::Surface> getSurface(tgfx::Context* context) = 0;

  /**
   * Releases any cached surface. Called when the drawable size changes or the caller wants to
   * reduce memory pressure.
   */
  virtual void freeSurface() {
    surface = nullptr;
  }

  /**
   * Presents the rendered content to the platform's display target. Default implementation is a
   * no-op; on-screen drawables (canvas, swap-chain) override to swap buffers.
   */
  virtual void present(tgfx::Context* /*context*/) {
  }

  /**
   * Synchronizes the drawable's logical size with the underlying platform target. Called by
   * PAGSurface when the canvas may have been resized externally.
   */
  virtual void updateSize() {
  }

 protected:
  std::shared_ptr<tgfx::Surface> surface = nullptr;
};

}  // namespace pagx
