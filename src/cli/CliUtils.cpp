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

// Registers every face of a font file and returns how many could be opened, 0 meaning the file
// itself cannot be read. `MakeFromPath` and `registerFont` both default to face 0, which is the
// whole of a single-face file but merely the first of a collection: PingFang.ttc holds 24 faces,
// and a request for "PingFang SC Medium" resolves to face 7 — unreachable unless every face is
// registered under its own (family, style) key. Probing stops at the first index that fails to
// open, so a single-face file costs one extra call.
static int RegisterFontFaces(FontConfig* fontConfig, const std::string& path,
                             bool addToFallbackChain) {
  static constexpr int MaxFontFaces = 64;
  int faceCount = 0;
  for (int index = 0; index < MaxFontFaces; index++) {
    auto typeface = tgfx::Typeface::MakeFromPath(path, index);
    if (typeface == nullptr) {
      break;
    }
    fontConfig->registerFont(path, index, typeface->fontFamily(), typeface->fontStyle());
    if (addToFallbackChain) {
      fontConfig->addFallbackFont(path, index);
    }
    faceCount++;
  }
  return faceCount;
}

bool LoadFontConfig(FontConfig* fontConfig, const std::vector<std::string>& fontFiles,
                    const std::vector<std::string>& fallbacks, const std::string& command) {
  for (const auto& fontFile : fontFiles) {
    if (RegisterFontFaces(fontConfig, fontFile, false) == 0) {
      std::cerr << command << ": failed to load font '" << fontFile << "'\n";
      return false;
    }
  }
  for (const auto& fallbackStr : fallbacks) {
    bool isFilePath =
        fallbackStr.find('/') != std::string::npos || fallbackStr.find('\\') != std::string::npos;
    if (!isFilePath) {
      auto ext = GetFileExtension(fallbackStr);
      isFilePath = ext == "ttf" || ext == "otf" || ext == "ttc" || ext == "woff" || ext == "woff2";
    }
    if (isFilePath) {
      // Register as both a main font (precise family match) and a fallback font, matching the
      // pre-refactor behavior where fallback fonts were also reachable via exact family lookup.
      // A file that cannot be read is fatal: the caller asked for this exact source, and silently
      // dropping it would shape the text with a substituted face instead.
      if (RegisterFontFaces(fontConfig, fallbackStr, true) == 0) {
        std::cerr << command << ": fallback font '" << fallbackStr << "' not found\n";
        return false;
      }
    } else {
      auto commaPos = fallbackStr.find(',');
      auto family = commaPos != std::string::npos ? fallbackStr.substr(0, commaPos) : fallbackStr;
      auto style = commaPos != std::string::npos ? fallbackStr.substr(commaPos + 1) : std::string();
      if (!fontConfig->registerSystemFont(family, style)) {
        std::cerr << command << ": fallback font '" << fallbackStr << "' not found\n";
        return false;
      }
      fontConfig->addFallbackSystemFont(family, style);
    }
  }
  return true;
}

bool EmbedFonts(PAGXDocument* document, const std::vector<std::string>& fallbacks,
                const std::string& command) {
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
    if (font->file.empty()) {
      continue;
    }
    // Also reach this file through the fallback chain: a (family, style) key holds one primary
    // registration, so unicode-range subset files sharing that key would otherwise overwrite each
    // other and drop every glyph that lives in an earlier subset.
    if (RegisterFontFaces(&fontConfig, font->file, true) == 0) {
      if (requiresFonts) {
        std::cerr << command << ": failed to load font '" << font->file << "'\n";
        return false;
      }
      std::cerr << command << ": failed to load font '" << font->file
                << "', skipped because the document needs no font\n";
    }
  }
  FontEmbedder::ClearEmbeddedGlyphRuns(document);
  document->applyLayout(&fontConfig);
  FontEmbedder embedder = {};
  if (!embedder.embed(document)) {
    std::cerr << command << ": font embedding failed\n";
    return false;
  }
  // A text that shaped to nothing means no font available here covered its characters. Writing the
  // document anyway would silently produce a file whose text is blank on every host lacking the
  // authored font, so fail loudly and name the texts instead.
  if (!embedder.unembeddedTexts().empty()) {
    std::cerr << command << ": " << embedder.unembeddedTexts().size()
              << " text(s) produced no glyph run, no available font covers them:\n";
    for (const auto& description : embedder.unembeddedTexts()) {
      std::cerr << command << ":   " << description << "\n";
    }
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
