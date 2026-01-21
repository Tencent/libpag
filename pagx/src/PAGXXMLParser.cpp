/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "PAGXXMLParser.h"
#include <cstring>
#include <sstream>

namespace pagx {

//==============================================================================
// Simple XML tokenizer
//==============================================================================

class XMLTokenizer {
 public:
  XMLTokenizer(const uint8_t* data, size_t length)
      : data(reinterpret_cast<const char*>(data)), length(length) {
  }

  std::unique_ptr<XMLNode> parse() {
    skipWhitespace();
    skipXMLDeclaration();
    skipWhitespace();
    return parseElement();
  }

 private:
  const char* data = nullptr;
  size_t length = 0;
  size_t pos = 0;

  char peek() const {
    return pos < length ? data[pos] : '\0';
  }

  char get() {
    return pos < length ? data[pos++] : '\0';
  }

  void skipWhitespace() {
    while (pos < length && (data[pos] == ' ' || data[pos] == '\t' || data[pos] == '\n' ||
                            data[pos] == '\r')) {
      pos++;
    }
  }

  void skipXMLDeclaration() {
    if (pos + 5 < length && strncmp(data + pos, "<?xml", 5) == 0) {
      while (pos < length && !(data[pos] == '?' && pos + 1 < length && data[pos + 1] == '>')) {
        pos++;
      }
      if (pos + 1 < length) {
        pos += 2;
      }
    }
  }

  void skipComment() {
    if (pos + 4 < length && strncmp(data + pos, "<!--", 4) == 0) {
      pos += 4;
      while (pos + 2 < length && !(data[pos] == '-' && data[pos + 1] == '-' && data[pos + 2] == '>')) {
        pos++;
      }
      if (pos + 2 < length) {
        pos += 3;
      }
    }
  }

  std::string parseTagName() {
    std::string name = {};
    while (pos < length && !isspace(data[pos]) && data[pos] != '>' && data[pos] != '/') {
      name += data[pos++];
    }
    return name;
  }

  std::string parseAttributeName() {
    std::string name = {};
    while (pos < length && !isspace(data[pos]) && data[pos] != '=' && data[pos] != '>' &&
           data[pos] != '/') {
      name += data[pos++];
    }
    return name;
  }

  std::string parseAttributeValue() {
    skipWhitespace();
    if (peek() != '=') {
      return "";
    }
    get();
    skipWhitespace();
    char quote = get();
    if (quote != '"' && quote != '\'') {
      return "";
    }
    std::string value = {};
    while (pos < length && data[pos] != quote) {
      if (data[pos] == '&') {
        value += parseEntity();
      } else {
        value += data[pos++];
      }
    }
    if (pos < length) {
      pos++;
    }
    return value;
  }

  char parseEntity() {
    pos++;
    std::string entity = {};
    while (pos < length && data[pos] != ';') {
      entity += data[pos++];
    }
    if (pos < length) {
      pos++;
    }
    if (entity == "lt") {
      return '<';
    }
    if (entity == "gt") {
      return '>';
    }
    if (entity == "amp") {
      return '&';
    }
    if (entity == "apos") {
      return '\'';
    }
    if (entity == "quot") {
      return '"';
    }
    return '?';
  }

  std::string parseText() {
    std::string text = {};
    while (pos < length && data[pos] != '<') {
      if (data[pos] == '&') {
        text += parseEntity();
      } else {
        text += data[pos++];
      }
    }
    auto start = text.find_first_not_of(" \t\n\r");
    auto end = text.find_last_not_of(" \t\n\r");
    if (start == std::string::npos) {
      return "";
    }
    return text.substr(start, end - start + 1);
  }

  std::string parseCDATA() {
    if (pos + 9 < length && strncmp(data + pos, "<![CDATA[", 9) == 0) {
      pos += 9;
      std::string content = {};
      while (pos + 2 < length && !(data[pos] == ']' && data[pos + 1] == ']' && data[pos + 2] == '>')) {
        content += data[pos++];
      }
      if (pos + 2 < length) {
        pos += 3;
      }
      return content;
    }
    return "";
  }

  std::unique_ptr<XMLNode> parseElement() {
    skipWhitespace();
    while (pos + 4 < length && strncmp(data + pos, "<!--", 4) == 0) {
      skipComment();
      skipWhitespace();
    }

    if (peek() != '<') {
      return nullptr;
    }
    get();

    auto node = std::make_unique<XMLNode>();
    node->tag = parseTagName();

    while (true) {
      skipWhitespace();
      if (peek() == '>') {
        get();
        break;
      }
      if (peek() == '/' && pos + 1 < length && data[pos + 1] == '>') {
        pos += 2;
        return node;
      }
      auto attrName = parseAttributeName();
      if (attrName.empty()) {
        break;
      }
      auto attrValue = parseAttributeValue();
      node->attributes[attrName] = attrValue;
    }

    while (true) {
      skipWhitespace();
      if (pos + 9 < length && strncmp(data + pos, "<![CDATA[", 9) == 0) {
        node->text = parseCDATA();
        continue;
      }
      while (pos + 4 < length && strncmp(data + pos, "<!--", 4) == 0) {
        skipComment();
        skipWhitespace();
      }
      if (peek() != '<') {
        auto text = parseText();
        if (!text.empty()) {
          node->text = text;
        }
        continue;
      }
      if (pos + 1 < length && data[pos + 1] == '/') {
        pos += 2;
        while (pos < length && data[pos] != '>') {
          pos++;
        }
        if (pos < length) {
          pos++;
        }
        break;
      }
      auto child = parseElement();
      if (child) {
        node->children.push_back(std::move(child));
      } else {
        break;
      }
    }

    return node;
  }
};

//==============================================================================
// PAGXXMLParser implementation
//==============================================================================

std::shared_ptr<Document> PAGXXMLParser::Parse(const uint8_t* data, size_t length) {
  auto root = parseXML(data, length);
  if (!root || root->tag != "pagx") {
    return nullptr;
  }
  auto doc = std::shared_ptr<Document>(new Document());
  parseDocument(root.get(), doc.get());
  return doc;
}

std::unique_ptr<XMLNode> PAGXXMLParser::parseXML(const uint8_t* data, size_t length) {
  XMLTokenizer tokenizer(data, length);
  return tokenizer.parse();
}

void PAGXXMLParser::parseDocument(const XMLNode* root, Document* doc) {
  doc->version = getAttribute(root, "version", "1.0");
  doc->width = getFloatAttribute(root, "width", 0);
  doc->height = getFloatAttribute(root, "height", 0);

  for (const auto& child : root->children) {
    if (child->tag == "Resources") {
      parseResources(child.get(), doc);
    } else if (child->tag == "Layer") {
      auto layer = parseLayer(child.get());
      if (layer) {
        doc->layers.push_back(std::move(layer));
      }
    }
  }
}

void PAGXXMLParser::parseResources(const XMLNode* node, Document* doc) {
  for (const auto& child : node->children) {
    auto resource = parseResource(child.get());
    if (resource) {
      doc->resources.push_back(std::move(resource));
    }
  }
}

std::unique_ptr<Resource> PAGXXMLParser::parseResource(const XMLNode* node) {
  if (node->tag == "Image") {
    return parseImage(node);
  }
  if (node->tag == "PathData") {
    return parsePathData(node);
  }
  if (node->tag == "SolidColor") {
    return parseSolidColor(node);
  }
  if (node->tag == "LinearGradient") {
    return parseLinearGradient(node);
  }
  if (node->tag == "RadialGradient") {
    return parseRadialGradient(node);
  }
  if (node->tag == "ConicGradient") {
    return parseConicGradient(node);
  }
  if (node->tag == "DiamondGradient") {
    return parseDiamondGradient(node);
  }
  if (node->tag == "ImagePattern") {
    return parseImagePattern(node);
  }
  if (node->tag == "Composition") {
    return parseComposition(node);
  }
  return nullptr;
}

std::unique_ptr<Layer> PAGXXMLParser::parseLayer(const XMLNode* node) {
  auto layer = std::make_unique<Layer>();
  layer->id = getAttribute(node, "id");
  layer->name = getAttribute(node, "name");
  layer->visible = getBoolAttribute(node, "visible", true);
  layer->alpha = getFloatAttribute(node, "alpha", 1);
  layer->blendMode = BlendModeFromString(getAttribute(node, "blendMode", "normal"));
  layer->x = getFloatAttribute(node, "x", 0);
  layer->y = getFloatAttribute(node, "y", 0);
  auto matrixStr = getAttribute(node, "matrix");
  if (!matrixStr.empty()) {
    layer->matrix = Matrix::Parse(matrixStr);
  }
  auto matrix3DStr = getAttribute(node, "matrix3D");
  if (!matrix3DStr.empty()) {
    layer->matrix3D = parseFloatList(matrix3DStr);
  }
  layer->preserve3D = getBoolAttribute(node, "preserve3D", false);
  layer->antiAlias = getBoolAttribute(node, "antiAlias", true);
  layer->groupOpacity = getBoolAttribute(node, "groupOpacity", false);
  layer->passThroughBackground = getBoolAttribute(node, "passThroughBackground", true);
  layer->excludeChildEffectsInLayerStyle =
      getBoolAttribute(node, "excludeChildEffectsInLayerStyle", false);
  auto scrollRectStr = getAttribute(node, "scrollRect");
  if (!scrollRectStr.empty()) {
    layer->scrollRect = parseRect(scrollRectStr);
    layer->hasScrollRect = true;
  }
  layer->mask = getAttribute(node, "mask");
  layer->maskType = MaskTypeFromString(getAttribute(node, "maskType", "alpha"));
  layer->composition = getAttribute(node, "composition");

  // Parse data-* custom attributes.
  for (const auto& [key, value] : node->attributes) {
    if (key.length() > 5 && key.substr(0, 5) == "data-") {
      layer->customData[key.substr(5)] = value;
    }
  }

  for (const auto& child : node->children) {
    if (child->tag == "contents") {
      parseContents(child.get(), layer.get());
    } else if (child->tag == "styles") {
      parseStyles(child.get(), layer.get());
    } else if (child->tag == "filters") {
      parseFilters(child.get(), layer.get());
    } else if (child->tag == "Layer") {
      auto childLayer = parseLayer(child.get());
      if (childLayer) {
        layer->children.push_back(std::move(childLayer));
      }
    }
  }

  return layer;
}

void PAGXXMLParser::parseContents(const XMLNode* node, Layer* layer) {
  for (const auto& child : node->children) {
    auto element = parseElement(child.get());
    if (element) {
      layer->contents.push_back(std::move(element));
    }
  }
}

void PAGXXMLParser::parseStyles(const XMLNode* node, Layer* layer) {
  for (const auto& child : node->children) {
    auto style = parseLayerStyle(child.get());
    if (style) {
      layer->styles.push_back(std::move(style));
    }
  }
}

void PAGXXMLParser::parseFilters(const XMLNode* node, Layer* layer) {
  for (const auto& child : node->children) {
    auto filter = parseLayerFilter(child.get());
    if (filter) {
      layer->filters.push_back(std::move(filter));
    }
  }
}

std::unique_ptr<Element> PAGXXMLParser::parseElement(const XMLNode* node) {
  if (node->tag == "Rectangle") {
    return parseRectangle(node);
  }
  if (node->tag == "Ellipse") {
    return parseEllipse(node);
  }
  if (node->tag == "Polystar") {
    return parsePolystar(node);
  }
  if (node->tag == "Path") {
    return parsePath(node);
  }
  if (node->tag == "TextSpan") {
    return parseTextSpan(node);
  }
  if (node->tag == "Fill") {
    return parseFill(node);
  }
  if (node->tag == "Stroke") {
    return parseStroke(node);
  }
  if (node->tag == "TrimPath") {
    return parseTrimPath(node);
  }
  if (node->tag == "RoundCorner") {
    return parseRoundCorner(node);
  }
  if (node->tag == "MergePath") {
    return parseMergePath(node);
  }
  if (node->tag == "TextModifier") {
    return parseTextModifier(node);
  }
  if (node->tag == "TextPath") {
    return parseTextPath(node);
  }
  if (node->tag == "TextLayout") {
    return parseTextLayout(node);
  }
  if (node->tag == "Repeater") {
    return parseRepeater(node);
  }
  if (node->tag == "Group") {
    return parseGroup(node);
  }
  return nullptr;
}

std::unique_ptr<ColorSource> PAGXXMLParser::parseColorSource(const XMLNode* node) {
  if (node->tag == "SolidColor") {
    return parseSolidColor(node);
  }
  if (node->tag == "LinearGradient") {
    return parseLinearGradient(node);
  }
  if (node->tag == "RadialGradient") {
    return parseRadialGradient(node);
  }
  if (node->tag == "ConicGradient") {
    return parseConicGradient(node);
  }
  if (node->tag == "DiamondGradient") {
    return parseDiamondGradient(node);
  }
  if (node->tag == "ImagePattern") {
    return parseImagePattern(node);
  }
  return nullptr;
}

std::unique_ptr<LayerStyle> PAGXXMLParser::parseLayerStyle(const XMLNode* node) {
  if (node->tag == "DropShadowStyle") {
    return parseDropShadowStyle(node);
  }
  if (node->tag == "InnerShadowStyle") {
    return parseInnerShadowStyle(node);
  }
  if (node->tag == "BackgroundBlurStyle") {
    return parseBackgroundBlurStyle(node);
  }
  return nullptr;
}

std::unique_ptr<LayerFilter> PAGXXMLParser::parseLayerFilter(const XMLNode* node) {
  if (node->tag == "BlurFilter") {
    return parseBlurFilter(node);
  }
  if (node->tag == "DropShadowFilter") {
    return parseDropShadowFilter(node);
  }
  if (node->tag == "InnerShadowFilter") {
    return parseInnerShadowFilter(node);
  }
  if (node->tag == "BlendFilter") {
    return parseBlendFilter(node);
  }
  if (node->tag == "ColorMatrixFilter") {
    return parseColorMatrixFilter(node);
  }
  return nullptr;
}

//==============================================================================
// Geometry element parsing
//==============================================================================

std::unique_ptr<Rectangle> PAGXXMLParser::parseRectangle(const XMLNode* node) {
  auto rect = std::make_unique<Rectangle>();
  auto centerStr = getAttribute(node, "center", "0,0");
  rect->center = parsePoint(centerStr);
  auto sizeStr = getAttribute(node, "size", "0,0");
  rect->size = parseSize(sizeStr);
  rect->roundness = getFloatAttribute(node, "roundness", 0);
  rect->reversed = getBoolAttribute(node, "reversed", false);
  return rect;
}

std::unique_ptr<Ellipse> PAGXXMLParser::parseEllipse(const XMLNode* node) {
  auto ellipse = std::make_unique<Ellipse>();
  auto centerStr = getAttribute(node, "center", "0,0");
  ellipse->center = parsePoint(centerStr);
  auto sizeStr = getAttribute(node, "size", "0,0");
  ellipse->size = parseSize(sizeStr);
  ellipse->reversed = getBoolAttribute(node, "reversed", false);
  return ellipse;
}

std::unique_ptr<Polystar> PAGXXMLParser::parsePolystar(const XMLNode* node) {
  auto polystar = std::make_unique<Polystar>();
  auto centerStr = getAttribute(node, "center", "0,0");
  polystar->center = parsePoint(centerStr);
  polystar->polystarType = PolystarTypeFromString(getAttribute(node, "polystarType", "star"));
  polystar->pointCount = getFloatAttribute(node, "pointCount", 5);
  polystar->outerRadius = getFloatAttribute(node, "outerRadius", 100);
  polystar->innerRadius = getFloatAttribute(node, "innerRadius", 50);
  polystar->rotation = getFloatAttribute(node, "rotation", 0);
  polystar->outerRoundness = getFloatAttribute(node, "outerRoundness", 0);
  polystar->innerRoundness = getFloatAttribute(node, "innerRoundness", 0);
  polystar->reversed = getBoolAttribute(node, "reversed", false);
  return polystar;
}

std::unique_ptr<Path> PAGXXMLParser::parsePath(const XMLNode* node) {
  auto path = std::make_unique<Path>();
  auto dataAttr = getAttribute(node, "data");
  if (!dataAttr.empty()) {
    path->data = PathData::FromSVGString(dataAttr);
  }
  path->reversed = getBoolAttribute(node, "reversed", false);
  return path;
}

std::unique_ptr<TextSpan> PAGXXMLParser::parseTextSpan(const XMLNode* node) {
  auto textSpan = std::make_unique<TextSpan>();
  textSpan->x = getFloatAttribute(node, "x", 0);
  textSpan->y = getFloatAttribute(node, "y", 0);
  textSpan->font = getAttribute(node, "font");
  textSpan->fontSize = getFloatAttribute(node, "fontSize", 12);
  textSpan->fontWeight = getIntAttribute(node, "fontWeight", 400);
  textSpan->fontStyle = FontStyleFromString(getAttribute(node, "fontStyle", "normal"));
  textSpan->tracking = getFloatAttribute(node, "tracking", 0);
  textSpan->baselineShift = getFloatAttribute(node, "baselineShift", 0);
  textSpan->text = node->text;
  return textSpan;
}

//==============================================================================
// Painter parsing
//==============================================================================

std::unique_ptr<Fill> PAGXXMLParser::parseFill(const XMLNode* node) {
  auto fill = std::make_unique<Fill>();
  fill->color = getAttribute(node, "color");
  fill->alpha = getFloatAttribute(node, "alpha", 1);
  fill->blendMode = BlendModeFromString(getAttribute(node, "blendMode", "normal"));
  fill->fillRule = FillRuleFromString(getAttribute(node, "fillRule", "winding"));
  fill->placement = PlacementFromString(getAttribute(node, "placement", "background"));

  for (const auto& child : node->children) {
    auto colorSource = parseColorSource(child.get());
    if (colorSource) {
      fill->colorSource = std::move(colorSource);
      break;
    }
  }

  return fill;
}

std::unique_ptr<Stroke> PAGXXMLParser::parseStroke(const XMLNode* node) {
  auto stroke = std::make_unique<Stroke>();
  stroke->color = getAttribute(node, "color");
  stroke->width = getFloatAttribute(node, "width", 1);
  stroke->alpha = getFloatAttribute(node, "alpha", 1);
  stroke->blendMode = BlendModeFromString(getAttribute(node, "blendMode", "normal"));
  stroke->cap = LineCapFromString(getAttribute(node, "cap", "butt"));
  stroke->join = LineJoinFromString(getAttribute(node, "join", "miter"));
  stroke->miterLimit = getFloatAttribute(node, "miterLimit", 4);
  auto dashesStr = getAttribute(node, "dashes");
  if (!dashesStr.empty()) {
    stroke->dashes = parseFloatList(dashesStr);
  }
  stroke->dashOffset = getFloatAttribute(node, "dashOffset", 0);
  stroke->align = StrokeAlignFromString(getAttribute(node, "align", "center"));
  stroke->placement = PlacementFromString(getAttribute(node, "placement", "background"));

  for (const auto& child : node->children) {
    auto colorSource = parseColorSource(child.get());
    if (colorSource) {
      stroke->colorSource = std::move(colorSource);
      break;
    }
  }

  return stroke;
}

//==============================================================================
// Modifier parsing
//==============================================================================

std::unique_ptr<TrimPath> PAGXXMLParser::parseTrimPath(const XMLNode* node) {
  auto trim = std::make_unique<TrimPath>();
  trim->start = getFloatAttribute(node, "start", 0);
  trim->end = getFloatAttribute(node, "end", 1);
  trim->offset = getFloatAttribute(node, "offset", 0);
  trim->trimType = TrimTypeFromString(getAttribute(node, "type", "separate"));
  return trim;
}

std::unique_ptr<RoundCorner> PAGXXMLParser::parseRoundCorner(const XMLNode* node) {
  auto round = std::make_unique<RoundCorner>();
  round->radius = getFloatAttribute(node, "radius", 0);
  return round;
}

std::unique_ptr<MergePath> PAGXXMLParser::parseMergePath(const XMLNode* node) {
  auto merge = std::make_unique<MergePath>();
  merge->mode = MergePathModeFromString(getAttribute(node, "mode", "append"));
  return merge;
}

std::unique_ptr<TextModifier> PAGXXMLParser::parseTextModifier(const XMLNode* node) {
  auto modifier = std::make_unique<TextModifier>();
  auto anchorStr = getAttribute(node, "anchorPoint", "0.5,0.5");
  modifier->anchorPoint = parsePoint(anchorStr);
  auto positionStr = getAttribute(node, "position", "0,0");
  modifier->position = parsePoint(positionStr);
  modifier->rotation = getFloatAttribute(node, "rotation", 0);
  auto scaleStr = getAttribute(node, "scale", "1,1");
  modifier->scale = parsePoint(scaleStr);
  modifier->skew = getFloatAttribute(node, "skew", 0);
  modifier->skewAxis = getFloatAttribute(node, "skewAxis", 0);
  modifier->alpha = getFloatAttribute(node, "alpha", 1);
  modifier->fillColor = getAttribute(node, "fillColor");
  modifier->strokeColor = getAttribute(node, "strokeColor");
  modifier->strokeWidth = getFloatAttribute(node, "strokeWidth", -1);

  for (const auto& child : node->children) {
    if (child->tag == "RangeSelector") {
      modifier->rangeSelectors.push_back(parseRangeSelector(child.get()));
    }
  }

  return modifier;
}

std::unique_ptr<TextPath> PAGXXMLParser::parseTextPath(const XMLNode* node) {
  auto textPath = std::make_unique<TextPath>();
  textPath->path = getAttribute(node, "path");
  textPath->pathAlign = TextPathAlignFromString(getAttribute(node, "align", "start"));
  textPath->firstMargin = getFloatAttribute(node, "firstMargin", 0);
  textPath->lastMargin = getFloatAttribute(node, "lastMargin", 0);
  textPath->perpendicularToPath = getBoolAttribute(node, "perpendicularToPath", true);
  textPath->reversed = getBoolAttribute(node, "reversed", false);
  textPath->forceAlignment = getBoolAttribute(node, "forceAlignment", false);
  return textPath;
}

std::unique_ptr<TextLayout> PAGXXMLParser::parseTextLayout(const XMLNode* node) {
  auto layout = std::make_unique<TextLayout>();
  layout->width = getFloatAttribute(node, "width", 0);
  layout->height = getFloatAttribute(node, "height", 0);
  layout->textAlign = TextAlignFromString(getAttribute(node, "align", "left"));
  layout->verticalAlign = VerticalAlignFromString(getAttribute(node, "verticalAlign", "top"));
  layout->lineHeight = getFloatAttribute(node, "lineHeight", 1.2f);
  layout->indent = getFloatAttribute(node, "indent", 0);
  layout->overflow = OverflowFromString(getAttribute(node, "overflow", "clip"));
  return layout;
}

std::unique_ptr<Repeater> PAGXXMLParser::parseRepeater(const XMLNode* node) {
  auto repeater = std::make_unique<Repeater>();
  repeater->copies = getFloatAttribute(node, "copies", 3);
  repeater->offset = getFloatAttribute(node, "offset", 0);
  repeater->order = RepeaterOrderFromString(getAttribute(node, "order", "belowOriginal"));
  auto anchorStr = getAttribute(node, "anchorPoint", "0,0");
  repeater->anchorPoint = parsePoint(anchorStr);
  auto positionStr = getAttribute(node, "position", "100,100");
  repeater->position = parsePoint(positionStr);
  repeater->rotation = getFloatAttribute(node, "rotation", 0);
  auto scaleStr = getAttribute(node, "scale", "1,1");
  repeater->scale = parsePoint(scaleStr);
  repeater->startAlpha = getFloatAttribute(node, "startAlpha", 1);
  repeater->endAlpha = getFloatAttribute(node, "endAlpha", 1);
  return repeater;
}

std::unique_ptr<Group> PAGXXMLParser::parseGroup(const XMLNode* node) {
  auto group = std::make_unique<Group>();
  // group->name (removed) = getAttribute(node, "name");
  auto anchorStr = getAttribute(node, "anchorPoint", "0,0");
  group->anchorPoint = parsePoint(anchorStr);
  auto positionStr = getAttribute(node, "position", "0,0");
  group->position = parsePoint(positionStr);
  group->rotation = getFloatAttribute(node, "rotation", 0);
  auto scaleStr = getAttribute(node, "scale", "1,1");
  group->scale = parsePoint(scaleStr);
  group->skew = getFloatAttribute(node, "skew", 0);
  group->skewAxis = getFloatAttribute(node, "skewAxis", 0);
  group->alpha = getFloatAttribute(node, "alpha", 1);

  for (const auto& child : node->children) {
    auto element = parseElement(child.get());
    if (element) {
      group->elements.push_back(std::move(element));
    }
  }

  return group;
}

RangeSelector PAGXXMLParser::parseRangeSelector(const XMLNode* node) {
  RangeSelector selector = {};
  selector.start = getFloatAttribute(node, "start", 0);
  selector.end = getFloatAttribute(node, "end", 1);
  selector.offset = getFloatAttribute(node, "offset", 0);
  selector.unit = SelectorUnitFromString(getAttribute(node, "unit", "percentage"));
  selector.shape = SelectorShapeFromString(getAttribute(node, "shape", "square"));
  selector.easeIn = getFloatAttribute(node, "easeIn", 0);
  selector.easeOut = getFloatAttribute(node, "easeOut", 0);
  selector.mode = SelectorModeFromString(getAttribute(node, "mode", "add"));
  selector.weight = getFloatAttribute(node, "weight", 1);
  selector.randomizeOrder = getBoolAttribute(node, "randomizeOrder", false);
  selector.randomSeed = getIntAttribute(node, "randomSeed", 0);
  return selector;
}

//==============================================================================
// Color source parsing
//==============================================================================

std::unique_ptr<SolidColor> PAGXXMLParser::parseSolidColor(const XMLNode* node) {
  auto solid = std::make_unique<SolidColor>();
  solid->id = getAttribute(node, "id");
  auto colorStr = getAttribute(node, "color");
  if (!colorStr.empty()) {
    solid->color = Color::Parse(colorStr);
  }
  return solid;
}

std::unique_ptr<LinearGradient> PAGXXMLParser::parseLinearGradient(const XMLNode* node) {
  auto gradient = std::make_unique<LinearGradient>();
  gradient->id = getAttribute(node, "id");
  auto startPointStr = getAttribute(node, "startPoint", "0,0");
  gradient->startPoint = parsePoint(startPointStr);
  auto endPointStr = getAttribute(node, "endPoint", "0,0");
  gradient->endPoint = parsePoint(endPointStr);
  auto matrixStr = getAttribute(node, "matrix");
  if (!matrixStr.empty()) {
    gradient->matrix = Matrix::Parse(matrixStr);
  }
  for (const auto& child : node->children) {
    if (child->tag == "ColorStop") {
      gradient->colorStops.push_back(parseColorStop(child.get()));
    }
  }
  return gradient;
}

std::unique_ptr<RadialGradient> PAGXXMLParser::parseRadialGradient(const XMLNode* node) {
  auto gradient = std::make_unique<RadialGradient>();
  gradient->id = getAttribute(node, "id");
  auto centerStr = getAttribute(node, "center", "0,0");
  gradient->center = parsePoint(centerStr);
  gradient->radius = getFloatAttribute(node, "radius", 0);
  auto matrixStr = getAttribute(node, "matrix");
  if (!matrixStr.empty()) {
    gradient->matrix = Matrix::Parse(matrixStr);
  }
  for (const auto& child : node->children) {
    if (child->tag == "ColorStop") {
      gradient->colorStops.push_back(parseColorStop(child.get()));
    }
  }
  return gradient;
}

std::unique_ptr<ConicGradient> PAGXXMLParser::parseConicGradient(const XMLNode* node) {
  auto gradient = std::make_unique<ConicGradient>();
  gradient->id = getAttribute(node, "id");
  auto centerStr = getAttribute(node, "center", "0,0");
  gradient->center = parsePoint(centerStr);
  gradient->startAngle = getFloatAttribute(node, "startAngle", 0);
  gradient->endAngle = getFloatAttribute(node, "endAngle", 360);
  auto matrixStr = getAttribute(node, "matrix");
  if (!matrixStr.empty()) {
    gradient->matrix = Matrix::Parse(matrixStr);
  }
  for (const auto& child : node->children) {
    if (child->tag == "ColorStop") {
      gradient->colorStops.push_back(parseColorStop(child.get()));
    }
  }
  return gradient;
}

std::unique_ptr<DiamondGradient> PAGXXMLParser::parseDiamondGradient(const XMLNode* node) {
  auto gradient = std::make_unique<DiamondGradient>();
  gradient->id = getAttribute(node, "id");
  auto centerStr = getAttribute(node, "center", "0,0");
  gradient->center = parsePoint(centerStr);
  gradient->halfDiagonal = getFloatAttribute(node, "halfDiagonal", 0);
  auto matrixStr = getAttribute(node, "matrix");
  if (!matrixStr.empty()) {
    gradient->matrix = Matrix::Parse(matrixStr);
  }
  for (const auto& child : node->children) {
    if (child->tag == "ColorStop") {
      gradient->colorStops.push_back(parseColorStop(child.get()));
    }
  }
  return gradient;
}

std::unique_ptr<ImagePattern> PAGXXMLParser::parseImagePattern(const XMLNode* node) {
  auto pattern = std::make_unique<ImagePattern>();
  pattern->id = getAttribute(node, "id");
  pattern->image = getAttribute(node, "image");
  pattern->tileModeX = TileModeFromString(getAttribute(node, "tileModeX", "clamp"));
  pattern->tileModeY = TileModeFromString(getAttribute(node, "tileModeY", "clamp"));
  pattern->sampling = SamplingModeFromString(getAttribute(node, "sampling", "linear"));
  auto matrixStr = getAttribute(node, "matrix");
  if (!matrixStr.empty()) {
    pattern->matrix = Matrix::Parse(matrixStr);
  }
  return pattern;
}

ColorStop PAGXXMLParser::parseColorStop(const XMLNode* node) {
  ColorStop stop = {};
  stop.offset = getFloatAttribute(node, "offset", 0);
  auto colorStr = getAttribute(node, "color");
  if (!colorStr.empty()) {
    stop.color = Color::Parse(colorStr);
  }
  return stop;
}

//==============================================================================
// Resource parsing
//==============================================================================

std::unique_ptr<Image> PAGXXMLParser::parseImage(const XMLNode* node) {
  auto image = std::make_unique<Image>();
  image->id = getAttribute(node, "id");
  image->source = getAttribute(node, "source");
  return image;
}

std::unique_ptr<PathDataResource> PAGXXMLParser::parsePathData(const XMLNode* node) {
  auto pathData = std::make_unique<PathDataResource>();
  pathData->id = getAttribute(node, "id");
  pathData->data = getAttribute(node, "data");
  return pathData;
}

std::unique_ptr<Composition> PAGXXMLParser::parseComposition(const XMLNode* node) {
  auto comp = std::make_unique<Composition>();
  comp->id = getAttribute(node, "id");
  comp->width = getFloatAttribute(node, "width", 0);
  comp->height = getFloatAttribute(node, "height", 0);
  for (const auto& child : node->children) {
    if (child->tag == "Layer") {
      auto layer = parseLayer(child.get());
      if (layer) {
        comp->layers.push_back(std::move(layer));
      }
    }
  }
  return comp;
}

//==============================================================================
// Layer style parsing
//==============================================================================

std::unique_ptr<DropShadowStyle> PAGXXMLParser::parseDropShadowStyle(const XMLNode* node) {
  auto style = std::make_unique<DropShadowStyle>();
  style->blendMode = BlendModeFromString(getAttribute(node, "blendMode", "normal"));
  style->offsetX = getFloatAttribute(node, "offsetX", 0);
  style->offsetY = getFloatAttribute(node, "offsetY", 0);
  style->blurrinessX = getFloatAttribute(node, "blurrinessX", 0);
  style->blurrinessY = getFloatAttribute(node, "blurrinessY", 0);
  auto colorStr = getAttribute(node, "color");
  if (!colorStr.empty()) {
    style->color = Color::Parse(colorStr);
  }
  style->showBehindLayer = getBoolAttribute(node, "showBehindLayer", true);
  return style;
}

std::unique_ptr<InnerShadowStyle> PAGXXMLParser::parseInnerShadowStyle(const XMLNode* node) {
  auto style = std::make_unique<InnerShadowStyle>();
  style->blendMode = BlendModeFromString(getAttribute(node, "blendMode", "normal"));
  style->offsetX = getFloatAttribute(node, "offsetX", 0);
  style->offsetY = getFloatAttribute(node, "offsetY", 0);
  style->blurrinessX = getFloatAttribute(node, "blurrinessX", 0);
  style->blurrinessY = getFloatAttribute(node, "blurrinessY", 0);
  auto colorStr = getAttribute(node, "color");
  if (!colorStr.empty()) {
    style->color = Color::Parse(colorStr);
  }
  return style;
}

std::unique_ptr<BackgroundBlurStyle> PAGXXMLParser::parseBackgroundBlurStyle(
    const XMLNode* node) {
  auto style = std::make_unique<BackgroundBlurStyle>();
  style->blendMode = BlendModeFromString(getAttribute(node, "blendMode", "normal"));
  style->blurrinessX = getFloatAttribute(node, "blurrinessX", 0);
  style->blurrinessY = getFloatAttribute(node, "blurrinessY", 0);
  style->tileMode = TileModeFromString(getAttribute(node, "tileMode", "mirror"));
  return style;
}

//==============================================================================
// Layer filter parsing
//==============================================================================

std::unique_ptr<BlurFilter> PAGXXMLParser::parseBlurFilter(const XMLNode* node) {
  auto filter = std::make_unique<BlurFilter>();
  filter->blurrinessX = getFloatAttribute(node, "blurrinessX", 0);
  filter->blurrinessY = getFloatAttribute(node, "blurrinessY", 0);
  filter->tileMode = TileModeFromString(getAttribute(node, "tileMode", "decal"));
  return filter;
}

std::unique_ptr<DropShadowFilter> PAGXXMLParser::parseDropShadowFilter(const XMLNode* node) {
  auto filter = std::make_unique<DropShadowFilter>();
  filter->offsetX = getFloatAttribute(node, "offsetX", 0);
  filter->offsetY = getFloatAttribute(node, "offsetY", 0);
  filter->blurrinessX = getFloatAttribute(node, "blurrinessX", 0);
  filter->blurrinessY = getFloatAttribute(node, "blurrinessY", 0);
  auto colorStr = getAttribute(node, "color");
  if (!colorStr.empty()) {
    filter->color = Color::Parse(colorStr);
  }
  filter->shadowOnly = getBoolAttribute(node, "shadowOnly", false);
  return filter;
}

std::unique_ptr<InnerShadowFilter> PAGXXMLParser::parseInnerShadowFilter(const XMLNode* node) {
  auto filter = std::make_unique<InnerShadowFilter>();
  filter->offsetX = getFloatAttribute(node, "offsetX", 0);
  filter->offsetY = getFloatAttribute(node, "offsetY", 0);
  filter->blurrinessX = getFloatAttribute(node, "blurrinessX", 0);
  filter->blurrinessY = getFloatAttribute(node, "blurrinessY", 0);
  auto colorStr = getAttribute(node, "color");
  if (!colorStr.empty()) {
    filter->color = Color::Parse(colorStr);
  }
  filter->shadowOnly = getBoolAttribute(node, "shadowOnly", false);
  return filter;
}

std::unique_ptr<BlendFilter> PAGXXMLParser::parseBlendFilter(const XMLNode* node) {
  auto filter = std::make_unique<BlendFilter>();
  auto colorStr = getAttribute(node, "color");
  if (!colorStr.empty()) {
    filter->color = Color::Parse(colorStr);
  }
  filter->blendMode = BlendModeFromString(getAttribute(node, "blendMode", "normal"));
  return filter;
}

std::unique_ptr<ColorMatrixFilter> PAGXXMLParser::parseColorMatrixFilter(const XMLNode* node) {
  auto filter = std::make_unique<ColorMatrixFilter>();
  auto matrixStr = getAttribute(node, "matrix");
  if (!matrixStr.empty()) {
    auto values = parseFloatList(matrixStr);
    for (size_t i = 0; i < std::min(values.size(), size_t(20)); i++) {
      filter->matrix[i] = values[i];
    }
  }
  return filter;
}

//==============================================================================
// Utility functions
//==============================================================================

std::string PAGXXMLParser::getAttribute(const XMLNode* node, const std::string& name,
                                        const std::string& defaultValue) {
  auto it = node->attributes.find(name);
  return it != node->attributes.end() ? it->second : defaultValue;
}

float PAGXXMLParser::getFloatAttribute(const XMLNode* node, const std::string& name,
                                       float defaultValue) {
  auto str = getAttribute(node, name);
  if (str.empty()) {
    return defaultValue;
  }
  try {
    return std::stof(str);
  } catch (...) {
    return defaultValue;
  }
}

int PAGXXMLParser::getIntAttribute(const XMLNode* node, const std::string& name, int defaultValue) {
  auto str = getAttribute(node, name);
  if (str.empty()) {
    return defaultValue;
  }
  try {
    return std::stoi(str);
  } catch (...) {
    return defaultValue;
  }
}

bool PAGXXMLParser::getBoolAttribute(const XMLNode* node, const std::string& name,
                                     bool defaultValue) {
  auto str = getAttribute(node, name);
  if (str.empty()) {
    return defaultValue;
  }
  return str == "true" || str == "1";
}

Point PAGXXMLParser::parsePoint(const std::string& str) {
  Point point = {};
  std::istringstream iss(str);
  std::string token = {};
  std::vector<float> values = {};
  while (std::getline(iss, token, ',')) {
    auto trimmed = token;
    trimmed.erase(0, trimmed.find_first_not_of(" \t"));
    trimmed.erase(trimmed.find_last_not_of(" \t") + 1);
    if (!trimmed.empty()) {
      values.push_back(std::stof(trimmed));
    }
  }
  if (values.size() >= 2) {
    point.x = values[0];
    point.y = values[1];
  }
  return point;
}

Size PAGXXMLParser::parseSize(const std::string& str) {
  Size size = {};
  std::istringstream iss(str);
  std::string token = {};
  std::vector<float> values = {};
  while (std::getline(iss, token, ',')) {
    auto trimmed = token;
    trimmed.erase(0, trimmed.find_first_not_of(" \t"));
    trimmed.erase(trimmed.find_last_not_of(" \t") + 1);
    if (!trimmed.empty()) {
      values.push_back(std::stof(trimmed));
    }
  }
  if (values.size() >= 2) {
    size.width = values[0];
    size.height = values[1];
  }
  return size;
}

Rect PAGXXMLParser::parseRect(const std::string& str) {
  Rect rect = {};
  std::istringstream iss(str);
  std::string token = {};
  std::vector<float> values = {};
  while (std::getline(iss, token, ',')) {
    auto trimmed = token;
    trimmed.erase(0, trimmed.find_first_not_of(" \t"));
    trimmed.erase(trimmed.find_last_not_of(" \t") + 1);
    if (!trimmed.empty()) {
      values.push_back(std::stof(trimmed));
    }
  }
  if (values.size() >= 4) {
    rect.x = values[0];
    rect.y = values[1];
    rect.width = values[2];
    rect.height = values[3];
  }
  return rect;
}

std::vector<float> PAGXXMLParser::parseFloatList(const std::string& str) {
  std::vector<float> values = {};
  std::istringstream iss(str);
  std::string token = {};
  while (std::getline(iss, token, ',')) {
    auto trimmed = token;
    trimmed.erase(0, trimmed.find_first_not_of(" \t"));
    trimmed.erase(trimmed.find_last_not_of(" \t") + 1);
    if (!trimmed.empty()) {
      try {
        values.push_back(std::stof(trimmed));
      } catch (...) {
      }
    }
  }
  return values;
}

}  // namespace pagx
