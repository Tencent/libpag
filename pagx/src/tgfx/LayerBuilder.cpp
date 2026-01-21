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
#include <array>
#include <tuple>
#include <unordered_map>
#include "pagx/model/BlurFilter.h"
#include "pagx/model/Composition.h"
#include "pagx/model/ConicGradient.h"
#include "pagx/model/DiamondGradient.h"
#include "pagx/model/DropShadowFilter.h"
#include "pagx/model/DropShadowStyle.h"
#include "pagx/model/Ellipse.h"
#include "pagx/model/Fill.h"
#include "pagx/model/Group.h"
#include "pagx/model/Image.h"
#include "pagx/model/ImagePattern.h"
#include "pagx/model/InnerShadowFilter.h"
#include "pagx/model/InnerShadowStyle.h"
#include "pagx/model/Layer.h"
#include "pagx/model/LinearGradient.h"
#include "pagx/model/MergePath.h"
#include "pagx/model/Node.h"
#include "pagx/model/Path.h"
#include "pagx/model/Polystar.h"
#include "pagx/model/RadialGradient.h"
#include "pagx/model/Rectangle.h"
#include "pagx/model/Repeater.h"
#include "pagx/model/RoundCorner.h"
#include "pagx/model/SolidColor.h"
#include "pagx/model/Stroke.h"
#include "pagx/model/TextSpan.h"
#include "pagx/model/TrimPath.h"
#include "pagx/SVGImporter.h"
#include "tgfx/core/Data.h"
#include "tgfx/core/Font.h"
#include "tgfx/core/Image.h"
#include "tgfx/core/Path.h"
#include "tgfx/core/TextBlob.h"
#include "tgfx/layers/Layer.h"
#include "tgfx/layers/LayerMaskType.h"
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
#include "tgfx/svg/TextShaper.h"

namespace pagx {

// Decode base64 encoded string to binary data.
static std::shared_ptr<tgfx::Data> Base64Decode(const std::string& encodedString) {
  static const std::array<unsigned char, 128> decodingTable = {
      64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
      64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62,
      64, 64, 64, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64, 64, 0,
      1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
      23, 24, 25, 64, 64, 64, 64, 64, 64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38,
      39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64};

  size_t inputLength = encodedString.size();
  if (inputLength % 4 != 0) {
    return nullptr;
  }

  size_t outputLength = inputLength / 4 * 3;
  if (inputLength >= 1 && encodedString[inputLength - 1] == '=') {
    outputLength--;
  }
  if (inputLength >= 2 && encodedString[inputLength - 2] == '=') {
    outputLength--;
  }

  auto output = static_cast<unsigned char*>(malloc(outputLength));
  if (!output) {
    return nullptr;
  }

  for (size_t i = 0, j = 0; i < inputLength;) {
    auto a = encodedString[i] <= 127 ? decodingTable[encodedString[i++]] : 64;
    auto b = encodedString[i] <= 127 ? decodingTable[encodedString[i++]] : 64;
    auto c = encodedString[i] <= 127 ? decodingTable[encodedString[i++]] : 64;
    auto d = encodedString[i] <= 127 ? decodingTable[encodedString[i++]] : 64;

    uint32_t triple = (a << 3 * 6) + (b << 2 * 6) + (c << 1 * 6) + (d << 0 * 6);

    if (j < outputLength) {
      output[j++] = (triple >> 2 * 8) & 0xFF;
    }
    if (j < outputLength) {
      output[j++] = (triple >> 1 * 8) & 0xFF;
    }
    if (j < outputLength) {
      output[j++] = (triple >> 0 * 8) & 0xFF;
    }
  }

  return tgfx::Data::MakeAdopted(output, outputLength, tgfx::Data::FreeProc);
}

// Decode a data URI (e.g., "data:image/png;base64,...") to an Image.
static std::shared_ptr<tgfx::Image> ImageFromDataURI(const std::string& dataURI) {
  if (dataURI.find("data:") != 0) {
    return nullptr;
  }

  auto commaPos = dataURI.find(',');
  if (commaPos == std::string::npos) {
    return nullptr;
  }

  auto header = dataURI.substr(0, commaPos);
  auto base64Data = dataURI.substr(commaPos + 1);

  if (header.find(";base64") == std::string::npos) {
    return nullptr;
  }

  auto data = Base64Decode(base64Data);
  if (!data) {
    return nullptr;
  }

  return tgfx::Image::MakeFromEncoded(data);
}

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

static tgfx::LayerMaskType ToTGFXMaskType(MaskType type) {
  switch (type) {
    case MaskType::Alpha:
      return tgfx::LayerMaskType::Alpha;
    case MaskType::Luminance:
      return tgfx::LayerMaskType::Luminance;
    case MaskType::Contour:
      return tgfx::LayerMaskType::Contour;
  }
  return tgfx::LayerMaskType::Alpha;
}

// Internal builder class
class LayerBuilderImpl {
 public:
  explicit LayerBuilderImpl(const LayerBuilder::Options& options) : _options(options) {
    // Create text shaper from options or fallback typefaces.
    if (_options.textShaper) {
      _textShaper = _options.textShaper;
    } else if (!_options.fallbackTypefaces.empty()) {
      _textShaper = tgfx::TextShaper::Make(_options.fallbackTypefaces);
    }
  }

  PAGXContent build(const Document& document) {
    // Cache resources for later lookup.
    _resources = &document.resources;

    // Clear layer mappings from previous builds.
    _layerById.clear();
    _pendingMasks.clear();

    PAGXContent content;
    content.width = document.width;
    content.height = document.height;

    // Build layer tree.
    auto rootLayer = tgfx::Layer::Make();
    for (const auto& layer : document.layers) {
      auto childLayer = convertLayer(layer.get());
      if (childLayer) {
        rootLayer->addChild(childLayer);
      }
    }

    // Apply masks after all layers are built (second pass).
    for (const auto& [layer, maskId, maskType] : _pendingMasks) {
      auto it = _layerById.find(maskId);
      if (it != _layerById.end()) {
        auto maskLayer = it->second;
        // tgfx requires mask layer to be visible for hasValidMask() check.
        // The mask layer won't be drawn because maskOwner is set.
        maskLayer->setVisible(true);
        layer->setMask(maskLayer);
        layer->setMaskType(maskType);
      }
    }

    content.root = rootLayer;
    _resources = nullptr;
    _layerById.clear();
    _pendingMasks.clear();
    return content;
  }

 private:
  std::shared_ptr<tgfx::Layer> convertLayer(const Layer* node) {
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
      // Register layer by ID for mask lookups.
      if (!node->id.empty()) {
        _layerById[node->id] = layer;
      }

      applyLayerAttributes(node, layer.get());

      // Queue mask to be applied in second pass.
      if (!node->mask.empty()) {
        std::string maskId = node->mask;
        // Remove leading '#' if present.
        if (!maskId.empty() && maskId[0] == '#') {
          maskId = maskId.substr(1);
        }
        _pendingMasks.emplace_back(layer, maskId, ToTGFXMaskType(node->maskType));
      }

      for (const auto& child : node->children) {
        auto childLayer = convertLayer(child.get());
        if (childLayer) {
          layer->addChild(childLayer);
        }
      }
    }

    return layer;
  }

  std::shared_ptr<tgfx::Layer> convertVectorLayer(const Layer* node) {
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

  std::shared_ptr<tgfx::VectorElement> convertVectorElement(const Element* node) {
    if (!node) {
      return nullptr;
    }

    switch (node->type()) {
      case ElementType::Rectangle:
        return convertRectangle(static_cast<const Rectangle*>(node));
      case ElementType::Ellipse:
        return convertEllipse(static_cast<const Ellipse*>(node));
      case ElementType::Polystar:
        return convertPolystar(static_cast<const Polystar*>(node));
      case ElementType::Path:
        return convertPath(static_cast<const Path*>(node));
      case ElementType::TextSpan:
        return convertTextSpan(static_cast<const TextSpan*>(node));
      case ElementType::Fill:
        return convertFill(static_cast<const Fill*>(node));
      case ElementType::Stroke:
        return convertStroke(static_cast<const Stroke*>(node));
      case ElementType::TrimPath:
        return convertTrimPath(static_cast<const TrimPath*>(node));
      case ElementType::RoundCorner:
        return convertRoundCorner(static_cast<const RoundCorner*>(node));
      case ElementType::MergePath:
        return convertMergePath(static_cast<const MergePath*>(node));
      case ElementType::Repeater:
        return convertRepeater(static_cast<const Repeater*>(node));
      case ElementType::Group:
        return convertGroup(static_cast<const Group*>(node));
      default:
        return nullptr;
    }
  }

  std::shared_ptr<tgfx::Rectangle> convertRectangle(const Rectangle* node) {
    auto rect = std::make_shared<tgfx::Rectangle>();
    rect->setCenter(ToTGFX(node->center));
    rect->setSize({node->size.width, node->size.height});
    rect->setRoundness(node->roundness);
    rect->setReversed(node->reversed);
    return rect;
  }

  std::shared_ptr<tgfx::Ellipse> convertEllipse(const Ellipse* node) {
    auto ellipse = std::make_shared<tgfx::Ellipse>();
    ellipse->setCenter(ToTGFX(node->center));
    ellipse->setSize({node->size.width, node->size.height});
    ellipse->setReversed(node->reversed);
    return ellipse;
  }

  std::shared_ptr<tgfx::Polystar> convertPolystar(const Polystar* node) {
    auto polystar = std::make_shared<tgfx::Polystar>();
    polystar->setCenter(ToTGFX(node->center));
    polystar->setPointCount(node->pointCount);
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

  std::shared_ptr<tgfx::ShapePath> convertPath(const Path* node) {
    auto shapePath = std::make_shared<tgfx::ShapePath>();
    auto tgfxPath = ToTGFX(node->data);
    shapePath->setPath(tgfxPath);
    return shapePath;
  }

  std::shared_ptr<tgfx::TextSpan> convertTextSpan(const TextSpan* node) {
    auto textSpan = std::make_shared<tgfx::TextSpan>();

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

    float xOffset = 0;
    if (!node->text.empty()) {
      std::shared_ptr<tgfx::TextBlob> textBlob = nullptr;
      // Use TextShaper for fallback support (including emoji).
      if (_textShaper) {
        textBlob = _textShaper->shape(node->text, typeface, node->fontSize);
      } else if (typeface) {
        auto font = tgfx::Font(typeface, node->fontSize);
        textBlob = tgfx::TextBlob::MakeFrom(node->text, font);
      }
      textSpan->setTextBlob(textBlob);
    }

    textSpan->setPosition(tgfx::Point::Make(node->x + xOffset, node->y));
    return textSpan;
  }

  std::shared_ptr<tgfx::FillStyle> convertFill(const Fill* node) {
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

  std::shared_ptr<tgfx::StrokeStyle> convertStroke(const Stroke* node) {
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

    stroke->setStrokeWidth(node->width);
    stroke->setAlpha(node->alpha);
    stroke->setLineCap(ToTGFX(node->cap));
    stroke->setLineJoin(ToTGFX(node->join));
    stroke->setMiterLimit(node->miterLimit);
    if (!node->dashes.empty()) {
      stroke->setDashes(node->dashes);
      stroke->setDashOffset(node->dashOffset);
    }

    return stroke;
  }

  std::shared_ptr<tgfx::ColorSource> convertColorSource(const ColorSource* node) {
    if (!node) {
      return nullptr;
    }

    switch (node->type()) {
      case ColorSourceType::SolidColor: {
        auto solid = static_cast<const SolidColor*>(node);
        return tgfx::SolidColor::Make(ToTGFX(solid->color));
      }
      case ColorSourceType::LinearGradient: {
        auto grad = static_cast<const LinearGradient*>(node);
        return convertLinearGradient(grad);
      }
      case ColorSourceType::RadialGradient: {
        auto grad = static_cast<const RadialGradient*>(node);
        return convertRadialGradient(grad);
      }
      case ColorSourceType::ImagePattern: {
        auto pattern = static_cast<const ImagePattern*>(node);
        return convertImagePattern(pattern);
      }
      default:
        return nullptr;
    }
  }

  std::shared_ptr<tgfx::ColorSource> convertLinearGradient(const LinearGradient* node) {
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

    return tgfx::Gradient::MakeLinear(ToTGFX(node->startPoint), ToTGFX(node->endPoint), colors,
                                      positions);
  }

  std::shared_ptr<tgfx::ColorSource> convertRadialGradient(const RadialGradient* node) {
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

    return tgfx::Gradient::MakeRadial(ToTGFX(node->center), node->radius, colors, positions);
  }

  std::shared_ptr<tgfx::ColorSource> convertImagePattern(const ImagePattern* node) {
    if (!node || node->image.empty()) {
      return nullptr;
    }

    // Load image from data URI, resource reference, or file path.
    std::shared_ptr<tgfx::Image> image = nullptr;
    if (node->image.find("#") == 0) {
      // Resource reference (e.g., "#image0") - look up in document resources.
      std::string resourceId = node->image.substr(1);
      image = findImageResource(resourceId);
    } else if (node->image.find("data:") == 0) {
      image = ImageFromDataURI(node->image);
    } else {
      // Try as file path.
      std::string imagePath = node->image;
      if (!_options.basePath.empty() && imagePath[0] != '/') {
        imagePath = _options.basePath + imagePath;
      }
      image = tgfx::Image::MakeFromFile(imagePath);
    }

    if (!image) {
      return nullptr;
    }

    // Convert tile modes.
    auto tileModeX = tgfx::TileMode::Clamp;
    auto tileModeY = tgfx::TileMode::Clamp;
    if (node->tileModeX == TileMode::Repeat) {
      tileModeX = tgfx::TileMode::Repeat;
    } else if (node->tileModeX == TileMode::Mirror) {
      tileModeX = tgfx::TileMode::Mirror;
    }
    if (node->tileModeY == TileMode::Repeat) {
      tileModeY = tgfx::TileMode::Repeat;
    } else if (node->tileModeY == TileMode::Mirror) {
      tileModeY = tgfx::TileMode::Mirror;
    }

    auto pattern = tgfx::ImagePattern::Make(image, tileModeX, tileModeY);
    if (pattern && !node->matrix.isIdentity()) {
      pattern->setMatrix(ToTGFX(node->matrix));
    }

    return pattern;
  }

  std::shared_ptr<tgfx::Image> findImageResource(const std::string& resourceId) {
    if (!_resources) {
      return nullptr;
    }
    for (const auto& resource : *_resources) {
      if (resource->nodeType() == NodeType::Image) {
        auto imageNode = static_cast<const Image*>(resource.get());
        if (imageNode->id == resourceId) {
          if (imageNode->source.find("data:") == 0) {
            return ImageFromDataURI(imageNode->source);
          } else {
            std::string imagePath = imageNode->source;
            if (!_options.basePath.empty() && imagePath[0] != '/') {
              imagePath = _options.basePath + imagePath;
            }
            return tgfx::Image::MakeFromFile(imagePath);
          }
        }
      }
    }
    return nullptr;
  }
  std::shared_ptr<tgfx::TrimPath> convertTrimPath(const TrimPath* node) {
    auto trim = std::make_shared<tgfx::TrimPath>();
    trim->setStart(node->start);
    trim->setEnd(node->end);
    trim->setOffset(node->offset);
    return trim;
  }

  std::shared_ptr<tgfx::RoundCorner> convertRoundCorner(const RoundCorner* node) {
    auto round = std::make_shared<tgfx::RoundCorner>();
    round->setRadius(node->radius);
    return round;
  }

  std::shared_ptr<tgfx::MergePath> convertMergePath(const MergePath*) {
    auto merge = std::make_shared<tgfx::MergePath>();
    return merge;
  }

  std::shared_ptr<tgfx::Repeater> convertRepeater(const Repeater* node) {
    auto repeater = std::make_shared<tgfx::Repeater>();
    repeater->setCopies(node->copies);
    repeater->setOffset(node->offset);
    return repeater;
  }

  std::shared_ptr<tgfx::VectorGroup> convertGroup(const Group* node) {
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

  void applyLayerAttributes(const Layer* node, tgfx::Layer* layer) {
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

  std::shared_ptr<tgfx::LayerStyle> convertLayerStyle(const LayerStyle* node) {
    if (!node) {
      return nullptr;
    }

    switch (node->type()) {
      case LayerStyleType::DropShadowStyle: {
        auto style = static_cast<const DropShadowStyle*>(node);
        return tgfx::DropShadowStyle::Make(style->offsetX, style->offsetY, style->blurrinessX,
                                           style->blurrinessY, ToTGFX(style->color));
      }
      case LayerStyleType::InnerShadowStyle: {
        auto style = static_cast<const InnerShadowStyle*>(node);
        return tgfx::InnerShadowStyle::Make(style->offsetX, style->offsetY, style->blurrinessX,
                                            style->blurrinessY, ToTGFX(style->color));
      }
      default:
        return nullptr;
    }
  }

  std::shared_ptr<tgfx::LayerFilter> convertLayerFilter(const LayerFilter* node) {
    if (!node) {
      return nullptr;
    }

    switch (node->type()) {
      case LayerFilterType::BlurFilter: {
        auto filter = static_cast<const pagx::BlurFilter*>(node);
        return tgfx::BlurFilter::Make(filter->blurrinessX, filter->blurrinessY);
      }
      case LayerFilterType::DropShadowFilter: {
        auto filter = static_cast<const DropShadowFilter*>(node);
        return tgfx::DropShadowFilter::Make(filter->offsetX, filter->offsetY, filter->blurrinessX,
                                            filter->blurrinessY, ToTGFX(filter->color));
      }
      default:
        return nullptr;
    }
  }

  LayerBuilder::Options _options = {};
  const std::vector<std::unique_ptr<Node>>* _resources = nullptr;
  std::shared_ptr<tgfx::TextShaper> _textShaper = nullptr;
  std::unordered_map<std::string, std::shared_ptr<tgfx::Layer>> _layerById = {};
  std::vector<std::tuple<std::shared_ptr<tgfx::Layer>, std::string, tgfx::LayerMaskType>>
      _pendingMasks = {};
};

// Public API implementation

PAGXContent LayerBuilder::Build(const Document& document, const Options& options) {
  LayerBuilderImpl builder(options);
  return builder.build(document);
}

PAGXContent LayerBuilder::FromFile(const std::string& filePath, const Options& options) {
  auto document = Document::FromFile(filePath);
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
  auto document = Document::FromXML(data, length);
  if (!document) {
    return {};
  }
  return Build(*document, options);
}

PAGXContent LayerBuilder::FromSVGFile(const std::string& filePath, const Options& options) {
  auto document = SVGImporter::Parse(filePath);
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
