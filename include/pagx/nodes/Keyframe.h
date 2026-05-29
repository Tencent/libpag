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

#pragma once

#include <cstdint>
#include <string>
#include "pagx/types/Color.h"
#include "pagx/types/Point.h"

namespace pagx {

using Frame = int64_t;
constexpr Frame ZeroFrame = 0;

enum class KeyframeInterpolationType : uint8_t {
  None = 0,
  Linear = 1,
  Bezier = 2,
  Hold = 3,
};

struct ImageRef {
  std::string id = {};

  bool operator==(const ImageRef& other) const {
    return id == other.id;
  }

  bool operator!=(const ImageRef& other) const {
    return !(*this == other);
  }
};

template <typename T>
struct Keyframe {
  Frame time = ZeroFrame;
  T value = {};
  KeyframeInterpolationType interpolation = KeyframeInterpolationType::Linear;
  Point bezierOut = {};
  Point bezierIn = {};
};

}  // namespace pagx
