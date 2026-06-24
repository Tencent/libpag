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
    fontConfig->registerFont(fontFile, 0, typeface->fontFamily(), typeface->fontStyle());
  }
  for (const auto& fallbackStr : fallbacks) {
    bool isFilePath = fallbackStr.find('/') != std::string::npos;
    if (!isFilePath) {
      auto dot = fallbackStr.rfind('.');
      if (dot != std::string::npos) {
        auto ext = fallbackStr.substr(dot);
        isFilePath = ext == ".ttf" || ext == ".otf" || ext == ".ttc" || ext == ".woff" ||
                     ext == ".woff2" || ext == ".TTF" || ext == ".OTF" || ext == ".TTC";
      }
    }
    if (isFilePath) {
      fontConfig->addFallbackFont(fallbackStr, 0);
    } else {
      auto commaPos = fallbackStr.find(',');
      auto family = commaPos != std::string::npos ? fallbackStr.substr(0, commaPos) : fallbackStr;
      auto style = commaPos != std::string::npos ? fallbackStr.substr(commaPos + 1) : std::string();
      if (!fontConfig->addFallbackSystemFont(family, style)) {
        std::cerr << command << ": fallback font '" << fallbackStr << "' not found\n";
      }
    }
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

}  // namespace pagx::cli
