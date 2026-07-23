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
#include <memory>
#include <string>
#include <variant>
#include "pagx/nodes/Keyframe.h"
#include "pagx/types/Matrix.h"

namespace pagx {

class PAGImage;

/**
 * KeyValue is the value carried by a Channel's keyframes. Its std::variant alternatives are
 * ordered to match ChannelValueType, so the active alternative index equals the corresponding
 * ChannelValueType value.
 */
using KeyValue = std::variant<float, bool, int, std::string, ImageRef, Color, Matrix,
                               std::shared_ptr<PAGImage>>;

/**
 * Discriminator for the value type carried by a Channel's keyframes. Aligned with the order of
 * KeyValue's std::variant alternatives.
 */
enum class ChannelValueType : uint8_t {
  Float = 0,
  Bool = 1,
  Int = 2,
  String = 3,
  ImageRef = 4,
  Color = 5,
  Matrix = 6,
  PAGImage = 7,
};

}  // namespace pagx
