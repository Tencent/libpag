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

#include "VerticalTextUtils.h"

namespace pagx {

VerticalOrientation VerticalTextUtils::getOrientation(int32_t c) {
  // Simplified from UTR#50: 4 major Rotated ranges covering Latin, Indic, African, and
  // Southeast Asian scripts. Everything else defaults to Upright (CJK, Emoji, symbols).
  if (c <= 0x10FF) return VerticalOrientation::Rotated;
  if (c >= 0x1200 && c <= 0x1AAF) return VerticalOrientation::Rotated;
  if (c >= 0x1CC0 && c <= 0x2E7F) return VerticalOrientation::Rotated;
  if (c >= 0xA4D0 && c <= 0xABFF) return VerticalOrientation::Rotated;

  // Tr (Transformed-Rotated) characters: CJK brackets and paired punctuation that need 90
  // degree rotation in vertical layout. These are Upright in UTR#50 but fall back to Rotated.
  if (c >= 0x3008 && c <= 0x3011) return VerticalOrientation::Rotated;
  if (c >= 0x3014 && c <= 0x301F) return VerticalOrientation::Rotated;
  if (c == 0x3030) return VerticalOrientation::Rotated;
  if (c == 0x30A0) return VerticalOrientation::Rotated;
  if (c == 0x30FC) return VerticalOrientation::Rotated;
  if (c >= 0xFF08 && c <= 0xFF09) return VerticalOrientation::Rotated;
  if (c == 0xFF0D) return VerticalOrientation::Rotated;
  if (c == 0xFF3B) return VerticalOrientation::Rotated;
  if (c == 0xFF3D) return VerticalOrientation::Rotated;
  if (c == 0xFF3F) return VerticalOrientation::Rotated;
  if (c >= 0xFF5B && c <= 0xFF60) return VerticalOrientation::Rotated;
  if (c == 0xFFE3) return VerticalOrientation::Rotated;

  return VerticalOrientation::Upright;
}

bool VerticalTextUtils::needsPunctuationOffset(int32_t c) {
  // Tu (Transformed-Upright) punctuation that occupies the lower-left area in horizontal layout
  // but should appear in the upper-right area in vertical layout.
  return c == 0xFF0C ||  // ， fullwidth comma
         c == 0x3002 ||  // 。 ideographic full stop
         c == 0x3001 ||  // 、 ideographic comma
         c == 0xFF1A ||  // ： fullwidth colon
         c == 0xFF1B;    // ； fullwidth semicolon
}

void VerticalTextUtils::getPunctuationOffset(float fontSize, float* outDx, float* outDy) {
  if (outDx) {
    *outDx = fontSize * 0.5f;
  }
  if (outDy) {
    *outDy = -fontSize * 0.5f;
  }
}

}  // namespace pagx
