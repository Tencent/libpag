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

#include "base/utils/Log.h"
#include "core/Color4f.h"

namespace pag {
const Color4f& Color4f::Invalid() {
  static const Color4f color = {
      std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
      std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
  return color;
}

const Color4f& Color4f::Transparent() {
  static const Color4f color = {0.0f, 0.0f, 0.0f, 0.0f};
  return color;
}

const Color4f& Color4f::Black() {
  static const Color4f color = {0.0f, 0.0f, 0.0f, 1.0f};
  return color;
}

const Color4f& Color4f::White() {
  static const Color4f color = {1.0f, 1.0f, 1.0f, 1.0f};
  return color;
}

Color4f Color4f::FromRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
  return {static_cast<float>(r) / 255.0f, static_cast<float>(g) / 255.0f,
          static_cast<float>(b) / 255.0f, static_cast<float>(a) / 255.0f};
}

float Color4f::operator[](int index) const {
  DEBUG_ASSERT(index >= 0 && index < 4);
  return (&red)[index];
}

float& Color4f::operator[](int index) {
  DEBUG_ASSERT(index >= 0 && index < 4);
  return (&red)[index];
}

bool Color4f::isValid() const {
  return red >= 0.0f && red <= 1.0f && green >= 0.0f && green <= 1.0f && blue >= 0.0f &&
         blue <= 1.0f && alpha >= 0.0f && alpha <= 1.0f;
}

bool Color4f::isOpaque() const {
  DEBUG_ASSERT(alpha <= 1.0f && alpha >= 0.0f);
  return alpha == 1.0f;
}

Color4f Color4f::unpremultiply() const {
  if (alpha == 0.0f) {
    return {0, 0, 0, 0};
  } else {
    float invAlpha = 1 / alpha;
    return {red * invAlpha, green * invAlpha, blue * invAlpha, alpha};
  }
}

}  // namespace pag
