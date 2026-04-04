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

#include "cli/CommandInsert.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include "pagx/PAGXExporter.h"
#include "pagx/PAGXImporter.h"
#include "pagx/SVGImporter.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Path.h"

namespace pagx::cli {

struct InsertOptions {
  std::string svgFile = {};
  std::string targetId = {};
  std::string inputFile = {};
  std::string outputFile = {};
};

static void PrintUsage() {
  std::cout << "Usage: pagx insert [options] <pagx-file>\n"
            << "\n"
            << "Insert content from another format into a target Layer of a PAGX file.\n"
            << "The target Layer's contents, children, styles, and filters are replaced.\n"
            << "The Layer's width and height are set from the source. Other attributes are\n"
            << "preserved.\n"
            << "\n"
            << "Options:\n"
            << "  --svg <file>       SVG file to insert (required)\n"
            << "  --id <id>          Target Layer id in the PAGX file (required)\n"
            << "  -o, --output <file>  Output PAGX file (default: overwrite input)\n"
            << "\n"
            << "Examples:\n"
            << "  pagx insert --svg icon.svg --id menuIcon design.pagx\n"
            << "  pagx insert --svg icon.svg --id menuIcon -o out.pagx design.pagx\n";
}

static int ParseOptions(int argc, char* argv[], InsertOptions* options) {
  int i = 1;
  while (i < argc) {
    std::string arg = argv[i];
    if (arg == "--svg" && i + 1 < argc) {
      options->svgFile = argv[++i];
    } else if (arg == "--id" && i + 1 < argc) {
      options->targetId = argv[++i];
    } else if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
      options->outputFile = argv[++i];
    } else if (arg == "--help" || arg == "-h") {
      PrintUsage();
      return -1;
    } else if (arg[0] == '-') {
      std::cerr << "pagx insert: error: unknown option '" << arg << "'\n";
      return 1;
    } else if (options->inputFile.empty()) {
      options->inputFile = arg;
    } else {
      std::cerr << "pagx insert: error: unexpected argument '" << arg << "'\n";
      return 1;
    }
    i++;
  }

  if (options->svgFile.empty()) {
    std::cerr << "pagx insert: error: missing --svg file\n";
    return 1;
  }
  if (options->targetId.empty()) {
    std::cerr << "pagx insert: error: missing --id\n";
    return 1;
  }
  if (options->inputFile.empty()) {
    std::cerr << "pagx insert: error: missing input PAGX file\n";
    return 1;
  }
  if (options->outputFile.empty()) {
    options->outputFile = options->inputFile;
  }
  return 0;
}

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

// Inline all PathData references in the SVG document by clearing their ids.
static void InlinePathData(PAGXDocument* svgDoc) {
  for (auto& node : svgDoc->nodes) {
    if (node->nodeType() == NodeType::PathData) {
      node->id.clear();
    }
  }
}

// Generate the inner XML content for a Layer containing the merged SVG content.
static std::string GenerateLayerContent(PAGXDocument* svgDoc, const std::string& indent) {
  auto tempDoc = PAGXDocument::Make(svgDoc->width, svgDoc->height);
  auto* layer = tempDoc->makeNode<Layer>("_insert_");

  // First SVG Layer's contents go directly into the target Layer.
  // Subsequent Layers are wrapped in Groups for painter scope isolation.
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
      layer->styles.insert(layer->styles.end(), svgLayer->styles.begin(),
                           svgLayer->styles.end());
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

  // Transfer node ownership to the temporary document so export works.
  for (auto& node : svgDoc->nodes) {
    tempDoc->nodes.push_back(std::move(node));
  }
  svgDoc->nodes.clear();
  svgDoc->layers.clear();

  tempDoc->layers.push_back(layer);
  auto xml = PAGXExporter::ToXML(*tempDoc);

  // Extract the inner content between <Layer id="_insert_"> and </Layer>.
  auto openTag = xml.find("<Layer id=\"_insert_\"");
  if (openTag == std::string::npos) {
    return "";
  }
  auto tagEnd = xml.find('>', openTag);
  if (tagEnd == std::string::npos) {
    return "";
  }
  // Self-closing tag means no inner content.
  if (xml[tagEnd - 1] == '/') {
    return "";
  }
  auto contentStart = tagEnd + 1;
  auto closeTag = xml.rfind("</Layer>");
  if (closeTag == std::string::npos || closeTag <= contentStart) {
    return "";
  }
  auto innerContent = xml.substr(contentStart, closeTag - contentStart);

  // Re-indent: the exported XML uses 2-space indent at depth 1.
  // Replace the export indent prefix with the target indent.
  std::string result;
  std::istringstream stream(innerContent);
  std::string line;
  while (std::getline(stream, line)) {
    if (line.empty() || line.find_first_not_of(" \t") == std::string::npos) {
      continue;
    }
    // The exported content is indented by 4 spaces (2 for <pagx> + 2 for <Layer>).
    // Strip those 4 spaces and replace with target indent + 2 (one level deeper).
    if (line.size() >= 4 && line.substr(0, 4) == "    ") {
      result += indent + "  " + line.substr(4) + "\n";
    } else {
      result += indent + "  " + line + "\n";
    }
  }
  return result;
}

// Find the full extent of a <Layer id="targetId">..</Layer> or <Layer id="targetId"/>
// in the XML text.
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
    // Self-closing: <Layer id="x" ... />
    if (xml[tagEnd - 1] == '/') {
      *outEnd = tagEnd + 1;
      return true;
    }
    // Find matching </Layer> considering nesting.
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
          *outEnd = nextClose + 8;  // length of "</Layer>"
          return true;
        }
        pos = nextClose + 8;
      }
    }
    return false;
  }
}

// Update or add width/height attributes in the opening <Layer> tag.
static std::string UpdateLayerTag(const std::string& openTag, float width, float height) {
  std::string result = openTag;
  auto widthStr = FloatToString(width);
  auto heightStr = FloatToString(height);

  // Replace existing width attribute, or add after id attribute.
  std::regex widthRegex(R"( width="[^"]*")");
  if (std::regex_search(result, widthRegex)) {
    result = std::regex_replace(result, widthRegex, " width=\"" + widthStr + "\"");
  } else {
    // Insert after id="..." attribute.
    std::regex idRegex(R"( id="[^"]*")");
    std::smatch match;
    if (std::regex_search(result, match, idRegex)) {
      auto pos = match.position() + match.length();
      result.insert(pos, " width=\"" + widthStr + "\"");
    }
  }

  // Replace existing height attribute, or add after width attribute.
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

static int InsertSVG(const InsertOptions& options) {
  auto xmlContent = ReadFile(options.inputFile);
  if (xmlContent.empty()) {
    std::cerr << "pagx insert: error: failed to read '" << options.inputFile << "'\n";
    return 1;
  }

  // Verify the target Layer exists.
  size_t layerStart = 0;
  size_t layerEnd = 0;
  if (!FindLayerSpan(xmlContent, options.targetId, &layerStart, &layerEnd)) {
    std::cerr << "pagx insert: error: no Layer with id '" << options.targetId << "' found\n";
    return 1;
  }

  auto svgDocument = SVGImporter::Parse(options.svgFile);
  if (svgDocument == nullptr) {
    std::cerr << "pagx insert: error: failed to parse '" << options.svgFile << "'\n";
    return 1;
  }
  if (!svgDocument->errors.empty()) {
    for (auto& error : svgDocument->errors) {
      std::cerr << "pagx insert: warning: " << error << "\n";
    }
  }
  if (svgDocument->layers.empty()) {
    std::cerr << "pagx insert: error: SVG produced no layers\n";
    return 1;
  }

  InlinePathData(svgDocument.get());

  auto indent = DetectIndent(xmlContent, layerStart);
  auto innerContent = GenerateLayerContent(svgDocument.get(), indent);

  // Extract the original opening tag and update width/height.
  auto layerText = xmlContent.substr(layerStart, layerEnd - layerStart);
  auto firstTagEnd = layerText.find('>');
  auto openTag = layerText.substr(0, firstTagEnd);
  // Remove self-closing slash if present.
  if (!openTag.empty() && openTag.back() == '/') {
    openTag.pop_back();
    // Remove trailing whitespace before the slash.
    while (!openTag.empty() && (openTag.back() == ' ' || openTag.back() == '\t')) {
      openTag.pop_back();
    }
  }
  openTag = UpdateLayerTag(openTag, svgDocument->width, svgDocument->height);

  // Build the replacement.
  std::string replacement;
  if (innerContent.empty()) {
    replacement = openTag + "/>";
  } else {
    replacement = openTag + ">\n" + innerContent + indent + "</Layer>";
  }

  // Replace in the original XML.
  auto result = xmlContent.substr(0, layerStart) + replacement + xmlContent.substr(layerEnd);

  std::ofstream out(options.outputFile);
  if (!out.is_open()) {
    std::cerr << "pagx insert: error: failed to write '" << options.outputFile << "'\n";
    return 1;
  }
  out << result;

  std::cout << "pagx insert: wrote " << options.outputFile << "\n";
  return 0;
}

int RunInsert(int argc, char* argv[]) {
  InsertOptions options = {};
  auto parseResult = ParseOptions(argc, argv, &options);
  if (parseResult != 0) {
    return parseResult == -1 ? 0 : parseResult;
  }
  return InsertSVG(options);
}

}  // namespace pagx::cli
