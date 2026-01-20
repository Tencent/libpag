/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except
//  in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "PAGXParser.h"
#include "PAGXAttributes.h"
#include "PAGXUtils.h"
#include "pagx/layers/TextLayouter.h"
#include "tgfx/core/Data.h"
#include "tgfx/core/Font.h"
#include "tgfx/core/Typeface.h"
#include "tgfx/layers/VectorLayer.h"
#include "tgfx/layers/filters/BlurFilter.h"
#include "tgfx/layers/filters/DropShadowFilter.h"
#include "tgfx/layers/layerstyles/DropShadowStyle.h"
#include "tgfx/layers/layerstyles/InnerShadowStyle.h"
#include "tgfx/layers/vectors/Ellipse.h"
#include "tgfx/layers/vectors/FillStyle.h"
#include "tgfx/layers/vectors/Gradient.h"
#include "tgfx/layers/vectors/ImagePattern.h"
#include "tgfx/layers/vectors/MergePath.h"
#include "tgfx/layers/vectors/Polystar.h"
#include "tgfx/layers/vectors/Rectangle.h"
#include "tgfx/layers/vectors/Repeater.h"
#include "tgfx/layers/vectors/RoundCorner.h"
#include "tgfx/layers/vectors/ShapePath.h"
#include "tgfx/layers/vectors/SolidColor.h"
#include "tgfx/layers/vectors/StrokeStyle.h"
#include "tgfx/layers/vectors/TextSpan.h"
#include "tgfx/layers/vectors/TrimPath.h"
#include "tgfx/layers/vectors/VectorGroup.h"
#include "tgfx/svg/SVGPathParser.h"

namespace pagx {

PAGXParser::PAGXParser(std::shared_ptr<DOMNode> rootNode, const std::string& basePath)
    : rootNode_(std::move(rootNode)), basePath_(basePath) {
}

bool PAGXParser::parse() {
  if (!rootNode_ || rootNode_->name != "pagx") {
    return false;
  }
  width_ = PAGXAttributes::ParseFloat(rootNode_, "width", 0.0f);
  height_ = PAGXAttributes::ParseFloat(rootNode_, "height", 0.0f);
  if (width_ <= 0 || height_ <= 0) {
    return false;
  }

  rootLayer = Layer::Make();
  rootLayer->setName("root");

  auto child = rootNode_->firstChild;
  while (child) {
    if (child->name == "Resources") {
      parseResources(child);
    } else if (child->name == "Layer") {
      auto layer = parseLayer(child);
      if (layer) {
        rootLayer->addChild(layer);
      }
    }
    child = child->nextSibling;
  }
  return true;
}

void PAGXParser::parseResources(const std::shared_ptr<DOMNode>& node) {
  auto child = node->firstChild;
  while (child) {
    if (child->name == "Image") {
      auto [found, id] = child->findAttribute("id");
      auto [srcFound, source] = child->findAttribute("source");
      if (found && srcFound && !id.empty()) {
        auto image = resolveImageReference(source);
        if (image) {
          images[id] = image;
        }
      }
    } else if (child->name == "SolidColor" || child->name == "LinearGradient" ||
               child->name == "RadialGradient" || child->name == "ConicGradient" ||
               child->name == "DiamondGradient" || child->name == "ImagePattern") {
      auto [found, id] = child->findAttribute("id");
      if (found && !id.empty()) {
        auto colorSource = parseColorSource(child);
        if (colorSource) {
          colorSources[id] = colorSource;
        }
      }
    }
    child = child->nextSibling;
  }
}

std::shared_ptr<Layer> PAGXParser::parseLayer(const std::shared_ptr<DOMNode>& node) {
  auto [hasComposition, composition] = node->findAttribute("composition");
  if (hasComposition) {
    // Composition reference is not supported in this version
    return nullptr;
  }

  auto layer = Layer::Make();

  // Parse layer attributes
  auto name = PAGXAttributes::ParseString(node, "name", "");
  layer->setName(name);

  auto visible = PAGXAttributes::ParseBool(node, "visible", true);
  layer->setVisible(visible);

  auto alpha = PAGXAttributes::ParseFloat(node, "alpha", 1.0f);
  layer->setAlpha(alpha);

  auto blendModeStr = PAGXAttributes::ParseString(node, "blendMode", "normal");
  layer->setBlendMode(PAGXAttributes::ParseBlendMode(blendModeStr));

  auto x = PAGXAttributes::ParseFloat(node, "x", 0.0f);
  auto y = PAGXAttributes::ParseFloat(node, "y", 0.0f);
  layer->setPosition({x, y});

  auto matrixStr = PAGXAttributes::ParseString(node, "matrix", "");
  if (!matrixStr.empty()) {
    layer->setMatrix(PAGXAttributes::ParseMatrix(matrixStr));
  }

  auto antiAlias = PAGXAttributes::ParseBool(node, "antiAlias", true);
  layer->setAllowsEdgeAntialiasing(antiAlias);

  auto groupOpacity = PAGXAttributes::ParseBool(node, "groupOpacity", false);
  layer->setAllowsGroupOpacity(groupOpacity);

  // Parse id for reference
  auto [hasId, id] = node->findAttribute("id");
  if (hasId && !id.empty()) {
    layerIdMap[id] = layer;
  }

  // Parse children nodes
  std::vector<std::shared_ptr<VectorElement>> contents;
  std::vector<std::shared_ptr<LayerFilter>> filters;
  std::vector<std::shared_ptr<LayerStyle>> styles;
  std::string maskRef;
  std::string maskTypeStr;

  auto child = node->firstChild;
  while (child) {
    if (child->name == "contents") {
      contents = parseContents(child);
    } else if (child->name == "filters") {
      filters = parseFilters(child);
    } else if (child->name == "styles") {
      styles = parseStyles(child);
    } else if (child->name == "Layer") {
      auto childLayer = parseLayer(child);
      if (childLayer) {
        layer->addChild(childLayer);
      }
    }
    child = child->nextSibling;
  }

  // Apply contents to VectorLayer if present
  if (!contents.empty()) {
    auto vectorLayer = VectorLayer::Make();
    vectorLayer->setContents(std::move(contents));
    layer->addChildAt(vectorLayer, 0);
  }

  // Apply filters and styles
  if (!filters.empty()) {
    layer->setFilters(filters);
  }
  if (!styles.empty()) {
    layer->setLayerStyles(styles);
  }

  // Handle mask reference
  auto [hasMask, mask] = node->findAttribute("mask");
  if (hasMask && !mask.empty() && mask[0] == '#') {
    maskRef = mask.substr(1);
    auto [hasMaskType, maskType] = node->findAttribute("maskType");
    maskTypeStr = hasMaskType ? maskType : "alpha";
  }

  // Mask will be resolved later in a second pass
  if (!maskRef.empty()) {
    auto it = layerIdMap.find(maskRef);
    if (it != layerIdMap.end()) {
      layer->setMask(it->second);
      layer->setMaskType(PAGXAttributes::ParseMaskType(maskTypeStr));
    }
  }

  return layer;
}

std::vector<std::shared_ptr<VectorElement>> PAGXParser::parseContents(
    const std::shared_ptr<DOMNode>& node) {
  std::vector<std::shared_ptr<VectorElement>> elements;
  auto child = node->firstChild;
  while (child) {
    auto element = parseVectorElement(child);
    if (element) {
      elements.push_back(element);
    }
    child = child->nextSibling;
  }
  return elements;
}

std::shared_ptr<VectorElement> PAGXParser::parseVectorElement(
    const std::shared_ptr<DOMNode>& node) {
  const auto& name = node->name;
  if (name == "Group") {
    return parseGroup(node);
  }
  if (name == "Rectangle") {
    return parseRectangle(node);
  }
  if (name == "Ellipse") {
    return parseEllipse(node);
  }
  if (name == "Polystar") {
    return parsePolystar(node);
  }
  if (name == "Path") {
    return parsePath(node);
  }
  if (name == "TextSpan") {
    return parseTextSpan(node);
  }
  if (name == "Fill") {
    return parseFill(node);
  }
  if (name == "Stroke") {
    return parseStroke(node);
  }
  if (name == "TrimPath") {
    return parseTrimPath(node);
  }
  if (name == "RoundCorner") {
    return parseRoundCorner(node);
  }
  if (name == "MergePath") {
    return parseMergePath(node);
  }
  if (name == "Repeater") {
    return parseRepeater(node);
  }
  return nullptr;
}

std::shared_ptr<VectorGroup> PAGXParser::parseGroup(const std::shared_ptr<DOMNode>& node) {
  auto group = std::make_shared<VectorGroup>();

  auto anchorStr = PAGXAttributes::ParseString(node, "anchor", "0,0");
  group->setAnchorPoint(PAGXAttributes::ParsePoint(anchorStr));

  auto positionStr = PAGXAttributes::ParseString(node, "position", "0,0");
  group->setPosition(PAGXAttributes::ParsePoint(positionStr));

  auto scaleStr = PAGXAttributes::ParseString(node, "scale", "1,1");
  group->setScale(PAGXAttributes::ParsePoint(scaleStr, {1.0f, 1.0f}));

  group->setRotation(PAGXAttributes::ParseFloat(node, "rotation", 0.0f));
  group->setAlpha(PAGXAttributes::ParseFloat(node, "alpha", 1.0f));
  group->setSkew(PAGXAttributes::ParseFloat(node, "skew", 0.0f));
  group->setSkewAxis(PAGXAttributes::ParseFloat(node, "skewAxis", 0.0f));

  std::vector<std::shared_ptr<VectorElement>> elements;
  auto child = node->firstChild;
  while (child) {
    auto element = parseVectorElement(child);
    if (element) {
      elements.push_back(element);
    }
    child = child->nextSibling;
  }
  group->setElements(std::move(elements));
  return group;
}

std::shared_ptr<Rectangle> PAGXParser::parseRectangle(const std::shared_ptr<DOMNode>& node) {
  auto rect = std::make_shared<Rectangle>();
  auto centerX = PAGXAttributes::ParseFloat(node, "centerX", 0.0f);
  auto centerY = PAGXAttributes::ParseFloat(node, "centerY", 0.0f);
  rect->setCenter({centerX, centerY});
  auto width = PAGXAttributes::ParseFloat(node, "width", 100.0f);
  auto height = PAGXAttributes::ParseFloat(node, "height", 100.0f);
  rect->setSize({width, height});
  rect->setRoundness(PAGXAttributes::ParseFloat(node, "roundness", 0.0f));
  rect->setReversed(PAGXAttributes::ParseBool(node, "reversed", false));
  return rect;
}

std::shared_ptr<Ellipse> PAGXParser::parseEllipse(const std::shared_ptr<DOMNode>& node) {
  auto ellipse = std::make_shared<Ellipse>();
  auto centerX = PAGXAttributes::ParseFloat(node, "centerX", 0.0f);
  auto centerY = PAGXAttributes::ParseFloat(node, "centerY", 0.0f);
  ellipse->setCenter({centerX, centerY});
  auto width = PAGXAttributes::ParseFloat(node, "width", 100.0f);
  auto height = PAGXAttributes::ParseFloat(node, "height", 100.0f);
  ellipse->setSize({width, height});
  ellipse->setReversed(PAGXAttributes::ParseBool(node, "reversed", false));
  return ellipse;
}

std::shared_ptr<Polystar> PAGXParser::parsePolystar(const std::shared_ptr<DOMNode>& node) {
  auto polystar = std::make_shared<Polystar>();
  auto centerX = PAGXAttributes::ParseFloat(node, "centerX", 0.0f);
  auto centerY = PAGXAttributes::ParseFloat(node, "centerY", 0.0f);
  polystar->setCenter({centerX, centerY});
  auto typeStr = PAGXAttributes::ParseString(node, "type", "star");
  polystar->setPolystarType(PAGXAttributes::ParsePolystarType(typeStr));
  polystar->setPointCount(PAGXAttributes::ParseFloat(node, "points", 5.0f));
  polystar->setOuterRadius(PAGXAttributes::ParseFloat(node, "outerRadius", 100.0f));
  polystar->setInnerRadius(PAGXAttributes::ParseFloat(node, "innerRadius", 50.0f));
  polystar->setRotation(PAGXAttributes::ParseFloat(node, "rotation", 0.0f));
  polystar->setOuterRoundness(PAGXAttributes::ParseFloat(node, "outerRoundness", 0.0f));
  polystar->setInnerRoundness(PAGXAttributes::ParseFloat(node, "innerRoundness", 0.0f));
  polystar->setReversed(PAGXAttributes::ParseBool(node, "reversed", false));
  return polystar;
}

std::shared_ptr<ShapePath> PAGXParser::parsePath(const std::shared_ptr<DOMNode>& node) {
  auto shapePath = std::make_shared<ShapePath>();
  auto d = PAGXAttributes::ParseString(node, "d", "");
  if (!d.empty()) {
    auto path = SVGPathParser::FromSVGString(d);
    if (path) {
      shapePath->setPath(*path);
    }
  }
  shapePath->setReversed(PAGXAttributes::ParseBool(node, "reversed", false));
  return shapePath;
}

std::shared_ptr<TextSpan> PAGXParser::parseTextSpan(const std::shared_ptr<DOMNode>& node) {
  auto textSpan = std::make_shared<TextSpan>();
  auto x = PAGXAttributes::ParseFloat(node, "x", 0.0f);
  auto y = PAGXAttributes::ParseFloat(node, "y", 0.0f);

  auto textContent = PAGXAttributes::GetTextContent(node);
  auto fontFamily = PAGXAttributes::ParseString(node, "font", "");
  auto fontSize = PAGXAttributes::ParseFloat(node, "fontSize", 12.0f);

  // Try to create typeface from font name
  auto typeface = fontFamily.empty() ? nullptr : Typeface::MakeFromName(fontFamily, "");

  Font font(typeface, fontSize);
  auto textBlob = TextLayouter::Layout(textContent, font);
  textSpan->setTextBlob(textBlob);

  // Parse text anchor and adjust position accordingly
  auto textAnchorStr = PAGXAttributes::ParseString(node, "textAnchor", "start");
  if (textBlob && textAnchorStr != "start") {
    auto bounds = textBlob->getTightBounds();
    if (textAnchorStr == "middle") {
      x -= bounds.width() * 0.5f;
    } else if (textAnchorStr == "end") {
      x -= bounds.width();
    }
  }

  textSpan->setPosition({x, y});

  return textSpan;
}

std::shared_ptr<FillStyle> PAGXParser::parseFill(const std::shared_ptr<DOMNode>& node) {
  auto fill = std::make_shared<FillStyle>();

  // Try to get color from attribute
  auto colorStr = PAGXAttributes::ParseString(node, "color", "");
  if (!colorStr.empty()) {
    if (colorStr[0] == '#' && colorStr.length() > 1 && !std::isxdigit(colorStr[1])) {
      // Reference to color source: #gradientId
      fill->setColorSource(resolveColorReference(colorStr));
    } else {
      // Direct color value
      fill->setColorSource(SolidColor::Make(PAGXAttributes::ParseColor(colorStr)));
    }
  } else {
    // Try inline color source
    auto inlineColor = parseInlineColorSource(node);
    if (inlineColor) {
      fill->setColorSource(inlineColor);
    }
  }

  fill->setAlpha(PAGXAttributes::ParseFloat(node, "alpha", 1.0f));
  auto blendModeStr = PAGXAttributes::ParseString(node, "blendMode", "normal");
  fill->setBlendMode(PAGXAttributes::ParseBlendMode(blendModeStr));
  auto fillRuleStr = PAGXAttributes::ParseString(node, "fillRule", "winding");
  fill->setFillRule(fillRuleStr == "evenOdd" ? FillRule::EvenOdd : FillRule::Winding);

  return fill;
}

std::shared_ptr<StrokeStyle> PAGXParser::parseStroke(const std::shared_ptr<DOMNode>& node) {
  auto stroke = std::make_shared<StrokeStyle>();

  auto colorStr = PAGXAttributes::ParseString(node, "color", "");
  if (!colorStr.empty()) {
    if (colorStr[0] == '#' && colorStr.length() > 1 && !std::isxdigit(colorStr[1])) {
      stroke->setColorSource(resolveColorReference(colorStr));
    } else {
      stroke->setColorSource(SolidColor::Make(PAGXAttributes::ParseColor(colorStr)));
    }
  } else {
    auto inlineColor = parseInlineColorSource(node);
    if (inlineColor) {
      stroke->setColorSource(inlineColor);
    }
  }

  stroke->setStrokeWidth(PAGXAttributes::ParseFloat(node, "width", 1.0f));
  stroke->setAlpha(PAGXAttributes::ParseFloat(node, "alpha", 1.0f));
  auto blendModeStr = PAGXAttributes::ParseString(node, "blendMode", "normal");
  stroke->setBlendMode(PAGXAttributes::ParseBlendMode(blendModeStr));

  auto capStr = PAGXAttributes::ParseString(node, "cap", "butt");
  stroke->setLineCap(PAGXAttributes::ParseLineCap(capStr));

  auto joinStr = PAGXAttributes::ParseString(node, "join", "miter");
  stroke->setLineJoin(PAGXAttributes::ParseLineJoin(joinStr));

  stroke->setMiterLimit(PAGXAttributes::ParseFloat(node, "miterLimit", 4.0f));

  auto dashesStr = PAGXAttributes::ParseString(node, "dashes", "");
  if (!dashesStr.empty()) {
    stroke->setDashes(PAGXAttributes::ParseDashes(dashesStr));
  }
  stroke->setDashOffset(PAGXAttributes::ParseFloat(node, "dashOffset", 0.0f));

  auto alignStr = PAGXAttributes::ParseString(node, "align", "center");
  stroke->setStrokeAlign(PAGXAttributes::ParseStrokeAlign(alignStr));

  return stroke;
}

std::shared_ptr<TrimPath> PAGXParser::parseTrimPath(const std::shared_ptr<DOMNode>& node) {
  auto trim = std::make_shared<TrimPath>();
  trim->setStart(PAGXAttributes::ParseFloat(node, "start", 0.0f));
  trim->setEnd(PAGXAttributes::ParseFloat(node, "end", 1.0f));
  trim->setOffset(PAGXAttributes::ParseFloat(node, "offset", 0.0f));
  auto typeStr = PAGXAttributes::ParseString(node, "type", "separate");
  trim->setTrimType(PAGXAttributes::ParseTrimPathType(typeStr));
  return trim;
}

std::shared_ptr<RoundCorner> PAGXParser::parseRoundCorner(const std::shared_ptr<DOMNode>& node) {
  auto round = std::make_shared<RoundCorner>();
  round->setRadius(PAGXAttributes::ParseFloat(node, "radius", 10.0f));
  return round;
}

std::shared_ptr<MergePath> PAGXParser::parseMergePath(const std::shared_ptr<DOMNode>& node) {
  auto merge = std::make_shared<MergePath>();
  auto opStr = PAGXAttributes::ParseString(node, "op", "append");
  merge->setMode(PAGXAttributes::ParseMergePathOp(opStr));
  return merge;
}

std::shared_ptr<Repeater> PAGXParser::parseRepeater(const std::shared_ptr<DOMNode>& node) {
  auto repeater = std::make_shared<Repeater>();
  repeater->setCopies(PAGXAttributes::ParseFloat(node, "copies", 3.0f));
  repeater->setOffset(PAGXAttributes::ParseFloat(node, "offset", 0.0f));

  auto orderStr = PAGXAttributes::ParseString(node, "order", "belowOriginal");
  repeater->setOrder(orderStr == "aboveOriginal" ? RepeaterOrder::AboveOriginal
                                                 : RepeaterOrder::BelowOriginal);

  auto anchorStr = PAGXAttributes::ParseString(node, "anchor", "0,0");
  repeater->setAnchorPoint(PAGXAttributes::ParsePoint(anchorStr));

  auto positionStr = PAGXAttributes::ParseString(node, "position", "100,100");
  repeater->setPosition(PAGXAttributes::ParsePoint(positionStr, {100.0f, 100.0f}));

  repeater->setRotation(PAGXAttributes::ParseFloat(node, "rotation", 0.0f));

  auto scaleStr = PAGXAttributes::ParseString(node, "scale", "1,1");
  repeater->setScale(PAGXAttributes::ParsePoint(scaleStr, {1.0f, 1.0f}));

  repeater->setStartAlpha(PAGXAttributes::ParseFloat(node, "startAlpha", 1.0f));
  repeater->setEndAlpha(PAGXAttributes::ParseFloat(node, "endAlpha", 1.0f));
  return repeater;
}

std::shared_ptr<ColorSource> PAGXParser::parseColorSource(const std::shared_ptr<DOMNode>& node) {
  const auto& name = node->name;
  if (name == "SolidColor") {
    auto colorStr = PAGXAttributes::ParseString(node, "color", "#000000");
    return SolidColor::Make(PAGXAttributes::ParseColor(colorStr));
  }
  if (name == "LinearGradient") {
    return parseLinearGradient(node);
  }
  if (name == "RadialGradient") {
    return parseRadialGradient(node);
  }
  if (name == "ConicGradient") {
    return parseConicGradient(node);
  }
  if (name == "DiamondGradient") {
    return parseDiamondGradient(node);
  }
  if (name == "ImagePattern") {
    return parseImagePattern(node);
  }
  return nullptr;
}

std::shared_ptr<ColorSource> PAGXParser::parseInlineColorSource(
    const std::shared_ptr<DOMNode>& parentNode) {
  auto child = parentNode->firstChild;
  while (child) {
    if (child->type == DOMNodeType::Element) {
      auto source = parseColorSource(child);
      if (source) {
        return source;
      }
    }
    child = child->nextSibling;
  }
  return nullptr;
}

std::vector<std::pair<float, Color>> PAGXParser::parseColorStops(
    const std::shared_ptr<DOMNode>& node) {
  std::vector<std::pair<float, Color>> stops;
  auto child = node->firstChild;
  while (child) {
    if (child->name == "ColorStop") {
      auto offset = PAGXAttributes::ParseFloat(child, "offset", 0.0f);
      auto colorStr = PAGXAttributes::ParseString(child, "color", "#000000");
      stops.emplace_back(offset, PAGXAttributes::ParseColor(colorStr));
    }
    child = child->nextSibling;
  }
  return stops;
}

std::shared_ptr<LinearGradient> PAGXParser::parseLinearGradient(
    const std::shared_ptr<DOMNode>& node) {
  auto startX = PAGXAttributes::ParseFloat(node, "startX", 0.0f);
  auto startY = PAGXAttributes::ParseFloat(node, "startY", 0.0f);
  auto endX = PAGXAttributes::ParseFloat(node, "endX", 0.0f);
  auto endY = PAGXAttributes::ParseFloat(node, "endY", 0.0f);

  auto stops = parseColorStops(node);
  std::vector<Color> colors;
  std::vector<float> positions;
  colors.reserve(stops.size());
  positions.reserve(stops.size());
  for (const auto& [offset, color] : stops) {
    positions.push_back(offset);
    colors.push_back(color);
  }

  auto gradient = Gradient::MakeLinear({startX, startY}, {endX, endY}, colors, positions);
  auto matrixStr = PAGXAttributes::ParseString(node, "matrix", "");
  if (!matrixStr.empty()) {
    gradient->setMatrix(PAGXAttributes::ParseMatrix(matrixStr));
  }
  return gradient;
}

std::shared_ptr<RadialGradient> PAGXParser::parseRadialGradient(
    const std::shared_ptr<DOMNode>& node) {
  auto centerX = PAGXAttributes::ParseFloat(node, "centerX", 0.0f);
  auto centerY = PAGXAttributes::ParseFloat(node, "centerY", 0.0f);
  auto radius = PAGXAttributes::ParseFloat(node, "radius", 0.0f);

  auto stops = parseColorStops(node);
  std::vector<Color> colors;
  std::vector<float> positions;
  colors.reserve(stops.size());
  positions.reserve(stops.size());
  for (const auto& [offset, color] : stops) {
    positions.push_back(offset);
    colors.push_back(color);
  }

  auto gradient = Gradient::MakeRadial({centerX, centerY}, radius, colors, positions);
  auto matrixStr = PAGXAttributes::ParseString(node, "matrix", "");
  if (!matrixStr.empty()) {
    gradient->setMatrix(PAGXAttributes::ParseMatrix(matrixStr));
  }
  return gradient;
}

std::shared_ptr<ConicGradient> PAGXParser::parseConicGradient(
    const std::shared_ptr<DOMNode>& node) {
  auto centerX = PAGXAttributes::ParseFloat(node, "centerX", 0.0f);
  auto centerY = PAGXAttributes::ParseFloat(node, "centerY", 0.0f);
  auto startAngle = PAGXAttributes::ParseFloat(node, "startAngle", 0.0f);
  auto endAngle = PAGXAttributes::ParseFloat(node, "endAngle", 360.0f);

  auto stops = parseColorStops(node);
  std::vector<Color> colors;
  std::vector<float> positions;
  colors.reserve(stops.size());
  positions.reserve(stops.size());
  for (const auto& [offset, color] : stops) {
    positions.push_back(offset);
    colors.push_back(color);
  }

  auto gradient = Gradient::MakeConic({centerX, centerY}, startAngle, endAngle, colors, positions);
  auto matrixStr = PAGXAttributes::ParseString(node, "matrix", "");
  if (!matrixStr.empty()) {
    gradient->setMatrix(PAGXAttributes::ParseMatrix(matrixStr));
  }
  return gradient;
}

std::shared_ptr<DiamondGradient> PAGXParser::parseDiamondGradient(
    const std::shared_ptr<DOMNode>& node) {
  auto centerX = PAGXAttributes::ParseFloat(node, "centerX", 0.0f);
  auto centerY = PAGXAttributes::ParseFloat(node, "centerY", 0.0f);
  auto halfDiagonal = PAGXAttributes::ParseFloat(node, "halfDiagonal", 0.0f);

  auto stops = parseColorStops(node);
  std::vector<Color> colors;
  std::vector<float> positions;
  colors.reserve(stops.size());
  positions.reserve(stops.size());
  for (const auto& [offset, color] : stops) {
    positions.push_back(offset);
    colors.push_back(color);
  }

  auto gradient = Gradient::MakeDiamond({centerX, centerY}, halfDiagonal, colors, positions);
  auto matrixStr = PAGXAttributes::ParseString(node, "matrix", "");
  if (!matrixStr.empty()) {
    gradient->setMatrix(PAGXAttributes::ParseMatrix(matrixStr));
  }
  return gradient;
}

std::shared_ptr<ImagePattern> PAGXParser::parseImagePattern(const std::shared_ptr<DOMNode>& node) {
  auto [hasImage, imageRef] = node->findAttribute("image");
  if (!hasImage || imageRef.empty()) {
    return nullptr;
  }

  std::shared_ptr<Image> image = nullptr;
  if (imageRef[0] == '#') {
    auto it = images.find(imageRef.substr(1));
    if (it != images.end()) {
      image = it->second;
    }
  }

  if (!image) {
    return nullptr;
  }

  auto tileModeXStr = PAGXAttributes::ParseString(node, "tileModeX", "clamp");
  auto tileModeYStr = PAGXAttributes::ParseString(node, "tileModeY", "clamp");

  auto pattern =
      ImagePattern::Make(image, PAGXAttributes::ParseTileMode(tileModeXStr),
                         PAGXAttributes::ParseTileMode(tileModeYStr), SamplingOptions());
  auto matrixStr = PAGXAttributes::ParseString(node, "matrix", "");
  if (!matrixStr.empty()) {
    pattern->setMatrix(PAGXAttributes::ParseMatrix(matrixStr));
  }
  return pattern;
}

std::vector<std::shared_ptr<LayerFilter>> PAGXParser::parseFilters(
    const std::shared_ptr<DOMNode>& node) {
  std::vector<std::shared_ptr<LayerFilter>> filters;
  auto child = node->firstChild;
  while (child) {
    if (child->name == "BlurFilter") {
      auto blurX = PAGXAttributes::ParseFloat(child, "blurrinessX", 0.0f);
      auto blurY = PAGXAttributes::ParseFloat(child, "blurrinessY", 0.0f);
      auto tileModeStr = PAGXAttributes::ParseString(child, "tileMode", "decal");
      auto filter = BlurFilter::Make(blurX, blurY, PAGXAttributes::ParseTileMode(tileModeStr));
      if (filter) {
        filters.push_back(filter);
      }
    } else if (child->name == "DropShadowFilter") {
      auto offsetX = PAGXAttributes::ParseFloat(child, "offsetX", 0.0f);
      auto offsetY = PAGXAttributes::ParseFloat(child, "offsetY", 0.0f);
      auto blurX = PAGXAttributes::ParseFloat(child, "blurrinessX", 0.0f);
      auto blurY = PAGXAttributes::ParseFloat(child, "blurrinessY", 0.0f);
      auto colorStr = PAGXAttributes::ParseString(child, "color", "#000000");
      auto shadowOnly = PAGXAttributes::ParseBool(child, "shadowOnly", false);
      auto filter = DropShadowFilter::Make(offsetX, offsetY, blurX, blurY,
                                           PAGXAttributes::ParseColor(colorStr), shadowOnly);
      if (filter) {
        filters.push_back(filter);
      }
    }
    child = child->nextSibling;
  }
  return filters;
}

std::vector<std::shared_ptr<LayerStyle>> PAGXParser::parseStyles(
    const std::shared_ptr<DOMNode>& node) {
  std::vector<std::shared_ptr<LayerStyle>> styles;
  auto child = node->firstChild;
  while (child) {
    if (child->name == "DropShadowStyle") {
      auto offsetX = PAGXAttributes::ParseFloat(child, "offsetX", 0.0f);
      auto offsetY = PAGXAttributes::ParseFloat(child, "offsetY", 0.0f);
      auto blurX = PAGXAttributes::ParseFloat(child, "blurrinessX", 0.0f);
      auto blurY = PAGXAttributes::ParseFloat(child, "blurrinessY", 0.0f);
      auto colorStr = PAGXAttributes::ParseString(child, "color", "#000000");
      auto showBehind = PAGXAttributes::ParseBool(child, "showBehindLayer", true);
      auto style = DropShadowStyle::Make(offsetX, offsetY, blurX, blurY,
                                         PAGXAttributes::ParseColor(colorStr), showBehind);
      if (style) {
        styles.push_back(style);
      }
    } else if (child->name == "InnerShadowStyle") {
      auto offsetX = PAGXAttributes::ParseFloat(child, "offsetX", 0.0f);
      auto offsetY = PAGXAttributes::ParseFloat(child, "offsetY", 0.0f);
      auto blurX = PAGXAttributes::ParseFloat(child, "blurrinessX", 0.0f);
      auto blurY = PAGXAttributes::ParseFloat(child, "blurrinessY", 0.0f);
      auto colorStr = PAGXAttributes::ParseString(child, "color", "#000000");
      auto style = InnerShadowStyle::Make(offsetX, offsetY, blurX, blurY,
                                          PAGXAttributes::ParseColor(colorStr));
      if (style) {
        styles.push_back(style);
      }
    }
    child = child->nextSibling;
  }
  return styles;
}

std::shared_ptr<ColorSource> PAGXParser::resolveColorReference(const std::string& value) {
  if (value.empty() || value[0] != '#') {
    return nullptr;
  }
  auto id = value.substr(1);
  auto it = colorSources.find(id);
  if (it != colorSources.end()) {
    return it->second;
  }
  return nullptr;
}

std::shared_ptr<Image> PAGXParser::resolveImageReference(const std::string& ref) {
  if (ref.empty()) {
    return nullptr;
  }
  // Check for data URI
  if (ref.find("data:") == 0) {
    // Parse data URI format: data:[<mediatype>][;base64],<data>
    auto commaPos = ref.find(',');
    if (commaPos == std::string::npos) {
      return nullptr;
    }
    auto header = ref.substr(0, commaPos);
    auto base64Data = ref.substr(commaPos + 1);
    // Check if it's base64 encoded
    if (header.find(";base64") == std::string::npos) {
      return nullptr;
    }
    auto data = pagx::Base64Decode(base64Data);
    if (data == nullptr) {
      return nullptr;
    }
    return Image::MakeFromEncoded(data);
  }
  // Relative path - resolve against basePath
  std::string fullPath = basePath_;
  if (!fullPath.empty() && fullPath.back() != '/') {
    fullPath += '/';
  }
  fullPath += ref;
  return Image::MakeFromFile(fullPath);
}

}  // namespace pagx
