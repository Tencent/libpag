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

#include "CommandFont.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include "pagx/PAGXExporter.h"
#include "pagx/PAGXImporter.h"
#include "renderer/FontEmbedder.h"
#include "renderer/TextLayout.h"
#include "tgfx/core/Typeface.h"

namespace pagx::cli {

static void PrintFontUsage() {
  std::cout << "Usage: pagx font [options] <file.pagx>\n"
            << "\n"
            << "Embeds fonts into a PAGX file by performing text layout and glyph extraction.\n"
            << "\n"
            << "Options:\n"
            << "  -o, --output <path>   Output file path (default: overwrite input)\n"
            << "  --font <path>         Extra font file path (can be specified multiple times)\n"
            << "  -h, --help            Show this help message\n";
}

int RunFont(int argc, char* argv[]) {
  std::string inputPath = {};
  std::string outputPath = {};
  std::vector<std::string> fontPaths = {};

  for (int i = 1; i < argc; i++) {
    if (std::strcmp(argv[i], "-h") == 0 || std::strcmp(argv[i], "--help") == 0) {
      PrintFontUsage();
      return 0;
    }
    if ((std::strcmp(argv[i], "-o") == 0 || std::strcmp(argv[i], "--output") == 0) &&
        i + 1 < argc) {
      outputPath = argv[++i];
      continue;
    }
    if (std::strcmp(argv[i], "--font") == 0 && i + 1 < argc) {
      fontPaths.push_back(argv[++i]);
      continue;
    }
    if (argv[i][0] == '-') {
      std::cerr << "pagx font: unknown option '" << argv[i] << "'\n";
      return 1;
    }
    if (inputPath.empty()) {
      inputPath = argv[i];
    } else {
      std::cerr << "pagx font: unexpected argument '" << argv[i] << "'\n";
      return 1;
    }
  }

  if (inputPath.empty()) {
    std::cerr << "pagx font: missing input file\n";
    PrintFontUsage();
    return 1;
  }

  if (outputPath.empty()) {
    outputPath = inputPath;
  }

  auto document = PAGXImporter::FromFile(inputPath);
  if (document == nullptr) {
    std::cerr << "pagx font: failed to load '" << inputPath << "'\n";
    return 1;
  }

  TextLayout textLayout = {};
  for (const auto& fontPath : fontPaths) {
    auto typeface = tgfx::Typeface::MakeFromPath(fontPath);
    if (typeface == nullptr) {
      std::cerr << "pagx font: failed to load font '" << fontPath << "'\n";
      return 1;
    }
    textLayout.registerTypeface(typeface);
  }

  auto result = textLayout.layout(document.get());

  FontEmbedder embedder = {};
  if (!embedder.embed(document.get(), result.shapedTextMap, result.textOrder)) {
    std::cerr << "pagx font: font embedding failed\n";
    return 1;
  }

  auto xml = PAGXExporter::ToXML(*document);

  std::ofstream out(outputPath);
  if (!out.is_open()) {
    std::cerr << "pagx font: failed to write '" << outputPath << "'\n";
    return 1;
  }
  out << xml;
  out.close();

  return 0;
}

}  // namespace pagx::cli
