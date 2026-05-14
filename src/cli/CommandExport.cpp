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
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include "cli/CliUtils.h"
#include "pagx/HTMLExporter.h"
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
  bool pptBakeUnsupported = true;
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
      << "  --format <format>           Output format (svg, pptx, html; inferred from --output "
         "extension)\n"
      << "  --text-to-path              Convert text to path geometry (default: native text)\n"
      << "\n"
      << "SVG options:\n"
      << "  --svg-indent <n>            Indentation spaces (default: 2, valid range: 0-16)\n"
      << "  --svg-no-xml-declaration    Omit the <?xml ...?> declaration\n"
      << "\n"
      << "PPT options:\n"
      << "  --ppt-no-bake-unsupported\n"
      << "                              Do not bake layers that use features OOXML cannot\n"
      << "                              represent natively (masks, scrollRect clipping, blend\n"
      << "                              modes outside of Normal/Multiply/Screen/Darken/Lighten,\n"
      << "                              wide-gamut color, and BackgroundBlurStyle) into PNG\n"
      << "                              patches. By default these layers are baked so the slide\n"
      << "                              matches the tgfx renderer; for unsupported blend modes\n"
      << "                              and BackgroundBlurStyle the backdrop beneath the layer\n"
      << "                              is baked too so the composite (or frosted-glass blur)\n"
      << "                              renders correctly, at the cost of turning native content\n"
      << "                              under the patch into pixels. Pass this flag to silently\n"
      << "                              drop those features and emit the layer as editable\n"
      << "                              shapes instead (mask ignored, scrollRect dropped, blend\n"
      << "                              falls back to Normal, wide-gamut clamped to sRGB).\n"
      << "\n"
      << "Examples:\n"
      << "  pagx export --input icon.pagx                    # PAGX to icon.svg\n"
      << "  pagx export --input icon.pagx --output out.svg   # PAGX to out.svg\n"
      << "  pagx export --input icon.pagx --output out.pptx  # PAGX to out.pptx\n"
      << "  pagx export --format svg --input icon.pagx       # force SVG output format\n"
      << "  pagx export --format pptx --input icon.pagx      # force PPTX output format\n"
      << "  pagx export --input icon.pagx --svg-indent 4     # 4-space indent\n"
      << "  pagx export --input icon.pagx --text-to-path     # convert text to paths\n"
      << "  pagx export --input icon.pagx --output out.pptx --ppt-no-bake-unsupported\n"
      << "                                                   # keep unsupported features "
         "editable\n"
      << "  pagx export --input icon.pagx --output icon.html # PAGX to HTML\n";
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
    } else if (arg == "--ppt-no-bake-unsupported") {
      options->pptBakeUnsupported = false;
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
  auto document = LoadDocument(options.inputFile, "pagx export");
  if (document == nullptr) {
    return 1;
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

// Wraps the HTML fragment returned by HTMLExporter::ToHTML in a complete <!DOCTYPE html>
// document so that the output file can be opened directly in a browser. The body is sized to
// the PAGX canvas.
static std::string WrapAsHTMLDocument(const std::string& fragment, float width, float height) {
  auto w = std::to_string(static_cast<int>(width));
  auto h = std::to_string(static_cast<int>(height));
  std::string bodyStyle = "margin:0;padding:0;width:" + w + "px;height:" + h +
                          "px;overflow:hidden;font-family:sans-serif";
  return "<!DOCTYPE html>\n<html>\n<head>\n<meta charset=\"utf-8\">\n<style>\nbody { " + bodyStyle +
         " }\n</style>\n</head>\n<body>\n" + fragment + "\n</body>\n</html>\n";
}

static int ExportToHTML(const ExportOptions& options) {
  auto document = PAGXImporter::FromFile(options.inputFile);
  if (document == nullptr) {
    std::cerr << "pagx export: error: failed to load '" << options.inputFile << "'\n";
    return 1;
  }
  for (auto& error : document->errors) {
    std::cerr << "pagx export: warning: " << error << "\n";
  }

  document->applyLayout();

  std::filesystem::path outputPath(options.outputFile);
  auto outputDir = outputPath.parent_path();
  if (outputDir.empty()) {
    outputDir = std::filesystem::current_path();
  }

  // Resource directory convention: sibling directory named after the HTML file's stem, so the
  // generated HTML references assets via "<stem>/…" relative URLs. This matches the new
  // HTMLExporter contract enforced by ToFile, but we call ToHTML explicitly because CLI needs
  // to wrap the fragment in a complete <!DOCTYPE html> document before writing it.
  auto outputStem = outputPath.stem().string();
  auto resourceDir = (outputDir / outputStem).string();
  auto fragment = HTMLExporter::ToHTML(*document, resourceDir);
  if (fragment.empty()) {
    std::cerr << "pagx export: error: HTMLExporter produced empty output for '" << options.inputFile
              << "'\n";
    return 1;
  }

  auto html = WrapAsHTMLDocument(fragment, document->width, document->height);

  if (!outputDir.empty() && !std::filesystem::exists(outputDir)) {
    std::error_code ec;
    std::filesystem::create_directories(outputDir, ec);
    if (ec) {
      std::cerr << "pagx export: error: failed to create directory '" << outputDir.string()
                << "': " << ec.message() << "\n";
      return 1;
    }
  }
  std::ofstream file(options.outputFile, std::ios::binary);
  if (!file) {
    std::cerr << "pagx export: error: failed to write '" << options.outputFile << "'\n";
    return 1;
  }
  file.write(html.data(), static_cast<std::streamsize>(html.size()));
  file.close();

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
  if (document->hasUnresolvedImports()) {
    std::cerr << "pagx export: error: unresolved import directive, run 'pagx resolve' first\n";
    return 1;
  }

  PPTExporter::Options pptOptions = {};
  pptOptions.convertTextToPath = options.textToPath;
  pptOptions.bakeUnsupported = options.pptBakeUnsupported;

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

  if (options.format == "html") {
    return ExportToHTML(options);
  }

  std::cerr << "pagx export: error: unsupported format '" << options.format << "'\n";
  return 1;
}

}  // namespace pagx::cli
