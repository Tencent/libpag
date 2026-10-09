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
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#if defined(TGFX_USE_OPENGL)

#include "rendering/drawables/Drawable.h"
#include "tgfx/gpu/opengl/eagl/EAGLWindow.h"

// The CAEAGLLayer declarations below are deprecated since iOS 12 but remain part of the published
// GL-backend API surface. Silence the deprecation warnings locally: this header is included from
// platform files compiled without libpag's GLES_SILENCE_DEPRECATION build define, and defining
// that macro here would leak into the whole including translation unit.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

namespace pag {

extern NSString* const AsyncSurfacePreparedNotification;

class GPUDrawable : public Drawable {
 public:
  static std::shared_ptr<GPUDrawable> FromLayer(CAEAGLLayer* layer);

  int width() const override;

  int height() const override;

  std::shared_ptr<tgfx::Device> getDevice() override;

  void updateSize() override;

  void present(tgfx::Context* context) override;

 protected:
  std::shared_ptr<tgfx::Surface> onCreateSurface(tgfx::Context* context) override;

  void onFreeSurface() override;

 private:
  std::weak_ptr<GPUDrawable> weakThis;
  int _width = 0;
  int _height = 0;
  CAEAGLLayer* layer = nil;
  std::shared_ptr<tgfx::EAGLWindow> window = nullptr;
  std::atomic<bool> bufferPreparing = false;

  explicit GPUDrawable(CAEAGLLayer* layer);

  void tryCreateSurface();
};
}  // namespace pag

#pragma clang diagnostic pop

#endif  // TGFX_USE_OPENGL
