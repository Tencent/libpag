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

#include "cli/CommandFont.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>
#include "cli/CliUtils.h"
#include "pagx/SystemFonts.h"
#include "tgfx/core/Font.h"
#include "tgfx/core/Typeface.h"

namespace pagx::cli {

// ---- font query ----

struct FontOptions {
  std::string fontFile = {};
  std::string fontName = {};
  float fontSize = 12.0f;
  bool sizeSpecified = false;
  bool jsonOutput = false;
  bool listMode = false;
};

static void PrintFontUsage() {
  std::cout << "Usage: pagx font [options]\n"
            << "\n"
            << "Query a font file, a system font by name, or enumerate system font families.\n"
            << "\n"
            << "Options:\n"
            << "  --file <path>          Query a font file\n"
            << "  --name <family,style>  Query a system font (e.g., \"Arial\" or \"Arial,Bold\")\n"
            << "  --size <pt>            Font size in points (default: 12)\n"
            << "  --json                 Output in JSON format\n"
            << "  --list                 List every installed system font family\n"
            << "  -h, --help             Show this help\n"
            << "\n"
            << "Exactly one of --file, --name, or --list must be specified.\n";
}

static void FormatFontListText(const std::vector<pagx::FontFamilyEntry>& entries) {
  for (const auto& entry : entries) {
    if (entry.styles.empty()) {
      std::cout << entry.family << "\n";
      continue;
    }
    std::cout << entry.family << " (";
    for (size_t i = 0; i < entry.styles.size(); i++) {
      if (i > 0) {
        std::cout << ", ";
      }
      std::cout << entry.styles[i];
    }
    std::cout << ")\n";
  }
}

static void FormatFontListJson(const std::vector<pagx::FontFamilyEntry>& entries) {
  std::cout << "[";
  for (size_t i = 0; i < entries.size(); i++) {
    if (i > 0) {
      std::cout << ",";
    }
    std::cout << "{\"family\":\"" << EscapeJson(entries[i].family) << "\",\"styles\":[";
    for (size_t j = 0; j < entries[i].styles.size(); j++) {
      if (j > 0) {
        std::cout << ",";
      }
      std::cout << "\"" << EscapeJson(entries[i].styles[j]) << "\"";
    }
    std::cout << "]}";
  }
  std::cout << "]\n";
}

int RunFont(int argc, char* argv[]) {
  if (argc < 2) {
    PrintFontUsage();
    return 1;
  }

  std::string first = argv[1];
  if (first == "--help" || first == "-h") {
    PrintFontUsage();
    return 0;
  }

  if (first == "info") {
    std::cerr << "pagx font: 'info' subcommand has been removed, use 'pagx font' instead\n";
    return 1;
  }

  if (first == "embed") {
    std::cerr << "pagx font: 'embed' subcommand has been removed, use 'pagx embed' instead\n";
    return 1;
  }

  FontOptions options = {};
  int i = 1;
  while (i < argc) {
    std::string arg = argv[i];
    if (arg == "--file" && i + 1 < argc) {
      options.fontFile = argv[++i];
    } else if (arg == "--name" && i + 1 < argc) {
      options.fontName = argv[++i];
    } else if (arg == "--size" && i + 1 < argc) {
      char* endPtr = nullptr;
      options.fontSize = strtof(argv[++i], &endPtr);
      if (endPtr == argv[i] || *endPtr != '\0' || !std::isfinite(options.fontSize) ||
          options.fontSize <= 0.0f) {
        std::cerr << "pagx font: invalid font size '" << argv[i] << "'\n";
        return 1;
      }
      options.sizeSpecified = true;
    } else if (arg == "--json") {
      options.jsonOutput = true;
    } else if (arg == "--list") {
      options.listMode = true;
    } else if (arg == "--help" || arg == "-h") {
      PrintFontUsage();
      return 0;
    } else if (arg[0] == '-') {
      std::cerr << "pagx font: unknown option '" << arg << "'\n";
      return 1;
    } else {
      std::cerr << "pagx font: unexpected argument '" << arg << "'\n";
      return 1;
    }
    i++;
  }

  if (options.listMode && (!options.fontFile.empty() || !options.fontName.empty())) {
    std::cerr << "pagx font: --list cannot be combined with --file or --name\n";
    return 1;
  }
  if (!options.fontFile.empty() && !options.fontName.empty()) {
    std::cerr << "pagx font: --file and --name are mutually exclusive\n";
    return 1;
  }
  if (!options.listMode && options.fontFile.empty() && options.fontName.empty()) {
    std::cerr << "pagx font: either --file, --name, or --list is required\n";
    return 1;
  }

  if (options.listMode) {
    auto entries = pagx::SystemFonts::AllFontFamilies();
    std::sort(entries.begin(), entries.end());
    if (options.jsonOutput) {
      FormatFontListJson(entries);
    } else {
      FormatFontListText(entries);
    }
    return 0;
  }

  std::shared_ptr<tgfx::Typeface> typeface = nullptr;
  if (!options.fontFile.empty()) {
    typeface = tgfx::Typeface::MakeFromPath(options.fontFile);
    if (typeface == nullptr) {
      std::cerr << "pagx font: failed to load font file '" << options.fontFile << "'\n";
      return 1;
    }
  } else {
    auto commaPos = options.fontName.find(',');
    auto family =
        commaPos != std::string::npos ? options.fontName.substr(0, commaPos) : options.fontName;
    auto style =
        commaPos != std::string::npos ? options.fontName.substr(commaPos + 1) : std::string();
    typeface = ResolveSystemTypeface(family, style);
    if (typeface == nullptr) {
      std::cerr << "pagx font: font '" << options.fontName << "' not found\n";
      return 1;
    }
  }

  tgfx::Font font(typeface, options.fontSize);
  auto metrics = font.getMetrics();

  if (options.jsonOutput) {
    std::cout << "{\"fontFamily\": \"" << EscapeJson(typeface->fontFamily()) << "\""
              << ", \"fontStyle\": \"" << EscapeJson(typeface->fontStyle()) << "\""
              << ", \"glyphsCount\": " << typeface->glyphsCount()
              << ", \"unitsPerEm\": " << typeface->unitsPerEm()
              << ", \"hasColor\": " << (typeface->hasColor() ? "true" : "false")
              << ", \"hasOutlines\": " << (typeface->hasOutlines() ? "true" : "false")
              << ", \"fontSize\": " << options.fontSize << ", \"top\": " << metrics.top
              << ", \"ascent\": " << metrics.ascent << ", \"descent\": " << metrics.descent
              << ", \"bottom\": " << metrics.bottom << ", \"leading\": " << metrics.leading
              << ", \"xMin\": " << metrics.xMin << ", \"xMax\": " << metrics.xMax
              << ", \"xHeight\": " << metrics.xHeight << ", \"capHeight\": " << metrics.capHeight
              << ", \"underlineThickness\": " << metrics.underlineThickness
              << ", \"underlinePosition\": " << metrics.underlinePosition << "}\n";
  } else {
    std::cout << "Font:\n";
    std::cout << "  fontFamily:  " << typeface->fontFamily() << "\n";
    std::cout << "  fontStyle:   " << typeface->fontStyle() << "\n";
    std::cout << "  glyphsCount: " << typeface->glyphsCount() << "\n";
    std::cout << "  unitsPerEm:  " << typeface->unitsPerEm() << "\n";
    std::cout << "  hasColor:    " << (typeface->hasColor() ? "true" : "false") << "\n";
    std::cout << "  hasOutlines: " << (typeface->hasOutlines() ? "true" : "false") << "\n";
    std::cout << "\n";
    std::cout << "Metrics (at " << options.fontSize << "pt):\n";
    std::cout << "  top:                  " << metrics.top << "\n";
    std::cout << "  ascent:               " << metrics.ascent << "\n";
    std::cout << "  descent:              " << metrics.descent << "\n";
    std::cout << "  bottom:               " << metrics.bottom << "\n";
    std::cout << "  leading:              " << metrics.leading << "\n";
    std::cout << "  xMin:                 " << metrics.xMin << "\n";
    std::cout << "  xMax:                 " << metrics.xMax << "\n";
    std::cout << "  xHeight:              " << metrics.xHeight << "\n";
    std::cout << "  capHeight:            " << metrics.capHeight << "\n";
    std::cout << "  underlineThickness:   " << metrics.underlineThickness << "\n";
    std::cout << "  underlinePosition:    " << metrics.underlinePosition << "\n";
  }

  return 0;
}

}  // namespace pagx::cli
