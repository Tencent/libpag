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
#include "tgfx/gpu/opengl/GLDevice.h"

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

  /**
   * Creates a Drawable that takes ownership of the given tgfx::Surface. The surface is returned
   * from getSurface() and kept alive for the lifetime of the Drawable.
   */
  static std::shared_ptr<Drawable> MakeFrom(std::shared_ptr<tgfx::Surface> surface);

  virtual int width() const {
    return surface ? surface->width() : 0;
  }
  virtual int height() const {
    return surface ? surface->height() : 0;
  }

  virtual std::shared_ptr<tgfx::Device> getDevice() {
    return tgfx::GLDevice::Current();
  }

  virtual std::shared_ptr<tgfx::Surface> getSurface(tgfx::Context* context) {
    // A surface created from a specific context cannot be used with a different one. Without this
    // guard, a PAGSurface built via pagx::MakeFrom(tgfx::Surface) would silently hand its surface
    // to whichever context PAGScene::draw() happens to lock, even if that context belongs to a
    // different device. Callers that need cross-device rendering should use pagx::Record() with
    // the matching context instead.
    if (surface && context != nullptr && surface->getContext() != context) {
      return nullptr;
    }
    return surface;
  }

  virtual void freeSurface() {
    surface = nullptr;
  }
  virtual void present(tgfx::Context* /*context*/) {
  }
  virtual void updateSize() {
  }

 protected:
  Drawable() = default;
  explicit Drawable(std::shared_ptr<tgfx::Surface> surface) : surface(std::move(surface)) {
  }

  std::shared_ptr<tgfx::Surface> surface = nullptr;
};

}  // namespace pagx
