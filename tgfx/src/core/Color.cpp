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
float Color4f::operator[](int index) const {
  DEBUG_ASSERT(index >= 0 && index < 4);
  return (&r)[index];
}

float& Color4f::operator[](int index) {
  DEBUG_ASSERT(index >= 0 && index < 4);
  return (&r)[index];
}

bool Color4f::isValid() const {
  return r >= 0.0f && r <= 1.0f && g >= 0.0f && g <= 1.0f && b >= 0.0f && b <= 1.0f && a >= 0.0f &&
         a <= 1.0f;
}

bool Color4f::isOpaque() const {
  DEBUG_ASSERT(a <= 1.0f && a >= 0.0f);
  return a == 1.0f;
}

Color4f Color4f::unpremultiply() const {
  if (a == 0.0f) {
    return {0, 0, 0, 0};
  } else {
    float invAlpha = 1 / a;
    return {r * invAlpha, g * invAlpha, b * invAlpha, a};
  }
}

}  // namespace pag
