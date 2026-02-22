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
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include "pagx/PAGXExporter.h"
#include "pagx/PAGXImporter.h"
#include "renderer/FontEmbedder.h"
#include "renderer/TextLayout.h"
#include "tgfx/core/Font.h"
#include "tgfx/core/Typeface.h"

namespace pagx::cli {

static std::string EscapeJson(const std::string& input) {
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
      default:
        result += ch;
        break;
    }
  }
  return result;
}

// Parse "family,style" or "family" into separate components.
static void ParseFontName(const std::string& nameStr, std::string* family, std::string* style) {
  auto commaPos = nameStr.find(',');
  if (commaPos != std::string::npos) {
    *family = nameStr.substr(0, commaPos);
    *style = nameStr.substr(commaPos + 1);
  } else {
    *family = nameStr;
    style->clear();
  }
}

// Resolve a typeface by name, first searching loaded typefaces then falling back to system fonts.
static std::shared_ptr<tgfx::Typeface> ResolveTypeface(
    const std::string& family, const std::string& style,
    const std::vector<std::shared_ptr<tgfx::Typeface>>& loadedTypefaces) {
  for (const auto& typeface : loadedTypefaces) {
    if (typeface->fontFamily() == family) {
      if (style.empty() || typeface->fontStyle() == style) {
        return typeface;
      }
    }
  }
  return tgfx::Typeface::MakeFromName(family, style);
}

// ---- font info ----

struct FontInfoOptions {
  std::string fontFile = {};
  std::string fontName = {};
  float fontSize = 12.0f;
  bool jsonOutput = false;
};

static void PrintFontInfoUsage() {
  std::cerr << "Usage: pagx font info [options]\n"
            << "\n"
            << "Query font identity and metrics.\n"
            << "\n"
            << "Options:\n"
            << "  --file <path>          Font file path\n"
            << "  --name <family,style>  System font name (e.g., \"Arial\" or \"Arial,Bold\")\n"
            << "  --size <pt>            Font size in points (default: 12)\n"
            << "  --json                 Output in JSON format\n"
            << "\n"
            << "Either --file or --name is required (mutually exclusive).\n";
}

static bool ParseFontInfoOptions(int argc, char* argv[], FontInfoOptions* options) {
  int i = 1;
  while (i < argc) {
    std::string arg = argv[i];
    if (arg == "--file" && i + 1 < argc) {
      options->fontFile = argv[++i];
    } else if (arg == "--name" && i + 1 < argc) {
      options->fontName = argv[++i];
    } else if (arg == "--size" && i + 1 < argc) {
      options->fontSize = strtof(argv[++i], nullptr);
      if (options->fontSize <= 0.0f) {
        std::cerr << "pagx font info: font size must be positive\n";
        return false;
      }
    } else if (arg == "--json") {
      options->jsonOutput = true;
    } else if (arg == "--help" || arg == "-h") {
      PrintFontInfoUsage();
      return false;
    } else if (arg[0] == '-') {
      std::cerr << "pagx font info: unknown option '" << arg << "'\n";
      return false;
    }
    i++;
  }
  if (options->fontFile.empty() && options->fontName.empty()) {
    std::cerr << "pagx font info: either --file or --name is required\n";
    return false;
  }
  if (!options->fontFile.empty() && !options->fontName.empty()) {
    std::cerr << "pagx font info: --file and --name are mutually exclusive\n";
    return false;
  }
  return true;
}

static int RunFontInfo(int argc, char* argv[]) {
  FontInfoOptions options = {};
  if (!ParseFontInfoOptions(argc, argv, &options)) {
    return 1;
  }

  std::shared_ptr<tgfx::Typeface> typeface = nullptr;
  if (!options.fontFile.empty()) {
    typeface = tgfx::Typeface::MakeFromPath(options.fontFile);
    if (typeface == nullptr) {
      std::cerr << "pagx font info: failed to load font file '" << options.fontFile << "'\n";
      return 1;
    }
  } else {
    std::string family = {};
    std::string style = {};
    ParseFontName(options.fontName, &family, &style);
    typeface = tgfx::Typeface::MakeFromName(family, style);
    if (typeface == nullptr) {
      std::cerr << "pagx font info: font '" << options.fontName << "' not found\n";
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

// ---- font embed ----

struct FontEmbedOptions {
  std::string inputFile = {};
  std::string outputFile = {};
  std::vector<std::string> fontFiles = {};
  std::vector<std::string> fallbacks = {};
};

static void PrintFontEmbedUsage() {
  std::cerr
      << "Usage: pagx font embed [options] <file.pagx>\n"
      << "\n"
      << "Embed fonts into a PAGX file by performing text layout and glyph extraction.\n"
      << "\n"
      << "Options:\n"
      << "  -o, --output <path>              Output file path (default: overwrite input)\n"
      << "  --file <path>                    Add a font file (can be specified multiple times)\n"
      << "  --fallback <family[,style]>      Add a fallback font (can be specified multiple\n"
      << "                                   times, tried in order). Matches loaded --file\n"
      << "                                   fonts first, then system fonts.\n"
      << "  -h, --help                       Show this help message\n";
}

static bool ParseFontEmbedOptions(int argc, char* argv[], FontEmbedOptions* options) {
  int i = 1;
  while (i < argc) {
    std::string arg = argv[i];
    if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
      options->outputFile = argv[++i];
    } else if (arg == "--file" && i + 1 < argc) {
      options->fontFiles.push_back(argv[++i]);
    } else if (arg == "--fallback" && i + 1 < argc) {
      options->fallbacks.push_back(argv[++i]);
    } else if (arg == "--help" || arg == "-h") {
      PrintFontEmbedUsage();
      return false;
    } else if (arg[0] == '-') {
      std::cerr << "pagx font embed: unknown option '" << arg << "'\n";
      return false;
    } else if (options->inputFile.empty()) {
      options->inputFile = arg;
    } else {
      std::cerr << "pagx font embed: unexpected argument '" << arg << "'\n";
      return false;
    }
    i++;
  }
  if (options->inputFile.empty()) {
    std::cerr << "pagx font embed: missing input file\n";
    PrintFontEmbedUsage();
    return false;
  }
  if (options->outputFile.empty()) {
    options->outputFile = options->inputFile;
  }
  return true;
}

static int RunFontEmbed(int argc, char* argv[]) {
  FontEmbedOptions options = {};
  if (!ParseFontEmbedOptions(argc, argv, &options)) {
    return 1;
  }

  auto document = PAGXImporter::FromFile(options.inputFile);
  if (document == nullptr) {
    std::cerr << "pagx font embed: failed to load '" << options.inputFile << "'\n";
    return 1;
  }

  // Load font files.
  TextLayout textLayout = {};
  std::vector<std::shared_ptr<tgfx::Typeface>> loadedTypefaces = {};
  for (const auto& fontFile : options.fontFiles) {
    auto typeface = tgfx::Typeface::MakeFromPath(fontFile);
    if (typeface == nullptr) {
      std::cerr << "pagx font embed: failed to load font '" << fontFile << "'\n";
      return 1;
    }
    loadedTypefaces.push_back(typeface);
    textLayout.registerTypeface(typeface);
  }

  // Resolve fallback typefaces.
  std::vector<std::shared_ptr<tgfx::Typeface>> fallbackTypefaces = {};
  for (const auto& fallbackStr : options.fallbacks) {
    std::string family = {};
    std::string style = {};
    ParseFontName(fallbackStr, &family, &style);
    auto typeface = ResolveTypeface(family, style, loadedTypefaces);
    if (typeface == nullptr) {
      std::cerr << "pagx font embed: fallback font '" << fallbackStr << "' not found\n";
      return 1;
    }
    fallbackTypefaces.push_back(typeface);
  }
  if (!fallbackTypefaces.empty()) {
    textLayout.setFallbackTypefaces(std::move(fallbackTypefaces));
  }

  auto result = textLayout.layout(document.get());

  FontEmbedder embedder = {};
  if (!embedder.embed(document.get(), result.shapedTextMap, result.textOrder)) {
    std::cerr << "pagx font embed: font embedding failed\n";
    return 1;
  }

  auto xml = PAGXExporter::ToXML(*document);

  std::ofstream out(options.outputFile);
  if (!out.is_open()) {
    std::cerr << "pagx font embed: failed to write '" << options.outputFile << "'\n";
    return 1;
  }
  out << xml;
  out.close();
  if (out.fail()) {
    std::cerr << "pagx font embed: error writing to '" << options.outputFile << "'\n";
    return 1;
  }

  return 0;
}

// ---- main entry ----

static void PrintFontUsage() {
  std::cerr << "Usage: pagx font <subcommand> [options]\n"
            << "\n"
            << "Subcommands:\n"
            << "  info    Query font identity and metrics\n"
            << "  embed   Embed fonts into a PAGX file\n"
            << "\n"
            << "Run 'pagx font <subcommand> --help' for details.\n";
}

int RunFont(int argc, char* argv[]) {
  if (argc < 2) {
    PrintFontUsage();
    return 1;
  }

  std::string subcommand = argv[1];

  if (subcommand == "--help" || subcommand == "-h") {
    PrintFontUsage();
    return 0;
  }

  if (subcommand == "info") {
    return RunFontInfo(argc - 1, argv + 1);
  }
  if (subcommand == "embed") {
    return RunFontEmbed(argc - 1, argv + 1);
  }

  std::cerr << "pagx font: unknown subcommand '" << subcommand << "'\n";
  std::cerr << "Run 'pagx font --help' for usage.\n";
  return 1;
}

}  // namespace pagx::cli
