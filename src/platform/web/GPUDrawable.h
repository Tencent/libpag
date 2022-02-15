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

#include "gpu/opengl/webgl//WebGLWindow.h"
#include "pag/pag.h"

namespace pag {
class GPUDrawable : public Drawable {
 public:
  static std::shared_ptr<GPUDrawable> FromCanvasID(const std::string& canvasID);

  int width() const override {
    return _width;
  }

  int height() const override {
    return _height;
  }

  void updateSize() override;

  std::shared_ptr<Device> getDevice() override;

  std::shared_ptr<Surface> createSurface(Context* context) override;

  void present(Context*) override {
  }

 private:
  explicit GPUDrawable(std::string canvasID);

  std::string canvasID;
  int _width = 0;
  int _height = 0;
  std::shared_ptr<WebGLWindow> window = nullptr;
};
}  // namespace pag
