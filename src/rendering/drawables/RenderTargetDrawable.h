/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
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

#include "Drawable.h"

namespace pag {

class RenderTargetDrawable : public Drawable {
 public:
  static std::shared_ptr<RenderTargetDrawable> MakeFrom(
      std::shared_ptr<tgfx::Device> device, const tgfx::BackendRenderTarget& renderTarget,
      tgfx::ImageOrigin origin);

  int width() const override {
    return renderTarget.width();
  }

  int height() const override {
    return renderTarget.height();
  }

  std::shared_ptr<tgfx::Device> getDevice() override {
    return device;
  }

 protected:
  std::shared_ptr<tgfx::Surface> onCreateSurface(tgfx::Context* context) override;

 private:
  std::shared_ptr<tgfx::Device> device = nullptr;
  tgfx::BackendRenderTarget renderTarget = {};
  tgfx::ImageOrigin origin = tgfx::ImageOrigin::TopLeft;

  RenderTargetDrawable(std::shared_ptr<tgfx::Device> device,
                       const tgfx::BackendRenderTarget& renderTarget, tgfx::ImageOrigin origin);
};
}  // namespace pag
