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

#include "Drawable.h"

namespace pag {
std::shared_ptr<tgfx::Surface> Drawable::getSurface(tgfx::Context* context, bool queryOnly) {
  if (context == nullptr) {
    return nullptr;
  }
  if (!queryOnly && surface == nullptr) {
    surface = onCreateSurface(context);
  }
  return surface;
}

std::shared_ptr<tgfx::Surface> Drawable::getFrontSurface(tgfx::Context* context, bool queryOnly) {
  return getSurface(context, queryOnly);
}

void Drawable::freeSurface() {
  surface = nullptr;
  onFreeSurface();
}

void Drawable::updateSize() {
}

void Drawable::present(tgfx::Context*) {
}

void Drawable::setTimeStamp(int64_t) {
}

void Drawable::onFreeSurface() {
}
}  // namespace pag
