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

#include "pagx/FontRegistration.h"
#include <algorithm>
#include <cctype>
#include "pagx/PAGXDocument.h"
#include "pagx/SystemFonts.h"
#include "pagx/nodes/Font.h"
#include "tgfx/core/Typeface.h"

namespace pagx {

std::shared_ptr<tgfx::Typeface> ResolveFallbackTypeface(const std::string& specifier) {
  bool isFilePath =
      specifier.find('/') != std::string::npos || specifier.find('\\') != std::string::npos;
  if (!isFilePath) {
    auto dot = specifier.rfind('.');
    if (dot != std::string::npos) {
      auto ext = specifier.substr(dot);
      for (auto& ch : ext) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
      }
      isFilePath =
          ext == ".ttf" || ext == ".otf" || ext == ".ttc" || ext == ".woff" || ext == ".woff2";
    }
  }
  if (isFilePath) {
    return tgfx::Typeface::MakeFromPath(specifier);
  }
  auto commaPos = specifier.find(',');
  auto family = commaPos != std::string::npos ? specifier.substr(0, commaPos) : specifier;
  auto style = commaPos != std::string::npos ? specifier.substr(commaPos + 1) : std::string();
  return SystemFonts::ResolveTypeface(family, style);
}

std::vector<std::string> RegisterFallbackFonts(FontConfig* fontConfig,
                                               const std::vector<std::string>& fallbacks) {
  std::vector<std::string> failures = {};
  for (const auto& fallback : fallbacks) {
    auto typeface = ResolveFallbackTypeface(fallback);
    if (typeface == nullptr) {
      failures.push_back(fallback);
      continue;
    }
    if (fallback.find('/') != std::string::npos || fallback.find('\\') != std::string::npos) {
      // File paths register as both a main font (precise family match) and a fallback font,
      // matching the pre-refactor behavior where fallback fonts were also reachable via exact
      // family lookup.
      fontConfig->registerFont(fallback, 0, typeface->fontFamily(), typeface->fontStyle());
      fontConfig->addFallbackFont(fallback, 0);
    } else {
      auto commaPos = fallback.find(',');
      auto family = commaPos != std::string::npos ? fallback.substr(0, commaPos) : fallback;
      auto style = commaPos != std::string::npos ? fallback.substr(commaPos + 1) : std::string();
      fontConfig->registerSystemFont(family, style);
      fontConfig->addFallbackSystemFont(family, style);
    }
  }
  return failures;
}

std::vector<std::string> RegisterDocumentFontFiles(PAGXDocument* document, FontConfig* fontConfig) {
  std::vector<std::string> failures = {};
  for (auto& node : document->nodes) {
    if (node->nodeType() != NodeType::Font) {
      continue;
    }
    auto* font = static_cast<Font*>(node.get());
    if (font->file.empty()) {
      continue;
    }
    auto typeface = tgfx::Typeface::MakeFromPath(font->file);
    if (typeface == nullptr) {
      failures.push_back(font->file);
      continue;
    }
    fontConfig->registerFont(font->file, 0, typeface->fontFamily(), typeface->fontStyle());
    // Also reach this file through the fallback chain: a (family, style) key holds one primary
    // registration, so unicode-range subset files sharing that key would otherwise overwrite
    // each other and drop every glyph that lives in an earlier subset.
    fontConfig->addFallbackFont(font->file, 0);
  }
  return failures;
}

}  // namespace pagx
