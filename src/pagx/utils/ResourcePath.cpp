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
//  license is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "pagx/utils/ResourcePath.h"

namespace pagx {
namespace {

bool IsAsciiAlpha(char value) {
  return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z');
}

bool IsSchemeCharacter(char value) {
  return IsAsciiAlpha(value) || (value >= '0' && value <= '9') || value == '+' || value == '.' ||
         value == '-';
}

bool HasDrivePrefix(const std::string& path) {
  return path.size() >= 2 && path[1] == ':' && IsAsciiAlpha(path[0]);
}

}  // namespace

std::string GetUriScheme(const std::string& path) {
  auto colon = path.find(':');
  if (colon == std::string::npos || colon == 0) {
    return {};
  }
  auto separator = path.find_first_of("/\\");
  if (separator != std::string::npos && separator < colon) {
    return {};
  }
  if (HasDrivePrefix(path)) {
    return {};
  }
  if (!IsAsciiAlpha(path[0])) {
    return {};
  }
  for (size_t i = 1; i < colon; i++) {
    if (!IsSchemeCharacter(path[i])) {
      return {};
    }
  }
  return path.substr(0, colon);
}

bool HasUriScheme(const std::string& path) {
  return !GetUriScheme(path).empty();
}

bool IsRelativeFilePath(const std::string& path) {
  if (path.empty() || HasUriScheme(path) || HasDrivePrefix(path)) {
    return false;
  }
  return path[0] != '/' && path[0] != '\\';
}

bool IsLocalFilePath(const std::string& path) {
  return !path.empty() && !HasUriScheme(path);
}

}  // namespace pagx
