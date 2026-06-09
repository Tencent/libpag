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
 * OffscreenDrawable backs PAGSurface::MakeOffscreen with a fresh OpenGL device and a transient
 * tgfx::Surface sized to the requested pixel dimensions. It is the default Drawable used by
 * unit tests and any caller that just needs to read pixels back via PAGSurface::readPixels.
 */
class OffscreenDrawable : public Drawable {
 public:
  static std::shared_ptr<OffscreenDrawable> Make(int width, int height);

  int width() const override {
    return _width;
  }

  int height() const override {
    return _height;
  }

  std::shared_ptr<tgfx::Device> getDevice() override;
  std::shared_ptr<tgfx::Surface> getSurface(tgfx::Context* context) override;

 private:
  OffscreenDrawable(int width, int height, std::shared_ptr<tgfx::Device> device);

  int _width = 0;
  int _height = 0;
  std::shared_ptr<tgfx::Device> _device = nullptr;
};

}  // namespace pagx
