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

#include "rendering/drawables/Drawable.h"

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

  std::shared_ptr<tgfx::Device> getDevice() override {
    return device;
  }

  std::shared_ptr<tgfx::Surface> getFrontSurface(tgfx::Context* context, bool queryOnly) override;

  void present(tgfx::Context* context) override;

 protected:
  std::shared_ptr<tgfx::Surface> onCreateSurface(tgfx::Context* context) override;

  void onFreeSurface() override;

 private:
  int _width;
  int _height;
  bool tryHardware;
  std::shared_ptr<tgfx::Device> device = nullptr;
  std::shared_ptr<tgfx::Surface> frontSurface;

  DoubleBufferedDrawable(int width, int height, bool tryHardware,
                         std::shared_ptr<tgfx::Device> device);

  std::shared_ptr<tgfx::Surface> makeSurface(tgfx::Context* context) const;
};
}  // namespace pag
