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
#include <iostream>
#include <string>
#include "cli/CliUtils.h"
#include "pagx/PAGXExporter.h"
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
        return tagName;
      }
    }
    pos = content.find('<', pos + 1);
  }
  return {};
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
  auto effectiveFormat = format.empty() ? GetFileExtension(filePath) : format;
  if (effectiveFormat != "svg") {
    result.error = "unsupported format '" + effectiveFormat + "'";
    return result;
  }
  result.document =
      SVGImporter::Parse(filePath, ToSVGOptions(formatOptions, targetWidth, targetHeight));
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
  auto effectiveFormat = format.empty() ? InferFormatFromContent(content) : format;
  if (effectiveFormat != "svg") {
    result.error = "unsupported inline import format '" + effectiveFormat + "'";
    return result;
  }
  result.document =
      SVGImporter::ParseString(content, ToSVGOptions(formatOptions, targetWidth, targetHeight));
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
            << "  pagx import --input icon.svg --output out.pagx   # SVG to out.pagx\n";
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
               arg == "--svg-preserve-unknown") {
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

  auto xml = PAGXExporter::ToXML(*result.document);
  if (!WriteStringToFile(xml, options.outputFile, "pagx import")) {
    return 1;
  }

  return 0;
}

}  // namespace pagx::cli
