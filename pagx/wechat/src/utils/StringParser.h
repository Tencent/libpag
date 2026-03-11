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

static tgfx::Color ParseHexColor(const std::string& hex) {
  if (hex.empty() || hex[0] != '#') {
    return tgfx::Color::FromRGBA(245, 245, 245);
  }
  if (hex.size() == 7) {
    int r = ParseHexDigit(hex[1]) * 16 + ParseHexDigit(hex[2]);
    int g = ParseHexDigit(hex[3]) * 16 + ParseHexDigit(hex[4]);
    int b = ParseHexDigit(hex[5]) * 16 + ParseHexDigit(hex[6]);
    return tgfx::Color::FromRGBA(r, g, b);
  }
  if (hex.size() == 9) {
    int r = ParseHexDigit(hex[1]) * 16 + ParseHexDigit(hex[2]);
    int g = ParseHexDigit(hex[3]) * 16 + ParseHexDigit(hex[4]);
    int b = ParseHexDigit(hex[5]) * 16 + ParseHexDigit(hex[6]);
    int a = ParseHexDigit(hex[7]) * 16 + ParseHexDigit(hex[8]);
    return tgfx::Color::FromRGBA(r, g, b, a);
  }
  return tgfx::Color::FromRGBA(245, 245, 245);
}

}  // namespace pagx
