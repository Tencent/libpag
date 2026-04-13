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
#include "cli/CliUtils.h"
#include "pagx/PAGXImporter.h"
#include "pagx/PPTExporter.h"
#include "pagx/SVGExporter.h"

namespace pagx::cli {

struct ExportOptions {
  std::string inputFile = {};
  std::string outputFile = {};
  std::string format = {};
  int svgIndent = 2;
  bool svgNoXmlDeclaration = false;
  bool textToPath = false;
  bool pptNoBakeMask = false;
  bool pptNoBakeTiledPattern = false;
  bool pptNoBridgeContours = false;
};

static void PrintUsage() {
  std::cout
      << "Usage: pagx export [options]\n"
      << "\n"
      << "Export a PAGX file to another format.\n"
      << "\n"
      << "Options:\n"
      << "  --input <file>              Input PAGX file (required)\n"
      << "  --output <file>             Output file (default: <input>.<format>)\n"
      << "  --format <format>           Output format (svg, pptx; inferred from --output "
         "extension)\n"
      << "  --text-to-path              Convert text to path geometry (default: native text)\n"
      << "\n"
      << "SVG options:\n"
      << "  --svg-indent <n>            Indentation spaces (default: 2, valid range: 0-16)\n"
      << "  --svg-no-xml-declaration    Omit the <?xml ...?> declaration\n"
      << "\n"
      << "PPT options:\n"
      << "  --ppt-no-bake-mask          Export masked layers as vector shapes instead of\n"
      << "                              rasterizing to bitmap\n"
      << "  --ppt-no-bake-tiled-pattern Use native OOXML a:tile instead of rasterizing\n"
      << "                              tiled image patterns to bitmap\n"
      << "  --ppt-no-bridge-contours    Emit each contour as a separate sub-path instead\n"
      << "                              of bridging nested contours (default: bridge on)\n"
      << "\n"
      << "Examples:\n"
      << "  pagx export --input icon.pagx                    # PAGX to icon.svg\n"
      << "  pagx export --input icon.pagx --output out.svg   # PAGX to out.svg\n"
      << "  pagx export --input icon.pagx --output out.pptx  # PAGX to out.pptx\n"
      << "  pagx export --format svg --input icon.pagx       # force SVG output format\n"
      << "  pagx export --format pptx --input icon.pagx      # force PPTX output format\n"
      << "  pagx export --input icon.pagx --svg-indent 4     # 4-space indent\n"
      << "  pagx export --input icon.pagx --text-to-path     # convert text to paths\n";
}

static int ParseOptions(int argc, char* argv[], ExportOptions* options) {
  int i = 1;
  while (i < argc) {
    std::string arg = argv[i];
    if (arg == "--input" && i + 1 < argc) {
      options->inputFile = argv[++i];
    } else if (arg == "--output" && i + 1 < argc) {
      options->outputFile = argv[++i];
    } else if (arg == "--format" && i + 1 < argc) {
      options->format = argv[++i];
    } else if (arg == "--svg-indent" && i + 1 < argc) {
      char* endPtr = nullptr;
      long value = strtol(argv[++i], &endPtr, 10);
      if (endPtr == argv[i] || *endPtr != '\0' || value < 0 || value > 16) {
        std::cerr << "pagx export: error: invalid indent '" << argv[i] << "' (must be 0-16)\n";
        return 1;
      }
      options->svgIndent = static_cast<int>(value);
    } else if (arg == "--svg-no-xml-declaration") {
      options->svgNoXmlDeclaration = true;
    } else if (arg == "--text-to-path") {
      options->textToPath = true;
    } else if (arg == "--ppt-no-bake-mask") {
      options->pptNoBakeMask = true;
    } else if (arg == "--ppt-no-bake-tiled-pattern") {
      options->pptNoBakeTiledPattern = true;
    } else if (arg == "--ppt-no-bridge-contours") {
      options->pptNoBridgeContours = true;
    } else if (arg == "--help" || arg == "-h") {
      PrintUsage();
      return -1;
    } else if (arg[0] == '-') {
      std::cerr << "pagx export: error: unknown option '" << arg << "'\n";
      return 1;
    } else {
      std::cerr << "pagx export: error: unexpected argument '" << arg
                << "', use --input to specify the input file\n";
      return 1;
    }
    i++;
  }

  if (options->inputFile.empty()) {
    std::cerr << "pagx export: error: missing --input file\n";
    return 1;
  }

  if (options->format.empty() && !options->outputFile.empty()) {
    options->format = GetFileExtension(options->outputFile);
  }
  if (options->format.empty()) {
    std::cerr << "pagx export: error: cannot infer output format, use --format or specify an "
                 "output file with a known extension\n";
    return 1;
  }

  if (options->outputFile.empty()) {
    options->outputFile = ReplaceExtension(options->inputFile, options->format);
  }

  return 0;
}

static int ExportToSVG(const ExportOptions& options) {
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
  if (document->hasUnresolvedImports()) {
    std::cerr << "pagx export: error: unresolved import directive, run 'pagx resolve' first\n";
    return 1;
  }

  SVGExporter::Options svgOptions = {};
  svgOptions.indent = options.svgIndent;
  svgOptions.xmlDeclaration = !options.svgNoXmlDeclaration;
  svgOptions.convertTextToPath = options.textToPath;

  if (!SVGExporter::ToFile(*document, options.outputFile, svgOptions)) {
    std::cerr << "pagx export: error: failed to write '" << options.outputFile << "'\n";
    return 1;
  }

  std::cout << "pagx export: wrote " << options.outputFile << "\n";
  return 0;
}

static int ExportToPPT(const ExportOptions& options) {
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

  PPTExporter::Options pptOptions = {};
  pptOptions.convertTextToPath = options.textToPath;
  pptOptions.bakeMask = !options.pptNoBakeMask;
  pptOptions.bakeTiledPattern = !options.pptNoBakeTiledPattern;
  pptOptions.bridgeContours = !options.pptNoBridgeContours;

  if (!PPTExporter::ToFile(*document, options.outputFile, pptOptions)) {
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
    return ExportToSVG(options);
  }
  if (options.format == "pptx") {
    return ExportToPPT(options);
  }

  std::cerr << "pagx export: error: unsupported format '" << options.format << "'\n";
  return 1;
}

}  // namespace pagx::cli
