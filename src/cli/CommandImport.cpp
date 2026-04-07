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
#include <cmath>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include "cli/CliUtils.h"
#include "pagx/PAGXExporter.h"
#include "pagx/PAGXImporter.h"
#include "pagx/SVGImporter.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Path.h"

namespace pagx::cli {

struct ImportOptions {
  std::string inputFile = {};
  std::string outputFile = {};
  std::string format = {};
  bool svgExpandUse = true;
  bool svgFlattenTransforms = false;
  bool svgPreserveUnknown = false;
  // Resolve mode: import content into an existing PAGX Layer.
  std::string resolveId = {};
  std::string pagxFile = {};
};

static void PrintUsage() {
  std::cout
      << "Usage: pagx import [options]\n"
      << "\n"
      << "Import a file from another format and convert it to PAGX.\n"
      << "\n"
      << "Options:\n"
      << "  --input <file>              Input file to import (required)\n"
      << "  --output <file>             Output PAGX file (default: <input>.pagx)\n"
      << "  --format <format>           Force input format (svg)\n"
      << "  --resolve <id>              Replace the content of a Layer in an existing PAGX\n"
      << "                              file with the imported content. Requires a PAGX file\n"
      << "                              as a positional argument.\n"
      << "\n"
      << "SVG options:\n"
      << "  --svg-no-expand-use         Do not expand <use> references\n"
      << "  --svg-flatten-transforms    Flatten nested transforms into single matrices\n"
      << "  --svg-preserve-unknown      Preserve unsupported SVG elements as Unknown nodes\n"
      << "\n"
      << "Examples:\n"
      << "  pagx import --input icon.svg                     # SVG to icon.pagx\n"
      << "  pagx import --input icon.svg --output out.pagx   # SVG to out.pagx\n"
      << "  pagx import --input icon.svg --resolve menuIcon design.pagx\n";
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
    } else if (arg == "--resolve" && i + 1 < argc) {
      options->resolveId = argv[++i];
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
    } else if (options->pagxFile.empty()) {
      options->pagxFile = arg;
    } else {
      std::cerr << "pagx import: error: unexpected argument '" << arg << "'\n";
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

  if (!options->resolveId.empty()) {
    if (options->pagxFile.empty()) {
      std::cerr << "pagx import: error: --resolve requires a PAGX file argument\n";
      return 1;
    }
    if (options->outputFile.empty()) {
      options->outputFile = options->pagxFile;
    }
  } else {
    if (!options->pagxFile.empty()) {
      std::cerr << "pagx import: error: unexpected argument '" << options->pagxFile
                << "', use --input to specify the input file\n";
      return 1;
    }
    if (options->outputFile.empty()) {
      options->outputFile = ReplaceExtension(options->inputFile, "pagx");
    }
  }

  return 0;
}

//--------------------------------------------------------------------------------------------------
// Convert mode: import a file and write a new PAGX.
//--------------------------------------------------------------------------------------------------

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

//--------------------------------------------------------------------------------------------------
// Resolve mode: import content into an existing PAGX Layer.
//--------------------------------------------------------------------------------------------------

static std::string ReadFile(const std::string& path) {
  std::ifstream in(path);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

static std::string DetectIndent(const std::string& xml, size_t layerPos) {
  auto lineStart = xml.rfind('\n', layerPos);
  if (lineStart == std::string::npos) {
    return "";
  }
  auto indent = xml.substr(lineStart + 1, layerPos - lineStart - 1);
  if (indent.find_first_not_of(" \t") != std::string::npos) {
    return "";
  }
  return indent;
}

static std::string FloatToString(float value) {
  if (std::isnan(value)) {
    return "";
  }
  if (value == static_cast<int>(value)) {
    return std::to_string(static_cast<int>(value));
  }
  std::ostringstream oss;
  oss << value;
  return oss.str();
}

static void InlinePathData(PAGXDocument* svgDoc) {
  for (auto& node : svgDoc->nodes) {
    if (node->nodeType() == NodeType::PathData) {
      node->id.clear();
    }
  }
}

static std::string GenerateLayerContent(PAGXDocument* svgDoc, const std::string& indent) {
  auto tempDoc = PAGXDocument::Make(svgDoc->width, svgDoc->height);
  auto* layer = tempDoc->makeNode<Layer>("_resolve_");

  bool first = true;
  for (auto* svgLayer : svgDoc->layers) {
    if (svgLayer->contents.empty() && svgLayer->children.empty()) {
      continue;
    }
    if (first) {
      layer->contents.insert(layer->contents.end(), svgLayer->contents.begin(),
                             svgLayer->contents.end());
      layer->children.insert(layer->children.end(), svgLayer->children.begin(),
                             svgLayer->children.end());
      layer->styles.insert(layer->styles.end(), svgLayer->styles.begin(), svgLayer->styles.end());
      layer->filters.insert(layer->filters.end(), svgLayer->filters.begin(),
                            svgLayer->filters.end());
      first = false;
    } else {
      auto* group = tempDoc->makeNode<Group>();
      group->elements.insert(group->elements.end(), svgLayer->contents.begin(),
                             svgLayer->contents.end());
      layer->contents.push_back(group);
    }
  }

  for (auto& node : svgDoc->nodes) {
    tempDoc->nodes.push_back(std::move(node));
  }
  svgDoc->nodes.clear();
  svgDoc->layers.clear();

  tempDoc->layers.push_back(layer);
  auto xml = PAGXExporter::ToXML(*tempDoc);

  auto openTag = xml.find("<Layer id=\"_resolve_\"");
  if (openTag == std::string::npos) {
    return "";
  }
  auto tagEnd = xml.find('>', openTag);
  if (tagEnd == std::string::npos) {
    return "";
  }
  if (xml[tagEnd - 1] == '/') {
    return "";
  }
  auto contentStart = tagEnd + 1;
  auto closeTag = xml.rfind("</Layer>");
  if (closeTag == std::string::npos || closeTag <= contentStart) {
    return "";
  }
  auto innerContent = xml.substr(contentStart, closeTag - contentStart);

  std::string result;
  std::istringstream stream(innerContent);
  std::string line;
  while (std::getline(stream, line)) {
    if (line.empty() || line.find_first_not_of(" \t") == std::string::npos) {
      continue;
    }
    if (line.size() >= 4 && line.substr(0, 4) == "    ") {
      result += indent + "  " + line.substr(4) + "\n";
    } else {
      result += indent + "  " + line + "\n";
    }
  }
  return result;
}

static bool FindLayerSpan(const std::string& xml, const std::string& targetId, size_t* outStart,
                          size_t* outEnd) {
  std::string idAttr = "id=\"" + targetId + "\"";
  size_t searchPos = 0;
  while (true) {
    auto layerPos = xml.find("<Layer", searchPos);
    if (layerPos == std::string::npos) {
      return false;
    }
    auto tagEnd = xml.find('>', layerPos);
    if (tagEnd == std::string::npos) {
      return false;
    }
    auto tagContent = xml.substr(layerPos, tagEnd - layerPos + 1);
    if (tagContent.find(idAttr) == std::string::npos) {
      searchPos = tagEnd + 1;
      continue;
    }
    *outStart = layerPos;
    if (xml[tagEnd - 1] == '/') {
      *outEnd = tagEnd + 1;
      return true;
    }
    int depth = 1;
    size_t pos = tagEnd + 1;
    while (pos < xml.size() && depth > 0) {
      auto nextOpen = xml.find("<Layer", pos);
      auto nextClose = xml.find("</Layer>", pos);
      if (nextClose == std::string::npos) {
        return false;
      }
      if (nextOpen != std::string::npos && nextOpen < nextClose) {
        auto openEnd = xml.find('>', nextOpen);
        if (openEnd != std::string::npos && xml[openEnd - 1] != '/') {
          depth++;
        }
        pos = openEnd + 1;
      } else {
        depth--;
        if (depth == 0) {
          *outEnd = nextClose + 8;
          return true;
        }
        pos = nextClose + 8;
      }
    }
    return false;
  }
}

static std::string UpdateLayerTag(const std::string& openTag, float width, float height) {
  std::string result = openTag;
  auto widthStr = FloatToString(width);
  auto heightStr = FloatToString(height);

  std::regex widthRegex(R"( width="[^"]*")");
  if (std::regex_search(result, widthRegex)) {
    result = std::regex_replace(result, widthRegex, " width=\"" + widthStr + "\"");
  } else {
    std::regex idRegex(R"( id="[^"]*")");
    std::smatch match;
    if (std::regex_search(result, match, idRegex)) {
      auto pos = match.position() + match.length();
      result.insert(pos, " width=\"" + widthStr + "\"");
    }
  }

  std::regex heightRegex(R"( height="[^"]*")");
  if (std::regex_search(result, heightRegex)) {
    result = std::regex_replace(result, heightRegex, " height=\"" + heightStr + "\"");
  } else {
    std::regex widthAttr(R"( width="[^"]*")");
    std::smatch match;
    if (std::regex_search(result, match, widthAttr)) {
      auto pos = match.position() + match.length();
      result.insert(pos, " height=\"" + heightStr + "\"");
    }
  }

  return result;
}

static int ResolveIntoLayer(const ImportOptions& options) {
  auto xmlContent = ReadFile(options.pagxFile);
  if (xmlContent.empty()) {
    std::cerr << "pagx import: error: failed to read '" << options.pagxFile << "'\n";
    return 1;
  }

  size_t layerStart = 0;
  size_t layerEnd = 0;
  if (!FindLayerSpan(xmlContent, options.resolveId, &layerStart, &layerEnd)) {
    std::cerr << "pagx import: error: no Layer with id '" << options.resolveId << "' found\n";
    return 1;
  }

  auto svgDocument = SVGImporter::Parse(options.inputFile);
  if (svgDocument == nullptr) {
    std::cerr << "pagx import: error: failed to parse '" << options.inputFile << "'\n";
    return 1;
  }
  if (!svgDocument->errors.empty()) {
    for (auto& error : svgDocument->errors) {
      std::cerr << "pagx import: warning: " << error << "\n";
    }
  }
  if (svgDocument->layers.empty()) {
    std::cerr << "pagx import: error: SVG produced no layers\n";
    return 1;
  }

  InlinePathData(svgDocument.get());

  auto indent = DetectIndent(xmlContent, layerStart);
  auto innerContent = GenerateLayerContent(svgDocument.get(), indent);

  auto layerText = xmlContent.substr(layerStart, layerEnd - layerStart);
  auto firstTagEnd = layerText.find('>');
  auto openTag = layerText.substr(0, firstTagEnd);
  if (!openTag.empty() && openTag.back() == '/') {
    openTag.pop_back();
    while (!openTag.empty() && (openTag.back() == ' ' || openTag.back() == '\t')) {
      openTag.pop_back();
    }
  }
  openTag = UpdateLayerTag(openTag, svgDocument->width, svgDocument->height);

  std::string replacement;
  if (innerContent.empty()) {
    replacement = openTag + "/>";
  } else {
    replacement = openTag + ">\n" + innerContent + indent + "</Layer>";
  }

  auto result = xmlContent.substr(0, layerStart) + replacement + xmlContent.substr(layerEnd);

  std::ofstream out(options.outputFile);
  if (!out.is_open()) {
    std::cerr << "pagx import: error: failed to write '" << options.outputFile << "'\n";
    return 1;
  }
  out << result;

  std::cout << "pagx import: wrote " << options.outputFile << "\n";
  return 0;
}

//--------------------------------------------------------------------------------------------------
// Entry point
//--------------------------------------------------------------------------------------------------

int RunImport(int argc, char* argv[]) {
  ImportOptions options = {};
  auto parseResult = ParseOptions(argc, argv, &options);
  if (parseResult != 0) {
    return parseResult == -1 ? 0 : parseResult;
  }

  if (!options.resolveId.empty()) {
    return ResolveIntoLayer(options);
  }

  if (options.format == "svg") {
    return ImportFromSVG(options);
  }

  std::cerr << "pagx import: error: unsupported format '" << options.format << "'\n";
  return 1;
}

}  // namespace pagx::cli
