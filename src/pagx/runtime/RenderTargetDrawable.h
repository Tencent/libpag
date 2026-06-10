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

#include "pagx/runtime/Drawable.h"

namespace pagx {

/**
 * RenderTargetDrawable backs a PAGSurface with an externally created backend render target. The
 * device is obtained from GLDevice::Current() so that the PAGSurface shares the same GPU context
 * as the caller.
 */
class RenderTargetDrawable : public Drawable {
 public:
  static std::shared_ptr<RenderTargetDrawable> MakeFrom(
      std::shared_ptr<tgfx::Device> device, const tgfx::BackendRenderTarget& renderTarget,
      tgfx::ImageOrigin origin);

  int width() const override {
    return _renderTarget.width();
  }

  int height() const override {
    return _renderTarget.height();
  }

  std::shared_ptr<tgfx::Device> getDevice() override {
    return _device;
  }

  std::shared_ptr<tgfx::Surface> getSurface(tgfx::Context* context) override;

 private:
  RenderTargetDrawable(std::shared_ptr<tgfx::Device> device,
                       const tgfx::BackendRenderTarget& renderTarget, tgfx::ImageOrigin origin);

  std::shared_ptr<tgfx::Device> _device = nullptr;
  tgfx::BackendRenderTarget _renderTarget = {};
  tgfx::ImageOrigin _origin = tgfx::ImageOrigin::TopLeft;
};

}  // namespace pagx
