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

#include <type_traits>
#include "pag/file.h"
#include "rendering/filters/utils/FilterBuffer.h"

namespace pag {

#define BLUR_LEVEL_1_LIMIT 10.0f
#define BLUR_LEVEL_2_LIMIT 15.0f
#define BLUR_LEVEL_3_LIMIT 55.0f
#define BLUR_LEVEL_4_LIMIT 120.0f
#define BLUR_LEVEL_5_LIMIT 300.0f

#define BLUR_LEVEL_MAX_LIMIT BLUR_LEVEL_5_LIMIT

#define BLUR_LEVEL_1_DEPTH 1
#define BLUR_LEVEL_2_DEPTH 2
#define BLUR_LEVEL_3_DEPTH 2
#define BLUR_LEVEL_4_DEPTH 3
#define BLUR_LEVEL_5_DEPTH 3

#define BLUR_DEPTH_MAX BLUR_LEVEL_5_DEPTH

#define BLUR_LEVEL_1_SCALE 1.0f
#define BLUR_LEVEL_2_SCALE 0.8f
#define BLUR_LEVEL_3_SCALE 0.5f
#define BLUR_LEVEL_4_SCALE 0.5f
#define BLUR_LEVEL_5_SCALE 0.5f

#define BLUR_STABLE 10.0f

#define BLUR_EXPEND 0.1f

struct BlurParam {
  int depth = BLUR_LEVEL_1_DEPTH;
  float scale = BLUR_LEVEL_1_SCALE;
  float value = 0.0;
  bool repeatEdgePixels = true;
  Enum blurDimensions = BlurDimensionsDirection::All;
};

struct PassBounds {
  tgfx::Rect inputBounds;
  tgfx::Rect outputBounds;
};

enum class BlurOptions : unsigned {
  None = 0,
  Up = 1 << 0,
  Down = 1 << 1,
  Horizontal = 1 << 2,
  Vertical = 1 << 3,
  RepeatEdgePixels = 1 << 4
};

inline BlurOptions operator&(BlurOptions lhs, BlurOptions rhs) {
  return static_cast<BlurOptions>(static_cast<std::underlying_type<BlurOptions>::type>(lhs) &
                                  static_cast<std::underlying_type<BlurOptions>::type>(rhs));
}

inline BlurOptions operator|(BlurOptions lhs, BlurOptions rhs) {
  return static_cast<BlurOptions>(static_cast<std::underlying_type<BlurOptions>::type>(lhs) |
                                  static_cast<std::underlying_type<BlurOptions>::type>(rhs));
}

inline BlurOptions& operator|=(BlurOptions& lhs, BlurOptions rhs) {
  lhs = lhs | rhs;
  return lhs;
}

}  // namespace pag
