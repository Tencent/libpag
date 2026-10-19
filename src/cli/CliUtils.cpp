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
#include <unordered_set>
#include "pagx/LayoutContext.h"
#include "pagx/PAGXImporter.h"
#include "pagx/nodes/Font.h"
#include "pagx/nodes/GlyphRun.h"
#include "pagx/nodes/Text.h"
#include "pagx/utils/FontFileLoader.h"
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

bool ParseFontAlias(const std::string& value, FontAlias* alias, const std::string& command) {
  auto equals = value.find('=');
  if (equals == std::string::npos || equals == 0 || equals + 1 >= value.size()) {
    std::cerr << command << ": malformed --font-alias '" << value
              << "' (expected <family>=<path>)\n";
    return false;
  }
  alias->family = value.substr(0, equals);
  alias->path = value.substr(equals + 1);
  return true;
}

bool LoadFontConfig(FontConfig* fontConfig, const std::vector<std::string>& fontFiles,
                    const std::vector<std::string>& fallbacks,
                    const std::vector<FontAlias>& aliases, const std::string& command) {
  for (const auto& fontFile : fontFiles) {
    if (RegisterFontFileFaces(fontConfig, fontFile, false) == 0) {
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
      if (RegisterFontFileFaces(fontConfig, fallbackStr, true) == 0) {
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
  // Font aliases come last so an explicit (family, file) pair always wins over a plain file
  // registration whose name table happened to produce the same key. The style half always comes
  // from each face, so the faces of an aliased collection stay distinguishable.
  for (const auto& alias : aliases) {
    if (RegisterFontFileFaces(fontConfig, alias.path, true, alias.family) == 0) {
      std::cerr << command << ": font alias '" << alias.family << "=" << alias.path
                << "' not found\n";
      return false;
    }
  }
  return true;
}

std::vector<std::string> FindUnloadableFontFiles(const PAGXDocument* document) {
  std::vector<std::string> files = {};
  for (auto& node : document->nodes) {
    if (node->nodeType() != NodeType::Font) {
      continue;
    }
    auto* font = static_cast<const Font*>(node.get());
    if (!font->file.empty() && MakeTypefacesFromFile(font->file).empty()) {
      files.push_back(font->file);
    }
  }
  return files;
}

// A text whose requested family resolves to nothing was shaped entirely with the substitute face
// the platform picked, and those outlines are now baked into the document. Such a text still has
// glyph runs, so FontEmbedder::unembeddedTexts() cannot see it; without this check the failure
// ships silently and only shows up when the render is compared with the source page.
//
// "Resolves to nothing" means layout could not find a face for the family at all. A family that
// the platform knows under another spelling is not reported: fontconfig-style aliasing
// (Arial -> Liberation Sans) still gives layout a face, and rejecting that would make a document
// authored on one platform unembeddable on another. A name the platform has never heard of is the
// case that silently produces substitute outlines.
static std::vector<std::string> CollectSubstitutedTexts(PAGXDocument* document) {
  std::vector<std::string> substituted = {};
  // applyLayout registers the document's own font files into the document's config, not into the
  // copy the caller passed in, so the resolved set is the document's.
  LayoutContext context(&document->fontConfig());
  for (auto& node : document->nodes) {
    if (node->nodeType() != NodeType::Text) {
      continue;
    }
    auto* text = static_cast<const Text*>(node.get());
    // Only texts this pass will shape can be substituted: a text that already carries glyph runs
    // was shaped by an earlier pass, from a font file or from outlines baked back then.
    if (text->fontFamily.empty() || !text->glyphRuns.empty()) {
      continue;
    }
    if (context.findTypeface(text->fontFamily, text->fontStyle) != nullptr) {
      continue;
    }
    if (!SystemFonts::FindFont(text->fontFamily, text->fontStyle).path.empty()) {
      continue;
    }
    substituted.push_back(FontEmbedder::DescribeText(text));
  }
  return substituted;
}

bool EmbedFonts(PAGXDocument* document, const std::vector<std::string>& fallbacks,
                const std::vector<FontAlias>& aliases, const std::string& command) {
  // Start from the document's own fallback chain: the HTML importer records the CSS font-family
  // stacks there, and applyLayout below replaces the whole config with the one passed in.
  FontConfig fontConfig = document->fontConfig();
  if (!LoadFontConfig(&fontConfig, {}, fallbacks, aliases, command)) {
    return false;
  }
  if (!fallbacks.empty() || !aliases.empty()) {
    FontEmbedder::ClearEmbeddedGlyphRuns(document);
  }
  // A source file is the only record of the family/style it provides, so a missing one cannot be
  // proven unused while any Text still asks for a font: shaping would silently fall back to a
  // substituted font with different outlines. Only a document that needs no font at all can skip
  // such a source. Pre-shaped runs have an explicit font dependency, or embedded glyphs that
  // need no source file at all.
  bool runtimeNeedsFonts = false;
  std::unordered_set<std::string> referencedFiles;
  for (auto& node : document->nodes) {
    if (node->nodeType() != NodeType::Text) continue;
    auto* text = static_cast<Text*>(node.get());
    runtimeNeedsFonts |= text->glyphRuns.empty() && !text->text.empty();
    for (auto* run : text->glyphRuns) {
      if (run->font != nullptr && run->font->glyphs.empty()) {
        referencedFiles.insert(run->font->file);
      }
    }
  }
  for (auto& file : FindUnloadableFontFiles(document)) {
    if (runtimeNeedsFonts || referencedFiles.count(file) != 0) {
      std::cerr << command << ": failed to load font '" << file << "'\n";
      return false;
    }
    std::cerr << command << ": failed to load font '" << file
              << "', skipped because the document needs no font\n";
  }
  // applyLayout registers the document's own font files on top of this config.
  document->applyLayout(&fontConfig);
  // Collected before embedding, while the difference between a runtime-shaped text and a
  // pre-shaped one is still visible.
  auto substituted = CollectSubstitutedTexts(document);
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
  if (!substituted.empty()) {
    std::cerr << command << ": " << substituted.size()
              << " text(s) ask for a font family that resolves to no available face; the baked "
                 "glyphs would come from a substitute:\n";
    for (const auto& description : substituted) {
      std::cerr << command << ":   " << description << "\n";
    }
    std::cerr << command << ": install the font or pass it with --font-alias <family>=<file>\n";
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
