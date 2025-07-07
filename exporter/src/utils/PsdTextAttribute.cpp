/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 Tencent. All rights reserved.
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

#include "PsdTextAttribute.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <iostream>
#include <string_view>
#include "src/base/utils/Log.h"
namespace exporter {

inline int HexCharToInt(const char c) {
  int result;
  auto [ptr, ec] = std::from_chars(&c, &c + 1, result, 16);
  if (ec != std::errc() || ptr != &c + 1) {
    LOGE("HexCharToInt error: %d (%s)", static_cast<int>(ec),
         std::make_error_code(ec).message().c_str());
    return -1;
  }
  return result;
}

size_t PsdTextAttribute::stringFormatTransform(std::vector<char>& dst, const uint8_t* src,
                                               const int len) {
  dst.push_back('/');
  dst.push_back('0');
  dst.push_back(' ');
  dst.push_back('<');
  dst.push_back(' ');

  for (int i = 0; i < len; i++) {
    char c = src[i];

    if (c & 0x80) {
      dst.push_back('W');
      i++;
    } else if (c == '\\') {
      dst.push_back('Z');
      i++;
    } else if (c == '<') {
      if (i + 1 < len && src[i + 1] == '<') {
        dst.push_back('<');
        dst.push_back(' ');
        i++;
      } else {
        dst.push_back('J');
      }
    } else if (c == '>') {
      if (i + 1 < len && src[i + 1] == '>') {
        dst.push_back('>');
        dst.push_back(' ');
        i++;
      } else {
        dst.push_back('J');
      }
    } else {
      dst.push_back(c);
      if (c == '(') {
        if (i + 2 < len && ((src[i + 1] == 0xFE && src[i + 2] == 0xFF) ||
                            (src[i + 1] == 0xFF && src[i + 2] == 0xFE))) {
          i++;
          bool isBE = (src[i] == 0xFE);
          dst.push_back('T');
          dst.push_back('M');
          i += 2;

          while (i < len && src[i] != ')') {
            uint16_t w1 = isBE ? ((src[i] << 8) | src[i + 1]) : ((src[i + 1] << 8) | src[i]);

            if ((w1 & 0xFF) == 0x5C && i + 3 < len) {
              i += 3;
              dst.push_back('.');
              dst.push_back('.');
              dst.push_back('.');
            } else if (w1 >= 0xD800 && w1 <= 0xDFFF && i + 4 < len) {
              i += 4;
              dst.push_back('.');
              dst.push_back('.');
              dst.push_back('.');
              dst.push_back('.');
            } else {
              i += 2;
              dst.push_back('.');
              dst.push_back('.');
            }
          }
          if (i < len) {
            dst.push_back(src[i]);
          }
        }
      }
    }
  }

  dst.push_back(' ');
  dst.push_back('>');
  dst.push_back('\0');

  return dst.size();
}

PsdTextAttribute::PsdTextAttribute(int size, const uint8_t* data) {
  mem.reserve(size);
  len = stringFormatTransform(mem, data, size);
  src = mem.data();
}

bool PsdTextAttribute::getIntegerByKeys(int& result, std::vector<int> keys, const int keyCount) {
  if (keys.empty() || keyCount <= 0) {
    return false;
  }

  if (key != keys[0]) {
    return false;
  }

  if (keyCount == 1) {
    if (valueType == PsdValueType::Integer) {
      result = intValue;
      return true;
    }
    return false;
  }

  if (valueType == PsdValueType::Dictionary || valueType == PsdValueType::Array) {
    const std::vector<int> subKeys(keys.begin() + 1, keys.end());

    for (const auto& attribute : attributeList) {
      if (attribute->getIntegerByKeys(result, subKeys, keyCount - 1)) {
        return true;
      }
    }
  }
  return false;
}

void PsdTextAttribute::skipSpaces() {
  while (pos < len && src[pos] == ' ') {
    pos++;
  }
}

bool PsdTextAttribute::getKey() {
  skipSpaces();
  if (pos >= len) {
    return false;
  }
  if (src[pos] != '/') {
    LOGE("Error: Expected '/' at position %d", pos);
    return false;
  }

  auto hasDigit = false;
  key = 0;
  while (++pos < len) {
    if (std::isdigit(static_cast<unsigned char>(src[pos]))) {
      key = key * 10 + HexCharToInt(src[pos]);
      hasDigit = true;
    } else if (src[pos] == ' ') {
      if (!hasDigit) {
        LOGE("Error: No digits found in key at position %d", pos);
      }
      return hasDigit;
    } else {
      LOGE("Error: Invalid character '%c' in key at position %d", src[pos], pos);
      return false;
    }
  }
  LOGE("Error: Unexpected end of input while parsing key");
  return false;
}

bool PsdTextAttribute::isBoolean(std::string_view& srcStr) {

  static constexpr std::array<std::string_view, 4> boolStrings = {"false", "FALSE", "true", "TRUE"};
  return std::any_of(boolStrings.begin(), boolStrings.end(),
                     [&srcStr](const std::string_view& s) { return srcStr == s; });
}

bool PsdTextAttribute::isInteger(int& num, std::string_view& srcStr) {
  try {
    num = std::stoi(srcStr.data());
  } catch (...) {
    return false;
  }
  return true;
}

bool PsdTextAttribute::isNumber(std::string_view& srcStr) {
  try {
    std::stof(srcStr.data());
  } catch (...) {
    return false;
  }
  return true;
}

bool PsdTextAttribute::getValue() {
  skipSpaces();
  if (pos >= len) {
    return false;
  }

  if (src[pos] == '<') {
    valueType = PsdValueType::Dictionary;
    auto startFlag = '<';
    auto endFlag = '>';

    pos++;
    skipSpaces();

    auto startPos = pos;
    int num = 1;
    for (; pos < len && num > 0; pos++) {
      if (src[pos] == startFlag) {
        num++;
      } else if (src[pos] == endFlag) {
        num--;
      }
    }
    if (num != 0) {
      LOGE("Error: Unmatched '<' in dictionary at position %d", startPos);
      return false;
    }

    buildAttributeList(startPos, pos - 1);
  } else if (src[pos] == '[') {
    valueType = PsdValueType::Array;
    auto startFlag = '[';
    auto endFlag = ']';

    pos++;
    skipSpaces();

    auto startPos = pos;
    int num = 1;
    for (; pos < len && num > 0; pos++) {
      if (src[pos] == startFlag) {
        num++;
      } else if (src[pos] == endFlag) {
        num--;
      }
    }
    if (num != 0) {
      LOGE("Error: Unmatched '[' in array at position %d", startPos);
      return false;
    }

    if (src[startPos] == '<') {
      buildAttributeArray(startPos, pos - 1);
    } else {
      valueType = PsdValueType::String;
      stringValue = "Array";
    }
  } else {
    char endFlag = ' ';
    if (src[pos] == '(') {
      endFlag = ')';
      valueType = PsdValueType::String;
      stringValue = "WString";
    }

    int startPos = pos;
    while (pos < len) {
      if (src[pos++] == endFlag) {
        break;
      }
    }

    if (valueType != PsdValueType::String) {
      std::string_view srcStr(src + startPos, pos - startPos - 1);
      if (isInteger(intValue, srcStr)) {
        valueType = PsdValueType::Integer;
      } else if (isNumber(srcStr)) {
        valueType = PsdValueType::Float;
      } else if (isBoolean(srcStr)) {
        valueType = PsdValueType::Boolean;
      } else {
        valueType = PsdValueType::String;
        stringValue = "CString";
      }
    }
  }

  return true;
}

int PsdTextAttribute::getAttribute() {
  if (getKey()) {
    if (!getValue()) {
      LOGE("Error: getAttribute error!");
    }
  }
  return pos;
}

void PsdTextAttribute::buildAttributeList(int startPos, int endPos) {
  while (startPos < endPos) {
    auto attribute = std::make_shared<PsdTextAttribute>(src + startPos, endPos - startPos);

    startPos += attribute->getAttribute();

    if (attribute->key >= 0 && attribute->valueType != PsdValueType::None) {
      attributeList.push_back(std::move(attribute));
    }
  }
}

void PsdTextAttribute::buildAttributeArray(int startPos, int endPos) {
  int index = 0;
  while (startPos < endPos) {
    auto attribute = std::make_shared<PsdTextAttribute>(src + startPos, endPos - startPos);
    auto hasValue = attribute->getValue();
    startPos += attribute->pos;
    if (!hasValue) {
      break;
    }
    attribute->key = index++;
    attributeList.emplace_back(std::move(attribute));
  }
}

}  // namespace exporter
