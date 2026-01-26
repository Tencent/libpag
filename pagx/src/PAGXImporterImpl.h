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
#include "pagx/nodes/BackgroundBlurStyle.h"
#include "pagx/nodes/BlendFilter.h"
#include "pagx/nodes/BlurFilter.h"
#include "pagx/nodes/ColorMatrixFilter.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/ConicGradient.h"
#include "pagx/nodes/DiamondGradient.h"
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/DropShadowFilter.h"
#include "pagx/nodes/DropShadowStyle.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Fill.h"
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
#include "pagx/nodes/TextLayout.h"
#include "pagx/nodes/TextModifier.h"
#include "pagx/nodes/TextPath.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TrimPath.h"
#include "pagx/nodes/Color.h"

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
 * Internal implementation of PAGX XML parser.
 */
class PAGXImporterImpl {
 public:
  /**
   * Parses XML data into a PAGXDocument.
   */
  static std::shared_ptr<PAGXDocument> Parse(const uint8_t* data, size_t length);

 private:
  static std::unique_ptr<XMLNode> parseXML(const uint8_t* data, size_t length);

  static void parseDocument(const XMLNode* root, PAGXDocument* doc);
  static void parseResources(const XMLNode* node, PAGXDocument* doc);
  static std::unique_ptr<Node> parseResource(const XMLNode* node);
  static std::unique_ptr<Layer> parseLayer(const XMLNode* node);
  static void parseContents(const XMLNode* node, Layer* layer);
  static void parseStyles(const XMLNode* node, Layer* layer);
  static void parseFilters(const XMLNode* node, Layer* layer);

  static std::unique_ptr<Element> parseElement(const XMLNode* node);
  static std::unique_ptr<ColorSource> parseColorSource(const XMLNode* node);
  static std::unique_ptr<LayerStyle> parseLayerStyle(const XMLNode* node);
  static std::unique_ptr<LayerFilter> parseLayerFilter(const XMLNode* node);

  static std::unique_ptr<Rectangle> parseRectangle(const XMLNode* node);
  static std::unique_ptr<Ellipse> parseEllipse(const XMLNode* node);
  static std::unique_ptr<Polystar> parsePolystar(const XMLNode* node);
  static std::unique_ptr<Path> parsePath(const XMLNode* node);
  static std::unique_ptr<Text> parseText(const XMLNode* node);
  static std::unique_ptr<Fill> parseFill(const XMLNode* node);
  static std::unique_ptr<Stroke> parseStroke(const XMLNode* node);
  static std::unique_ptr<TrimPath> parseTrimPath(const XMLNode* node);
  static std::unique_ptr<RoundCorner> parseRoundCorner(const XMLNode* node);
  static std::unique_ptr<MergePath> parseMergePath(const XMLNode* node);
  static std::unique_ptr<TextModifier> parseTextModifier(const XMLNode* node);
  static std::unique_ptr<TextPath> parseTextPath(const XMLNode* node);
  static std::unique_ptr<TextLayout> parseTextLayout(const XMLNode* node);
  static std::unique_ptr<Repeater> parseRepeater(const XMLNode* node);
  static std::unique_ptr<Group> parseGroup(const XMLNode* node);
  static RangeSelector parseRangeSelector(const XMLNode* node);

  static std::unique_ptr<SolidColor> parseSolidColor(const XMLNode* node);
  static std::unique_ptr<LinearGradient> parseLinearGradient(const XMLNode* node);
  static std::unique_ptr<RadialGradient> parseRadialGradient(const XMLNode* node);
  static std::unique_ptr<ConicGradient> parseConicGradient(const XMLNode* node);
  static std::unique_ptr<DiamondGradient> parseDiamondGradient(const XMLNode* node);
  static std::unique_ptr<ImagePattern> parseImagePattern(const XMLNode* node);
  static ColorStop parseColorStop(const XMLNode* node);

  static std::unique_ptr<Image> parseImage(const XMLNode* node);
  static std::unique_ptr<PathData> parsePathData(const XMLNode* node);
  static std::unique_ptr<Composition> parseComposition(const XMLNode* node);

  static std::unique_ptr<DropShadowStyle> parseDropShadowStyle(const XMLNode* node);
  static std::unique_ptr<InnerShadowStyle> parseInnerShadowStyle(const XMLNode* node);
  static std::unique_ptr<BackgroundBlurStyle> parseBackgroundBlurStyle(const XMLNode* node);

  static std::unique_ptr<BlurFilter> parseBlurFilter(const XMLNode* node);
  static std::unique_ptr<DropShadowFilter> parseDropShadowFilter(const XMLNode* node);
  static std::unique_ptr<InnerShadowFilter> parseInnerShadowFilter(const XMLNode* node);
  static std::unique_ptr<BlendFilter> parseBlendFilter(const XMLNode* node);
  static std::unique_ptr<ColorMatrixFilter> parseColorMatrixFilter(const XMLNode* node);

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
  static Color parseColor(const std::string& str);
  static std::vector<float> parseFloatList(const std::string& str);
};

}  // namespace pagx
