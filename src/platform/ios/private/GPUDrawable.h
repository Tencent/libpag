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

#include "rendering/drawables/Drawable.h"
#include "tgfx/opengl/eagl/EAGLWindow.h"

namespace pag {

class GPUDrawable : public Drawable {
 public:
  static std::shared_ptr<GPUDrawable> FromLayer(CAEAGLLayer* layer);

  int width() const override;

  int height() const override;

  void updateSize() override;

  void present(tgfx::Context* context) override;

 protected:
  std::shared_ptr<tgfx::Device> onCreateDevice() override;

  std::shared_ptr<tgfx::Surface> onCreateSurface(tgfx::Context* context) override;

 private:
  std::weak_ptr<GPUDrawable> weakThis;
  int _width = 0;
  int _height = 0;
  CAEAGLLayer* layer = nil;
  std::shared_ptr<tgfx::EAGLWindow> window = nullptr;

  explicit GPUDrawable(CAEAGLLayer* layer);
  void tryCreateSurface();
};
}  // namespace pag
