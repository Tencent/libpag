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

#include <cstdint>

namespace pag {
class Swizzle {
 public:
  constexpr Swizzle() : Swizzle("rgba") {
  }

  constexpr bool operator==(const Swizzle& that) const {
    return key == that.key;
  }
  constexpr bool operator!=(const Swizzle& that) const {
    return !(*this == that);
  }

  /**
   * Compact representation of the swizzle suitable for a key.
   */
  constexpr uint16_t asKey() const {
    return key;
  }

  /**
   * 4 char null terminated string consisting only of chars 'r', 'g', 'b', 'a'.
   */
  const char* c_str() const {
    return swiz;
  }

  char operator[](int i) const {
    return swiz[i];
  }

  static constexpr Swizzle RGBA() {
    return Swizzle("rgba");
  }
  static constexpr Swizzle AAAA() {
    return Swizzle("aaaa");
  }
  static constexpr Swizzle RRRR() {
    return Swizzle("rrrr");
  }
  static constexpr Swizzle RRRA() {
    return Swizzle("rrra");
  }
  static constexpr Swizzle RGRG() {
    return Swizzle("rgrg");
  }
  static constexpr Swizzle RARA() {
    return Swizzle("rara");
  }

 private:
  char swiz[5];
  uint16_t key = 0;

  static constexpr int CToI(char c) {
    switch (c) {
      case 'r':
        return 0;
      case 'g':
        return 1;
      case 'b':
        return 2;
      case 'a':
        return 3;
      case '1':
        return 4;
      default:
        return -1;
    }
  }

  constexpr explicit Swizzle(const char c[4])
      : swiz{c[0], c[1], c[2], c[3], '\0'},
        key((CToI(c[0]) << 0) | (CToI(c[1]) << 4) | (CToI(c[2]) << 8) | (CToI(c[3]) << 12)) {
  }
};
}  // namespace pag
