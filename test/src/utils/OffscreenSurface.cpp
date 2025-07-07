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

#include "OffscreenSurface.h"
#include "rendering/drawables/OffscreenDrawable.h"

namespace pag {
std::shared_ptr<PAGSurface> OffscreenSurface::Make(int width, int height) {
  auto device = DevicePool::Make();
  if (device == nullptr || width <= 0 || height <= 0) {
    return nullptr;
  }
  auto drawable =
      std::shared_ptr<OffscreenDrawable>(new OffscreenDrawable(width, height, std::move(device)));
  return PAGSurface::MakeFrom(std::move(drawable));
}
}  // namespace pag
