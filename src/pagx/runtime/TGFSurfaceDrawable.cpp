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

#include "TGFSurfaceDrawable.h"
#include "tgfx/core/Surface.h"
#include "tgfx/gpu/opengl/GLDevice.h"

namespace pagx {

TGFSurfaceDrawable::TGFSurfaceDrawable(std::shared_ptr<tgfx::Surface> surface)
    : ownedSurface(std::move(surface)) {
}

int TGFSurfaceDrawable::width() const {
  return ownedSurface ? ownedSurface->width() : 0;
}

int TGFSurfaceDrawable::height() const {
  return ownedSurface ? ownedSurface->height() : 0;
}

std::shared_ptr<tgfx::Device> TGFSurfaceDrawable::getDevice() {
  return tgfx::GLDevice::Current();
}

std::shared_ptr<tgfx::Surface> TGFSurfaceDrawable::getSurface(tgfx::Context*) {
  return ownedSurface;
}

}  // namespace pagx
