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

namespace pag {

#define BLUR_LIMIT_BLURRINESS (40.0f)
#define BLUR_MODE_PIC_MAX_RADIUS (16.0f)
#define BLUR_MODE_PIC_MAX_LEVEL (3.0f)
#define BLUR_MODE_SHADOW_MAX_RADIUS (16.0f)
#define BLUR_MODE_SHADOW_MAX_LEVEL (3.0f)
#define STROKE_MAX_SPREAD_SIZE (25.0f)
#define STROKE_SPREAD_MIN_THICK_SIZE (12.0f)

enum class BlurMode {
  Picture = 0,
  Shadow = 1,
};
enum class BlurDirection {
  Both = 0,
  Vertical = 1,
  Horizontal = 2,
};
}  // namespace pag
