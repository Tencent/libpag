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

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>
#include <vector>
#include "SystemFonts.h"
#include "pagx/FontProvider.h"
#include "tgfx/core/Typeface.h"

namespace pagx::cli {

static inline bool FontFamilyMatch(const std::string& requested, const std::string& actual) {
  if (requested.size() != actual.size()) {
    return false;
  }
  for (size_t i = 0; i < requested.size(); i++) {
    if (std::tolower(static_cast<unsigned char>(requested[i])) !=
        std::tolower(static_cast<unsigned char>(actual[i]))) {
      return false;
    }
  }
  return true;
}

/**
 * Resolves a system font by family and style with fallback. First attempts an exact match with the
 * given style. If the result's fontFamily does not match the requested family (case-insensitive),
 * or the style is not found, falls back to the family's default style.
 */
static inline std::shared_ptr<tgfx::Typeface> ResolveSystemTypeface(const std::string& family,
                                                                    const std::string& style) {
  auto typeface = tgfx::Typeface::MakeFromName(family, style);
  if (typeface != nullptr && FontFamilyMatch(family, typeface->fontFamily())) {
    return typeface;
  }
  if (!style.empty()) {
    typeface = tgfx::Typeface::MakeFromName(family, "");
    if (typeface != nullptr && FontFamilyMatch(family, typeface->fontFamily())) {
      return typeface;
    }
  }
  return nullptr;
}

/**
 * Adds system fallback fonts to the given FontConfig as deferred entries. Fonts are loaded on
 * demand during text shaping, not upfront.
 */
inline void SetupSystemFallbackFonts(FontConfig& fontProvider) {
  auto locations = SystemFonts::FallbackTypefaces();
  for (const auto& loc : locations) {
    fontProvider.addFallbackFont(loc.path, loc.ttcIndex, loc.fontFamily, loc.fontStyle);
  }
}

/**
 * Resolves a fallback font specifier to a Typeface. Accepts either a font file path (containing
 * '/' or ending with a known font extension) or a font name in "family[,style]" format.
 */
inline std::shared_ptr<tgfx::Typeface> ResolveFallbackTypeface(const std::string& specifier) {
  // Treat as file path if it contains '/' or ends with a known font extension.
  bool isFilePath = specifier.find('/') != std::string::npos;
  if (!isFilePath) {
    auto dot = specifier.rfind('.');
    if (dot != std::string::npos) {
      auto ext = specifier.substr(dot);
      isFilePath = ext == ".ttf" || ext == ".otf" || ext == ".ttc" || ext == ".woff" ||
                   ext == ".woff2" || ext == ".TTF" || ext == ".OTF" || ext == ".TTC";
    }
  }
  if (isFilePath) {
    return tgfx::Typeface::MakeFromPath(specifier);
  }
  auto commaPos = specifier.find(',');
  auto family = commaPos != std::string::npos ? specifier.substr(0, commaPos) : specifier;
  auto style = commaPos != std::string::npos ? specifier.substr(commaPos + 1) : std::string();
  return ResolveSystemTypeface(family, style);
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
