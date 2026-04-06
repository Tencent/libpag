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

#include "cli/CommandLint.h"
#include <libxml/parser.h>
#include <libxml/xmlschemas.h>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "cli/CliUtils.h"
#include "pagx/PAGXDocument.h"
#include "pagx/PAGXImporter.h"
#include "pagx/svg/SVGPathParser.h"
#include "pagx_xsd.h"

namespace pagx::cli {

// --- Diagnostic structures ---

struct LintDiagnostic {
  int line = 0;
  std::string category = {};
  std::string message = {};
};

// --- XML helper functions ---

static std::string XmlAttr(xmlNodePtr node, const char* name) {
  if (node == nullptr) {
    return {};
  }
  xmlChar* value = xmlGetProp(node, reinterpret_cast<const xmlChar*>(name));
  if (value == nullptr) {
    return {};
  }
  std::string result(reinterpret_cast<const char*>(value));
  xmlFree(value);
  return result;
}

static float XmlAttrFloat(xmlNodePtr node, const char* name) {
  auto str = XmlAttr(node, name);
  if (str.empty()) {
    return NAN;
  }
  char* endPtr = nullptr;
  float val = strtof(str.c_str(), &endPtr);
  if (endPtr == str.c_str()) {
    return NAN;
  }
  return val;
}

static bool XmlNodeIs(xmlNodePtr node, const char* name) {
  if (node == nullptr || node->type != XML_ELEMENT_NODE) {
    return false;
  }
  return xmlStrcmp(node->name, reinterpret_cast<const xmlChar*>(name)) == 0;
}

static std::string XmlNodeName(xmlNodePtr node) {
  if (node == nullptr || node->name == nullptr) {
    return {};
  }
  return reinterpret_cast<const char*>(node->name);
}

static std::vector<xmlNodePtr> XmlChildElements(xmlNodePtr node) {
  std::vector<xmlNodePtr> result = {};
  if (node == nullptr) {
    return result;
  }
  for (xmlNodePtr child = node->children; child != nullptr; child = child->next) {
    if (child->type == XML_ELEMENT_NODE) {
      result.push_back(child);
    }
  }
  return result;
}

static xmlNodePtr XmlFindRoot(xmlDocPtr doc) {
  if (doc == nullptr) {
    return nullptr;
  }
  return xmlDocGetRootElement(doc);
}

// --- XSD validation ---

#if LIBXML_VERSION >= 21200
static void CollectStructuredError(void* context, const xmlError* xmlError) {
#else
static void CollectStructuredError(void* context, xmlError* xmlError) {
#endif
  if (xmlError == nullptr) {
    return;
  }
  auto* diagnostics = static_cast<std::vector<LintDiagnostic>*>(context);
  LintDiagnostic diag = {};
  diag.line = xmlError->line;
  diag.category = "error";
  std::string msg(xmlError->message != nullptr ? xmlError->message : "Unknown error");
  while (!msg.empty() && msg.back() == '\n') {
    msg.pop_back();
  }
  diag.message = std::move(msg);
  diagnostics->push_back(std::move(diag));
}

static bool ValidateXsd(xmlDocPtr doc, std::vector<LintDiagnostic>& diagnostics) {
  const auto& xsdContent = PagxXsdContent();
  xmlSchemaParserCtxtPtr parserCtxt =
      xmlSchemaNewMemParserCtxt(xsdContent.c_str(), static_cast<int>(xsdContent.size()));
  if (parserCtxt == nullptr) {
    LintDiagnostic diag = {};
    diag.category = "error";
    diag.message = "Internal error: failed to create schema parser context";
    diagnostics.push_back(std::move(diag));
    return false;
  }

  xmlSchemaPtr schema = xmlSchemaParse(parserCtxt);
  xmlSchemaFreeParserCtxt(parserCtxt);
  if (schema == nullptr) {
    LintDiagnostic diag = {};
    diag.category = "error";
    diag.message = "Internal error: failed to parse XSD schema";
    diagnostics.push_back(std::move(diag));
    return false;
  }

  xmlSchemaValidCtxtPtr validCtxt = xmlSchemaNewValidCtxt(schema);
  if (validCtxt == nullptr) {
    xmlSchemaFree(schema);
    LintDiagnostic diag = {};
    diag.category = "error";
    diag.message = "Internal error: failed to create validation context";
    diagnostics.push_back(std::move(diag));
    return false;
  }

  xmlSchemaSetValidStructuredErrors(validCtxt, CollectStructuredError, &diagnostics);
  int ret = xmlSchemaValidateDoc(validCtxt, doc);

  xmlSchemaFreeValidCtxt(validCtxt);
  xmlSchemaFree(schema);
  return ret == 0;
}

static void CollectSemanticErrors(const std::string& filePath,
                                  std::vector<LintDiagnostic>& diagnostics) {
  auto pagxDoc = pagx::PAGXImporter::FromFile(filePath);
  if (pagxDoc == nullptr) {
    return;
  }
  for (const auto& errorStr : pagxDoc->errors) {
    LintDiagnostic diag = {};
    diag.category = "error";
    if (errorStr.compare(0, 5, "line ") == 0) {
      auto colonPos = errorStr.find(':', 5);
      if (colonPos != std::string::npos) {
        auto lineStr = errorStr.substr(5, colonPos - 5);
        char* endPtr = nullptr;
        long lineNum = strtol(lineStr.c_str(), &endPtr, 10);
        diag.line = (endPtr != lineStr.c_str() && *endPtr == '\0') ? static_cast<int>(lineNum) : 0;
        diag.message = errorStr.substr(colonPos + 2);
      } else {
        diag.message = errorStr;
      }
    } else {
      diag.message = errorStr;
    }
    diagnostics.push_back(std::move(diag));
  }
}

// --- Node traversal helpers ---

static void CollectAllNodes(xmlNodePtr node, std::vector<xmlNodePtr>& nodes);

static void CollectAllNodesFromChildren(xmlNodePtr node, std::vector<xmlNodePtr>& nodes) {
  for (auto& child : XmlChildElements(node)) {
    CollectAllNodes(child, nodes);
  }
}

static void CollectAllNodes(xmlNodePtr node, std::vector<xmlNodePtr>& nodes) {
  if (node == nullptr) {
    return;
  }
  nodes.push_back(node);
  CollectAllNodesFromChildren(node, nodes);
}

static xmlNodePtr FindNodeById(xmlNodePtr root, const std::string& id) {
  std::vector<xmlNodePtr> allNodes = {};
  CollectAllNodes(root, allNodes);
  for (auto* node : allNodes) {
    if (XmlAttr(node, "id") == id) {
      return node;
    }
  }
  return nullptr;
}

static std::string GetNodeDisplayName(xmlNodePtr node) {
  auto name = XmlAttr(node, "name");
  if (!name.empty()) {
    return name;
  }
  auto id = XmlAttr(node, "id");
  if (!id.empty()) {
    return id;
  }
  return XmlNodeName(node);
}

// --- Pass 1: Empty node detection ---

static bool IsGeometryNode(xmlNodePtr node) {
  auto name = XmlNodeName(node);
  return name == "Rectangle" || name == "Ellipse" || name == "Polystar" || name == "Path";
}

static bool IsPainterNode(xmlNodePtr node) {
  auto name = XmlNodeName(node);
  return name == "Fill" || name == "Stroke";
}

static bool IsModifierNode(xmlNodePtr node) {
  auto name = XmlNodeName(node);
  return name == "TrimPath" || name == "RoundCorner" || name == "MergePath";
}

static bool IsContentNode(xmlNodePtr node) {
  return IsGeometryNode(node) || IsPainterNode(node) || IsModifierNode(node) ||
         XmlNodeName(node) == "Group" || XmlNodeName(node) == "Repeater" ||
         XmlNodeName(node) == "Text" || XmlNodeName(node) == "TextBox" ||
         XmlNodeName(node) == "TextPath" || XmlNodeName(node) == "TextModifier";
}

static bool IsLayerChildNode(xmlNodePtr node) {
  return XmlNodeName(node) == "Layer";
}

static void DetectEmptyNodes(xmlNodePtr root, std::vector<LintDiagnostic>& diagnostics) {
  std::vector<xmlNodePtr> allNodes = {};
  CollectAllNodes(root, allNodes);

  for (auto* node : allNodes) {
    auto nodeName = XmlNodeName(node);

    if (nodeName == "Layer") {
      auto children = XmlChildElements(node);
      bool hasContents = false;
      bool hasChildLayers = false;
      for (auto* child : children) {
        if (IsContentNode(child)) {
          hasContents = true;
        }
        if (IsLayerChildNode(child)) {
          hasChildLayers = true;
        }
      }
      bool hasWidth = !std::isnan(XmlAttrFloat(node, "width"));
      bool hasHeight = !std::isnan(XmlAttrFloat(node, "height"));
      auto composition = XmlAttr(node, "composition");
      bool hasComposition = !composition.empty();

      if (!hasContents && !hasChildLayers && !hasWidth && !hasHeight && !hasComposition) {
        LintDiagnostic diag = {};
        diag.line = static_cast<int>(node->line);
        diag.category = "warning";
        diag.message =
            "empty node(s) can be removed: Layer (no contents, children, or composition)";
        diagnostics.push_back(std::move(diag));
      }
    }

    if (nodeName == "Group") {
      auto children = XmlChildElements(node);
      bool hasElements = false;
      for (auto* child : children) {
        if (IsContentNode(child)) {
          hasElements = true;
          break;
        }
      }
      if (!hasElements) {
        LintDiagnostic diag = {};
        diag.line = static_cast<int>(node->line);
        diag.category = "warning";
        diag.message = "empty node(s) can be removed: Group (no elements)";
        diagnostics.push_back(std::move(diag));
      }
    }

    if (nodeName == "Stroke") {
      float width = XmlAttrFloat(node, "width");
      if (!std::isnan(width) && width <= 0.0f) {
        LintDiagnostic diag = {};
        diag.line = static_cast<int>(node->line);
        diag.category = "warning";
        diag.message = "empty node(s) can be removed: Stroke width <= 0, no visual effect";
        diagnostics.push_back(std::move(diag));
      }
    }
  }
}

// --- Pass 7: Full canvas clip mask detection ---

static void DetectFullCanvasClipMasks(xmlNodePtr root, float canvasWidth, float canvasHeight,
                                      std::vector<LintDiagnostic>& diagnostics) {
  std::vector<xmlNodePtr> allNodes = {};
  CollectAllNodes(root, allNodes);

  for (auto* node : allNodes) {
    if (!XmlNodeIs(node, "Layer")) {
      continue;
    }
    auto maskRef = XmlAttr(node, "mask");
    if (maskRef.empty() || maskRef[0] != '@') {
      continue;
    }
    std::string maskId = maskRef.substr(1);
    auto* maskNode = FindNodeById(root, maskId);
    if (maskNode == nullptr || !XmlNodeIs(maskNode, "Layer")) {
      continue;
    }

    float maskX = XmlAttrFloat(maskNode, "x");
    float maskY = XmlAttrFloat(maskNode, "y");
    if (std::isnan(maskX)) {
      maskX = 0;
    }
    if (std::isnan(maskY)) {
      maskY = 0;
    }
    if (maskX != 0 || maskY != 0) {
      continue;
    }

    auto maskMatrix = XmlAttr(maskNode, "matrix");
    if (!maskMatrix.empty() && maskMatrix != "1,0,0,1,0,0") {
      continue;
    }

    auto maskChildren = XmlChildElements(maskNode);
    xmlNodePtr rectNode = nullptr;
    for (auto* child : maskChildren) {
      if (XmlNodeIs(child, "Rectangle")) {
        rectNode = child;
        break;
      }
    }
    if (rectNode == nullptr) {
      continue;
    }

    auto sizeStr = XmlAttr(rectNode, "size");
    if (sizeStr.empty()) {
      continue;
    }
    float rectWidth = 0;
    float rectHeight = 0;
    if (sscanf(sizeStr.c_str(), "%f,%f", &rectWidth, &rectHeight) != 2) {
      continue;
    }

    auto posStr = XmlAttr(rectNode, "position");
    float rectPosX = 0;
    float rectPosY = 0;
    if (!posStr.empty()) {
      if (sscanf(posStr.c_str(), "%f,%f", &rectPosX, &rectPosY) != 2) {
        continue;
      }
    }

    float left = rectPosX - rectWidth * 0.5f;
    float top = rectPosY - rectHeight * 0.5f;
    float right = rectPosX + rectWidth * 0.5f;
    float bottom = rectPosY + rectHeight * 0.5f;
    if (left <= 0 && top <= 0 && right >= canvasWidth && bottom >= canvasHeight) {
      LintDiagnostic diag = {};
      diag.line = static_cast<int>(node->line);
      diag.category = "warning";
      diag.message = "full-canvas clip mask(s) can be removed (no visible effect)";
      diagnostics.push_back(std::move(diag));
    }
  }
}

// --- Pass 13: Unreferenced resource detection ---

struct ResourceInfo {
  int line = 0;
  std::string nodeType = {};
};

static void CollectResourceIds(xmlNodePtr root,
                               std::unordered_map<std::string, ResourceInfo>& resourceIds) {
  auto children = XmlChildElements(root);
  for (auto* child : children) {
    if (XmlNodeIs(child, "Resources")) {
      std::vector<xmlNodePtr> allResNodes = {};
      CollectAllNodesFromChildren(child, allResNodes);
      for (auto* resNode : allResNodes) {
        auto id = XmlAttr(resNode, "id");
        if (!id.empty()) {
          ResourceInfo info = {};
          info.line = static_cast<int>(resNode->line);
          info.nodeType = XmlNodeName(resNode);
          resourceIds[id] = info;
        }
      }
      break;
    }
  }
}

static void CollectReferencedIds(xmlNodePtr node, std::unordered_set<std::string>& referencedIds) {
  if (node == nullptr) {
    return;
  }
  for (xmlAttrPtr attr = node->properties; attr != nullptr; attr = attr->next) {
    if (attr->children != nullptr && attr->children->content != nullptr) {
      std::string value(reinterpret_cast<const char*>(attr->children->content));
      if (!value.empty() && value[0] == '@') {
        referencedIds.insert(value.substr(1));
      }
    }
  }
  for (auto& child : XmlChildElements(node)) {
    CollectReferencedIds(child, referencedIds);
  }
}

static void DetectUnreferencedResources(xmlNodePtr root, std::vector<LintDiagnostic>& diagnostics) {
  std::unordered_map<std::string, ResourceInfo> resourceIds = {};
  CollectResourceIds(root, resourceIds);

  std::unordered_set<std::string> referencedIds = {};
  CollectReferencedIds(root, referencedIds);

  for (const auto& pair : resourceIds) {
    if (referencedIds.find(pair.first) == referencedIds.end()) {
      LintDiagnostic diag = {};
      diag.line = pair.second.line;
      diag.category = "warning";
      diag.message =
          "unreferenced resource: <" + pair.second.nodeType + "> (id='" + pair.first + "')";
      diagnostics.push_back(std::move(diag));
    }
  }
}

// --- Pass 2: PathData deduplication detection ---

static void DetectDuplicatePathData(xmlNodePtr root, std::vector<LintDiagnostic>& diagnostics) {
  std::vector<xmlNodePtr> allNodes = {};
  CollectAllNodes(root, allNodes);

  std::vector<std::pair<std::string, int>> pathDataList = {};
  for (auto* node : allNodes) {
    if (XmlNodeIs(node, "PathData")) {
      auto data = XmlAttr(node, "data");
      if (!data.empty()) {
        pathDataList.emplace_back(data, static_cast<int>(node->line));
      }
    }
  }

  std::unordered_map<std::string, int> seen = {};
  for (const auto& item : pathDataList) {
    auto it = seen.find(item.first);
    if (it != seen.end()) {
      LintDiagnostic diag = {};
      diag.line = item.second;
      diag.category = "info";
      diag.message = "duplicate PathData, same content as line " + std::to_string(it->second);
      diagnostics.push_back(std::move(diag));
    } else {
      seen[item.first] = item.second;
    }
  }
}

// --- Pass 3: Gradient deduplication detection ---

static std::string SerializeGradientNode(xmlNodePtr node) {
  std::ostringstream oss;
  oss << XmlNodeName(node) << "|";
  oss << XmlAttr(node, "startPoint") << "|";
  oss << XmlAttr(node, "endPoint") << "|";
  oss << XmlAttr(node, "center") << "|";
  oss << XmlAttr(node, "radius") << "|";
  oss << XmlAttr(node, "startAngle") << "|";
  oss << XmlAttr(node, "endAngle") << "|";
  oss << XmlAttr(node, "matrix") << "|";

  auto children = XmlChildElements(node);
  for (auto* child : children) {
    if (XmlNodeIs(child, "ColorStop")) {
      oss << "stop:" << XmlAttr(child, "offset") << ":" << XmlAttr(child, "color") << ";";
    }
  }
  return oss.str();
}

static bool IsGradientNode(xmlNodePtr node) {
  auto name = XmlNodeName(node);
  return name == "LinearGradient" || name == "RadialGradient" || name == "ConicGradient" ||
         name == "DiamondGradient";
}

static void DetectDuplicateGradients(xmlNodePtr root, std::vector<LintDiagnostic>& diagnostics) {
  std::vector<xmlNodePtr> allNodes = {};
  CollectAllNodes(root, allNodes);

  struct GradientEntry {
    std::string signature = {};
    std::string nodeType = {};
    int line = 0;
  };
  std::vector<GradientEntry> gradientList = {};
  for (auto* node : allNodes) {
    if (IsGradientNode(node)) {
      GradientEntry entry = {};
      entry.signature = SerializeGradientNode(node);
      entry.nodeType = XmlNodeName(node);
      entry.line = static_cast<int>(node->line);
      gradientList.push_back(std::move(entry));
    }
  }

  std::unordered_map<std::string, int> seen = {};
  for (const auto& item : gradientList) {
    auto it = seen.find(item.signature);
    if (it != seen.end()) {
      LintDiagnostic diag = {};
      diag.line = item.line;
      diag.category = "info";
      diag.message =
          "duplicate " + item.nodeType + ", same parameters as line " + std::to_string(it->second);
      diagnostics.push_back(std::move(diag));
    } else {
      seen[item.signature] = item.line;
    }
  }
}

// --- Pass 4: Mergeable Group detection ---

static std::string SerializePainter(xmlNodePtr node) {
  if (!IsPainterNode(node)) {
    return {};
  }
  std::ostringstream oss;
  oss << XmlNodeName(node) << "|";
  oss << XmlAttr(node, "color") << "|";
  oss << XmlAttr(node, "alpha") << "|";
  oss << XmlAttr(node, "blendMode") << "|";
  if (XmlNodeIs(node, "Fill")) {
    oss << XmlAttr(node, "fillRule") << "|";
    oss << XmlAttr(node, "placement") << "|";
  }
  if (XmlNodeIs(node, "Stroke")) {
    oss << XmlAttr(node, "width") << "|";
    oss << XmlAttr(node, "cap") << "|";
    oss << XmlAttr(node, "join") << "|";
    oss << XmlAttr(node, "miterLimit") << "|";
    oss << XmlAttr(node, "dashes") << "|";
    oss << XmlAttr(node, "dashOffset") << "|";
    oss << XmlAttr(node, "dashAdaptive") << "|";
    oss << XmlAttr(node, "align") << "|";
    oss << XmlAttr(node, "placement") << "|";
  }

  auto children = XmlChildElements(node);
  for (auto* child : children) {
    if (IsGradientNode(child)) {
      oss << "gradient:" << SerializeGradientNode(child) << ";";
    } else if (XmlNodeIs(child, "SolidColor")) {
      oss << "solid:" << XmlAttr(child, "color") << ";";
    } else if (XmlNodeIs(child, "ImagePattern")) {
      oss << "pattern:" << XmlAttr(child, "image") << ";";
    }
  }
  return oss.str();
}

static bool HasDefaultGroupTransform(xmlNodePtr node) {
  auto anchor = XmlAttr(node, "anchor");
  auto position = XmlAttr(node, "position");
  auto rotation = XmlAttrFloat(node, "rotation");
  auto scale = XmlAttr(node, "scale");
  auto skew = XmlAttrFloat(node, "skew");
  auto skewAxis = XmlAttrFloat(node, "skewAxis");
  auto alpha = XmlAttrFloat(node, "alpha");
  auto width = XmlAttrFloat(node, "width");
  auto height = XmlAttrFloat(node, "height");
  auto left = XmlAttrFloat(node, "left");
  auto right = XmlAttrFloat(node, "right");
  auto top = XmlAttrFloat(node, "top");
  auto bottom = XmlAttrFloat(node, "bottom");
  auto centerX = XmlAttrFloat(node, "centerX");
  auto centerY = XmlAttrFloat(node, "centerY");

  if (!anchor.empty() && anchor != "0,0") {
    return false;
  }
  if (!position.empty() && position != "0,0") {
    return false;
  }
  if (!std::isnan(rotation) && rotation != 0) {
    return false;
  }
  if (!scale.empty() && scale != "1,1") {
    return false;
  }
  if (!std::isnan(skew) && skew != 0) {
    return false;
  }
  if (!std::isnan(skewAxis) && skewAxis != 0) {
    return false;
  }
  if (!std::isnan(alpha) && alpha != 1) {
    return false;
  }
  if (!std::isnan(width) || !std::isnan(height)) {
    return false;
  }
  if (!std::isnan(left) || !std::isnan(right) || !std::isnan(top) || !std::isnan(bottom) ||
      !std::isnan(centerX) || !std::isnan(centerY)) {
    return false;
  }
  return true;
}

static std::string ExtractPaintersSignature(xmlNodePtr groupNode) {
  auto children = XmlChildElements(groupNode);
  std::ostringstream oss;
  bool seenPainter = false;
  for (auto* child : children) {
    if (IsPainterNode(child)) {
      seenPainter = true;
      oss << SerializePainter(child);
    } else if (seenPainter) {
      return {};
    } else if (!IsGeometryNode(child)) {
      return {};
    }
  }
  return oss.str();
}

static void DetectMergeableGroups(xmlNodePtr root, std::vector<LintDiagnostic>& diagnostics) {
  std::vector<xmlNodePtr> allNodes = {};
  CollectAllNodes(root, allNodes);

  for (auto* node : allNodes) {
    if (!XmlNodeIs(node, "Layer") && !XmlNodeIs(node, "Group") && !XmlNodeIs(node, "TextBox")) {
      continue;
    }
    auto children = XmlChildElements(node);
    size_t i = 0;
    while (i < children.size()) {
      auto* current = children[i];
      if (!XmlNodeIs(current, "Group") || !HasDefaultGroupTransform(current)) {
        i++;
        continue;
      }
      auto sig = ExtractPaintersSignature(current);
      if (sig.empty()) {
        i++;
        continue;
      }
      size_t j = i + 1;
      while (j < children.size()) {
        auto* next = children[j];
        if (!XmlNodeIs(next, "Group") || !HasDefaultGroupTransform(next)) {
          break;
        }
        auto nextSig = ExtractPaintersSignature(next);
        if (nextSig != sig) {
          break;
        }
        LintDiagnostic diag = {};
        diag.line = static_cast<int>(next->line);
        diag.category = "info";
        diag.message = "can be merged with previous Group (same painters)";
        diagnostics.push_back(std::move(diag));
        j++;
      }
      i = j;
    }
  }
}

// --- Pass 5: Redundant first-child Group detection ---

static bool CanUnwrapFirstChildGroup(xmlNodePtr groupNode) {
  if (!XmlNodeIs(groupNode, "Group")) {
    return false;
  }
  auto alpha = XmlAttrFloat(groupNode, "alpha");
  if (!std::isnan(alpha) && alpha != 1.0f) {
    return false;
  }
  auto position = XmlAttr(groupNode, "position");
  if (!position.empty() && position != "0,0") {
    return false;
  }
  auto anchor = XmlAttr(groupNode, "anchor");
  if (!anchor.empty() && anchor != "0,0") {
    return false;
  }
  auto rotation = XmlAttrFloat(groupNode, "rotation");
  if (!std::isnan(rotation) && rotation != 0) {
    return false;
  }
  auto scale = XmlAttr(groupNode, "scale");
  if (!scale.empty() && scale != "1,1") {
    return false;
  }
  auto skew = XmlAttrFloat(groupNode, "skew");
  if (!std::isnan(skew) && skew != 0) {
    return false;
  }
  auto skewAxis = XmlAttrFloat(groupNode, "skewAxis");
  if (!std::isnan(skewAxis) && skewAxis != 0) {
    return false;
  }
  auto width = XmlAttrFloat(groupNode, "width");
  auto height = XmlAttrFloat(groupNode, "height");
  if (!std::isnan(width) || !std::isnan(height)) {
    return false;
  }
  auto left = XmlAttrFloat(groupNode, "left");
  auto right = XmlAttrFloat(groupNode, "right");
  auto top = XmlAttrFloat(groupNode, "top");
  auto bottom = XmlAttrFloat(groupNode, "bottom");
  auto centerX = XmlAttrFloat(groupNode, "centerX");
  auto centerY = XmlAttrFloat(groupNode, "centerY");
  if (!std::isnan(left) || !std::isnan(right) || !std::isnan(top) || !std::isnan(bottom) ||
      !std::isnan(centerX) || !std::isnan(centerY)) {
    return false;
  }

  auto children = XmlChildElements(groupNode);
  for (auto* child : children) {
    auto childRight = XmlAttrFloat(child, "right");
    auto childBottom = XmlAttrFloat(child, "bottom");
    auto childCenterX = XmlAttrFloat(child, "centerX");
    auto childCenterY = XmlAttrFloat(child, "centerY");
    if (!std::isnan(childRight) || !std::isnan(childBottom) || !std::isnan(childCenterX) ||
        !std::isnan(childCenterY)) {
      return false;
    }
  }
  return true;
}

static void CollectUnwrappableFirstChildGroups(const std::vector<xmlNodePtr>& elements,
                                               std::vector<LintDiagnostic>& diagnostics) {
  if (elements.empty()) {
    return;
  }
  auto* first = elements[0];
  if (XmlNodeIs(first, "Group") && CanUnwrapFirstChildGroup(first)) {
    LintDiagnostic diag = {};
    diag.line = static_cast<int>(first->line);
    diag.category = "info";
    diag.message =
        "redundant first-child Group: no preceding elements need painter isolation."
        " Fix: unwrap this Group only";
    diagnostics.push_back(std::move(diag));
  }
}

static void DetectUnwrappableFirstChildGroups(xmlNodePtr root,
                                              std::vector<LintDiagnostic>& diagnostics) {
  std::vector<xmlNodePtr> allNodes = {};
  CollectAllNodes(root, allNodes);

  for (auto* node : allNodes) {
    std::vector<xmlNodePtr> contentChildren = {};
    auto children = XmlChildElements(node);
    for (auto* child : children) {
      if (IsContentNode(child)) {
        contentChildren.push_back(child);
      }
    }
    if (XmlNodeIs(node, "Layer") || XmlNodeIs(node, "Group") || XmlNodeIs(node, "TextBox")) {
      CollectUnwrappableFirstChildGroups(contentChildren, diagnostics);
    }
  }
}

// --- Pass 6: Path to primitive detection ---

struct PathPoint {
  float x = 0;
  float y = 0;
};

static std::vector<std::string> ParseVerbs(const std::string& data) {
  std::vector<std::string> verbs = {};
  for (size_t i = 0; i < data.size(); i++) {
    char c = data[i];
    if (c == 'M' || c == 'm' || c == 'L' || c == 'l' || c == 'H' || c == 'h' || c == 'V' ||
        c == 'v' || c == 'C' || c == 'c' || c == 'S' || c == 's' || c == 'Q' || c == 'q' ||
        c == 'T' || c == 't' || c == 'A' || c == 'a' || c == 'Z' || c == 'z') {
      verbs.push_back(std::string(1, static_cast<char>(std::toupper(c))));
    }
  }
  return verbs;
}

static std::vector<float> ParseNumbers(const std::string& data) {
  std::vector<float> nums = {};
  size_t i = 0;
  while (i < data.size()) {
    char c = data[i];
    if ((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.') {
      char* end = nullptr;
      float val = strtof(data.c_str() + i, &end);
      if (end != data.c_str() + i) {
        nums.push_back(val);
        i = static_cast<size_t>(end - data.c_str());
        continue;
      }
    }
    i++;
  }
  return nums;
}

static bool IsAxisAlignedRectangle(const std::string& data, const std::vector<std::string>& verbs) {
  bool hasCorrectVerbs = false;
  if (verbs.size() == 5) {
    hasCorrectVerbs =
        verbs[0] == "M" && verbs[1] == "L" && verbs[2] == "L" && verbs[3] == "L" && verbs[4] == "Z";
  } else if (verbs.size() == 6) {
    hasCorrectVerbs = verbs[0] == "M" && verbs[1] == "L" && verbs[2] == "L" && verbs[3] == "L" &&
                      verbs[4] == "L" && verbs[5] == "Z";
  }
  if (!hasCorrectVerbs) {
    return false;
  }
  auto nums = ParseNumbers(data);
  // Extract points: M x,y then L x,y pairs.
  size_t pointCount = (verbs.size() == 5) ? 4 : 5;
  if (nums.size() < pointCount * 2) {
    return false;
  }
  std::vector<PathPoint> points(pointCount);
  for (size_t i = 0; i < pointCount; i++) {
    points[i] = {nums[i * 2], nums[i * 2 + 1]};
  }
  // Close the polygon by connecting last point back to first.
  // Check that every edge is axis-aligned (horizontal or vertical).
  for (size_t i = 0; i < pointCount; i++) {
    auto& p1 = points[i];
    auto& p2 = points[(i + 1) % pointCount];
    float dx = std::abs(p2.x - p1.x);
    float dy = std::abs(p2.y - p1.y);
    if (dx > 0.01f && dy > 0.01f) {
      return false;  // Diagonal edge — not a rectangle.
    }
  }
  return true;
}

static bool IsApproximateEllipse(const std::string& data, const std::vector<std::string>& verbs) {
  if (verbs.size() != 6) {
    return false;
  }
  if (!(verbs[0] == "M" && verbs[1] == "C" && verbs[2] == "C" && verbs[3] == "C" &&
        verbs[4] == "C" && verbs[5] == "Z")) {
    return false;
  }
  auto nums = ParseNumbers(data);
  // M x,y + 4 * (C cp1x,cp1y cp2x,cp2y x,y) = 2 + 4*6 = 26 numbers
  if (nums.size() < 26) {
    return false;
  }
  // Extract the 4 on-curve points (start point + 4 endpoints of cubic segments).
  PathPoint onCurve[4];
  onCurve[0] = {nums[0], nums[1]};    // M point
  onCurve[1] = {nums[8], nums[9]};    // End of first C
  onCurve[2] = {nums[14], nums[15]};  // End of second C
  onCurve[3] = {nums[20], nums[21]};  // End of third C
  // Fourth C endpoint should return to start: nums[26], nums[27] ≈ onCurve[0]
  // Compute bounding box of on-curve points.
  float minX = onCurve[0].x, maxX = onCurve[0].x;
  float minY = onCurve[0].y, maxY = onCurve[0].y;
  for (int i = 1; i < 4; i++) {
    minX = std::min(minX, onCurve[i].x);
    maxX = std::max(maxX, onCurve[i].x);
    minY = std::min(minY, onCurve[i].y);
    maxY = std::max(maxY, onCurve[i].y);
  }
  float cx = (minX + maxX) / 2.0f;
  float cy = (minY + maxY) / 2.0f;
  float rx = (maxX - minX) / 2.0f;
  float ry = (maxY - minY) / 2.0f;
  if (rx < 0.01f || ry < 0.01f) {
    return false;
  }
  // Check that on-curve points are at the cardinal positions of the ellipse (top/right/bottom/left).
  // For a standard ellipse drawn with 4 cubic Beziers, the on-curve points should be at the
  // midpoints of each side of the bounding box.
  bool foundTop = false, foundBottom = false, foundLeft = false, foundRight = false;
  static constexpr float TOLERANCE = 1.0f;
  for (int i = 0; i < 4; i++) {
    float dx = std::abs(onCurve[i].x - cx);
    float dy = std::abs(onCurve[i].y - cy);
    if (dx < TOLERANCE && std::abs(dy - ry) < TOLERANCE) {
      if (onCurve[i].y < cy) {
        foundTop = true;
      } else {
        foundBottom = true;
      }
    } else if (dy < TOLERANCE && std::abs(dx - rx) < TOLERANCE) {
      if (onCurve[i].x < cx) {
        foundLeft = true;
      } else {
        foundRight = true;
      }
    }
  }
  if (!(foundTop && foundBottom && foundLeft && foundRight)) {
    return false;
  }
  // Verify control points match the kappa ratio for circular arcs.
  // kappa ≈ 0.5522847 — control point offset from on-curve point for a quarter circle.
  static constexpr float KAPPA = 0.5522847f;
  static constexpr float CP_TOLERANCE = 2.0f;
  float expectedCpOffsetX = rx * KAPPA;
  float expectedCpOffsetY = ry * KAPPA;
  // Check each cubic segment's control points.
  for (int seg = 0; seg < 4; seg++) {
    float cp1x = nums[2 + seg * 6];
    float cp1y = nums[3 + seg * 6];
    float cp2x = nums[4 + seg * 6];
    float cp2y = nums[5 + seg * 6];
    // The control points should be offset from their respective on-curve points by kappa * radius.
    PathPoint segStart = (seg == 0) ? onCurve[0] : PathPoint{nums[seg * 6], nums[seg * 6 + 1]};
    PathPoint segEnd = {nums[6 + seg * 6], nums[7 + seg * 6]};
    float cp1DistX = std::abs(cp1x - segStart.x);
    float cp1DistY = std::abs(cp1y - segStart.y);
    float cp2DistX = std::abs(cp2x - segEnd.x);
    float cp2DistY = std::abs(cp2y - segEnd.y);
    // One of the distances should be ~0 and the other ~kappa*r.
    bool cp1Valid =
        (cp1DistX < CP_TOLERANCE && std::abs(cp1DistY - expectedCpOffsetY) < CP_TOLERANCE) ||
        (cp1DistY < CP_TOLERANCE && std::abs(cp1DistX - expectedCpOffsetX) < CP_TOLERANCE);
    bool cp2Valid =
        (cp2DistX < CP_TOLERANCE && std::abs(cp2DistY - expectedCpOffsetY) < CP_TOLERANCE) ||
        (cp2DistY < CP_TOLERANCE && std::abs(cp2DistX - expectedCpOffsetX) < CP_TOLERANCE);
    if (!cp1Valid || !cp2Valid) {
      return false;
    }
  }
  return true;
}

static void DetectPathsToPrimitives(xmlNodePtr root, std::vector<LintDiagnostic>& diagnostics) {
  std::vector<xmlNodePtr> allNodes = {};
  CollectAllNodes(root, allNodes);

  std::unordered_map<std::string, std::string> pathDataMap = {};
  for (auto* node : allNodes) {
    if (XmlNodeIs(node, "PathData")) {
      auto id = XmlAttr(node, "id");
      auto data = XmlAttr(node, "data");
      if (!id.empty() && !data.empty()) {
        pathDataMap[id] = data;
      }
    }
  }

  for (auto* node : allNodes) {
    if (!XmlNodeIs(node, "Path")) {
      continue;
    }
    auto left = XmlAttrFloat(node, "left");
    auto right = XmlAttrFloat(node, "right");
    auto top = XmlAttrFloat(node, "top");
    auto bottom = XmlAttrFloat(node, "bottom");
    auto centerX = XmlAttrFloat(node, "centerX");
    auto centerY = XmlAttrFloat(node, "centerY");
    if (!std::isnan(left) || !std::isnan(right) || !std::isnan(top) || !std::isnan(bottom) ||
        !std::isnan(centerX) || !std::isnan(centerY)) {
      continue;
    }

    auto dataRef = XmlAttr(node, "data");
    if (dataRef.empty()) {
      continue;
    }
    std::string pathDataStr;
    if (dataRef[0] == '@') {
      auto it = pathDataMap.find(dataRef.substr(1));
      if (it != pathDataMap.end()) {
        pathDataStr = it->second;
      }
    } else {
      pathDataStr = dataRef;
    }
    if (pathDataStr.empty()) {
      continue;
    }

    auto verbs = ParseVerbs(pathDataStr);
    if (IsAxisAlignedRectangle(pathDataStr, verbs)) {
      LintDiagnostic diag = {};
      diag.line = static_cast<int>(node->line);
      diag.category = "info";
      diag.message = "Path can be replaced with Rectangle";
      diagnostics.push_back(std::move(diag));
    } else if (IsApproximateEllipse(pathDataStr, verbs)) {
      LintDiagnostic diag = {};
      diag.line = static_cast<int>(node->line);
      diag.category = "info";
      diag.message = "Path can be replaced with Ellipse";
      diagnostics.push_back(std::move(diag));
    }
  }
}

// --- Pass 9: Coordinate localization detection ---

static bool HasConstraintAttributes(xmlNodePtr node) {
  auto left = XmlAttrFloat(node, "left");
  auto right = XmlAttrFloat(node, "right");
  auto top = XmlAttrFloat(node, "top");
  auto bottom = XmlAttrFloat(node, "bottom");
  auto centerX = XmlAttrFloat(node, "centerX");
  auto centerY = XmlAttrFloat(node, "centerY");
  return !std::isnan(left) || !std::isnan(right) || !std::isnan(top) || !std::isnan(bottom) ||
         !std::isnan(centerX) || !std::isnan(centerY);
}

static void DetectLocalizableCoordinates(xmlNodePtr root,
                                         std::vector<LintDiagnostic>& diagnostics) {
  std::vector<xmlNodePtr> allNodes = {};
  CollectAllNodes(root, allNodes);

  for (auto* node : allNodes) {
    if (!XmlNodeIs(node, "Layer")) {
      continue;
    }
    auto matrix = XmlAttr(node, "matrix");
    if (!matrix.empty() && matrix != "1,0,0,1,0,0") {
      continue;
    }
    auto composition = XmlAttr(node, "composition");
    if (!composition.empty()) {
      continue;
    }
    auto children = XmlChildElements(node);
    bool hasContents = false;
    for (auto* child : children) {
      if (IsContentNode(child)) {
        hasContents = true;
        break;
      }
    }
    if (!hasContents) {
      continue;
    }
    if (HasConstraintAttributes(node)) {
      continue;
    }

    bool childHasConstraints = false;
    for (auto* child : children) {
      if (IsContentNode(child) && HasConstraintAttributes(child)) {
        childHasConstraints = true;
        break;
      }
    }
    if (childHasConstraints) {
      continue;
    }

    float x = XmlAttrFloat(node, "x");
    float y = XmlAttrFloat(node, "y");
    if (std::isnan(x)) {
      x = 0;
    }
    if (std::isnan(y)) {
      y = 0;
    }
    if (x != 0 || y != 0) {
      LintDiagnostic diag = {};
      diag.line = static_cast<int>(node->line);
      diag.category = "info";
      diag.message = "Layer coordinates can be localized to origin";
      diagnostics.push_back(std::move(diag));
    }
  }
}

// --- Pass 10: PathData localization detection ---

static std::pair<float, float> ComputePathBoundsOrigin(const std::string& data) {
  auto pathData = PathDataFromSVGString(data);
  auto bounds = pathData.getBounds();
  return {bounds.x, bounds.y};
}

static void DetectLocalizablePathData(xmlNodePtr root, std::vector<LintDiagnostic>& diagnostics) {
  std::vector<xmlNodePtr> allNodes = {};
  CollectAllNodes(root, allNodes);

  // Collect all PathData IDs and their references.
  // If any referencing Path has a parent Group/Layer with rotation, skip the PathData.
  std::unordered_set<std::string> rotatedPathDataIds = {};
  for (auto* node : allNodes) {
    if (!XmlNodeIs(node, "Path")) {
      continue;
    }
    auto dataRef = XmlAttr(node, "data");
    if (dataRef.empty() || dataRef[0] != '@') {
      continue;
    }
    auto id = dataRef.substr(1);
    // Walk up ancestors to check for rotation on Group or Layer.
    for (xmlNodePtr parent = node->parent; parent != nullptr; parent = parent->parent) {
      if (parent->type != XML_ELEMENT_NODE) {
        continue;
      }
      auto rotation = XmlAttrFloat(parent, "rotation");
      if (!std::isnan(rotation) && rotation != 0.0f) {
        rotatedPathDataIds.insert(id);
        break;
      }
    }
  }

  for (auto* node : allNodes) {
    if (!XmlNodeIs(node, "PathData")) {
      continue;
    }
    auto data = XmlAttr(node, "data");
    if (data.empty()) {
      continue;
    }
    auto id = XmlAttr(node, "id");
    if (!id.empty() && rotatedPathDataIds.count(id) > 0) {
      continue;
    }
    auto boundsOrigin = ComputePathBoundsOrigin(data);
    if (std::abs(boundsOrigin.first) >= 0.5f || std::abs(boundsOrigin.second) >= 0.5f) {
      LintDiagnostic diag = {};
      diag.line = static_cast<int>(node->line);
      diag.category = "info";
      diag.message = "PathData can be localized to origin";
      diagnostics.push_back(std::move(diag));
    }
  }
}

// --- Pass 12: Extractable Composition detection ---

static std::string SerializeLayerContents(xmlNodePtr layer);

static std::string SerializeElement(xmlNodePtr node) {
  std::ostringstream oss;
  oss << XmlNodeName(node) << "{";

  for (xmlAttrPtr attr = node->properties; attr != nullptr; attr = attr->next) {
    std::string name(reinterpret_cast<const char*>(attr->name));
    if (name == "id" || name == "name") {
      continue;
    }
    auto value = XmlAttr(node, name.c_str());
    oss << name << "=" << value << ";";
  }

  // Include text content (e.g. CDATA in <Text>) so nodes with different text are not
  // considered structurally identical.
  for (xmlNodePtr child = node->children; child != nullptr; child = child->next) {
    if (child->type == XML_TEXT_NODE || child->type == XML_CDATA_SECTION_NODE) {
      auto* content = xmlNodeGetContent(child);
      if (content != nullptr) {
        oss << "TEXT=" << reinterpret_cast<const char*>(content) << ";";
        xmlFree(content);
      }
    }
  }

  auto children = XmlChildElements(node);
  for (auto* child : children) {
    oss << SerializeElement(child);
  }
  oss << "}";
  return oss.str();
}

static std::string SerializeLayerContents(xmlNodePtr layer) {
  std::ostringstream oss;
  auto children = XmlChildElements(layer);
  for (auto* child : children) {
    if (IsContentNode(child)) {
      oss << SerializeElement(child);
    } else if (XmlNodeIs(child, "Layer")) {
      oss << "Layer{" << SerializeLayerContents(child) << "}";
    }
  }
  return oss.str();
}

static bool IsExtractableCandidate(xmlNodePtr layer) {
  auto children = XmlChildElements(layer);
  bool hasContents = false;
  bool hasChildLayers = false;
  int contentCount = 0;
  for (auto* child : children) {
    if (IsContentNode(child)) {
      hasContents = true;
      contentCount++;
    }
    if (XmlNodeIs(child, "Layer")) {
      hasChildLayers = true;
    }
  }
  if (!hasContents || hasChildLayers) {
    return false;
  }
  // Require at least 3 content nodes — a single shape + painter is too trivial to extract.
  if (contentCount < 3) {
    return false;
  }

  auto composition = XmlAttr(layer, "composition");
  if (!composition.empty()) {
    return false;
  }
  auto matrix = XmlAttr(layer, "matrix");
  if (!matrix.empty() && matrix != "1,0,0,1,0,0") {
    return false;
  }

  auto right = XmlAttrFloat(layer, "right");
  auto bottom = XmlAttrFloat(layer, "bottom");
  auto centerX = XmlAttrFloat(layer, "centerX");
  auto centerY = XmlAttrFloat(layer, "centerY");
  auto flex = XmlAttrFloat(layer, "flex");
  if (!std::isnan(right) || !std::isnan(bottom) || !std::isnan(centerX) || !std::isnan(centerY)) {
    return false;
  }
  if (!std::isnan(flex) && flex != 0) {
    return false;
  }
  return true;
}

static void CollectExtractableLayers(xmlNodePtr node, std::vector<xmlNodePtr>& candidates) {
  if (XmlNodeIs(node, "Layer") && IsExtractableCandidate(node)) {
    candidates.push_back(node);
  }
  auto children = XmlChildElements(node);
  for (auto* child : children) {
    if (XmlNodeIs(child, "Layer")) {
      CollectExtractableLayers(child, candidates);
    }
  }
}

static void DetectExtractableCompositions(xmlNodePtr root,
                                          std::vector<LintDiagnostic>& diagnostics) {
  std::vector<xmlNodePtr> candidates = {};
  auto children = XmlChildElements(root);
  for (auto* child : children) {
    if (XmlNodeIs(child, "Layer")) {
      CollectExtractableLayers(child, candidates);
    }
  }

  std::unordered_map<std::string, std::vector<xmlNodePtr>> groups = {};
  for (auto* layer : candidates) {
    auto signature = SerializeLayerContents(layer);
    groups[signature].push_back(layer);
  }

  for (const auto& pair : groups) {
    if (pair.second.size() < 2) {
      continue;
    }
    int firstLine = static_cast<int>(pair.second[0]->line);
    for (size_t i = 1; i < pair.second.size(); i++) {
      LintDiagnostic diag = {};
      diag.line = static_cast<int>(pair.second[i]->line);
      diag.category = "info";
      diag.message = "structurally identical to Layer at line " + std::to_string(firstLine) +
                     ", can extract to shared Composition";
      diagnostics.push_back(std::move(diag));
    }
  }
}

// --- Lint hints (performance warnings) ---

struct RepeaterContext {
  float productSoFar = 1.0f;
  int repeaterCount = 0;
  bool hasRepeater = false;
};

static bool IsHighCostNode(xmlNodePtr node) {
  auto name = XmlNodeName(node);
  return name == "Repeater" || name == "BlurFilter" || name == "DropShadowStyle" ||
         name == "BackgroundBlurStyle" || name == "InnerShadowStyle";
}

static bool HasHighCostChildren(xmlNodePtr node);

static bool CheckHighCostRecursive(const std::vector<xmlNodePtr>& children) {
  for (auto* child : children) {
    if (IsHighCostNode(child)) {
      return true;
    }
    if (HasHighCostChildren(child)) {
      return true;
    }
  }
  return false;
}

static bool HasHighCostChildren(xmlNodePtr node) {
  return CheckHighCostRecursive(XmlChildElements(node));
}

static void CollectLintHintsFromContents(xmlNodePtr layer, const std::string& displayName,
                                         RepeaterContext ctx,
                                         std::vector<LintDiagnostic>& diagnostics);

static void CollectLintHintsFromElements(const std::vector<xmlNodePtr>& elements,
                                         const std::string& displayName, RepeaterContext ctx,
                                         std::vector<LintDiagnostic>& diagnostics) {
  float localProduct = ctx.productSoFar;
  int localRepeaterCount = ctx.repeaterCount;

  for (auto* element : elements) {
    if (XmlNodeIs(element, "Repeater")) {
      float copies = XmlAttrFloat(element, "copies");
      if (!std::isnan(copies)) {
        localProduct *= copies;
        localRepeaterCount++;
      }
    }
  }

  bool hasRepeater = localProduct > ctx.productSoFar;
  bool hasStrokeWithDashes = false;

  // Collect the product of Repeaters that appear AFTER each element index. A Repeater only
  // affects elements that precede it in the same scope. So a Group at index i is affected by
  // Repeaters at indices > i. We compute a suffix product for each position.
  std::vector<float> suffixProduct(elements.size() + 1, 1.0f);
  std::vector<int> suffixRepeaterCount(elements.size() + 1, 0);
  for (int i = static_cast<int>(elements.size()) - 1; i >= 0; --i) {
    suffixProduct[i] = suffixProduct[i + 1];
    suffixRepeaterCount[i] = suffixRepeaterCount[i + 1];
    if (XmlNodeIs(elements[i], "Repeater")) {
      float copies = XmlAttrFloat(elements[i], "copies");
      if (!std::isnan(copies)) {
        suffixProduct[i] *= copies;
        suffixRepeaterCount[i]++;
      }
    }
  }

  for (size_t idx = 0; idx < elements.size(); ++idx) {
    auto* element = elements[idx];
    auto nodeName = XmlNodeName(element);

    if (nodeName == "Repeater") {
      float copies = XmlAttrFloat(element, "copies");
      if (!std::isnan(copies) && copies > 200.0f) {
        LintDiagnostic diag = {};
        diag.line = static_cast<int>(element->line);
        diag.category = "warning";
        diag.message = displayName + ": Repeater with high copies count (" +
                       std::to_string(static_cast<int>(copies)) + ") may cause performance issues";
        diagnostics.push_back(std::move(diag));
      }
      if (localProduct > 500.0f && (ctx.productSoFar > 1.0f || localRepeaterCount > 1)) {
        LintDiagnostic diag = {};
        diag.line = static_cast<int>(element->line);
        diag.category = "warning";
        diag.message = displayName + ": Nested Repeater with product " +
                       std::to_string(static_cast<int>(localProduct)) +
                       " (> 500) may cause performance issues";
        diagnostics.push_back(std::move(diag));
      }
    }

    if (nodeName == "Stroke") {
      auto align = XmlAttr(element, "align");
      if (hasRepeater && !align.empty() && align != "center") {
        LintDiagnostic diag = {};
        diag.line = static_cast<int>(element->line);
        diag.category = "warning";
        std::string alignStr = (align == "inside") ? "Inside" : "Outside";
        diag.message = displayName + ": Stroke with non-center alignment (" + alignStr +
                       ") in Repeater scope requires expensive CPU path operations";
        diagnostics.push_back(std::move(diag));
      }
      auto dashes = XmlAttr(element, "dashes");
      if (!dashes.empty()) {
        hasStrokeWithDashes = true;
      }
    }

    if (nodeName == "Path") {
      auto dataRef = XmlAttr(element, "data");
      if (!dataRef.empty() && dataRef[0] == '@') {
        auto root = element->doc != nullptr ? xmlDocGetRootElement(element->doc) : nullptr;
        if (root != nullptr) {
          auto* pathDataNode = FindNodeById(root, dataRef.substr(1));
          if (pathDataNode != nullptr) {
            auto data = XmlAttr(pathDataNode, "data");
            auto verbs = ParseVerbs(data);
            if (verbs.size() > 500) {
              LintDiagnostic diag = {};
              diag.line = static_cast<int>(element->line);
              diag.category = "warning";
              diag.message = displayName + ": Path with high complexity (" +
                             std::to_string(verbs.size()) +
                             " verbs) may cause rendering performance issues";
              diagnostics.push_back(std::move(diag));
            }
          }
        }
      }
    }

    if (nodeName == "Group" || nodeName == "TextBox") {
      // A Group at position idx is affected by Repeaters that appear AFTER it (suffix product).
      // Combine with the incoming context product.
      float groupProduct = ctx.productSoFar * suffixProduct[idx + 1];
      int groupRepeaterCount = ctx.repeaterCount + suffixRepeaterCount[idx + 1];
      RepeaterContext childCtx = {};
      childCtx.productSoFar = groupProduct;
      childCtx.repeaterCount = groupRepeaterCount;
      childCtx.hasRepeater = groupProduct > ctx.productSoFar;
      CollectLintHintsFromElements(XmlChildElements(element), displayName, childCtx, diagnostics);
    }
  }

  if (hasRepeater && hasStrokeWithDashes) {
    LintDiagnostic diag = {};
    diag.category = "warning";
    diag.message = displayName +
                   ": Dashed Stroke within Repeater scope may cause performance "
                   "issues due to complex rendering calculations";
    diagnostics.push_back(std::move(diag));
  }
}

static void CollectLintHintsFromContents(xmlNodePtr layer, const std::string& displayName,
                                         RepeaterContext ctx,
                                         std::vector<LintDiagnostic>& diagnostics) {
  auto children = XmlChildElements(layer);
  std::vector<xmlNodePtr> contentChildren = {};
  for (auto* child : children) {
    if (IsContentNode(child)) {
      contentChildren.push_back(child);
    }
  }
  CollectLintHintsFromElements(contentChildren, displayName, ctx, diagnostics);
}

static void CollectBlurHints(xmlNodePtr layer, const std::string& displayName,
                             std::vector<LintDiagnostic>& diagnostics) {
  auto children = XmlChildElements(layer);
  for (auto* child : children) {
    auto nodeName = XmlNodeName(child);

    if (nodeName == "BlurFilter") {
      float blurX = XmlAttrFloat(child, "blurX");
      float blurY = XmlAttrFloat(child, "blurY");
      if (std::isnan(blurX)) {
        blurX = 0;
      }
      if (std::isnan(blurY)) {
        blurY = 0;
      }
      if (blurX > 256.0f || blurY > 256.0f) {
        LintDiagnostic diag = {};
        diag.line = static_cast<int>(child->line);
        diag.category = "warning";
        diag.message =
            displayName +
            ": BlurFilter with high blur radius (X:" + std::to_string(static_cast<int>(blurX)) +
            " Y:" + std::to_string(static_cast<int>(blurY)) + ") may cause performance issues";
        diagnostics.push_back(std::move(diag));
      }
    }

    if (nodeName == "DropShadowStyle") {
      float blurX = XmlAttrFloat(child, "blurX");
      float blurY = XmlAttrFloat(child, "blurY");
      if (std::isnan(blurX)) {
        blurX = 0;
      }
      if (std::isnan(blurY)) {
        blurY = 0;
      }
      if (blurX > 256.0f || blurY > 256.0f) {
        LintDiagnostic diag = {};
        diag.line = static_cast<int>(child->line);
        diag.category = "warning";
        diag.message = displayName + ": DropShadowStyle with high blur radius (X:" +
                       std::to_string(static_cast<int>(blurX)) +
                       " Y:" + std::to_string(static_cast<int>(blurY)) +
                       ") may cause performance issues";
        diagnostics.push_back(std::move(diag));
      }
    }

    if (nodeName == "BackgroundBlurStyle") {
      float blurX = XmlAttrFloat(child, "blurX");
      float blurY = XmlAttrFloat(child, "blurY");
      if (std::isnan(blurX)) {
        blurX = 0;
      }
      if (std::isnan(blurY)) {
        blurY = 0;
      }
      if (blurX > 256.0f || blurY > 256.0f) {
        LintDiagnostic diag = {};
        diag.line = static_cast<int>(child->line);
        diag.category = "warning";
        diag.message = displayName + ": BackgroundBlurStyle with high blur radius (X:" +
                       std::to_string(static_cast<int>(blurX)) +
                       " Y:" + std::to_string(static_cast<int>(blurY)) +
                       ") may cause performance issues";
        diagnostics.push_back(std::move(diag));
      }
    }

    if (nodeName == "InnerShadowStyle") {
      float blurX = XmlAttrFloat(child, "blurX");
      float blurY = XmlAttrFloat(child, "blurY");
      if (std::isnan(blurX)) {
        blurX = 0;
      }
      if (std::isnan(blurY)) {
        blurY = 0;
      }
      if (blurX > 256.0f || blurY > 256.0f) {
        LintDiagnostic diag = {};
        diag.line = static_cast<int>(child->line);
        diag.category = "warning";
        diag.message = displayName + ": InnerShadowStyle with high blur radius (X:" +
                       std::to_string(static_cast<int>(blurX)) +
                       " Y:" + std::to_string(static_cast<int>(blurY)) +
                       ") may cause performance issues";
        diagnostics.push_back(std::move(diag));
      }
    }
  }
}

static bool CanDowngradeLayerToGroup(xmlNodePtr layer) {
  auto children = XmlChildElements(layer);
  for (auto* child : children) {
    auto name = XmlNodeName(child);
    if (name == "Layer" || name == "DropShadowStyle" || name == "InnerShadowStyle" ||
        name == "BackgroundBlurStyle" || name == "BlurFilter") {
      return false;
    }
  }

  auto mask = XmlAttr(layer, "mask");
  auto composition = XmlAttr(layer, "composition");
  auto blendMode = XmlAttr(layer, "blendMode");
  auto visible = XmlAttr(layer, "visible");
  auto hasScrollRect = XmlAttr(layer, "hasScrollRect");
  auto matrix = XmlAttr(layer, "matrix");
  auto matrix3D = XmlAttr(layer, "matrix3D");
  auto preserve3D = XmlAttr(layer, "preserve3D");
  auto groupOpacity = XmlAttr(layer, "groupOpacity");
  auto passThroughBackground = XmlAttr(layer, "passThroughBackground");
  auto antiAlias = XmlAttr(layer, "antiAlias");
  auto id = XmlAttr(layer, "id");
  auto name = XmlAttr(layer, "name");
  auto customData = XmlAttr(layer, "customData");
  auto layout = XmlAttr(layer, "layout");
  auto flex = XmlAttrFloat(layer, "flex");
  auto width = XmlAttrFloat(layer, "width");
  auto height = XmlAttrFloat(layer, "height");
  auto includeInLayout = XmlAttr(layer, "includeInLayout");

  if (!mask.empty()) {
    return false;
  }
  if (!composition.empty()) {
    return false;
  }
  if (!blendMode.empty() && blendMode != "normal") {
    return false;
  }
  if (!visible.empty() && visible != "true") {
    return false;
  }
  if (!hasScrollRect.empty() && hasScrollRect != "false") {
    return false;
  }
  if (!matrix.empty() && matrix != "1,0,0,1,0,0") {
    return false;
  }
  if (!matrix3D.empty() && matrix3D != "1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1") {
    return false;
  }
  if (!preserve3D.empty() && preserve3D != "false") {
    return false;
  }
  if (!groupOpacity.empty() && groupOpacity != "true") {
    return false;
  }
  if (!passThroughBackground.empty() && passThroughBackground != "true") {
    return false;
  }
  if (!antiAlias.empty() && antiAlias != "true") {
    return false;
  }
  if (!id.empty() || !name.empty()) {
    return false;
  }
  if (!customData.empty()) {
    return false;
  }
  if (!layout.empty() && layout != "none") {
    return false;
  }
  if (!std::isnan(flex) && flex != 0) {
    return false;
  }
  if (!std::isnan(width) || !std::isnan(height)) {
    return false;
  }
  if (HasConstraintAttributes(layer)) {
    return false;
  }
  if (!includeInLayout.empty() && includeInLayout != "true") {
    return false;
  }
  return true;
}

static bool IsMaskSimpleRectangle(xmlNodePtr root, const std::string& maskId) {
  auto* maskNode = FindNodeById(root, maskId);
  if (maskNode == nullptr || !XmlNodeIs(maskNode, "Layer")) {
    return false;
  }

  auto children = XmlChildElements(maskNode);
  if (children.size() != 2) {
    return false;
  }
  if (!XmlNodeIs(children[0], "Rectangle")) {
    return false;
  }
  if (!XmlNodeIs(children[1], "Fill")) {
    return false;
  }

  auto fillNode = children[1];
  auto fillChildren = XmlChildElements(fillNode);
  bool hasSolidColor = false;
  for (auto* fc : fillChildren) {
    if (XmlNodeIs(fc, "SolidColor")) {
      hasSolidColor = true;
      break;
    }
  }
  auto colorAttr = XmlAttr(fillNode, "color");
  if (!hasSolidColor && colorAttr.empty()) {
    return false;
  }
  auto fillAlpha = XmlAttrFloat(fillNode, "alpha");
  if (!std::isnan(fillAlpha) && fillAlpha < 0.999f) {
    return false;
  }

  auto maskMatrix = XmlAttr(maskNode, "matrix");
  if (!maskMatrix.empty() && maskMatrix != "1,0,0,1,0,0") {
    return false;
  }
  auto maskMatrix3D = XmlAttr(maskNode, "matrix3D");
  if (!maskMatrix3D.empty() && maskMatrix3D != "1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1") {
    return false;
  }

  auto blendMode = XmlAttr(maskNode, "blendMode");
  if (!blendMode.empty() && blendMode != "normal") {
    return false;
  }

  return true;
}

static void CollectLintHints(xmlNodePtr layer, xmlNodePtr root, bool isRoot, bool parentHasLayout,
                             std::vector<LintDiagnostic>& diagnostics) {
  auto displayName = GetNodeDisplayName(layer);

  if (!isRoot && !parentHasLayout) {
    auto includeInLayout = XmlAttr(layer, "includeInLayout");
    if (!includeInLayout.empty() && includeInLayout != "true") {
      LintDiagnostic diag = {};
      diag.line = static_cast<int>(layer->line);
      diag.category = "warning";
      diag.message =
          displayName + ": includeInLayout=false has no effect, parent has no container layout";
      diagnostics.push_back(std::move(diag));
    }

    auto flex = XmlAttrFloat(layer, "flex");
    if (!std::isnan(flex) && flex > 0) {
      LintDiagnostic diag = {};
      diag.line = static_cast<int>(layer->line);
      diag.category = "warning";
      diag.message = displayName + ": flex has no effect, parent has no container layout";
      diagnostics.push_back(std::move(diag));
    }
  }

  auto children = XmlChildElements(layer);
  std::vector<xmlNodePtr> childLayers = {};
  for (auto* child : children) {
    if (XmlNodeIs(child, "Layer")) {
      childLayers.push_back(child);
    }
  }

  auto layout = XmlAttr(layer, "layout");
  bool thisHasLayout = !layout.empty() && layout != "none";

  if (!isRoot && !childLayers.empty() && !thisHasLayout) {
    bool allDowngradable = true;
    for (auto* childLayer : childLayers) {
      if (!CanDowngradeLayerToGroup(childLayer)) {
        allDowngradable = false;
        break;
      }
    }
    if (allDowngradable) {
      LintDiagnostic diag = {};
      diag.line = static_cast<int>(layer->line);
      diag.category = "warning";
      diag.message = displayName + ": " + std::to_string(childLayers.size()) +
                     " child Layer(s) use no Layer-exclusive "
                     "features and could be downgraded to Groups";
      diagnostics.push_back(std::move(diag));
    }
  }

  RepeaterContext ctx = {};
  ctx.productSoFar = 1.0f;
  ctx.repeaterCount = 0;
  ctx.hasRepeater = false;
  CollectLintHintsFromContents(layer, displayName, ctx, diagnostics);
  CollectBlurHints(layer, displayName, diagnostics);

  auto alpha = XmlAttrFloat(layer, "alpha");
  if (!std::isnan(alpha) && alpha < 0.05f && alpha > 0.0f && HasHighCostChildren(layer)) {
    LintDiagnostic diag = {};
    diag.line = static_cast<int>(layer->line);
    diag.category = "warning";
    diag.message = displayName + ": Low opacity (" + std::to_string(static_cast<int>(alpha * 100)) +
                   "%) with high-cost elements (Repeater/Blur) may indicate unnecessary rendering";
    diagnostics.push_back(std::move(diag));
  }

  auto maskRef = XmlAttr(layer, "mask");
  auto maskType = XmlAttr(layer, "maskType");
  if (!maskRef.empty() && maskRef[0] == '@') {
    std::string maskId = maskRef.substr(1);
    if ((maskType.empty() || maskType == "alpha") && IsMaskSimpleRectangle(root, maskId)) {
      LintDiagnostic diag = {};
      diag.line = static_cast<int>(layer->line);
      diag.category = "warning";
      diag.message =
          displayName +
          ": Using rectangular mask could be replaced with clipToBounds for better performance";
      diagnostics.push_back(std::move(diag));
    }
  }

  for (auto* childLayer : childLayers) {
    CollectLintHints(childLayer, root, false, thisHasLayout, diagnostics);
  }
}

static void CollectAllLintHints(xmlNodePtr root, std::vector<LintDiagnostic>& diagnostics) {
  auto children = XmlChildElements(root);
  for (auto* child : children) {
    if (XmlNodeIs(child, "Layer")) {
      CollectLintHints(child, root, true, false, diagnostics);
    }
  }
}

// --- Output formatting ---

static void PrintDiagnosticsText(const std::vector<LintDiagnostic>& diagnostics,
                                 const std::string& file) {
  for (const auto& diag : diagnostics) {
    if (diag.line > 0) {
      std::cerr << file << ":" << diag.line << ": " << diag.message << "\n";
    } else {
      std::cerr << file << ": " << diag.message << "\n";
    }
  }
}

static void PrintDiagnosticsJson(const std::vector<LintDiagnostic>& diagnostics,
                                 const std::string& file) {
  std::cout << "{\n";
  std::cout << "  \"file\": \"" << EscapeJson(file) << "\",\n";
  std::cout << "  \"ok\": " << (diagnostics.empty() ? "true" : "false") << ",\n";
  std::cout << "  \"diagnostics\": [";
  for (size_t i = 0; i < diagnostics.size(); ++i) {
    if (i > 0) {
      std::cout << ",";
    }
    std::cout << "\n    {\"line\": " << diagnostics[i].line << ", \"category\": \""
              << EscapeJson(diagnostics[i].category) << "\", \"message\": \""
              << EscapeJson(diagnostics[i].message) << "\"}";
  }
  if (!diagnostics.empty()) {
    std::cout << "\n  ";
  }
  std::cout << "]\n";
  std::cout << "}\n";
}

static void PrintUsage() {
  std::cout << "Usage: pagx lint [options] <file.pagx>\n"
            << "\n"
            << "Check a PAGX file for errors, structural issues, and optimization opportunities.\n"
            << "This command performs validation and detection without modifying the file.\n"
            << "\n"
            << "Options:\n"
            << "  --json                 Output in JSON format\n"
            << "  -h, --help             Show this help message\n";
}

int RunLint(int argc, char* argv[]) {
  bool jsonOutput = false;
  std::string filePath = {};

  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--json") == 0) {
      jsonOutput = true;
    } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
      PrintUsage();
      return 0;
    } else if (argv[i][0] == '-') {
      std::cerr << "pagx lint: unknown option '" << argv[i] << "'\n";
      return 1;
    } else {
      filePath = argv[i];
    }
  }

  if (filePath.empty()) {
    std::cerr << "pagx lint: missing input file\n";
    return 1;
  }

  std::vector<LintDiagnostic> diagnostics = {};

  xmlDocPtr doc = xmlReadFile(filePath.c_str(), nullptr, XML_PARSE_NONET);
  if (doc == nullptr) {
    LintDiagnostic diag = {};
    diag.category = "error";
    diag.message = "Failed to parse XML document";
    diagnostics.push_back(std::move(diag));
    if (jsonOutput) {
      PrintDiagnosticsJson(diagnostics, filePath);
    } else {
      PrintDiagnosticsText(diagnostics, filePath);
    }
    return 1;
  }

  bool xsdValid = ValidateXsd(doc, diagnostics);
  if (xsdValid) {
    CollectSemanticErrors(filePath, diagnostics);
  }

  bool hasErrors = false;
  for (const auto& diag : diagnostics) {
    if (diag.category == "error") {
      hasErrors = true;
      break;
    }
  }

  if (!hasErrors) {
    xmlNodePtr root = XmlFindRoot(doc);

    float canvasWidth = XmlAttrFloat(root, "width");
    float canvasHeight = XmlAttrFloat(root, "height");
    if (std::isnan(canvasWidth)) {
      canvasWidth = 0;
    }
    if (std::isnan(canvasHeight)) {
      canvasHeight = 0;
    }

    DetectEmptyNodes(root, diagnostics);
    DetectFullCanvasClipMasks(root, canvasWidth, canvasHeight, diagnostics);
    DetectUnreferencedResources(root, diagnostics);
    DetectDuplicatePathData(root, diagnostics);
    DetectDuplicateGradients(root, diagnostics);
    DetectMergeableGroups(root, diagnostics);
    DetectUnwrappableFirstChildGroups(root, diagnostics);
    DetectPathsToPrimitives(root, diagnostics);
    DetectLocalizableCoordinates(root, diagnostics);
    DetectLocalizablePathData(root, diagnostics);
    DetectExtractableCompositions(root, diagnostics);
    CollectAllLintHints(root, diagnostics);
  }

  xmlFreeDoc(doc);

  if (jsonOutput) {
    PrintDiagnosticsJson(diagnostics, filePath);
  } else if (!diagnostics.empty()) {
    PrintDiagnosticsText(diagnostics, filePath);
  } else {
    std::cout << filePath << ": ok\n";
  }

  for (const auto& diag : diagnostics) {
    if (diag.category == "error" || diag.category == "warning") {
      return 1;
    }
  }
  return 0;
}

}  // namespace pagx::cli
