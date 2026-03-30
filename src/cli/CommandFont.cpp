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
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include "cli/CliUtils.h"
#include "pagx/PAGXExporter.h"
#include "pagx/PAGXImporter.h"
#include "renderer/FontEmbedder.h"
#include "tgfx/core/Font.h"
#include "tgfx/core/Typeface.h"

namespace pagx::cli {

// ---- font info ----

struct FontInfoOptions {
  std::string fontFile = {};
  std::string fontName = {};
  float fontSize = 12.0f;
  bool jsonOutput = false;
};

static void PrintFontInfoUsage() {
  std::cout << "Usage: pagx font info [options]\n"
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

// Returns 0 on success, -1 if help was printed, 1 on error.
static int ParseFontInfoOptions(int argc, char* argv[], FontInfoOptions* options) {
  int i = 1;
  while (i < argc) {
    std::string arg = argv[i];
    if (arg == "--file" && i + 1 < argc) {
      options->fontFile = argv[++i];
    } else if (arg == "--name" && i + 1 < argc) {
      options->fontName = argv[++i];
    } else if (arg == "--size" && i + 1 < argc) {
      char* endPtr = nullptr;
      options->fontSize = strtof(argv[++i], &endPtr);
      if (endPtr == argv[i] || *endPtr != '\0' || !std::isfinite(options->fontSize) ||
          options->fontSize <= 0.0f) {
        std::cerr << "pagx font info: invalid font size '" << argv[i] << "'\n";
        return 1;
      }
    } else if (arg == "--json") {
      options->jsonOutput = true;
    } else if (arg == "--help" || arg == "-h") {
      PrintFontInfoUsage();
      return -1;
    } else if (arg[0] == '-') {
      std::cerr << "pagx font info: unknown option '" << arg << "'\n";
      return 1;
    }
    i++;
  }
  if (options->fontFile.empty() && options->fontName.empty()) {
    std::cerr << "pagx font info: either --file or --name is required\n";
    return 1;
  }
  if (!options->fontFile.empty() && !options->fontName.empty()) {
    std::cerr << "pagx font info: --file and --name are mutually exclusive\n";
    return 1;
  }
  return 0;
}

static int RunFontInfo(int argc, char* argv[]) {
  FontInfoOptions options = {};
  auto parseResult = ParseFontInfoOptions(argc, argv, &options);
  if (parseResult != 0) {
    return parseResult == -1 ? 0 : parseResult;
  }

  std::shared_ptr<tgfx::Typeface> typeface = nullptr;
  if (!options.fontFile.empty()) {
    typeface = tgfx::Typeface::MakeFromPath(options.fontFile);
    if (typeface == nullptr) {
      std::cerr << "pagx font info: failed to load font file '" << options.fontFile << "'\n";
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
  std::cout
      << "Usage: pagx font embed [options] <file.pagx>\n"
      << "\n"
      << "Embed fonts into a PAGX file by performing text layout and glyph extraction.\n"
      << "\n"
      << "Options:\n"
      << "  -o, --output <path>              Output file path (default: overwrite input)\n"
      << "  --file <path>                    Register a font file (can be specified multiple\n"
      << "                                   times)\n"
      << "  --fallback <path|name>           Add a fallback font file or system font name (can\n"
      << "                                   be specified multiple times)\n"
      << "  -h, --help                       Show this help message\n";
}

// Returns 0 on success, -1 if help was printed, 1 on error.
static int ParseFontEmbedOptions(int argc, char* argv[], FontEmbedOptions* options) {
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
      return -1;
    } else if (arg[0] == '-') {
      std::cerr << "pagx font embed: unknown option '" << arg << "'\n";
      return 1;
    } else if (options->inputFile.empty()) {
      options->inputFile = arg;
    } else {
      std::cerr << "pagx font embed: unexpected argument '" << arg << "'\n";
      return 1;
    }
    i++;
  }
  if (options->inputFile.empty()) {
    std::cerr << "pagx font embed: missing input file\n";
    return 1;
  }
  if (options->outputFile.empty()) {
    options->outputFile = options->inputFile;
  }
  return 0;
}

static int RunFontEmbed(int argc, char* argv[]) {
  FontEmbedOptions options = {};
  auto parseResult = ParseFontEmbedOptions(argc, argv, &options);
  if (parseResult != 0) {
    return parseResult == -1 ? 0 : parseResult;
  }

  auto document = PAGXImporter::FromFile(options.inputFile);
  if (document == nullptr) {
    std::cerr << "pagx font embed: failed to load '" << options.inputFile << "'\n";
    return 1;
  }

  // Load font files.
  FontConfig fontProvider = {};
  for (const auto& fontFile : options.fontFiles) {
    auto typeface = tgfx::Typeface::MakeFromPath(fontFile);
    if (typeface == nullptr) {
      std::cerr << "pagx font embed: failed to load font '" << fontFile << "'\n";
      return 1;
    }
    fontProvider.registerTypeface(typeface);
  }

  // Resolve fallback typefaces: user-specified first, then system fallbacks.
  std::vector<std::shared_ptr<tgfx::Typeface>> fallbackTypefaces = {};
  for (const auto& fallbackStr : options.fallbacks) {
    auto typeface = ResolveFallbackTypeface(fallbackStr);
    if (typeface == nullptr) {
      std::cerr << "pagx font embed: fallback font '" << fallbackStr << "' not found\n";
      return 1;
    }
    fontProvider.registerTypeface(typeface);
    fallbackTypefaces.push_back(typeface);
  }
  if (!fallbackTypefaces.empty()) {
    fontProvider.addFallbackTypefaces(std::move(fallbackTypefaces));
  }
  auto systemFallbacks = SystemFonts::FallbackTypefaces();
  for (const auto& loc : systemFallbacks) {
    fontProvider.addFallbackFont(loc.path, loc.ttcIndex, loc.fontFamily, loc.fontStyle);
  }

  document->setFontConfig(fontProvider);
  FontEmbedder::ClearEmbeddedGlyphRuns(document.get());
  document->applyLayout();

  FontEmbedder embedder = {};
  if (!embedder.embed(document.get())) {
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

  std::cout << "pagx font embed: wrote " << options.outputFile << "\n";
  return 0;
}

// ---- main entry ----

static void PrintFontUsage() {
  std::cout << "Usage: pagx font <subcommand> [options]\n"
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
