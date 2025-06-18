/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "StringHelper.h"
#include <iostream>
#include "AEHelper.h"

namespace StringHelper {

std::string AeMemoryHandleToString(const AEGP_MemHandle& handle) {
  const auto& suites = AEHelper::GetSuites();
  char16_t* str = nullptr;
  suites->MemorySuite1()->AEGP_LockMemHandle(handle, reinterpret_cast<void**>(&str));

  std::string u8str;
  try {
    const char16_t* p = str;
    while (*p) {
      char32_t codePoint = *p++;
      if (codePoint >= 0xD800 && codePoint <= 0xDBFF && *p) {
        char32_t lowSurrogate = *p++;
        codePoint = 0x10000 + ((codePoint - 0xD800) << 10) + (lowSurrogate - 0xDC00);
      }

      if (codePoint <= 0x7F) {
        u8str.push_back(static_cast<char>(codePoint));
      } else if (codePoint <= 0x7FF) {
        u8str.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
        u8str.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
      } else if (codePoint <= 0xFFFF) {
        u8str.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
        u8str.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        u8str.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
      } else {
        u8str.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
        u8str.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
        u8str.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        u8str.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
      }
    }
  } catch (const std::exception& e) {
    std::cerr << "AeMemoryHandleToString failed: " << e.what() << std::endl;
  }

  suites->MemorySuite1()->AEGP_UnlockMemHandle(handle);
  return u8str;
}

bool LastIsSpace(const std::string& text) {
  if (text.empty()) {
    return false;
  }
  char last = text.back();
  return last == ' ' || last == '\n' || last == '\t' || last == '\r' || last == '\v' ||
         last == '\f';
}

std::string DeleteLastSpace(const std::string& text) {
  std::string result = text;
  while (!result.empty() && LastIsSpace(result)) {
    result.pop_back();
  }
  return result;
}

}  // namespace StringHelper
