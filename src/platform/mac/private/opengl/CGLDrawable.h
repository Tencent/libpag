/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "rendering/drawables/Drawable.h"
#include "tgfx/gpu/opengl/cgl/CGLWindow.h"

namespace pag {

extern NSString* const AsyncSurfacePreparedNotification;

class CGLDrawable : public Drawable {
 public:
  static std::shared_ptr<CGLDrawable> FromView(NSView* view);

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
  std::weak_ptr<CGLDrawable> weakThis;
  int _width = 0;
  int _height = 0;
  NSView* view = nil;
  std::shared_ptr<tgfx::CGLWindow> window = nullptr;
  std::atomic<bool> bufferPreparing = false;

  explicit CGLDrawable(NSView* view);

  void tryCreateSurface();
};
}  // namespace pag
