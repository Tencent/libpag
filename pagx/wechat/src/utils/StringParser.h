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

#include <string>
#include "tgfx/core/Color.h"

namespace pagx {

static int ParseHexDigit(char ch) {
  if (ch >= '0' && ch <= '9') {
    return ch - '0';
  }
  if (ch >= 'a' && ch <= 'f') {
    return ch - 'a' + 10;
  }
  if (ch >= 'A' && ch <= 'F') {
    return ch - 'A' + 10;
  }
  return -1;
}

static tgfx::Color DefaultBackgroundColor() {
  return tgfx::Color::FromRGBA(245, 245, 245);
}

static tgfx::Color ParseHexColor(const std::string& hex) {
  if (hex.empty() || hex[0] != '#') {
    return DefaultBackgroundColor();
  }
  if (hex.size() == 4) {
    int r = ParseHexDigit(hex[1]);
    int g = ParseHexDigit(hex[2]);
    int b = ParseHexDigit(hex[3]);
    if (r < 0 || g < 0 || b < 0) {
      return DefaultBackgroundColor();
    }
    return tgfx::Color::FromRGBA(r * 17, g * 17, b * 17);
  }
  if (hex.size() == 7 || hex.size() == 9) {
    int r1 = ParseHexDigit(hex[1]);
    int r2 = ParseHexDigit(hex[2]);
    int g1 = ParseHexDigit(hex[3]);
    int g2 = ParseHexDigit(hex[4]);
    int b1 = ParseHexDigit(hex[5]);
    int b2 = ParseHexDigit(hex[6]);
    if (r1 < 0 || r2 < 0 || g1 < 0 || g2 < 0 || b1 < 0 || b2 < 0) {
      return DefaultBackgroundColor();
    }
    int r = r1 * 16 + r2;
    int g = g1 * 16 + g2;
    int b = b1 * 16 + b2;
    if (hex.size() == 9) {
      int a1 = ParseHexDigit(hex[7]);
      int a2 = ParseHexDigit(hex[8]);
      if (a1 < 0 || a2 < 0) {
        return DefaultBackgroundColor();
      }
      return tgfx::Color::FromRGBA(r, g, b, a1 * 16 + a2);
    }
    return tgfx::Color::FromRGBA(r, g, b);
  }
  return DefaultBackgroundColor();
}

}  // namespace pagx
