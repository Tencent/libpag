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

#pragma once

#include <charconv>
#include <string>
#include <string_view>
#include "tinyxml2.h"

namespace exporter {
template <typename T>
T SafeStringToInt(const char* str, T defaultValue) {
  if (str == nullptr) {
    return defaultValue;
  }
  std::string_view sv(str);
  if (sv.empty()) {
    return defaultValue;
  }

  T value{};
  auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), value);

  if (ec == std::errc()) {
    return value;
  }
  return defaultValue;
}

float SafeStringToFloat(std::string_view str, float defaultValue);

bool SafeStringEqual(const char* str, const char* target);

std::string FormatFloat(float value, int precision);

void AddElement(tinyxml2::XMLElement* parent, const std::string& name, const std::string& value);

const char* GetChildElementText(tinyxml2::XMLElement* fatherElement, const char* childName);

}  // namespace exporter
