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

#include "cli/CommandVerify.h"
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlschemas.h>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include "cli/CliUtils.h"
#include "cli/CommandLayout.h"
#include "cli/CommandRender.h"
#include "cli/CommandResolve.h"
#include "cli/FormatUtils.h"
#include "cli/LayoutUtils.h"
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/BackgroundBlurStyle.h"
#include "pagx/nodes/BlurFilter.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/DropShadowStyle.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Font.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/InnerShadowStyle.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/LayoutNode.h"
#include "pagx/nodes/LinearGradient.h"
#include "pagx/nodes/MergePath.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/PathData.h"
#include "pagx/nodes/Polystar.h"
#include "pagx/nodes/RadialGradient.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/Repeater.h"
#include "pagx/nodes/RoundCorner.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextBox.h"
#include "pagx/nodes/TrimPath.h"
#include "pagx_xsd.h"
#include "tgfx/core/ImageCodec.h"
#include "tgfx/core/Pixmap.h"

namespace pagx::cli {

// ============================================================================
// Data Structures
// ============================================================================

struct VerifyDiagnostic {
  std::vector<int> lines;
  std::string message;
};

struct VerifyOptions {
  std::string inputFile;
  std::string id;
  float scale = 1.0f;
  bool skipRender = false;
  bool skipLayout = false;
  bool jsonOutput = false;
};

// ============================================================================
// Helper Functions
// ============================================================================

static bool IsGradient(NodeType type) {
  return type == NodeType::LinearGradient || type == NodeType::RadialGradient ||
         type == NodeType::ConicGradient || type == NodeType::DiamondGradient;
}

static void AddDiagnostic(std::vector<VerifyDiagnostic>& diagnostics, int line,
                          const std::string& message) {
  VerifyDiagnostic diag;
  diag.lines.push_back(line);
  diag.message = message;
  diagnostics.push_back(std::move(diag));
}

static void AddDiagnostic(std::vector<VerifyDiagnostic>& diagnostics, const std::vector<int>& lines,
                          const std::string& message) {
  VerifyDiagnostic diag;
  diag.lines = lines;
  diag.message = message;
  diagnostics.push_back(std::move(diag));
}

static std::string FormatBounds(float x, float y, float w, float h) {
  std::ostringstream oss;
  oss << "(" << static_cast<int>(x) << "," << static_cast<int>(y) << "," << static_cast<int>(w)
      << "," << static_cast<int>(h) << ")";
  return oss.str();
}

// ============================================================================
// Static Detection: Empty Nodes
// ============================================================================

static void DetectEmptyLayer(const Layer* layer, bool parentHasLayout,
                             std::vector<VerifyDiagnostic>& diagnostics) {
  if (!layer->contents.empty() || !layer->children.empty() || layer->composition != nullptr) {
    return;
  }
  if (!std::isnan(layer->width) || !std::isnan(layer->height)) {
    return;
  }
  if (!std::isnan(layer->percentWidth) || !std::isnan(layer->percentHeight)) {
    return;
  }
  bool hasSizeDependentConstraint = (!std::isnan(layer->right) || !std::isnan(layer->bottom) ||
                                     !std::isnan(layer->centerX) || !std::isnan(layer->centerY));
  if (hasSizeDependentConstraint) {
    return;
  }
  bool inParentLayout = parentHasLayout && layer->includeInLayout;
  if (inParentLayout) {
    return;
  }
  AddDiagnostic(diagnostics, layer->sourceLine,
                "empty Layer with no content, children, or composition. "
                "Fix: check if this Layer is needed");
}

static void DetectEmptyGroup(const Group* group, std::vector<VerifyDiagnostic>& diagnostics) {
  if (!group->elements.empty()) {
    return;
  }
  if (!std::isnan(group->width) || !std::isnan(group->height)) {
    return;
  }
  if (!std::isnan(group->percentWidth) || !std::isnan(group->percentHeight)) {
    return;
  }
  bool hasSizeDependentConstraint = (!std::isnan(group->right) || !std::isnan(group->bottom) ||
                                     !std::isnan(group->centerX) || !std::isnan(group->centerY));
  if (hasSizeDependentConstraint) {
    return;
  }
  const char* typeName = group->nodeType() == NodeType::TextBox ? "TextBox" : "Group";
  AddDiagnostic(diagnostics, group->sourceLine,
                std::string("empty ") + typeName +
                    " with no elements. Fix: check if this container is needed");
}

static void DetectZeroStrokeWidth(const Stroke* stroke,
                                  std::vector<VerifyDiagnostic>& diagnostics) {
  if (stroke->width <= 0.0f) {
    AddDiagnostic(
        diagnostics, stroke->sourceLine,
        "Stroke width <= 0 produces no visual output. Fix: check if this Stroke is needed");
  }
}

// ============================================================================
// Static Detection: Full Canvas Clip Mask
// ============================================================================

static void DetectFullCanvasClipMask(const Layer* layer, float canvasWidth, float canvasHeight,
                                     std::vector<VerifyDiagnostic>& diagnostics) {
  if (layer->mask == nullptr) {
    return;
  }
  auto* maskLayer = layer->mask;
  if (maskLayer->x != 0 || maskLayer->y != 0) {
    return;
  }
  if (!maskLayer->matrix.isIdentity()) {
    return;
  }
  if (maskLayer->contents.size() != 1) {
    return;
  }
  if (maskLayer->contents[0]->nodeType() != NodeType::Rectangle) {
    return;
  }
  auto* rect = static_cast<Rectangle*>(maskLayer->contents[0]);
  float left = rect->position.x - rect->size.width * 0.5f;
  float top = rect->position.y - rect->size.height * 0.5f;
  float right = rect->position.x + rect->size.width * 0.5f;
  float bottom = rect->position.y + rect->size.height * 0.5f;
  if (left <= 0 && top <= 0 && right >= canvasWidth && bottom >= canvasHeight) {
    AddDiagnostic(
        diagnostics, layer->sourceLine,
        "mask covers entire canvas, no clipping effect. Fix: check if this mask is needed");
  }
}

// ============================================================================
// Static Detection: Unreferenced Resources
// ============================================================================

static void CollectReferencedIds(const PAGXDocument* doc, std::unordered_set<std::string>& refs);

static void CollectRefsFromLayer(const Layer* layer, std::unordered_set<std::string>& refs);
static void CollectRefsFromElement(const Element* element, std::unordered_set<std::string>& refs);

static void CollectRefsFromElement(const Element* element, std::unordered_set<std::string>& refs) {
  if (element == nullptr) {
    return;
  }
  if (!element->id.empty()) {
    refs.insert(element->id);
  }

  auto type = element->nodeType();
  if (type == NodeType::Group || type == NodeType::TextBox) {
    auto* group = static_cast<const Group*>(element);
    for (auto* child : group->elements) {
      CollectRefsFromElement(child, refs);
    }
  } else if (type == NodeType::Path) {
    auto* path = static_cast<const Path*>(element);
    if (path->data != nullptr && !path->data->id.empty()) {
      refs.insert(path->data->id);
    }
  } else if (type == NodeType::Fill) {
    auto* fill = static_cast<const Fill*>(element);
    if (fill->color != nullptr) {
      if (!fill->color->id.empty()) {
        refs.insert(fill->color->id);
      }
      if (fill->color->nodeType() == NodeType::ImagePattern) {
        auto* pattern = static_cast<const ImagePattern*>(fill->color);
        if (pattern->image != nullptr && !pattern->image->id.empty()) {
          refs.insert(pattern->image->id);
        }
      }
    }
  } else if (type == NodeType::Stroke) {
    auto* stroke = static_cast<const Stroke*>(element);
    if (stroke->color != nullptr) {
      if (!stroke->color->id.empty()) {
        refs.insert(stroke->color->id);
      }
      if (stroke->color->nodeType() == NodeType::ImagePattern) {
        auto* pattern = static_cast<const ImagePattern*>(stroke->color);
        if (pattern->image != nullptr && !pattern->image->id.empty()) {
          refs.insert(pattern->image->id);
        }
      }
    }
  } else if (type == NodeType::Text) {
    auto* text = static_cast<const Text*>(element);
    for (auto* glyphRun : text->glyphRuns) {
      if (glyphRun != nullptr && glyphRun->font != nullptr && !glyphRun->font->id.empty()) {
        refs.insert(glyphRun->font->id);
      }
    }
  }
}

static void CollectRefsFromLayer(const Layer* layer, std::unordered_set<std::string>& refs) {
  if (layer == nullptr) {
    return;
  }
  if (!layer->id.empty()) {
    refs.insert(layer->id);
  }
  if (layer->mask != nullptr) {
    CollectRefsFromLayer(layer->mask, refs);
  }
  if (layer->composition != nullptr && !layer->composition->id.empty()) {
    refs.insert(layer->composition->id);
  }
  for (auto* element : layer->contents) {
    CollectRefsFromElement(element, refs);
  }
  for (auto* child : layer->children) {
    CollectRefsFromLayer(child, refs);
  }
}

static void CollectReferencedIds(const PAGXDocument* doc, std::unordered_set<std::string>& refs) {
  for (auto* layer : doc->layers) {
    CollectRefsFromLayer(layer, refs);
  }
  for (auto& node : doc->nodes) {
    if (node->nodeType() == NodeType::Composition) {
      auto* comp = static_cast<const Composition*>(node.get());
      for (auto* layer : comp->layers) {
        CollectRefsFromLayer(layer, refs);
      }
    }
  }
}

static void DetectUnreferencedResources(const PAGXDocument* doc,
                                        std::vector<VerifyDiagnostic>& diagnostics) {
  std::unordered_set<std::string> refs;
  CollectReferencedIds(doc, refs);

  for (const auto& nodePtr : doc->nodes) {
    auto* node = nodePtr.get();
    if (node->id.empty()) {
      continue;
    }
    auto type = node->nodeType();
    if (type == NodeType::Layer || type == NodeType::Document) {
      continue;
    }
    if (refs.find(node->id) == refs.end()) {
      AddDiagnostic(diagnostics, node->sourceLine,
                    std::string("unreferenced resource <") + NodeTypeName(type) + "> id=\"" +
                        node->id +
                        "\". Fix: check if this resource should be "
                        "referenced or removed");
    }
  }
}

// ============================================================================
// XML DOM Signature Infrastructure
// ============================================================================

using LineNodeMap = std::unordered_multimap<int, xmlNodePtr>;

static void BuildLineNodeMap(xmlNodePtr node, LineNodeMap& map) {
  for (auto cur = node; cur != nullptr; cur = cur->next) {
    if (cur->type == XML_ELEMENT_NODE) {
      map.insert({static_cast<int>(cur->line), cur});
      BuildLineNodeMap(cur->children, map);
    }
  }
}

// ============================================================================
// XSD Schema Validation
// ============================================================================

static void XsdValidationCallback(void* ctx, const xmlError* error) {
  if (error == nullptr || error->message == nullptr) {
    return;
  }
  // Skip errors about "data-*" attributes — PAGX allows custom data attributes on any element
  // but XSD cannot express prefix-based wildcard attributes.
  std::string msg(error->message);
  if (msg.find("data-") != std::string::npos && msg.find("attribute") != std::string::npos) {
    return;
  }
  // Trim trailing newline.
  while (!msg.empty() && msg.back() == '\n') {
    msg.pop_back();
  }
  auto* diags = static_cast<std::vector<VerifyDiagnostic>*>(ctx);
  int line = error->line > 0 ? error->line : 0;
  AddDiagnostic(*diags, line, msg);
}

static void RunXsdValidation(xmlDocPtr xmlDoc, std::vector<VerifyDiagnostic>& diagnostics) {
  auto& xsdContent = PagxXsdContent();
  auto* parserCtxt =
      xmlSchemaNewMemParserCtxt(xsdContent.data(), static_cast<int>(xsdContent.size()));
  if (parserCtxt == nullptr) {
    return;
  }
  auto* schema = xmlSchemaParse(parserCtxt);
  xmlSchemaFreeParserCtxt(parserCtxt);
  if (schema == nullptr) {
    return;
  }
  auto* validCtxt = xmlSchemaNewValidCtxt(schema);
  if (validCtxt == nullptr) {
    xmlSchemaFree(schema);
    return;
  }
  xmlSchemaSetValidStructuredErrors(validCtxt, XsdValidationCallback, &diagnostics);
  xmlSchemaValidateDoc(validCtxt, xmlDoc);
  xmlSchemaFreeValidCtxt(validCtxt);
  xmlSchemaFree(schema);
}

static xmlNodePtr FindXmlNode(const LineNodeMap& map, int sourceLine, const char* elementName) {
  auto range = map.equal_range(sourceLine);
  for (auto it = range.first; it != range.second; ++it) {
    if (xmlStrcmp(it->second->name, reinterpret_cast<const xmlChar*>(elementName)) == 0) {
      return it->second;
    }
  }
  return nullptr;
}

static void StripAttribute(xmlNodePtr node, const char* attrName) {
  auto* attr = xmlHasProp(node, reinterpret_cast<const xmlChar*>(attrName));
  if (attr != nullptr) {
    xmlRemoveProp(attr);
  }
}

static void StripDataAttributes(xmlNodePtr node) {
  auto* attr = node->properties;
  while (attr != nullptr) {
    auto* next = attr->next;
    auto name = std::string(reinterpret_cast<const char*>(attr->name));
    if (name.compare(0, 5, "data-") == 0) {
      xmlRemoveProp(attr);
    }
    attr = next;
  }
}

static void StripCommonAttrsRecursive(xmlNodePtr node) {
  for (auto cur = node; cur != nullptr; cur = cur->next) {
    if (cur->type != XML_ELEMENT_NODE) {
      continue;
    }
    StripAttribute(cur, "id");
    StripAttribute(cur, "name");
    StripDataAttributes(cur);
    StripCommonAttrsRecursive(cur->children);
  }
}

static std::string GenerateNodeSignature(xmlNodePtr node,
                                         const std::vector<const char*>& extraStripAttrs) {
  auto* copy = xmlCopyNode(node, 1);
  if (copy == nullptr) {
    return {};
  }
  ReorderAttributesRecursive(copy);
  StripCommonAttrsRecursive(copy);
  for (auto* attr : extraStripAttrs) {
    StripAttribute(copy, attr);
  }
  std::string output;
  SerializeNode(output, copy, 0, 2);
  xmlFreeNode(copy);
  return output;
}

// ============================================================================
// Static Detection: Duplicate PathData
// ============================================================================

static void DetectDuplicatePathData(const PAGXDocument* doc, const LineNodeMap& lineNodeMap,
                                    std::vector<VerifyDiagnostic>& diagnostics) {
  std::unordered_map<std::string, int> seen;
  for (const auto& nodePtr : doc->nodes) {
    if (nodePtr->nodeType() != NodeType::PathData) {
      continue;
    }
    auto* pathData = static_cast<PathData*>(nodePtr.get());
    auto* xmlNode = FindXmlNode(lineNodeMap, pathData->sourceLine, "PathData");
    if (xmlNode == nullptr) {
      continue;
    }
    auto sig = GenerateNodeSignature(xmlNode, {});
    if (sig.empty()) {
      continue;
    }
    auto it = seen.find(sig);
    if (it != seen.end()) {
      AddDiagnostic(diagnostics, pathData->sourceLine,
                    "duplicate PathData, identical to line " + std::to_string(it->second) +
                        ". Fix: extract to <Resources> and reference via @id");
    } else {
      seen[sig] = pathData->sourceLine;
    }
  }
}

// ============================================================================
// Static Detection: Duplicate Gradients
// ============================================================================

static void DetectDuplicateGradients(const PAGXDocument* doc, const LineNodeMap& lineNodeMap,
                                     std::vector<VerifyDiagnostic>& diagnostics) {
  static const std::vector<const char*> gradientStripAttrs = {"matrix"};
  std::unordered_map<std::string, int> seen;
  for (const auto& nodePtr : doc->nodes) {
    if (!IsGradient(nodePtr->nodeType())) {
      continue;
    }
    std::string typeName = NodeTypeName(nodePtr->nodeType());
    auto* xmlNode = FindXmlNode(lineNodeMap, nodePtr->sourceLine, typeName.c_str());
    if (xmlNode == nullptr) {
      continue;
    }
    auto sig = GenerateNodeSignature(xmlNode, gradientStripAttrs);
    if (sig.empty()) {
      continue;
    }
    auto it = seen.find(sig);
    if (it != seen.end()) {
      AddDiagnostic(diagnostics, nodePtr->sourceLine,
                    "duplicate " + typeName + ", identical to line " + std::to_string(it->second) +
                        ". Fix: extract to <Resources> and reference via @id");
    } else {
      seen[sig] = nodePtr->sourceLine;
    }
  }
}

// ============================================================================
// Static Detection: Mergeable Consecutive Groups
// ============================================================================

static bool HasDefaultGroupTransform(const Group* group) {
  if (group->anchor.x != 0 || group->anchor.y != 0) {
    return false;
  }
  if (group->position.x != 0 || group->position.y != 0) {
    return false;
  }
  if (group->rotation != 0) {
    return false;
  }
  if (group->scale.x != 1 || group->scale.y != 1) {
    return false;
  }
  if (group->skew != 0) {
    return false;
  }
  if (group->alpha != 1) {
    return false;
  }
  if (!std::isnan(group->width) || !std::isnan(group->height)) {
    return false;
  }
  if (!std::isnan(group->percentWidth) || !std::isnan(group->percentHeight)) {
    return false;
  }
  return true;
}

static std::string SerializePainterSignature(xmlNodePtr groupNode) {
  std::string sig;
  bool foundPainter = false;
  for (auto child = groupNode->children; child != nullptr; child = child->next) {
    if (child->type != XML_ELEMENT_NODE) {
      continue;
    }
    auto childName = std::string(reinterpret_cast<const char*>(child->name));
    bool isPainter = (childName == "Fill" || childName == "Stroke");
    if (isPainter) {
      foundPainter = true;
      sig += GenerateNodeSignature(child, {});
    } else if (foundPainter) {
      break;
    }
  }
  return sig;
}

static void DetectMergeableGroups(const std::vector<Element*>& elements,
                                  const LineNodeMap& lineNodeMap,
                                  std::vector<VerifyDiagnostic>& diagnostics) {
  size_t i = 0;
  while (i < elements.size()) {
    auto* current = elements[i];
    if (current->nodeType() != NodeType::Group) {
      i++;
      continue;
    }
    auto* group = static_cast<const Group*>(current);
    if (!HasDefaultGroupTransform(group)) {
      i++;
      continue;
    }
    auto* xmlNode = FindXmlNode(lineNodeMap, group->sourceLine, "Group");
    if (xmlNode == nullptr) {
      i++;
      continue;
    }
    auto sig = SerializePainterSignature(xmlNode);
    if (sig.empty()) {
      i++;
      continue;
    }
    size_t j = i + 1;
    while (j < elements.size()) {
      auto* next = elements[j];
      if (next->nodeType() != NodeType::Group) {
        break;
      }
      auto* nextGroup = static_cast<const Group*>(next);
      if (!HasDefaultGroupTransform(nextGroup)) {
        break;
      }
      auto* nextXmlNode = FindXmlNode(lineNodeMap, nextGroup->sourceLine, "Group");
      if (nextXmlNode == nullptr) {
        break;
      }
      auto nextSig = SerializePainterSignature(nextXmlNode);
      if (nextSig != sig) {
        break;
      }
      AddDiagnostic(
          diagnostics, next->sourceLine,
          "consecutive Groups share identical painters. Fix: merge geometry into one Group");
      j++;
    }
    i = j;
  }
}

// ============================================================================
// Static Detection: Painter Leak
// ============================================================================

static bool IsGeometryElement(NodeType type) {
  return type == NodeType::Rectangle || type == NodeType::Ellipse || type == NodeType::Polystar ||
         type == NodeType::Path || type == NodeType::Text;
}

static bool ContainsRepeater(const std::vector<Element*>& elements) {
  for (auto* element : elements) {
    if (element->nodeType() == NodeType::Repeater) {
      return true;
    }
  }
  return false;
}

static void DetectPainterLeak(const std::vector<Element*>& elements,
                              std::vector<VerifyDiagnostic>& diagnostics) {
  if (ContainsRepeater(elements)) {
    return;
  }
  int lastPainterIndex = -1;
  for (size_t i = 0; i < elements.size(); i++) {
    auto type = elements[i]->nodeType();
    if (!IsPainter(type)) {
      continue;
    }
    if (lastPainterIndex >= 0) {
      bool hasGeometry = false;
      for (int j = lastPainterIndex + 1; j < static_cast<int>(i); j++) {
        if (IsGeometryElement(elements[j]->nodeType())) {
          hasGeometry = true;
          break;
        }
      }
      if (hasGeometry) {
        AddDiagnostic(diagnostics, elements[i]->sourceLine,
                      "painter leaks geometry from prior painters. "
                      "Fix: add a Group to isolate subsequent shape-painter pairs");
      }
    }
    lastPainterIndex = static_cast<int>(i);
  }
}

// ============================================================================
// Static Detection: Redundant First-Child Group
// ============================================================================

static bool CanUnwrapFirstChildGroup(const Group* group) {
  if (group->alpha != 1.0f) {
    return false;
  }
  if (group->position.x != 0 || group->position.y != 0) {
    return false;
  }
  if (group->anchor.x != 0 || group->anchor.y != 0) {
    return false;
  }
  if (group->rotation != 0) {
    return false;
  }
  if (group->scale.x != 1 || group->scale.y != 1) {
    return false;
  }
  if (group->skew != 0) {
    return false;
  }
  if (!std::isnan(group->width) || !std::isnan(group->height)) {
    return false;
  }
  if (!std::isnan(group->percentWidth) || !std::isnan(group->percentHeight)) {
    return false;
  }
  if (!std::isnan(group->left) || !std::isnan(group->right) || !std::isnan(group->top) ||
      !std::isnan(group->bottom) || !std::isnan(group->centerX) || !std::isnan(group->centerY)) {
    return false;
  }
  for (auto* child : group->elements) {
    auto* layoutNode = LayoutNode::AsLayoutNode(child);
    if (layoutNode == nullptr) {
      continue;
    }
    if (!std::isnan(layoutNode->right) || !std::isnan(layoutNode->bottom) ||
        !std::isnan(layoutNode->centerX) || !std::isnan(layoutNode->centerY)) {
      return false;
    }
  }
  return true;
}

static void DetectRedundantFirstChildGroup(const std::vector<Element*>& elements,
                                           std::vector<VerifyDiagnostic>& diagnostics) {
  if (elements.empty()) {
    return;
  }
  auto* first = elements[0];
  if (first->nodeType() != NodeType::Group) {
    return;
  }
  auto* group = static_cast<const Group*>(first);
  if (!CanUnwrapFirstChildGroup(group)) {
    return;
  }
  // Groups with customData carry intentional metadata that would be lost if unwrapped.
  if (!group->customData.empty()) {
    return;
  }
  // If there are following siblings AND the group contains painters, unwrapping would let those
  // painters bleed onto subsequent geometry. The Group is therefore providing real painter
  // isolation and should not be flagged.
  if (elements.size() > 1 && HasPainter(group->elements)) {
    return;
  }
  AddDiagnostic(diagnostics, group->sourceLine,
                "redundant first-child Group, no painter isolation needed. "
                "Fix: unwrap — move children into parent scope");
}

// ============================================================================
// Static Detection: Path to Primitives
// ============================================================================

static void DetectPathToPrimitives(const Path* path, std::vector<VerifyDiagnostic>& diagnostics) {
  if (path->data == nullptr || path->data->isEmpty()) {
    return;
  }
  if (!std::isnan(path->left) || !std::isnan(path->right) || !std::isnan(path->top) ||
      !std::isnan(path->bottom) || !std::isnan(path->centerX) || !std::isnan(path->centerY)) {
    return;
  }
  auto& verbs = path->data->verbs();
  if (verbs.size() == 5 || verbs.size() == 6) {
    bool allLine = true;
    for (size_t i = 1; i < verbs.size() - 1; i++) {
      if (verbs[i] != PathVerb::Line) {
        allLine = false;
        break;
      }
    }
    if (allLine && verbs[0] == PathVerb::Move && verbs.back() == PathVerb::Close) {
      auto& pts = path->data->points();
      bool axisAligned = true;
      size_t pointCount = pts.size();
      for (size_t i = 0; i < pointCount && axisAligned; i++) {
        auto& p1 = pts[i];
        auto& p2 = pts[(i + 1) % pointCount];
        float dx = std::abs(p2.x - p1.x);
        float dy = std::abs(p2.y - p1.y);
        if (dx > 0.01f && dy > 0.01f) {
          axisAligned = false;
        }
      }
      if (axisAligned) {
        AddDiagnostic(diagnostics, path->sourceLine,
                      "Path draws an axis-aligned rectangle. "
                      "Fix: replace with <Rectangle> for better performance");
        return;
      }
    }
  }
  if (verbs.size() == 6 && verbs[0] == PathVerb::Move && verbs[1] == PathVerb::Cubic &&
      verbs[2] == PathVerb::Cubic && verbs[3] == PathVerb::Cubic && verbs[4] == PathVerb::Cubic &&
      verbs[5] == PathVerb::Close) {
    auto& pts = path->data->points();
    // M(1) + 4*C(3 each) = 13 points minimum.
    if (pts.size() >= 13) {
      Point onCurve[4];
      onCurve[0] = pts[0];  // M point
      onCurve[1] = pts[3];  // End of first C
      onCurve[2] = pts[6];  // End of second C
      onCurve[3] = pts[9];  // End of third C
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
      bool isEllipse = rx >= 0.01f && ry >= 0.01f;
      // Check that on-curve points are at the cardinal positions (top/right/bottom/left).
      if (isEllipse) {
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
        isEllipse = foundTop && foundBottom && foundLeft && foundRight;
      }
      // Verify control points match the kappa ratio for circular arcs.
      // kappa ~ 0.5522847 -- control point offset from on-curve point for a quarter circle.
      if (isEllipse) {
        static constexpr float KAPPA = 0.5522847f;
        static constexpr float CP_TOLERANCE = 2.0f;
        float expectedCpOffsetX = rx * KAPPA;
        float expectedCpOffsetY = ry * KAPPA;
        for (int seg = 0; seg < 4 && isEllipse; seg++) {
          Point cp1 = pts[1 + seg * 3];
          Point cp2 = pts[2 + seg * 3];
          Point segStart = (seg == 0) ? pts[0] : pts[seg * 3];
          Point segEnd = pts[3 + seg * 3];
          float cp1DistX = std::abs(cp1.x - segStart.x);
          float cp1DistY = std::abs(cp1.y - segStart.y);
          float cp2DistX = std::abs(cp2.x - segEnd.x);
          float cp2DistY = std::abs(cp2.y - segEnd.y);
          bool cp1Valid =
              (cp1DistX < CP_TOLERANCE && std::abs(cp1DistY - expectedCpOffsetY) < CP_TOLERANCE) ||
              (cp1DistY < CP_TOLERANCE && std::abs(cp1DistX - expectedCpOffsetX) < CP_TOLERANCE);
          bool cp2Valid =
              (cp2DistX < CP_TOLERANCE && std::abs(cp2DistY - expectedCpOffsetY) < CP_TOLERANCE) ||
              (cp2DistY < CP_TOLERANCE && std::abs(cp2DistX - expectedCpOffsetX) < CP_TOLERANCE);
          isEllipse = cp1Valid && cp2Valid;
        }
      }
      if (isEllipse) {
        AddDiagnostic(diagnostics, path->sourceLine,
                      "Path draws an ellipse. Fix: replace with <Ellipse> for better performance");
      }
    }
  }
}

// ============================================================================
// Static Detection: Structurally Identical Layers
// ============================================================================

// Attributes to strip from the root Layer node for Composition extraction comparison.
// These are positional/identity attributes that don't affect the structural content.
// clang-format off
static const std::vector<const char*> LAYER_ROOT_STRIP_ATTRS = {
    "x", "y", "matrix", "matrix3D",
    "left", "right", "top", "bottom", "centerX", "centerY",
    "flex", "includeInLayout",
};
// clang-format on

static std::string GenerateLayerSignature(const LineNodeMap& lineNodeMap, int sourceLine) {
  auto* xmlNode = FindXmlNode(lineNodeMap, sourceLine, "Layer");
  if (xmlNode == nullptr) {
    return {};
  }
  return GenerateNodeSignature(xmlNode, LAYER_ROOT_STRIP_ATTRS);
}

// Count total Layer nodes in a subtree (including the root).
static int CountLayerNodes(const Layer* layer) {
  int count = 1;
  for (auto* child : layer->children) {
    count += CountLayerNodes(child);
  }
  return count;
}

// Single Layer with child subtree is a candidate for extraction if it repeats.
// Requires at least 3 total Layer nodes (root + 2 children or deeper nesting)
// to justify the overhead of a Composition wrapper.
static bool IsSubtreeCandidate(const Layer* layer) {
  if (layer->children.empty()) {
    return false;
  }
  if (CountLayerNodes(layer) < 3) {
    return false;
  }
  if (layer->composition != nullptr) {
    return false;
  }
  if (!layer->matrix.isIdentity()) {
    return false;
  }
  if (!std::isnan(layer->right) || !std::isnan(layer->bottom) || !std::isnan(layer->centerX) ||
      !std::isnan(layer->centerY)) {
    return false;
  }
  if (layer->flex > 0) {
    return false;
  }
  return true;
}

static void CollectSubtreeCandidates(const Layer* layer, std::vector<const Layer*>& candidates) {
  if (IsSubtreeCandidate(layer)) {
    candidates.push_back(layer);
  }
  for (auto* child : layer->children) {
    CollectSubtreeCandidates(child, candidates);
  }
}

struct ParentInfo {
  std::vector<const Layer*> children;
  std::vector<std::string> sigs;
};

static void CollectParent(const std::vector<Layer*>& children, const LineNodeMap& lineNodeMap,
                          std::vector<ParentInfo>& parents) {
  if (children.size() < 2) {
    return;
  }
  ParentInfo info;
  for (auto* child : children) {
    info.children.push_back(child);
    info.sigs.push_back(GenerateLayerSignature(lineNodeMap, child->sourceLine));
  }
  parents.push_back(std::move(info));
}

static void CollectParentsRecursive(const Layer* layer, const LineNodeMap& lineNodeMap,
                                    std::vector<ParentInfo>& parents) {
  CollectParent(layer->children, lineNodeMap, parents);
  for (auto* child : layer->children) {
    CollectParentsRecursive(child, lineNodeMap, parents);
  }
}

static void DetectStructurallyIdenticalLayers(const PAGXDocument* doc,
                                              const LineNodeMap& lineNodeMap,
                                              std::vector<VerifyDiagnostic>& diagnostics) {
  // Collect all parent containers with >= 2 children for sequence detection.
  std::vector<ParentInfo> parents;

  CollectParent(doc->layers, lineNodeMap, parents);
  for (auto* layer : doc->layers) {
    CollectParentsRecursive(layer, lineNodeMap, parents);
  }

  std::unordered_set<std::string> reported;

  // Pattern A: consecutive sibling Layer sequences (window >= 2) that repeat.
  // Greedy longest-first: scan from largest window down to 2, mark covered positions.
  // Skip sequences where all Layers are leaf nodes (no children).
  // Track which (parent, child-index) positions are already covered by a longer match.
  std::set<std::pair<int, int>> covered;

  for (size_t pi = 0; pi < parents.size(); pi++) {
    auto& p = parents[pi];
    auto n = p.sigs.size();
    // Try window sizes from largest to smallest.
    for (size_t win = n; win >= 2; win--) {
      // Collect all occurrences of each signature at this window size.
      std::unordered_map<std::string, std::vector<int>> sigStarts;
      for (size_t start = 0; start + win <= n; start++) {
        // Skip if any position in the window is already covered.
        bool anyCovered = false;
        for (size_t k = start; k < start + win; k++) {
          if (covered.count({static_cast<int>(pi), static_cast<int>(k)})) {
            anyCovered = true;
            break;
          }
        }
        if (anyCovered) {
          continue;
        }
        // Skip if all Layers in the window are leaf nodes.
        bool hasSubtree = false;
        for (size_t k = start; k < start + win; k++) {
          if (!p.children[k]->children.empty()) {
            hasSubtree = true;
            break;
          }
        }
        if (!hasSubtree) {
          continue;
        }
        // Skip if any Layer has an empty signature (xmlNode not found).
        bool hasEmpty = false;
        for (size_t k = start; k < start + win; k++) {
          if (p.sigs[k].empty()) {
            hasEmpty = true;
            break;
          }
        }
        if (hasEmpty) {
          continue;
        }
        std::string sig;
        for (size_t k = start; k < start + win; k++) {
          sig += p.sigs[k] + "|";
        }
        sigStarts[sig].push_back(static_cast<int>(start));
      }
      // Report and cover positions for signatures with >= 2 occurrences.
      for (auto& entry : sigStarts) {
        if (entry.second.size() < 2) {
          continue;
        }
        std::vector<int> lines;
        for (int start : entry.second) {
          lines.push_back(p.children[start]->sourceLine);
          for (size_t k = 0; k < win; k++) {
            covered.insert({static_cast<int>(pi), start + static_cast<int>(k)});
          }
        }
        std::sort(lines.begin(), lines.end());
        lines.erase(std::unique(lines.begin(), lines.end()), lines.end());
        if (lines.size() < 2) {
          continue;
        }
        std::ostringstream oss;
        for (size_t i = 0; i < lines.size(); i++) {
          if (i > 0) {
            oss << ",";
          }
          oss << lines[i];
        }
        auto key = oss.str();
        if (!reported.count(key)) {
          reported.insert(key);
          AddDiagnostic(diagnostics, lines,
                        "structurally identical Layers (lines " + key +
                            "). Fix: extract to a shared <Composition> in <Resources>");
        }
      }
    }
  }

  // Collect covered source lines for Pattern B to skip.
  std::unordered_set<int> coveredLines;
  for (auto& pos : covered) {
    coveredLines.insert(parents[pos.first].children[pos.second]->sourceLine);
  }

  // Pattern B: single Layer with child subtree that repeats (not already covered by Pattern A).
  std::vector<const Layer*> subtreeCandidates;
  for (auto* layer : doc->layers) {
    CollectSubtreeCandidates(layer, subtreeCandidates);
  }
  // Filter out candidates already covered by Pattern A.
  std::vector<const Layer*> filteredCandidates;
  for (auto* layer : subtreeCandidates) {
    if (!coveredLines.count(layer->sourceLine)) {
      filteredCandidates.push_back(layer);
    }
  }
  std::unordered_map<std::string, std::vector<const Layer*>> subtreeMap;
  for (auto* layer : filteredCandidates) {
    auto sig = GenerateLayerSignature(lineNodeMap, layer->sourceLine);
    if (sig.empty()) {
      continue;
    }
    subtreeMap[sig].push_back(layer);
  }
  for (auto& pair : subtreeMap) {
    if (pair.second.size() < 2) {
      continue;
    }
    std::vector<int> lines;
    for (auto* layer : pair.second) {
      lines.push_back(layer->sourceLine);
    }
    std::sort(lines.begin(), lines.end());
    std::ostringstream oss;
    for (size_t i = 0; i < lines.size(); i++) {
      if (i > 0) {
        oss << ",";
      }
      oss << lines[i];
    }
    auto key = oss.str();
    if (reported.count(key)) {
      continue;
    }
    reported.insert(key);
    AddDiagnostic(diagnostics, lines,
                  "structurally identical Layers (lines " + key +
                      "). Fix: extract to a shared <Composition> in <Resources>");
  }
}

// ============================================================================
// Static Detection: includeInLayout/flex Without Parent Layout,
//                   gap/alignment/arrangement Without Self Layout
// ============================================================================

static void DetectIneffectiveLayoutAttrs(const Layer* layer, bool parentHasLayout,
                                         std::vector<VerifyDiagnostic>& diagnostics) {
  if (!parentHasLayout) {
    if (!layer->includeInLayout) {
      AddDiagnostic(diagnostics, layer->sourceLine,
                    "includeInLayout=\"false\" has no effect, parent has no layout. "
                    "Fix: check if includeInLayout or parent layout is missing");
    }
    if (layer->flex > 0) {
      AddDiagnostic(diagnostics, layer->sourceLine,
                    "flex has no effect, parent has no layout. Fix: check if parent needs "
                    "layout=\"horizontal/vertical\" or if flex should be removed");
    }
  }
  // gap/alignment/arrangement require layout on self (padding does not — it works without layout).
  if (layer->layout == LayoutMode::None) {
    if (layer->gap > 0) {
      AddDiagnostic(diagnostics, layer->sourceLine,
                    "gap has no effect without layout. "
                    "Fix: add layout=\"horizontal\" or layout=\"vertical\", or remove gap");
    }
    if (layer->alignment != Alignment::Stretch) {
      AddDiagnostic(diagnostics, layer->sourceLine,
                    "alignment has no effect without layout. "
                    "Fix: add layout=\"horizontal\" or layout=\"vertical\", or remove alignment");
    }
    if (layer->arrangement != Arrangement::Start) {
      AddDiagnostic(diagnostics, layer->sourceLine,
                    "arrangement has no effect without layout. "
                    "Fix: add layout=\"horizontal\" or layout=\"vertical\", or remove arrangement");
    }
  }
}

// ============================================================================
// Static Detection: Downgradable Layers
// ============================================================================

static bool CanDowngradeLayerToGroup(const Layer* layer) {
  return layer->children.empty() && pagx::IsLayerShell(layer);
}

static void DetectDowngradableLayers(const Layer* parentLayer,
                                     std::vector<VerifyDiagnostic>& diagnostics) {
  if (parentLayer->layout != LayoutMode::None) {
    return;
  }
  if (parentLayer->children.empty()) {
    return;
  }
  // Cannot downgrade if parent has both contents and children — contents render behind
  // children, so moving children to contents would change the paint order.
  if (!parentLayer->contents.empty()) {
    return;
  }
  // All children must be downgradable — partial downgrade changes paint order.
  for (auto* child : parentLayer->children) {
    if (!CanDowngradeLayerToGroup(child)) {
      return;
    }
  }
  auto count = static_cast<int>(parentLayer->children.size());
  AddDiagnostic(diagnostics, parentLayer->sourceLine,
                std::to_string(count) +
                    " child Layer(s) use no Layer-exclusive features. "
                    "Fix: check if they can be replaced with <Group>");
}

// ============================================================================
// Static Detection: Mergeable Adjacent Layers
// ============================================================================

static void DetectMergeableAdjacentLayers(const std::vector<Layer*>& layers, bool hasLayout,
                                          bool dedupWithDowngrade,
                                          std::vector<VerifyDiagnostic>& diagnostics) {
  if (hasLayout) {
    return;
  }
  if (layers.size() < 2) {
    return;
  }
  // When DetectDowngradableLayers already covers this parent (no contents, no layout), skip if
  // all children are downgradable to avoid duplicate diagnostics.
  if (dedupWithDowngrade) {
    bool allDowngradable = true;
    for (auto* layer : layers) {
      if (!CanDowngradeLayerToGroup(layer)) {
        allDowngradable = false;
        break;
      }
    }
    if (allDowngradable) {
      return;
    }
  }
  size_t runStart = 0;
  while (runStart < layers.size()) {
    if (!CanDowngradeLayerToGroup(layers[runStart])) {
      runStart++;
      continue;
    }
    auto runEnd = runStart + 1;
    while (runEnd < layers.size() && CanDowngradeLayerToGroup(layers[runEnd])) {
      runEnd++;
    }
    if (runEnd - runStart >= 2) {
      std::vector<int> lines;
      for (auto i = runStart; i < runEnd; i++) {
        lines.push_back(layers[i]->sourceLine);
      }
      auto count = static_cast<int>(runEnd - runStart);
      AddDiagnostic(diagnostics, lines,
                    std::to_string(count) + " adjacent Layer(s) can be merged into one.");
    }
    runStart = runEnd;
  }
}

// ============================================================================
// Static Detection: Repeater Copies
// ============================================================================

struct RepeaterContext {
  float productSoFar = 1.0f;
  bool hasNonCenterStroke = false;
  bool hasDashedStroke = false;
};

static void CollectRepeaterProducts(const std::vector<Element*>& elements, RepeaterContext& ctx,
                                    std::vector<VerifyDiagnostic>& diagnostics);

static void CollectRepeaterProducts(const std::vector<Element*>& elements, RepeaterContext& ctx,
                                    std::vector<VerifyDiagnostic>& diagnostics) {
  float localProduct = ctx.productSoFar;

  for (auto* element : elements) {
    auto type = element->nodeType();
    if (type == NodeType::Repeater) {
      auto* repeater = static_cast<const Repeater*>(element);
      localProduct *= repeater->copies;
    }
  }

  bool hasRepeater = localProduct > ctx.productSoFar;

  // Build suffix product: Repeater only affects elements that precede it in the same scope.
  // A Group at index i is affected by Repeaters at indices > i.
  std::vector<float> suffixProduct(elements.size() + 1, 1.0f);
  for (int i = static_cast<int>(elements.size()) - 1; i >= 0; --i) {
    suffixProduct[i] = suffixProduct[i + 1];
    if (elements[i]->nodeType() == NodeType::Repeater) {
      auto* repeater = static_cast<const Repeater*>(elements[i]);
      suffixProduct[i] *= repeater->copies;
    }
  }

  for (size_t idx = 0; idx < elements.size(); ++idx) {
    auto* element = elements[idx];
    auto type = element->nodeType();
    if (type == NodeType::Stroke) {
      auto* stroke = static_cast<const Stroke*>(element);
      if (hasRepeater && stroke->align != StrokeAlign::Center) {
        std::string alignStr = (stroke->align == StrokeAlign::Inside) ? "inside" : "outside";
        AddDiagnostic(diagnostics, stroke->sourceLine,
                      "Stroke align=\"" + alignStr +
                          "\" inside Repeater forces CPU rendering. Fix: check if align=\"center\" "
                          "(default) works, or move Stroke outside Repeater");
      }
      if (hasRepeater && !stroke->dashes.empty()) {
        AddDiagnostic(diagnostics, stroke->sourceLine,
                      "dashed Stroke inside Repeater multiplies rendering cost. "
                      "Fix: check if dashes can be removed or Stroke moved outside Repeater");
      }
    } else if (type == NodeType::Repeater) {
      auto* repeater = static_cast<const Repeater*>(element);
      static constexpr float REPEATER_THRESHOLD = 200.0f;
      if (localProduct > REPEATER_THRESHOLD) {
        AddDiagnostic(diagnostics, repeater->sourceLine,
                      "Repeater produces " + std::to_string(static_cast<int>(localProduct)) +
                          " total copies (threshold: " +
                          std::to_string(static_cast<int>(REPEATER_THRESHOLD)) +
                          "), will cause slow rendering. Fix: check if copies can be reduced or "
                          "content simplified");
      }
    } else if (type == NodeType::Group || type == NodeType::TextBox) {
      auto* group = static_cast<const Group*>(element);
      float groupProduct = ctx.productSoFar * suffixProduct[idx + 1];
      RepeaterContext childCtx;
      childCtx.productSoFar = groupProduct;
      CollectRepeaterProducts(group->elements, childCtx, diagnostics);
    }
  }
}

// ============================================================================
// Static Detection: High Path Complexity
// ============================================================================

static void DetectHighPathComplexity(const Path* path, std::vector<VerifyDiagnostic>& diagnostics) {
  if (path->data == nullptr) {
    return;
  }
  auto verbCount = path->data->verbs().size();
  if (verbCount > 1024) {
    AddDiagnostic(diagnostics, path->sourceLine,
                  "Path with " + std::to_string(verbCount) +
                      " verbs (> 1024), may cause slow rendering. "
                      "Fix: check if path can be simplified or split");
  }
}

// ============================================================================
// Static Detection: Large Blur Radius
// ============================================================================

static void DetectLargeBlurRadius(const BlurFilter* blur,
                                  std::vector<VerifyDiagnostic>& diagnostics) {
  if (blur->blurX > 256.0f || blur->blurY > 256.0f) {
    AddDiagnostic(
        diagnostics, blur->sourceLine,
        "BlurFilter radius too large (X:" + std::to_string(static_cast<int>(blur->blurX)) +
            " Y:" + std::to_string(static_cast<int>(blur->blurY)) +
            ", max recommended 256). Fix: check if blur radius can be reduced");
  }
}

static void DetectLargeDropShadowRadius(const DropShadowStyle* shadow,
                                        std::vector<VerifyDiagnostic>& diagnostics) {
  if (shadow->blurX > 256.0f || shadow->blurY > 256.0f) {
    AddDiagnostic(diagnostics, shadow->sourceLine,
                  "DropShadowStyle blur radius too large (X:" +
                      std::to_string(static_cast<int>(shadow->blurX)) +
                      " Y:" + std::to_string(static_cast<int>(shadow->blurY)) +
                      "). Fix: check if blur radius can be reduced");
  }
}

static void DetectLargeBackgroundBlurRadius(const BackgroundBlurStyle* blur,
                                            std::vector<VerifyDiagnostic>& diagnostics) {
  if (blur->blurX > 256.0f || blur->blurY > 256.0f) {
    AddDiagnostic(diagnostics, blur->sourceLine,
                  "BackgroundBlurStyle blur radius too large (X:" +
                      std::to_string(static_cast<int>(blur->blurX)) +
                      " Y:" + std::to_string(static_cast<int>(blur->blurY)) +
                      "). Fix: check if blur radius can be reduced");
  }
}

static void DetectLargeInnerShadowRadius(const InnerShadowStyle* shadow,
                                         std::vector<VerifyDiagnostic>& diagnostics) {
  if (shadow->blurX > 256.0f || shadow->blurY > 256.0f) {
    AddDiagnostic(diagnostics, shadow->sourceLine,
                  "InnerShadowStyle blur radius too large (X:" +
                      std::to_string(static_cast<int>(shadow->blurX)) +
                      " Y:" + std::to_string(static_cast<int>(shadow->blurY)) +
                      "). Fix: check if blur radius can be reduced");
  }
}

// ============================================================================
// Static Detection: Low Opacity High-Cost Children
// ============================================================================

static bool HasHighCostChildren(const Layer* layer);
static bool HasHighCostElements(const std::vector<Element*>& elements);

static bool HasHighCostElements(const std::vector<Element*>& elements) {
  for (auto* element : elements) {
    auto type = element->nodeType();
    if (type == NodeType::Repeater) {
      return true;
    }
    if (type == NodeType::Group || type == NodeType::TextBox) {
      auto* group = static_cast<const Group*>(element);
      if (HasHighCostElements(group->elements)) {
        return true;
      }
    }
  }
  return false;
}

static bool HasHighCostChildren(const Layer* layer) {
  for (auto* filter : layer->filters) {
    if (filter->nodeType() == NodeType::BlurFilter) {
      return true;
    }
  }
  for (auto* style : layer->styles) {
    auto type = style->nodeType();
    if (type == NodeType::DropShadowStyle || type == NodeType::InnerShadowStyle ||
        type == NodeType::BackgroundBlurStyle) {
      return true;
    }
  }
  if (HasHighCostElements(layer->contents)) {
    return true;
  }
  for (auto* child : layer->children) {
    if (HasHighCostChildren(child)) {
      return true;
    }
  }
  return false;
}

static void DetectLowOpacityHighCost(const Layer* layer,
                                     std::vector<VerifyDiagnostic>& diagnostics) {
  if (layer->alpha < 0.05f && layer->alpha > 0.0f && HasHighCostChildren(layer)) {
    AddDiagnostic(diagnostics, layer->sourceLine,
                  "opacity " + std::to_string(static_cast<int>(layer->alpha * 100)) +
                      "% with high-cost children (Repeater/Blur), nearly invisible but still "
                      "rendered. Fix: check if this Layer should be visible=\"false\" or removed");
  }
}

// ============================================================================
// Static Detection: Rectangular Mask Can Use clipToBounds
// ============================================================================

static void DetectRectangularMask(const Layer* layer, std::vector<VerifyDiagnostic>& diagnostics) {
  if (layer->mask == nullptr) {
    return;
  }
  if (layer->maskType != MaskType::Alpha) {
    return;
  }
  auto* maskLayer = layer->mask;
  if (maskLayer->contents.size() != 2) {
    return;
  }
  if (maskLayer->contents[0]->nodeType() != NodeType::Rectangle ||
      maskLayer->contents[1]->nodeType() != NodeType::Fill) {
    return;
  }
  auto* rect = static_cast<Rectangle*>(maskLayer->contents[0]);
  // A rounded or reversed rectangle is NOT clip-equivalent to a plain rectangular bounds clip,
  // so suggesting scrollRect/clipToBounds would change the rendered output.
  if (rect->roundness != 0 || rect->reversed) {
    return;
  }
  auto* fill = static_cast<Fill*>(maskLayer->contents[1]);
  if (fill->alpha < 0.999f) {
    return;
  }
  if (!maskLayer->matrix.isIdentity()) {
    return;
  }
  AddDiagnostic(diagnostics, layer->sourceLine,
                "rectangular alpha mask can use clipToBounds instead. "
                "Fix: check if clipToBounds=\"true\" can replace the mask for better performance");
}

// ============================================================================
// Static Detection: Run All
// ============================================================================

// For Rectangle/Ellipse, detect when 'size' is fully overridden by width/height or opposite-edge
// constraints on both axes — the animatable 'size' value then becomes dead data.
static void DetectSizeIgnoredForRectOrEllipse(const LayoutNode* node, const Size& size,
                                              int sourceLine,
                                              std::vector<VerifyDiagnostic>& diagnostics) {
  bool hasExplicitW = !std::isnan(node->width) || !std::isnan(node->percentWidth);
  bool hasExplicitH = !std::isnan(node->height) || !std::isnan(node->percentHeight);
  bool wFromConstraints = !std::isnan(node->left) && !std::isnan(node->right);
  bool hFromConstraints = !std::isnan(node->top) && !std::isnan(node->bottom);
  bool wIgnored = hasExplicitW || wFromConstraints;
  bool hIgnored = hasExplicitH || hFromConstraints;
  bool hasSizeAttribute = size.width != 0 || size.height != 0;
  if (wIgnored && hIgnored && hasSizeAttribute) {
    AddDiagnostic(diagnostics, sourceLine,
                  "size ignored, width/height or constraints take priority. Fix: remove size");
  }
}

static void RunStaticDetectionOnElements(const std::vector<Element*>& elements,
                                         const LineNodeMap& lineNodeMap,
                                         std::vector<VerifyDiagnostic>& diagnostics) {
  DetectMergeableGroups(elements, lineNodeMap, diagnostics);
  DetectPainterLeak(elements, diagnostics);
  DetectRedundantFirstChildGroup(elements, diagnostics);

  RepeaterContext ctx;
  ctx.productSoFar = 1.0f;
  CollectRepeaterProducts(elements, ctx, diagnostics);

  for (auto* element : elements) {
    auto type = element->nodeType();
    if (type == NodeType::Rectangle) {
      auto* rect = static_cast<const Rectangle*>(element);
      DetectSizeIgnoredForRectOrEllipse(rect, rect->size, element->sourceLine, diagnostics);
    } else if (type == NodeType::Ellipse) {
      auto* ellipse = static_cast<const Ellipse*>(element);
      DetectSizeIgnoredForRectOrEllipse(ellipse, ellipse->size, element->sourceLine, diagnostics);
    }
    if (type == NodeType::Group) {
      auto* group = static_cast<const Group*>(element);
      DetectEmptyGroup(group, diagnostics);
      RunStaticDetectionOnElements(group->elements, lineNodeMap, diagnostics);
    } else if (type == NodeType::TextBox) {
      auto* textBox = static_cast<const TextBox*>(element);
      DetectEmptyGroup(textBox, diagnostics);
      RunStaticDetectionOnElements(textBox->elements, lineNodeMap, diagnostics);
    } else if (type == NodeType::Stroke) {
      auto* stroke = static_cast<const Stroke*>(element);
      DetectZeroStrokeWidth(stroke, diagnostics);
    } else if (type == NodeType::Path) {
      auto* path = static_cast<const Path*>(element);
      DetectPathToPrimitives(path, diagnostics);
      DetectHighPathComplexity(path, diagnostics);
    }
  }
}

static void RunStaticDetectionOnLayer(const Layer* layer, float canvasWidth, float canvasHeight,
                                      bool parentHasLayout, const LineNodeMap& lineNodeMap,
                                      std::vector<VerifyDiagnostic>& diagnostics) {
  DetectEmptyLayer(layer, parentHasLayout, diagnostics);
  DetectFullCanvasClipMask(layer, canvasWidth, canvasHeight, diagnostics);
  DetectIneffectiveLayoutAttrs(layer, parentHasLayout, diagnostics);
  DetectDowngradableLayers(layer, diagnostics);
  // Dedup with DetectDowngradableLayers: it fires when parent has no contents and no layout.
  bool dedupWithDowngrade = layer->contents.empty() && layer->layout == LayoutMode::None;
  DetectMergeableAdjacentLayers(layer->children, layer->layout != LayoutMode::None,
                                dedupWithDowngrade, diagnostics);
  DetectLowOpacityHighCost(layer, diagnostics);
  DetectRectangularMask(layer, diagnostics);

  for (auto* filter : layer->filters) {
    if (filter->nodeType() == NodeType::BlurFilter) {
      DetectLargeBlurRadius(static_cast<const BlurFilter*>(filter), diagnostics);
    }
  }
  for (auto* style : layer->styles) {
    auto type = style->nodeType();
    if (type == NodeType::DropShadowStyle) {
      DetectLargeDropShadowRadius(static_cast<const DropShadowStyle*>(style), diagnostics);
    } else if (type == NodeType::BackgroundBlurStyle) {
      DetectLargeBackgroundBlurRadius(static_cast<const BackgroundBlurStyle*>(style), diagnostics);
    } else if (type == NodeType::InnerShadowStyle) {
      DetectLargeInnerShadowRadius(static_cast<const InnerShadowStyle*>(style), diagnostics);
    }
  }

  RunStaticDetectionOnElements(layer->contents, lineNodeMap, diagnostics);

  bool thisHasLayout = layer->layout != LayoutMode::None;
  for (auto* child : layer->children) {
    RunStaticDetectionOnLayer(child, canvasWidth, canvasHeight, thisHasLayout, lineNodeMap,
                              diagnostics);
  }
}

static void RunStaticDetection(const PAGXDocument* doc, const LineNodeMap& lineNodeMap,
                               std::vector<VerifyDiagnostic>& diagnostics,
                               const Layer* targetLayer = nullptr) {
  if (targetLayer == nullptr) {
    DetectUnreferencedResources(doc, diagnostics);
    DetectDuplicatePathData(doc, lineNodeMap, diagnostics);
    DetectDuplicateGradients(doc, lineNodeMap, diagnostics);
    DetectStructurallyIdenticalLayers(doc, lineNodeMap, diagnostics);
    // Root-level merge detection on doc->layers. No dedup needed — DetectDowngradableLayers
    // does not run at root level.
    DetectMergeableAdjacentLayers(doc->layers, false, false, diagnostics);
    for (auto* layer : doc->layers) {
      RunStaticDetectionOnLayer(layer, doc->width, doc->height, false, lineNodeMap, diagnostics);
    }
  } else {
    RunStaticDetectionOnLayer(targetLayer, doc->width, doc->height, false, lineNodeMap,
                              diagnostics);
  }

  for (const auto& err : doc->errors) {
    VerifyDiagnostic diag;
    if (err.compare(0, 5, "line ") == 0) {
      auto colonPos = err.find(':', 5);
      if (colonPos != std::string::npos) {
        auto lineStr = err.substr(5, colonPos - 5);
        char* endPtr = nullptr;
        long lineNum = strtol(lineStr.c_str(), &endPtr, 10);
        if (endPtr != lineStr.c_str() && *endPtr == '\0') {
          diag.lines.push_back(static_cast<int>(lineNum));
        }
        diag.message = err.substr(colonPos + 2);
      } else {
        diag.message = err;
      }
    } else {
      diag.message = err;
    }
    if (diag.lines.empty()) {
      diag.lines.push_back(0);
    }
    diagnostics.push_back(std::move(diag));
  }
}

// ============================================================================
// Spatial Detection
// ============================================================================

struct SpatialRect {
  float x = 0;
  float y = 0;
  float width = 0;
  float height = 0;
};

static bool RectsOverlap(const SpatialRect& a, const SpatialRect& b) {
  return a.x < b.x + b.width && a.x + a.width > b.x && a.y < b.y + b.height && a.y + a.height > b.y;
}

static bool IsFullyContained(const SpatialRect& parent, const SpatialRect& child) {
  static constexpr float TOLERANCE = 0.5f;
  return (child.x + TOLERANCE) >= parent.x && (child.y + TOLERANCE) >= parent.y &&
         (child.x + child.width) <= (parent.x + parent.width + TOLERANCE) &&
         (child.y + child.height) <= (parent.y + parent.height + TOLERANCE);
}

static bool ElementsHaveLeafContent(const std::vector<Element*>& elements);

static bool HasLeafContent(const Layer* layer) {
  if (layer->composition != nullptr) {
    return true;
  }
  if (ElementsHaveLeafContent(layer->contents)) {
    return true;
  }
  for (auto* child : layer->children) {
    if (HasLeafContent(child)) {
      return true;
    }
  }
  return false;
}

static bool ElementsHaveLeafContent(const std::vector<Element*>& elements) {
  for (auto* element : elements) {
    auto type = element->nodeType();
    if (type == NodeType::Rectangle || type == NodeType::Ellipse || type == NodeType::Polystar ||
        type == NodeType::Path || type == NodeType::Text) {
      return true;
    }
    if (type == NodeType::Group || type == NodeType::TextBox) {
      auto* group = static_cast<const Group*>(element);
      if (ElementsHaveLeafContent(group->elements)) {
        return true;
      }
    }
  }
  return false;
}

// Checks whether a child LayoutNode's width/height depends on its parent's resolved size through
// opposite-edge, centering, or percent constraints. Accumulates the result into the provided
// output flags (existing true values are preserved).
static void UpdateSizeDependenciesFromLayoutNode(const LayoutNode* node, bool* outDependsOnW,
                                                 bool* outDependsOnH) {
  if (!std::isnan(node->right) || !std::isnan(node->centerX) || !std::isnan(node->percentWidth)) {
    *outDependsOnW = true;
  }
  if (!std::isnan(node->bottom) || !std::isnan(node->centerY) || !std::isnan(node->percentHeight)) {
    *outDependsOnH = true;
  }
}

// Walks a list of child elements and accumulates whether any of them depend on the parent's
// resolved size through size-sensitive layout constraints.
static void CollectChildrenSizeDependencies(const std::vector<Element*>& elements,
                                            bool* outDependsOnW, bool* outDependsOnH) {
  for (auto* element : elements) {
    auto* node = LayoutNode::AsLayoutNode(element);
    if (node == nullptr) {
      continue;
    }
    UpdateSizeDependenciesFromLayoutNode(node, outDependsOnW, outDependsOnH);
  }
}

static void DetectZeroSizeGroup(const Group* group, std::vector<VerifyDiagnostic>& diagnostics) {
  auto bounds = group->layoutBounds();
  if (bounds.width != 0 && bounds.height != 0) {
    return;
  }
  if (!ElementsHaveLeafContent(group->elements)) {
    return;
  }
  // Skip when an explicit zero size is authored — the user intentionally collapsed this group.
  bool explicitZeroW =
      (bounds.width == 0 && !std::isnan(group->width) && group->width == 0) ||
      (bounds.width == 0 && !std::isnan(group->percentWidth) && group->percentWidth == 0);
  bool explicitZeroH =
      (bounds.height == 0 && !std::isnan(group->height) && group->height == 0) ||
      (bounds.height == 0 && !std::isnan(group->percentHeight) && group->percentHeight == 0);
  if (explicitZeroW || explicitZeroH) {
    return;
  }
  // Only report when at least one child depends on the group size (opposite-edge, centering,
  // or percent).
  bool childrenDependOnWidth = false;
  bool childrenDependOnHeight = false;
  CollectChildrenSizeDependencies(group->elements, &childrenDependOnWidth, &childrenDependOnHeight);
  bool zeroWidthMatters = bounds.width == 0 && childrenDependOnWidth;
  bool zeroHeightMatters = bounds.height == 0 && childrenDependOnHeight;
  if (!zeroWidthMatters && !zeroHeightMatters) {
    return;
  }
  std::ostringstream oss;
  oss << "zero size (" << static_cast<int>(bounds.width) << "x" << static_cast<int>(bounds.height)
      << "), children depend on this container for layout. Fix: check why this container has no "
         "size";
  AddDiagnostic(diagnostics, group->sourceLine, oss.str());
}

static void DetectZeroSize(const Layer* layer, const Layer* parentLayer,
                           std::vector<VerifyDiagnostic>& diagnostics) {
  auto bounds = layer->layoutBounds();
  if (bounds.width != 0 && bounds.height != 0) {
    return;
  }
  if (!HasLeafContent(layer)) {
    return;
  }
  bool inParentLayout =
      parentLayer != nullptr && parentLayer->layout != LayoutMode::None && layer->includeInLayout;
  if (!inParentLayout) {
    bool explicitZeroW =
        (bounds.width == 0 && !std::isnan(layer->width) && layer->width == 0) ||
        (bounds.width == 0 && !std::isnan(layer->percentWidth) && layer->percentWidth == 0);
    bool explicitZeroH =
        (bounds.height == 0 && !std::isnan(layer->height) && layer->height == 0) ||
        (bounds.height == 0 && !std::isnan(layer->percentHeight) && layer->percentHeight == 0);
    if (explicitZeroW || explicitZeroH) {
      return;
    }
  }

  bool hasLayoutChildren = layer->layout != LayoutMode::None || layer->clipToBounds;
  bool childrenDependOnWidth = false;
  bool childrenDependOnHeight = false;
  for (auto* child : layer->children) {
    UpdateSizeDependenciesFromLayoutNode(child, &childrenDependOnWidth, &childrenDependOnHeight);
    if (child->flex > 0 && layer->layout == LayoutMode::Horizontal) {
      childrenDependOnWidth = true;
    }
    if (child->flex > 0 && layer->layout == LayoutMode::Vertical) {
      childrenDependOnHeight = true;
    }
  }
  // VectorElements in layer->contents also consult the parent size for opposite-edge or percent
  // constraints — treat them as size-dependent too.
  CollectChildrenSizeDependencies(layer->contents, &childrenDependOnWidth, &childrenDependOnHeight);
  bool zeroWidthMatters = bounds.width == 0 && (hasLayoutChildren || childrenDependOnWidth);
  bool zeroHeightMatters = bounds.height == 0 && (hasLayoutChildren || childrenDependOnHeight);
  if (!zeroWidthMatters && !zeroHeightMatters && !inParentLayout) {
    return;
  }

  std::ostringstream oss;
  oss << "zero size (" << static_cast<int>(bounds.width) << "x" << static_cast<int>(bounds.height)
      << "), children depend on this container for layout";

  if (layer->flex > 0 && parentLayer != nullptr && parentLayer->layout != LayoutMode::None) {
    bool horizontal = parentLayer->layout == LayoutMode::Horizontal;
    auto parentBounds = parentLayer->layoutBounds();
    float parentMainSize = horizontal ? parentBounds.width : parentBounds.height;
    if (parentMainSize <= 0) {
      const char* axis = horizontal ? "width" : "height";
      oss << " (flex child, parent " << axis << " is 0)";
    }
  }
  oss << ". Fix: check why this container has no size";
  AddDiagnostic(diagnostics, layer->sourceLine, oss.str());
}

static void DetectConstraintsIgnoredByLayout(const Layer* layer, const Layer* parentLayer,
                                             std::vector<VerifyDiagnostic>& diagnostics) {
  if (parentLayer == nullptr || parentLayer->layout == LayoutMode::None) {
    return;
  }
  if (!layer->includeInLayout) {
    return;
  }
  bool hasConstraints =
      (!std::isnan(layer->left) || !std::isnan(layer->right) || !std::isnan(layer->centerX) ||
       !std::isnan(layer->top) || !std::isnan(layer->bottom) || !std::isnan(layer->centerY));
  if (!hasConstraints) {
    return;
  }
  std::string attrs;
  if (!std::isnan(layer->left)) {
    attrs += "left ";
  }
  if (!std::isnan(layer->right)) {
    attrs += "right ";
  }
  if (!std::isnan(layer->centerX)) {
    attrs += "centerX ";
  }
  if (!std::isnan(layer->top)) {
    attrs += "top ";
  }
  if (!std::isnan(layer->bottom)) {
    attrs += "bottom ";
  }
  if (!std::isnan(layer->centerY)) {
    attrs += "centerY ";
  }
  if (!attrs.empty() && attrs.back() == ' ') {
    attrs.pop_back();
  }
  AddDiagnostic(diagnostics, layer->sourceLine,
                "constraints (" + attrs +
                    ") ignored, parent has container layout. Fix: check if constraints should be "
                    "removed or includeInLayout=\"false\" added");
}

// Returns true when the layer's main-axis size is determined purely by measuring its children.
// A content-measured main axis means flex children cannot receive distributed space.
static bool IsMainAxisContentMeasured(const Layer* layer, const Layer* parentLayer) {
  bool horizontal = layer->layout == LayoutMode::Horizontal;
  float explicitMain = horizontal ? layer->width : layer->height;
  if (!std::isnan(explicitMain)) {
    return false;
  }
  float percentMain = horizontal ? layer->percentWidth : layer->percentHeight;
  if (!std::isnan(percentMain)) {
    return false;
  }
  bool mainFromConstraints = horizontal ? (!std::isnan(layer->left) && !std::isnan(layer->right))
                                        : (!std::isnan(layer->top) && !std::isnan(layer->bottom));
  if (mainFromConstraints) {
    return false;
  }
  if (parentLayer != nullptr && parentLayer->layout != LayoutMode::None && layer->includeInLayout) {
    bool parentHorizontal = parentLayer->layout == LayoutMode::Horizontal;
    // Flex distributes along the parent's layout axis. If that matches this layer's main axis,
    // the main-axis size is assigned by flex, not by content measurement.
    if (layer->flex > 0 && parentHorizontal == horizontal) {
      return false;
    }
    // Stretch assigns the parent's cross-axis size to children without explicit cross-axis size.
    // If the parent's cross axis matches this layer's main axis, the main-axis size comes from
    // stretch, not from content measurement.
    if (parentHorizontal != horizontal && parentLayer->alignment == Alignment::Stretch) {
      return false;
    }
  }
  return true;
}

static void DetectFlexInContentMeasuredParent(const Layer* layer, const Layer* parentLayer,
                                              const Layer* grandparentLayer,
                                              std::vector<VerifyDiagnostic>& diagnostics) {
  if (parentLayer == nullptr || parentLayer->layout == LayoutMode::None) {
    return;
  }
  if (layer->flex <= 0 || !layer->includeInLayout) {
    return;
  }
  bool horizontal = parentLayer->layout == LayoutMode::Horizontal;
  if (!std::isnan(horizontal ? layer->width : layer->height)) {
    return;
  }
  if (!std::isnan(horizontal ? layer->percentWidth : layer->percentHeight)) {
    return;
  }
  if (!IsMainAxisContentMeasured(parentLayer, grandparentLayer)) {
    return;
  }
  // In a content-measured parent, a flex child still displays at its own measured size even though
  // flex does not distribute extra space. Only flag it when the child ends up with zero main-axis
  // size after layout — that is the case where flex is truly ineffective.
  auto childBounds = layer->layoutBounds();
  float childMainSize = horizontal ? childBounds.width : childBounds.height;
  if (childMainSize > 0) {
    return;
  }
  const char* axis = horizontal ? "width" : "height";
  AddDiagnostic(diagnostics, layer->sourceLine,
                std::string("flex has no effect, parent ") + axis +
                    " is 0 after layout. Fix: give the parent an explicit " + axis +
                    " or ensure it receives size from its own parent layout");
}

static void CheckContentOriginOffset(const std::vector<Element*>& elements, const Padding& padding,
                                     int sourceLine, std::vector<VerifyDiagnostic>& diagnostics) {
  float minX = FLT_MAX;
  float minY = FLT_MAX;
  bool hasChild = false;
  for (auto* element : elements) {
    auto* layoutNode = LayoutNode::AsLayoutNode(element);
    if (layoutNode == nullptr) {
      continue;
    }
    auto bounds = layoutNode->layoutBounds();
    if (bounds.width == 0 && bounds.height == 0) {
      continue;
    }
    minX = std::min(minX, bounds.x);
    minY = std::min(minY, bounds.y);
    hasChild = true;
  }
  if (!hasChild) {
    return;
  }
  static constexpr float TOLERANCE = 0.5f;
  float expectedX = padding.left;
  float expectedY = padding.top;
  if (std::abs(minX - expectedX) > TOLERANCE || std::abs(minY - expectedY) > TOLERANCE) {
    auto expectedStr = "(" + std::to_string(static_cast<int>(expectedX)) + "," +
                       std::to_string(static_cast<int>(expectedY)) + ")";
    AddDiagnostic(diagnostics, sourceLine,
                  "children start at (" + std::to_string(static_cast<int>(minX)) + "," +
                      std::to_string(static_cast<int>(minY)) + "), not " + expectedStr +
                      ", container measurement inaccurate. Fix: shift all children so "
                      "top-left starts at " +
                      expectedStr + ", preserving relative positions");
  }
}

static void DetectContentOriginOffset(const Layer* layer, const Layer* parentLayer,
                                      std::vector<VerifyDiagnostic>& diagnostics) {
  if (!std::isnan(layer->width) || !std::isnan(layer->height)) {
    return;
  }
  if (!std::isnan(layer->percentWidth) || !std::isnan(layer->percentHeight)) {
    return;
  }
  bool sizeFromConstraints = (!std::isnan(layer->left) && !std::isnan(layer->right)) &&
                             (!std::isnan(layer->top) && !std::isnan(layer->bottom));
  if (sizeFromConstraints) {
    return;
  }
  bool sizeFromFlex = layer->flex > 0 && parentLayer != nullptr &&
                      parentLayer->layout != LayoutMode::None && layer->includeInLayout;
  if (sizeFromFlex) {
    return;
  }
  bool inParentLayout =
      parentLayer != nullptr && parentLayer->layout != LayoutMode::None && layer->includeInLayout;
  bool hasSizeDependentConstraint = (!std::isnan(layer->right) || !std::isnan(layer->bottom) ||
                                     !std::isnan(layer->centerX) || !std::isnan(layer->centerY));
  if (!inParentLayout && !hasSizeDependentConstraint) {
    return;
  }
  CheckContentOriginOffset(layer->contents, layer->padding, layer->sourceLine, diagnostics);
}

static void DetectContentOriginOffsetForGroup(const Group* group,
                                              std::vector<VerifyDiagnostic>& diagnostics) {
  if (!std::isnan(group->width) || !std::isnan(group->height)) {
    return;
  }
  if (!std::isnan(group->percentWidth) || !std::isnan(group->percentHeight)) {
    return;
  }
  bool sizeFromConstraints = (!std::isnan(group->left) && !std::isnan(group->right)) &&
                             (!std::isnan(group->top) && !std::isnan(group->bottom));
  if (sizeFromConstraints) {
    return;
  }
  bool hasSizeDependentConstraint = (!std::isnan(group->right) || !std::isnan(group->bottom) ||
                                     !std::isnan(group->centerX) || !std::isnan(group->centerY));
  if (!hasSizeDependentConstraint) {
    return;
  }
  for (auto* element : group->elements) {
    auto* node = LayoutNode::AsLayoutNode(element);
    if (node != nullptr && node->hasConstraints()) {
      return;
    }
  }
  CheckContentOriginOffset(group->elements, group->padding, group->sourceLine, diagnostics);
}

static void DetectOverlappingSiblings(const Layer* parentLayer,
                                      std::vector<VerifyDiagnostic>& diagnostics) {
  if (parentLayer->layout == LayoutMode::None) {
    return;
  }
  std::vector<std::pair<const Layer*, SpatialRect>> layoutChildren;
  for (auto* child : parentLayer->children) {
    if (!child->includeInLayout) {
      continue;
    }
    auto bounds = child->layoutBounds();
    SpatialRect rect = {bounds.x, bounds.y, bounds.width, bounds.height};
    layoutChildren.emplace_back(child, rect);
  }
  for (size_t i = 0; i < layoutChildren.size(); i++) {
    for (size_t j = i + 1; j < layoutChildren.size(); j++) {
      if (RectsOverlap(layoutChildren[i].second, layoutChildren[j].second)) {
        auto& a = layoutChildren[i];
        auto& b = layoutChildren[j];
        std::vector<int> lines = {a.first->sourceLine, b.first->sourceLine};
        AddDiagnostic(diagnostics, lines,
                      "overlapping siblings " +
                          FormatBounds(a.second.x, a.second.y, a.second.width, a.second.height) +
                          " and " +
                          FormatBounds(b.second.x, b.second.y, b.second.width, b.second.height) +
                          " in container layout. Fix: check parent layout direction, gap, padding, "
                          "and children sizes");
      }
    }
  }
}

static void DetectConstraintConflicts(const LayoutNode* node, int sourceLine,
                                      std::vector<VerifyDiagnostic>& diagnostics) {
  if (!std::isnan(node->centerX)) {
    if (!std::isnan(node->left) && !std::isnan(node->right)) {
      AddDiagnostic(diagnostics, sourceLine,
                    "left and right ignored, centerX takes priority. Fix: remove left and right");
    } else if (!std::isnan(node->left)) {
      AddDiagnostic(diagnostics, sourceLine,
                    "left ignored, centerX takes priority. Fix: remove left");
    } else if (!std::isnan(node->right)) {
      AddDiagnostic(diagnostics, sourceLine,
                    "right ignored, centerX takes priority. Fix: remove right");
    }
  }
  if (!std::isnan(node->centerY)) {
    if (!std::isnan(node->top) && !std::isnan(node->bottom)) {
      AddDiagnostic(diagnostics, sourceLine,
                    "top and bottom ignored, centerY takes priority. Fix: remove top and bottom");
    } else if (!std::isnan(node->top)) {
      AddDiagnostic(diagnostics, sourceLine,
                    "top ignored, centerY takes priority. Fix: remove top");
    } else if (!std::isnan(node->bottom)) {
      AddDiagnostic(diagnostics, sourceLine,
                    "bottom ignored, centerY takes priority. Fix: remove bottom");
    }
  }
  // Priority on the width axis: left+right > percentWidth > authored width. Explicit width vs
  // percentWidth cannot coexist in XML (single attribute with or without '%'), so only the
  // opposite-edge vs percent collision is reportable here.
  bool wFromOpposite = !std::isnan(node->left) && !std::isnan(node->right);
  if (wFromOpposite && !std::isnan(node->percentWidth)) {
    AddDiagnostic(diagnostics, sourceLine,
                  "percentWidth ignored, left+right constraints take priority. "
                  "Fix: remove percentWidth");
  }
  bool hFromOpposite = !std::isnan(node->top) && !std::isnan(node->bottom);
  if (hFromOpposite && !std::isnan(node->percentHeight)) {
    AddDiagnostic(diagnostics, sourceLine,
                  "percentHeight ignored, top+bottom constraints take priority. "
                  "Fix: remove percentHeight");
  }
}

// Flag explicit width/height that is shadowed by opposite-edge constraints. The layout priority is
// left+right > percentWidth > authored width (top+bottom analogous for height), so when both
// opposite edges are set the authored width/height contributes nothing. percentWidth/percentHeight
// collisions are reported by DetectConstraintConflicts for all LayoutNode types.
static void DetectExplicitSizeShadowedByOppositeEdge(const LayoutNode* node, int sourceLine,
                                                     std::vector<VerifyDiagnostic>& diagnostics) {
  bool wFromConstraints = !std::isnan(node->left) && !std::isnan(node->right);
  bool hFromConstraints = !std::isnan(node->top) && !std::isnan(node->bottom);
  if (wFromConstraints && !std::isnan(node->width)) {
    AddDiagnostic(diagnostics, sourceLine,
                  "width ignored, left+right constraints take priority. Fix: remove width");
  }
  if (hFromConstraints && !std::isnan(node->height)) {
    AddDiagnostic(diagnostics, sourceLine,
                  "height ignored, top+bottom constraints take priority. Fix: remove height");
  }
}

static void DetectRedundantZeroConstraints(const LayoutNode* node, int sourceLine,
                                           std::vector<VerifyDiagnostic>& diagnostics) {
  if (!std::isnan(node->left) && node->left == 0.0f && std::isnan(node->right) &&
      std::isnan(node->centerX)) {
    AddDiagnostic(diagnostics, sourceLine,
                  "left=\"0\" redundant, same as default positioning. Fix: remove left");
  }
  if (!std::isnan(node->top) && node->top == 0.0f && std::isnan(node->bottom) &&
      std::isnan(node->centerY)) {
    AddDiagnostic(diagnostics, sourceLine,
                  "top=\"0\" redundant, same as default positioning. Fix: remove top");
  }
}

static void DetectStretchFillAffectedByPadding(const std::vector<Element*>& elements,
                                               const Padding& padding,
                                               std::vector<VerifyDiagnostic>& diagnostics) {
  if (padding.isZero()) {
    return;
  }
  for (auto* element : elements) {
    auto type = element->nodeType();
    if (type != NodeType::Rectangle && type != NodeType::Ellipse) {
      continue;
    }
    auto* layoutNode = LayoutNode::AsLayoutNode(element);
    if (layoutNode == nullptr) {
      continue;
    }
    bool hStretch = layoutNode->left == 0.0f && layoutNode->right == 0.0f &&
                    (padding.left != 0 || padding.right != 0);
    bool vStretch = layoutNode->top == 0.0f && layoutNode->bottom == 0.0f &&
                    (padding.top != 0 || padding.bottom != 0);
    if (hStretch || vStretch) {
      AddDiagnostic(diagnostics, element->sourceLine,
                    "stretch-fill element affected by padding, background will be inset. Fix: use "
                    "nested container — move background to an outer Layer without padding");
    }
  }
}

static void DetectChildExceedingParent(const Layer* parentLayer,
                                       std::vector<VerifyDiagnostic>& diagnostics) {
  if (parentLayer->clipToBounds || parentLayer->hasScrollRect || parentLayer->mask != nullptr) {
    return;
  }
  auto parentBounds = parentLayer->layoutBounds();
  if (parentBounds.width <= 0 || parentBounds.height <= 0) {
    return;
  }
  SpatialRect parent = {0, 0, parentBounds.width, parentBounds.height};
  for (auto* child : parentLayer->children) {
    auto childBounds = child->layoutBounds();
    if (childBounds.width <= 0 || childBounds.height <= 0) {
      continue;
    }
    SpatialRect childRect = {childBounds.x, childBounds.y, childBounds.width, childBounds.height};
    if (!IsFullyContained(parent, childRect)) {
      AddDiagnostic(
          diagnostics, child->sourceLine,
          "child extends beyond parent bounds " +
              FormatBounds(childBounds.x, childBounds.y, childBounds.width, childBounds.height) +
              " outside " + FormatBounds(0, 0, parentBounds.width, parentBounds.height) +
              ". Fix: adjust size/position, move child to a sibling layer, or add clipToBounds");
    }
  }
}

static void DetectContainerOverflow(const Layer* parentLayer,
                                    std::vector<VerifyDiagnostic>& diagnostics) {
  if (parentLayer->layout == LayoutMode::None) {
    return;
  }
  bool horizontal = parentLayer->layout == LayoutMode::Horizontal;
  float explicitMain = horizontal ? parentLayer->width : parentLayer->height;
  float percentMain = horizontal ? parentLayer->percentWidth : parentLayer->percentHeight;
  bool mainFromConstraints =
      horizontal ? (!std::isnan(parentLayer->left) && !std::isnan(parentLayer->right))
                 : (!std::isnan(parentLayer->top) && !std::isnan(parentLayer->bottom));
  if (std::isnan(explicitMain) && std::isnan(percentMain) && !mainFromConstraints) {
    return;
  }
  auto parentBounds = parentLayer->layoutBounds();
  float parentMainSize = horizontal ? parentBounds.width : parentBounds.height;
  if (parentMainSize <= 0) {
    return;
  }
  float paddingStart = horizontal ? parentLayer->padding.left : parentLayer->padding.top;
  float paddingEnd = horizontal ? parentLayer->padding.right : parentLayer->padding.bottom;
  float availableMain = parentMainSize - paddingStart - paddingEnd;

  float totalFixed = 0;
  size_t visibleCount = 0;
  bool hasFlex = false;
  for (auto* child : parentLayer->children) {
    if (!child->includeInLayout) {
      continue;
    }
    visibleCount++;
    float childExplicitMain = horizontal ? child->width : child->height;
    float childPercentMain = horizontal ? child->percentWidth : child->percentHeight;
    if (!std::isnan(childExplicitMain)) {
      totalFixed += childExplicitMain;
    } else if (!std::isnan(childPercentMain)) {
      // A percentage-sized child scales with the parent; its exact contribution cannot be
      // determined statically. Treat it as 0 (optimistic) so fixed-child overflow from the
      // remaining siblings is still detectable.
      continue;
    } else if (child->flex > 0) {
      hasFlex = true;
    } else {
      auto childBounds = child->layoutBounds();
      float measured = horizontal ? childBounds.width : childBounds.height;
      totalFixed += measured;
    }
  }
  if (hasFlex) {
    return;
  }
  float totalGap = parentLayer->gap * static_cast<float>(visibleCount > 1 ? visibleCount - 1 : 0);
  float totalRequired = totalFixed + totalGap;
  static constexpr float TOLERANCE = 0.5f;
  if (totalRequired > availableMain + TOLERANCE) {
    AddDiagnostic(diagnostics, parentLayer->sourceLine,
                  "children overflow parent: total " +
                      std::to_string(static_cast<int>(totalRequired)) + "px exceeds available " +
                      std::to_string(static_cast<int>(availableMain)) +
                      "px. Fix: check children sizes, gap, or parent size");
  }
}

static void DetectNegativeConstraintSize(const Layer* layer, float containerW, float containerH,
                                         std::vector<VerifyDiagnostic>& diagnostics) {
  if (!std::isnan(layer->left) && !std::isnan(layer->right) && !std::isnan(containerW)) {
    float derived = containerW - layer->left - layer->right;
    if (derived < 0) {
      AddDiagnostic(diagnostics, layer->sourceLine,
                    "left=" + std::to_string(static_cast<int>(layer->left)) +
                        " + right=" + std::to_string(static_cast<int>(layer->right)) + " = " +
                        std::to_string(static_cast<int>(layer->left + layer->right)) +
                        " exceeds parent width " + std::to_string(static_cast<int>(containerW)) +
                        ", negative derived size. Fix: check left/right values");
    }
  }
  if (!std::isnan(layer->top) && !std::isnan(layer->bottom) && !std::isnan(containerH)) {
    float derived = containerH - layer->top - layer->bottom;
    if (derived < 0) {
      AddDiagnostic(diagnostics, layer->sourceLine,
                    "top=" + std::to_string(static_cast<int>(layer->top)) +
                        " + bottom=" + std::to_string(static_cast<int>(layer->bottom)) + " = " +
                        std::to_string(static_cast<int>(layer->top + layer->bottom)) +
                        " exceeds parent height " + std::to_string(static_cast<int>(containerH)) +
                        ", negative derived size. Fix: check top/bottom values");
    }
  }
}

static void DetectNegativeConstraintSizeForElement(const LayoutNode* layoutNode, int sourceLine,
                                                   float containerW, float containerH,
                                                   std::vector<VerifyDiagnostic>& diagnostics) {
  if (layoutNode == nullptr) {
    return;
  }
  if (!std::isnan(layoutNode->left) && !std::isnan(layoutNode->right) && !std::isnan(containerW)) {
    float derived = containerW - layoutNode->left - layoutNode->right;
    if (derived < 0) {
      AddDiagnostic(diagnostics, sourceLine,
                    "left=" + std::to_string(static_cast<int>(layoutNode->left)) +
                        " + right=" + std::to_string(static_cast<int>(layoutNode->right)) + " = " +
                        std::to_string(static_cast<int>(layoutNode->left + layoutNode->right)) +
                        " exceeds parent width " + std::to_string(static_cast<int>(containerW)) +
                        ", negative derived size. Fix: check left/right values");
    }
  }
  if (!std::isnan(layoutNode->top) && !std::isnan(layoutNode->bottom) && !std::isnan(containerH)) {
    float derived = containerH - layoutNode->top - layoutNode->bottom;
    if (derived < 0) {
      AddDiagnostic(diagnostics, sourceLine,
                    "top=" + std::to_string(static_cast<int>(layoutNode->top)) + " + bottom=" +
                        std::to_string(static_cast<int>(layoutNode->bottom)) + " = " +
                        std::to_string(static_cast<int>(layoutNode->top + layoutNode->bottom)) +
                        " exceeds parent height " + std::to_string(static_cast<int>(containerH)) +
                        ", negative derived size. Fix: check top/bottom values");
    }
  }
}

static bool IsContentMeasuredContainer(const Group* group) {
  if (!std::isnan(group->width) || !std::isnan(group->height)) {
    return false;
  }
  if (!std::isnan(group->percentWidth) || !std::isnan(group->percentHeight)) {
    return false;
  }
  bool wFromConstraints = !std::isnan(group->left) && !std::isnan(group->right);
  bool hFromConstraints = !std::isnan(group->top) && !std::isnan(group->bottom);
  return !wFromConstraints && !hFromConstraints;
}

static bool IsContentMeasuredContainer(const Layer* layer, const Layer* parentLayer) {
  if (!std::isnan(layer->width) || !std::isnan(layer->height)) {
    return false;
  }
  if (!std::isnan(layer->percentWidth) || !std::isnan(layer->percentHeight)) {
    return false;
  }
  bool wFromConstraints = !std::isnan(layer->left) && !std::isnan(layer->right);
  bool hFromConstraints = !std::isnan(layer->top) && !std::isnan(layer->bottom);
  if (wFromConstraints || hFromConstraints) {
    return false;
  }
  if (layer->flex > 0 && parentLayer != nullptr && parentLayer->layout != LayoutMode::None &&
      layer->includeInLayout) {
    return false;
  }
  return true;
}

static void DetectIneffectiveCentering(const std::vector<Element*>& elements,
                                       std::vector<VerifyDiagnostic>& diagnostics) {
  for (auto* element : elements) {
    auto* layoutNode = LayoutNode::AsLayoutNode(element);
    if (layoutNode == nullptr) {
      continue;
    }
    bool hasCenterX = !std::isnan(layoutNode->centerX);
    bool hasCenterY = !std::isnan(layoutNode->centerY);
    if (!hasCenterX && !hasCenterY) {
      continue;
    }
    auto type = element->nodeType();
    if (type == NodeType::Group || type == NodeType::TextBox) {
      continue;
    }
    if (hasCenterX) {
      AddDiagnostic(diagnostics, element->sourceLine,
                    "centerX ineffective: parent is content-measured, centering relative to own "
                    "size. Fix: move centerX to the Group/Layer itself");
    }
    if (hasCenterY) {
      AddDiagnostic(diagnostics, element->sourceLine,
                    "centerY ineffective: parent is content-measured, centering relative to own "
                    "size. Fix: move centerY to the Group/Layer itself");
    }
  }
}

// Computes the union bounding box of all direct children (LayoutNodes) within a container, then
// checks whether that union box is centered in the container. Applies when the container itself is
// positioned with centerX="0" or centerY="0" — a visually off-center union bbox inside such a
// container defeats the purpose of centering.
static void CheckCenteringMargins(const Padding& padding, float containerW, float containerH,
                                  float minX, float minY, float maxX, float maxY, bool checkX,
                                  bool checkY, int sourceLine,
                                  std::vector<VerifyDiagnostic>& diagnostics) {
  static constexpr float TOLERANCE = 2.0f;
  bool xAsymmetric = false;
  bool yAsymmetric = false;
  int leftM = 0, rightM = 0, topM = 0, bottomM = 0;
  if (checkX && containerW > 0) {
    float leftMargin = minX - padding.left;
    float rightMargin = (containerW - padding.right) - maxX;
    float diff = std::abs(leftMargin - rightMargin);
    if (diff > TOLERANCE && (leftMargin > TOLERANCE || rightMargin > TOLERANCE)) {
      xAsymmetric = true;
      leftM = static_cast<int>(leftMargin);
      rightM = static_cast<int>(rightMargin);
    }
  }
  if (checkY && containerH > 0) {
    float topMargin = minY - padding.top;
    float bottomMargin = (containerH - padding.bottom) - maxY;
    float diff = std::abs(topMargin - bottomMargin);
    if (diff > TOLERANCE && (topMargin > TOLERANCE || bottomMargin > TOLERANCE)) {
      yAsymmetric = true;
      topM = static_cast<int>(topMargin);
      bottomM = static_cast<int>(bottomMargin);
    }
  }
  if (!xAsymmetric && !yAsymmetric) {
    return;
  }
  std::string msg = "visual bounds of all contents not centered in container:";
  if (xAsymmetric) {
    msg += " left margin " + std::to_string(leftM) + " != right margin " + std::to_string(rightM);
  }
  if (xAsymmetric && yAsymmetric) {
    msg += ",";
  }
  if (yAsymmetric) {
    msg += " top margin " + std::to_string(topM) + " != bottom margin " + std::to_string(bottomM);
  }
  msg += ". Fix: adjust content to center within the container";
  AddDiagnostic(diagnostics, sourceLine, msg);
}

// Recursively accumulates the pixel-level union bounding box of all leaf VectorElements within a
// container, translating coordinates through nested Groups and child Layers. This gives the true
// visual extent of rendered content, regardless of how many container layers are nested in between.
static void AccumulatePixelBounds(const std::vector<Element*>& elements, float offsetX,
                                  float offsetY, float& minX, float& minY, float& maxX, float& maxY,
                                  bool& hasLeaf);

static void AccumulatePixelBoundsForLayer(const Layer* layer, float offsetX, float offsetY,
                                          float& minX, float& minY, float& maxX, float& maxY,
                                          bool& hasLeaf) {
  auto bounds = layer->layoutBounds();
  float childOffsetX = offsetX + bounds.x;
  float childOffsetY = offsetY + bounds.y;
  AccumulatePixelBounds(layer->contents, childOffsetX, childOffsetY, minX, minY, maxX, maxY,
                        hasLeaf);
  for (auto* child : layer->children) {
    AccumulatePixelBoundsForLayer(child, childOffsetX, childOffsetY, minX, minY, maxX, maxY,
                                  hasLeaf);
  }
}

static void AccumulatePixelBounds(const std::vector<Element*>& elements, float offsetX,
                                  float offsetY, float& minX, float& minY, float& maxX, float& maxY,
                                  bool& hasLeaf) {
  for (auto* element : elements) {
    auto type = element->nodeType();
    if (type == NodeType::Group) {
      auto* group = static_cast<const Group*>(element);
      auto bounds = group->layoutBounds();
      AccumulatePixelBounds(group->elements, offsetX + bounds.x, offsetY + bounds.y, minX, minY,
                            maxX, maxY, hasLeaf);
      continue;
    }
    auto* layoutNode = LayoutNode::AsLayoutNode(element);
    if (layoutNode == nullptr) {
      continue;
    }
    auto bounds = layoutNode->layoutBounds();
    if (bounds.width <= 0 && bounds.height <= 0) {
      continue;
    }
    float absX = offsetX + bounds.x;
    float absY = offsetY + bounds.y;
    minX = std::min(minX, absX);
    minY = std::min(minY, absY);
    maxX = std::max(maxX, absX + bounds.width);
    maxY = std::max(maxY, absY + bounds.height);
    hasLeaf = true;
  }
}

static void DetectContentCenteringAsymmetry(const Layer* layer, const Layer* parentLayer,
                                            std::vector<VerifyDiagnostic>& diagnostics) {
  bool checkX = !std::isnan(layer->centerX) && layer->centerX == 0.0f;
  bool checkY = !std::isnan(layer->centerY) && layer->centerY == 0.0f;
  if (!checkX && !checkY) {
    return;
  }
  if (IsContentMeasuredContainer(layer, parentLayer)) {
    return;
  }
  auto layerBounds = layer->layoutBounds();
  if (layerBounds.width <= 0 || layerBounds.height <= 0) {
    return;
  }
  float minX = FLT_MAX;
  float minY = FLT_MAX;
  float maxX = -FLT_MAX;
  float maxY = -FLT_MAX;
  bool hasLeaf = false;
  AccumulatePixelBounds(layer->contents, 0, 0, minX, minY, maxX, maxY, hasLeaf);
  for (auto* child : layer->children) {
    AccumulatePixelBoundsForLayer(child, 0, 0, minX, minY, maxX, maxY, hasLeaf);
  }
  if (!hasLeaf) {
    return;
  }
  CheckCenteringMargins(layer->padding, layerBounds.width, layerBounds.height, minX, minY, maxX,
                        maxY, checkX, checkY, layer->sourceLine, diagnostics);
}

static void DetectContentCenteringAsymmetry(const Group* group,
                                            std::vector<VerifyDiagnostic>& diagnostics) {
  bool checkX = !std::isnan(group->centerX) && group->centerX == 0.0f;
  bool checkY = !std::isnan(group->centerY) && group->centerY == 0.0f;
  if (!checkX && !checkY) {
    return;
  }
  if (IsContentMeasuredContainer(group)) {
    return;
  }
  auto groupBounds = group->layoutBounds();
  if (groupBounds.width <= 0 || groupBounds.height <= 0) {
    return;
  }
  float minX = FLT_MAX;
  float minY = FLT_MAX;
  float maxX = -FLT_MAX;
  float maxY = -FLT_MAX;
  bool hasLeaf = false;
  AccumulatePixelBounds(group->elements, 0, 0, minX, minY, maxX, maxY, hasLeaf);
  if (!hasLeaf) {
    return;
  }
  CheckCenteringMargins(group->padding, groupBounds.width, groupBounds.height, minX, minY, maxX,
                        maxY, checkX, checkY, group->sourceLine, diagnostics);
}

static void DetectNestedTextBox(const TextBox* textBox, const TextBox* parentTextBox,
                                std::vector<VerifyDiagnostic>& diagnostics) {
  if (parentTextBox != nullptr) {
    AddDiagnostic(diagnostics, textBox->sourceLine,
                  "nested TextBox, inner layout overridden by outer TextBox. "
                  "Fix: check if inner TextBox should be removed");
  }
}

static void RunSpatialDetectionOnElements(const std::vector<Element*>& elements,
                                          const TextBox* parentTextBox, float containerW,
                                          float containerH,
                                          std::vector<VerifyDiagnostic>& diagnostics);

static void RunSpatialDetectionOnElements(const std::vector<Element*>& elements,
                                          const TextBox* parentTextBox, float containerW,
                                          float containerH,
                                          std::vector<VerifyDiagnostic>& diagnostics) {
  for (auto* element : elements) {
    auto type = element->nodeType();
    auto* layoutNode = LayoutNode::AsLayoutNode(element);
    if (layoutNode != nullptr) {
      DetectConstraintConflicts(layoutNode, element->sourceLine, diagnostics);
      DetectNegativeConstraintSizeForElement(layoutNode, element->sourceLine, containerW,
                                             containerH, diagnostics);
      DetectRedundantZeroConstraints(layoutNode, element->sourceLine, diagnostics);
      // Only Rectangle / Ellipse / Group use width/height purely for layout sizing. TextBox keeps
      // this->width/height as the authored text-box dimensions for shaping (independent semantic),
      // so it is excluded here. Path / Polystar / Text / TextPath use width/height to drive
      // uniform scaling, still useful alongside edge constraints; they are also excluded. Note:
      // percentWidth/Height collisions with opposite edges are reported by
      // DetectConstraintConflicts for all LayoutNode types.
      if (type == NodeType::Rectangle || type == NodeType::Ellipse || type == NodeType::Group) {
        DetectExplicitSizeShadowedByOppositeEdge(layoutNode, element->sourceLine, diagnostics);
      }
    }
    if (type == NodeType::Group) {
      auto* group = static_cast<const Group*>(element);
      if (IsContentMeasuredContainer(group)) {
        DetectIneffectiveCentering(group->elements, diagnostics);
      }
      DetectZeroSizeGroup(group, diagnostics);
      DetectContentOriginOffsetForGroup(group, diagnostics);
      DetectContentCenteringAsymmetry(group, diagnostics);
      DetectStretchFillAffectedByPadding(group->elements, group->padding, diagnostics);
      auto bounds = group->layoutBounds();
      RunSpatialDetectionOnElements(group->elements, parentTextBox, bounds.width, bounds.height,
                                    diagnostics);
    } else if (type == NodeType::TextBox) {
      auto* textBox = static_cast<const TextBox*>(element);
      DetectNestedTextBox(textBox, parentTextBox, diagnostics);
      if (IsContentMeasuredContainer(textBox)) {
        DetectIneffectiveCentering(textBox->elements, diagnostics);
      }
      DetectZeroSizeGroup(textBox, diagnostics);
      DetectContentOriginOffsetForGroup(textBox, diagnostics);
      DetectContentCenteringAsymmetry(textBox, diagnostics);
      DetectStretchFillAffectedByPadding(textBox->elements, textBox->padding, diagnostics);
      auto bounds = textBox->layoutBounds();
      RunSpatialDetectionOnElements(textBox->elements, textBox, bounds.width, bounds.height,
                                    diagnostics);
    }
  }
}

static void RunSpatialDetectionOnLayer(const Layer* layer, const Layer* parentLayer,
                                       const Layer* grandparentLayer, const SpatialRect& clipRect,
                                       std::vector<VerifyDiagnostic>& diagnostics) {
  auto layerBounds = layer->layoutBounds();

  // Check if this layer is completely outside the inherited clip region.
  if (layerBounds.width > 0 && layerBounds.height > 0 && clipRect.width > 0 &&
      clipRect.height > 0 && HasLeafContent(layer)) {
    SpatialRect layerRect = {layerBounds.x, layerBounds.y, layerBounds.width, layerBounds.height};
    if (!RectsOverlap(layerRect, clipRect)) {
      AddDiagnostic(diagnostics, layer->sourceLine,
                    "completely outside visible region, not visible. "
                    "Fix: check if this Layer should be repositioned or removed");
    }
  }

  DetectZeroSize(layer, parentLayer, diagnostics);
  DetectConstraintsIgnoredByLayout(layer, parentLayer, diagnostics);
  DetectFlexInContentMeasuredParent(layer, parentLayer, grandparentLayer, diagnostics);
  DetectContentOriginOffset(layer, parentLayer, diagnostics);
  DetectContentCenteringAsymmetry(layer, parentLayer, diagnostics);
  DetectOverlappingSiblings(layer, diagnostics);
  DetectConstraintConflicts(layer, layer->sourceLine, diagnostics);
  DetectExplicitSizeShadowedByOppositeEdge(layer, layer->sourceLine, diagnostics);
  DetectRedundantZeroConstraints(layer, layer->sourceLine, diagnostics);
  DetectStretchFillAffectedByPadding(layer->contents, layer->padding, diagnostics);
  DetectChildExceedingParent(layer, diagnostics);
  DetectContainerOverflow(layer, diagnostics);

  auto parentBounds = parentLayer != nullptr ? parentLayer->layoutBounds() : Rect{0, 0, 0, 0};
  float containerW = parentLayer != nullptr ? parentBounds.width : clipRect.width;
  float containerH = parentLayer != nullptr ? parentBounds.height : clipRect.height;
  if (parentLayer == nullptr ||
      (parentLayer->layout == LayoutMode::None || !layer->includeInLayout)) {
    DetectNegativeConstraintSize(layer, containerW, containerH, diagnostics);
  }

  if (IsContentMeasuredContainer(layer, parentLayer)) {
    DetectIneffectiveCentering(layer->contents, diagnostics);
  }

  RunSpatialDetectionOnElements(layer->contents, nullptr, layerBounds.width, layerBounds.height,
                                diagnostics);

  // Compute the clip region for children: translate to this layer's local coords,
  // then intersect with this layer's bounds if it clips.
  // For mask, this conservatively assumes the mask clips to the layer's bounds.
  SpatialRect childClip = {clipRect.x - layerBounds.x, clipRect.y - layerBounds.y, clipRect.width,
                           clipRect.height};
  if (layer->clipToBounds || layer->hasScrollRect || layer->mask != nullptr) {
    float cx1 = std::max(childClip.x, 0.0f);
    float cy1 = std::max(childClip.y, 0.0f);
    float cx2 = std::min(childClip.x + childClip.width, layerBounds.width);
    float cy2 = std::min(childClip.y + childClip.height, layerBounds.height);
    childClip = {cx1, cy1, std::max(cx2 - cx1, 0.0f), std::max(cy2 - cy1, 0.0f)};
  }

  for (auto* child : layer->children) {
    RunSpatialDetectionOnLayer(child, layer, parentLayer, childClip, diagnostics);
  }
}

static void RunSpatialDetection(const PAGXDocument* doc, std::vector<VerifyDiagnostic>& diagnostics,
                                const Layer* targetLayer = nullptr) {
  SpatialRect canvasClip = {0, 0, static_cast<float>(doc->width), static_cast<float>(doc->height)};
  if (targetLayer != nullptr) {
    RunSpatialDetectionOnLayer(targetLayer, nullptr, nullptr, canvasClip, diagnostics);
  } else {
    for (auto* layer : doc->layers) {
      RunSpatialDetectionOnLayer(layer, nullptr, nullptr, canvasClip, diagnostics);
    }
  }
}

// ============================================================================
// Output
// ============================================================================

static bool CompareDiagnosticsByLine(const VerifyDiagnostic& a, const VerifyDiagnostic& b) {
  int lineA = a.lines.empty() ? 0 : a.lines[0];
  int lineB = b.lines.empty() ? 0 : b.lines[0];
  return lineA < lineB;
}

static void SortDiagnostics(std::vector<VerifyDiagnostic>& diagnostics) {
  std::sort(diagnostics.begin(), diagnostics.end(), CompareDiagnosticsByLine);
}

static void PrintDiagnosticsText(const std::vector<VerifyDiagnostic>& diagnostics,
                                 const std::string& file) {
  for (const auto& diag : diagnostics) {
    std::cerr << file << ":";
    if (!diag.lines.empty()) {
      for (size_t i = 0; i < diag.lines.size(); i++) {
        if (i > 0) {
          std::cerr << ",";
        }
        std::cerr << diag.lines[i];
      }
    }
    std::cerr << ": " << diag.message << "\n";
  }
}

static void PrintDiagnosticsJson(const std::vector<VerifyDiagnostic>& diagnostics,
                                 const std::string& file) {
  std::cout << "{\"file\": \"" << EscapeJson(file)
            << "\", \"ok\": " << (diagnostics.empty() ? "true" : "false") << ", \"diagnostics\": [";
  for (size_t i = 0; i < diagnostics.size(); i++) {
    if (i > 0) {
      std::cout << ", ";
    }
    std::cout << "{\"lines\": [";
    for (size_t j = 0; j < diagnostics[i].lines.size(); j++) {
      if (j > 0) {
        std::cout << ", ";
      }
      std::cout << diagnostics[i].lines[j];
    }
    std::cout << "], \"message\": \"" << EscapeJson(diagnostics[i].message) << "\"}";
  }
  std::cout << "]}\n";
}

// ============================================================================
// Command
// ============================================================================

static void PrintUsage() {
  std::cout << "Usage: pagx verify [options] <file.pagx>\n"
            << "\n"
            << "Validate a PAGX file: run all checks, optionally output layout XML\n"
            << "(with bounds) and render screenshot.\n"
            << "\n"
            << "Options:\n"
            << "  --id <id>           Limit scope to the Layer with the specified id\n"
            << "  --scale <float>     Render scale factor (default: 1.0)\n"
            << "  --skip-render       Skip screenshot generation\n"
            << "  --skip-layout       Skip layout XML generation\n"
            << "  --json              Output diagnostics in JSON format\n"
            << "  -h, --help          Show this help message\n";
}

static int ParseOptions(int argc, char* argv[], VerifyOptions* opts) {
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--id") == 0) {
      if (i + 1 >= argc) {
        std::cerr << "pagx verify: --id requires an argument\n";
        return 1;
      }
      opts->id = argv[++i];
    } else if (strcmp(argv[i], "--scale") == 0) {
      if (i + 1 >= argc) {
        std::cerr << "pagx verify: --scale requires an argument\n";
        return 1;
      }
      opts->scale = static_cast<float>(atof(argv[++i]));
      if (opts->scale <= 0.0f || !std::isfinite(opts->scale)) {
        std::cerr << "pagx verify: --scale must be a positive number\n";
        return 1;
      }
    } else if (strcmp(argv[i], "--skip-render") == 0) {
      opts->skipRender = true;
    } else if (strcmp(argv[i], "--skip-layout") == 0) {
      opts->skipLayout = true;
    } else if (strcmp(argv[i], "--json") == 0) {
      opts->jsonOutput = true;
    } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
      PrintUsage();
      return -1;
    } else if (argv[i][0] == '-') {
      std::cerr << "pagx verify: unknown option '" << argv[i] << "'\n";
      return 1;
    } else {
      opts->inputFile = argv[i];
    }
  }
  if (opts->inputFile.empty()) {
    std::cerr << "pagx verify: missing input file\n";
    return 1;
  }
  return 0;
}

int RunVerify(int argc, char* argv[]) {
  VerifyOptions opts;
  int rc = ParseOptions(argc, argv, &opts);
  if (rc != 0) {
    return rc == -1 ? 0 : rc;
  }

  // Step 1: Resolve all import directives in the file (full file, idempotent).
  std::vector<std::string> resolveArgs = {"pagx-verify", opts.inputFile};
  std::vector<char*> resolveArgv;
  for (auto& arg : resolveArgs) {
    resolveArgv.push_back(const_cast<char*>(arg.c_str()));
  }
  resolveArgv.push_back(nullptr);
  if (RunResolve(static_cast<int>(resolveArgs.size()), resolveArgv.data()) != 0) {
    std::cerr << "pagx verify: warning: resolve failed for '" << opts.inputFile << "'\n";
  }

  // Step 2: Load document and compute layout.
  auto doc = LoadDocument(opts.inputFile, "pagx verify");
  if (doc == nullptr) {
    return 1;
  }

  doc->applyLayout();

  // Parse the XML DOM for signature-based duplicate detection.
  auto xmlDoc = xmlReadFile(opts.inputFile.c_str(), nullptr, XML_PARSE_NONET | XML_PARSE_NOBLANKS);
  LineNodeMap lineNodeMap;
  if (xmlDoc != nullptr) {
    auto* root = xmlDocGetRootElement(xmlDoc);
    if (root != nullptr) {
      BuildLineNodeMap(root, lineNodeMap);
    }
  }

  const Layer* targetLayer = nullptr;
  if (!opts.id.empty()) {
    targetLayer = doc->findNode<Layer>(opts.id);
    if (targetLayer == nullptr) {
      std::cerr << "pagx verify: node not found: " << opts.id << "\n";
      if (xmlDoc != nullptr) {
        xmlFreeDoc(xmlDoc);
      }
      return 1;
    }
  }

  std::vector<VerifyDiagnostic> diagnostics;
  if (xmlDoc != nullptr) {
    RunXsdValidation(xmlDoc, diagnostics);
  }
  RunStaticDetection(doc.get(), lineNodeMap, diagnostics, targetLayer);
  RunSpatialDetection(doc.get(), diagnostics, targetLayer);

  if (xmlDoc != nullptr) {
    xmlFreeDoc(xmlDoc);
  }

  SortDiagnostics(diagnostics);

  if (opts.jsonOutput) {
    PrintDiagnosticsJson(diagnostics, opts.inputFile);
  } else if (!diagnostics.empty()) {
    PrintDiagnosticsText(diagnostics, opts.inputFile);
  }

  std::string baseName = GetBaseName(opts.inputFile);
  std::string suffix = opts.id.empty() ? "" : ("." + opts.id);

  if (!opts.skipLayout) {
    std::string layoutPath = baseName + suffix + ".layout.xml";
    auto layoutXml = GenerateLayoutXml(doc.get(), targetLayer);
    if (!WriteStringToFile(layoutXml, layoutPath, "pagx verify")) {
      return 1;
    }
  }

  if (opts.skipRender) {
    return diagnostics.empty() ? 0 : 1;
  }

  std::string pngPath = baseName + suffix + ".png";
  std::vector<std::string> renderArgs = {"pagx", "render", "--scale", std::to_string(opts.scale),
                                         "-o",   pngPath};
  if (!opts.id.empty()) {
    renderArgs.push_back("--id");
    renderArgs.push_back(opts.id);
  }
  renderArgs.push_back(opts.inputFile);

  std::vector<char*> renderArgv;
  for (auto& arg : renderArgs) {
    renderArgv.push_back(const_cast<char*>(arg.c_str()));
  }
  renderArgv.push_back(nullptr);

  auto bitmap = RenderToBitmap(static_cast<int>(renderArgs.size()), renderArgv.data());
  if (!bitmap.isEmpty()) {
    tgfx::Pixmap pixmap(bitmap);
    auto encodedData = tgfx::ImageCodec::Encode(pixmap, tgfx::EncodedFormat::PNG, 100);
    if (encodedData != nullptr) {
      std::ofstream pngFile(pngPath, std::ios::binary);
      if (pngFile.is_open()) {
        pngFile.write(reinterpret_cast<const char*>(encodedData->data()),
                      static_cast<std::streamsize>(encodedData->size()));
      }
    }
    std::cerr << "Wrote " << pngPath << " (" << bitmap.width() << "x" << bitmap.height() << " @"
              << opts.scale << "x)\n";
  }

  return diagnostics.empty() ? 0 : 1;
}

}  // namespace pagx::cli
