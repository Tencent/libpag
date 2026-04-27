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
#include <cmath>
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
#include "pagx/nodes/Gradient.h"
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

static void ReportError(PAGXDocument* doc, const DOMNode* node, const std::string& message) {
  auto fullMessage = "line " + std::to_string(node->line) + ": " + message;
  doc->errors.push_back(fullMessage);
#if DEBUG
  LOGE("%s", fullMessage.c_str());
#endif
}

static const std::string& GetAttribute(const DOMNode* node, const std::string& name,
                                       const std::string& defaultValue = EmptyString());
static float GetFloatAttribute(const DOMNode* node, const std::string& name, float defaultValue = 0,
                               PAGXDocument* doc = nullptr);
static int GetIntAttribute(const DOMNode* node, const std::string& name, int defaultValue = 0,
                           PAGXDocument* doc = nullptr);
static bool GetBoolAttribute(const DOMNode* node, const std::string& name,
                             bool defaultValue = false, PAGXDocument* doc = nullptr);
static Point ParsePoint(const std::string& str, bool* outValid = nullptr);
static Size ParseSize(const std::string& str, bool* outValid = nullptr);
static Rect ParseRect(const std::string& str, bool* outValid = nullptr);
static Color ParseColor(const std::string& str, bool* outValid = nullptr);
static Point GetPointAttribute(const DOMNode* node, const char* name, Point defaultValue,
                               PAGXDocument* doc);
static Size GetSizeAttribute(const DOMNode* node, const char* name, Size defaultValue,
                             PAGXDocument* doc);
static Rect GetRectAttribute(const DOMNode* node, const char* name, Rect defaultValue,
                             PAGXDocument* doc);
static Color GetColorAttribute(const DOMNode* node, const char* name, PAGXDocument* doc);

template <typename EnumType>
static EnumType GetEnumAttribute(const DOMNode* node, const char* name,
                                 const std::string& defaultStr, PAGXDocument* doc,
                                 EnumType (*fromString)(const std::string&),
                                 bool (*isValid)(const std::string&)) {
  auto& str = GetAttribute(node, name, defaultStr);
  if (!isValid(str)) {
    ReportError(doc, node,
                "Invalid value '" + str + "' for '" + std::string(name) + "' attribute.");
    return fromString(defaultStr);
  }
  return fromString(str);
}

#define GET_ENUM(node, name, defaultStr, doc, EnumType)                         \
  GetEnumAttribute<EnumType>(node, name, defaultStr, doc, EnumType##FromString, \
                             IsValid##EnumType##String)

// Forward declarations for parse functions
static void ParseDocument(const DOMNode* root, PAGXDocument* doc);
static void ParseResources(const DOMNode* node, PAGXDocument* doc);
static Layer* ParseLayer(const DOMNode* node, PAGXDocument* doc);
static void ParseContents(const DOMNode* node, Layer* layer, PAGXDocument* doc);
static void ParseStyles(const DOMNode* node, Layer* layer, PAGXDocument* doc);
static void ParseFilters(const DOMNode* node, Layer* layer, PAGXDocument* doc);
static Element* ParseElement(const DOMNode* node, PAGXDocument* doc);
static ColorSource* ParseColorSource(const DOMNode* node, PAGXDocument* doc);
static LayerStyle* ParseLayerStyle(const DOMNode* node, PAGXDocument* doc);
static LayerFilter* ParseLayerFilter(const DOMNode* node, PAGXDocument* doc);
static Rectangle* ParseRectangle(const DOMNode* node, PAGXDocument* doc);
static Ellipse* ParseEllipse(const DOMNode* node, PAGXDocument* doc);
static Polystar* ParsePolystar(const DOMNode* node, PAGXDocument* doc);
static Path* ParsePath(const DOMNode* node, PAGXDocument* doc);
static Text* ParseText(const DOMNode* node, PAGXDocument* doc);
static Fill* ParseFill(const DOMNode* node, PAGXDocument* doc);
static Stroke* ParseStroke(const DOMNode* node, PAGXDocument* doc);
static TrimPath* ParseTrimPath(const DOMNode* node, PAGXDocument* doc);
static RoundCorner* ParseRoundCorner(const DOMNode* node, PAGXDocument* doc);
static MergePath* ParseMergePath(const DOMNode* node, PAGXDocument* doc);
static TextModifier* ParseTextModifier(const DOMNode* node, PAGXDocument* doc);
static TextPath* ParseTextPath(const DOMNode* node, PAGXDocument* doc);
static TextBox* ParseTextBox(const DOMNode* node, PAGXDocument* doc);
static Repeater* ParseRepeater(const DOMNode* node, PAGXDocument* doc);
static Group* ParseGroup(const DOMNode* node, PAGXDocument* doc);
static RangeSelector* ParseRangeSelector(const DOMNode* node, PAGXDocument* doc);
static SolidColor* ParseSolidColor(const DOMNode* node, PAGXDocument* doc);
static LinearGradient* ParseLinearGradient(const DOMNode* node, PAGXDocument* doc);
static RadialGradient* ParseRadialGradient(const DOMNode* node, PAGXDocument* doc);
static ConicGradient* ParseConicGradient(const DOMNode* node, PAGXDocument* doc);
static DiamondGradient* ParseDiamondGradient(const DOMNode* node, PAGXDocument* doc);
static ImagePattern* ParseImagePattern(const DOMNode* node, PAGXDocument* doc);
static ColorStop* ParseColorStop(const DOMNode* node, PAGXDocument* doc);
static Image* ParseImage(const DOMNode* node, PAGXDocument* doc);
static PathData* ParsePathData(const DOMNode* node, PAGXDocument* doc);
static Composition* ParseComposition(const DOMNode* node, PAGXDocument* doc);
static Font* ParseFont(const DOMNode* node, PAGXDocument* doc);
static Glyph* ParseGlyph(const DOMNode* node, PAGXDocument* doc);
static GlyphRun* ParseGlyphRun(const DOMNode* node, PAGXDocument* doc);
static DropShadowStyle* ParseDropShadowStyle(const DOMNode* node, PAGXDocument* doc);
static InnerShadowStyle* ParseInnerShadowStyle(const DOMNode* node, PAGXDocument* doc);
static BackgroundBlurStyle* ParseBackgroundBlurStyle(const DOMNode* node, PAGXDocument* doc);
static BlurFilter* ParseBlurFilter(const DOMNode* node, PAGXDocument* doc);
static DropShadowFilter* ParseDropShadowFilter(const DOMNode* node, PAGXDocument* doc);
static InnerShadowFilter* ParseInnerShadowFilter(const DOMNode* node, PAGXDocument* doc);
static BlendFilter* ParseBlendFilter(const DOMNode* node, PAGXDocument* doc);
static ColorMatrixFilter* ParseColorMatrixFilter(const DOMNode* node, PAGXDocument* doc);

//==============================================================================
// Custom data parsing
//==============================================================================

static void EscapeXML(const std::string& input, std::string& output) {
  for (char c : input) {
    switch (c) {
      case '&':
        output += "&amp;";
        break;
      case '<':
        output += "&lt;";
        break;
      case '>':
        output += "&gt;";
        break;
      case '"':
        output += "&quot;";
        break;
      case '\'':
        output += "&apos;";
        break;
      default:
        output += c;
        break;
    }
  }
}

static void SerializeDOMNode(const DOMNode* node, std::string& output) {
  if (node->type == DOMNodeType::Text) {
    EscapeXML(node->name, output);
    return;
  }
  output += "<";
  output += node->name;
  for (const auto& attr : node->attributes) {
    output += " ";
    output += attr.name;
    output += "=\"";
    EscapeXML(attr.value, output);
    output += "\"";
  }
  if (!node->firstChild) {
    output += "/>";
    return;
  }
  output += ">";
  auto child = node->firstChild;
  while (child) {
    SerializeDOMNode(child.get(), output);
    child = child->nextSibling;
  }
  output += "</";
  output += node->name;
  output += ">";
}

static void ParseCustomData(const DOMNode* xmlNode, Node* node) {
  for (const auto& attr : xmlNode->attributes) {
    if (attr.name.length() > 5 && attr.name.compare(0, 5, "data-") == 0) {
      auto key = attr.name.substr(5);
      if (IsValidCustomDataKey(key)) {
        node->customData[std::move(key)] = attr.value;
      }
    }
  }
}

template <typename T>
static T* makeNodeFromXML(const DOMNode* xmlNode, PAGXDocument* doc) {
  auto id = GetAttribute(xmlNode, "id");
  // If a node with this ID was pre-registered, reuse it instead of creating a duplicate.
  auto* existing = id.empty() ? nullptr : doc->findNode(id);
  if (existing) {
    ParseCustomData(xmlNode, existing);
    existing->sourceLine = xmlNode->line;
    return static_cast<T*>(existing);
  }
  auto* node = doc->makeNode<T>(id);
  if (node) {
    ParseCustomData(xmlNode, node);
    node->sourceLine = xmlNode->line;
  }
  return node;
}

//==============================================================================
// Internal parser implementation
//==============================================================================

// Pre-registers a resource node by tag name. Creates an empty node of the correct type and
// registers its ID in nodeMap. Returns true if the tag name is a known resource type.
// NOTE: When adding a new resource type to PAGX, add a corresponding branch here and in
// ParseResource() below.
static bool PreRegisterResource(const DOMNode* node, PAGXDocument* doc) {
  auto id = GetAttribute(node, "id");
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
// PreRegisterResource() above.
static bool ParseResource(const DOMNode* node, PAGXDocument* doc) {
  if (node->name == "Image") {
    ParseImage(node, doc);
  } else if (node->name == "PathData") {
    ParsePathData(node, doc);
  } else if (node->name == "Font") {
    ParseFont(node, doc);
  } else if (node->name == "Composition") {
    ParseComposition(node, doc);
  } else {
    return ParseColorSource(node, doc) != nullptr;
  }
  return true;
}

static void ParseResources(const DOMNode* node, PAGXDocument* doc) {
  // First pass: pre-register all resource IDs so that cross-references via '@id' resolve
  // regardless of XML declaration order.
  auto child = node->firstChild;
  while (child) {
    if (child->type == DOMNodeType::Element) {
      PreRegisterResource(child.get(), doc);
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
    if (!ParseResource(current.get(), doc)) {
      ReportError(doc, current.get(),
                  "Element '" + current->name +
                      "' is not allowed in 'Resources'."
                      " Expected: Image, PathData, Composition, Font,"
                      " SolidColor, LinearGradient, RadialGradient,"
                      " ConicGradient, DiamondGradient, ImagePattern.");
    }
  }
}

// Forward declarations for helper functions defined later in the file.
static std::optional<float> GetOptionalFloatAttribute(const DOMNode* node, const std::string& name,
                                                      PAGXDocument* doc);

static float GetFloatAttributeOrNaN(const DOMNode* node, const std::string& name,
                                    PAGXDocument* doc) {
  auto value = GetOptionalFloatAttribute(node, name, doc);
  return value.has_value() ? *value : NAN;
}

// Parses a dimension attribute that may contain a percentage suffix (e.g. "100" or "50%") and
// writes the result into the node's absolute and percent fields.
// - When the value ends with "%": sets *outPercent to the numeric part and *outAbsolute to NaN.
// - Otherwise: sets *outAbsolute to the numeric value and *outPercent to NaN.
// Inputs forbidden by the XSD DimensionType pattern "[0-9]*\.?[0-9]+%?" (leading whitespace,
// sign, hex prefix, non-finite, negative, whitespace around the "%") report an error and leave
// both outputs as NaN. Absent attributes also leave both outputs as NaN but do not report.
static void ReadDimension(const DOMNode* node, const std::string& name, float* outAbsolute,
                          float* outPercent, PAGXDocument* doc) {
  *outAbsolute = NAN;
  *outPercent = NAN;
  auto* str = node->findAttribute(name);
  if (str == nullptr || str->empty()) {
    return;
  }
  const char* cstr = str->c_str();
  // Reject leading whitespace, sign, and hex prefix; strtof would otherwise accept them.
  char first = cstr[0];
  if (first == ' ' || first == '\t' || first == '+' || first == '-') {
    ReportError(doc, node, "Invalid value '" + *str + "' for '" + name + "' attribute.");
    return;
  }
  if (first == '0' && (cstr[1] == 'x' || cstr[1] == 'X')) {
    ReportError(doc, node, "Invalid value '" + *str + "' for '" + name + "' attribute.");
    return;
  }
  char* endPtr = nullptr;
  float value = strtof(cstr, &endPtr);
  if (endPtr == cstr || !std::isfinite(value) || value < 0) {
    ReportError(doc, node, "Invalid value '" + *str + "' for '" + name + "' attribute.");
    return;
  }
  if (*endPtr == '%') {
    if (endPtr[1] != '\0') {
      ReportError(doc, node, "Invalid value '" + *str + "' for '" + name + "' attribute.");
      return;
    }
    *outPercent = value;
    return;
  }
  if (*endPtr != '\0') {
    ReportError(doc, node, "Invalid value '" + *str + "' for '" + name + "' attribute.");
    return;
  }
  *outAbsolute = value;
}

static Padding GetPaddingAttribute(const DOMNode* node, PAGXDocument* doc) {
  auto& str = GetAttribute(node, "padding");
  if (str.empty()) {
    return {};
  }
  auto count = ParseFloatList(str).size();
  if (count != 1 && count != 2 && count != 4) {
    ReportError(doc, node, "Invalid value '" + str + "' for 'padding' attribute.");
    return {};
  }
  return PaddingFromString(str);
}

static Layer* ParseLayer(const DOMNode* node, PAGXDocument* doc) {
  auto layer = makeNodeFromXML<Layer>(node, doc);
  if (!layer) {
    return nullptr;
  }
  layer->name = GetAttribute(node, "name");
  layer->visible = GetBoolAttribute(node, "visible", Default<Layer>().visible, doc);
  layer->alpha = GetFloatAttribute(node, "alpha", Default<Layer>().alpha, doc);
  layer->blendMode = GET_ENUM(node, "blendMode", "normal", doc, BlendMode);
  layer->x = GetFloatAttribute(node, "x", Default<Layer>().x, doc);
  layer->y = GetFloatAttribute(node, "y", Default<Layer>().y, doc);
  ReadDimension(node, "width", &layer->width, &layer->percentWidth, doc);
  ReadDimension(node, "height", &layer->height, &layer->percentHeight, doc);
  layer->layout = GET_ENUM(node, "layout", "none", doc, LayoutMode);
  layer->gap = GetFloatAttribute(node, "gap", Default<Layer>().gap, doc);
  layer->flex = GetFloatAttribute(node, "flex", Default<Layer>().flex, doc);
  layer->padding = GetPaddingAttribute(node, doc);
  layer->alignment = GET_ENUM(node, "alignment", "stretch", doc, Alignment);
  layer->arrangement = GET_ENUM(node, "arrangement", "start", doc, Arrangement);
  layer->includeInLayout =
      GetBoolAttribute(node, "includeInLayout", Default<Layer>().includeInLayout, doc);
  layer->left = GetFloatAttributeOrNaN(node, "left", doc);
  layer->right = GetFloatAttributeOrNaN(node, "right", doc);
  layer->top = GetFloatAttributeOrNaN(node, "top", doc);
  layer->bottom = GetFloatAttributeOrNaN(node, "bottom", doc);
  layer->centerX = GetFloatAttributeOrNaN(node, "centerX", doc);
  layer->centerY = GetFloatAttributeOrNaN(node, "centerY", doc);
  auto matrixStr = GetAttribute(node, "matrix");
  if (!matrixStr.empty()) {
    if (ParseFloatList(matrixStr).size() < 6) {
      ReportError(doc, node, "Invalid value '" + matrixStr + "' for 'matrix' attribute.");
    } else {
      layer->matrix = MatrixFromString(matrixStr);
    }
  }
  auto matrix3DStr = GetAttribute(node, "matrix3D");
  if (!matrix3DStr.empty()) {
    auto floats = ParseFloatList(matrix3DStr);
    if (floats.size() == 16) {
      memcpy(layer->matrix3D.values, floats.data(), 16 * sizeof(float));
    } else {
      ReportError(doc, node, "Invalid 'matrix3D' value (expected 16 numbers).");
    }
  }
  layer->preserve3D = GetBoolAttribute(node, "preserve3D", Default<Layer>().preserve3D, doc);
  layer->antiAlias = GetBoolAttribute(node, "antiAlias", Default<Layer>().antiAlias, doc);
  layer->groupOpacity = GetBoolAttribute(node, "groupOpacity", Default<Layer>().groupOpacity, doc);
  layer->passThroughBackground =
      GetBoolAttribute(node, "passThroughBackground", Default<Layer>().passThroughBackground, doc);
  auto scrollRectStr = GetAttribute(node, "scrollRect");
  if (!scrollRectStr.empty()) {
    layer->scrollRect = GetRectAttribute(node, "scrollRect", {}, doc);
    layer->hasScrollRect = true;
  }
  layer->clipToBounds = GetBoolAttribute(node, "clipToBounds", Default<Layer>().clipToBounds, doc);

  auto maskAttr = GetAttribute(node, "mask");
  if (!maskAttr.empty() && maskAttr[0] == '@') {
    layer->mask = doc->findNode<Layer>(maskAttr.substr(1));
    if (!layer->mask) {
      ReportError(doc, node, "Resource '" + maskAttr + "' not found for 'mask' attribute.");
    }
  }
  layer->maskType = GET_ENUM(node, "maskType", "alpha", doc, MaskType);

  auto compositionAttr = GetAttribute(node, "composition");
  if (!compositionAttr.empty() && compositionAttr[0] == '@') {
    layer->composition = doc->findNode<Composition>(compositionAttr.substr(1));
    if (!layer->composition) {
      ReportError(doc, node,
                  "Resource '" + compositionAttr + "' not found for 'composition' attribute.");
    }
  }

  // Build directive attributes.
  layer->importDirective.source = GetAttribute(node, "import");
  layer->importDirective.format = GetAttribute(node, "importFormat");

  auto child = node->firstChild;
  while (child) {
    auto current = child;
    child = child->nextSibling;
    if (current->type != DOMNodeType::Element) {
      continue;
    }
    // Legacy format: support container nodes for backward compatibility.
    if (current->name == "contents") {
      ParseContents(current.get(), layer, doc);
      continue;
    }
    if (current->name == "styles") {
      ParseStyles(current.get(), layer, doc);
      continue;
    }
    if (current->name == "filters") {
      ParseFilters(current.get(), layer, doc);
      continue;
    }
    // New format: direct child elements without container nodes.
    if (current->name == "Layer") {
      auto childLayer = ParseLayer(current.get(), doc);
      if (childLayer) {
        layer->children.push_back(childLayer);
      }
      continue;
    }
    if (current->name == "svg") {
      // Inline SVG: serialize the entire <svg> node (including children) as raw XML text.
      if (!layer->importDirective.content.empty()) {
        ReportError(doc, current.get(),
                    "Multiple inline <svg> elements in the same Layer. Only the last one is kept.");
      }
      std::string svgText;
      SerializeDOMNode(current.get(), svgText);
      layer->importDirective.content = std::move(svgText);
      continue;
    }
    // Try to parse as VectorElement.
    auto element = ParseElement(current.get(), doc);
    if (element) {
      layer->contents.push_back(element);
      continue;
    }
    // Try to parse as LayerStyle.
    auto style = ParseLayerStyle(current.get(), doc);
    if (style) {
      layer->styles.push_back(style);
      continue;
    }
    // Try to parse as LayerFilter.
    auto filter = ParseLayerFilter(current.get(), doc);
    if (filter) {
      layer->filters.push_back(filter);
      continue;
    }
    // Unknown node type - report error.
    ReportError(doc, current.get(),
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

static void ParseContents(const DOMNode* node, Layer* layer, PAGXDocument* doc) {
  auto child = node->firstChild;
  while (child) {
    auto current = child;
    child = child->nextSibling;
    if (current->type != DOMNodeType::Element) {
      continue;
    }
    auto element = ParseElement(current.get(), doc);
    if (element) {
      layer->contents.push_back(element);
    } else {
      ReportError(doc, current.get(),
                  "Element '" + current->name +
                      "' is not allowed in 'contents'."
                      " Expected: Group, Rectangle, Ellipse, Polystar,"
                      " Path, Text, Fill, Stroke, TrimPath, RoundCorner,"
                      " MergePath, TextModifier, TextPath, TextBox, Repeater.");
    }
  }
}

static void ParseStyles(const DOMNode* node, Layer* layer, PAGXDocument* doc) {
  auto child = node->firstChild;
  while (child) {
    auto current = child;
    child = child->nextSibling;
    if (current->type != DOMNodeType::Element) {
      continue;
    }
    auto style = ParseLayerStyle(current.get(), doc);
    if (style) {
      layer->styles.push_back(style);
    } else {
      ReportError(doc, current.get(),
                  "Element '" + current->name +
                      "' is not allowed in 'styles'."
                      " Expected: DropShadowStyle, InnerShadowStyle,"
                      " BackgroundBlurStyle.");
    }
  }
}

static void ParseFilters(const DOMNode* node, Layer* layer, PAGXDocument* doc) {
  auto child = node->firstChild;
  while (child) {
    auto current = child;
    child = child->nextSibling;
    if (current->type != DOMNodeType::Element) {
      continue;
    }
    auto filter = ParseLayerFilter(current.get(), doc);
    if (filter) {
      layer->filters.push_back(filter);
    } else {
      ReportError(doc, current.get(),
                  "Element '" + current->name +
                      "' is not allowed in 'filters'."
                      " Expected: BlurFilter, DropShadowFilter,"
                      " InnerShadowFilter, BlendFilter, ColorMatrixFilter.");
    }
  }
}

static Element* ParseElement(const DOMNode* node, PAGXDocument* doc) {
  if (node->name == "Rectangle") {
    return ParseRectangle(node, doc);
  }
  if (node->name == "Ellipse") {
    return ParseEllipse(node, doc);
  }
  if (node->name == "Polystar") {
    return ParsePolystar(node, doc);
  }
  if (node->name == "Path") {
    return ParsePath(node, doc);
  }
  if (node->name == "Text") {
    return ParseText(node, doc);
  }
  if (node->name == "Fill") {
    return ParseFill(node, doc);
  }
  if (node->name == "Stroke") {
    return ParseStroke(node, doc);
  }
  if (node->name == "TrimPath") {
    return ParseTrimPath(node, doc);
  }
  if (node->name == "RoundCorner") {
    return ParseRoundCorner(node, doc);
  }
  if (node->name == "MergePath") {
    return ParseMergePath(node, doc);
  }
  if (node->name == "TextModifier") {
    return ParseTextModifier(node, doc);
  }
  if (node->name == "TextPath") {
    return ParseTextPath(node, doc);
  }
  if (node->name == "TextBox") {
    return ParseTextBox(node, doc);
  }
  if (node->name == "Repeater") {
    return ParseRepeater(node, doc);
  }
  if (node->name == "Group") {
    return ParseGroup(node, doc);
  }
  return nullptr;
}

static ColorSource* ParseColorSource(const DOMNode* node, PAGXDocument* doc) {
  if (node->name == "SolidColor") {
    return ParseSolidColor(node, doc);
  }
  if (node->name == "LinearGradient") {
    return ParseLinearGradient(node, doc);
  }
  if (node->name == "RadialGradient") {
    return ParseRadialGradient(node, doc);
  }
  if (node->name == "ConicGradient") {
    return ParseConicGradient(node, doc);
  }
  if (node->name == "DiamondGradient") {
    return ParseDiamondGradient(node, doc);
  }
  if (node->name == "ImagePattern") {
    return ParseImagePattern(node, doc);
  }
  return nullptr;
}

static LayerStyle* ParseLayerStyle(const DOMNode* node, PAGXDocument* doc) {
  if (node->name == "DropShadowStyle") {
    return ParseDropShadowStyle(node, doc);
  }
  if (node->name == "InnerShadowStyle") {
    return ParseInnerShadowStyle(node, doc);
  }
  if (node->name == "BackgroundBlurStyle") {
    return ParseBackgroundBlurStyle(node, doc);
  }
  return nullptr;
}

static LayerFilter* ParseLayerFilter(const DOMNode* node, PAGXDocument* doc) {
  if (node->name == "BlurFilter") {
    return ParseBlurFilter(node, doc);
  }
  if (node->name == "DropShadowFilter") {
    return ParseDropShadowFilter(node, doc);
  }
  if (node->name == "InnerShadowFilter") {
    return ParseInnerShadowFilter(node, doc);
  }
  if (node->name == "BlendFilter") {
    return ParseBlendFilter(node, doc);
  }
  if (node->name == "ColorMatrixFilter") {
    return ParseColorMatrixFilter(node, doc);
  }
  return nullptr;
}

//==============================================================================
// Geometry element parsing
//==============================================================================

static Rectangle* ParseRectangle(const DOMNode* node, PAGXDocument* doc) {
  auto rect = makeNodeFromXML<Rectangle>(node, doc);
  if (!rect) {
    return nullptr;
  }
  rect->size = GetSizeAttribute(node, "size", {0, 0}, doc);
  rect->position = GetPointAttribute(node, "position", Default<Rectangle>().position, doc);
  rect->roundness = GetFloatAttribute(node, "roundness", Default<Rectangle>().roundness, doc);
  rect->reversed = GetBoolAttribute(node, "reversed", Default<Rectangle>().reversed, doc);
  rect->left = GetFloatAttributeOrNaN(node, "left", doc);
  rect->right = GetFloatAttributeOrNaN(node, "right", doc);
  rect->top = GetFloatAttributeOrNaN(node, "top", doc);
  rect->bottom = GetFloatAttributeOrNaN(node, "bottom", doc);
  rect->centerX = GetFloatAttributeOrNaN(node, "centerX", doc);
  rect->centerY = GetFloatAttributeOrNaN(node, "centerY", doc);
  ReadDimension(node, "width", &rect->width, &rect->percentWidth, doc);
  ReadDimension(node, "height", &rect->height, &rect->percentHeight, doc);
  return rect;
}

static Ellipse* ParseEllipse(const DOMNode* node, PAGXDocument* doc) {
  auto ellipse = makeNodeFromXML<Ellipse>(node, doc);
  if (!ellipse) {
    return nullptr;
  }
  ellipse->size = GetSizeAttribute(node, "size", {0, 0}, doc);
  ellipse->position = GetPointAttribute(node, "position", Default<Ellipse>().position, doc);
  ellipse->reversed = GetBoolAttribute(node, "reversed", Default<Ellipse>().reversed, doc);
  ellipse->left = GetFloatAttributeOrNaN(node, "left", doc);
  ellipse->right = GetFloatAttributeOrNaN(node, "right", doc);
  ellipse->top = GetFloatAttributeOrNaN(node, "top", doc);
  ellipse->bottom = GetFloatAttributeOrNaN(node, "bottom", doc);
  ellipse->centerX = GetFloatAttributeOrNaN(node, "centerX", doc);
  ellipse->centerY = GetFloatAttributeOrNaN(node, "centerY", doc);
  ReadDimension(node, "width", &ellipse->width, &ellipse->percentWidth, doc);
  ReadDimension(node, "height", &ellipse->height, &ellipse->percentHeight, doc);
  return ellipse;
}

static Polystar* ParsePolystar(const DOMNode* node, PAGXDocument* doc) {
  auto polystar = makeNodeFromXML<Polystar>(node, doc);
  if (!polystar) {
    return nullptr;
  }
  polystar->type = GET_ENUM(node, "type", "star", doc, PolystarType);
  polystar->pointCount = GetFloatAttribute(node, "pointCount", Default<Polystar>().pointCount, doc);
  polystar->outerRadius =
      GetFloatAttribute(node, "outerRadius", Default<Polystar>().outerRadius, doc);
  polystar->innerRadius =
      GetFloatAttribute(node, "innerRadius", Default<Polystar>().innerRadius, doc);
  polystar->rotation = GetFloatAttribute(node, "rotation", Default<Polystar>().rotation, doc);
  polystar->outerRoundness =
      GetFloatAttribute(node, "outerRoundness", Default<Polystar>().outerRoundness, doc);
  polystar->innerRoundness =
      GetFloatAttribute(node, "innerRoundness", Default<Polystar>().innerRoundness, doc);
  polystar->reversed = GetBoolAttribute(node, "reversed", Default<Polystar>().reversed, doc);
  polystar->position = GetPointAttribute(node, "position", Default<Polystar>().position, doc);
  polystar->left = GetFloatAttributeOrNaN(node, "left", doc);
  polystar->right = GetFloatAttributeOrNaN(node, "right", doc);
  polystar->top = GetFloatAttributeOrNaN(node, "top", doc);
  polystar->bottom = GetFloatAttributeOrNaN(node, "bottom", doc);
  polystar->centerX = GetFloatAttributeOrNaN(node, "centerX", doc);
  polystar->centerY = GetFloatAttributeOrNaN(node, "centerY", doc);
  ReadDimension(node, "width", &polystar->width, &polystar->percentWidth, doc);
  ReadDimension(node, "height", &polystar->height, &polystar->percentHeight, doc);
  return polystar;
}

static Path* ParsePath(const DOMNode* node, PAGXDocument* doc) {
  auto path = makeNodeFromXML<Path>(node, doc);
  if (!path) {
    return nullptr;
  }
  auto dataAttr = GetAttribute(node, "data");
  if (!dataAttr.empty()) {
    if (dataAttr[0] == '@') {
      // Reference to PathData resource
      path->data = doc->findNode<PathData>(dataAttr.substr(1));
      if (!path->data) {
        ReportError(doc, node, "Resource '" + dataAttr + "' not found for 'data' attribute.");
      }
    } else {
      // Inline path data
      path->data = doc->makeNode<PathData>();
      path->data->sourceLine = node->line;
      *path->data = PathDataFromSVGString(dataAttr);
    }
  }
  path->reversed = GetBoolAttribute(node, "reversed", Default<Path>().reversed, doc);
  path->position = GetPointAttribute(node, "position", Default<Path>().position, doc);
  path->left = GetFloatAttributeOrNaN(node, "left", doc);
  path->right = GetFloatAttributeOrNaN(node, "right", doc);
  path->top = GetFloatAttributeOrNaN(node, "top", doc);
  path->bottom = GetFloatAttributeOrNaN(node, "bottom", doc);
  path->centerX = GetFloatAttributeOrNaN(node, "centerX", doc);
  path->centerY = GetFloatAttributeOrNaN(node, "centerY", doc);
  ReadDimension(node, "width", &path->width, &path->percentWidth, doc);
  ReadDimension(node, "height", &path->height, &path->percentHeight, doc);
  return path;
}

static Text* ParseText(const DOMNode* node, PAGXDocument* doc) {
  auto text = makeNodeFromXML<Text>(node, doc);
  if (!text) {
    return nullptr;
  }
  // Parse text content from attribute first, then fallback to text child node
  auto textAttr = GetAttribute(node, "text");
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
  text->position = GetPointAttribute(node, "position", Default<Text>().position, doc);
  text->fontFamily = GetAttribute(node, "fontFamily");
  text->fontStyle = GetAttribute(node, "fontStyle");
  text->fontSize = GetFloatAttribute(node, "fontSize", Default<Text>().fontSize, doc);
  text->letterSpacing =
      GetFloatAttribute(node, "letterSpacing", Default<Text>().letterSpacing, doc);
  text->fauxBold = GetBoolAttribute(node, "fauxBold", Default<Text>().fauxBold, doc);
  text->fauxItalic = GetBoolAttribute(node, "fauxItalic", Default<Text>().fauxItalic, doc);
  text->textAnchor = GET_ENUM(node, "textAnchor", "start", doc, TextAnchor);
  text->baseline = GET_ENUM(node, "baseline", "lineBox", doc, TextBaseline);

  // Parse GlyphRun children for precomposition mode
  auto child = node->firstChild;
  while (child) {
    if (child->type == DOMNodeType::Element) {
      if (child->name == "GlyphRun") {
        auto glyphRun = ParseGlyphRun(child.get(), doc);
        if (glyphRun) {
          text->glyphRuns.push_back(glyphRun);
        }
      } else {
        ReportError(doc, child.get(),
                    "Element '" + child->name + "' is not allowed in 'Text'. Expected: GlyphRun.");
      }
    }
    child = child->nextSibling;
  }

  text->left = GetFloatAttributeOrNaN(node, "left", doc);
  text->right = GetFloatAttributeOrNaN(node, "right", doc);
  text->top = GetFloatAttributeOrNaN(node, "top", doc);
  text->bottom = GetFloatAttributeOrNaN(node, "bottom", doc);
  text->centerX = GetFloatAttributeOrNaN(node, "centerX", doc);
  text->centerY = GetFloatAttributeOrNaN(node, "centerY", doc);
  ReadDimension(node, "width", &text->width, &text->percentWidth, doc);
  ReadDimension(node, "height", &text->height, &text->percentHeight, doc);
  return text;
}

//==============================================================================
// Painter parsing
//==============================================================================

static ColorSource* ParseColorAttr(const std::string& colorAttr, PAGXDocument* doc,
                                   const DOMNode* node) {
  if (colorAttr.empty()) {
    return nullptr;
  }
  if (colorAttr[0] == '@') {
    auto result = doc->findNode<ColorSource>(colorAttr.substr(1));
    if (!result) {
      ReportError(doc, node, "Resource '" + colorAttr + "' not found for 'color' attribute.");
    }
    return result;
  }
  auto solidColor = doc->makeNode<SolidColor>();
  solidColor->color = GetColorAttribute(node, "color", doc);
  return solidColor;
}

static ColorSource* ParseChildColorSource(const DOMNode* node, PAGXDocument* doc) {
  ColorSource* result = nullptr;
  auto child = node->firstChild;
  while (child) {
    if (child->type == DOMNodeType::Element) {
      auto colorSource = ParseColorSource(child.get(), doc);
      if (colorSource) {
        result = colorSource;
      } else {
        ReportError(doc, child.get(),
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

static Fill* ParseFill(const DOMNode* node, PAGXDocument* doc) {
  auto fill = makeNodeFromXML<Fill>(node, doc);
  if (!fill) {
    return nullptr;
  }
  fill->color = ParseColorAttr(GetAttribute(node, "color"), doc, node);
  fill->alpha = GetFloatAttribute(node, "alpha", Default<Fill>().alpha, doc);
  fill->blendMode = GET_ENUM(node, "blendMode", "normal", doc, BlendMode);
  fill->fillRule = GET_ENUM(node, "fillRule", "winding", doc, FillRule);
  fill->placement = GET_ENUM(node, "placement", "background", doc, LayerPlacement);
  auto childColor = ParseChildColorSource(node, doc);
  if (childColor) {
    fill->color = childColor;
  }
  return fill;
}

static Stroke* ParseStroke(const DOMNode* node, PAGXDocument* doc) {
  auto stroke = makeNodeFromXML<Stroke>(node, doc);
  if (!stroke) {
    return nullptr;
  }
  stroke->color = ParseColorAttr(GetAttribute(node, "color"), doc, node);
  stroke->width = GetFloatAttribute(node, "width", Default<Stroke>().width, doc);
  stroke->alpha = GetFloatAttribute(node, "alpha", Default<Stroke>().alpha, doc);
  stroke->blendMode = GET_ENUM(node, "blendMode", "normal", doc, BlendMode);
  stroke->cap = GET_ENUM(node, "cap", "butt", doc, LineCap);
  stroke->join = GET_ENUM(node, "join", "miter", doc, LineJoin);
  stroke->miterLimit = GetFloatAttribute(node, "miterLimit", Default<Stroke>().miterLimit, doc);
  auto dashesStr = GetAttribute(node, "dashes");
  if (!dashesStr.empty()) {
    stroke->dashes = ParseFloatList(dashesStr);
  }
  stroke->dashOffset = GetFloatAttribute(node, "dashOffset", Default<Stroke>().dashOffset, doc);
  stroke->dashAdaptive =
      GetBoolAttribute(node, "dashAdaptive", Default<Stroke>().dashAdaptive, doc);
  stroke->align = GET_ENUM(node, "align", "center", doc, StrokeAlign);
  stroke->placement = GET_ENUM(node, "placement", "background", doc, LayerPlacement);
  auto childColor = ParseChildColorSource(node, doc);
  if (childColor) {
    stroke->color = childColor;
  }
  return stroke;
}

//==============================================================================
// Modifier parsing
//==============================================================================

static TrimPath* ParseTrimPath(const DOMNode* node, PAGXDocument* doc) {
  auto trim = makeNodeFromXML<TrimPath>(node, doc);
  if (!trim) {
    return nullptr;
  }
  trim->start = GetFloatAttribute(node, "start", Default<TrimPath>().start, doc);
  trim->end = GetFloatAttribute(node, "end", Default<TrimPath>().end, doc);
  trim->offset = GetFloatAttribute(node, "offset", Default<TrimPath>().offset, doc);
  trim->type = GET_ENUM(node, "type", "separate", doc, TrimType);
  return trim;
}

static RoundCorner* ParseRoundCorner(const DOMNode* node, PAGXDocument* doc) {
  auto round = makeNodeFromXML<RoundCorner>(node, doc);
  if (!round) {
    return nullptr;
  }
  round->radius = GetFloatAttribute(node, "radius", Default<RoundCorner>().radius, doc);
  return round;
}

static MergePath* ParseMergePath(const DOMNode* node, PAGXDocument* doc) {
  auto merge = makeNodeFromXML<MergePath>(node, doc);
  if (!merge) {
    return nullptr;
  }
  merge->mode = GET_ENUM(node, "mode", "append", doc, MergePathMode);
  return merge;
}

static TextModifier* ParseTextModifier(const DOMNode* node, PAGXDocument* doc) {
  auto modifier = makeNodeFromXML<TextModifier>(node, doc);
  if (!modifier) {
    return nullptr;
  }
  modifier->anchor = GetPointAttribute(node, "anchor", Default<TextModifier>().anchor, doc);
  modifier->position = GetPointAttribute(node, "position", Default<TextModifier>().position, doc);
  modifier->rotation = GetFloatAttribute(node, "rotation", Default<TextModifier>().rotation, doc);
  modifier->scale = GetPointAttribute(node, "scale", Default<TextModifier>().scale, doc);
  modifier->skew = GetFloatAttribute(node, "skew", Default<TextModifier>().skew, doc);
  modifier->skewAxis = GetFloatAttribute(node, "skewAxis", Default<TextModifier>().skewAxis, doc);
  modifier->alpha = GetFloatAttribute(node, "alpha", Default<TextModifier>().alpha, doc);
  auto fillColorAttr = GetAttribute(node, "fillColor");
  if (!fillColorAttr.empty()) {
    modifier->fillColor = GetColorAttribute(node, "fillColor", doc);
  }
  auto strokeColorAttr = GetAttribute(node, "strokeColor");
  if (!strokeColorAttr.empty()) {
    modifier->strokeColor = GetColorAttribute(node, "strokeColor", doc);
  }
  auto strokeWidthAttr = GetAttribute(node, "strokeWidth");
  if (!strokeWidthAttr.empty()) {
    modifier->strokeWidth = GetFloatAttribute(node, "strokeWidth", 0, doc);
  }

  auto child = node->firstChild;
  while (child) {
    if (child->type == DOMNodeType::Element) {
      if (child->name == "RangeSelector") {
        auto selector = ParseRangeSelector(child.get(), doc);
        if (selector) {
          modifier->selectors.push_back(selector);
        }
      } else {
        ReportError(doc, child.get(),
                    "Element '" + child->name +
                        "' is not allowed in 'TextModifier'."
                        " Expected: RangeSelector.");
      }
    }
    child = child->nextSibling;
  }

  return modifier;
}

static TextPath* ParseTextPath(const DOMNode* node, PAGXDocument* doc) {
  auto textPath = makeNodeFromXML<TextPath>(node, doc);
  if (!textPath) {
    return nullptr;
  }
  auto pathAttr = GetAttribute(node, "path");
  if (!pathAttr.empty()) {
    if (pathAttr[0] == '@') {
      // Reference to PathData resource
      textPath->path = doc->findNode<PathData>(pathAttr.substr(1));
      if (!textPath->path) {
        ReportError(doc, node, "Resource '" + pathAttr + "' not found for 'path' attribute.");
      }
    } else {
      // Inline path data
      textPath->path = doc->makeNode<PathData>();
      textPath->path->sourceLine = node->line;
      *textPath->path = PathDataFromSVGString(pathAttr);
    }
  }
  textPath->baselineOrigin =
      GetPointAttribute(node, "baselineOrigin", Default<TextPath>().baselineOrigin, doc);
  textPath->baselineAngle =
      GetFloatAttribute(node, "baselineAngle", Default<TextPath>().baselineAngle, doc);
  textPath->firstMargin =
      GetFloatAttribute(node, "firstMargin", Default<TextPath>().firstMargin, doc);
  textPath->lastMargin = GetFloatAttribute(node, "lastMargin", Default<TextPath>().lastMargin, doc);
  textPath->perpendicular =
      GetBoolAttribute(node, "perpendicular", Default<TextPath>().perpendicular, doc);
  textPath->reversed = GetBoolAttribute(node, "reversed", Default<TextPath>().reversed, doc);
  textPath->forceAlignment =
      GetBoolAttribute(node, "forceAlignment", Default<TextPath>().forceAlignment, doc);
  textPath->left = GetFloatAttributeOrNaN(node, "left", doc);
  textPath->right = GetFloatAttributeOrNaN(node, "right", doc);
  textPath->top = GetFloatAttributeOrNaN(node, "top", doc);
  textPath->bottom = GetFloatAttributeOrNaN(node, "bottom", doc);
  textPath->centerX = GetFloatAttributeOrNaN(node, "centerX", doc);
  textPath->centerY = GetFloatAttributeOrNaN(node, "centerY", doc);
  ReadDimension(node, "width", &textPath->width, &textPath->percentWidth, doc);
  ReadDimension(node, "height", &textPath->height, &textPath->percentHeight, doc);
  return textPath;
}

static TextBox* ParseTextBox(const DOMNode* node, PAGXDocument* doc) {
  auto textBox = makeNodeFromXML<TextBox>(node, doc);
  if (!textBox) {
    return nullptr;
  }
  // Group properties
  textBox->anchor = GetPointAttribute(node, "anchor", Default<TextBox>().anchor, doc);
  textBox->position = GetPointAttribute(node, "position", Default<TextBox>().position, doc);
  textBox->rotation = GetFloatAttribute(node, "rotation", Default<TextBox>().rotation, doc);
  textBox->scale = GetPointAttribute(node, "scale", Default<TextBox>().scale, doc);
  textBox->skew = GetFloatAttribute(node, "skew", Default<TextBox>().skew, doc);
  textBox->skewAxis = GetFloatAttribute(node, "skewAxis", Default<TextBox>().skewAxis, doc);
  textBox->alpha = GetFloatAttribute(node, "alpha", Default<TextBox>().alpha, doc);
  ReadDimension(node, "width", &textBox->width, &textBox->percentWidth, doc);
  ReadDimension(node, "height", &textBox->height, &textBox->percentHeight, doc);
  textBox->padding = GetPaddingAttribute(node, doc);
  // TextBox typography properties
  textBox->textAlign = GET_ENUM(node, "textAlign", "start", doc, TextAlign);
  textBox->paragraphAlign = GET_ENUM(node, "paragraphAlign", "near", doc, ParagraphAlign);
  textBox->writingMode = GET_ENUM(node, "writingMode", "horizontal", doc, WritingMode);
  textBox->lineHeight = GetFloatAttribute(node, "lineHeight", Default<TextBox>().lineHeight, doc);
  textBox->wordWrap = GetBoolAttribute(node, "wordWrap", Default<TextBox>().wordWrap, doc);
  textBox->overflow = GET_ENUM(node, "overflow", "visible", doc, Overflow);
  // Constraint properties
  textBox->left = GetFloatAttributeOrNaN(node, "left", doc);
  textBox->right = GetFloatAttributeOrNaN(node, "right", doc);
  textBox->top = GetFloatAttributeOrNaN(node, "top", doc);
  textBox->bottom = GetFloatAttributeOrNaN(node, "bottom", doc);
  textBox->centerX = GetFloatAttributeOrNaN(node, "centerX", doc);
  textBox->centerY = GetFloatAttributeOrNaN(node, "centerY", doc);
  // Child elements
  auto child = node->firstChild;
  while (child) {
    if (child->type == DOMNodeType::Element) {
      auto element = ParseElement(child.get(), doc);
      if (element) {
        textBox->elements.push_back(element);
      } else {
        ReportError(doc, child.get(),
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

static Repeater* ParseRepeater(const DOMNode* node, PAGXDocument* doc) {
  auto repeater = makeNodeFromXML<Repeater>(node, doc);
  if (!repeater) {
    return nullptr;
  }
  repeater->copies = GetFloatAttribute(node, "copies", Default<Repeater>().copies, doc);
  repeater->offset = GetFloatAttribute(node, "offset", Default<Repeater>().offset, doc);
  repeater->order = GET_ENUM(node, "order", "belowOriginal", doc, RepeaterOrder);
  repeater->anchor = GetPointAttribute(node, "anchor", Default<Repeater>().anchor, doc);
  repeater->position = GetPointAttribute(node, "position", Default<Repeater>().position, doc);
  repeater->rotation = GetFloatAttribute(node, "rotation", Default<Repeater>().rotation, doc);
  repeater->scale = GetPointAttribute(node, "scale", Default<Repeater>().scale, doc);
  repeater->startAlpha = GetFloatAttribute(node, "startAlpha", Default<Repeater>().startAlpha, doc);
  repeater->endAlpha = GetFloatAttribute(node, "endAlpha", Default<Repeater>().endAlpha, doc);
  return repeater;
}

static Group* ParseGroup(const DOMNode* node, PAGXDocument* doc) {
  auto group = makeNodeFromXML<Group>(node, doc);
  if (!group) {
    return nullptr;
  }
  group->anchor = GetPointAttribute(node, "anchor", Default<Group>().anchor, doc);
  group->position = GetPointAttribute(node, "position", Default<Group>().position, doc);
  group->rotation = GetFloatAttribute(node, "rotation", Default<Group>().rotation, doc);
  group->scale = GetPointAttribute(node, "scale", Default<Group>().scale, doc);
  group->skew = GetFloatAttribute(node, "skew", Default<Group>().skew, doc);
  group->skewAxis = GetFloatAttribute(node, "skewAxis", Default<Group>().skewAxis, doc);
  group->alpha = GetFloatAttribute(node, "alpha", Default<Group>().alpha, doc);
  ReadDimension(node, "width", &group->width, &group->percentWidth, doc);
  ReadDimension(node, "height", &group->height, &group->percentHeight, doc);
  group->padding = GetPaddingAttribute(node, doc);
  group->left = GetFloatAttributeOrNaN(node, "left", doc);
  group->right = GetFloatAttributeOrNaN(node, "right", doc);
  group->top = GetFloatAttributeOrNaN(node, "top", doc);
  group->bottom = GetFloatAttributeOrNaN(node, "bottom", doc);
  group->centerX = GetFloatAttributeOrNaN(node, "centerX", doc);
  group->centerY = GetFloatAttributeOrNaN(node, "centerY", doc);

  auto child = node->firstChild;
  while (child) {
    if (child->type == DOMNodeType::Element) {
      auto element = ParseElement(child.get(), doc);
      if (element) {
        group->elements.push_back(element);
      } else {
        ReportError(doc, child.get(),
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

static RangeSelector* ParseRangeSelector(const DOMNode* node, PAGXDocument* doc) {
  auto selector = makeNodeFromXML<RangeSelector>(node, doc);
  if (!selector) {
    return nullptr;
  }
  selector->start = GetFloatAttribute(node, "start", Default<RangeSelector>().start, doc);
  selector->end = GetFloatAttribute(node, "end", Default<RangeSelector>().end, doc);
  selector->offset = GetFloatAttribute(node, "offset", Default<RangeSelector>().offset, doc);
  selector->unit = GET_ENUM(node, "unit", "percentage", doc, SelectorUnit);
  selector->shape = GET_ENUM(node, "shape", "square", doc, SelectorShape);
  selector->easeIn = GetFloatAttribute(node, "easeIn", Default<RangeSelector>().easeIn, doc);
  selector->easeOut = GetFloatAttribute(node, "easeOut", Default<RangeSelector>().easeOut, doc);
  selector->mode = GET_ENUM(node, "mode", "add", doc, SelectorMode);
  selector->weight = GetFloatAttribute(node, "weight", Default<RangeSelector>().weight, doc);
  selector->randomOrder =
      GetBoolAttribute(node, "randomOrder", Default<RangeSelector>().randomOrder, doc);
  selector->randomSeed =
      GetIntAttribute(node, "randomSeed", Default<RangeSelector>().randomSeed, doc);
  return selector;
}

//==============================================================================
// Color source parsing
//==============================================================================

static SolidColor* ParseSolidColor(const DOMNode* node, PAGXDocument* doc) {
  auto solid = makeNodeFromXML<SolidColor>(node, doc);
  if (!solid) {
    return nullptr;
  }
  auto colorStr = GetAttribute(node, "color");
  if (!colorStr.empty()) {
    solid->color = GetColorAttribute(node, "color", doc);
  } else {
    // Fallback to individual red/green/blue/alpha attributes
    solid->color.red = GetFloatAttribute(node, "red", 0, doc);
    solid->color.green = GetFloatAttribute(node, "green", 0, doc);
    solid->color.blue = GetFloatAttribute(node, "blue", 0, doc);
    solid->color.alpha = GetFloatAttribute(node, "alpha", 1, doc);
    solid->color.colorSpace = GET_ENUM(node, "colorSpace", "sRGB", doc, ColorSpace);
  }
  return solid;
}

static void ParseGradientCommon(const DOMNode* node, PAGXDocument* doc, Gradient* gradient) {
  auto matrixStr = GetAttribute(node, "matrix");
  if (!matrixStr.empty()) {
    if (ParseFloatList(matrixStr).size() < 6) {
      ReportError(doc, node, "Invalid value '" + matrixStr + "' for 'matrix' attribute.");
    } else {
      gradient->matrix = MatrixFromString(matrixStr);
    }
  }
  gradient->fitsToGeometry =
      GetBoolAttribute(node, "fitsToGeometry", Default<LinearGradient>().fitsToGeometry, doc);
  auto child = node->firstChild;
  while (child) {
    if (child->type == DOMNodeType::Element && child->name == "ColorStop") {
      auto stop = ParseColorStop(child.get(), doc);
      if (stop) {
        gradient->colorStops.push_back(stop);
      }
    } else if (child->type == DOMNodeType::Element) {
      ReportError(doc, child.get(),
                  "Element '" + child->name + "' is not allowed in '" + node->name +
                      "'. Expected: ColorStop.");
    }
    child = child->nextSibling;
  }
}

static LinearGradient* ParseLinearGradient(const DOMNode* node, PAGXDocument* doc) {
  auto gradient = makeNodeFromXML<LinearGradient>(node, doc);
  if (!gradient) {
    return nullptr;
  }
  gradient->startPoint =
      GetPointAttribute(node, "startPoint", Default<LinearGradient>().startPoint, doc);
  gradient->endPoint = GetPointAttribute(node, "endPoint", Default<LinearGradient>().endPoint, doc);
  ParseGradientCommon(node, doc, gradient);
  return gradient;
}

static RadialGradient* ParseRadialGradient(const DOMNode* node, PAGXDocument* doc) {
  auto gradient = makeNodeFromXML<RadialGradient>(node, doc);
  if (!gradient) {
    return nullptr;
  }
  gradient->center = GetPointAttribute(node, "center", Default<RadialGradient>().center, doc);
  gradient->radius = GetFloatAttribute(node, "radius", Default<RadialGradient>().radius, doc);
  ParseGradientCommon(node, doc, gradient);
  return gradient;
}

static ConicGradient* ParseConicGradient(const DOMNode* node, PAGXDocument* doc) {
  auto gradient = makeNodeFromXML<ConicGradient>(node, doc);
  if (!gradient) {
    return nullptr;
  }
  gradient->center = GetPointAttribute(node, "center", Default<ConicGradient>().center, doc);
  gradient->startAngle =
      GetFloatAttribute(node, "startAngle", Default<ConicGradient>().startAngle, doc);
  gradient->endAngle = GetFloatAttribute(node, "endAngle", Default<ConicGradient>().endAngle, doc);
  ParseGradientCommon(node, doc, gradient);
  return gradient;
}

static DiamondGradient* ParseDiamondGradient(const DOMNode* node, PAGXDocument* doc) {
  auto gradient = makeNodeFromXML<DiamondGradient>(node, doc);
  if (!gradient) {
    return nullptr;
  }
  gradient->center = GetPointAttribute(node, "center", Default<DiamondGradient>().center, doc);
  gradient->radius = GetFloatAttribute(node, "radius", Default<DiamondGradient>().radius, doc);
  ParseGradientCommon(node, doc, gradient);
  return gradient;
}

static ImagePattern* ParseImagePattern(const DOMNode* node, PAGXDocument* doc) {
  auto pattern = makeNodeFromXML<ImagePattern>(node, doc);
  if (!pattern) {
    return nullptr;
  }
  auto imageAttr = GetAttribute(node, "image");
  if (!imageAttr.empty()) {
    if (imageAttr[0] == '@') {
      pattern->image = doc->findNode<Image>(imageAttr.substr(1));
      if (!pattern->image) {
        ReportError(doc, node, "Resource '" + imageAttr + "' not found for 'image' attribute.");
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
  pattern->tileModeX = GET_ENUM(node, "tileModeX", "decal", doc, TileMode);
  pattern->tileModeY = GET_ENUM(node, "tileModeY", "decal", doc, TileMode);
  pattern->filterMode = GET_ENUM(node, "filterMode", "linear", doc, FilterMode);
  pattern->mipmapMode = GET_ENUM(node, "mipmapMode", "linear", doc, MipmapMode);
  pattern->scaleMode = GET_ENUM(node, "scaleMode", "letterBox", doc, ScaleMode);
  auto matrixStr = GetAttribute(node, "matrix");
  if (!matrixStr.empty()) {
    if (ParseFloatList(matrixStr).size() < 6) {
      ReportError(doc, node, "Invalid value '" + matrixStr + "' for 'matrix' attribute.");
    } else {
      pattern->matrix = MatrixFromString(matrixStr);
    }
  }
  return pattern;
}

static ColorStop* ParseColorStop(const DOMNode* node, PAGXDocument* doc) {
  auto stop = makeNodeFromXML<ColorStop>(node, doc);
  stop->offset = GetFloatAttribute(node, "offset", Default<ColorStop>().offset, doc);
  auto colorStr = GetAttribute(node, "color");
  if (!colorStr.empty()) {
    stop->color = GetColorAttribute(node, "color", doc);
  }
  return stop;
}

//==============================================================================
// Resource parsing
//==============================================================================

static Image* ParseImage(const DOMNode* node, PAGXDocument* doc) {
  auto image = makeNodeFromXML<Image>(node, doc);
  if (!image) {
    return nullptr;
  }
  auto source = GetAttribute(node, "source");
  auto data = DecodeBase64DataURI(source);
  if (data) {
    image->data = data;
  } else {
    image->filePath = source;
  }
  return image;
}

static PathData* ParsePathData(const DOMNode* node, PAGXDocument* doc) {
  auto pathData = makeNodeFromXML<PathData>(node, doc);
  if (!pathData) {
    return nullptr;
  }
  auto data = GetAttribute(node, "data");
  if (!data.empty()) {
    *pathData = PathDataFromSVGString(data);
  }
  return pathData;
}

static Composition* ParseComposition(const DOMNode* node, PAGXDocument* doc) {
  auto comp = makeNodeFromXML<Composition>(node, doc);
  if (!comp) {
    return nullptr;
  }
  comp->width = GetFloatAttribute(node, "width", Default<Composition>().width, doc);
  comp->height = GetFloatAttribute(node, "height", Default<Composition>().height, doc);
  auto child = node->firstChild;
  while (child) {
    if (child->type == DOMNodeType::Element) {
      if (child->name == "Layer") {
        auto layer = ParseLayer(child.get(), doc);
        if (layer) {
          comp->layers.push_back(layer);
        }
      } else {
        ReportError(
            doc, child.get(),
            "Element '" + child->name + "' is not allowed in 'Composition'. Expected: Layer.");
      }
    }
    child = child->nextSibling;
  }
  return comp;
}

static Font* ParseFont(const DOMNode* node, PAGXDocument* doc) {
  auto font = makeNodeFromXML<Font>(node, doc);
  if (!font) {
    return nullptr;
  }
  font->unitsPerEm = GetIntAttribute(node, "unitsPerEm", Default<Font>().unitsPerEm, doc);
  auto child = node->firstChild;
  while (child) {
    if (child->type == DOMNodeType::Element) {
      if (child->name == "Glyph") {
        auto glyph = ParseGlyph(child.get(), doc);
        if (glyph) {
          font->glyphs.push_back(glyph);
        }
      } else {
        ReportError(doc, child.get(),
                    "Element '" + child->name + "' is not allowed in 'Font'. Expected: Glyph.");
      }
    }
    child = child->nextSibling;
  }
  return font;
}

static Glyph* ParseGlyph(const DOMNode* node, PAGXDocument* doc) {
  auto glyph = makeNodeFromXML<Glyph>(node, doc);
  if (!glyph) {
    return nullptr;
  }
  auto pathAttr = GetAttribute(node, "path");
  if (!pathAttr.empty()) {
    if (pathAttr[0] == '@') {
      glyph->path = doc->findNode<PathData>(pathAttr.substr(1));
      if (!glyph->path) {
        ReportError(doc, node,
                    "Resource '@" + pathAttr.substr(1) + "' not found for 'path' attribute.");
      }
    } else {
      glyph->path = doc->makeNode<PathData>();
      glyph->path->sourceLine = node->line;
      *glyph->path = PathDataFromSVGString(pathAttr);
    }
  }
  auto imageAttr = GetAttribute(node, "image");
  if (!imageAttr.empty()) {
    if (imageAttr[0] == '@') {
      // Reference to existing Image resource
      glyph->image = doc->findNode<Image>(imageAttr.substr(1));
      if (!glyph->image) {
        ReportError(doc, node,
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
  auto offsetStr = GetAttribute(node, "offset");
  if (!offsetStr.empty()) {
    glyph->offset = GetPointAttribute(node, "offset", Default<Glyph>().offset, doc);
  }
  glyph->advance = GetFloatAttribute(node, "advance", Default<Glyph>().advance, doc);
  return glyph;
}

static std::vector<Point> ParseSemicolonSeparatedPoints(const std::string& str) {
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

static GlyphRun* ParseGlyphRun(const DOMNode* node, PAGXDocument* doc) {
  auto run = makeNodeFromXML<GlyphRun>(node, doc);
  if (!run) {
    return nullptr;
  }
  auto fontAttr = GetAttribute(node, "font");
  if (!fontAttr.empty() && fontAttr[0] == '@') {
    auto fontId = fontAttr.substr(1);
    run->font = doc->findNode<Font>(fontId);
    if (!run->font) {
      ReportError(doc, node, "Resource '" + fontAttr + "' not found for 'font' attribute.");
    }
  }
  run->fontSize = GetFloatAttribute(node, "fontSize", Default<GlyphRun>().fontSize, doc);
  run->x = GetFloatAttribute(node, "x", Default<GlyphRun>().x, doc);
  run->y = GetFloatAttribute(node, "y", Default<GlyphRun>().y, doc);

  // Parse glyphs regardless of whether font is valid, to maintain data consistency.
  auto glyphsStr = GetAttribute(node, "glyphs");
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
        ReportError(doc, node,
                    "Invalid 'glyphs' value: parse error at position " +
                        std::to_string(ptr - glyphsStr.c_str()) + ".");
        break;
      }
      if (value < 0 || value > 65535) {
        ReportError(doc, node,
                    "Invalid 'glyphs' value: glyph index " + std::to_string(value) +
                        " out of range [0, 65535].");
        break;
      }
      run->glyphs.push_back(static_cast<uint16_t>(value));
      ptr = endPtr;
    }
  }

  // Parse xOffsets (comma-separated x offsets)
  auto xOffsetsStr = GetAttribute(node, "xOffsets");
  if (!xOffsetsStr.empty()) {
    run->xOffsets = ParseFloatList(xOffsetsStr);
  }

  // Parse positions
  auto posStr = GetAttribute(node, "positions");
  if (!posStr.empty()) {
    run->positions = ParseSemicolonSeparatedPoints(posStr);
  }

  // Parse anchors
  auto anchorsStr = GetAttribute(node, "anchors");
  if (!anchorsStr.empty()) {
    run->anchors = ParseSemicolonSeparatedPoints(anchorsStr);
  }

  // Parse scales
  auto scalesStr = GetAttribute(node, "scales");
  if (!scalesStr.empty()) {
    run->scales = ParseSemicolonSeparatedPoints(scalesStr);
  }

  // Parse rotations (comma-separated angles in degrees)
  auto rotationsStr = GetAttribute(node, "rotations");
  if (!rotationsStr.empty()) {
    run->rotations = ParseFloatList(rotationsStr);
  }

  // Parse skews (comma-separated angles in degrees)
  auto skewsStr = GetAttribute(node, "skews");
  if (!skewsStr.empty()) {
    run->skews = ParseFloatList(skewsStr);
  }

  // Parse linebox bounds (x,y,w,h)
  auto boundsStr = GetAttribute(node, "bounds");
  if (!boundsStr.empty()) {
    auto values = ParseFloatList(boundsStr);
    if (values.size() >= 4) {
      run->bounds = Rect::MakeXYWH(values[0], values[1], values[2], values[3]);
    } else {
      ReportError(doc, node, "Invalid 'bounds' value (expected 4 numbers).");
    }
  }

  return run;
}

//==============================================================================
// Layer style parsing
//==============================================================================

template <typename T>
static void ParseShadowAttributes(const DOMNode* node, PAGXDocument* doc, float& offsetX,
                                  float& offsetY, float& blurX, float& blurY, Color& color) {
  offsetX = GetFloatAttribute(node, "offsetX", Default<T>().offsetX, doc);
  offsetY = GetFloatAttribute(node, "offsetY", Default<T>().offsetY, doc);
  blurX = GetFloatAttribute(node, "blurX", Default<T>().blurX, doc);
  blurY = GetFloatAttribute(node, "blurY", Default<T>().blurY, doc);
  auto colorStr = GetAttribute(node, "color");
  if (!colorStr.empty()) {
    color = GetColorAttribute(node, "color", doc);
  }
}

static DropShadowStyle* ParseDropShadowStyle(const DOMNode* node, PAGXDocument* doc) {
  auto style = makeNodeFromXML<DropShadowStyle>(node, doc);
  if (!style) {
    return nullptr;
  }
  style->blendMode = GET_ENUM(node, "blendMode", "normal", doc, BlendMode);
  style->excludeChildEffects = GetBoolAttribute(
      node, "excludeChildEffects", Default<DropShadowStyle>().excludeChildEffects, doc);
  ParseShadowAttributes<DropShadowStyle>(node, doc, style->offsetX, style->offsetY, style->blurX,
                                         style->blurY, style->color);
  style->showBehindLayer =
      GetBoolAttribute(node, "showBehindLayer", Default<DropShadowStyle>().showBehindLayer, doc);
  return style;
}

static InnerShadowStyle* ParseInnerShadowStyle(const DOMNode* node, PAGXDocument* doc) {
  auto style = makeNodeFromXML<InnerShadowStyle>(node, doc);
  if (!style) {
    return nullptr;
  }
  style->blendMode = GET_ENUM(node, "blendMode", "normal", doc, BlendMode);
  style->excludeChildEffects = GetBoolAttribute(
      node, "excludeChildEffects", Default<InnerShadowStyle>().excludeChildEffects, doc);
  ParseShadowAttributes<InnerShadowStyle>(node, doc, style->offsetX, style->offsetY, style->blurX,
                                          style->blurY, style->color);
  return style;
}

static BackgroundBlurStyle* ParseBackgroundBlurStyle(const DOMNode* node, PAGXDocument* doc) {
  auto style = makeNodeFromXML<BackgroundBlurStyle>(node, doc);
  if (!style) {
    return nullptr;
  }
  style->blendMode = GET_ENUM(node, "blendMode", "normal", doc, BlendMode);
  style->excludeChildEffects = GetBoolAttribute(
      node, "excludeChildEffects", Default<BackgroundBlurStyle>().excludeChildEffects, doc);
  style->blurX = GetFloatAttribute(node, "blurX", Default<BackgroundBlurStyle>().blurX, doc);
  style->blurY = GetFloatAttribute(node, "blurY", Default<BackgroundBlurStyle>().blurY, doc);
  style->tileMode = GET_ENUM(node, "tileMode", "mirror", doc, TileMode);
  return style;
}

//==============================================================================
// Layer filter parsing
//==============================================================================

static BlurFilter* ParseBlurFilter(const DOMNode* node, PAGXDocument* doc) {
  auto filter = makeNodeFromXML<BlurFilter>(node, doc);
  if (!filter) {
    return nullptr;
  }
  filter->blurX = GetFloatAttribute(node, "blurX", Default<BlurFilter>().blurX, doc);
  filter->blurY = GetFloatAttribute(node, "blurY", Default<BlurFilter>().blurY, doc);
  filter->tileMode = GET_ENUM(node, "tileMode", "decal", doc, TileMode);
  return filter;
}

static DropShadowFilter* ParseDropShadowFilter(const DOMNode* node, PAGXDocument* doc) {
  auto filter = makeNodeFromXML<DropShadowFilter>(node, doc);
  if (!filter) {
    return nullptr;
  }
  ParseShadowAttributes<DropShadowFilter>(node, doc, filter->offsetX, filter->offsetY,
                                          filter->blurX, filter->blurY, filter->color);
  filter->shadowOnly =
      GetBoolAttribute(node, "shadowOnly", Default<DropShadowFilter>().shadowOnly, doc);
  return filter;
}

static InnerShadowFilter* ParseInnerShadowFilter(const DOMNode* node, PAGXDocument* doc) {
  auto filter = makeNodeFromXML<InnerShadowFilter>(node, doc);
  if (!filter) {
    return nullptr;
  }
  ParseShadowAttributes<InnerShadowFilter>(node, doc, filter->offsetX, filter->offsetY,
                                           filter->blurX, filter->blurY, filter->color);
  filter->shadowOnly =
      GetBoolAttribute(node, "shadowOnly", Default<InnerShadowFilter>().shadowOnly, doc);
  return filter;
}

static BlendFilter* ParseBlendFilter(const DOMNode* node, PAGXDocument* doc) {
  auto filter = makeNodeFromXML<BlendFilter>(node, doc);
  if (!filter) {
    return nullptr;
  }
  auto colorStr = GetAttribute(node, "color");
  if (!colorStr.empty()) {
    filter->color = GetColorAttribute(node, "color", doc);
  }
  filter->blendMode = GET_ENUM(node, "blendMode", "normal", doc, BlendMode);
  return filter;
}

static ColorMatrixFilter* ParseColorMatrixFilter(const DOMNode* node, PAGXDocument* doc) {
  auto filter = makeNodeFromXML<ColorMatrixFilter>(node, doc);
  if (!filter) {
    return nullptr;
  }
  auto matrixStr = GetAttribute(node, "matrix");
  if (!matrixStr.empty()) {
    auto values = ParseFloatList(matrixStr);
    if (values.size() < 20) {
      ReportError(doc, node, "Invalid 'matrix' value (expected 20 numbers).");
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

static const std::string& GetAttribute(const DOMNode* node, const std::string& name,
                                       const std::string& defaultValue) {
  auto* value = node->findAttribute(name);
  return value ? *value : defaultValue;
}

static float GetFloatAttribute(const DOMNode* node, const std::string& name, float defaultValue,
                               PAGXDocument* doc) {
  auto* str = node->findAttribute(name);
  if (!str || str->empty()) {
    return defaultValue;
  }
  char* endPtr = nullptr;
  float value = strtof(str->c_str(), &endPtr);
  if (endPtr == str->c_str()) {
    if (doc) {
      ReportError(doc, node, "Invalid value '" + *str + "' for '" + name + "' attribute.");
    }
    return defaultValue;
  }
  return value;
}

static int GetIntAttribute(const DOMNode* node, const std::string& name, int defaultValue,
                           PAGXDocument* doc) {
  auto* str = node->findAttribute(name);
  if (!str || str->empty()) {
    return defaultValue;
  }
  char* endPtr = nullptr;
  long value = strtol(str->c_str(), &endPtr, 10);
  if (endPtr == str->c_str()) {
    if (doc) {
      ReportError(doc, node, "Invalid value '" + *str + "' for '" + name + "' attribute.");
    }
    return defaultValue;
  }
  if (value < INT_MIN || value > INT_MAX) {
    if (doc) {
      ReportError(doc, node, "Value out of range for '" + name + "' attribute.");
    }
    return defaultValue;
  }
  return static_cast<int>(value);
}

static bool GetBoolAttribute(const DOMNode* node, const std::string& name, bool defaultValue,
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
    ReportError(doc, node, "Invalid value '" + *str + "' for '" + name + "' attribute.");
  }
  return defaultValue;
}

static std::optional<float> GetOptionalFloatAttribute(const DOMNode* node, const std::string& name,
                                                      PAGXDocument* doc) {
  auto* str = node->findAttribute(name);
  if (!str || str->empty()) {
    return std::nullopt;
  }
  char* endPtr = nullptr;
  float value = strtof(str->c_str(), &endPtr);
  if (endPtr == str->c_str()) {
    if (doc) {
      ReportError(doc, node, "Invalid value '" + *str + "' for '" + name + "' attribute.");
    }
    return std::nullopt;
  }
  return value;
}

static const char* SkipWhitespaceAndComma(const char* ptr, const char* end) {
  while (ptr < end && (*ptr == ' ' || *ptr == '\t' || *ptr == ',')) {
    ++ptr;
  }
  return ptr;
}

static std::pair<float, float> ParseTwoFloats(const std::string& str, bool* outValid) {
  float first = 0;
  float second = 0;
  const char* ptr = str.c_str();
  const char* end = ptr + str.size();
  char* endPtr = nullptr;
  ptr = SkipWhitespaceAndComma(ptr, end);
  first = strtof(ptr, &endPtr);
  if (endPtr > ptr) {
    ptr = SkipWhitespaceAndComma(endPtr, end);
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

static Point ParsePoint(const std::string& str, bool* outValid) {
  auto [x, y] = ParseTwoFloats(str, outValid);
  return {x, y};
}

static Size ParseSize(const std::string& str, bool* outValid) {
  auto [width, height] = ParseTwoFloats(str, outValid);
  return {width, height};
}

static Rect ParseRect(const std::string& str, bool* outValid) {
  Rect rect = {};
  const char* ptr = str.c_str();
  const char* end = ptr + str.size();
  char* endPtr = nullptr;
  bool valid = true;
  ptr = SkipWhitespaceAndComma(ptr, end);
  rect.x = strtof(ptr, &endPtr);
  if (endPtr > ptr) {
    ptr = SkipWhitespaceAndComma(endPtr, end);
    rect.y = strtof(ptr, &endPtr);
  }
  if (endPtr > ptr) {
    ptr = SkipWhitespaceAndComma(endPtr, end);
    rect.width = strtof(ptr, &endPtr);
  }
  if (endPtr > ptr) {
    ptr = SkipWhitespaceAndComma(endPtr, end);
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

static Color ParseColor(const std::string& str, bool* outValid) {
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
      ptr = SkipWhitespaceAndComma(ptr, strEnd);
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
static T GetParsedAttribute(const DOMNode* node, const char* name, T defaultValue,
                            PAGXDocument* doc, Parser parser) {
  auto* str = node->findAttribute(name);
  if (!str || str->empty()) {
    return defaultValue;
  }
  bool valid = false;
  auto result = parser(*str, &valid);
  if (!valid) {
    ReportError(doc, node,
                "Invalid value '" + *str + "' for '" + std::string(name) + "' attribute.");
    return defaultValue;
  }
  return result;
}

static Point GetPointAttribute(const DOMNode* node, const char* name, Point defaultValue,
                               PAGXDocument* doc) {
  return GetParsedAttribute(node, name, defaultValue, doc, ParsePoint);
}

static Size GetSizeAttribute(const DOMNode* node, const char* name, Size defaultValue,
                             PAGXDocument* doc) {
  return GetParsedAttribute(node, name, defaultValue, doc, ParseSize);
}

static Rect GetRectAttribute(const DOMNode* node, const char* name, Rect defaultValue,
                             PAGXDocument* doc) {
  return GetParsedAttribute(node, name, defaultValue, doc, ParseRect);
}

static Color GetColorAttribute(const DOMNode* node, const char* name, PAGXDocument* doc) {
  auto* str = node->findAttribute(name);
  if (!str || str->empty()) {
    return {};
  }
  bool valid = false;
  auto result = ParseColor(*str, &valid);
  if (!valid) {
    ReportError(doc, node,
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
  ParseDocument(root.get(), doc.get());
  return doc;
}

static void ParseDocument(const DOMNode* root, PAGXDocument* doc) {
  doc->width = GetFloatAttribute(root, "width", 0, doc);
  doc->height = GetFloatAttribute(root, "height", 0, doc);
  ParseCustomData(root, doc);

  // First pass: Parse Resources.
  auto child = root->getFirstChild("Resources");
  if (child) {
    ParseResources(child.get(), doc);
  }

  // Second pass: Parse Layers.
  child = root->firstChild;
  while (child) {
    if (child->type == DOMNodeType::Element) {
      if (child->name == "Layer") {
        auto layer = ParseLayer(child.get(), doc);
        if (layer) {
          doc->layers.push_back(layer);
        }
      } else if (child->name != "Resources") {
        ReportError(
            doc, child.get(),
            "Element '" + child->name + "' is not allowed in 'pagx'. Expected: Resources, Layer.");
      }
    }
    child = child->nextSibling;
  }
}

}  // namespace pagx
