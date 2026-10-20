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
#include <charconv>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include "pagx/PAGXImporter.h"
#include "pagx/nodes/Font.h"
#include "renderer/FontEmbedder.h"

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

static bool ParseInt(const std::string& value, int* number) {
  auto result = std::from_chars(value.data(), value.data() + value.size(), *number);
  return result.ec == std::errc() && result.ptr == value.data() + value.size();
}

bool ParseFontFaceBinding(char* argv[], FontFaceBinding* binding) {
  binding->family = argv[0];
  int index = 0;
  if (binding->family.empty() || argv[1][0] == '\0' || !ParseInt(argv[2], &index) || index < -1) {
    return false;
  }
  if (index == -1) {
    binding->face.systemFont.fontFamily = argv[1];
    binding->face.systemFont.fontStyle = argv[3];
  } else {
    binding->face.path = argv[1];
    binding->face.faceIndex = index;
  }
  std::string ranges = argv[4];
  if (!ranges.empty() && ranges.back() == ',') return false;
  size_t start = 0;
  while (start < ranges.size()) {
    auto end = ranges.find(',', start);
    if (end == std::string::npos) end = ranges.size();
    auto token = ranges.substr(start, end - start);
    auto dash = token.find('-');
    int lo = 0, hi = 0;
    if (dash == std::string::npos || !ParseInt(token.substr(0, dash), &lo) ||
        !ParseInt(token.substr(dash + 1), &hi) || lo < 0 || lo > hi || hi > 0x10ffff)
      return false;
    binding->face.unicodeRanges.emplace_back(lo, hi);
    start = end + 1;
  }
  return true;
}

bool LoadFontFaces(FontConfig* config, const std::vector<FontFaceBinding>& bindings,
                   const std::string& command) {
  for (const auto& binding : bindings) {
    if (config->addFontFace(binding.family, binding.face)) continue;
    // An unavailable system family is a normal CSS fallback; captured files are required.
    if (binding.face.path.empty()) continue;
    std::cerr << command << ": failed to load bound font '" << binding.face.path << "'\n";
    return false;
  }
  return true;
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
      // Register as both a main font (precise family match) and a fallback font, matching the
      // pre-refactor behavior where fallback fonts were also reachable via exact family lookup.
      auto typeface = tgfx::Typeface::MakeFromPath(fallbackStr);
      if (typeface == nullptr) {
        std::cerr << command << ": fallback font '" << fallbackStr << "' not found\n";
      } else {
        fontConfig->registerFont(fallbackStr, 0, typeface->fontFamily(), typeface->fontStyle());
        fontConfig->addFallbackFont(fallbackStr, 0);
      }
    } else {
      auto commaPos = fallbackStr.find(',');
      auto family = commaPos != std::string::npos ? fallbackStr.substr(0, commaPos) : fallbackStr;
      auto style = commaPos != std::string::npos ? fallbackStr.substr(commaPos + 1) : std::string();
      if (!fontConfig->registerSystemFont(family, style)) {
        std::cerr << command << ": fallback font '" << fallbackStr << "' not found\n";
      } else {
        fontConfig->addFallbackSystemFont(family, style);
      }
    }
  }
  return true;
}

bool EmbedFonts(PAGXDocument* document, const std::vector<std::string>& fallbacks,
                const std::string& command, const std::vector<FontFaceBinding>& bindings) {
  // Start from the document's own fallback chain: the HTML importer records the CSS font-family
  // stacks there, and applyLayout below replaces the whole config with the one passed in.
  FontConfig fontConfig = document->fontConfig();
  if (!LoadFontConfig(&fontConfig, {}, fallbacks, command)) {
    return false;
  }
  // A source file is the only record of the family/style it provides, so a missing one cannot be
  // proven unused while any Text still asks for a font: shaping would silently fall back to a
  // substituted font with different outlines. Only a document that needs no font at all can skip
  // such a source.
  bool requiresFonts = !document->getRequiredFonts().empty();
  for (auto& node : document->nodes) {
    if (node->nodeType() != NodeType::Font) {
      continue;
    }
    auto* font = static_cast<Font*>(node.get());
    if (!font->file.empty()) {
      auto typeface = tgfx::Typeface::MakeFromPath(font->file);
      if (typeface == nullptr) {
        if (requiresFonts) {
          std::cerr << command << ": failed to load font '" << font->file << "'\n";
          return false;
        }
        std::cerr << command << ": failed to load font '" << font->file
                  << "', skipped because the document needs no font\n";
        continue;
      }
      fontConfig.registerFont(font->file, 0, typeface->fontFamily(), typeface->fontStyle());
      // Also reach this file through the fallback chain: a (family, style) key holds one primary
      // registration, so unicode-range subset files sharing that key would otherwise overwrite
      // each other and drop every glyph that lives in an earlier subset.
      fontConfig.addFallbackFont(font->file, 0);
    }
  }
  if (!LoadFontFaces(&fontConfig, bindings, command)) return false;
  FontEmbedder::ClearEmbeddedGlyphRuns(document);
  document->applyLayout(&fontConfig);
  FontEmbedder embedder = {};
  if (!embedder.embed(document)) {
    std::cerr << command << ": font embedding failed\n";
    return false;
  }
  return true;
}

bool WriteStringToFile(const std::string& content, const std::string& filePath,
                       const std::string& command) {
  auto tempPath = filePath + ".tmp";
  {
    std::ofstream out(tempPath);
    if (!out.is_open()) {
      std::cerr << command << ": failed to write '" << filePath
                << "' (creating temp file failed)\n";
      return false;
    }
    out << content;
    out.close();
    if (out.fail()) {
      std::cerr << command << ": failed to write '" << filePath << "' (writing temp file failed)\n";
      std::remove(tempPath.c_str());
      return false;
    }
  }
  std::error_code ec;
  std::filesystem::rename(tempPath, filePath, ec);
  if (ec) {
    std::cerr << command << ": failed to write '" << filePath
              << "' (replacing output file failed)\n";
    std::remove(tempPath.c_str());
    return false;
  }
  std::cout << command << ": wrote " << filePath << "\n";
  return true;
}

}  // namespace pagx::cli
