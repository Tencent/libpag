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

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "pagx/PAGXDocument.h"

namespace pagx {

/**
 * Internal XML node representation for parsing.
 */
struct XMLNode {
  std::string tag = {};
  std::unordered_map<std::string, std::string> attributes = {};
  std::vector<std::unique_ptr<XMLNode>> children = {};
  std::string text = {};
};

/**
 * Parser for PAGX XML format.
 */
class PAGXXMLParser {
 public:
  /**
   * Parses XML data into a PAGXDocument.
   */
  static std::shared_ptr<PAGXDocument> Parse(const uint8_t* data, size_t length);

 private:
  static std::unique_ptr<XMLNode> parseXML(const uint8_t* data, size_t length);

  static void parseDocument(const XMLNode* root, PAGXDocument* doc);
  static void parseResources(const XMLNode* node, PAGXDocument* doc);
  static std::unique_ptr<ResourceNode> parseResource(const XMLNode* node);
  static std::unique_ptr<LayerNode> parseLayer(const XMLNode* node);
  static void parseContents(const XMLNode* node, LayerNode* layer);
  static void parseStyles(const XMLNode* node, LayerNode* layer);
  static void parseFilters(const XMLNode* node, LayerNode* layer);

  static std::unique_ptr<VectorElementNode> parseVectorElement(const XMLNode* node);
  static std::unique_ptr<ColorSourceNode> parseColorSource(const XMLNode* node);
  static std::unique_ptr<LayerStyleNode> parseLayerStyle(const XMLNode* node);
  static std::unique_ptr<LayerFilterNode> parseLayerFilter(const XMLNode* node);

  static std::unique_ptr<RectangleNode> parseRectangle(const XMLNode* node);
  static std::unique_ptr<EllipseNode> parseEllipse(const XMLNode* node);
  static std::unique_ptr<PolystarNode> parsePolystar(const XMLNode* node);
  static std::unique_ptr<PathNode> parsePath(const XMLNode* node);
  static std::unique_ptr<TextSpanNode> parseTextSpan(const XMLNode* node);
  static std::unique_ptr<FillNode> parseFill(const XMLNode* node);
  static std::unique_ptr<StrokeNode> parseStroke(const XMLNode* node);
  static std::unique_ptr<TrimPathNode> parseTrimPath(const XMLNode* node);
  static std::unique_ptr<RoundCornerNode> parseRoundCorner(const XMLNode* node);
  static std::unique_ptr<MergePathNode> parseMergePath(const XMLNode* node);
  static std::unique_ptr<TextModifierNode> parseTextModifier(const XMLNode* node);
  static std::unique_ptr<TextPathNode> parseTextPath(const XMLNode* node);
  static std::unique_ptr<TextLayoutNode> parseTextLayout(const XMLNode* node);
  static std::unique_ptr<RepeaterNode> parseRepeater(const XMLNode* node);
  static std::unique_ptr<GroupNode> parseGroup(const XMLNode* node);
  static RangeSelectorNode parseRangeSelector(const XMLNode* node);

  static std::unique_ptr<SolidColorNode> parseSolidColor(const XMLNode* node);
  static std::unique_ptr<LinearGradientNode> parseLinearGradient(const XMLNode* node);
  static std::unique_ptr<RadialGradientNode> parseRadialGradient(const XMLNode* node);
  static std::unique_ptr<ConicGradientNode> parseConicGradient(const XMLNode* node);
  static std::unique_ptr<DiamondGradientNode> parseDiamondGradient(const XMLNode* node);
  static std::unique_ptr<ImagePatternNode> parseImagePattern(const XMLNode* node);
  static ColorStopNode parseColorStop(const XMLNode* node);

  static std::unique_ptr<ImageNode> parseImage(const XMLNode* node);
  static std::unique_ptr<CompositionNode> parseComposition(const XMLNode* node);

  static std::unique_ptr<DropShadowStyleNode> parseDropShadowStyle(const XMLNode* node);
  static std::unique_ptr<InnerShadowStyleNode> parseInnerShadowStyle(const XMLNode* node);
  static std::unique_ptr<BackgroundBlurStyleNode> parseBackgroundBlurStyle(const XMLNode* node);

  static std::unique_ptr<BlurFilterNode> parseBlurFilter(const XMLNode* node);
  static std::unique_ptr<DropShadowFilterNode> parseDropShadowFilter(const XMLNode* node);
  static std::unique_ptr<InnerShadowFilterNode> parseInnerShadowFilter(const XMLNode* node);
  static std::unique_ptr<BlendFilterNode> parseBlendFilter(const XMLNode* node);
  static std::unique_ptr<ColorMatrixFilterNode> parseColorMatrixFilter(const XMLNode* node);

  static std::string getAttribute(const XMLNode* node, const std::string& name,
                                  const std::string& defaultValue = "");
  static float getFloatAttribute(const XMLNode* node, const std::string& name,
                                 float defaultValue = 0);
  static int getIntAttribute(const XMLNode* node, const std::string& name, int defaultValue = 0);
  static bool getBoolAttribute(const XMLNode* node, const std::string& name,
                               bool defaultValue = false);
  static Point parsePoint(const std::string& str);
  static Size parseSize(const std::string& str);
  static Rect parseRect(const std::string& str);
  static std::vector<float> parseFloatList(const std::string& str);
};

}  // namespace pagx
