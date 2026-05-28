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
#include "pagx/Diagnostic.h"
#include "pagx/HTMLExporter.h"
#include "pagx/PAGExporter.h"
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
  // When true, PAG export promotes all Baker+Codec warnings to errors (§14.2).
  // The CLI then returns a non-zero exit code. Maps 1:1 onto
  // PAGExporter::Options::strict.
  bool pagStrict = false;
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
      << "  --format <format>           Output format (svg, pptx, html, pag; inferred from "
         "--output extension)\n"
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
      << "PAG options:\n"
      << "  --pag-strict                Treat Baker/Codec warnings as errors (non-zero exit "
         "when any warning is produced)\n"
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
      << "  pagx export --input icon.pagx --output icon.html # PAGX to HTML\n"
      << "  pagx export --input icon.pagx --format pag       # PAGX to icon.pag (binary)\n"
      << "  pagx export --input icon.pagx --format pag --pag-strict\n"
      << "                                                   # fail on any Baker/Codec "
         "warning\n";
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
    } else if (arg == "--pag-strict") {
      options->pagStrict = true;
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

static int ExportToHTML(const ExportOptions& options) {
  auto document = PAGXImporter::FromFile(options.inputFile);
  if (document == nullptr) {
    std::cerr << "pagx export: error: failed to load '" << options.inputFile << "'\n";
    return 1;
  }
  for (auto& error : document->errors) {
    std::cerr << "pagx export: warning: " << error << "\n";
  }
  if (document->hasUnresolvedImports()) {
    std::cerr << "pagx export: error: unresolved import directive, run 'pagx resolve' first\n";
    return 1;
  }

  std::string errorMsg;
  if (!HTMLExporter::ToFile(*document, options.outputFile, {}, &errorMsg)) {
    std::cerr << "pagx export: error: " << (errorMsg.empty() ? "export failed" : errorMsg) << "\n";
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

// Exports a PAGX file to PAG v2 binary format via PAGExporter::ToFile.
// Honours --pag-strict by mapping it onto PAGExporter::Options::strict —
// when true, Baker/Codec Warning-severity diagnostics are promoted to
// errors inside PAGExporter, flipping Result.ok=false and producing a
// non-zero exit code here without us having to inspect the warnings
// vector separately (§14.2 contract).
static int ExportToPAG(const ExportOptions& options) {
  auto document = LoadDocument(options.inputFile, "pagx export");
  if (document == nullptr) {
    return 1;
  }
  if (document->hasUnresolvedImports()) {
    std::cerr << "pagx export: error: unresolved import directive, run 'pagx resolve' first\n";
    return 1;
  }
  document->applyLayout();

  PAGExporter::Options pagOptions;
  pagOptions.strict = options.pagStrict;
  auto result = PAGExporter::ToFile(*document, options.outputFile, pagOptions);

  // Emit diagnostics regardless of success so users always see what
  // happened. Format: "<severity>: [<CodeName>] <message> @0x<offset>".
  for (const auto& d : result.errors) {
    std::cerr << "pagx export: error: " << FormatDiagnostic(d) << "\n";
  }
  for (const auto& d : result.warnings) {
    std::cerr << "pagx export: warning: " << FormatDiagnostic(d) << "\n";
  }

  if (!result.ok) {
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

  if (options.format == "pag") {
    return ExportToPAG(options);
  }

  std::cerr << "pagx export: error: unsupported format '" << options.format << "'\n";
  return 1;
}

}  // namespace pagx::cli
