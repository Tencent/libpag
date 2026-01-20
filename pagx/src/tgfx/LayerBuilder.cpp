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

#include "pagx/LayerBuilder.h"
#include "pagx/PAGXSVGParser.h"
#include "tgfx/core/Font.h"
#include "tgfx/core/Image.h"
#include "tgfx/core/Path.h"
#include "tgfx/core/TextBlob.h"
#include "tgfx/layers/Layer.h"
#include "tgfx/layers/VectorLayer.h"
#include "tgfx/layers/filters/BlurFilter.h"
#include "tgfx/layers/filters/DropShadowFilter.h"
#include "tgfx/layers/filters/LayerFilter.h"
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

namespace pagx {

// Type converters from pagx to tgfx
static tgfx::Point ToTGFX(const Point& p) {
  return tgfx::Point::Make(p.x, p.y);
}

static tgfx::Color ToTGFX(const Color& c) {
  return {c.red, c.green, c.blue, c.alpha};
}

static tgfx::Matrix ToTGFX(const Matrix& m) {
  return tgfx::Matrix::MakeAll(m.a, m.c, m.tx, m.b, m.d, m.ty);
}

static tgfx::Path ToTGFX(const PathData& pathData) {
  tgfx::Path path;
  pathData.forEach([&](PathVerb verb, const float* pts) {
    switch (verb) {
      case PathVerb::Move:
        path.moveTo(pts[0], pts[1]);
        break;
      case PathVerb::Line:
        path.lineTo(pts[0], pts[1]);
        break;
      case PathVerb::Quad:
        path.quadTo(pts[0], pts[1], pts[2], pts[3]);
        break;
      case PathVerb::Cubic:
        path.cubicTo(pts[0], pts[1], pts[2], pts[3], pts[4], pts[5]);
        break;
      case PathVerb::Close:
        path.close();
        break;
    }
  });
  return path;
}

static tgfx::LineCap ToTGFX(LineCap cap) {
  switch (cap) {
    case LineCap::Butt:
      return tgfx::LineCap::Butt;
    case LineCap::Round:
      return tgfx::LineCap::Round;
    case LineCap::Square:
      return tgfx::LineCap::Square;
  }
  return tgfx::LineCap::Butt;
}

static tgfx::LineJoin ToTGFX(LineJoin join) {
  switch (join) {
    case LineJoin::Miter:
      return tgfx::LineJoin::Miter;
    case LineJoin::Round:
      return tgfx::LineJoin::Round;
    case LineJoin::Bevel:
      return tgfx::LineJoin::Bevel;
  }
  return tgfx::LineJoin::Miter;
}

// Internal builder class
class LayerBuilderImpl {
 public:
  explicit LayerBuilderImpl(const LayerBuilder::Options& options) : _options(options) {
  }

  PAGXContent build(const PAGXDocument& document) {
    PAGXContent content;
    content.width = document.width;
    content.height = document.height;

    // Build layer tree
    auto rootLayer = tgfx::Layer::Make();
    for (const auto& layer : document.layers) {
      auto childLayer = convertLayerNode(layer.get());
      if (childLayer) {
        rootLayer->addChild(childLayer);
      }
    }

    content.root = rootLayer;
    return content;
  }

 private:
  std::shared_ptr<tgfx::Layer> convertLayerNode(const LayerNode* node) {
    if (!node) {
      return nullptr;
    }

    std::shared_ptr<tgfx::Layer> layer = nullptr;

    if (!node->contents.empty()) {
      layer = convertVectorLayer(node);
    } else {
      layer = tgfx::Layer::Make();
    }

    if (layer) {
      applyLayerAttributes(node, layer.get());

      for (const auto& child : node->children) {
        auto childLayer = convertLayerNode(child.get());
        if (childLayer) {
          layer->addChild(childLayer);
        }
      }
    }

    return layer;
  }

  std::shared_ptr<tgfx::Layer> convertVectorLayer(const LayerNode* node) {
    auto layer = tgfx::VectorLayer::Make();
    std::vector<std::shared_ptr<tgfx::VectorElement>> contents;

    for (const auto& element : node->contents) {
      auto tgfxElement = convertVectorElement(element.get());
      if (tgfxElement) {
        contents.push_back(tgfxElement);
      }
    }

    layer->setContents(contents);
    return layer;
  }

  std::shared_ptr<tgfx::VectorElement> convertVectorElement(const VectorElementNode* node) {
    if (!node) {
      return nullptr;
    }

    switch (node->type()) {
      case NodeType::Rectangle:
        return convertRectangle(static_cast<const RectangleNode*>(node));
      case NodeType::Ellipse:
        return convertEllipse(static_cast<const EllipseNode*>(node));
      case NodeType::Polystar:
        return convertPolystar(static_cast<const PolystarNode*>(node));
      case NodeType::Path:
        return convertPath(static_cast<const PathNode*>(node));
      case NodeType::TextSpan:
        return convertTextSpan(static_cast<const TextSpanNode*>(node));
      case NodeType::Fill:
        return convertFill(static_cast<const FillNode*>(node));
      case NodeType::Stroke:
        return convertStroke(static_cast<const StrokeNode*>(node));
      case NodeType::TrimPath:
        return convertTrimPath(static_cast<const TrimPathNode*>(node));
      case NodeType::RoundCorner:
        return convertRoundCorner(static_cast<const RoundCornerNode*>(node));
      case NodeType::MergePath:
        return convertMergePath(static_cast<const MergePathNode*>(node));
      case NodeType::Repeater:
        return convertRepeater(static_cast<const RepeaterNode*>(node));
      case NodeType::Group:
        return convertGroup(static_cast<const GroupNode*>(node));
      default:
        return nullptr;
    }
  }

  std::shared_ptr<tgfx::Rectangle> convertRectangle(const RectangleNode* node) {
    auto rect = std::make_shared<tgfx::Rectangle>();
    rect->setCenter(tgfx::Point::Make(node->centerX, node->centerY));
    rect->setSize({node->width, node->height});
    rect->setRoundness(node->roundness);
    rect->setReversed(node->reversed);
    return rect;
  }

  std::shared_ptr<tgfx::Ellipse> convertEllipse(const EllipseNode* node) {
    auto ellipse = std::make_shared<tgfx::Ellipse>();
    ellipse->setCenter(tgfx::Point::Make(node->centerX, node->centerY));
    ellipse->setSize({node->width, node->height});
    ellipse->setReversed(node->reversed);
    return ellipse;
  }

  std::shared_ptr<tgfx::Polystar> convertPolystar(const PolystarNode* node) {
    auto polystar = std::make_shared<tgfx::Polystar>();
    polystar->setCenter(tgfx::Point::Make(node->centerX, node->centerY));
    polystar->setPointCount(node->points);
    polystar->setOuterRadius(node->outerRadius);
    polystar->setInnerRadius(node->innerRadius);
    polystar->setOuterRoundness(node->outerRoundness);
    polystar->setInnerRoundness(node->innerRoundness);
    polystar->setRotation(node->rotation);
    polystar->setReversed(node->reversed);
    if (node->polystarType == PolystarType::Polygon) {
      polystar->setPolystarType(tgfx::PolystarType::Polygon);
    } else {
      polystar->setPolystarType(tgfx::PolystarType::Star);
    }
    return polystar;
  }

  std::shared_ptr<tgfx::ShapePath> convertPath(const PathNode* node) {
    auto shapePath = std::make_shared<tgfx::ShapePath>();
    auto tgfxPath = ToTGFX(node->d);
    shapePath->setPath(tgfxPath);
    return shapePath;
  }

  std::shared_ptr<tgfx::TextSpan> convertTextSpan(const TextSpanNode* node) {
    auto textSpan = std::make_shared<tgfx::TextSpan>();
    textSpan->setPosition(tgfx::Point::Make(node->x, node->y));

    std::shared_ptr<tgfx::Typeface> typeface = nullptr;
    if (!node->font.empty() && !_options.fallbackTypefaces.empty()) {
      for (const auto& tf : _options.fallbackTypefaces) {
        if (tf && tf->fontFamily() == node->font) {
          typeface = tf;
          break;
        }
      }
    }
    if (!typeface && !_options.fallbackTypefaces.empty()) {
      typeface = _options.fallbackTypefaces[0];
    }
    if (typeface && !node->text.empty()) {
      auto font = tgfx::Font(typeface, node->fontSize);
      auto textBlob = tgfx::TextBlob::MakeFrom(node->text, font);
      textSpan->setTextBlob(textBlob);
    }

    return textSpan;
  }

  std::shared_ptr<tgfx::FillStyle> convertFill(const FillNode* node) {
    auto fill = std::make_shared<tgfx::FillStyle>();

    std::shared_ptr<tgfx::ColorSource> colorSource = nullptr;
    if (node->colorSource) {
      colorSource = convertColorSource(node->colorSource.get());
    } else if (!node->color.empty()) {
      auto color = Color::Parse(node->color);
      colorSource = tgfx::SolidColor::Make(ToTGFX(color));
    }

    if (colorSource) {
      fill->setColorSource(colorSource);
    }

    fill->setAlpha(node->alpha);
    return fill;
  }

  std::shared_ptr<tgfx::StrokeStyle> convertStroke(const StrokeNode* node) {
    auto stroke = std::make_shared<tgfx::StrokeStyle>();

    std::shared_ptr<tgfx::ColorSource> colorSource = nullptr;
    if (node->colorSource) {
      colorSource = convertColorSource(node->colorSource.get());
    } else if (!node->color.empty()) {
      auto color = Color::Parse(node->color);
      colorSource = tgfx::SolidColor::Make(ToTGFX(color));
    }

    if (colorSource) {
      stroke->setColorSource(colorSource);
    }

    stroke->setStrokeWidth(node->strokeWidth);
    stroke->setAlpha(node->alpha);
    stroke->setLineCap(ToTGFX(node->cap));
    stroke->setLineJoin(ToTGFX(node->join));
    stroke->setMiterLimit(node->miterLimit);

    return stroke;
  }

  std::shared_ptr<tgfx::ColorSource> convertColorSource(const ColorSourceNode* node) {
    if (!node) {
      return nullptr;
    }

    switch (node->type()) {
      case NodeType::SolidColor: {
        auto solid = static_cast<const SolidColorNode*>(node);
        return tgfx::SolidColor::Make(ToTGFX(solid->color));
      }
      case NodeType::LinearGradient: {
        auto grad = static_cast<const LinearGradientNode*>(node);
        return convertLinearGradient(grad);
      }
      case NodeType::RadialGradient: {
        auto grad = static_cast<const RadialGradientNode*>(node);
        return convertRadialGradient(grad);
      }
      default:
        return nullptr;
    }
  }

  std::shared_ptr<tgfx::ColorSource> convertLinearGradient(const LinearGradientNode* node) {
    std::vector<tgfx::Color> colors;
    std::vector<float> positions;

    for (const auto& stop : node->colorStops) {
      colors.push_back(ToTGFX(stop.color));
      positions.push_back(stop.offset);
    }

    if (colors.empty()) {
      colors = {tgfx::Color::Black(), tgfx::Color::White()};
      positions = {0.0f, 1.0f};
    }

    auto startPoint = tgfx::Point::Make(node->startX, node->startY);
    auto endPoint = tgfx::Point::Make(node->endX, node->endY);

    return tgfx::Gradient::MakeLinear(startPoint, endPoint, colors, positions);
  }

  std::shared_ptr<tgfx::ColorSource> convertRadialGradient(const RadialGradientNode* node) {
    std::vector<tgfx::Color> colors;
    std::vector<float> positions;

    for (const auto& stop : node->colorStops) {
      colors.push_back(ToTGFX(stop.color));
      positions.push_back(stop.offset);
    }

    if (colors.empty()) {
      colors = {tgfx::Color::Black(), tgfx::Color::White()};
      positions = {0.0f, 1.0f};
    }

    auto center = tgfx::Point::Make(node->centerX, node->centerY);
    return tgfx::Gradient::MakeRadial(center, node->radius, colors, positions);
  }

  std::shared_ptr<tgfx::TrimPath> convertTrimPath(const TrimPathNode* node) {
    auto trim = std::make_shared<tgfx::TrimPath>();
    trim->setStart(node->start);
    trim->setEnd(node->end);
    trim->setOffset(node->offset);
    return trim;
  }

  std::shared_ptr<tgfx::RoundCorner> convertRoundCorner(const RoundCornerNode* node) {
    auto round = std::make_shared<tgfx::RoundCorner>();
    round->setRadius(node->radius);
    return round;
  }

  std::shared_ptr<tgfx::MergePath> convertMergePath(const MergePathNode*) {
    auto merge = std::make_shared<tgfx::MergePath>();
    return merge;
  }

  std::shared_ptr<tgfx::Repeater> convertRepeater(const RepeaterNode* node) {
    auto repeater = std::make_shared<tgfx::Repeater>();
    repeater->setCopies(node->copies);
    repeater->setOffset(node->offset);
    return repeater;
  }

  std::shared_ptr<tgfx::VectorGroup> convertGroup(const GroupNode* node) {
    auto group = std::make_shared<tgfx::VectorGroup>();
    std::vector<std::shared_ptr<tgfx::VectorElement>> elements;

    for (const auto& element : node->elements) {
      auto tgfxElement = convertVectorElement(element.get());
      if (tgfxElement) {
        elements.push_back(tgfxElement);
      }
    }

    group->setElements(elements);
    return group;
  }

  void applyLayerAttributes(const LayerNode* node, tgfx::Layer* layer) {
    layer->setVisible(node->visible);
    layer->setAlpha(node->alpha);

    if (!node->matrix.isIdentity()) {
      layer->setMatrix(ToTGFX(node->matrix));
    }

    // Layer styles
    std::vector<std::shared_ptr<tgfx::LayerStyle>> styles;
    for (const auto& style : node->styles) {
      auto tgfxStyle = convertLayerStyle(style.get());
      if (tgfxStyle) {
        styles.push_back(tgfxStyle);
      }
    }
    if (!styles.empty()) {
      layer->setLayerStyles(styles);
    }

    // Layer filters
    std::vector<std::shared_ptr<tgfx::LayerFilter>> filters;
    for (const auto& filter : node->filters) {
      auto tgfxFilter = convertLayerFilter(filter.get());
      if (tgfxFilter) {
        filters.push_back(tgfxFilter);
      }
    }
    if (!filters.empty()) {
      layer->setFilters(filters);
    }
  }

  std::shared_ptr<tgfx::LayerStyle> convertLayerStyle(const LayerStyleNode* node) {
    if (!node) {
      return nullptr;
    }

    switch (node->type()) {
      case NodeType::DropShadowStyle: {
        auto style = static_cast<const DropShadowStyleNode*>(node);
        return tgfx::DropShadowStyle::Make(style->offsetX, style->offsetY, style->blurrinessX,
                                           style->blurrinessY, ToTGFX(style->color));
      }
      case NodeType::InnerShadowStyle: {
        auto style = static_cast<const InnerShadowStyleNode*>(node);
        return tgfx::InnerShadowStyle::Make(style->offsetX, style->offsetY, style->blurrinessX,
                                            style->blurrinessY, ToTGFX(style->color));
      }
      default:
        return nullptr;
    }
  }

  std::shared_ptr<tgfx::LayerFilter> convertLayerFilter(const LayerFilterNode* node) {
    if (!node) {
      return nullptr;
    }

    switch (node->type()) {
      case NodeType::BlurFilter: {
        auto filter = static_cast<const BlurFilterNode*>(node);
        return tgfx::BlurFilter::Make(filter->blurrinessX, filter->blurrinessY);
      }
      case NodeType::DropShadowFilter: {
        auto filter = static_cast<const DropShadowFilterNode*>(node);
        return tgfx::DropShadowFilter::Make(filter->offsetX, filter->offsetY, filter->blurrinessX,
                                            filter->blurrinessY, ToTGFX(filter->color));
      }
      default:
        return nullptr;
    }
  }

  LayerBuilder::Options _options = {};
};

// Public API implementation

PAGXContent LayerBuilder::Build(const PAGXDocument& document, const Options& options) {
  LayerBuilderImpl builder(options);
  return builder.build(document);
}

PAGXContent LayerBuilder::FromFile(const std::string& filePath, const Options& options) {
  auto document = PAGXDocument::FromFile(filePath);
  if (!document) {
    return {};
  }

  auto opts = options;
  if (opts.basePath.empty()) {
    auto lastSlash = filePath.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
      opts.basePath = filePath.substr(0, lastSlash + 1);
    }
  }

  return Build(*document, opts);
}

PAGXContent LayerBuilder::FromData(const uint8_t* data, size_t length, const Options& options) {
  auto document = PAGXDocument::FromXML(data, length);
  if (!document) {
    return {};
  }
  return Build(*document, options);
}

PAGXContent LayerBuilder::FromSVGFile(const std::string& filePath, const Options& options) {
  auto document = PAGXSVGParser::Parse(filePath);
  if (!document) {
    return {};
  }

  auto opts = options;
  if (opts.basePath.empty()) {
    auto lastSlash = filePath.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
      opts.basePath = filePath.substr(0, lastSlash + 1);
    }
  }

  return Build(*document, opts);
}

}  // namespace pagx
