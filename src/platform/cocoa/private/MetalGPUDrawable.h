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

#if defined(TGFX_USE_METAL)

#import <QuartzCore/QuartzCore.h>
#include "rendering/drawables/Drawable.h"
#include "tgfx/gpu/metal/MetalWindow.h"

namespace pag {

/**
 * Drawable that renders into an externally owned CAMetalLayer. Shared between iOS and macOS
 * since Metal's CAMetalLayer API is identical across the two platforms — only the host layer
 * type name differs at the OC-facade level.
 */
class MetalGPUDrawable : public Drawable {
 public:
  static std::shared_ptr<MetalGPUDrawable> FromLayer(CAMetalLayer* layer);

  int width() const override {
    return _width;
  }

  int height() const override {
    return _height;
  }

  std::shared_ptr<tgfx::Device> getDevice() override;

  void updateSize() override;

  void present(tgfx::Context* context) override;

 protected:
  std::shared_ptr<tgfx::Surface> onCreateSurface(tgfx::Context* context) override;

  void onFreeSurface() override;

 private:
  int _width = 0;
  int _height = 0;
  CAMetalLayer* layer = nil;
  std::shared_ptr<tgfx::MetalWindow> window = nullptr;

  explicit MetalGPUDrawable(CAMetalLayer* layer);
};

}  // namespace pag

#endif  // TGFX_USE_METAL
