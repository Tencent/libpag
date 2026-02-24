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

#include "Base64.h"
#include <array>
#include <memory>

namespace pagx {

std::shared_ptr<Data> Base64Decode(const std::string& encodedString) {
  static const std::array<unsigned char, 128> decodingTable = {
      64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
      64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62,
      64, 64, 64, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64, 64, 0,
      1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
      23, 24, 25, 64, 64, 64, 64, 64, 64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38,
      39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64};

  size_t inputLength = encodedString.size();
  if (inputLength % 4 != 0) {
    return nullptr;
  }

  size_t outputLength = inputLength / 4 * 3;
  if (inputLength >= 1 && encodedString[inputLength - 1] == '=') {
    outputLength--;
  }
  if (inputLength >= 2 && encodedString[inputLength - 2] == '=') {
    outputLength--;
  }

  auto output = std::make_unique<uint8_t[]>(outputLength);

  for (size_t i = 0, j = 0; i < inputLength;) {
    auto ch0 = static_cast<unsigned char>(encodedString[i]);
    auto ch1 = static_cast<unsigned char>(encodedString[i + 1]);
    auto ch2 = static_cast<unsigned char>(encodedString[i + 2]);
    auto ch3 = static_cast<unsigned char>(encodedString[i + 3]);
    auto a = ch0 < 128 ? decodingTable[ch0] : 64;
    auto b = ch1 < 128 ? decodingTable[ch1] : 64;
    auto c = ch2 < 128 ? decodingTable[ch2] : 64;
    auto d = ch3 < 128 ? decodingTable[ch3] : 64;
    i += 4;
    // The first two characters must always be valid Base64 characters.
    if (a >= 64 || b >= 64) {
      return nullptr;
    }
    // The third character can only be invalid (64) if it is a padding '='.
    if (c >= 64 && encodedString[i - 2] != '=') {
      return nullptr;
    }
    // The fourth character can only be invalid (64) if it is a padding '='.
    if (d >= 64 && encodedString[i - 1] != '=') {
      return nullptr;
    }

    uint32_t triple = (a << 3 * 6) + (b << 2 * 6) + (c << 1 * 6) + (d << 0 * 6);

    if (j < outputLength) {
      output[j++] = (triple >> 2 * 8) & 0xFF;
    }
    if (j < outputLength) {
      output[j++] = (triple >> 1 * 8) & 0xFF;
    }
    if (j < outputLength) {
      output[j++] = (triple >> 0 * 8) & 0xFF;
    }
  }

  return Data::MakeAdopt(output.release(), outputLength);
}

std::shared_ptr<Data> DecodeBase64DataURI(const std::string& dataURI) {
  if (dataURI.find("data:") != 0) {
    return nullptr;
  }
  auto commaPos = dataURI.find(',');
  if (commaPos == std::string::npos) {
    return nullptr;
  }
  auto base64Pos = dataURI.find(";base64");
  if (base64Pos == std::string::npos || base64Pos > commaPos) {
    return nullptr;
  }
  return Base64Decode(dataURI.substr(commaPos + 1));
}

std::string Base64Encode(const uint8_t* data, size_t length) {
  static const char encodingTable[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string result;
  result.reserve((length + 2) / 3 * 4);

  for (size_t i = 0; i < length; i += 3) {
    uint32_t val = (data[i] << 16);
    if (i + 1 < length) val += (data[i + 1] << 8);
    if (i + 2 < length) val += data[i + 2];

    result += encodingTable[(val >> 18) & 0x3F];
    result += encodingTable[(val >> 12) & 0x3F];
    result += (i + 1 < length) ? encodingTable[(val >> 6) & 0x3F] : '=';
    result += (i + 2 < length) ? encodingTable[val & 0x3F] : '=';
  }
  return result;
}

}  // namespace pagx
