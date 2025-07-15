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

#include "rendering/graphics/Canvas.h"
#include "tgfx/core/Surface.h"

namespace pag {
class SurfaceUtil {
 public:
  static std::shared_ptr<tgfx::Surface> MakeContentSurface(Canvas* parentCanvas,
                                                           const tgfx::Rect& bounds,
                                                           float scaleFactorLimit = FLT_MAX,
                                                           float scale = 1.f,
                                                           bool usesMSAA = false);
};
}  // namespace pag
