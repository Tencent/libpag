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

#include "cli/CommandConvert.h"
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include "pagx/PAGXExporter.h"
#include "pagx/PAGXImporter.h"
#include "pagx/SVGExporter.h"
#include "pagx/SVGImporter.h"

namespace pagx::cli {

struct SVGExportOptions {
  int indent = 2;
  bool noXmlDeclaration = false;
  bool noConvertTextToPath = false;
};

struct SVGImportOptions {
  bool expandUse = true;
  bool flattenTransforms = false;
  bool preserveUnknown = false;
};

struct ConvertOptions {
  std::string inputFile = {};
  std::string outputFile = {};
  std::string outputFormat = {};
  SVGExportOptions svgExport = {};
  SVGImportOptions svgImport = {};
};

static void PrintUsage() {
  std::cout
      << "Usage: pagx convert [options] <input> <output>\n"
      << "\n"
      << "Convert between PAGX and other formats. The conversion direction\n"
      << "is inferred from file extensions (e.g. .pagx -> .svg or .svg -> .pagx).\n"
      << "\n"
      << "Options:\n"
      << "  -f <format>               Override output format (svg, pagx)\n"
      << "\n"
      << "SVG output options:\n"
      << "  --indent <n>              Indentation spaces (default: 2, valid range: 0-16)\n"
      << "  --no-xml-declaration      Omit the <?xml ...?> declaration\n"
      << "  --no-convert-text-to-path Keep text as <text> elements instead of <path>\n"
      << "\n"
      << "SVG input options:\n"
      << "  --no-expand-use           Do not expand <use> references\n"
      << "  --flatten-transforms      Flatten nested transforms into single matrices\n"
      << "  --preserve-unknown        Preserve unsupported SVG elements as Unknown nodes\n"
      << "\n"
      << "Examples:\n"
      << "  pagx convert input.pagx output.svg          # PAGX to SVG\n"
      << "  pagx convert input.svg output.pagx           # SVG to PAGX\n"
      << "  pagx convert --indent 4 input.pagx out.svg   # PAGX to SVG with 4-space indent\n"
      << "  pagx convert -f svg input.pagx output        # force SVG output format\n";
}

static std::string InferFormat(const std::string& path) {
  auto dot = path.rfind('.');
  if (dot != std::string::npos) {
    auto ext = path.substr(dot + 1);
    if (ext == "svg") {
      return "svg";
    }
    if (ext == "pagx") {
      return "pagx";
    }
  }
  return {};
}

static int ParseOptions(int argc, char* argv[], ConvertOptions* options) {
  std::vector<std::string> positional = {};
  int i = 1;
  while (i < argc) {
    std::string arg = argv[i];
    if (arg == "-f" && i + 1 < argc) {
      options->outputFormat = argv[++i];
    } else if (arg == "--indent" && i + 1 < argc) {
      char* endPtr = nullptr;
      long value = strtol(argv[++i], &endPtr, 10);
      if (endPtr == argv[i] || *endPtr != '\0' || value < 0 || value > 16) {
        std::cerr << "pagx convert: error: invalid indent '" << argv[i]
                  << "' (must be 0-16)\n";
        return 1;
      }
      options->svgExport.indent = static_cast<int>(value);
    } else if (arg == "--no-xml-declaration") {
      options->svgExport.noXmlDeclaration = true;
    } else if (arg == "--no-convert-text-to-path") {
      options->svgExport.noConvertTextToPath = true;
    } else if (arg == "--no-expand-use") {
      options->svgImport.expandUse = false;
    } else if (arg == "--flatten-transforms") {
      options->svgImport.flattenTransforms = true;
    } else if (arg == "--preserve-unknown") {
      options->svgImport.preserveUnknown = true;
    } else if (arg == "--help" || arg == "-h") {
      PrintUsage();
      return -1;
    } else if (arg[0] == '-') {
      std::cerr << "pagx convert: error: unknown option '" << arg << "'\n";
      return 1;
    } else {
      positional.push_back(arg);
    }
    i++;
  }

  if (positional.empty()) {
    std::cerr << "pagx convert: error: missing input and output files\n";
    return 1;
  }
  if (positional.size() < 2) {
    std::cerr << "pagx convert: error: missing output file\n";
    return 1;
  }
  if (positional.size() > 2) {
    std::cerr << "pagx convert: error: too many positional arguments\n";
    return 1;
  }

  options->inputFile = positional[0];
  options->outputFile = positional[1];

  if (options->outputFormat.empty()) {
    options->outputFormat = InferFormat(options->outputFile);
  }
  if (options->outputFormat.empty()) {
    std::cerr << "pagx convert: error: cannot infer output format from '"
              << options->outputFile << "', use -f to specify\n";
    return 1;
  }
  return 0;
}

static int ConvertToSVG(const ConvertOptions& options) {
  auto document = PAGXImporter::FromFile(options.inputFile);
  if (document == nullptr) {
    std::cerr << "pagx convert: error: failed to load '" << options.inputFile << "'\n";
    return 1;
  }
  if (!document->errors.empty()) {
    for (auto& error : document->errors) {
      std::cerr << "pagx convert: warning: " << error << "\n";
    }
  }

  SVGExporter::Options svgOptions = {};
  svgOptions.indent = options.svgExport.indent;
  svgOptions.xmlDeclaration = !options.svgExport.noXmlDeclaration;
  svgOptions.convertTextToPath = !options.svgExport.noConvertTextToPath;

  if (!SVGExporter::ToFile(*document, options.outputFile, svgOptions)) {
    std::cerr << "pagx convert: error: failed to write '" << options.outputFile << "'\n";
    return 1;
  }

  std::cout << "pagx convert: wrote " << options.outputFile << "\n";
  return 0;
}

static int ConvertToPAGX(const ConvertOptions& options) {
  auto inputFormat = InferFormat(options.inputFile);
  if (inputFormat != "svg") {
    std::cerr << "pagx convert: error: unsupported input format for '"
              << options.inputFile << "'\n";
    return 1;
  }

  SVGImporter::Options svgOptions = {};
  svgOptions.expandUseReferences = options.svgImport.expandUse;
  svgOptions.flattenTransforms = options.svgImport.flattenTransforms;
  svgOptions.preserveUnknownElements = options.svgImport.preserveUnknown;

  auto document = SVGImporter::Parse(options.inputFile, svgOptions);
  if (document == nullptr) {
    std::cerr << "pagx convert: error: failed to parse '" << options.inputFile << "'\n";
    return 1;
  }
  if (!document->errors.empty()) {
    for (auto& error : document->errors) {
      std::cerr << "pagx convert: warning: " << error << "\n";
    }
  }

  auto xml = PAGXExporter::ToXML(*document);
  std::ofstream out(options.outputFile);
  if (!out.is_open()) {
    std::cerr << "pagx convert: error: failed to write '" << options.outputFile << "'\n";
    return 1;
  }
  out << xml;

  std::cout << "pagx convert: wrote " << options.outputFile << "\n";
  return 0;
}

int RunConvert(int argc, char* argv[]) {
  ConvertOptions options = {};
  auto parseResult = ParseOptions(argc, argv, &options);
  if (parseResult != 0) {
    return parseResult == -1 ? 0 : parseResult;
  }

  if (options.outputFormat == "svg") {
    return ConvertToSVG(options);
  }
  if (options.outputFormat == "pagx") {
    return ConvertToPAGX(options);
  }

  std::cerr << "pagx convert: error: unsupported output format '" << options.outputFormat << "'\n";
  return 1;
}

}  // namespace pagx::cli
