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

#include "cli/CliUtils.h"
#include <cctype>
#include <cmath>
#include <fstream>
#include <iostream>
#include "pagx/PAGXImporter.h"

namespace pagx::cli {

std::shared_ptr<PAGXDocument> LoadDocument(const std::string& filePath,
                                           const std::string& command) {
  auto document = PAGXImporter::FromFile(filePath);
  if (document == nullptr) {
    std::cerr << command << ": failed to load '" << filePath << "'\n";
    return nullptr;
  }
  for (auto& error : document->errors) {
    std::cerr << command << ": warning: " << error << "\n";
  }
  return document;
}

bool LoadFontConfig(FontConfig* fontConfig, const std::vector<std::string>& fontFiles,
                    const std::vector<std::string>& fallbacks, const std::string& command) {
  for (const auto& fontFile : fontFiles) {
    auto typeface = tgfx::Typeface::MakeFromPath(fontFile);
    if (typeface == nullptr) {
      std::cerr << command << ": failed to load font '" << fontFile << "'\n";
      return false;
    }
    fontConfig->registerTypeface(typeface);
  }
  std::vector<std::shared_ptr<tgfx::Typeface>> fallbackTypefaces = {};
  for (const auto& fallbackStr : fallbacks) {
    auto typeface = ResolveFallbackTypeface(fallbackStr);
    if (typeface == nullptr) {
      std::cerr << command << ": fallback font '" << fallbackStr << "' not found\n";
      return false;
    }
    fontConfig->registerTypeface(typeface);
    fallbackTypefaces.push_back(typeface);
  }
  if (!fallbackTypefaces.empty()) {
    fontConfig->addFallbackTypefaces(std::move(fallbackTypefaces));
  }
  return true;
}

bool WriteStringToFile(const std::string& content, const std::string& filePath,
                       const std::string& command) {
  std::ofstream out(filePath);
  if (!out.is_open()) {
    std::cerr << command << ": failed to write '" << filePath << "'\n";
    return false;
  }
  out << content;
  out.close();
  if (out.fail()) {
    std::cerr << command << ": error writing to '" << filePath << "'\n";
    return false;
  }
  std::cout << command << ": wrote " << filePath << "\n";
  return true;
}

bool MapFontStyleToCSS(const std::string& styleName, std::string* weight, std::string* style) {
  std::string normalized;
  normalized.reserve(styleName.size());
  for (char c : styleName) {
    if (c == ' ' || c == '-' || c == '_') {
      continue;
    }
    normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
  }
  bool italic = false;
  const std::string italicTokens[] = {"italic", "oblique"};
  for (const auto& token : italicTokens) {
    auto pos = normalized.find(token);
    if (pos != std::string::npos) {
      italic = true;
      normalized.erase(pos, token.size());
      break;
    }
  }
  struct WeightEntry {
    const char* name;
    const char* css;
  };
  static const WeightEntry weightTable[] = {
      {"hairline", "100"},  {"thin", "100"},      {"extralight", "200"}, {"ultralight", "200"},
      {"semilight", "300"}, {"light", "300"},     {"regular", "400"},    {"normal", "400"},
      {"roman", "400"},     {"book", "400"},      {"medium", "500"},     {"semibold", "600"},
      {"demibold", "600"},  {"extrabold", "800"}, {"ultrabold", "800"},  {"bold", "700"},
      {"black", "900"},     {"heavy", "900"},
  };
  const char* matchedWeight = nullptr;
  if (normalized.empty()) {
    matchedWeight = "400";
  } else {
    for (const auto& entry : weightTable) {
      if (normalized == entry.name) {
        matchedWeight = entry.css;
        break;
      }
    }
  }
  if (matchedWeight == nullptr) {
    return false;
  }
  if (weight != nullptr) {
    *weight = matchedWeight;
  }
  if (style != nullptr) {
    *style = italic ? "italic" : "normal";
  }
  return true;
}

}  // namespace pagx::cli
