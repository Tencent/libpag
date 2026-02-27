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
#include "renderer/TextLayout.h"
#include "tgfx/core/Typeface.h"

namespace pagx::cli {

// ---- Platform-specific font definitions --------------------------------------------------------
// On backends where Typeface::MakeFromName() works (CoreText on macOS, DirectWrite on Windows),
// fonts are specified by family name only. On FreeType-only backends (macOS with TGFX_USE_FREETYPE,
// Linux), fonts are specified by file path with a name fallback.

struct NameFontEntry {
  const char* family;
  const char* style;
};

struct PathFontEntry {
  const char* path;
  int ttcIndex;
  const char* family;
  const char* style;
};

#if defined(__APPLE__) && !defined(TGFX_USE_FREETYPE)
// macOS CoreText backend: MakeFromName works directly.

static constexpr NameFontEntry RegisterFonts[] = {
    {"Arial", "Regular"},     {"Arial", "Bold"},        {"Arial", "Italic"},
    {"Arial", "Bold Italic"}, {"Helvetica", "Regular"}, {"Helvetica Neue", "Regular"},
};

static constexpr NameFontEntry FallbackFonts[] = {
    {"PingFang SC", ""}, {"Apple SD Gothic Neo", ""},
    {"Helvetica", ""},   {"Myanmar Sangam MN", ""},
    {"Thonburi", ""},    {"Menlo", ""},
    {"Kefa", ""},        {"Apple Color Emoji", ""},
};

inline std::shared_ptr<tgfx::Typeface> LoadTypeface(const NameFontEntry& font) {
  return tgfx::Typeface::MakeFromName(font.family, font.style);
}

#elif defined(__APPLE__) && defined(TGFX_USE_FREETYPE)
// macOS FreeType backend: MakeFromName returns nullptr, use file paths.

static constexpr PathFontEntry RegisterFonts[] = {
    {"/System/Library/Fonts/Supplemental/Arial.ttf", 0, "Arial", "Regular"},
    {"/System/Library/Fonts/Supplemental/Arial Bold.ttf", 0, "Arial", "Bold"},
    {"/System/Library/Fonts/Supplemental/Arial Italic.ttf", 0, "Arial", "Italic"},
    {"/System/Library/Fonts/Supplemental/Arial Bold Italic.ttf", 0, "Arial", "Bold Italic"},
    {"/System/Library/Fonts/Helvetica.ttc", 0, "Helvetica", "Regular"},
    {"/System/Library/Fonts/HelveticaNeue.ttc", 0, "Helvetica Neue", "Regular"},
};

static constexpr PathFontEntry FallbackFonts[] = {
    {"/System/Library/Fonts/PingFang.ttc", 1, "PingFang SC", ""},
    {"/System/Library/Fonts/AppleSDGothicNeo.ttc", 0, "Apple SD Gothic Neo", ""},
    {"/System/Library/Fonts/Helvetica.ttc", 0, "Helvetica", ""},
    {"/System/Library/Fonts/Myanmar Sangam MN.ttc", 0, "Myanmar Sangam MN", ""},
    {"/System/Library/Fonts/Thonburi.ttc", 0, "Thonburi", ""},
    {"/System/Library/Fonts/Menlo.ttc", 0, "Menlo", ""},
    {"/System/Library/Fonts/Kefa.ttc", 0, "Kefa", ""},
    {"/System/Library/Fonts/Apple Color Emoji.ttc", 0, "Apple Color Emoji", ""},
};

inline std::shared_ptr<tgfx::Typeface> LoadTypeface(const PathFontEntry& font) {
  auto typeface = tgfx::Typeface::MakeFromPath(font.path, font.ttcIndex);
  if (typeface == nullptr) {
    typeface = tgfx::Typeface::MakeFromName(font.family, font.style);
  }
  return typeface;
}

#elif defined(_WIN32)
// Windows: FreeType backend with DirectWrite support, MakeFromName works.

static constexpr NameFontEntry RegisterFonts[] = {
    {"Arial", "Regular"},
    {"Arial", "Bold"},
    {"Arial", "Italic"},
    {"Arial", "Bold Italic"},
    {"Times New Roman", "Regular"},
};

static constexpr NameFontEntry FallbackFonts[] = {
    {"Microsoft YaHei", ""},    {"Times New Roman", ""}, {"Microsoft Sans Serif", ""},
    {"Microsoft JhengHei", ""}, {"Leelawadee UI", ""},   {"MS Gothic", ""},
    {"Malgun Gothic", ""},      {"STSong", ""},
};

inline std::shared_ptr<tgfx::Typeface> LoadTypeface(const NameFontEntry& font) {
  return tgfx::Typeface::MakeFromName(font.family, font.style);
}

#elif defined(__linux__)
// Linux: FreeType only, MakeFromName returns nullptr, use file paths.

static constexpr PathFontEntry RegisterFonts[] = {
    {"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 0, "DejaVu Sans", "Book"},
    {"/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 0, "DejaVu Sans", "Bold"},
    {"/usr/share/fonts/truetype/dejavu/DejaVuSans-Oblique.ttf", 0, "DejaVu Sans", "Oblique"},
    {"/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf", 0, "Liberation Sans",
     "Regular"},
    {"/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf", 0, "Liberation Sans", "Bold"},
};

static constexpr PathFontEntry FallbackFonts[] = {
    {"/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc", 0, "Noto Sans CJK SC", ""},
    {"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 0, "DejaVu Sans", ""},
    {"/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf", 0, "Liberation Sans", ""},
    {"/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf", 0, "Noto Sans", ""},
    {"/usr/share/fonts/truetype/freefont/FreeSans.ttf", 0, "FreeSans", ""},
    {"/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf", 0, "Noto Color Emoji", ""},
};

inline std::shared_ptr<tgfx::Typeface> LoadTypeface(const PathFontEntry& font) {
  auto typeface = tgfx::Typeface::MakeFromPath(font.path, font.ttcIndex);
  if (typeface == nullptr) {
    typeface = tgfx::Typeface::MakeFromName(font.family, font.style);
  }
  return typeface;
}

#endif

// ---- Public API --------------------------------------------------------------------------------

/**
 * Registers common system fonts on the given TextLayout for font matching, and sets fallback
 * typefaces for characters not found in the primary font. The font list is platform-specific:
 * macOS uses CoreText names or FreeType paths, Windows uses DirectWrite names, and Linux uses
 * FreeType paths under /usr/share/fonts.
 */
inline void SetupSystemFallbackFonts(TextLayout& textLayout) {
#if defined(__APPLE__) || defined(_WIN32) || defined(__linux__)
  for (const auto& font : RegisterFonts) {
    auto typeface = LoadTypeface(font);
    if (typeface != nullptr) {
      textLayout.registerTypeface(typeface);
    }
  }

  std::vector<std::shared_ptr<tgfx::Typeface>> fallbacks = {};
  for (const auto& font : FallbackFonts) {
    auto typeface = LoadTypeface(font);
    if (typeface != nullptr) {
      fallbacks.push_back(std::move(typeface));
    }
  }
  if (!fallbacks.empty()) {
    textLayout.setFallbackTypefaces(std::move(fallbacks));
  }
#endif
}

/**
 * Returns a list of system fallback typefaces for use as additional fallbacks.
 */
inline std::vector<std::shared_ptr<tgfx::Typeface>> GetSystemFallbackTypefaces() {
  std::vector<std::shared_ptr<tgfx::Typeface>> fallbacks = {};
#if defined(__APPLE__) || defined(_WIN32) || defined(__linux__)
  for (const auto& font : FallbackFonts) {
    auto typeface = LoadTypeface(font);
    if (typeface != nullptr) {
      fallbacks.push_back(std::move(typeface));
    }
  }
#endif
  return fallbacks;
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
