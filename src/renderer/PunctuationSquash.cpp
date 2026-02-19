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

#include "PunctuationSquash.h"

namespace pagx {

PunctuationPosition PunctuationSquash::GetPosition(int32_t c) {
  switch (c) {
    // Leading (opening brackets, left quotes)
    case 0xFF08:  // （
    case 0x3008:  // 〈
    case 0x300A:  // 《
    case 0x300C:  // 「
    case 0x300E:  // 『
    case 0x3010:  // 【
    case 0x3014:  // 〔
    case 0x3016:  // 〖
    case 0x3018:  // 〘
    case 0x301A:  // 〚
    case 0xFF3B:  // ［
    case 0xFF5B:  // ｛
    case 0x2018:  // '
    case 0x201C:  // "
      return PunctuationPosition::Leading;

    // Trailing (closing brackets, comma, period)
    case 0xFF09:  // ）
    case 0x3009:  // 〉
    case 0x300B:  // 》
    case 0x300D:  // 」
    case 0x300F:  // 』
    case 0x3011:  // 】
    case 0x3015:  // 〕
    case 0x3017:  // 〗
    case 0x3019:  // 〙
    case 0x301B:  // 〛
    case 0xFF3D:  // ］
    case 0xFF5D:  // ｝
    case 0x2019:  // '
    case 0x201D:  // "
    case 0x3001:  // 、
    case 0x3002:  // 。
    case 0xFF0C:  // ，
    case 0xFF0E:  // ．
    case 0xFF01:  // ！
    case 0xFF1F:  // ？
      return PunctuationPosition::Trailing;

    // Middle (colon, semicolon, middle dot)
    case 0xFF1A:  // ：
    case 0xFF1B:  // ；
    case 0x30FB:  // ・
      return PunctuationPosition::Middle;

    default:
      return PunctuationPosition::None;
  }
}

float PunctuationSquash::GetSquashAmount(float advance) {
  return advance * 0.5f;
}

bool PunctuationSquash::ShouldSquashBetween(int32_t prevChar, int32_t nextChar) {
  auto prevPos = GetPosition(prevChar);
  auto nextPos = GetPosition(nextChar);
  if (prevPos == PunctuationPosition::None || nextPos == PunctuationPosition::None) {
    return false;
  }
  if (prevPos == PunctuationPosition::Trailing && nextPos == PunctuationPosition::Leading) {
    return true;
  }
  if (prevPos != PunctuationPosition::None && nextPos != PunctuationPosition::None) {
    return true;
  }
  return false;
}

}  // namespace pagx
