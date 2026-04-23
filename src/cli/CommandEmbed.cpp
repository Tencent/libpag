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

#include "cli/CommandEmbed.h"
#include <iostream>
#include <string>
#include <vector>
#include "cli/CliUtils.h"
#include "pagx/FontConfig.h"
#include "pagx/PAGXExporter.h"
#include "renderer/FontEmbedder.h"
#include "renderer/ImageEmbedder.h"

struct EmbedOptions {
  std::string inputFile = {};
  std::string outputFile = {};
  std::vector<std::string> fontFiles = {};
  std::vector<std::string> fallbacks = {};
  bool skipFonts = false;
  bool skipImages = false;
};

static void PrintEmbedUsage() {
  std::cout
      << "Usage: pagx embed [options] <file.pagx>\n"
      << "\n"
      << "Embed fonts and images into a PAGX file as base64.\n"
      << "\n"
      << "Options:\n"
      << "  -o, --output <path>              Output file path (default: overwrite input)\n"
      << "  --file <path>                    Register a font file (can be specified multiple\n"
      << "                                   times)\n"
      << "  --fallback <path|name>           Add a fallback font file or system font name (can\n"
      << "                                   be specified multiple times)\n"
      << "  --skip-fonts                     Do not embed fonts\n"
      << "  --skip-images                    Do not embed images\n"
      << "  -h, --help                       Show this help message\n";
}

static int ParseEmbedOptions(int argc, char* argv[], EmbedOptions* options) {
  int i = 1;
  while (i < argc) {
    std::string arg = argv[i];
    if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
      options->outputFile = argv[++i];
    } else if (arg == "--file" && i + 1 < argc) {
      options->fontFiles.push_back(argv[++i]);
    } else if (arg == "--fallback" && i + 1 < argc) {
      options->fallbacks.push_back(argv[++i]);
    } else if (arg == "--skip-fonts") {
      options->skipFonts = true;
    } else if (arg == "--skip-images") {
      options->skipImages = true;
    } else if (arg == "--help" || arg == "-h") {
      PrintEmbedUsage();
      return -1;
    } else if (arg[0] == '-') {
      std::cerr << "pagx embed: unknown option '" << arg << "'\n";
      return 1;
    } else if (options->inputFile.empty()) {
      options->inputFile = arg;
    } else {
      std::cerr << "pagx embed: unexpected argument '" << arg << "'\n";
      return 1;
    }
    i++;
  }
  if (options->inputFile.empty()) {
    std::cerr << "pagx embed: missing input file\n";
    return 1;
  }
  if (options->outputFile.empty()) {
    options->outputFile = options->inputFile;
  }
  return 0;
}

namespace pagx::cli {

int RunEmbed(int argc, char* argv[]) {
  EmbedOptions options = {};
  auto parseResult = ParseEmbedOptions(argc, argv, &options);
  if (parseResult != 0) {
    return parseResult == -1 ? 0 : parseResult;
  }

  if (options.skipFonts && options.skipImages) {
    std::cerr << "pagx embed: --skip-fonts and --skip-images cannot both be set\n";
    return 1;
  }

  auto document = LoadDocument(options.inputFile, "pagx embed");
  if (document == nullptr) {
    return 1;
  }
  if (document->hasUnresolvedImports()) {
    std::cerr << "pagx embed: error: unresolved import directive, run 'pagx resolve' first\n";
    return 1;
  }

  if (!options.skipFonts) {
    FontConfig fontConfig = {};
    if (!LoadFontConfig(&fontConfig, options.fontFiles, options.fallbacks, "pagx embed")) {
      return 1;
    }
    FontEmbedder::ClearEmbeddedGlyphRuns(document.get());
    document->applyLayout(&fontConfig);
    FontEmbedder embedder = {};
    if (!embedder.embed(document.get())) {
      std::cerr << "pagx embed: font embedding failed\n";
      return 1;
    }
  }

  if (!options.skipImages) {
    ImageEmbedder imageEmbedder = {};
    if (!imageEmbedder.embed(document.get())) {
      std::cerr << "pagx embed: failed to load image '" << imageEmbedder.lastErrorPath() << "'\n";
      return 1;
    }
  }

  auto xml = PAGXExporter::ToXML(*document);
  if (!WriteStringToFile(xml, options.outputFile, "pagx embed")) {
    return 1;
  }
  return 0;
}

}  // namespace pagx::cli
