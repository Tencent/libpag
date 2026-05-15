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
#include <sstream>
#include <string>
#include "cli/CliUtils.h"
#include "pagx/HTMLImporter.h"
#include "pagx/PAGXExporter.h"
#include "pagx/PAGXOptimizer.h"
#include "pagx/SVGImporter.h"

namespace pagx::cli {

//--------------------------------------------------------------------------------------------------
// Format-specific option parsing
//--------------------------------------------------------------------------------------------------

void ParseFormatOptions(int argc, char* argv[], ImportFormatOptions* options) {
  for (int i = 0; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "--svg-no-expand-use") {
      options->svgExpandUse = false;
    } else if (arg == "--svg-flatten-transforms") {
      options->svgFlattenTransforms = true;
    } else if (arg == "--svg-preserve-unknown") {
      options->svgPreserveUnknown = true;
    } else if (arg == "--html-strict") {
      options->htmlStrict = true;
    } else if (arg == "--html-preserve-unknown") {
      options->htmlPreserveUnknown = true;
    } else if (arg == "--html-no-prefer-body-size") {
      options->htmlPreferBodySize = false;
    } else if (arg == "--html-no-normalize") {
      options->htmlAutoNormalize = false;
    } else if (arg == "--html-infer-flex") {
      options->htmlInferFlex = true;
    }
  }
}

//--------------------------------------------------------------------------------------------------
// Format inference
//--------------------------------------------------------------------------------------------------

static std::string InferFormatFromContent(const std::string& content) {
  auto pos = content.find('<');
  while (pos != std::string::npos) {
    if (pos + 1 < content.size() && content[pos + 1] != '/' && content[pos + 1] != '!' &&
        content[pos + 1] != '?') {
      auto tagEnd = content.find_first_of(" \t\n/>", pos + 1);
      if (tagEnd != std::string::npos) {
        auto tagName = content.substr(pos + 1, tagEnd - pos - 1);
        for (auto& ch : tagName) {
          ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        if (tagName == "html" || tagName == "body") {
          return "html";
        }
        return tagName;
      }
    }
    pos = content.find('<', pos + 1);
  }
  return {};
}

static std::string NormalizeFormat(const std::string& format, const std::string& fallback) {
  std::string f = format.empty() ? fallback : format;
  for (auto& ch : f) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  if (f == "htm" || f == "xhtml") f = "html";
  return f;
}

//--------------------------------------------------------------------------------------------------
// Import functions
//--------------------------------------------------------------------------------------------------

static SVGImporter::Options ToSVGOptions(const ImportFormatOptions& formatOptions,
                                         float targetWidth = NAN, float targetHeight = NAN) {
  SVGImporter::Options svgOptions = {};
  svgOptions.expandUseReferences = formatOptions.svgExpandUse;
  svgOptions.flattenTransforms = formatOptions.svgFlattenTransforms;
  svgOptions.preserveUnknownElements = formatOptions.svgPreserveUnknown;
  svgOptions.targetWidth = targetWidth;
  svgOptions.targetHeight = targetHeight;
  return svgOptions;
}

static HTMLImporter::Options ToHTMLOptions(const ImportFormatOptions& formatOptions,
                                           float targetWidth = NAN, float targetHeight = NAN) {
  HTMLImporter::Options options = {};
  options.strict = formatOptions.htmlStrict;
  options.preserveUnknownElements = formatOptions.htmlPreserveUnknown;
  options.preferBodySize = formatOptions.htmlPreferBodySize;
  options.autoNormalize = formatOptions.htmlAutoNormalize;
  options.inferFlexFromAbsolute = formatOptions.htmlInferFlex;
  options.targetWidth = targetWidth;
  options.targetHeight = targetHeight;
  return options;
}

static void InlinePathData(PAGXDocument* doc) {
  for (auto& node : doc->nodes) {
    if (node->nodeType() == NodeType::PathData) {
      node->id.clear();
    }
  }
}

ImportResult ImportFile(const std::string& filePath, const std::string& format,
                        const ImportFormatOptions& formatOptions, float targetWidth,
                        float targetHeight) {
  ImportResult result = {};
  auto extension = GetFileExtension(filePath);
  std::string effectiveFormat = NormalizeFormat(format, extension);
  if (effectiveFormat == "svg") {
    result.document =
        SVGImporter::Parse(filePath, ToSVGOptions(formatOptions, targetWidth, targetHeight));
  } else if (effectiveFormat == "html") {
    result.document =
        HTMLImporter::Parse(filePath, ToHTMLOptions(formatOptions, targetWidth, targetHeight));
  } else {
    result.error = "unsupported format '" + effectiveFormat + "'";
    return result;
  }
  if (result.document == nullptr) {
    result.error = "failed to parse '" + filePath + "'";
    return result;
  }
  InlinePathData(result.document.get());
  result.warnings = std::move(result.document->errors);
  return result;
}

ImportResult ImportString(const std::string& content, const std::string& format,
                          const ImportFormatOptions& formatOptions, float targetWidth,
                          float targetHeight) {
  ImportResult result = {};
  std::string effectiveFormat = NormalizeFormat(format, InferFormatFromContent(content));
  if (effectiveFormat == "svg") {
    result.document =
        SVGImporter::ParseString(content, ToSVGOptions(formatOptions, targetWidth, targetHeight));
  } else if (effectiveFormat == "html") {
    result.document =
        HTMLImporter::ParseString(content, ToHTMLOptions(formatOptions, targetWidth, targetHeight));
  } else {
    result.error = "unsupported inline import format '" + effectiveFormat + "'";
    return result;
  }
  if (result.document == nullptr) {
    result.error = "failed to parse inline content";
    return result;
  }
  InlinePathData(result.document.get());
  result.warnings = std::move(result.document->errors);
  return result;
}

//--------------------------------------------------------------------------------------------------
// CLI entry point
//--------------------------------------------------------------------------------------------------

struct ImportOptions {
  std::string inputFile = {};
  std::string outputFile = {};
  std::string format = {};
  ImportFormatOptions formatOptions = {};
};

static void PrintUsage() {
  std::cout
      << "Usage: pagx import [options]\n"
      << "\n"
      << "Import a file from another format and convert it to PAGX.\n"
      << "\n"
      << "Options:\n"
      << "  --input <file>                 Input file to import (required)\n"
      << "  --output <file>                Output PAGX file (default: <input>.pagx)\n"
      << "  --format <format>              Force input format (svg, html)\n"
      << "\n"
      << "SVG options:\n"
      << "  --svg-no-expand-use            Do not expand <use> references\n"
      << "  --svg-flatten-transforms       Flatten nested transforms into single matrices\n"
      << "  --svg-preserve-unknown         Preserve unsupported SVG elements as Unknown nodes\n"
      << "\n"
      << "HTML options:\n"
      << "  --html-strict                  Treat HTML import warnings as errors\n"
      << "  --html-preserve-unknown        Keep unknown HTML tags as empty Layers\n"
      << "  --html-no-prefer-body-size     Prefer --target* over <body> intrinsic size\n"
      << "  --html-no-normalize            Skip the HTML subset normalizer (debug)\n"
      << "  --html-infer-flex              Recover display:flex from absolute layout (lossy)\n"
      << "\n"
      << "Examples:\n"
      << "  pagx import --input icon.svg                      # SVG to icon.pagx\n"
      << "  pagx import --input layout.html                   # HTML to layout.pagx\n"
      << "  pagx import --input page.html --output card.pagx  # HTML to card.pagx\n";
}

static int ParseOptions(int argc, char* argv[], ImportOptions* options) {
  int i = 1;
  while (i < argc) {
    std::string arg = argv[i];
    if (arg == "--input" && i + 1 < argc) {
      options->inputFile = argv[++i];
    } else if ((arg == "--output" || arg == "-o") && i + 1 < argc) {
      options->outputFile = argv[++i];
    } else if (arg == "--format" && i + 1 < argc) {
      options->format = argv[++i];
    } else if (arg == "--svg-no-expand-use" || arg == "--svg-flatten-transforms" ||
               arg == "--svg-preserve-unknown" || arg == "--html-strict" ||
               arg == "--html-preserve-unknown" || arg == "--html-no-prefer-body-size" ||
               arg == "--html-no-normalize" || arg == "--html-infer-flex") {
      // Handled by ParseFormatOptions below.
    } else if (arg == "--help" || arg == "-h") {
      PrintUsage();
      return -1;
    } else if (arg[0] == '-') {
      std::cerr << "pagx import: error: unknown option '" << arg << "'\n";
      return 1;
    } else {
      std::cerr << "pagx import: error: unexpected argument '" << arg << "'\n";
      return 1;
    }
    i++;
  }

  if (options->inputFile.empty()) {
    std::cerr << "pagx import: error: missing --input\n";
    return 1;
  }

  if (options->outputFile.empty()) {
    options->outputFile = ReplaceExtension(options->inputFile, "pagx");
  }

  ParseFormatOptions(argc, argv, &options->formatOptions);
  return 0;
}

int RunImport(int argc, char* argv[]) {
  ImportOptions options = {};
  auto parseResult = ParseOptions(argc, argv, &options);
  if (parseResult != 0) {
    return parseResult == -1 ? 0 : parseResult;
  }

  auto result = ImportFile(options.inputFile, options.format, options.formatOptions);
  if (!result.error.empty()) {
    std::cerr << "pagx import: error: " << result.error << "\n";
    return 1;
  }
  for (auto& warning : result.warnings) {
    std::cerr << "pagx import: warning: " << warning << "\n";
  }

  auto optimizeResult = PAGXOptimizer::Optimize(result.document.get());
  if (!optimizeResult.converged) {
    std::cerr << "pagx import: warning: PAGXOptimizer did not converge within "
              << optimizeResult.iterationsUsed << " iteration(s); output may be sub-optimal\n";
  }

  auto xml = PAGXExporter::ToXML(*result.document);
  if (!WriteStringToFile(xml, options.outputFile, "pagx import")) {
    return 1;
  }

  return 0;
}

}  // namespace pagx::cli
