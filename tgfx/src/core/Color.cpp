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

#include "tgfx/core/Color.h"
#include "core/utils/Log.h"

namespace tgfx {
const Color& Color::Transparent() {
  static const Color color = {0.0f, 0.0f, 0.0f, 0.0f};
  return color;
}

const Color& Color::Black() {
  static const Color color = {0.0f, 0.0f, 0.0f, 1.0f};
  return color;
}

const Color& Color::White() {
  static const Color color = {1.0f, 1.0f, 1.0f, 1.0f};
  return color;
}

Color Color::FromRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
  auto alpha = a == 255 ? 1.0f : static_cast<float>(a) / 255.0f;
  return {static_cast<float>(r) / 255.0f, static_cast<float>(g) / 255.0f,
          static_cast<float>(b) / 255.0f, alpha};
}

float Color::operator[](int index) const {
  DEBUG_ASSERT(index >= 0 && index < 4);
  return (&red)[index];
}

float& Color::operator[](int index) {
  DEBUG_ASSERT(index >= 0 && index < 4);
  return (&red)[index];
}

bool Color::isValid() const {
  return red >= 0.0f && red <= 1.0f && green >= 0.0f && green <= 1.0f && blue >= 0.0f &&
         blue <= 1.0f && alpha >= 0.0f && alpha <= 1.0f;
}

bool Color::isOpaque() const {
  DEBUG_ASSERT(alpha <= 1.0f && alpha >= 0.0f);
  return alpha == 1.0f;
}

Color Color::unpremultiply() const {
  if (alpha == 0.0f) {
    return {0, 0, 0, 0};
  } else {
    float invAlpha = 1 / alpha;
    return {red * invAlpha, green * invAlpha, blue * invAlpha, alpha};
  }
}

}  // namespace tgfx
