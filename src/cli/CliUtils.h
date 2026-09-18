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
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>
#include "pagx/FontConfig.h"
#include "pagx/PAGXDocument.h"
#include "pagx/SystemFonts.h"
#include "pagx/nodes/Layer.h"
#include "pagx/utils/VerifyUtils.h"
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

static inline bool FontStyleMatch(const std::string& requested, const std::string& actual) {
  if (requested.empty()) {
    return true;
  }
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
 * Resolves a system font by family and style with fallback. First attempts MakeFromName for an
 * exact match. If MakeFromName is unavailable (e.g. FreeType backend on macOS), falls back to
 * SystemFonts::FindFont to locate the font file path and loads via MakeFromPath.
 */
static inline std::shared_ptr<tgfx::Typeface> ResolveSystemTypeface(const std::string& family,
                                                                    const std::string& style) {
  auto typeface = tgfx::Typeface::MakeFromName(family, style);
  if (typeface != nullptr && FontFamilyMatch(family, typeface->fontFamily()) &&
      FontStyleMatch(style, typeface->fontStyle())) {
    return typeface;
  }
  if (!style.empty()) {
    typeface = tgfx::Typeface::MakeFromName(family, "");
    if (typeface != nullptr && FontFamilyMatch(family, typeface->fontFamily()) &&
        FontStyleMatch(style, typeface->fontStyle())) {
      return typeface;
    }
  }
  // Fallback: locate the font file via platform APIs and load by path.
  auto location = pagx::SystemFonts::FindFont(family, style);
  if (!location.path.empty()) {
    return tgfx::Typeface::MakeFromPath(location.path, location.ttcIndex);
  }
  if (!style.empty()) {
    location = pagx::SystemFonts::FindFont(family, "");
    if (!location.path.empty()) {
      return tgfx::Typeface::MakeFromPath(location.path, location.ttcIndex);
    }
  }
  return nullptr;
}

inline size_t FindLastPathSeparator(const std::string& path) {
  auto slash = path.rfind('/');
  auto backslash = path.rfind('\\');
  if (slash == std::string::npos) return backslash;
  if (backslash == std::string::npos) return slash;
  return std::max(slash, backslash);
}

/**
 * Resolves a fallback font specifier to a Typeface. Accepts either a font file path (containing
 * a path separator or ending with a known font extension) or a font name in "family[,style]" format.
 */
inline std::shared_ptr<tgfx::Typeface> ResolveFallbackTypeface(const std::string& specifier) {
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
  return ResolveSystemTypeface(family, style);
}

/**
 * Extracts the file extension from a path (without the dot) in lowercase, or returns an empty
 * string if none.
 */
inline std::string GetFileExtension(const std::string& path) {
  auto dot = path.rfind('.');
  if (dot != std::string::npos) {
    auto ext = path.substr(dot + 1);
    for (auto& ch : ext) {
      ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return ext;
  }
  return {};
}

/**
 * Replaces the file extension in a path with a new extension. If the path has no extension, appends
 * the new extension.
 */
inline std::string ReplaceExtension(const std::string& path, const std::string& newExt) {
  auto dot = path.rfind('.');
  if (dot != std::string::npos) {
    return path.substr(0, dot + 1) + newExt;
  }
  return path + "." + newExt;
}

/**
 * Extracts the directory part of a path (including trailing slash), or returns "./" if none.
 */
inline std::string GetDirectory(const std::string& path) {
  auto slash = FindLastPathSeparator(path);
  if (slash != std::string::npos) {
    return path.substr(0, slash + 1);
  }
  return "./";
}

/**
 * True when `path` is already absolute (POSIX `/...`) or a Windows drive path
 * (`C:/...` / `C:\...`), or carries an explicit scheme (`data:` / `http(s):`).
 * Such paths must not be re-rooted against a base directory.
 */
inline bool IsAbsoluteOrSchemePath(const std::string& path) {
  if (path.empty()) {
    return false;
  }
  if (path[0] == '/' || path[0] == '\\') {
    return true;  // POSIX absolute (or UNC).
  }
  if (path.size() >= 2 && path[1] == ':' &&
      ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z'))) {
    return true;  // Windows drive-absolute, e.g. C:/dir.
  }
  // Explicit scheme such as data: or http(s):, which is self-contained.
  if (path.rfind("data:", 0) == 0 || path.rfind("http://", 0) == 0 ||
      path.rfind("https://", 0) == 0) {
    return true;
  }
  return false;
}

/**
 * Joins a base directory (expected to end with a slash) and a referenced path.
 * Absolute / scheme-qualified references are returned unchanged so they are not
 * incorrectly re-rooted against the base directory (e.g. `./` + `/Users/x.svg`
 * → `.//Users/x.svg`).
 */
inline std::string JoinPath(const std::string& baseDir, const std::string& path) {
  if (IsAbsoluteOrSchemePath(path)) {
    return path;
  }
  return baseDir + path;
}

/**
 * Extracts the base name from a path (filename without directory and extension).
 */
inline std::string GetBaseName(const std::string& path) {
  auto slash = FindLastPathSeparator(path);
  auto base = (slash != std::string::npos) ? path.substr(slash + 1) : path;
  auto dot = base.rfind('.');
  if (dot != std::string::npos) {
    return base.substr(0, dot);
  }
  return base;
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

/**
 * Loads a PAGX document from a file. Prints load errors and warnings to stderr using the given
 * command name as prefix. Returns nullptr on failure.
 */
std::shared_ptr<PAGXDocument> LoadDocument(const std::string& filePath, const std::string& command);

/**
 * Loads font files and fallback typefaces into a FontConfig. Prints errors to stderr using the
 * given command name as prefix. Returns false on failure.
 */
bool LoadFontConfig(FontConfig* fontConfig, const std::vector<std::string>& fontFiles,
                    const std::vector<std::string>& fallbacks, const std::string& command);

/**
 * Writes a string to a file. Prints errors to stderr using the given command name as prefix.
 * On success, prints a "wrote <path>" message to stdout and returns true.
 */
bool WriteStringToFile(const std::string& content, const std::string& filePath,
                       const std::string& command);

}  // namespace pagx::cli
