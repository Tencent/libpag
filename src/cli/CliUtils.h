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

#pragma once

#include <cstdio>
#include <string>
#include <vector>
#include "SystemFonts.h"
#include "renderer/TextLayout.h"
#include "tgfx/core/Typeface.h"

namespace pagx::cli {

/**
 * Registers system fallback fonts on the given TextLayout by querying native platform APIs.
 * The fallback typefaces cover CJK, emoji, and other scripts installed on the system.
 */
inline void SetupSystemFallbackFonts(TextLayout& textLayout) {
  auto fallbacks = SystemFonts::FallbackTypefaces();
  for (auto& typeface : fallbacks) {
    textLayout.registerTypeface(typeface);
  }
  if (!fallbacks.empty()) {
    textLayout.setFallbackTypefaces(std::move(fallbacks));
  }
}

inline std::string EscapeJson(const std::string& input) {
  std::string result = {};
  result.reserve(input.size() + 16);
  for (char ch : input) {
    switch (ch) {
      case '"':
        result += "\\\"";
        break;
      case '\\':
        result += "\\\\";
        break;
      case '\n':
        result += "\\n";
        break;
      case '\r':
        result += "\\r";
        break;
      case '\t':
        result += "\\t";
        break;
      case '\b':
        result += "\\b";
        break;
      case '\f':
        result += "\\f";
        break;
      default:
        if (static_cast<unsigned char>(ch) < 0x20) {
          char buf[8] = {};
          snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(ch));
          result += buf;
        } else {
          result += ch;
        }
        break;
    }
  }
  return result;
}

}  // namespace pagx::cli
