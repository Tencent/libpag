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

#include "cli/CommandImport.h"
#include <fstream>
#include <iostream>
#include <string>
#include "cli/CliUtils.h"
#include "pagx/PAGXExporter.h"
#include "pagx/SVGImporter.h"

namespace pagx::cli {

struct ImportOptions {
  std::string inputFile = {};
  std::string outputFile = {};
  std::string format = {};
  bool svgExpandUse = true;
  bool svgFlattenTransforms = false;
  bool svgPreserveUnknown = false;
};

static void PrintUsage() {
  std::cout << "Usage: pagx import [options]\n"
            << "\n"
            << "Import a file from another format and convert it to PAGX.\n"
            << "\n"
            << "Options:\n"
            << "  --input <file>              Input file to import (required)\n"
            << "  --output <file>             Output PAGX file (default: <input>.pagx)\n"
            << "  --format <format>           Force input format (svg)\n"
            << "\n"
            << "SVG options:\n"
            << "  --svg-no-expand-use         Do not expand <use> references\n"
            << "  --svg-flatten-transforms    Flatten nested transforms into single matrices\n"
            << "  --svg-preserve-unknown      Preserve unsupported SVG elements as Unknown nodes\n"
            << "\n"
            << "Examples:\n"
            << "  pagx import --input icon.svg                     # SVG to icon.pagx\n"
            << "  pagx import --input icon.svg --output out.pagx   # SVG to out.pagx\n"
            << "  pagx import --format svg --input drawing.xml     # force SVG format\n";
}

static int ParseOptions(int argc, char* argv[], ImportOptions* options) {
  int i = 1;
  while (i < argc) {
    std::string arg = argv[i];
    if (arg == "--input" && i + 1 < argc) {
      options->inputFile = argv[++i];
    } else if (arg == "--output" && i + 1 < argc) {
      options->outputFile = argv[++i];
    } else if (arg == "--format" && i + 1 < argc) {
      options->format = argv[++i];
    } else if (arg == "--svg-no-expand-use") {
      options->svgExpandUse = false;
    } else if (arg == "--svg-flatten-transforms") {
      options->svgFlattenTransforms = true;
    } else if (arg == "--svg-preserve-unknown") {
      options->svgPreserveUnknown = true;
    } else if (arg == "--help" || arg == "-h") {
      PrintUsage();
      return -1;
    } else if (arg[0] == '-') {
      std::cerr << "pagx import: error: unknown option '" << arg << "'\n";
      return 1;
    } else {
      std::cerr << "pagx import: error: unexpected argument '" << arg
                << "', use --input to specify the input file\n";
      return 1;
    }
    i++;
  }

  if (options->inputFile.empty()) {
    std::cerr << "pagx import: error: missing --input file\n";
    return 1;
  }

  if (options->format.empty()) {
    options->format = GetFileExtension(options->inputFile);
  }

  if (options->outputFile.empty()) {
    options->outputFile = ReplaceExtension(options->inputFile, "pagx");
  }

  return 0;
}

static int ImportFromSVG(const ImportOptions& options) {
  SVGImporter::Options svgOptions = {};
  svgOptions.expandUseReferences = options.svgExpandUse;
  svgOptions.flattenTransforms = options.svgFlattenTransforms;
  svgOptions.preserveUnknownElements = options.svgPreserveUnknown;

  auto document = SVGImporter::Parse(options.inputFile, svgOptions);
  if (document == nullptr) {
    std::cerr << "pagx import: error: failed to parse '" << options.inputFile << "'\n";
    return 1;
  }
  if (!document->errors.empty()) {
    for (auto& error : document->errors) {
      std::cerr << "pagx import: warning: " << error << "\n";
    }
  }

  auto xml = PAGXExporter::ToXML(*document);
  std::ofstream out(options.outputFile);
  if (!out.is_open()) {
    std::cerr << "pagx import: error: failed to write '" << options.outputFile << "'\n";
    return 1;
  }
  out << xml;

  std::cout << "pagx import: wrote " << options.outputFile << "\n";
  return 0;
}

int RunImport(int argc, char* argv[]) {
  ImportOptions options = {};
  auto parseResult = ParseOptions(argc, argv, &options);
  if (parseResult != 0) {
    return parseResult == -1 ? 0 : parseResult;
  }

  if (options.format == "svg") {
    return ImportFromSVG(options);
  }

  std::cerr << "pagx import: error: unsupported format '" << options.format << "'\n";
  return 1;
}

}  // namespace pagx::cli
