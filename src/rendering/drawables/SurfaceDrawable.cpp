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

#include "SurfaceDrawable.h"
#include "tgfx/core/Canvas.h"
#include "tgfx/core/Paint.h"

namespace pag {
bool SurfaceDrawable::needsPresentationCopy() const {
  return true;
}

std::shared_ptr<tgfx::Surface> SurfaceDrawable::onCreateSurface(tgfx::Context* context) {
  auto window = getWindow();
  if (window == nullptr) {
    return nullptr;
  }
  // Creating a window Surface records a clear, so querying it here could publish rejected work.
  return tgfx::Surface::Make(context, width(), height(), false, 1, false, 0, window->colorSpace());
}

bool SurfaceDrawable::preparePresent(tgfx::Context* context) {
  if (context == nullptr || surface == nullptr) {
    return false;
  }
  auto image = surface->makeImageSnapshot();
  if (image == nullptr) {
    return false;
  }
  auto windowSurface = tgfx::Surface::MakeFrom(context, getWindow());
  if (windowSurface == nullptr) {
    return false;
  }
  tgfx::Paint paint = {};
  paint.setBlendMode(tgfx::BlendMode::Src);
  // A resize can change native dimensions before staging is recreated. Replace the entire current
  // frame so its initial clear cannot escape as a partial or rejected presentation.
  windowSurface->getCanvas()->drawImageRect(
      std::move(image), tgfx::Rect::MakeWH(windowSurface->width(), windowSurface->height()),
      tgfx::SamplingOptions(tgfx::FilterMode::Nearest), &paint);
  return true;
}
}  // namespace pag
