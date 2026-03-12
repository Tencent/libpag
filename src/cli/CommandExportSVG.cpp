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

#include "cli/CommandExportSVG.h"
#include <iostream>
#include <string>
#include "pagx/PAGXImporter.h"
#include "pagx/SVGExporter.h"

namespace pagx::cli {

struct ExportSVGOptions {
  std::string inputFile = {};
  std::string outputFile = {};
  int indent = 2;
  bool noXmlDeclaration = false;
};

static void PrintExportSVGUsage() {
  std::cout << "Usage: pagx export-svg [options] <file.pagx>\n"
            << "\n"
            << "Options:\n"
            << "  -o, --output <path>       Output SVG file path (default: input path with .svg "
               "extension)\n"
            << "  --indent <n>              Indentation spaces (default: 2)\n"
            << "  --no-xml-declaration      Omit the <?xml ...?> declaration\n";
}

static int ParseExportSVGOptions(int argc, char* argv[], ExportSVGOptions* options) {
  int i = 1;
  while (i < argc) {
    std::string arg = argv[i];
    if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
      options->outputFile = argv[++i];
    } else if (arg == "--indent" && i + 1 < argc) {
      char* endPtr = nullptr;
      long val = strtol(argv[++i], &endPtr, 10);
      if (endPtr == argv[i] || *endPtr != '\0' || val < 0 || val > 16) {
        std::cerr << "pagx export-svg: invalid indent '" << argv[i] << "'\n";
        return 1;
      }
      options->indent = static_cast<int>(val);
    } else if (arg == "--no-xml-declaration") {
      options->noXmlDeclaration = true;
    } else if (arg == "--help" || arg == "-h") {
      PrintExportSVGUsage();
      return -1;
    } else if (arg[0] == '-') {
      std::cerr << "pagx export-svg: unknown option '" << arg << "'\n";
      return 1;
    } else {
      options->inputFile = arg;
    }
    i++;
  }
  if (options->inputFile.empty()) {
    std::cerr << "pagx export-svg: missing input file\n";
    return 1;
  }
  if (options->outputFile.empty()) {
    auto dot = options->inputFile.rfind('.');
    auto base = dot != std::string::npos ? options->inputFile.substr(0, dot) : options->inputFile;
    options->outputFile = base + ".svg";
  }
  return 0;
}

int RunExportSVG(int argc, char* argv[]) {
  ExportSVGOptions options = {};
  auto parseResult = ParseExportSVGOptions(argc, argv, &options);
  if (parseResult != 0) {
    return parseResult == -1 ? 0 : parseResult;
  }

  auto document = PAGXImporter::FromFile(options.inputFile);
  if (document == nullptr) {
    std::cerr << "pagx export-svg: failed to load '" << options.inputFile << "'\n";
    return 1;
  }
  if (!document->errors.empty()) {
    for (auto& error : document->errors) {
      std::cerr << "pagx export-svg: warning: " << error << "\n";
    }
  }

  SVGExporter::Options exportOptions = {};
  exportOptions.indent = options.indent;
  exportOptions.xmlDeclaration = !options.noXmlDeclaration;

  if (!SVGExporter::ToFile(*document, options.outputFile, exportOptions)) {
    std::cerr << "pagx export-svg: failed to write '" << options.outputFile << "'\n";
    return 1;
  }

  std::cout << "pagx export-svg: wrote " << options.outputFile << "\n";
  return 0;
}

}  // namespace pagx::cli
