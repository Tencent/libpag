/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "rendering/drawables/Drawable.h"
#include "tgfx/gpu/DoubleBufferedWindow.h"

namespace pag {
class DoubleBufferedDrawable : public Drawable {
 public:
  static std::shared_ptr<DoubleBufferedDrawable> Make(int width, int height, bool tryHardware,
                                                      std::shared_ptr<tgfx::Device> device);

  int width() const override {
    return _width;
  }

  int height() const override {
    return _height;
  }

  void present(tgfx::Context* context) override;

 protected:
  explicit DoubleBufferedDrawable(std::shared_ptr<tgfx::DoubleBufferedWindow> window);

  std::shared_ptr<tgfx::Surface> getFrontSurface() const override {
    return window->getFrontSurface();
  }

  std::shared_ptr<tgfx::Surface> getBackSurface() const override {
    return window->getBackSurface();
  }

  std::shared_ptr<tgfx::Surface> onCreateSurface(tgfx::Context* context) override;

  std::shared_ptr<tgfx::Device> onCreateDevice() override;

  int _width = 0;
  int _height = 0;
  std::shared_ptr<tgfx::DoubleBufferedWindow> window;

 private:
  DoubleBufferedDrawable(int width, int height, std::shared_ptr<tgfx::DoubleBufferedWindow> window);
};
}  // namespace pag
