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
#include "tgfx/core/Image.h"
#include "tgfx/core/Path.h"
#include "tgfx/layers/Layer.h"
#include "tgfx/layers/VectorLayer.h"
#include "tgfx/layers/filters/BlurFilter.h"
#include "tgfx/layers/filters/DropShadowFilter.h"
#include "tgfx/layers/layerstyles/DropShadowStyle.h"
#include "tgfx/layers/layerstyles/InnerShadowStyle.h"
#include "tgfx/layers/vectors/Ellipse.h"
#include "tgfx/layers/vectors/FillStyle.h"
#include "tgfx/layers/vectors/Gradient.h"
#include "tgfx/layers/vectors/VectorGroup.h"
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

namespace pagx {

// Type converters from pagx to tgfx
static tgfx::Point ToTGFX(const Point& p) {
  return tgfx::Point::Make(p.x, p.y);
}

static tgfx::Color ToTGFX(const Color& c) {
  return {c.red, c.green, c.blue, c.alpha};
}

static tgfx::Matrix ToTGFX(const Matrix& m) {
  return tgfx::Matrix::MakeAll(m.scaleX, m.skewX, m.transX, m.skewY, m.scaleY, m.transY);
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

// Internal builder class
class LayerBuilderImpl {
 public:
  LayerBuilderImpl(const LayerBuilder::Options& options) : _options(options) {
  }

  PAGXContent build(const PAGXDocument& document) {
    PAGXContent content;
    content.width = document.width();
    content.height = document.height();

    auto root = document.root();
    if (!root) {
      return content;
    }

    // Build resources map
    auto resources = document.resources();
    if (resources) {
      for (size_t i = 0; i < resources->childCount(); ++i) {
        auto resource = resources->childAt(i);
        if (!resource->id().empty()) {
          _resourceNodes[resource->id()] = resource;
        }
      }
    }

    // Build layer tree
    auto layer = tgfx::Layer::Make();
    for (size_t i = 0; i < root->childCount(); ++i) {
      auto childLayer = convertNode(root->childAt(i));
      if (childLayer) {
        layer->addChild(childLayer);
      }
    }

    content.root = layer;
    return content;
  }

 private:
  std::shared_ptr<tgfx::Layer> convertNode(const PAGXNode* node) {
    if (!node) {
      return nullptr;
    }

    std::shared_ptr<tgfx::Layer> layer = nullptr;

    switch (node->type()) {
      case PAGXNodeType::Layer:
      case PAGXNodeType::Group:
        layer = convertGroup(node);
        break;
      case PAGXNodeType::Rectangle:
      case PAGXNodeType::Ellipse:
      case PAGXNodeType::Polystar:
      case PAGXNodeType::Path:
      case PAGXNodeType::Text:
        layer = convertVectorLayer(node);
        break;
      case PAGXNodeType::Image:
        layer = convertImage(node);
        break;
      default:
        break;
    }

    if (layer) {
      applyCommonAttributes(node, layer.get());
    }

    return layer;
  }

  std::shared_ptr<tgfx::Layer> convertGroup(const PAGXNode* node) {
    auto layer = tgfx::Layer::Make();
    for (size_t i = 0; i < node->childCount(); ++i) {
      auto childLayer = convertNode(node->childAt(i));
      if (childLayer) {
        layer->addChild(childLayer);
      }
    }
    return layer;
  }

  std::shared_ptr<tgfx::Layer> convertVectorLayer(const PAGXNode* node) {
    auto layer = tgfx::VectorLayer::Make();

    // Convert vector element
    auto element = convertVectorElement(node);
    if (element) {
      layer->setContent(element);
    }

    return layer;
  }

  std::shared_ptr<tgfx::VectorElement> convertVectorElement(const PAGXNode* node) {
    std::shared_ptr<tgfx::VectorElement> element = nullptr;

    switch (node->type()) {
      case PAGXNodeType::Rectangle:
        element = convertRectangle(node);
        break;
      case PAGXNodeType::Ellipse:
        element = convertEllipse(node);
        break;
      case PAGXNodeType::Polystar:
        element = convertPolystar(node);
        break;
      case PAGXNodeType::Path:
        element = convertPath(node);
        break;
      case PAGXNodeType::Text:
        element = convertText(node);
        break;
      case PAGXNodeType::Group:
        element = convertVectorGroup(node);
        break;
      default:
        break;
    }

    if (element) {
      // Add fill and stroke styles from children
      for (size_t i = 0; i < node->childCount(); ++i) {
        auto child = node->childAt(i);
        if (child->type() == PAGXNodeType::Fill) {
          auto fill = convertFill(child);
          if (fill) {
            element->addChild(fill);
          }
        } else if (child->type() == PAGXNodeType::Stroke) {
          auto stroke = convertStroke(child);
          if (stroke) {
            element->addChild(stroke);
          }
        } else if (child->type() == PAGXNodeType::TrimPath) {
          auto trim = convertTrimPath(child);
          if (trim) {
            element->addChild(trim);
          }
        } else if (child->type() == PAGXNodeType::RoundCorner) {
          auto round = convertRoundCorner(child);
          if (round) {
            element->addChild(round);
          }
        } else if (child->type() == PAGXNodeType::MergePath) {
          auto merge = convertMergePath(child);
          if (merge) {
            element->addChild(merge);
          }
        } else if (child->type() == PAGXNodeType::Repeater) {
          auto repeater = convertRepeater(child);
          if (repeater) {
            element->addChild(repeater);
          }
        }
      }
    }

    return element;
  }

  std::shared_ptr<tgfx::Rectangle> convertRectangle(const PAGXNode* node) {
    float x = node->getFloatAttribute("x", 0);
    float y = node->getFloatAttribute("y", 0);
    float width = node->getFloatAttribute("width", 0);
    float height = node->getFloatAttribute("height", 0);
    float rx = node->getFloatAttribute("rx", 0);
    float ry = node->getFloatAttribute("ry", 0);

    auto rect = tgfx::Rectangle::Make();
    rect->setX(x);
    rect->setY(y);
    rect->setWidth(width);
    rect->setHeight(height);
    rect->setRoundedCornerX(rx);
    rect->setRoundedCornerY(ry);
    return rect;
  }

  std::shared_ptr<tgfx::Ellipse> convertEllipse(const PAGXNode* node) {
    float cx = node->getFloatAttribute("cx", 0);
    float cy = node->getFloatAttribute("cy", 0);
    float rx = node->getFloatAttribute("rx", 0);
    float ry = node->getFloatAttribute("ry", 0);

    auto ellipse = tgfx::Ellipse::Make();
    ellipse->setCenterX(cx);
    ellipse->setCenterY(cy);
    ellipse->setRadiusX(rx);
    ellipse->setRadiusY(ry);
    return ellipse;
  }

  std::shared_ptr<tgfx::Polystar> convertPolystar(const PAGXNode* node) {
    auto polystar = tgfx::Polystar::Make();
    polystar->setCenterX(node->getFloatAttribute("centerX", 0));
    polystar->setCenterY(node->getFloatAttribute("centerY", 0));
    polystar->setPoints(node->getFloatAttribute("points", 5));
    polystar->setInnerRadius(node->getFloatAttribute("innerRadius", 0));
    polystar->setOuterRadius(node->getFloatAttribute("outerRadius", 0));
    polystar->setInnerRoundness(node->getFloatAttribute("innerRoundness", 0));
    polystar->setOuterRoundness(node->getFloatAttribute("outerRoundness", 0));
    polystar->setRotation(node->getFloatAttribute("rotation", 0));

    std::string type = node->getAttribute("type", "Star");
    if (type == "Polygon") {
      polystar->setType(tgfx::PolystarType::Polygon);
    } else {
      polystar->setType(tgfx::PolystarType::Star);
    }

    return polystar;
  }

  std::shared_ptr<tgfx::ShapePath> convertPath(const PAGXNode* node) {
    auto pathData = node->getPathAttribute("d");
    auto tgfxPath = ToTGFX(pathData);
    auto shapePath = tgfx::ShapePath::Make();
    shapePath->setPath(tgfxPath);
    return shapePath;
  }

  std::shared_ptr<tgfx::TextSpan> convertText(const PAGXNode* node) {
    std::string text = node->getAttribute("text");
    float fontSize = node->getFloatAttribute("fontSize", 16);
    std::string fontFamily = node->getAttribute("fontFamily");

    auto textSpan = tgfx::TextSpan::Make();
    textSpan->setText(text);

    // Create font from typeface
    std::shared_ptr<tgfx::Typeface> typeface = nullptr;
    if (!fontFamily.empty() && !_options.fallbackTypefaces.empty()) {
      for (auto& tf : _options.fallbackTypefaces) {
        if (tf && tf->fontFamily() == fontFamily) {
          typeface = tf;
          break;
        }
      }
    }
    if (!typeface && !_options.fallbackTypefaces.empty()) {
      typeface = _options.fallbackTypefaces[0];
    }

    if (typeface) {
      textSpan->setTypeface(typeface);
    }
    textSpan->setFontSize(fontSize);

    float x = node->getFloatAttribute("x", 0);
    float y = node->getFloatAttribute("y", 0);
    textSpan->setX(x);
    textSpan->setY(y);

    return textSpan;
  }

  std::shared_ptr<tgfx::VectorGroup> convertVectorGroup(const PAGXNode* node) {
    auto group = tgfx::VectorGroup::Make();
    for (size_t i = 0; i < node->childCount(); ++i) {
      auto child = node->childAt(i);
      if (child->type() != PAGXNodeType::Fill && child->type() != PAGXNodeType::Stroke) {
        auto childElement = convertVectorElement(child);
        if (childElement) {
          group->addChild(childElement);
        }
      }
    }
    return group;
  }

  std::shared_ptr<tgfx::FillStyle> convertFill(const PAGXNode* node) {
    auto fill = tgfx::FillStyle::Make();

    // Try to get color source from reference
    std::string colorSourceRef = node->getAttribute("colorSourceRef");
    if (!colorSourceRef.empty()) {
      auto colorSource = convertColorSourceFromRef(colorSourceRef);
      if (colorSource) {
        fill->setColorSource(colorSource);
      }
    } else {
      // Use solid color
      auto color = node->getColorAttribute("color", Color::Black());
      auto solidColor = tgfx::SolidColor::Make(ToTGFX(color));
      fill->setColorSource(solidColor);
    }

    return fill;
  }

  std::shared_ptr<tgfx::StrokeStyle> convertStroke(const PAGXNode* node) {
    auto stroke = tgfx::StrokeStyle::Make();

    // Color source
    std::string colorSourceRef = node->getAttribute("colorSourceRef");
    if (!colorSourceRef.empty()) {
      auto colorSource = convertColorSourceFromRef(colorSourceRef);
      if (colorSource) {
        stroke->setColorSource(colorSource);
      }
    } else {
      auto color = node->getColorAttribute("color", Color::Black());
      auto solidColor = tgfx::SolidColor::Make(ToTGFX(color));
      stroke->setColorSource(solidColor);
    }

    // Stroke properties
    float width = node->getFloatAttribute("width", 1);
    stroke->setStrokeWidth(width);

    std::string lineCap = node->getAttribute("lineCap", "butt");
    if (lineCap == "round") {
      stroke->setLineCap(tgfx::LineCap::Round);
    } else if (lineCap == "square") {
      stroke->setLineCap(tgfx::LineCap::Square);
    } else {
      stroke->setLineCap(tgfx::LineCap::Butt);
    }

    std::string lineJoin = node->getAttribute("lineJoin", "miter");
    if (lineJoin == "round") {
      stroke->setLineJoin(tgfx::LineJoin::Round);
    } else if (lineJoin == "bevel") {
      stroke->setLineJoin(tgfx::LineJoin::Bevel);
    } else {
      stroke->setLineJoin(tgfx::LineJoin::Miter);
    }

    float miterLimit = node->getFloatAttribute("miterLimit", 4);
    stroke->setMiterLimit(miterLimit);

    return stroke;
  }

  std::shared_ptr<tgfx::ColorSource> convertColorSourceFromRef(const std::string& refId) {
    auto it = _resourceNodes.find(refId);
    if (it == _resourceNodes.end()) {
      return nullptr;
    }
    return convertColorSource(it->second);
  }

  std::shared_ptr<tgfx::ColorSource> convertColorSource(const PAGXNode* node) {
    if (!node) {
      return nullptr;
    }

    switch (node->type()) {
      case PAGXNodeType::SolidColor: {
        auto color = node->getColorAttribute("color", Color::Black());
        return tgfx::SolidColor::Make(ToTGFX(color));
      }
      case PAGXNodeType::LinearGradient: {
        return convertLinearGradient(node);
      }
      case PAGXNodeType::RadialGradient: {
        return convertRadialGradient(node);
      }
      default:
        return nullptr;
    }
  }

  std::shared_ptr<tgfx::ColorSource> convertLinearGradient(const PAGXNode* node) {
    float x1 = node->getFloatAttribute("x1", 0);
    float y1 = node->getFloatAttribute("y1", 0);
    float x2 = node->getFloatAttribute("x2", 1);
    float y2 = node->getFloatAttribute("y2", 0);

    int stopCount = node->getIntAttribute("stopCount", 0);
    std::vector<tgfx::Color> colors;
    std::vector<float> positions;

    for (int i = 0; i < stopCount; ++i) {
      std::string prefix = "stop" + std::to_string(i) + "_";
      float offset = node->getFloatAttribute(prefix + "offset", 0);
      auto color = node->getColorAttribute(prefix + "color", Color::Black());
      colors.push_back(ToTGFX(color));
      positions.push_back(offset);
    }

    if (colors.empty()) {
      colors = {tgfx::Color::Black(), tgfx::Color::White()};
      positions = {0.0f, 1.0f};
    }

    auto startPoint = tgfx::Point::Make(x1, y1);
    auto endPoint = tgfx::Point::Make(x2, y2);

    return tgfx::Gradient::MakeLinear(startPoint, endPoint, colors, positions);
  }

  std::shared_ptr<tgfx::ColorSource> convertRadialGradient(const PAGXNode* node) {
    float cx = node->getFloatAttribute("cx", 0.5f);
    float cy = node->getFloatAttribute("cy", 0.5f);
    float r = node->getFloatAttribute("r", 0.5f);

    int stopCount = node->getIntAttribute("stopCount", 0);
    std::vector<tgfx::Color> colors;
    std::vector<float> positions;

    for (int i = 0; i < stopCount; ++i) {
      std::string prefix = "stop" + std::to_string(i) + "_";
      float offset = node->getFloatAttribute(prefix + "offset", 0);
      auto color = node->getColorAttribute(prefix + "color", Color::Black());
      colors.push_back(ToTGFX(color));
      positions.push_back(offset);
    }

    if (colors.empty()) {
      colors = {tgfx::Color::Black(), tgfx::Color::White()};
      positions = {0.0f, 1.0f};
    }

    auto center = tgfx::Point::Make(cx, cy);
    return tgfx::Gradient::MakeRadial(center, r, colors, positions);
  }

  std::shared_ptr<tgfx::TrimPath> convertTrimPath(const PAGXNode* node) {
    auto trim = tgfx::TrimPath::Make();
    trim->setStart(node->getFloatAttribute("start", 0));
    trim->setEnd(node->getFloatAttribute("end", 1));
    trim->setOffset(node->getFloatAttribute("offset", 0));
    return trim;
  }

  std::shared_ptr<tgfx::RoundCorner> convertRoundCorner(const PAGXNode* node) {
    auto round = tgfx::RoundCorner::Make();
    round->setRadius(node->getFloatAttribute("radius", 0));
    return round;
  }

  std::shared_ptr<tgfx::MergePath> convertMergePath(const PAGXNode* node) {
    auto merge = tgfx::MergePath::Make();
    std::string mode = node->getAttribute("mode", "Merge");
    // Set merge mode based on string
    return merge;
  }

  std::shared_ptr<tgfx::Repeater> convertRepeater(const PAGXNode* node) {
    auto repeater = tgfx::Repeater::Make();
    repeater->setCopies(node->getFloatAttribute("copies", 1));
    repeater->setOffset(node->getFloatAttribute("offset", 0));
    return repeater;
  }

  std::shared_ptr<tgfx::Layer> convertImage(const PAGXNode* node) {
    std::string href = node->getAttribute("href");
    if (href.empty()) {
      return nullptr;
    }

    std::string imagePath = href;
    if (!_options.basePath.empty() && href[0] != '/' && href.find("://") == std::string::npos) {
      imagePath = _options.basePath + href;
    }

    auto image = tgfx::Image::MakeFromFile(imagePath);
    if (!image) {
      return nullptr;
    }

    auto layer = tgfx::Layer::Make();
    // Note: Image rendering would typically use ImageLayer or similar
    // For now, we just create a placeholder layer

    return layer;
  }

  void applyCommonAttributes(const PAGXNode* node, tgfx::Layer* layer) {
    // Transform
    if (node->hasAttribute("transform")) {
      auto matrix = node->getMatrixAttribute("transform");
      layer->setMatrix(ToTGFX(matrix));
    }

    // Opacity
    if (node->hasAttribute("opacity")) {
      layer->setAlpha(node->getFloatAttribute("opacity", 1.0f));
    }

    // Visibility
    if (node->hasAttribute("visible")) {
      layer->setVisible(node->getBoolAttribute("visible", true));
    }

    // Filters
    for (size_t i = 0; i < node->childCount(); ++i) {
      auto child = node->childAt(i);
      if (child->type() == PAGXNodeType::BlurFilter) {
        float radius = child->getFloatAttribute("blurRadius", 0);
        auto filter = tgfx::BlurFilter::Make(radius, radius);
        layer->setFilter(filter);
      } else if (child->type() == PAGXNodeType::DropShadowFilter) {
        float dx = child->getFloatAttribute("dx", 0);
        float dy = child->getFloatAttribute("dy", 0);
        float blur = child->getFloatAttribute("blurRadius", 0);
        auto color = child->getColorAttribute("color", Color::Black());
        auto filter = tgfx::DropShadowFilter::Make(dx, dy, blur, blur, ToTGFX(color));
        layer->setFilter(filter);
      }
    }

    // Layer styles
    std::vector<std::shared_ptr<tgfx::LayerStyle>> styles;
    for (size_t i = 0; i < node->childCount(); ++i) {
      auto child = node->childAt(i);
      if (child->type() == PAGXNodeType::DropShadowStyle) {
        float dx = child->getFloatAttribute("dx", 0);
        float dy = child->getFloatAttribute("dy", 0);
        float blur = child->getFloatAttribute("blurRadius", 0);
        auto color = child->getColorAttribute("color", Color::Black());
        auto style = tgfx::DropShadowStyle::Make(dx, dy, blur, blur, ToTGFX(color));
        styles.push_back(style);
      } else if (child->type() == PAGXNodeType::InnerShadowStyle) {
        float dx = child->getFloatAttribute("dx", 0);
        float dy = child->getFloatAttribute("dy", 0);
        float blur = child->getFloatAttribute("blurRadius", 0);
        auto color = child->getColorAttribute("color", Color::Black());
        auto style = tgfx::InnerShadowStyle::Make(dx, dy, blur, blur, ToTGFX(color));
        styles.push_back(style);
      }
    }
    if (!styles.empty()) {
      layer->setLayerStyles(styles);
    }
  }

  LayerBuilder::Options _options = {};
  std::unordered_map<std::string, const PAGXNode*> _resourceNodes = {};
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
