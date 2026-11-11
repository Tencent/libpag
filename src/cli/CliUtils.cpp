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
#ifdef _WIN32
#include <windows.h>
#endif
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include "pagx/PAGXImporter.h"
#include "pagx/nodes/Font.h"
#include "renderer/FontEmbedder.h"

namespace pagx::cli {

std::ostream& CommandError(const std::string& command) {
  return std::cerr << command << ": error: ";
}

#ifdef _WIN32
std::wstring UTF8ToWide(const std::string& value) {
  if (value.empty()) return {};
  int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                 static_cast<int>(value.size()), nullptr, 0);
  if (size <= 0) return {};
  std::wstring wide(static_cast<size_t>(size), L'\0');
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
                      wide.data(), size);
  return wide;
}

std::wstring QuoteWindowsArg(const std::wstring& value) {
  std::wstring out = L"\"";
  size_t backslashes = 0;
  for (wchar_t ch : value) {
    if (ch == L'\\') {
      backslashes++;
      continue;
    }
    if (ch == L'\"') {
      out.append(backslashes * 2 + 1, L'\\');
      out.push_back(L'\"');
    } else {
      out.append(backslashes, L'\\');
      out.push_back(ch);
    }
    backslashes = 0;
  }
  out.append(backslashes * 2, L'\\');
  out.push_back(L'\"');
  return out;
}
#endif

std::shared_ptr<PAGXDocument> LoadDocument(const std::string& filePath,
                                           const std::string& command) {
  auto document = PAGXImporter::FromFile(filePath);
  if (document == nullptr) {
    CommandError(command) << "failed to load '" << filePath << "'\n";
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
      CommandError(command) << "failed to load font '" << fontFile << "'\n";
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
