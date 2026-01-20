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

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "tgfx/core/Image.h"
#include "tgfx/layers/Layer.h"
#include "tgfx/layers/VectorLayer.h"
#include "tgfx/layers/filters/LayerFilter.h"
#include "tgfx/layers/layerstyles/LayerStyle.h"
#include "tgfx/layers/vectors/ColorSource.h"
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
#include "tgfx/layers/vectors/StrokeStyle.h"
#include "tgfx/layers/vectors/TextSpan.h"
#include "tgfx/layers/vectors/TrimPath.h"
#include "tgfx/layers/vectors/VectorElement.h"
#include "tgfx/layers/vectors/VectorGroup.h"
#include "tgfx/svg/xml/XMLDOM.h"

namespace pagx {

using namespace tgfx;

class PAGXParser {
 public:
  PAGXParser(std::shared_ptr<DOMNode> rootNode, const std::string& basePath);

  bool parse();

  float width() const {
    return width_;
  }

  float height() const {
    return height_;
  }

  std::shared_ptr<Layer> getRoot() const {
    return rootLayer;
  }

 private:
  void parseResources(const std::shared_ptr<DOMNode>& node);

  std::shared_ptr<Layer> parseLayer(const std::shared_ptr<DOMNode>& node);

  std::vector<std::shared_ptr<VectorElement>> parseContents(const std::shared_ptr<DOMNode>& node);

  std::shared_ptr<VectorElement> parseVectorElement(const std::shared_ptr<DOMNode>& node);

  std::shared_ptr<VectorGroup> parseGroup(const std::shared_ptr<DOMNode>& node);

  std::shared_ptr<Rectangle> parseRectangle(const std::shared_ptr<DOMNode>& node);

  std::shared_ptr<Ellipse> parseEllipse(const std::shared_ptr<DOMNode>& node);

  std::shared_ptr<Polystar> parsePolystar(const std::shared_ptr<DOMNode>& node);

  std::shared_ptr<ShapePath> parsePath(const std::shared_ptr<DOMNode>& node);

  std::shared_ptr<TextSpan> parseTextSpan(const std::shared_ptr<DOMNode>& node);

  std::shared_ptr<FillStyle> parseFill(const std::shared_ptr<DOMNode>& node);

  std::shared_ptr<StrokeStyle> parseStroke(const std::shared_ptr<DOMNode>& node);

  std::shared_ptr<TrimPath> parseTrimPath(const std::shared_ptr<DOMNode>& node);

  std::shared_ptr<RoundCorner> parseRoundCorner(const std::shared_ptr<DOMNode>& node);

  std::shared_ptr<MergePath> parseMergePath(const std::shared_ptr<DOMNode>& node);

  std::shared_ptr<Repeater> parseRepeater(const std::shared_ptr<DOMNode>& node);

  std::shared_ptr<ColorSource> parseColorSource(const std::shared_ptr<DOMNode>& node);

  std::shared_ptr<ColorSource> parseInlineColorSource(const std::shared_ptr<DOMNode>& parentNode);

  std::shared_ptr<LinearGradient> parseLinearGradient(const std::shared_ptr<DOMNode>& node);

  std::shared_ptr<RadialGradient> parseRadialGradient(const std::shared_ptr<DOMNode>& node);

  std::shared_ptr<ConicGradient> parseConicGradient(const std::shared_ptr<DOMNode>& node);

  std::shared_ptr<DiamondGradient> parseDiamondGradient(const std::shared_ptr<DOMNode>& node);

  std::shared_ptr<ImagePattern> parseImagePattern(const std::shared_ptr<DOMNode>& node);

  std::vector<std::pair<float, Color>> parseColorStops(const std::shared_ptr<DOMNode>& node);

  std::vector<std::shared_ptr<LayerFilter>> parseFilters(const std::shared_ptr<DOMNode>& node);

  std::vector<std::shared_ptr<LayerStyle>> parseStyles(const std::shared_ptr<DOMNode>& node);

  std::shared_ptr<ColorSource> resolveColorReference(const std::string& value);

  std::shared_ptr<Image> resolveImageReference(const std::string& ref);

  std::shared_ptr<DOMNode> rootNode_ = nullptr;
  std::string basePath_;
  float width_ = 0.0f;
  float height_ = 0.0f;
  std::shared_ptr<Layer> rootLayer = nullptr;
  std::unordered_map<std::string, std::shared_ptr<ColorSource>> colorSources;
  std::unordered_map<std::string, std::shared_ptr<Image>> images;
  std::unordered_map<std::string, std::shared_ptr<Layer>> layerIdMap;
};

}  // namespace pagx
