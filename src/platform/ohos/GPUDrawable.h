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

#include <ace/xcomponent/native_interface_xcomponent.h>
#include "rendering/drawables/Drawable.h"
#include "tgfx/opengl/egl/EGLWindow.h"

namespace pag {

class GPUDrawable : public Drawable {
 public:
  static std::shared_ptr<GPUDrawable> FromXComponent(OH_NativeXComponent* xComponent, void* window);

  ~GPUDrawable() override;

  int width() const override {
    return _width;
  }

  int height() const override {
    return _height;
  }

  std::shared_ptr<tgfx::Device> getDevice() override;

  void updateSize() override;

  void present(tgfx::Context* context) override;

  void setTimeStamp(int64_t timeStamp) override;

 protected:
  std::shared_ptr<tgfx::Surface> onCreateSurface(tgfx::Context* context) override;

  void onFreeSurface() override;

 private:
  int _width = 0;
  int _height = 0;

  OH_NativeXComponent* xComponent = nullptr;
  void* nativeWindow = nullptr;
  int64_t currentTimeStamp = 0;
  std::shared_ptr<tgfx::EGLWindow> window = nullptr;

  explicit GPUDrawable(OH_NativeXComponent* xComponent, void* window);
};
}  // namespace pag
