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
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <vector>
#include "utils/Base64.h"
#include "utils/StringParser.h"
#include "svg/SVGPathParser.h"
#include "xml/XMLDOM.h"
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
#include "pagx/types/Color.h"

namespace pagx {

//==============================================================================
// Forward declarations and utility functions
//==============================================================================

static const std::string EMPTY_STRING = {};

static void reportError(PAGXDocument* doc, const DOMNode* node, const std::string& message) {
  doc->errors.push_back("line " + std::to_string(node->line) + ": " + message);
}

static const std::string& getAttribute(const DOMNode* node, const std::string& name,
                                       const std::string& defaultValue = EMPTY_STRING);
static float getFloatAttribute(const DOMNode* node, const std::string& name,
                               float defaultValue = 0,
                               PAGXDocument* doc = nullptr);
static int getIntAttribute(const DOMNode* node, const std::string& name, int defaultValue = 0,
                           PAGXDocument* doc = nullptr);
static bool getBoolAttribute(const DOMNode* node, const std::string& name,
                             bool defaultValue = false, PAGXDocument* doc = nullptr);
static Point parsePoint(const std::string& str);
static Size parseSize(const std::string& str);
static Rect parseRect(const std::string& str);
static Color parseColor(const std::string& str);
static Point getPointAttribute(const DOMNode* node, const char* name, Point defaultValue,
                               PAGXDocument* doc);
static Size getSizeAttribute(const DOMNode* node, const char* name, Size defaultValue,
                             PAGXDocument* doc);
static Rect getRectAttribute(const DOMNode* node, const char* name, Rect defaultValue,
                             PAGXDocument* doc);
static Color getColorAttribute(const DOMNode* node, const char* name, PAGXDocument* doc);

// Forward declarations for parse functions
static void parseDocument(const DOMNode* root, PAGXDocument* doc);
static void parseResources(const DOMNode* node, PAGXDocument* doc);
static Node* parseResource(const DOMNode* node, PAGXDocument* doc);
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
// Internal parser implementation
//==============================================================================

static void parseResources(const DOMNode* node, PAGXDocument* doc) {
  auto child = node->firstChild;
  while (child) {
    auto current = child;
    child = child->nextSibling;
    if (current->type != DOMNodeType::Element) {
      continue;
    }
    // Try to parse as a resource (Image, PathData, Composition, Font)
    auto resource = parseResource(current.get(), doc);
    if (resource) {
      continue;
    }
    // Try to parse as a color source (SolidColor, Gradient, ImagePattern)
    auto colorSource = parseColorSource(current.get(), doc);
    if (colorSource) {
      continue;
    }
    // Unknown resource type - report error.
    reportError(doc, current.get(),
                "Element '" + current->name +
                    "' is not allowed in 'Resources'."
                    " Expected: Image, PathData, Composition, Font,"
                    " SolidColor, LinearGradient, RadialGradient,"
                    " ConicGradient, DiamondGradient, ImagePattern.");
  }
}

static Node* parseResource(const DOMNode* node, PAGXDocument* doc) {
  if (node->name == "Image") {
    return parseImage(node, doc);
  }
  if (node->name == "PathData") {
    return parsePathData(node, doc);
  }
  if (node->name == "Composition") {
    return parseComposition(node, doc);
  }
  if (node->name == "Font") {
    return parseFont(node, doc);
  }
  return nullptr;
}

static Layer* parseLayer(const DOMNode* node, PAGXDocument* doc) {
  auto id = getAttribute(node, "id");
  auto layer = doc->makeNode<Layer>(id);
  if (!layer) {
    return nullptr;
  }
  layer->name = getAttribute(node, "name");
  layer->visible = getBoolAttribute(node, "visible", true, doc);
  layer->alpha = getFloatAttribute(node, "alpha", 1, doc);
  layer->blendMode = BlendModeFromString(getAttribute(node, "blendMode", "normal"));
  layer->x = getFloatAttribute(node, "x", 0, doc);
  layer->y = getFloatAttribute(node, "y", 0, doc);
  auto matrixStr = getAttribute(node, "matrix");
  if (!matrixStr.empty()) {
    layer->matrix = MatrixFromString(matrixStr);
  }
  auto matrix3DStr = getAttribute(node, "matrix3D");
  if (!matrix3DStr.empty()) {
    auto floats = ParseFloatList(matrix3DStr);
    if (floats.size() == 16) {
      memcpy(layer->matrix3D.values, floats.data(), 16 * sizeof(float));
    }
  }
  layer->preserve3D = getBoolAttribute(node, "preserve3D", false, doc);
  layer->antiAlias = getBoolAttribute(node, "antiAlias", true, doc);
  layer->groupOpacity = getBoolAttribute(node, "groupOpacity", false, doc);
  layer->passThroughBackground = getBoolAttribute(node, "passThroughBackground", true, doc);
  layer->excludeChildEffectsInLayerStyle =
      getBoolAttribute(node, "excludeChildEffectsInLayerStyle", false, doc);
  auto scrollRectStr = getAttribute(node, "scrollRect");
  if (!scrollRectStr.empty()) {
    layer->scrollRect = getRectAttribute(node, "scrollRect", {}, doc);
    layer->hasScrollRect = true;
  }

  auto maskAttr = getAttribute(node, "mask");
  if (!maskAttr.empty() && maskAttr[0] == '@') {
    layer->mask = doc->findNode<Layer>(maskAttr.substr(1));
    if (!layer->mask) {
      reportError(doc, node, "Resource '" + maskAttr + "' not found for 'mask' attribute.");
    }
  }
  layer->maskType = MaskTypeFromString(getAttribute(node, "maskType", "alpha"));

  auto compositionAttr = getAttribute(node, "composition");
  if (!compositionAttr.empty() && compositionAttr[0] == '@') {
    layer->composition = doc->findNode<Composition>(compositionAttr.substr(1));
    if (!layer->composition) {
      reportError(doc, node,
                  "Resource '" + compositionAttr + "' not found for 'composition' attribute.");
    }
  }

  // Parse data-* custom attributes.
  for (const auto& attr : node->attributes) {
    if (attr.name.length() > 5 && attr.name.compare(0, 5, "data-") == 0) {
      layer->customData[attr.name.substr(5)] = attr.value;
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
  auto rect = doc->makeNode<Rectangle>(getAttribute(node, "id"));
  if (!rect) {
    return nullptr;
  }
  rect->center = getPointAttribute(node, "center", {0, 0}, doc);
  rect->size = getSizeAttribute(node, "size", {100, 100}, doc);
  rect->roundness = getFloatAttribute(node, "roundness", 0, doc);
  rect->reversed = getBoolAttribute(node, "reversed", false, doc);
  return rect;
}

static Ellipse* parseEllipse(const DOMNode* node, PAGXDocument* doc) {
  auto ellipse = doc->makeNode<Ellipse>(getAttribute(node, "id"));
  if (!ellipse) {
    return nullptr;
  }
  ellipse->center = getPointAttribute(node, "center", {0, 0}, doc);
  ellipse->size = getSizeAttribute(node, "size", {100, 100}, doc);
  ellipse->reversed = getBoolAttribute(node, "reversed", false, doc);
  return ellipse;
}

static Polystar* parsePolystar(const DOMNode* node, PAGXDocument* doc) {
  auto polystar = doc->makeNode<Polystar>(getAttribute(node, "id"));
  if (!polystar) {
    return nullptr;
  }
  polystar->center = getPointAttribute(node, "center", {0, 0}, doc);
  polystar->type = PolystarTypeFromString(getAttribute(node, "type", "star"));
  polystar->pointCount = getFloatAttribute(node, "pointCount", 5, doc);
  polystar->outerRadius = getFloatAttribute(node, "outerRadius", 100, doc);
  polystar->innerRadius = getFloatAttribute(node, "innerRadius", 50, doc);
  polystar->rotation = getFloatAttribute(node, "rotation", 0, doc);
  polystar->outerRoundness = getFloatAttribute(node, "outerRoundness", 0, doc);
  polystar->innerRoundness = getFloatAttribute(node, "innerRoundness", 0, doc);
  polystar->reversed = getBoolAttribute(node, "reversed", false, doc);
  return polystar;
}

static Path* parsePath(const DOMNode* node, PAGXDocument* doc) {
  auto id = getAttribute(node, "id");
  auto path = doc->makeNode<Path>(id);
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
      path->data->setPathData(PathDataFromSVGString(dataAttr));
    }
  }
  path->reversed = getBoolAttribute(node, "reversed", false, doc);
  return path;
}

static Text* parseText(const DOMNode* node, PAGXDocument* doc) {
  auto text = doc->makeNode<Text>(getAttribute(node, "id"));
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
  text->position = getPointAttribute(node, "position", {0, 0}, doc);
  text->fontFamily = getAttribute(node, "fontFamily");
  text->fontStyle = getAttribute(node, "fontStyle");
  text->fontSize = getFloatAttribute(node, "fontSize", 12, doc);
  text->letterSpacing = getFloatAttribute(node, "letterSpacing", 0, doc);
  text->fauxBold = getBoolAttribute(node, "fauxBold", false, doc);
  text->fauxItalic = getBoolAttribute(node, "fauxItalic", false, doc);
  text->textAnchor = TextAnchorFromString(getAttribute(node, "textAnchor", "start"));

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
                    "Element '" + child->name +
                        "' is not allowed in '" + node->name +
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
  auto id = getAttribute(node, "id");
  auto fill = doc->makeNode<Fill>(id);
  if (!fill) {
    return nullptr;
  }
  fill->color = parseColorAttr(getAttribute(node, "color"), doc, node);
  fill->alpha = getFloatAttribute(node, "alpha", 1, doc);
  fill->blendMode = BlendModeFromString(getAttribute(node, "blendMode", "normal"));
  fill->fillRule = FillRuleFromString(getAttribute(node, "fillRule", "winding"));
  fill->placement = LayerPlacementFromString(getAttribute(node, "placement", "background"));
  auto childColor = parseChildColorSource(node, doc);
  if (childColor) {
    fill->color = childColor;
  }
  if (fill->color == nullptr) {
    fill->color = doc->makeNode<SolidColor>();
  }
  return fill;
}

static Stroke* parseStroke(const DOMNode* node, PAGXDocument* doc) {
  auto id = getAttribute(node, "id");
  auto stroke = doc->makeNode<Stroke>(id);
  if (!stroke) {
    return nullptr;
  }
  stroke->color = parseColorAttr(getAttribute(node, "color"), doc, node);
  stroke->width = getFloatAttribute(node, "width", 1, doc);
  stroke->alpha = getFloatAttribute(node, "alpha", 1, doc);
  stroke->blendMode = BlendModeFromString(getAttribute(node, "blendMode", "normal"));
  stroke->cap = LineCapFromString(getAttribute(node, "cap", "butt"));
  stroke->join = LineJoinFromString(getAttribute(node, "join", "miter"));
  stroke->miterLimit = getFloatAttribute(node, "miterLimit", 4, doc);
  auto dashesStr = getAttribute(node, "dashes");
  if (!dashesStr.empty()) {
    stroke->dashes = ParseFloatList(dashesStr);
  }
  stroke->dashOffset = getFloatAttribute(node, "dashOffset", 0, doc);
  stroke->dashAdaptive = getBoolAttribute(node, "dashAdaptive", false, doc);
  stroke->align = StrokeAlignFromString(getAttribute(node, "align", "center"));
  stroke->placement = LayerPlacementFromString(getAttribute(node, "placement", "background"));
  auto childColor = parseChildColorSource(node, doc);
  if (childColor) {
    stroke->color = childColor;
  }
  if (stroke->color == nullptr) {
    stroke->color = doc->makeNode<SolidColor>();
  }
  return stroke;
}

//==============================================================================
// Modifier parsing
//==============================================================================

static TrimPath* parseTrimPath(const DOMNode* node, PAGXDocument* doc) {
  auto trim = doc->makeNode<TrimPath>(getAttribute(node, "id"));
  if (!trim) {
    return nullptr;
  }
  trim->start = getFloatAttribute(node, "start", 0, doc);
  trim->end = getFloatAttribute(node, "end", 1, doc);
  trim->offset = getFloatAttribute(node, "offset", 0, doc);
  trim->type = TrimTypeFromString(getAttribute(node, "type", "separate"));
  return trim;
}

static RoundCorner* parseRoundCorner(const DOMNode* node, PAGXDocument* doc) {
  auto round = doc->makeNode<RoundCorner>(getAttribute(node, "id"));
  if (!round) {
    return nullptr;
  }
  round->radius = getFloatAttribute(node, "radius", 10, doc);
  return round;
}

static MergePath* parseMergePath(const DOMNode* node, PAGXDocument* doc) {
  auto merge = doc->makeNode<MergePath>(getAttribute(node, "id"));
  if (!merge) {
    return nullptr;
  }
  merge->mode = MergePathModeFromString(getAttribute(node, "mode", "append"));
  return merge;
}

static TextModifier* parseTextModifier(const DOMNode* node, PAGXDocument* doc) {
  auto id = getAttribute(node, "id");
  auto modifier = doc->makeNode<TextModifier>(id);
  if (!modifier) {
    return nullptr;
  }
  modifier->anchor = getPointAttribute(node, "anchor", {0, 0}, doc);
  modifier->position = getPointAttribute(node, "position", {0, 0}, doc);
  modifier->rotation = getFloatAttribute(node, "rotation", 0, doc);
  modifier->scale = getPointAttribute(node, "scale", {1, 1}, doc);
  modifier->skew = getFloatAttribute(node, "skew", 0, doc);
  modifier->skewAxis = getFloatAttribute(node, "skewAxis", 0, doc);
  modifier->alpha = getFloatAttribute(node, "alpha", 1, doc);
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
  auto id = getAttribute(node, "id");
  auto textPath = doc->makeNode<TextPath>(id);
  if (!textPath) {
    return nullptr;
  }
  auto pathAttr = getAttribute(node, "path");
  if (!pathAttr.empty()) {
    if (pathAttr[0] == '@') {
      // Reference to PathData resource
      textPath->path = doc->findNode<PathData>(pathAttr.substr(1));
      if (!textPath->path) {
        reportError(doc, node,
                    "Resource '" + pathAttr + "' not found for 'path' attribute.");
      }
    } else {
      // Inline path data
      textPath->path = doc->makeNode<PathData>();
      textPath->path->setPathData(PathDataFromSVGString(pathAttr));
    }
  }
  textPath->baselineOrigin = getPointAttribute(node, "baselineOrigin", {0, 0}, doc);
  textPath->baselineAngle = getFloatAttribute(node, "baselineAngle", 0, doc);
  textPath->firstMargin = getFloatAttribute(node, "firstMargin", 0, doc);
  textPath->lastMargin = getFloatAttribute(node, "lastMargin", 0, doc);
  textPath->perpendicular = getBoolAttribute(node, "perpendicular", true, doc);
  textPath->reversed = getBoolAttribute(node, "reversed", false, doc);
  textPath->forceAlignment = getBoolAttribute(node, "forceAlignment", false, doc);
  return textPath;
}

static TextBox* parseTextBox(const DOMNode* node, PAGXDocument* doc) {
  auto textBox = doc->makeNode<TextBox>(getAttribute(node, "id"));
  if (!textBox) {
    return nullptr;
  }
  textBox->position = getPointAttribute(node, "position", {0, 0}, doc);
  textBox->size = getSizeAttribute(node, "size", {0, 0}, doc);
  textBox->textAlign = TextAlignFromString(getAttribute(node, "textAlign", "start"));
  textBox->paragraphAlign = ParagraphAlignFromString(getAttribute(node, "paragraphAlign", "near"));
  textBox->writingMode = WritingModeFromString(getAttribute(node, "writingMode", "horizontal"));
  textBox->lineHeight = getFloatAttribute(node, "lineHeight", 0, doc);
  textBox->wordWrap = getBoolAttribute(node, "wordWrap", true, doc);
  textBox->overflow = OverflowFromString(getAttribute(node, "overflow", "visible"));
  return textBox;
}

static Repeater* parseRepeater(const DOMNode* node, PAGXDocument* doc) {
  auto repeater = doc->makeNode<Repeater>(getAttribute(node, "id"));
  if (!repeater) {
    return nullptr;
  }
  repeater->copies = getFloatAttribute(node, "copies", 3, doc);
  repeater->offset = getFloatAttribute(node, "offset", 0, doc);
  repeater->order = RepeaterOrderFromString(getAttribute(node, "order", "belowOriginal"));
  repeater->anchor = getPointAttribute(node, "anchor", {0, 0}, doc);
  repeater->position = getPointAttribute(node, "position", {100, 100}, doc);
  repeater->rotation = getFloatAttribute(node, "rotation", 0, doc);
  repeater->scale = getPointAttribute(node, "scale", {1, 1}, doc);
  repeater->startAlpha = getFloatAttribute(node, "startAlpha", 1, doc);
  repeater->endAlpha = getFloatAttribute(node, "endAlpha", 1, doc);
  return repeater;
}

static Group* parseGroup(const DOMNode* node, PAGXDocument* doc) {
  auto group = doc->makeNode<Group>(getAttribute(node, "id"));
  if (!group) {
    return nullptr;
  }
  group->anchor = getPointAttribute(node, "anchor", {0, 0}, doc);
  group->position = getPointAttribute(node, "position", {0, 0}, doc);
  group->rotation = getFloatAttribute(node, "rotation", 0, doc);
  group->scale = getPointAttribute(node, "scale", {1, 1}, doc);
  group->skew = getFloatAttribute(node, "skew", 0, doc);
  group->skewAxis = getFloatAttribute(node, "skewAxis", 0, doc);
  group->alpha = getFloatAttribute(node, "alpha", 1, doc);

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
  auto selector = doc->makeNode<RangeSelector>(getAttribute(node, "id"));
  if (!selector) {
    return nullptr;
  }
  selector->start = getFloatAttribute(node, "start", 0, doc);
  selector->end = getFloatAttribute(node, "end", 1, doc);
  selector->offset = getFloatAttribute(node, "offset", 0, doc);
  selector->unit = SelectorUnitFromString(getAttribute(node, "unit", "percentage"));
  selector->shape = SelectorShapeFromString(getAttribute(node, "shape", "square"));
  selector->easeIn = getFloatAttribute(node, "easeIn", 0, doc);
  selector->easeOut = getFloatAttribute(node, "easeOut", 0, doc);
  selector->mode = SelectorModeFromString(getAttribute(node, "mode", "add"));
  selector->weight = getFloatAttribute(node, "weight", 1, doc);
  selector->randomOrder = getBoolAttribute(node, "randomOrder", false, doc);
  selector->randomSeed = getIntAttribute(node, "randomSeed", 0, doc);
  return selector;
}

//==============================================================================
// Color source parsing
//==============================================================================

static SolidColor* parseSolidColor(const DOMNode* node, PAGXDocument* doc) {
  auto solid = doc->makeNode<SolidColor>(getAttribute(node, "id"));
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
    solid->color.colorSpace = ColorSpaceFromString(getAttribute(node, "colorSpace", "sRGB"));
  }
  return solid;
}

static void parseGradientCommon(const DOMNode* node, PAGXDocument* doc, Matrix& matrix,
                                std::vector<ColorStop*>& colorStops) {
  auto matrixStr = getAttribute(node, "matrix");
  if (!matrixStr.empty()) {
    matrix = MatrixFromString(matrixStr);
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
  auto gradient = doc->makeNode<LinearGradient>(getAttribute(node, "id"));
  if (!gradient) {
    return nullptr;
  }
  gradient->startPoint = getPointAttribute(node, "startPoint", {0, 0}, doc);
  gradient->endPoint = getPointAttribute(node, "endPoint", {0, 0}, doc);
  parseGradientCommon(node, doc, gradient->matrix, gradient->colorStops);
  return gradient;
}

static RadialGradient* parseRadialGradient(const DOMNode* node, PAGXDocument* doc) {
  auto gradient = doc->makeNode<RadialGradient>(getAttribute(node, "id"));
  if (!gradient) {
    return nullptr;
  }
  gradient->center = getPointAttribute(node, "center", {0, 0}, doc);
  gradient->radius = getFloatAttribute(node, "radius", 0, doc);
  parseGradientCommon(node, doc, gradient->matrix, gradient->colorStops);
  return gradient;
}

static ConicGradient* parseConicGradient(const DOMNode* node, PAGXDocument* doc) {
  auto gradient = doc->makeNode<ConicGradient>(getAttribute(node, "id"));
  if (!gradient) {
    return nullptr;
  }
  gradient->center = getPointAttribute(node, "center", {0, 0}, doc);
  gradient->startAngle = getFloatAttribute(node, "startAngle", 0, doc);
  gradient->endAngle = getFloatAttribute(node, "endAngle", 360, doc);
  parseGradientCommon(node, doc, gradient->matrix, gradient->colorStops);
  return gradient;
}

static DiamondGradient* parseDiamondGradient(const DOMNode* node, PAGXDocument* doc) {
  auto gradient = doc->makeNode<DiamondGradient>(getAttribute(node, "id"));
  if (!gradient) {
    return nullptr;
  }
  gradient->center = getPointAttribute(node, "center", {0, 0}, doc);
  gradient->radius = getFloatAttribute(node, "radius", 0, doc);
  parseGradientCommon(node, doc, gradient->matrix, gradient->colorStops);
  return gradient;
}

static ImagePattern* parseImagePattern(const DOMNode* node, PAGXDocument* doc) {
  auto id = getAttribute(node, "id");
  auto pattern = doc->makeNode<ImagePattern>(id);
  if (!pattern) {
    return nullptr;
  }
  auto imageAttr = getAttribute(node, "image");
  if (!imageAttr.empty() && imageAttr[0] == '@') {
    pattern->image = doc->findNode<Image>(imageAttr.substr(1));
    if (!pattern->image) {
      reportError(doc, node,
                  "Resource '" + imageAttr + "' not found for 'image' attribute.");
    }
  }
  pattern->tileModeX = TileModeFromString(getAttribute(node, "tileModeX", "clamp"));
  pattern->tileModeY = TileModeFromString(getAttribute(node, "tileModeY", "clamp"));
  pattern->filterMode = FilterModeFromString(getAttribute(node, "filterMode", "linear"));
  pattern->mipmapMode = MipmapModeFromString(getAttribute(node, "mipmapMode", "linear"));
  auto matrixStr = getAttribute(node, "matrix");
  if (!matrixStr.empty()) {
    pattern->matrix = MatrixFromString(matrixStr);
  }
  return pattern;
}

static ColorStop* parseColorStop(const DOMNode* node, PAGXDocument* doc) {
  auto stop = doc->makeNode<ColorStop>();
  stop->offset = getFloatAttribute(node, "offset", 0, doc);
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
  auto id = getAttribute(node, "id");
  auto image = doc->makeNode<Image>(id);
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
  auto id = getAttribute(node, "id");
  auto pathData = doc->makeNode<PathData>(id);
  if (!pathData) {
    return nullptr;
  }
  auto data = getAttribute(node, "data");
  if (!data.empty()) {
    pathData->setPathData(PathDataFromSVGString(data));
  }
  return pathData;
}

static Composition* parseComposition(const DOMNode* node, PAGXDocument* doc) {
  auto comp = doc->makeNode<Composition>(getAttribute(node, "id"));
  if (!comp) {
    return nullptr;
  }
  comp->width = getFloatAttribute(node, "width", 0, doc);
  comp->height = getFloatAttribute(node, "height", 0, doc);
  auto child = node->firstChild;
  while (child) {
    if (child->type == DOMNodeType::Element) {
      if (child->name == "Layer") {
        auto layer = parseLayer(child.get(), doc);
        if (layer) {
          comp->layers.push_back(layer);
        }
      } else {
        reportError(doc, child.get(),
                    "Element '" + child->name +
                        "' is not allowed in 'Composition'. Expected: Layer.");
      }
    }
    child = child->nextSibling;
  }
  return comp;
}

static Font* parseFont(const DOMNode* node, PAGXDocument* doc) {
  auto font = doc->makeNode<Font>(getAttribute(node, "id"));
  if (!font) {
    return nullptr;
  }
  font->unitsPerEm = getIntAttribute(node, "unitsPerEm", 1000, doc);
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
                    "Element '" + child->name +
                        "' is not allowed in 'Font'. Expected: Glyph.");
      }
    }
    child = child->nextSibling;
  }
  return font;
}

static Glyph* parseGlyph(const DOMNode* node, PAGXDocument* doc) {
  auto glyph = doc->makeNode<Glyph>(getAttribute(node, "id"));
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
      glyph->path->setPathData(PathDataFromSVGString(pathAttr));
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
    glyph->offset = getPointAttribute(node, "offset", {0, 0}, doc);
  }
  glyph->advance = getFloatAttribute(node, "advance", 0, doc);
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
  auto run = doc->makeNode<GlyphRun>(getAttribute(node, "id"));
  if (!run) {
    return nullptr;
  }
  auto fontAttr = getAttribute(node, "font");
  if (!fontAttr.empty() && fontAttr[0] == '@') {
    auto fontId = fontAttr.substr(1);
    run->font = doc->findNode<Font>(fontId);
    if (!run->font) {
      reportError(doc, node,
                  "Resource '" + fontAttr + "' not found for 'font' attribute.");
    }
  }
  run->fontSize = getFloatAttribute(node, "fontSize", 12, doc);
  run->x = getFloatAttribute(node, "x", 0, doc);
  run->y = getFloatAttribute(node, "y", 0, doc);

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

  return run;
}

//==============================================================================
// Layer style parsing
//==============================================================================

static void parseShadowAttributes(const DOMNode* node, PAGXDocument* doc, float& offsetX,
                                  float& offsetY, float& blurX, float& blurY, Color& color) {
  offsetX = getFloatAttribute(node, "offsetX", 0, doc);
  offsetY = getFloatAttribute(node, "offsetY", 0, doc);
  blurX = getFloatAttribute(node, "blurX", 0, doc);
  blurY = getFloatAttribute(node, "blurY", 0, doc);
  auto colorStr = getAttribute(node, "color");
  if (!colorStr.empty()) {
    color = getColorAttribute(node, "color", doc);
  }
}

static DropShadowStyle* parseDropShadowStyle(const DOMNode* node, PAGXDocument* doc) {
  auto style = doc->makeNode<DropShadowStyle>(getAttribute(node, "id"));
  if (!style) {
    return nullptr;
  }
  style->blendMode = BlendModeFromString(getAttribute(node, "blendMode", "normal"));
  parseShadowAttributes(node, doc, style->offsetX, style->offsetY, style->blurX, style->blurY,
                         style->color);
  style->showBehindLayer = getBoolAttribute(node, "showBehindLayer", true, doc);
  return style;
}

static InnerShadowStyle* parseInnerShadowStyle(const DOMNode* node, PAGXDocument* doc) {
  auto style = doc->makeNode<InnerShadowStyle>(getAttribute(node, "id"));
  if (!style) {
    return nullptr;
  }
  style->blendMode = BlendModeFromString(getAttribute(node, "blendMode", "normal"));
  parseShadowAttributes(node, doc, style->offsetX, style->offsetY, style->blurX, style->blurY,
                         style->color);
  return style;
}

static BackgroundBlurStyle* parseBackgroundBlurStyle(
    const DOMNode* node, PAGXDocument* doc) {
  auto style = doc->makeNode<BackgroundBlurStyle>(getAttribute(node, "id"));
  if (!style) {
    return nullptr;
  }
  style->blendMode = BlendModeFromString(getAttribute(node, "blendMode", "normal"));
  style->blurX = getFloatAttribute(node, "blurX", 0, doc);
  style->blurY = getFloatAttribute(node, "blurY", 0, doc);
  style->tileMode = TileModeFromString(getAttribute(node, "tileMode", "mirror"));
  return style;
}

//==============================================================================
// Layer filter parsing
//==============================================================================

static BlurFilter* parseBlurFilter(const DOMNode* node, PAGXDocument* doc) {
  auto filter = doc->makeNode<BlurFilter>(getAttribute(node, "id"));
  if (!filter) {
    return nullptr;
  }
  filter->blurX = getFloatAttribute(node, "blurX", 0, doc);
  filter->blurY = getFloatAttribute(node, "blurY", 0, doc);
  filter->tileMode = TileModeFromString(getAttribute(node, "tileMode", "decal"));
  return filter;
}

static DropShadowFilter* parseDropShadowFilter(const DOMNode* node, PAGXDocument* doc) {
  auto filter = doc->makeNode<DropShadowFilter>(getAttribute(node, "id"));
  if (!filter) {
    return nullptr;
  }
  parseShadowAttributes(node, doc, filter->offsetX, filter->offsetY, filter->blurX, filter->blurY,
                         filter->color);
  filter->shadowOnly = getBoolAttribute(node, "shadowOnly", false, doc);
  return filter;
}

static InnerShadowFilter* parseInnerShadowFilter(const DOMNode* node, PAGXDocument* doc) {
  auto filter = doc->makeNode<InnerShadowFilter>(getAttribute(node, "id"));
  if (!filter) {
    return nullptr;
  }
  parseShadowAttributes(node, doc, filter->offsetX, filter->offsetY, filter->blurX, filter->blurY,
                         filter->color);
  filter->shadowOnly = getBoolAttribute(node, "shadowOnly", false, doc);
  return filter;
}

static BlendFilter* parseBlendFilter(const DOMNode* node, PAGXDocument* doc) {
  auto filter = doc->makeNode<BlendFilter>(getAttribute(node, "id"));
  if (!filter) {
    return nullptr;
  }
  auto colorStr = getAttribute(node, "color");
  if (!colorStr.empty()) {
    filter->color = getColorAttribute(node, "color", doc);
  }
  filter->blendMode = BlendModeFromString(getAttribute(node, "blendMode", "normal"));
  return filter;
}

static ColorMatrixFilter* parseColorMatrixFilter(const DOMNode* node, PAGXDocument* doc) {
  auto filter = doc->makeNode<ColorMatrixFilter>(getAttribute(node, "id"));
  if (!filter) {
    return nullptr;
  }
  auto matrixStr = getAttribute(node, "matrix");
  if (!matrixStr.empty()) {
    auto values = ParseFloatList(matrixStr);
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

static float getFloatAttribute(const DOMNode* node, const std::string& name,
                               float defaultValue, PAGXDocument* doc) {
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
  return static_cast<int>(value);
}

static bool getBoolAttribute(const DOMNode* node, const std::string& name,
                             bool defaultValue, PAGXDocument* doc) {
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

static const char* skipWhitespaceAndComma(const char* ptr, const char* end) {
  while (ptr < end && (*ptr == ' ' || *ptr == '\t' || *ptr == ',')) {
    ++ptr;
  }
  return ptr;
}

static Point parsePoint(const std::string& str) {
  Point point = {};
  const char* ptr = str.c_str();
  const char* end = ptr + str.size();
  char* endPtr = nullptr;
  ptr = skipWhitespaceAndComma(ptr, end);
  point.x = strtof(ptr, &endPtr);
  if (endPtr > ptr) {
    ptr = skipWhitespaceAndComma(endPtr, end);
    point.y = strtof(ptr, &endPtr);
  }
  return point;
}

static Size parseSize(const std::string& str) {
  Size size = {};
  const char* ptr = str.c_str();
  const char* end = ptr + str.size();
  char* endPtr = nullptr;
  ptr = skipWhitespaceAndComma(ptr, end);
  size.width = strtof(ptr, &endPtr);
  if (endPtr > ptr) {
    ptr = skipWhitespaceAndComma(endPtr, end);
    size.height = strtof(ptr, &endPtr);
  }
  return size;
}

static Rect parseRect(const std::string& str) {
  Rect rect = {};
  const char* ptr = str.c_str();
  const char* end = ptr + str.size();
  char* endPtr = nullptr;
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
  return 0;
}
}  // namespace

static Color parseColor(const std::string& str) {
  if (str.empty()) {
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
      color.red = static_cast<float>(r * 17) / 255.0f;
      color.green = static_cast<float>(g * 17) / 255.0f;
      color.blue = static_cast<float>(b * 17) / 255.0f;
      color.alpha = 1.0f;
      return color;
    }
    if (str.size() == 7 || str.size() == 9) {
      int r = parseHexDigit(str[1]) * 16 + parseHexDigit(str[2]);
      int g = parseHexDigit(str[3]) * 16 + parseHexDigit(str[4]);
      int b = parseHexDigit(str[5]) * 16 + parseHexDigit(str[6]);
      color.red = static_cast<float>(r) / 255.0f;
      color.green = static_cast<float>(g) / 255.0f;
      color.blue = static_cast<float>(b) / 255.0f;
      color.alpha = str.size() == 9
                        ? static_cast<float>(parseHexDigit(str[7]) * 16 + parseHexDigit(str[8])) /
                              255.0f
                        : 1.0f;
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
    return color;
  }
  return {};
}

static bool isValidParsedValue(const std::string& str, const char* endPtr) {
  return endPtr > str.c_str();
}

static Point getPointAttribute(const DOMNode* node, const char* name, Point defaultValue,
                               PAGXDocument* doc) {
  auto* str = node->findAttribute(name);
  if (!str || str->empty()) {
    return defaultValue;
  }
  auto result = parsePoint(*str);
  const char* ptr = str->c_str();
  char* endPtr = nullptr;
  strtof(ptr, &endPtr);
  if (!isValidParsedValue(*str, endPtr)) {
    reportError(doc, node,
                "Invalid value '" + *str + "' for '" + std::string(name) + "' attribute.");
    return defaultValue;
  }
  return result;
}

static Size getSizeAttribute(const DOMNode* node, const char* name, Size defaultValue,
                             PAGXDocument* doc) {
  auto* str = node->findAttribute(name);
  if (!str || str->empty()) {
    return defaultValue;
  }
  auto result = parseSize(*str);
  const char* ptr = str->c_str();
  char* endPtr = nullptr;
  strtof(ptr, &endPtr);
  if (!isValidParsedValue(*str, endPtr)) {
    reportError(doc, node,
                "Invalid value '" + *str + "' for '" + std::string(name) + "' attribute.");
    return defaultValue;
  }
  return result;
}

static Rect getRectAttribute(const DOMNode* node, const char* name, Rect defaultValue,
                             PAGXDocument* doc) {
  auto* str = node->findAttribute(name);
  if (!str || str->empty()) {
    return defaultValue;
  }
  auto result = parseRect(*str);
  const char* ptr = str->c_str();
  char* endPtr = nullptr;
  strtof(ptr, &endPtr);
  if (!isValidParsedValue(*str, endPtr)) {
    reportError(doc, node,
                "Invalid value '" + *str + "' for '" + std::string(name) + "' attribute.");
    return defaultValue;
  }
  return result;
}

static Color getColorAttribute(const DOMNode* node, const char* name, PAGXDocument* doc) {
  auto* str = node->findAttribute(name);
  if (!str || str->empty()) {
    return {};
  }
  auto result = parseColor(*str);
  if (result.red == 0 && result.green == 0 && result.blue == 0 && result.alpha == 0 &&
      !str->empty() && (*str)[0] != '#' && str->find("srgb(") != 0 && str->find("p3(") != 0) {
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
        reportError(doc, child.get(),
                    "Element '" + child->name +
                        "' is not allowed in 'pagx'. Expected: Resources, Layer.");
      }
    }
    child = child->nextSibling;
  }
}

}  // namespace pagx
