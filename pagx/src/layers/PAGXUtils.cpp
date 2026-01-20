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

#include "PAGXUtils.h"
#include <array>
#include <cstdlib>

namespace pagx {

std::shared_ptr<tgfx::Data> Base64Decode(const std::string& encodedString) {
  static const std::array<unsigned char, 128> decodingTable = {
      64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
      64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62,
      64, 64, 64, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64, 64, 0,
      1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
      23, 24, 25, 64, 64, 64, 64, 64, 64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38,
      39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64};

  size_t inLength = encodedString.size();
  if (inLength % 4 != 0) {
    return nullptr;
  }

  size_t outLength = inLength / 4 * 3;
  if (encodedString[inLength - 1] == '=') {
    outLength--;
  }
  if (encodedString[inLength - 2] == '=') {
    outLength--;
  }

  auto out = static_cast<unsigned char*>(malloc(outLength));
  auto outData = tgfx::Data::MakeAdopted(out, outLength, tgfx::Data::FreeProc);

  for (size_t i = 0, j = 0; i < inLength;) {
    uint32_t a = encodedString[i] == '='
                     ? 0 & i++
                     : decodingTable[static_cast<unsigned char>(encodedString[i++])];
    uint32_t b = encodedString[i] == '='
                     ? 0 & i++
                     : decodingTable[static_cast<unsigned char>(encodedString[i++])];
    uint32_t c = encodedString[i] == '='
                     ? 0 & i++
                     : decodingTable[static_cast<unsigned char>(encodedString[i++])];
    uint32_t d = encodedString[i] == '='
                     ? 0 & i++
                     : decodingTable[static_cast<unsigned char>(encodedString[i++])];

    uint32_t triple = (a << 18) + (b << 12) + (c << 6) + d;

    if (j < outLength) {
      out[j++] = (triple >> 16) & 0xFF;
    }
    if (j < outLength) {
      out[j++] = (triple >> 8) & 0xFF;
    }
    if (j < outLength) {
      out[j++] = triple & 0xFF;
    }
  }

  return outData;
}

}  // namespace pagx
