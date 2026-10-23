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
//  or required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. see the license for the specific language governing permissions and
//  limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "GlobalFontRegistry.h"
#include <utility>
#include "tgfx/core/Typeface.h"

namespace pagx {

static FontConfig& GlobalConfig() {
  static FontConfig config = {};
  return config;
}

static std::mutex& GlobalMutex() {
  static std::mutex mutex = {};
  return mutex;
}

bool GlobalFontRegistry::RegisterFontFromPath(const std::string& fontPath) {
  if (fontPath.empty()) {
    return false;
  }
  auto typeface = tgfx::Typeface::MakeFromPath(fontPath, 0);
  if (typeface == nullptr) {
    return false;
  }
  std::lock_guard<std::mutex> autoLock(GlobalMutex());
  // The typeface parsed above is cached by tgfx, so re-parsing inside registerFont is cheap; the
  // registration key (family/style) is read from the file itself.
  GlobalConfig().registerFont(fontPath, 0, "", "");
  return true;
}

bool GlobalFontRegistry::RegisterFontFromBytes(const void* data, size_t length) {
  if (data == nullptr || length == 0) {
    return false;
  }
  auto typeface = tgfx::Typeface::MakeFromBytes(data, static_cast<size_t>(length), 0);
  if (typeface == nullptr) {
    return false;
  }
  std::lock_guard<std::mutex> autoLock(GlobalMutex());
  GlobalConfig().registerFont(data, length, 0, "", "");
  return true;
}

void GlobalFontRegistry::SetFallbackFontNames(const std::vector<std::string>& names) {
  std::lock_guard<std::mutex> autoLock(GlobalMutex());
  // Replace semantics, matching the legacy SetFallbackFontNames: the list is reset, names are
  // resolved from the system in order, and unknown names are skipped.
  GlobalConfig().clearFallbackFonts();
  for (const auto& name : names) {
    GlobalConfig().addFallbackSystemFont(name, "");
  }
}

std::mutex& GlobalFontRegistry::GetMutex() {
  return GlobalMutex();
}

FontConfig& GlobalFontRegistry::GetFontConfig() {
  return GlobalConfig();
}

void GlobalFontRegistry::ResetForTesting() {
  std::lock_guard<std::mutex> autoLock(GlobalMutex());
  GlobalConfig() = FontConfig();
}

}  // namespace pagx
