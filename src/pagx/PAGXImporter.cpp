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

#include "pagx/PAGXImporter.h"
#include <climits>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <vector>
#include "base/utils/Log.h"
#include "pagx/PAGXDefaults.h"
#include "pagx/nodes/BackgroundBlurStyle.h"
#include "pagx/nodes/BlendFilter.h"
#include "pagx/nodes/BlurFilter.h"
#include "pagx/nodes/ColorMatrixFilter.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/ConicGradient.h"
#include "pagx/nodes/DiamondGradient.h"
#include "pagx/nodes/DropShadowFilter.h"
#include "pagx/nodes/DropShadowStyle.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Font.h"
#include "pagx/nodes/GlyphRun.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/InnerShadowFilter.h"
#include "pagx/nodes/InnerShadowStyle.h"
#include "pagx/nodes/LinearGradient.h"
#include "pagx/nodes/MergePath.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/PathData.h"
#include "pagx/nodes/Polystar.h"
#include "pagx/nodes/RadialGradient.h"
#include "pagx/nodes/RangeSelector.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/Repeater.h"
#include "pagx/nodes/RoundCorner.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextBox.h"
#include "pagx/nodes/TextModifier.h"
#include "pagx/nodes/TextPath.h"
#include "pagx/nodes/TrimPath.h"
#include "pagx/svg/SVGPathParser.h"
#include "pagx/types/Color.h"
#include "pagx/utils/Base64.h"
#include "pagx/utils/StringParser.h"
#include "pagx/xml/XMLDOM.h"

namespace pagx {

//==============================================================================
// Forward declarations and utility functions
//==============================================================================

static const std::string& EmptyString() {
  static const std::string empty = {};
  return empty;
}

static void reportError(PAGXDocument* doc, const DOMNode* node, const std::string& message) {
  auto fullMessage = "line " + std::to_string(node->line) + ": " + message;
  doc->errors.push_back(fullMessage);
#if DEBUG
  LOGE("%s", fullMessage.c_str());
#endif
}

static const std::string& getAttribute(const DOMNode* node, const std::string& name,
                                       const std::string& defaultValue = EmptyString());
static float getFloatAttribute(const DOMNode* node, const std::string& name, float defaultValue = 0,
                               PAGXDocument* doc = nullptr);
static int getIntAttribute(const DOMNode* node, const std::string& name, int defaultValue = 0,
                           PAGXDocument* doc = nullptr);
static bool getBoolAttribute(const DOMNode* node, const std::string& name,
                             bool defaultValue = false, PAGXDocument* doc = nullptr);
static Point parsePoint(const std::string& str, bool* outValid = nullptr);
static Size parseSize(const std::string& str, bool* outValid = nullptr);
static Rect parseRect(const std::string& str, bool* outValid = nullptr);
static Color parseColor(const std::string& str, bool* outValid = nullptr);
static Point getPointAttribute(const DOMNode* node, const char* name, Point defaultValue,
                               PAGXDocument* doc);
static Size getSizeAttribute(const DOMNode* node, const char* name, Size defaultValue,
                             PAGXDocument* doc);
static Rect getRectAttribute(const DOMNode* node, const char* name, Rect defaultValue,
                             PAGXDocument* doc);
static Color getColorAttribute(const DOMNode* node, const char* name, PAGXDocument* doc);

template <typename EnumType>
static EnumType getEnumAttribute(const DOMNode* node, const char* name,
                                 const std::string& defaultStr, PAGXDocument* doc,
                                 EnumType (*fromString)(const std::string&),
                                 bool (*isValid)(const std::string&)) {
  auto& str = getAttribute(node, name, defaultStr);
  if (!isValid(str)) {
    reportError(doc, node,
                "Invalid value '" + str + "' for '" + std::string(name) + "' attribute.");
    return fromString(defaultStr);
  }
  return fromString(str);
}

#define GET_ENUM(node, name, defaultStr, doc, EnumType)                         \
  getEnumAttribute<EnumType>(node, name, defaultStr, doc, EnumType##FromString, \
                             IsValid##EnumType##String)

// Forward declarations for parse functions
static void parseDocument(const DOMNode* root, PAGXDocument* doc);
static void parseResources(const DOMNode* node, PAGXDocument* doc);
static Layer* parseLayer(const DOMNode* node, PAGXDocument* doc);
static void parseContents(const DOMNode* node, Layer* layer, PAGXDocument* doc);
static void parseStyles(const DOMNode* node, Layer* layer, PAGXDocument* doc);
static void parseFilters(const DOMNode* node, Layer* layer, PAGXDocument* doc);
static Element* parseElement(const DOMNode* node, PAGXDocument* doc);
static ColorSource* parseColorSource(const DOMNode* node, PAGXDocument* doc);
static LayerStyle* parseLayerStyle(const DOMNode* node, PAGXDocument* doc);
static LayerFilter* parseLayerFilter(const DOMNode* node, PAGXDocument* doc);
static Rectangle* parseRectangle(const DOMNode* node, PAGXDocument* doc);
static Ellipse* parseEllipse(const DOMNode* node, PAGXDocument* doc);
static Polystar* parsePolystar(const DOMNode* node, PAGXDocument* doc);
static Path* parsePath(const DOMNode* node, PAGXDocument* doc);
static Text* parseText(const DOMNode* node, PAGXDocument* doc);
static Fill* parseFill(const DOMNode* node, PAGXDocument* doc);
static Stroke* parseStroke(const DOMNode* node, PAGXDocument* doc);
static TrimPath* parseTrimPath(const DOMNode* node, PAGXDocument* doc);
static RoundCorner* parseRoundCorner(const DOMNode* node, PAGXDocument* doc);
static MergePath* parseMergePath(const DOMNode* node, PAGXDocument* doc);
static TextModifier* parseTextModifier(const DOMNode* node, PAGXDocument* doc);
static TextPath* parseTextPath(const DOMNode* node, PAGXDocument* doc);
static TextBox* parseTextBox(const DOMNode* node, PAGXDocument* doc);
static Repeater* parseRepeater(const DOMNode* node, PAGXDocument* doc);
static Group* parseGroup(const DOMNode* node, PAGXDocument* doc);
static RangeSelector* parseRangeSelector(const DOMNode* node, PAGXDocument* doc);
static SolidColor* parseSolidColor(const DOMNode* node, PAGXDocument* doc);
static LinearGradient* parseLinearGradient(const DOMNode* node, PAGXDocument* doc);
static RadialGradient* parseRadialGradient(const DOMNode* node, PAGXDocument* doc);
static ConicGradient* parseConicGradient(const DOMNode* node, PAGXDocument* doc);
static DiamondGradient* parseDiamondGradient(const DOMNode* node, PAGXDocument* doc);
static ImagePattern* parseImagePattern(const DOMNode* node, PAGXDocument* doc);
static ColorStop* parseColorStop(const DOMNode* node, PAGXDocument* doc);
static Image* parseImage(const DOMNode* node, PAGXDocument* doc);
static PathData* parsePathData(const DOMNode* node, PAGXDocument* doc);
static Composition* parseComposition(const DOMNode* node, PAGXDocument* doc);
static Font* parseFont(const DOMNode* node, PAGXDocument* doc);
static Glyph* parseGlyph(const DOMNode* node, PAGXDocument* doc);
static GlyphRun* parseGlyphRun(const DOMNode* node, PAGXDocument* doc);
static DropShadowStyle* parseDropShadowStyle(const DOMNode* node, PAGXDocument* doc);
static InnerShadowStyle* parseInnerShadowStyle(const DOMNode* node, PAGXDocument* doc);
static BackgroundBlurStyle* parseBackgroundBlurStyle(const DOMNode* node, PAGXDocument* doc);
static BlurFilter* parseBlurFilter(const DOMNode* node, PAGXDocument* doc);
static DropShadowFilter* parseDropShadowFilter(const DOMNode* node, PAGXDocument* doc);
static InnerShadowFilter* parseInnerShadowFilter(const DOMNode* node, PAGXDocument* doc);
static BlendFilter* parseBlendFilter(const DOMNode* node, PAGXDocument* doc);
static ColorMatrixFilter* parseColorMatrixFilter(const DOMNode* node, PAGXDocument* doc);

//==============================================================================
// Custom data parsing
//==============================================================================

static void parseCustomData(const DOMNode* xmlNode, Node* node) {
  for (const auto& attr : xmlNode->attributes) {
    if (attr.name.length() > 5 && attr.name.compare(0, 5, "data-") == 0) {
      auto key = attr.name.substr(5);
      if (Node::IsValidCustomDataKey(key)) {
        node->customData[std::move(key)] = attr.value;
      }
    }
  }
}

template <typename T>
static T* makeNodeFromXML(const DOMNode* xmlNode, PAGXDocument* doc) {
  auto id = getAttribute(xmlNode, "id");
  // If a node with this ID was pre-registered, reuse it instead of creating a duplicate.
  auto* existing = id.empty() ? nullptr : doc->findNode(id);
  if (existing) {
    parseCustomData(xmlNode, existing);
    return static_cast<T*>(existing);
  }
  auto* node = doc->makeNode<T>(id);
  if (node) {
    parseCustomData(xmlNode, node);
  }
  return node;
}

//==============================================================================
// Internal parser implementation
//==============================================================================

// Pre-registers a resource node by tag name. Creates an empty node of the correct type and
// registers its ID in nodeMap. Returns true if the tag name is a known resource type.
// NOTE: When adding a new resource type to PAGX, add a corresponding branch here and in
// parseResource() below.
static bool preRegisterResource(const DOMNode* node, PAGXDocument* doc) {
  auto id = getAttribute(node, "id");
  if (id.empty()) {
    return true;
  }
  if (node->name == "Image") {
    doc->makeNode<Image>(id);
  } else if (node->name == "PathData") {
    doc->makeNode<PathData>(id);
  } else if (node->name == "Font") {
    doc->makeNode<Font>(id);
  } else if (node->name == "Composition") {
    doc->makeNode<Composition>(id);
  } else if (node->name == "SolidColor") {
    doc->makeNode<SolidColor>(id);
  } else if (node->name == "LinearGradient") {
    doc->makeNode<LinearGradient>(id);
  } else if (node->name == "RadialGradient") {
    doc->makeNode<RadialGradient>(id);
  } else if (node->name == "ConicGradient") {
    doc->makeNode<ConicGradient>(id);
  } else if (node->name == "DiamondGradient") {
    doc->makeNode<DiamondGradient>(id);
  } else if (node->name == "ImagePattern") {
    doc->makeNode<ImagePattern>(id);
  } else {
    return false;
  }
  return true;
}

// Fully parses a resource node by tag name. Pre-registered nodes are reused by makeNodeFromXML.
// Returns true if the tag name is a known resource type.
// NOTE: When adding a new resource type to PAGX, add a corresponding branch here and in
// preRegisterResource() above.
static bool parseResource(const DOMNode* node, PAGXDocument* doc) {
  if (node->name == "Image") {
    parseImage(node, doc);
  } else if (node->name == "PathData") {
    parsePathData(node, doc);
  } else if (node->name == "Font") {
    parseFont(node, doc);
  } else if (node->name == "Composition") {
    parseComposition(node, doc);
  } else {
    return parseColorSource(node, doc) != nullptr;
  }
  return true;
}

static void parseResources(const DOMNode* node, PAGXDocument* doc) {
  // First pass: pre-register all resource IDs so that cross-references via '@id' resolve
  // regardless of XML declaration order.
  auto child = node->firstChild;
  while (child) {
    if (child->type == DOMNodeType::Element) {
      preRegisterResource(child.get(), doc);
    }
    child = child->nextSibling;
  }
  // Second pass: fully parse each resource. Pre-registered nodes are reused by makeNodeFromXML.
  child = node->firstChild;
  while (child) {
    auto current = child;
    child = child->nextSibling;
    if (current->type != DOMNodeType::Element) {
      continue;
    }
    if (!parseResource(current.get(), doc)) {
      reportError(doc, current.get(),
                  "Element '" + current->name +
                      "' is not allowed in 'Resources'."
                      " Expected: Image, PathData, Composition, Font,"
                      " SolidColor, LinearGradient, RadialGradient,"
                      " ConicGradient, DiamondGradient, ImagePattern.");
    }
  }
}

// Forward declarations for helper functions defined later in the file.
static std::optional<float> getOptionalFloatAttribute(const DOMNode* node, const std::string& name,
                                                      PAGXDocument* doc);

static float getFloatAttributeOrNaN(const DOMNode* node, const std::string& name,
                                    PAGXDocument* doc) {
  auto value = getOptionalFloatAttribute(node, name, doc);
  return value.has_value() ? *value : NAN;
}

static Layer* parseLayer(const DOMNode* node, PAGXDocument* doc) {
  auto layer = makeNodeFromXML<Layer>(node, doc);
  if (!layer) {
    return nullptr;
  }
  layer->name = getAttribute(node, "name");
  layer->visible = getBoolAttribute(node, "visible", Default<Layer>().visible, doc);
  layer->alpha = getFloatAttribute(node, "alpha", Default<Layer>().alpha, doc);
  layer->blendMode = GET_ENUM(node, "blendMode", "normal", doc, BlendMode);
  layer->x = getFloatAttribute(node, "x", Default<Layer>().x, doc);
  layer->y = getFloatAttribute(node, "y", Default<Layer>().y, doc);
  layer->width = getFloatAttributeOrNaN(node, "width", doc);
  layer->height = getFloatAttributeOrNaN(node, "height", doc);
  layer->layout = GET_ENUM(node, "layout", "none", doc, LayoutMode);
  layer->gap = getFloatAttribute(node, "gap", Default<Layer>().gap, doc);
  layer->flex = getFloatAttribute(node, "flex", Default<Layer>().flex, doc);
  auto paddingStr = getAttribute(node, "padding");
  if (!paddingStr.empty()) {
    auto paddingValues = ParseFloatList(paddingStr);
    auto count = paddingValues.size();
    if (count != 1 && count != 2 && count != 4) {
      reportError(doc, node, "Invalid value '" + paddingStr + "' for 'padding' attribute.");
    } else if (count == 1) {
      layer->padding = {paddingValues[0], paddingValues[0], paddingValues[0], paddingValues[0]};
    } else if (count == 2) {
      layer->padding = {paddingValues[0], paddingValues[1], paddingValues[0], paddingValues[1]};
    } else {
      layer->padding = {paddingValues[0], paddingValues[1], paddingValues[2], paddingValues[3]};
    }
  }
  auto alignmentStr = getAttribute(node, "alignment");
  if (!alignmentStr.empty()) {
    if (!IsValidAlignmentString(alignmentStr)) {
      reportError(doc, node, "Invalid value '" + alignmentStr + "' for 'alignment' attribute.");
    }
    layer->alignment = AlignmentFromString(alignmentStr);
  }
  auto arrangementStr = getAttribute(node, "arrangement");
  if (!arrangementStr.empty()) {
    if (!IsValidArrangementString(arrangementStr)) {
      reportError(doc, node, "Invalid value '" + arrangementStr + "' for 'arrangement' attribute.");
    }
    layer->arrangement = ArrangementFromString(arrangementStr);
  }
  layer->includeInLayout =
      getBoolAttribute(node, "includeInLayout", Default<Layer>().includeInLayout, doc);
  layer->left = getFloatAttributeOrNaN(node, "left", doc);
  layer->right = getFloatAttributeOrNaN(node, "right", doc);
  layer->top = getFloatAttributeOrNaN(node, "top", doc);
  layer->bottom = getFloatAttributeOrNaN(node, "bottom", doc);
  layer->centerX = getFloatAttributeOrNaN(node, "centerX", doc);
  layer->centerY = getFloatAttributeOrNaN(node, "centerY", doc);
  auto matrixStr = getAttribute(node, "matrix");
  if (!matrixStr.empty()) {
    if (ParseFloatList(matrixStr).size() < 6) {
      reportError(doc, node, "Invalid value '" + matrixStr + "' for 'matrix' attribute.");
    } else {
      layer->matrix = MatrixFromString(matrixStr);
    }
  }
  auto matrix3DStr = getAttribute(node, "matrix3D");
  if (!matrix3DStr.empty()) {
    auto floats = ParseFloatList(matrix3DStr);
    if (floats.size() == 16) {
      memcpy(layer->matrix3D.values, floats.data(), 16 * sizeof(float));
    } else {
      reportError(doc, node, "Invalid 'matrix3D' value (expected 16 numbers).");
    }
  }
  layer->preserve3D = getBoolAttribute(node, "preserve3D", Default<Layer>().preserve3D, doc);
  layer->antiAlias = getBoolAttribute(node, "antiAlias", Default<Layer>().antiAlias, doc);
  layer->groupOpacity = getBoolAttribute(node, "groupOpacity", Default<Layer>().groupOpacity, doc);
  layer->passThroughBackground =
      getBoolAttribute(node, "passThroughBackground", Default<Layer>().passThroughBackground, doc);
  auto scrollRectStr = getAttribute(node, "scrollRect");
  if (!scrollRectStr.empty()) {
    layer->scrollRect = getRectAttribute(node, "scrollRect", {}, doc);
    layer->hasScrollRect = true;
  }
  layer->clipToBounds = getBoolAttribute(node, "clipToBounds", Default<Layer>().clipToBounds, doc);

  auto maskAttr = getAttribute(node, "mask");
  if (!maskAttr.empty() && maskAttr[0] == '@') {
    layer->mask = doc->findNode<Layer>(maskAttr.substr(1));
    if (!layer->mask) {
      reportError(doc, node, "Resource '" + maskAttr + "' not found for 'mask' attribute.");
    }
  }
  layer->maskType = GET_ENUM(node, "maskType", "alpha", doc, MaskType);

  auto compositionAttr = getAttribute(node, "composition");
  if (!compositionAttr.empty() && compositionAttr[0] == '@') {
    layer->composition = doc->findNode<Composition>(compositionAttr.substr(1));
    if (!layer->composition) {
      reportError(doc, node,
                  "Resource '" + compositionAttr + "' not found for 'composition' attribute.");
    }
  }

  auto child = node->firstChild;
  while (child) {
    auto current = child;
    child = child->nextSibling;
    if (current->type != DOMNodeType::Element) {
      continue;
    }
    // Legacy format: support container nodes for backward compatibility.
    if (current->name == "contents") {
      parseContents(current.get(), layer, doc);
      continue;
    }
    if (current->name == "styles") {
      parseStyles(current.get(), layer, doc);
      continue;
    }
    if (current->name == "filters") {
      parseFilters(current.get(), layer, doc);
      continue;
    }
    // New format: direct child elements without container nodes.
    if (current->name == "Layer") {
      auto childLayer = parseLayer(current.get(), doc);
      if (childLayer) {
        layer->children.push_back(childLayer);
      }
      continue;
    }
    // Try to parse as VectorElement.
    auto element = parseElement(current.get(), doc);
    if (element) {
      layer->contents.push_back(element);
      continue;
    }
    // Try to parse as LayerStyle.
    auto style = parseLayerStyle(current.get(), doc);
    if (style) {
      layer->styles.push_back(style);
      continue;
    }
    // Try to parse as LayerFilter.
    auto filter = parseLayerFilter(current.get(), doc);
    if (filter) {
      layer->filters.push_back(filter);
      continue;
    }
    // Unknown node type - report error.
    reportError(doc, current.get(),
                "Element '" + current->name +
                    "' is not allowed in 'Layer'."
                    " Expected: Layer, Group, Rectangle, Ellipse, Polystar,"
                    " Path, Text, Fill, Stroke, TrimPath, RoundCorner,"
                    " MergePath, TextModifier, TextPath, TextBox, Repeater,"
                    " DropShadowStyle, InnerShadowStyle, BackgroundBlurStyle,"
                    " BlurFilter, DropShadowFilter, InnerShadowFilter,"
                    " BlendFilter, ColorMatrixFilter.");
  }

  return layer;
}

static void parseContents(const DOMNode* node, Layer* layer, PAGXDocument* doc) {
  auto child = node->firstChild;
  while (child) {
    auto current = child;
    child = child->nextSibling;
    if (current->type != DOMNodeType::Element) {
      continue;
    }
    auto element = parseElement(current.get(), doc);
    if (element) {
      layer->contents.push_back(element);
    } else {
      reportError(doc, current.get(),
                  "Element '" + current->name +
                      "' is not allowed in 'contents'."
                      " Expected: Group, Rectangle, Ellipse, Polystar,"
                      " Path, Text, Fill, Stroke, TrimPath, RoundCorner,"
                      " MergePath, TextModifier, TextPath, TextBox, Repeater.");
    }
  }
}

static void parseStyles(const DOMNode* node, Layer* layer, PAGXDocument* doc) {
  auto child = node->firstChild;
  while (child) {
    auto current = child;
    child = child->nextSibling;
    if (current->type != DOMNodeType::Element) {
      continue;
    }
    auto style = parseLayerStyle(current.get(), doc);
    if (style) {
      layer->styles.push_back(style);
    } else {
      reportError(doc, current.get(),
                  "Element '" + current->name +
                      "' is not allowed in 'styles'."
                      " Expected: DropShadowStyle, InnerShadowStyle,"
                      " BackgroundBlurStyle.");
    }
  }
}

static void parseFilters(const DOMNode* node, Layer* layer, PAGXDocument* doc) {
  auto child = node->firstChild;
  while (child) {
    auto current = child;
    child = child->nextSibling;
    if (current->type != DOMNodeType::Element) {
      continue;
    }
    auto filter = parseLayerFilter(current.get(), doc);
    if (filter) {
      layer->filters.push_back(filter);
    } else {
      reportError(doc, current.get(),
                  "Element '" + current->name +
                      "' is not allowed in 'filters'."
                      " Expected: BlurFilter, DropShadowFilter,"
                      " InnerShadowFilter, BlendFilter, ColorMatrixFilter.");
    }
  }
}

static Element* parseElement(const DOMNode* node, PAGXDocument* doc) {
  if (node->name == "Rectangle") {
    return parseRectangle(node, doc);
  }
  if (node->name == "Ellipse") {
    return parseEllipse(node, doc);
  }
  if (node->name == "Polystar") {
    return parsePolystar(node, doc);
  }
  if (node->name == "Path") {
    return parsePath(node, doc);
  }
  if (node->name == "Text") {
    return parseText(node, doc);
  }
  if (node->name == "Fill") {
    return parseFill(node, doc);
  }
  if (node->name == "Stroke") {
    return parseStroke(node, doc);
  }
  if (node->name == "TrimPath") {
    return parseTrimPath(node, doc);
  }
  if (node->name == "RoundCorner") {
    return parseRoundCorner(node, doc);
  }
  if (node->name == "MergePath") {
    return parseMergePath(node, doc);
  }
  if (node->name == "TextModifier") {
    return parseTextModifier(node, doc);
  }
  if (node->name == "TextPath") {
    return parseTextPath(node, doc);
  }
  if (node->name == "TextBox") {
    return parseTextBox(node, doc);
  }
  if (node->name == "Repeater") {
    return parseRepeater(node, doc);
  }
  if (node->name == "Group") {
    return parseGroup(node, doc);
  }
  return nullptr;
}

static ColorSource* parseColorSource(const DOMNode* node, PAGXDocument* doc) {
  if (node->name == "SolidColor") {
    return parseSolidColor(node, doc);
  }
  if (node->name == "LinearGradient") {
    return parseLinearGradient(node, doc);
  }
  if (node->name == "RadialGradient") {
    return parseRadialGradient(node, doc);
  }
  if (node->name == "ConicGradient") {
    return parseConicGradient(node, doc);
  }
  if (node->name == "DiamondGradient") {
    return parseDiamondGradient(node, doc);
  }
  if (node->name == "ImagePattern") {
    return parseImagePattern(node, doc);
  }
  return nullptr;
}

static LayerStyle* parseLayerStyle(const DOMNode* node, PAGXDocument* doc) {
  if (node->name == "DropShadowStyle") {
    return parseDropShadowStyle(node, doc);
  }
  if (node->name == "InnerShadowStyle") {
    return parseInnerShadowStyle(node, doc);
  }
  if (node->name == "BackgroundBlurStyle") {
    return parseBackgroundBlurStyle(node, doc);
  }
  return nullptr;
}

static LayerFilter* parseLayerFilter(const DOMNode* node, PAGXDocument* doc) {
  if (node->name == "BlurFilter") {
    return parseBlurFilter(node, doc);
  }
  if (node->name == "DropShadowFilter") {
    return parseDropShadowFilter(node, doc);
  }
  if (node->name == "InnerShadowFilter") {
    return parseInnerShadowFilter(node, doc);
  }
  if (node->name == "BlendFilter") {
    return parseBlendFilter(node, doc);
  }
  if (node->name == "ColorMatrixFilter") {
    return parseColorMatrixFilter(node, doc);
  }
  return nullptr;
}

//==============================================================================
// Geometry element parsing
//==============================================================================

static Rectangle* parseRectangle(const DOMNode* node, PAGXDocument* doc) {
  auto rect = makeNodeFromXML<Rectangle>(node, doc);
  if (!rect) {
    return nullptr;
  }
  rect->size = getSizeAttribute(node, "size", {0, 0}, doc);
  auto* posAttr = node->findAttribute("position");
  if (posAttr && !posAttr->empty()) {
    rect->position = getPointAttribute(node, "position", Default<Rectangle>().position, doc);
  } else {
    rect->position = {rect->size.width * 0.5f, rect->size.height * 0.5f};
  }
  rect->roundness = getFloatAttribute(node, "roundness", Default<Rectangle>().roundness, doc);
  rect->reversed = getBoolAttribute(node, "reversed", Default<Rectangle>().reversed, doc);
  rect->left = getFloatAttributeOrNaN(node, "left", doc);
  rect->right = getFloatAttributeOrNaN(node, "right", doc);
  rect->top = getFloatAttributeOrNaN(node, "top", doc);
  rect->bottom = getFloatAttributeOrNaN(node, "bottom", doc);
  rect->centerX = getFloatAttributeOrNaN(node, "centerX", doc);
  rect->centerY = getFloatAttributeOrNaN(node, "centerY", doc);
  return rect;
}

static Ellipse* parseEllipse(const DOMNode* node, PAGXDocument* doc) {
  auto ellipse = makeNodeFromXML<Ellipse>(node, doc);
  if (!ellipse) {
    return nullptr;
  }
  ellipse->size = getSizeAttribute(node, "size", {0, 0}, doc);
  auto* posAttr = node->findAttribute("position");
  if (posAttr && !posAttr->empty()) {
    ellipse->position = getPointAttribute(node, "position", Default<Ellipse>().position, doc);
  } else {
    ellipse->position = {ellipse->size.width * 0.5f, ellipse->size.height * 0.5f};
  }
  ellipse->reversed = getBoolAttribute(node, "reversed", Default<Ellipse>().reversed, doc);
  ellipse->left = getFloatAttributeOrNaN(node, "left", doc);
  ellipse->right = getFloatAttributeOrNaN(node, "right", doc);
  ellipse->top = getFloatAttributeOrNaN(node, "top", doc);
  ellipse->bottom = getFloatAttributeOrNaN(node, "bottom", doc);
  ellipse->centerX = getFloatAttributeOrNaN(node, "centerX", doc);
  ellipse->centerY = getFloatAttributeOrNaN(node, "centerY", doc);
  return ellipse;
}

static Polystar* parsePolystar(const DOMNode* node, PAGXDocument* doc) {
  auto polystar = makeNodeFromXML<Polystar>(node, doc);
  if (!polystar) {
    return nullptr;
  }
  polystar->type = GET_ENUM(node, "type", "star", doc, PolystarType);
  polystar->pointCount = getFloatAttribute(node, "pointCount", Default<Polystar>().pointCount, doc);
  polystar->outerRadius =
      getFloatAttribute(node, "outerRadius", Default<Polystar>().outerRadius, doc);
  polystar->innerRadius =
      getFloatAttribute(node, "innerRadius", Default<Polystar>().innerRadius, doc);
  polystar->rotation = getFloatAttribute(node, "rotation", Default<Polystar>().rotation, doc);
  polystar->outerRoundness =
      getFloatAttribute(node, "outerRoundness", Default<Polystar>().outerRoundness, doc);
  polystar->innerRoundness =
      getFloatAttribute(node, "innerRoundness", Default<Polystar>().innerRoundness, doc);
  polystar->reversed = getBoolAttribute(node, "reversed", Default<Polystar>().reversed, doc);
  auto* posAttr = node->findAttribute("position");
  if (posAttr && !posAttr->empty()) {
    polystar->position = getPointAttribute(node, "position", Default<Polystar>().position, doc);
  } else {
    auto bounds = polystar->getContentBounds();
    polystar->position = {-bounds.x, -bounds.y};
  }
  polystar->left = getFloatAttributeOrNaN(node, "left", doc);
  polystar->right = getFloatAttributeOrNaN(node, "right", doc);
  polystar->top = getFloatAttributeOrNaN(node, "top", doc);
  polystar->bottom = getFloatAttributeOrNaN(node, "bottom", doc);
  polystar->centerX = getFloatAttributeOrNaN(node, "centerX", doc);
  polystar->centerY = getFloatAttributeOrNaN(node, "centerY", doc);
  return polystar;
}

static Path* parsePath(const DOMNode* node, PAGXDocument* doc) {
  auto path = makeNodeFromXML<Path>(node, doc);
  if (!path) {
    return nullptr;
  }
  auto dataAttr = getAttribute(node, "data");
  if (!dataAttr.empty()) {
    if (dataAttr[0] == '@') {
      // Reference to PathData resource
      path->data = doc->findNode<PathData>(dataAttr.substr(1));
      if (!path->data) {
        reportError(doc, node, "Resource '" + dataAttr + "' not found for 'data' attribute.");
      }
    } else {
      // Inline path data
      path->data = doc->makeNode<PathData>();
      *path->data = PathDataFromSVGString(dataAttr);
    }
  }
  path->reversed = getBoolAttribute(node, "reversed", Default<Path>().reversed, doc);
  path->position = getPointAttribute(node, "position", Default<Path>().position, doc);
  path->left = getFloatAttributeOrNaN(node, "left", doc);
  path->right = getFloatAttributeOrNaN(node, "right", doc);
  path->top = getFloatAttributeOrNaN(node, "top", doc);
  path->bottom = getFloatAttributeOrNaN(node, "bottom", doc);
  path->centerX = getFloatAttributeOrNaN(node, "centerX", doc);
  path->centerY = getFloatAttributeOrNaN(node, "centerY", doc);
  return path;
}

static Text* parseText(const DOMNode* node, PAGXDocument* doc) {
  auto text = makeNodeFromXML<Text>(node, doc);
  if (!text) {
    return nullptr;
  }
  // Parse text content from attribute first, then fallback to text child node
  auto textAttr = getAttribute(node, "text");
  if (!textAttr.empty()) {
    text->text = textAttr;
  } else {
    auto textChild = node->firstChild;
    while (textChild) {
      if (textChild->type == DOMNodeType::Text) {
        text->text = textChild->name;
        break;
      }
      textChild = textChild->nextSibling;
    }
  }
  text->position = getPointAttribute(node, "position", Default<Text>().position, doc);
  text->fontFamily = getAttribute(node, "fontFamily");
  text->fontStyle = getAttribute(node, "fontStyle");
  text->fontSize = getFloatAttribute(node, "fontSize", Default<Text>().fontSize, doc);
  text->letterSpacing =
      getFloatAttribute(node, "letterSpacing", Default<Text>().letterSpacing, doc);
  text->fauxBold = getBoolAttribute(node, "fauxBold", Default<Text>().fauxBold, doc);
  text->fauxItalic = getBoolAttribute(node, "fauxItalic", Default<Text>().fauxItalic, doc);
  text->textAnchor = GET_ENUM(node, "textAnchor", "start", doc, TextAnchor);
  text->baseline = GET_ENUM(node, "baseline", "lineBox", doc, TextBaseline);

  // Parse GlyphRun children for precomposition mode
  auto child = node->firstChild;
  while (child) {
    if (child->type == DOMNodeType::Element) {
      if (child->name == "GlyphRun") {
        auto glyphRun = parseGlyphRun(child.get(), doc);
        if (glyphRun) {
          text->glyphRuns.push_back(glyphRun);
        }
      } else {
        reportError(doc, child.get(),
                    "Element '" + child->name + "' is not allowed in 'Text'. Expected: GlyphRun.");
      }
    }
    child = child->nextSibling;
  }

  text->left = getFloatAttributeOrNaN(node, "left", doc);
  text->right = getFloatAttributeOrNaN(node, "right", doc);
  text->top = getFloatAttributeOrNaN(node, "top", doc);
  text->bottom = getFloatAttributeOrNaN(node, "bottom", doc);
  text->centerX = getFloatAttributeOrNaN(node, "centerX", doc);
  text->centerY = getFloatAttributeOrNaN(node, "centerY", doc);
  return text;
}

//==============================================================================
// Painter parsing
//==============================================================================

static ColorSource* parseColorAttr(const std::string& colorAttr, PAGXDocument* doc,
                                   const DOMNode* node) {
  if (colorAttr.empty()) {
    return nullptr;
  }
  if (colorAttr[0] == '@') {
    auto result = doc->findNode<ColorSource>(colorAttr.substr(1));
    if (!result) {
      reportError(doc, node, "Resource '" + colorAttr + "' not found for 'color' attribute.");
    }
    return result;
  }
  auto solidColor = doc->makeNode<SolidColor>();
  solidColor->color = getColorAttribute(node, "color", doc);
  return solidColor;
}

static ColorSource* parseChildColorSource(const DOMNode* node, PAGXDocument* doc) {
  ColorSource* result = nullptr;
  auto child = node->firstChild;
  while (child) {
    if (child->type == DOMNodeType::Element) {
      auto colorSource = parseColorSource(child.get(), doc);
      if (colorSource) {
        result = colorSource;
      } else {
        reportError(doc, child.get(),
                    "Element '" + child->name + "' is not allowed in '" + node->name +
                        "'. Expected: SolidColor, LinearGradient,"
                        " RadialGradient, ConicGradient,"
                        " DiamondGradient, ImagePattern.");
      }
    }
    child = child->nextSibling;
  }
  return result;
}

static Fill* parseFill(const DOMNode* node, PAGXDocument* doc) {
  auto fill = makeNodeFromXML<Fill>(node, doc);
  if (!fill) {
    return nullptr;
  }
  fill->color = parseColorAttr(getAttribute(node, "color"), doc, node);
  fill->alpha = getFloatAttribute(node, "alpha", Default<Fill>().alpha, doc);
  fill->blendMode = GET_ENUM(node, "blendMode", "normal", doc, BlendMode);
  fill->fillRule = GET_ENUM(node, "fillRule", "winding", doc, FillRule);
  fill->placement = GET_ENUM(node, "placement", "background", doc, LayerPlacement);
  auto childColor = parseChildColorSource(node, doc);
  if (childColor) {
    fill->color = childColor;
  }
  return fill;
}

static Stroke* parseStroke(const DOMNode* node, PAGXDocument* doc) {
  auto stroke = makeNodeFromXML<Stroke>(node, doc);
  if (!stroke) {
    return nullptr;
  }
  stroke->color = parseColorAttr(getAttribute(node, "color"), doc, node);
  stroke->width = getFloatAttribute(node, "width", Default<Stroke>().width, doc);
  stroke->alpha = getFloatAttribute(node, "alpha", Default<Stroke>().alpha, doc);
  stroke->blendMode = GET_ENUM(node, "blendMode", "normal", doc, BlendMode);
  stroke->cap = GET_ENUM(node, "cap", "butt", doc, LineCap);
  stroke->join = GET_ENUM(node, "join", "miter", doc, LineJoin);
  stroke->miterLimit = getFloatAttribute(node, "miterLimit", Default<Stroke>().miterLimit, doc);
  auto dashesStr = getAttribute(node, "dashes");
  if (!dashesStr.empty()) {
    stroke->dashes = ParseFloatList(dashesStr);
  }
  stroke->dashOffset = getFloatAttribute(node, "dashOffset", Default<Stroke>().dashOffset, doc);
  stroke->dashAdaptive =
      getBoolAttribute(node, "dashAdaptive", Default<Stroke>().dashAdaptive, doc);
  stroke->align = GET_ENUM(node, "align", "center", doc, StrokeAlign);
  stroke->placement = GET_ENUM(node, "placement", "background", doc, LayerPlacement);
  auto childColor = parseChildColorSource(node, doc);
  if (childColor) {
    stroke->color = childColor;
  }
  return stroke;
}

//==============================================================================
// Modifier parsing
//==============================================================================

static TrimPath* parseTrimPath(const DOMNode* node, PAGXDocument* doc) {
  auto trim = makeNodeFromXML<TrimPath>(node, doc);
  if (!trim) {
    return nullptr;
  }
  trim->start = getFloatAttribute(node, "start", Default<TrimPath>().start, doc);
  trim->end = getFloatAttribute(node, "end", Default<TrimPath>().end, doc);
  trim->offset = getFloatAttribute(node, "offset", Default<TrimPath>().offset, doc);
  trim->type = GET_ENUM(node, "type", "separate", doc, TrimType);
  return trim;
}

static RoundCorner* parseRoundCorner(const DOMNode* node, PAGXDocument* doc) {
  auto round = makeNodeFromXML<RoundCorner>(node, doc);
  if (!round) {
    return nullptr;
  }
  round->radius = getFloatAttribute(node, "radius", Default<RoundCorner>().radius, doc);
  return round;
}

static MergePath* parseMergePath(const DOMNode* node, PAGXDocument* doc) {
  auto merge = makeNodeFromXML<MergePath>(node, doc);
  if (!merge) {
    return nullptr;
  }
  merge->mode = GET_ENUM(node, "mode", "append", doc, MergePathMode);
  return merge;
}

static TextModifier* parseTextModifier(const DOMNode* node, PAGXDocument* doc) {
  auto modifier = makeNodeFromXML<TextModifier>(node, doc);
  if (!modifier) {
    return nullptr;
  }
  modifier->anchor = getPointAttribute(node, "anchor", Default<TextModifier>().anchor, doc);
  modifier->position = getPointAttribute(node, "position", Default<TextModifier>().position, doc);
  modifier->rotation = getFloatAttribute(node, "rotation", Default<TextModifier>().rotation, doc);
  modifier->scale = getPointAttribute(node, "scale", Default<TextModifier>().scale, doc);
  modifier->skew = getFloatAttribute(node, "skew", Default<TextModifier>().skew, doc);
  modifier->skewAxis = getFloatAttribute(node, "skewAxis", Default<TextModifier>().skewAxis, doc);
  modifier->alpha = getFloatAttribute(node, "alpha", Default<TextModifier>().alpha, doc);
  auto fillColorAttr = getAttribute(node, "fillColor");
  if (!fillColorAttr.empty()) {
    modifier->fillColor = getColorAttribute(node, "fillColor", doc);
  }
  auto strokeColorAttr = getAttribute(node, "strokeColor");
  if (!strokeColorAttr.empty()) {
    modifier->strokeColor = getColorAttribute(node, "strokeColor", doc);
  }
  auto strokeWidthAttr = getAttribute(node, "strokeWidth");
  if (!strokeWidthAttr.empty()) {
    modifier->strokeWidth = getFloatAttribute(node, "strokeWidth", 0, doc);
  }

  auto child = node->firstChild;
  while (child) {
    if (child->type == DOMNodeType::Element) {
      if (child->name == "RangeSelector") {
        auto selector = parseRangeSelector(child.get(), doc);
        if (selector) {
          modifier->selectors.push_back(selector);
        }
      } else {
        reportError(doc, child.get(),
                    "Element '" + child->name +
                        "' is not allowed in 'TextModifier'."
                        " Expected: RangeSelector.");
      }
    }
    child = child->nextSibling;
  }

  return modifier;
}

static TextPath* parseTextPath(const DOMNode* node, PAGXDocument* doc) {
  auto textPath = makeNodeFromXML<TextPath>(node, doc);
  if (!textPath) {
    return nullptr;
  }
  auto pathAttr = getAttribute(node, "path");
  if (!pathAttr.empty()) {
    if (pathAttr[0] == '@') {
      // Reference to PathData resource
      textPath->path = doc->findNode<PathData>(pathAttr.substr(1));
      if (!textPath->path) {
        reportError(doc, node, "Resource '" + pathAttr + "' not found for 'path' attribute.");
      }
    } else {
      // Inline path data
      textPath->path = doc->makeNode<PathData>();
      *textPath->path = PathDataFromSVGString(pathAttr);
    }
  }
  textPath->baselineOrigin =
      getPointAttribute(node, "baselineOrigin", Default<TextPath>().baselineOrigin, doc);
  textPath->baselineAngle =
      getFloatAttribute(node, "baselineAngle", Default<TextPath>().baselineAngle, doc);
  textPath->firstMargin =
      getFloatAttribute(node, "firstMargin", Default<TextPath>().firstMargin, doc);
  textPath->lastMargin = getFloatAttribute(node, "lastMargin", Default<TextPath>().lastMargin, doc);
  textPath->perpendicular =
      getBoolAttribute(node, "perpendicular", Default<TextPath>().perpendicular, doc);
  textPath->reversed = getBoolAttribute(node, "reversed", Default<TextPath>().reversed, doc);
  textPath->forceAlignment =
      getBoolAttribute(node, "forceAlignment", Default<TextPath>().forceAlignment, doc);
  textPath->left = getFloatAttributeOrNaN(node, "left", doc);
  textPath->right = getFloatAttributeOrNaN(node, "right", doc);
  textPath->top = getFloatAttributeOrNaN(node, "top", doc);
  textPath->bottom = getFloatAttributeOrNaN(node, "bottom", doc);
  textPath->centerX = getFloatAttributeOrNaN(node, "centerX", doc);
  textPath->centerY = getFloatAttributeOrNaN(node, "centerY", doc);
  return textPath;
}

static TextBox* parseTextBox(const DOMNode* node, PAGXDocument* doc) {
  auto textBox = makeNodeFromXML<TextBox>(node, doc);
  if (!textBox) {
    return nullptr;
  }
  // Group properties
  textBox->anchor = getPointAttribute(node, "anchor", Default<TextBox>().anchor, doc);
  textBox->position = getPointAttribute(node, "position", Default<TextBox>().position, doc);
  textBox->rotation = getFloatAttribute(node, "rotation", Default<TextBox>().rotation, doc);
  textBox->scale = getPointAttribute(node, "scale", Default<TextBox>().scale, doc);
  textBox->skew = getFloatAttribute(node, "skew", Default<TextBox>().skew, doc);
  textBox->skewAxis = getFloatAttribute(node, "skewAxis", Default<TextBox>().skewAxis, doc);
  textBox->alpha = getFloatAttribute(node, "alpha", Default<TextBox>().alpha, doc);
  textBox->width = getFloatAttributeOrNaN(node, "width", doc);
  textBox->height = getFloatAttributeOrNaN(node, "height", doc);
  // TextBox typography properties
  textBox->textAlign = GET_ENUM(node, "textAlign", "start", doc, TextAlign);
  textBox->paragraphAlign = GET_ENUM(node, "paragraphAlign", "near", doc, ParagraphAlign);
  textBox->writingMode = GET_ENUM(node, "writingMode", "horizontal", doc, WritingMode);
  textBox->lineHeight = getFloatAttribute(node, "lineHeight", Default<TextBox>().lineHeight, doc);
  textBox->wordWrap = getBoolAttribute(node, "wordWrap", Default<TextBox>().wordWrap, doc);
  textBox->overflow = GET_ENUM(node, "overflow", "visible", doc, Overflow);
  // Constraint properties
  textBox->left = getFloatAttributeOrNaN(node, "left", doc);
  textBox->right = getFloatAttributeOrNaN(node, "right", doc);
  textBox->top = getFloatAttributeOrNaN(node, "top", doc);
  textBox->bottom = getFloatAttributeOrNaN(node, "bottom", doc);
  textBox->centerX = getFloatAttributeOrNaN(node, "centerX", doc);
  textBox->centerY = getFloatAttributeOrNaN(node, "centerY", doc);
  // Child elements
  auto child = node->firstChild;
  while (child) {
    if (child->type == DOMNodeType::Element) {
      auto element = parseElement(child.get(), doc);
      if (element) {
        textBox->elements.push_back(element);
      } else {
        reportError(doc, child.get(),
                    "Element '" + child->name +
                        "' is not allowed in 'TextBox'."
                        " Expected: Rectangle, Ellipse, Polystar, Path,"
                        " Text, Fill, Stroke, TrimPath, RoundCorner,"
                        " MergePath, TextModifier, TextPath,"
                        " Repeater, Group.");
      }
    }
    child = child->nextSibling;
  }
  return textBox;
}

static Repeater* parseRepeater(const DOMNode* node, PAGXDocument* doc) {
  auto repeater = makeNodeFromXML<Repeater>(node, doc);
  if (!repeater) {
    return nullptr;
  }
  repeater->copies = getFloatAttribute(node, "copies", Default<Repeater>().copies, doc);
  repeater->offset = getFloatAttribute(node, "offset", Default<Repeater>().offset, doc);
  repeater->order = GET_ENUM(node, "order", "belowOriginal", doc, RepeaterOrder);
  repeater->anchor = getPointAttribute(node, "anchor", Default<Repeater>().anchor, doc);
  repeater->position = getPointAttribute(node, "position", Default<Repeater>().position, doc);
  repeater->rotation = getFloatAttribute(node, "rotation", Default<Repeater>().rotation, doc);
  repeater->scale = getPointAttribute(node, "scale", Default<Repeater>().scale, doc);
  repeater->startAlpha = getFloatAttribute(node, "startAlpha", Default<Repeater>().startAlpha, doc);
  repeater->endAlpha = getFloatAttribute(node, "endAlpha", Default<Repeater>().endAlpha, doc);
  return repeater;
}

static Group* parseGroup(const DOMNode* node, PAGXDocument* doc) {
  auto group = makeNodeFromXML<Group>(node, doc);
  if (!group) {
    return nullptr;
  }
  group->anchor = getPointAttribute(node, "anchor", Default<Group>().anchor, doc);
  group->position = getPointAttribute(node, "position", Default<Group>().position, doc);
  group->rotation = getFloatAttribute(node, "rotation", Default<Group>().rotation, doc);
  group->scale = getPointAttribute(node, "scale", Default<Group>().scale, doc);
  group->skew = getFloatAttribute(node, "skew", Default<Group>().skew, doc);
  group->skewAxis = getFloatAttribute(node, "skewAxis", Default<Group>().skewAxis, doc);
  group->alpha = getFloatAttribute(node, "alpha", Default<Group>().alpha, doc);
  group->width = getFloatAttributeOrNaN(node, "width", doc);
  group->height = getFloatAttributeOrNaN(node, "height", doc);
  group->left = getFloatAttributeOrNaN(node, "left", doc);
  group->right = getFloatAttributeOrNaN(node, "right", doc);
  group->top = getFloatAttributeOrNaN(node, "top", doc);
  group->bottom = getFloatAttributeOrNaN(node, "bottom", doc);
  group->centerX = getFloatAttributeOrNaN(node, "centerX", doc);
  group->centerY = getFloatAttributeOrNaN(node, "centerY", doc);

  auto child = node->firstChild;
  while (child) {
    if (child->type == DOMNodeType::Element) {
      auto element = parseElement(child.get(), doc);
      if (element) {
        group->elements.push_back(element);
      } else {
        reportError(doc, child.get(),
                    "Element '" + child->name +
                        "' is not allowed in 'Group'."
                        " Expected: Rectangle, Ellipse, Polystar, Path,"
                        " Text, Fill, Stroke, TrimPath, RoundCorner,"
                        " MergePath, TextModifier, TextPath, TextBox,"
                        " Repeater, Group.");
      }
    }
    child = child->nextSibling;
  }

  return group;
}

static RangeSelector* parseRangeSelector(const DOMNode* node, PAGXDocument* doc) {
  auto selector = makeNodeFromXML<RangeSelector>(node, doc);
  if (!selector) {
    return nullptr;
  }
  selector->start = getFloatAttribute(node, "start", Default<RangeSelector>().start, doc);
  selector->end = getFloatAttribute(node, "end", Default<RangeSelector>().end, doc);
  selector->offset = getFloatAttribute(node, "offset", Default<RangeSelector>().offset, doc);
  selector->unit = GET_ENUM(node, "unit", "percentage", doc, SelectorUnit);
  selector->shape = GET_ENUM(node, "shape", "square", doc, SelectorShape);
  selector->easeIn = getFloatAttribute(node, "easeIn", Default<RangeSelector>().easeIn, doc);
  selector->easeOut = getFloatAttribute(node, "easeOut", Default<RangeSelector>().easeOut, doc);
  selector->mode = GET_ENUM(node, "mode", "add", doc, SelectorMode);
  selector->weight = getFloatAttribute(node, "weight", Default<RangeSelector>().weight, doc);
  selector->randomOrder =
      getBoolAttribute(node, "randomOrder", Default<RangeSelector>().randomOrder, doc);
  selector->randomSeed =
      getIntAttribute(node, "randomSeed", Default<RangeSelector>().randomSeed, doc);
  return selector;
}

//==============================================================================
// Color source parsing
//==============================================================================

static SolidColor* parseSolidColor(const DOMNode* node, PAGXDocument* doc) {
  auto solid = makeNodeFromXML<SolidColor>(node, doc);
  if (!solid) {
    return nullptr;
  }
  auto colorStr = getAttribute(node, "color");
  if (!colorStr.empty()) {
    solid->color = getColorAttribute(node, "color", doc);
  } else {
    // Fallback to individual red/green/blue/alpha attributes
    solid->color.red = getFloatAttribute(node, "red", 0, doc);
    solid->color.green = getFloatAttribute(node, "green", 0, doc);
    solid->color.blue = getFloatAttribute(node, "blue", 0, doc);
    solid->color.alpha = getFloatAttribute(node, "alpha", 1, doc);
    solid->color.colorSpace = GET_ENUM(node, "colorSpace", "sRGB", doc, ColorSpace);
  }
  return solid;
}

static void parseGradientCommon(const DOMNode* node, PAGXDocument* doc, Matrix& matrix,
                                std::vector<ColorStop*>& colorStops) {
  auto matrixStr = getAttribute(node, "matrix");
  if (!matrixStr.empty()) {
    if (ParseFloatList(matrixStr).size() < 6) {
      reportError(doc, node, "Invalid value '" + matrixStr + "' for 'matrix' attribute.");
    } else {
      matrix = MatrixFromString(matrixStr);
    }
  }
  auto child = node->firstChild;
  while (child) {
    if (child->type == DOMNodeType::Element && child->name == "ColorStop") {
      auto stop = parseColorStop(child.get(), doc);
      if (stop) {
        colorStops.push_back(stop);
      }
    } else if (child->type == DOMNodeType::Element) {
      reportError(doc, child.get(),
                  "Element '" + child->name + "' is not allowed in '" + node->name +
                      "'. Expected: ColorStop.");
    }
    child = child->nextSibling;
  }
}

static LinearGradient* parseLinearGradient(const DOMNode* node, PAGXDocument* doc) {
  auto gradient = makeNodeFromXML<LinearGradient>(node, doc);
  if (!gradient) {
    return nullptr;
  }
  gradient->startPoint =
      getPointAttribute(node, "startPoint", Default<LinearGradient>().startPoint, doc);
  gradient->endPoint = getPointAttribute(node, "endPoint", Default<LinearGradient>().endPoint, doc);
  parseGradientCommon(node, doc, gradient->matrix, gradient->colorStops);
  return gradient;
}

static RadialGradient* parseRadialGradient(const DOMNode* node, PAGXDocument* doc) {
  auto gradient = makeNodeFromXML<RadialGradient>(node, doc);
  if (!gradient) {
    return nullptr;
  }
  gradient->center = getPointAttribute(node, "center", Default<RadialGradient>().center, doc);
  gradient->radius = getFloatAttribute(node, "radius", Default<RadialGradient>().radius, doc);
  parseGradientCommon(node, doc, gradient->matrix, gradient->colorStops);
  return gradient;
}

static ConicGradient* parseConicGradient(const DOMNode* node, PAGXDocument* doc) {
  auto gradient = makeNodeFromXML<ConicGradient>(node, doc);
  if (!gradient) {
    return nullptr;
  }
  gradient->center = getPointAttribute(node, "center", Default<ConicGradient>().center, doc);
  gradient->startAngle =
      getFloatAttribute(node, "startAngle", Default<ConicGradient>().startAngle, doc);
  gradient->endAngle = getFloatAttribute(node, "endAngle", Default<ConicGradient>().endAngle, doc);
  parseGradientCommon(node, doc, gradient->matrix, gradient->colorStops);
  return gradient;
}

static DiamondGradient* parseDiamondGradient(const DOMNode* node, PAGXDocument* doc) {
  auto gradient = makeNodeFromXML<DiamondGradient>(node, doc);
  if (!gradient) {
    return nullptr;
  }
  gradient->center = getPointAttribute(node, "center", Default<DiamondGradient>().center, doc);
  gradient->radius = getFloatAttribute(node, "radius", Default<DiamondGradient>().radius, doc);
  parseGradientCommon(node, doc, gradient->matrix, gradient->colorStops);
  return gradient;
}

static ImagePattern* parseImagePattern(const DOMNode* node, PAGXDocument* doc) {
  auto pattern = makeNodeFromXML<ImagePattern>(node, doc);
  if (!pattern) {
    return nullptr;
  }
  auto imageAttr = getAttribute(node, "image");
  if (!imageAttr.empty()) {
    if (imageAttr[0] == '@') {
      pattern->image = doc->findNode<Image>(imageAttr.substr(1));
      if (!pattern->image) {
        reportError(doc, node, "Resource '" + imageAttr + "' not found for 'image' attribute.");
      }
    } else {
      // Inline image source (data URI or file path)
      pattern->image = doc->makeNode<Image>();
      auto data = DecodeBase64DataURI(imageAttr);
      if (data) {
        pattern->image->data = data;
      } else {
        pattern->image->filePath = imageAttr;
      }
    }
  }
  pattern->tileModeX = GET_ENUM(node, "tileModeX", "clamp", doc, TileMode);
  pattern->tileModeY = GET_ENUM(node, "tileModeY", "clamp", doc, TileMode);
  pattern->filterMode = GET_ENUM(node, "filterMode", "linear", doc, FilterMode);
  pattern->mipmapMode = GET_ENUM(node, "mipmapMode", "linear", doc, MipmapMode);
  auto matrixStr = getAttribute(node, "matrix");
  if (!matrixStr.empty()) {
    if (ParseFloatList(matrixStr).size() < 6) {
      reportError(doc, node, "Invalid value '" + matrixStr + "' for 'matrix' attribute.");
    } else {
      pattern->matrix = MatrixFromString(matrixStr);
    }
  }
  return pattern;
}

static ColorStop* parseColorStop(const DOMNode* node, PAGXDocument* doc) {
  auto stop = makeNodeFromXML<ColorStop>(node, doc);
  stop->offset = getFloatAttribute(node, "offset", Default<ColorStop>().offset, doc);
  auto colorStr = getAttribute(node, "color");
  if (!colorStr.empty()) {
    stop->color = getColorAttribute(node, "color", doc);
  }
  return stop;
}

//==============================================================================
// Resource parsing
//==============================================================================

static Image* parseImage(const DOMNode* node, PAGXDocument* doc) {
  auto image = makeNodeFromXML<Image>(node, doc);
  if (!image) {
    return nullptr;
  }
  auto source = getAttribute(node, "source");
  auto data = DecodeBase64DataURI(source);
  if (data) {
    image->data = data;
  } else {
    image->filePath = source;
  }
  return image;
}

static PathData* parsePathData(const DOMNode* node, PAGXDocument* doc) {
  auto pathData = makeNodeFromXML<PathData>(node, doc);
  if (!pathData) {
    return nullptr;
  }
  auto data = getAttribute(node, "data");
  if (!data.empty()) {
    *pathData = PathDataFromSVGString(data);
  }
  return pathData;
}

static Composition* parseComposition(const DOMNode* node, PAGXDocument* doc) {
  auto comp = makeNodeFromXML<Composition>(node, doc);
  if (!comp) {
    return nullptr;
  }
  comp->width = getFloatAttribute(node, "width", Default<Composition>().width, doc);
  comp->height = getFloatAttribute(node, "height", Default<Composition>().height, doc);
  auto child = node->firstChild;
  while (child) {
    if (child->type == DOMNodeType::Element) {
      if (child->name == "Layer") {
        auto layer = parseLayer(child.get(), doc);
        if (layer) {
          comp->layers.push_back(layer);
        }
      } else {
        reportError(
            doc, child.get(),
            "Element '" + child->name + "' is not allowed in 'Composition'. Expected: Layer.");
      }
    }
    child = child->nextSibling;
  }
  return comp;
}

static Font* parseFont(const DOMNode* node, PAGXDocument* doc) {
  auto font = makeNodeFromXML<Font>(node, doc);
  if (!font) {
    return nullptr;
  }
  font->unitsPerEm = getIntAttribute(node, "unitsPerEm", Default<Font>().unitsPerEm, doc);
  auto child = node->firstChild;
  while (child) {
    if (child->type == DOMNodeType::Element) {
      if (child->name == "Glyph") {
        auto glyph = parseGlyph(child.get(), doc);
        if (glyph) {
          font->glyphs.push_back(glyph);
        }
      } else {
        reportError(doc, child.get(),
                    "Element '" + child->name + "' is not allowed in 'Font'. Expected: Glyph.");
      }
    }
    child = child->nextSibling;
  }
  return font;
}

static Glyph* parseGlyph(const DOMNode* node, PAGXDocument* doc) {
  auto glyph = makeNodeFromXML<Glyph>(node, doc);
  if (!glyph) {
    return nullptr;
  }
  auto pathAttr = getAttribute(node, "path");
  if (!pathAttr.empty()) {
    if (pathAttr[0] == '@') {
      glyph->path = doc->findNode<PathData>(pathAttr.substr(1));
      if (!glyph->path) {
        reportError(doc, node,
                    "Resource '@" + pathAttr.substr(1) + "' not found for 'path' attribute.");
      }
    } else {
      glyph->path = doc->makeNode<PathData>();
      *glyph->path = PathDataFromSVGString(pathAttr);
    }
  }
  auto imageAttr = getAttribute(node, "image");
  if (!imageAttr.empty()) {
    if (imageAttr[0] == '@') {
      // Reference to existing Image resource
      glyph->image = doc->findNode<Image>(imageAttr.substr(1));
      if (!glyph->image) {
        reportError(doc, node,
                    "Resource '@" + imageAttr.substr(1) + "' not found for 'image' attribute.");
      }
    } else {
      // Inline image source (data URI or file path)
      glyph->image = doc->makeNode<Image>();
      auto data = DecodeBase64DataURI(imageAttr);
      if (data) {
        glyph->image->data = data;
      } else {
        glyph->image->filePath = imageAttr;
      }
    }
  }
  auto offsetStr = getAttribute(node, "offset");
  if (!offsetStr.empty()) {
    glyph->offset = getPointAttribute(node, "offset", Default<Glyph>().offset, doc);
  }
  glyph->advance = getFloatAttribute(node, "advance", Default<Glyph>().advance, doc);
  return glyph;
}

static std::vector<Point> parseSemicolonSeparatedPoints(const std::string& str) {
  std::vector<Point> result = {};
  const char* ptr = str.c_str();
  const char* end = ptr + str.size();
  while (ptr < end) {
    while (ptr < end && (*ptr == ' ' || *ptr == '\t' || *ptr == ';')) {
      ++ptr;
    }
    if (ptr >= end) {
      break;
    }
    char* endPtr = nullptr;
    float x = strtof(ptr, &endPtr);
    if (endPtr == ptr) {
      break;
    }
    ptr = endPtr;
    while (ptr < end && (*ptr == ' ' || *ptr == '\t' || *ptr == ',')) {
      ++ptr;
    }
    float y = strtof(ptr, &endPtr);
    if (endPtr == ptr) {
      break;
    }
    ptr = endPtr;
    result.push_back({x, y});
  }
  return result;
}

static GlyphRun* parseGlyphRun(const DOMNode* node, PAGXDocument* doc) {
  auto run = makeNodeFromXML<GlyphRun>(node, doc);
  if (!run) {
    return nullptr;
  }
  auto fontAttr = getAttribute(node, "font");
  if (!fontAttr.empty() && fontAttr[0] == '@') {
    auto fontId = fontAttr.substr(1);
    run->font = doc->findNode<Font>(fontId);
    if (!run->font) {
      reportError(doc, node, "Resource '" + fontAttr + "' not found for 'font' attribute.");
    }
  }
  run->fontSize = getFloatAttribute(node, "fontSize", Default<GlyphRun>().fontSize, doc);
  run->x = getFloatAttribute(node, "x", Default<GlyphRun>().x, doc);
  run->y = getFloatAttribute(node, "y", Default<GlyphRun>().y, doc);

  // Parse glyphs regardless of whether font is valid, to maintain data consistency.
  auto glyphsStr = getAttribute(node, "glyphs");
  if (!glyphsStr.empty()) {
    const char* ptr = glyphsStr.c_str();
    const char* end = ptr + glyphsStr.size();
    while (ptr < end) {
      while (ptr < end && (*ptr == ' ' || *ptr == '\t' || *ptr == ',')) {
        ++ptr;
      }
      if (ptr >= end) {
        break;
      }
      char* endPtr = nullptr;
      long value = strtol(ptr, &endPtr, 10);
      if (endPtr == ptr) {
        reportError(doc, node,
                    "Invalid 'glyphs' value: parse error at position " +
                        std::to_string(ptr - glyphsStr.c_str()) + ".");
        break;
      }
      if (value < 0 || value > 65535) {
        reportError(doc, node,
                    "Invalid 'glyphs' value: glyph index " + std::to_string(value) +
                        " out of range [0, 65535].");
        break;
      }
      run->glyphs.push_back(static_cast<uint16_t>(value));
      ptr = endPtr;
    }
  }

  // Parse xOffsets (comma-separated x offsets)
  auto xOffsetsStr = getAttribute(node, "xOffsets");
  if (!xOffsetsStr.empty()) {
    run->xOffsets = ParseFloatList(xOffsetsStr);
  }

  // Parse positions
  auto posStr = getAttribute(node, "positions");
  if (!posStr.empty()) {
    run->positions = parseSemicolonSeparatedPoints(posStr);
  }

  // Parse anchors
  auto anchorsStr = getAttribute(node, "anchors");
  if (!anchorsStr.empty()) {
    run->anchors = parseSemicolonSeparatedPoints(anchorsStr);
  }

  // Parse scales
  auto scalesStr = getAttribute(node, "scales");
  if (!scalesStr.empty()) {
    run->scales = parseSemicolonSeparatedPoints(scalesStr);
  }

  // Parse rotations (comma-separated angles in degrees)
  auto rotationsStr = getAttribute(node, "rotations");
  if (!rotationsStr.empty()) {
    run->rotations = ParseFloatList(rotationsStr);
  }

  // Parse skews (comma-separated angles in degrees)
  auto skewsStr = getAttribute(node, "skews");
  if (!skewsStr.empty()) {
    run->skews = ParseFloatList(skewsStr);
  }

  // Parse linebox bounds (x,y,w,h)
  auto boundsStr = getAttribute(node, "bounds");
  if (!boundsStr.empty()) {
    auto values = ParseFloatList(boundsStr);
    if (values.size() >= 4) {
      run->bounds = Rect::MakeXYWH(values[0], values[1], values[2], values[3]);
    }
  }

  return run;
}

//==============================================================================
// Layer style parsing
//==============================================================================

template <typename T>
static void parseShadowAttributes(const DOMNode* node, PAGXDocument* doc, float& offsetX,
                                  float& offsetY, float& blurX, float& blurY, Color& color) {
  offsetX = getFloatAttribute(node, "offsetX", Default<T>().offsetX, doc);
  offsetY = getFloatAttribute(node, "offsetY", Default<T>().offsetY, doc);
  blurX = getFloatAttribute(node, "blurX", Default<T>().blurX, doc);
  blurY = getFloatAttribute(node, "blurY", Default<T>().blurY, doc);
  auto colorStr = getAttribute(node, "color");
  if (!colorStr.empty()) {
    color = getColorAttribute(node, "color", doc);
  }
}

static DropShadowStyle* parseDropShadowStyle(const DOMNode* node, PAGXDocument* doc) {
  auto style = makeNodeFromXML<DropShadowStyle>(node, doc);
  if (!style) {
    return nullptr;
  }
  style->blendMode = GET_ENUM(node, "blendMode", "normal", doc, BlendMode);
  style->excludeChildEffects = getBoolAttribute(
      node, "excludeChildEffects", Default<DropShadowStyle>().excludeChildEffects, doc);
  parseShadowAttributes<DropShadowStyle>(node, doc, style->offsetX, style->offsetY, style->blurX,
                                         style->blurY, style->color);
  style->showBehindLayer =
      getBoolAttribute(node, "showBehindLayer", Default<DropShadowStyle>().showBehindLayer, doc);
  return style;
}

static InnerShadowStyle* parseInnerShadowStyle(const DOMNode* node, PAGXDocument* doc) {
  auto style = makeNodeFromXML<InnerShadowStyle>(node, doc);
  if (!style) {
    return nullptr;
  }
  style->blendMode = GET_ENUM(node, "blendMode", "normal", doc, BlendMode);
  style->excludeChildEffects = getBoolAttribute(
      node, "excludeChildEffects", Default<InnerShadowStyle>().excludeChildEffects, doc);
  parseShadowAttributes<InnerShadowStyle>(node, doc, style->offsetX, style->offsetY, style->blurX,
                                          style->blurY, style->color);
  return style;
}

static BackgroundBlurStyle* parseBackgroundBlurStyle(const DOMNode* node, PAGXDocument* doc) {
  auto style = makeNodeFromXML<BackgroundBlurStyle>(node, doc);
  if (!style) {
    return nullptr;
  }
  style->blendMode = GET_ENUM(node, "blendMode", "normal", doc, BlendMode);
  style->excludeChildEffects = getBoolAttribute(
      node, "excludeChildEffects", Default<BackgroundBlurStyle>().excludeChildEffects, doc);
  style->blurX = getFloatAttribute(node, "blurX", Default<BackgroundBlurStyle>().blurX, doc);
  style->blurY = getFloatAttribute(node, "blurY", Default<BackgroundBlurStyle>().blurY, doc);
  style->tileMode = GET_ENUM(node, "tileMode", "mirror", doc, TileMode);
  return style;
}

//==============================================================================
// Layer filter parsing
//==============================================================================

static BlurFilter* parseBlurFilter(const DOMNode* node, PAGXDocument* doc) {
  auto filter = makeNodeFromXML<BlurFilter>(node, doc);
  if (!filter) {
    return nullptr;
  }
  filter->blurX = getFloatAttribute(node, "blurX", Default<BlurFilter>().blurX, doc);
  filter->blurY = getFloatAttribute(node, "blurY", Default<BlurFilter>().blurY, doc);
  filter->tileMode = GET_ENUM(node, "tileMode", "decal", doc, TileMode);
  return filter;
}

static DropShadowFilter* parseDropShadowFilter(const DOMNode* node, PAGXDocument* doc) {
  auto filter = makeNodeFromXML<DropShadowFilter>(node, doc);
  if (!filter) {
    return nullptr;
  }
  parseShadowAttributes<DropShadowFilter>(node, doc, filter->offsetX, filter->offsetY,
                                          filter->blurX, filter->blurY, filter->color);
  filter->shadowOnly =
      getBoolAttribute(node, "shadowOnly", Default<DropShadowFilter>().shadowOnly, doc);
  return filter;
}

static InnerShadowFilter* parseInnerShadowFilter(const DOMNode* node, PAGXDocument* doc) {
  auto filter = makeNodeFromXML<InnerShadowFilter>(node, doc);
  if (!filter) {
    return nullptr;
  }
  parseShadowAttributes<InnerShadowFilter>(node, doc, filter->offsetX, filter->offsetY,
                                           filter->blurX, filter->blurY, filter->color);
  filter->shadowOnly =
      getBoolAttribute(node, "shadowOnly", Default<InnerShadowFilter>().shadowOnly, doc);
  return filter;
}

static BlendFilter* parseBlendFilter(const DOMNode* node, PAGXDocument* doc) {
  auto filter = makeNodeFromXML<BlendFilter>(node, doc);
  if (!filter) {
    return nullptr;
  }
  auto colorStr = getAttribute(node, "color");
  if (!colorStr.empty()) {
    filter->color = getColorAttribute(node, "color", doc);
  }
  filter->blendMode = GET_ENUM(node, "blendMode", "normal", doc, BlendMode);
  return filter;
}

static ColorMatrixFilter* parseColorMatrixFilter(const DOMNode* node, PAGXDocument* doc) {
  auto filter = makeNodeFromXML<ColorMatrixFilter>(node, doc);
  if (!filter) {
    return nullptr;
  }
  auto matrixStr = getAttribute(node, "matrix");
  if (!matrixStr.empty()) {
    auto values = ParseFloatList(matrixStr);
    if (values.size() < 20) {
      reportError(doc, node, "Invalid 'matrix' value (expected 20 numbers).");
    }
    for (size_t i = 0; i < std::min(values.size(), size_t(20)); i++) {
      filter->matrix[i] = values[i];
    }
  }
  return filter;
}

//==============================================================================
// Utility functions
//==============================================================================

static const std::string& getAttribute(const DOMNode* node, const std::string& name,
                                       const std::string& defaultValue) {
  auto* value = node->findAttribute(name);
  return value ? *value : defaultValue;
}

static float getFloatAttribute(const DOMNode* node, const std::string& name, float defaultValue,
                               PAGXDocument* doc) {
  auto* str = node->findAttribute(name);
  if (!str || str->empty()) {
    return defaultValue;
  }
  char* endPtr = nullptr;
  float value = strtof(str->c_str(), &endPtr);
  if (endPtr == str->c_str()) {
    if (doc) {
      reportError(doc, node, "Invalid value '" + *str + "' for '" + name + "' attribute.");
    }
    return defaultValue;
  }
  return value;
}

static int getIntAttribute(const DOMNode* node, const std::string& name, int defaultValue,
                           PAGXDocument* doc) {
  auto* str = node->findAttribute(name);
  if (!str || str->empty()) {
    return defaultValue;
  }
  char* endPtr = nullptr;
  long value = strtol(str->c_str(), &endPtr, 10);
  if (endPtr == str->c_str()) {
    if (doc) {
      reportError(doc, node, "Invalid value '" + *str + "' for '" + name + "' attribute.");
    }
    return defaultValue;
  }
  if (value < INT_MIN || value > INT_MAX) {
    if (doc) {
      reportError(doc, node, "Value out of range for '" + name + "' attribute.");
    }
    return defaultValue;
  }
  return static_cast<int>(value);
}

static bool getBoolAttribute(const DOMNode* node, const std::string& name, bool defaultValue,
                             PAGXDocument* doc) {
  auto* str = node->findAttribute(name);
  if (!str || str->empty()) {
    return defaultValue;
  }
  if (*str == "true" || *str == "1") {
    return true;
  }
  if (*str == "false" || *str == "0") {
    return false;
  }
  if (doc) {
    reportError(doc, node, "Invalid value '" + *str + "' for '" + name + "' attribute.");
  }
  return defaultValue;
}

static std::optional<float> getOptionalFloatAttribute(const DOMNode* node, const std::string& name,
                                                      PAGXDocument* doc) {
  auto* str = node->findAttribute(name);
  if (!str || str->empty()) {
    return std::nullopt;
  }
  char* endPtr = nullptr;
  float value = strtof(str->c_str(), &endPtr);
  if (endPtr == str->c_str()) {
    if (doc) {
      reportError(doc, node, "Invalid value '" + *str + "' for '" + name + "' attribute.");
    }
    return std::nullopt;
  }
  return value;
}

static const char* skipWhitespaceAndComma(const char* ptr, const char* end) {
  while (ptr < end && (*ptr == ' ' || *ptr == '\t' || *ptr == ',')) {
    ++ptr;
  }
  return ptr;
}

static std::pair<float, float> parseTwoFloats(const std::string& str, bool* outValid) {
  float first = 0;
  float second = 0;
  const char* ptr = str.c_str();
  const char* end = ptr + str.size();
  char* endPtr = nullptr;
  ptr = skipWhitespaceAndComma(ptr, end);
  first = strtof(ptr, &endPtr);
  if (endPtr > ptr) {
    ptr = skipWhitespaceAndComma(endPtr, end);
    second = strtof(ptr, &endPtr);
    if (outValid) {
      *outValid = (endPtr > ptr);
    }
  } else {
    if (outValid) {
      *outValid = false;
    }
  }
  return {first, second};
}

static Point parsePoint(const std::string& str, bool* outValid) {
  auto [x, y] = parseTwoFloats(str, outValid);
  return {x, y};
}

static Size parseSize(const std::string& str, bool* outValid) {
  auto [width, height] = parseTwoFloats(str, outValid);
  return {width, height};
}

static Rect parseRect(const std::string& str, bool* outValid) {
  Rect rect = {};
  const char* ptr = str.c_str();
  const char* end = ptr + str.size();
  char* endPtr = nullptr;
  bool valid = true;
  ptr = skipWhitespaceAndComma(ptr, end);
  rect.x = strtof(ptr, &endPtr);
  if (endPtr > ptr) {
    ptr = skipWhitespaceAndComma(endPtr, end);
    rect.y = strtof(ptr, &endPtr);
  }
  if (endPtr > ptr) {
    ptr = skipWhitespaceAndComma(endPtr, end);
    rect.width = strtof(ptr, &endPtr);
  }
  if (endPtr > ptr) {
    ptr = skipWhitespaceAndComma(endPtr, end);
    rect.height = strtof(ptr, &endPtr);
    if (endPtr <= ptr) {
      valid = false;
    }
  } else {
    valid = false;
  }
  if (outValid) {
    *outValid = valid;
  }
  return rect;
}

namespace {
int parseHexDigit(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }
  return -1;
}
}  // namespace

static Color parseColor(const std::string& str, bool* outValid) {
  if (str.empty()) {
    if (outValid) {
      *outValid = false;
    }
    return {};
  }
  // Hex format: #RGB, #RRGGBB, #RRGGBBAA (sRGB)
  if (str[0] == '#') {
    Color color = {};
    color.colorSpace = ColorSpace::SRGB;
    if (str.size() == 4) {
      int r = parseHexDigit(str[1]);
      int g = parseHexDigit(str[2]);
      int b = parseHexDigit(str[3]);
      if (r < 0 || g < 0 || b < 0) {
        if (outValid) {
          *outValid = false;
        }
        return {};
      }
      color.red = static_cast<float>(r * 17) / 255.0f;
      color.green = static_cast<float>(g * 17) / 255.0f;
      color.blue = static_cast<float>(b * 17) / 255.0f;
      color.alpha = 1.0f;
      if (outValid) {
        *outValid = true;
      }
      return color;
    }
    if (str.size() == 7 || str.size() == 9) {
      int r1 = parseHexDigit(str[1]);
      int r2 = parseHexDigit(str[2]);
      int g1 = parseHexDigit(str[3]);
      int g2 = parseHexDigit(str[4]);
      int b1 = parseHexDigit(str[5]);
      int b2 = parseHexDigit(str[6]);
      if (r1 < 0 || r2 < 0 || g1 < 0 || g2 < 0 || b1 < 0 || b2 < 0) {
        if (outValid) {
          *outValid = false;
        }
        return {};
      }
      int r = r1 * 16 + r2;
      int g = g1 * 16 + g2;
      int b = b1 * 16 + b2;
      color.red = static_cast<float>(r) / 255.0f;
      color.green = static_cast<float>(g) / 255.0f;
      color.blue = static_cast<float>(b) / 255.0f;
      if (str.size() == 9) {
        int a1 = parseHexDigit(str[7]);
        int a2 = parseHexDigit(str[8]);
        if (a1 < 0 || a2 < 0) {
          if (outValid) {
            *outValid = false;
          }
          return {};
        }
        color.alpha = static_cast<float>(a1 * 16 + a2) / 255.0f;
      } else {
        color.alpha = 1.0f;
      }
      if (outValid) {
        *outValid = true;
      }
      return color;
    }
  }
  // sRGB float format: srgb(r, g, b) or srgb(r, g, b, a)
  // Display P3 format: p3(r, g, b) or p3(r, g, b, a)
  struct FunctionalColorFormat {
    const char* prefix;
    size_t prefixLen;
    ColorSpace colorSpace;
  };
  static const FunctionalColorFormat formats[] = {
      {"srgb(", 5, ColorSpace::SRGB},
      {"p3(", 3, ColorSpace::DisplayP3},
  };
  for (const auto& fmt : formats) {
    if (str.compare(0, fmt.prefixLen, fmt.prefix) != 0) {
      continue;
    }
    const char* ptr = str.c_str() + fmt.prefixLen;
    const char* strEnd = str.c_str() + str.size();
    char* endPtr = nullptr;
    float components[4] = {};
    int count = 0;
    for (; count < 4 && ptr < strEnd && *ptr != ')'; ++count) {
      ptr = skipWhitespaceAndComma(ptr, strEnd);
      if (ptr >= strEnd || *ptr == ')') {
        break;
      }
      components[count] = strtof(ptr, &endPtr);
      if (endPtr == ptr) {
        break;
      }
      ptr = endPtr;
    }
    if (count < 3) {
      continue;
    }
    Color color = {};
    color.red = components[0];
    color.green = components[1];
    color.blue = components[2];
    color.alpha = count >= 4 ? components[3] : 1.0f;
    color.colorSpace = fmt.colorSpace;
    if (outValid) {
      *outValid = true;
    }
    return color;
  }
  if (outValid) {
    *outValid = false;
  }
  return {};
}

template <typename T, typename Parser>
static T getParsedAttribute(const DOMNode* node, const char* name, T defaultValue,
                            PAGXDocument* doc, Parser parser) {
  auto* str = node->findAttribute(name);
  if (!str || str->empty()) {
    return defaultValue;
  }
  bool valid = false;
  auto result = parser(*str, &valid);
  if (!valid) {
    reportError(doc, node,
                "Invalid value '" + *str + "' for '" + std::string(name) + "' attribute.");
    return defaultValue;
  }
  return result;
}

static Point getPointAttribute(const DOMNode* node, const char* name, Point defaultValue,
                               PAGXDocument* doc) {
  return getParsedAttribute(node, name, defaultValue, doc, parsePoint);
}

static Size getSizeAttribute(const DOMNode* node, const char* name, Size defaultValue,
                             PAGXDocument* doc) {
  return getParsedAttribute(node, name, defaultValue, doc, parseSize);
}

static Rect getRectAttribute(const DOMNode* node, const char* name, Rect defaultValue,
                             PAGXDocument* doc) {
  return getParsedAttribute(node, name, defaultValue, doc, parseRect);
}

static Color getColorAttribute(const DOMNode* node, const char* name, PAGXDocument* doc) {
  auto* str = node->findAttribute(name);
  if (!str || str->empty()) {
    return {};
  }
  bool valid = false;
  auto result = parseColor(*str, &valid);
  if (!valid) {
    reportError(doc, node,
                "Invalid value '" + *str + "' for '" + std::string(name) + "' attribute.");
  }
  return result;
}

//==============================================================================
// Public API implementation
//==============================================================================

std::shared_ptr<PAGXDocument> PAGXImporter::FromFile(const std::string& filePath) {
  std::ifstream file(filePath, std::ios::binary | std::ios::ate);
  if (!file) {
    return nullptr;
  }
  auto fileSize = file.tellg();
  if (fileSize <= 0) {
    return nullptr;
  }
  file.seekg(0, std::ios::beg);
  std::string content;
  content.resize(static_cast<size_t>(fileSize));
  if (!file.read(&content[0], fileSize)) {
    return nullptr;
  }

  auto doc = FromXML(content);
  if (doc) {
    // Convert relative paths to absolute paths
    std::string basePath = {};
    auto lastSlash = filePath.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
      basePath = filePath.substr(0, lastSlash + 1);
    }
    if (!basePath.empty()) {
      for (auto& node : doc->nodes) {
        if (node->nodeType() == NodeType::Image) {
          auto* image = static_cast<Image*>(node.get());
          if (!image->filePath.empty() && image->filePath[0] != '/' &&
              image->filePath.find("://") == std::string::npos) {
            image->filePath = basePath + image->filePath;
          }
        }
      }
    }
  }
  return doc;
}

std::shared_ptr<PAGXDocument> PAGXImporter::FromXML(const std::string& xmlContent) {
  return FromXML(reinterpret_cast<const uint8_t*>(xmlContent.data()), xmlContent.size());
}

std::shared_ptr<PAGXDocument> PAGXImporter::FromXML(const uint8_t* data, size_t length) {
  auto dom = XMLDOM::Make(data, length);
  if (!dom) {
    return nullptr;
  }
  auto root = dom->getRootNode();
  if (!root || root->name != "pagx") {
    return nullptr;
  }
  auto doc = std::shared_ptr<PAGXDocument>(new PAGXDocument());
  parseDocument(root.get(), doc.get());
  return doc;
}

static void parseDocument(const DOMNode* root, PAGXDocument* doc) {
  doc->version = getAttribute(root, "version", "1.0");
  doc->width = getFloatAttribute(root, "width", 0, doc);
  doc->height = getFloatAttribute(root, "height", 0, doc);
  parseCustomData(root, doc);

  // First pass: Parse Resources.
  auto child = root->getFirstChild("Resources");
  if (child) {
    parseResources(child.get(), doc);
  }

  // Second pass: Parse Layers.
  child = root->firstChild;
  while (child) {
    if (child->type == DOMNodeType::Element) {
      if (child->name == "Layer") {
        auto layer = parseLayer(child.get(), doc);
        if (layer) {
          doc->layers.push_back(layer);
        }
      } else if (child->name != "Resources") {
        reportError(
            doc, child.get(),
            "Element '" + child->name + "' is not allowed in 'pagx'. Expected: Resources, Layer.");
      }
    }
    child = child->nextSibling;
  }
}

}  // namespace pagx
