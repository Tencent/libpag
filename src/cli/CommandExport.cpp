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

#include "cli/CommandExport.h"
#include <iostream>
#include <string>
#include "pagx/PAGXImporter.h"
#include "pagx/SVGExporter.h"

namespace pagx::cli {

struct ExportOptions {
  std::string inputFile = {};
  std::string outputFile = {};
  std::string format = {};
  int indent = 2;
  bool noXmlDeclaration = false;
};

static void PrintUsage() {
  std::cout << "Usage: pagx export [options] <file.pagx>\n"
            << "\n"
            << "Options:\n"
            << "  -o, --output <path>       Output file path (default: input path with format "
               "extension)\n"
            << "  --format <type>           Export format (default: inferred from output "
               "extension, fallback: svg)\n"
            << "\n"
            << "SVG options:\n"
            << "  --indent <n>              Indentation spaces (default: 2, valid range: 0-16)\n"
            << "  --no-xml-declaration      Omit the <?xml ...?> declaration\n";
}

static std::string InferFormat(const std::string& outputFile) {
  auto dot = outputFile.rfind('.');
  if (dot != std::string::npos) {
    auto ext = outputFile.substr(dot + 1);
    if (ext == "svg") {
      return "svg";
    }
  }
  return {};
}

static int ParseOptions(int argc, char* argv[], ExportOptions* options) {
  int i = 1;
  while (i < argc) {
    std::string argument = argv[i];
    if ((argument == "-o" || argument == "--output") && i + 1 < argc) {
      options->outputFile = argv[++i];
    } else if (argument == "--format" && i + 1 < argc) {
      options->format = argv[++i];
    } else if (argument == "--indent" && i + 1 < argc) {
      char* endPtr = nullptr;
      long value = strtol(argv[++i], &endPtr, 10);
      if (endPtr == argv[i] || *endPtr != '\0' || value < 0 || value > 16) {
        std::cerr << "pagx export: error: invalid indent '" << argv[i]
                  << "' (must be 0-16)\n";
        return 1;
      }
      options->indent = static_cast<int>(value);
    } else if (argument == "--no-xml-declaration") {
      options->noXmlDeclaration = true;
    } else if (argument == "--help" || argument == "-h") {
      PrintUsage();
      return -1;
    } else if (argument[0] == '-') {
      std::cerr << "pagx export: error: unknown option '" << argument << "'\n";
      return 1;
    } else {
      options->inputFile = argument;
    }
    i++;
  }
  if (options->inputFile.empty()) {
    std::cerr << "pagx export: error: missing input file\n";
    return 1;
  }

  // Infer format from output extension, then fallback to "svg".
  if (options->format.empty() && !options->outputFile.empty()) {
    options->format = InferFormat(options->outputFile);
  }
  if (options->format.empty()) {
    options->format = "svg";
  }

  // Generate default output path if not specified.
  if (options->outputFile.empty()) {
    auto dot = options->inputFile.rfind('.');
    auto base = dot != std::string::npos ? options->inputFile.substr(0, dot) : options->inputFile;
    if (options->format == "svg") {
      options->outputFile = base + ".svg";
    } else {
      options->outputFile = base + "." + options->format;
    }
  }
  return 0;
}

static int ExportSVG(const ExportOptions& options) {
  auto document = PAGXImporter::FromFile(options.inputFile);
  if (document == nullptr) {
    std::cerr << "pagx export: error: failed to load '" << options.inputFile << "'\n";
    return 1;
  }
  if (!document->errors.empty()) {
    for (auto& error : document->errors) {
      std::cerr << "pagx export: warning: " << error << "\n";
    }
  }

  SVGExporter::Options svgOptions = {};
  svgOptions.indent = options.indent;
  svgOptions.xmlDeclaration = !options.noXmlDeclaration;

  if (!SVGExporter::ToFile(*document, options.outputFile, svgOptions)) {
    std::cerr << "pagx export: error: failed to write '" << options.outputFile << "'\n";
    return 1;
  }

  std::cout << "pagx export: wrote " << options.outputFile << "\n";
  return 0;
}

int RunExport(int argc, char* argv[]) {
  ExportOptions options = {};
  auto parseResult = ParseOptions(argc, argv, &options);
  if (parseResult != 0) {
    return parseResult == -1 ? 0 : parseResult;
  }

  if (options.format == "svg") {
    return ExportSVG(options);
  }

  std::cerr << "pagx export: error: unsupported format '" << options.format << "'\n";
  return 1;
}

}  // namespace pagx::cli
