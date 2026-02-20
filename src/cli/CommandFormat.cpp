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

#include "CommandFormat.h"
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <libxml/parser.h>
#include <libxml/tree.h>

namespace pagx::cli {

static void PrintFormatUsage() {
  std::cout << "Usage: pagx format [options] <file.pagx>\n"
            << "\n"
            << "Formats (pretty-prints) a PAGX file with consistent indentation and attribute\n"
            << "ordering. Does not modify values or structure.\n"
            << "\n"
            << "Options:\n"
            << "  -o, --output <path>   Output file path (default: overwrite input)\n"
            << "  --indent <n>          Indentation spaces (default: 2)\n"
            << "  -h, --help            Show this help message\n";
}

// Attribute ordering tables extracted from PAGXExporter.cpp.
// Each entry maps an element name to the canonical attribute order.
// Attributes not in the table retain their original relative order and are appended at the end.
// "data-*" attributes on Layer are always placed last.

// clang-format off
static const std::unordered_map<std::string, std::vector<const char*>> ATTRIBUTE_ORDER = {
    {"pagx", {"version", "width", "height"}},
    {"Layer", {"id", "name", "visible", "alpha", "blendMode", "x", "y", "matrix", "matrix3D",
               "preserve3D", "antiAlias", "groupOpacity", "passThroughBackground",
               "excludeChildEffectsInLayerStyle", "scrollRect", "mask", "maskType", "composition"}},
    {"Rectangle", {"center", "size", "roundness", "reversed"}},
    {"Ellipse", {"center", "size", "reversed"}},
    {"Polystar", {"center", "type", "pointCount", "outerRadius", "innerRadius", "rotation",
                  "outerRoundness", "innerRoundness", "reversed"}},
    {"Path", {"data", "reversed"}},
    {"Text", {"text", "position", "fontFamily", "fontStyle", "fontSize", "letterSpacing",
              "fauxBold", "fauxItalic", "textAnchor"}},
    {"GlyphRun", {"font", "fontSize", "glyphs", "x", "y", "xOffsets", "positions", "anchors",
                  "scales", "rotations", "skews"}},
    {"Fill", {"color", "alpha", "blendMode", "fillRule", "placement"}},
    {"Stroke", {"color", "width", "alpha", "blendMode", "cap", "join", "miterLimit", "dashes",
                "dashOffset", "dashAdaptive", "align", "placement"}},
    {"TrimPath", {"start", "end", "offset", "type"}},
    {"RoundCorner", {"radius"}},
    {"MergePath", {"mode"}},
    {"TextModifier", {"anchor", "position", "rotation", "scale", "skew", "skewAxis", "alpha",
                      "fillColor", "strokeColor", "strokeWidth"}},
    {"RangeSelector", {"start", "end", "offset", "unit", "shape", "easeIn", "easeOut", "mode",
                       "weight", "randomOrder", "randomSeed"}},
    {"TextPath", {"path", "baselineOrigin", "baselineAngle", "firstMargin", "lastMargin",
                  "perpendicular", "reversed", "forceAlignment"}},
    {"TextBox", {"position", "size", "textAlign", "paragraphAlign", "writingMode", "lineHeight",
                 "wordWrap", "overflow"}},
    {"Repeater", {"copies", "offset", "order", "anchor", "position", "rotation", "scale",
                  "startAlpha", "endAlpha"}},
    {"Group", {"anchor", "position", "rotation", "scale", "skew", "skewAxis", "alpha"}},
    {"SolidColor", {"id", "color"}},
    {"LinearGradient", {"id", "startPoint", "endPoint", "matrix"}},
    {"RadialGradient", {"id", "center", "radius", "matrix"}},
    {"ConicGradient", {"id", "center", "startAngle", "endAngle", "matrix"}},
    {"DiamondGradient", {"id", "center", "radius", "matrix"}},
    {"ImagePattern", {"id", "image", "tileModeX", "tileModeY", "filterMode", "mipmapMode",
                      "matrix"}},
    {"ColorStop", {"offset", "color"}},
    {"Image", {"id", "source"}},
    {"PathData", {"id", "data"}},
    {"Composition", {"id", "width", "height"}},
    {"Font", {"id", "unitsPerEm"}},
    {"Glyph", {"id", "path", "image", "offset", "advance"}},
    {"Resources", {}},
    {"DropShadowStyle", {"blendMode", "offsetX", "offsetY", "blurX", "blurY", "color",
                         "showBehindLayer"}},
    {"InnerShadowStyle", {"blendMode", "offsetX", "offsetY", "blurX", "blurY", "color"}},
    {"BackgroundBlurStyle", {"blendMode", "blurX", "blurY", "tileMode"}},
    {"BlurFilter", {"blurX", "blurY", "tileMode"}},
    {"DropShadowFilter", {"offsetX", "offsetY", "blurX", "blurY", "color", "shadowOnly"}},
    {"InnerShadowFilter", {"offsetX", "offsetY", "blurX", "blurY", "color", "shadowOnly"}},
    {"BlendFilter", {"color", "blendMode"}},
    {"ColorMatrixFilter", {"matrix"}},
};
// clang-format on

struct SortKeyComparator {
  const std::vector<int>* keys = nullptr;
  bool operator()(size_t a, size_t b) const {
    return (*keys)[a] < (*keys)[b];
  }
};

static void ReorderAttributes(xmlNodePtr node) {
  if (node->type != XML_ELEMENT_NODE || node->properties == nullptr) {
    return;
  }
  auto elementName = std::string(reinterpret_cast<const char*>(node->name));
  auto it = ATTRIBUTE_ORDER.find(elementName);
  if (it == ATTRIBUTE_ORDER.end()) {
    return;
  }
  const auto& order = it->second;

  // Build a position map: attribute name -> index in canonical order.
  std::unordered_map<std::string, int> positionMap = {};
  for (int i = 0; i < static_cast<int>(order.size()); i++) {
    positionMap[order[i]] = i;
  }

  // Collect all existing attributes.
  std::vector<xmlAttrPtr> attrs = {};
  for (auto attr = node->properties; attr != nullptr; attr = attr->next) {
    attrs.push_back(attr);
  }

  // Sort: known attributes by canonical order, unknown attributes after them in original order.
  int unknownBase = static_cast<int>(order.size());
  int unknownIndex = 0;
  std::vector<int> sortKeys = {};
  sortKeys.reserve(attrs.size());
  for (auto* attr : attrs) {
    auto name = std::string(reinterpret_cast<const char*>(attr->name));
    auto posIt = positionMap.find(name);
    if (posIt != positionMap.end()) {
      sortKeys.push_back(posIt->second);
    } else {
      sortKeys.push_back(unknownBase + unknownIndex);
      unknownIndex++;
    }
  }

  // Build sorted index array and apply stable sort.
  std::vector<size_t> indices(attrs.size());
  for (size_t i = 0; i < indices.size(); i++) {
    indices[i] = i;
  }
  SortKeyComparator comparator = {&sortKeys};
  std::stable_sort(indices.begin(), indices.end(), comparator);

  // Rebuild the attribute linked list in sorted order.
  node->properties = nullptr;
  xmlAttrPtr prev = nullptr;
  for (auto idx : indices) {
    auto* attr = attrs[idx];
    attr->prev = prev;
    attr->next = nullptr;
    if (prev != nullptr) {
      prev->next = attr;
    } else {
      node->properties = attr;
    }
    prev = attr;
  }
}

static void ReorderAttributesRecursive(xmlNodePtr node) {
  for (auto cur = node; cur != nullptr; cur = cur->next) {
    if (cur->type == XML_ELEMENT_NODE) {
      ReorderAttributes(cur);
      ReorderAttributesRecursive(cur->children);
    }
  }
}

static void SerializeNode(std::string& output, xmlNodePtr node, int indentLevel, int indentSpaces) {
  for (auto cur = node; cur != nullptr; cur = cur->next) {
    if (cur->type == XML_ELEMENT_NODE) {
      auto indent = std::string(static_cast<size_t>(indentLevel * indentSpaces), ' ');
      output += indent;
      output += "<";
      output += reinterpret_cast<const char*>(cur->name);

      for (auto attr = cur->properties; attr != nullptr; attr = attr->next) {
        auto* value = xmlGetProp(cur, attr->name);
        output += " ";
        output += reinterpret_cast<const char*>(attr->name);
        output += "=\"";
        if (value != nullptr) {
          // The value from xmlGetProp is already entity-escaped for attribute context.
          // But we need to ensure proper escaping: xmlGetProp returns decoded value,
          // so we re-escape for attribute output.
          auto* raw = reinterpret_cast<const char*>(value);
          for (auto* p = raw; *p != '\0'; p++) {
            switch (*p) {
              case '&':
                output += "&amp;";
                break;
              case '<':
                output += "&lt;";
                break;
              case '"':
                output += "&quot;";
                break;
              default:
                output += *p;
                break;
            }
          }
          xmlFree(value);
        }
        output += "\"";
      }

      if (cur->children != nullptr) {
        output += ">\n";
        SerializeNode(output, cur->children, indentLevel + 1, indentSpaces);
        output += indent;
        output += "</";
        output += reinterpret_cast<const char*>(cur->name);
        output += ">\n";
      } else {
        output += "/>\n";
      }
    }
  }
}

int RunFormat(int argc, char* argv[]) {
  std::string inputPath = {};
  std::string outputPath = {};
  int indentSpaces = 2;

  for (int i = 1; i < argc; i++) {
    if (std::strcmp(argv[i], "-h") == 0 || std::strcmp(argv[i], "--help") == 0) {
      PrintFormatUsage();
      return 0;
    }
    if ((std::strcmp(argv[i], "-o") == 0 || std::strcmp(argv[i], "--output") == 0) &&
        i + 1 < argc) {
      outputPath = argv[++i];
      continue;
    }
    if (std::strcmp(argv[i], "--indent") == 0 && i + 1 < argc) {
      indentSpaces = std::atoi(argv[++i]);
      if (indentSpaces < 0) {
        indentSpaces = 0;
      }
      continue;
    }
    if (argv[i][0] == '-') {
      std::cerr << "pagx format: unknown option '" << argv[i] << "'\n";
      return 1;
    }
    if (inputPath.empty()) {
      inputPath = argv[i];
    } else {
      std::cerr << "pagx format: unexpected argument '" << argv[i] << "'\n";
      return 1;
    }
  }

  if (inputPath.empty()) {
    std::cerr << "pagx format: missing input file\n";
    PrintFormatUsage();
    return 1;
  }

  if (outputPath.empty()) {
    outputPath = inputPath;
  }

  auto doc = xmlReadFile(inputPath.c_str(), nullptr, XML_PARSE_NONET | XML_PARSE_NOBLANKS);
  if (doc == nullptr) {
    std::cerr << "pagx format: failed to parse '" << inputPath << "'\n";
    return 1;
  }

  auto root = xmlDocGetRootElement(doc);
  if (root == nullptr) {
    std::cerr << "pagx format: empty document '" << inputPath << "'\n";
    xmlFreeDoc(doc);
    return 1;
  }

  ReorderAttributesRecursive(root);

  std::string output = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  SerializeNode(output, root, 0, indentSpaces);

  xmlFreeDoc(doc);

  std::ofstream out(outputPath);
  if (!out.is_open()) {
    std::cerr << "pagx format: failed to write '" << outputPath << "'\n";
    return 1;
  }
  out << output;
  out.close();
  if (out.fail()) {
    std::cerr << "pagx format: error writing to '" << outputPath << "'\n";
    return 1;
  }

  return 0;
}

}  // namespace pagx::cli
