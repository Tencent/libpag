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

// The CAEAGLLayer declarations below are deprecated since iOS 12 but remain part of the published
// GL-backend API surface. Silence the deprecation warnings locally: this header is included from
// platform files compiled without libpag's GLES_SILENCE_DEPRECATION build define, and defining
// that macro here would leak into the whole including translation unit.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

#include "rendering/drawables/Drawable.h"
#include "tgfx/gpu/opengl/eagl/EAGLWindow.h"

namespace pag {

extern NSString* const AsyncSurfacePreparedNotification;

class EAGLDrawable : public Drawable {
 public:
  static std::shared_ptr<EAGLDrawable> FromLayer(CAEAGLLayer* layer);

  int width() const override;

  int height() const override;

  std::shared_ptr<tgfx::Device> getDevice() override;

  void updateSize() override;

  void present(tgfx::Context* context) override;

 protected:
  std::shared_ptr<tgfx::Surface> onCreateSurface(tgfx::Context* context) override;

  void onFreeSurface() override;

 private:
  std::weak_ptr<EAGLDrawable> weakThis;
  int _width = 0;
  int _height = 0;
  CAEAGLLayer* layer = nil;
  std::shared_ptr<tgfx::EAGLWindow> window = nullptr;
  std::atomic<bool> bufferPreparing = false;

  explicit EAGLDrawable(CAEAGLLayer* layer);

  void tryCreateSurface();
};
}  // namespace pag

#pragma clang diagnostic pop
