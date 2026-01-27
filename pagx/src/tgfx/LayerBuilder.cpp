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
#include <tuple>
#include <unordered_map>
#include "pagx/PAGXImporter.h"
#include "pagx/nodes/BackgroundBlurStyle.h"
#include "pagx/nodes/BlurFilter.h"
#include "pagx/nodes/ColorSpace.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/ConicGradient.h"
#include "pagx/nodes/Data.h"
#include "pagx/nodes/DiamondGradient.h"
#include "Base64.h"
#include "StringParser.h"
#include "pagx/nodes/DropShadowFilter.h"
#include "pagx/nodes/DropShadowStyle.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Font.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/InnerShadowFilter.h"
#include "pagx/nodes/InnerShadowStyle.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/LinearGradient.h"
#include "pagx/nodes/MergePath.h"
#include "pagx/nodes/Node.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/Polystar.h"
#include "pagx/nodes/RadialGradient.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/Repeater.h"
#include "pagx/nodes/RoundCorner.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TrimPath.h"
#include "tgfx/core/ColorSpace.h"
#include "tgfx/core/CustomTypeface.h"
#include "tgfx/core/Data.h"
#include "tgfx/core/Font.h"
#include "tgfx/core/Image.h"
#include "tgfx/core/Path.h"
#include "tgfx/core/ImageCodec.h"
#include "tgfx/core/TextBlob.h"
#include "tgfx/core/TextBlobBuilder.h"
#include "tgfx/svg/SVGPathParser.h"
#include "tgfx/layers/Layer.h"
#include "tgfx/layers/LayerMaskType.h"
#include "tgfx/layers/VectorLayer.h"
#include "tgfx/layers/filters/BlurFilter.h"
#include "tgfx/layers/filters/DropShadowFilter.h"
#include "tgfx/layers/filters/LayerFilter.h"
#include "tgfx/layers/layerstyles/BackgroundBlurStyle.h"
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
#include "tgfx/layers/vectors/Text.h"
#include "tgfx/layers/vectors/TrimPath.h"
#include "tgfx/layers/vectors/VectorGroup.h"

#ifdef DEBUG
#include <cassert>
#define DEBUG_ASSERT(x) assert(x)
#else
#define DEBUG_ASSERT(x)
#endif

namespace pagx {

// Parse a single hex digit (0-9, a-f, A-F) to its integer value.
static int ParseHexDigit(char c) {
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

// Parse a hex color string (#RGB, #RRGGBB, or #RRGGBBAA) to tgfx::Color.
static tgfx::Color ParseHexColor(const std::string& str) {
  if (str.empty() || str[0] != '#') {
    return tgfx::Color::Black();
  }
  auto hex = str.substr(1);
  if (hex.size() == 3) {
    int r = ParseHexDigit(hex[0]);
    int g = ParseHexDigit(hex[1]);
    int b = ParseHexDigit(hex[2]);
    return {static_cast<float>(r * 17) / 255.0f, static_cast<float>(g * 17) / 255.0f,
            static_cast<float>(b * 17) / 255.0f, 1.0f};
  }
  if (hex.size() == 6) {
    int r = ParseHexDigit(hex[0]) * 16 + ParseHexDigit(hex[1]);
    int g = ParseHexDigit(hex[2]) * 16 + ParseHexDigit(hex[3]);
    int b = ParseHexDigit(hex[4]) * 16 + ParseHexDigit(hex[5]);
    return {static_cast<float>(r) / 255.0f, static_cast<float>(g) / 255.0f,
            static_cast<float>(b) / 255.0f, 1.0f};
  }
  if (hex.size() == 8) {
    int r = ParseHexDigit(hex[0]) * 16 + ParseHexDigit(hex[1]);
    int g = ParseHexDigit(hex[2]) * 16 + ParseHexDigit(hex[3]);
    int b = ParseHexDigit(hex[4]) * 16 + ParseHexDigit(hex[5]);
    int a = ParseHexDigit(hex[6]) * 16 + ParseHexDigit(hex[7]);
    return {static_cast<float>(r) / 255.0f, static_cast<float>(g) / 255.0f,
            static_cast<float>(b) / 255.0f, static_cast<float>(a) / 255.0f};
  }
  return tgfx::Color::Black();
}

// Parse a color string to pagx::Color. Supports:
// - Hex format: #RGB, #RRGGBB, #RRGGBBAA
// - sRGB float format: srgb(r, g, b) or srgb(r, g, b, a)
// - Display P3 format: p3(r, g, b) or p3(r, g, b, a)
static Color ParseColorString(const std::string& str) {
  if (str.empty()) {
    return {};
  }
  // Hex format
  if (str[0] == '#') {
    auto hex = str.substr(1);
    Color color = {};
    color.colorSpace = ColorSpace::SRGB;
    if (hex.size() == 3) {
      int r = ParseHexDigit(hex[0]);
      int g = ParseHexDigit(hex[1]);
      int b = ParseHexDigit(hex[2]);
      color.red = static_cast<float>(r * 17) / 255.0f;
      color.green = static_cast<float>(g * 17) / 255.0f;
      color.blue = static_cast<float>(b * 17) / 255.0f;
      color.alpha = 1.0f;
      return color;
    }
    if (hex.size() == 6) {
      int r = ParseHexDigit(hex[0]) * 16 + ParseHexDigit(hex[1]);
      int g = ParseHexDigit(hex[2]) * 16 + ParseHexDigit(hex[3]);
      int b = ParseHexDigit(hex[4]) * 16 + ParseHexDigit(hex[5]);
      color.red = static_cast<float>(r) / 255.0f;
      color.green = static_cast<float>(g) / 255.0f;
      color.blue = static_cast<float>(b) / 255.0f;
      color.alpha = 1.0f;
      return color;
    }
    if (hex.size() == 8) {
      int r = ParseHexDigit(hex[0]) * 16 + ParseHexDigit(hex[1]);
      int g = ParseHexDigit(hex[2]) * 16 + ParseHexDigit(hex[3]);
      int b = ParseHexDigit(hex[4]) * 16 + ParseHexDigit(hex[5]);
      int a = ParseHexDigit(hex[6]) * 16 + ParseHexDigit(hex[7]);
      color.red = static_cast<float>(r) / 255.0f;
      color.green = static_cast<float>(g) / 255.0f;
      color.blue = static_cast<float>(b) / 255.0f;
      color.alpha = static_cast<float>(a) / 255.0f;
      return color;
    }
  }
  // sRGB float format: srgb(r, g, b) or srgb(r, g, b, a)
  if (str.size() > 5 && str.substr(0, 5) == "srgb(") {
    auto start = str.find('(');
    auto end = str.find(')');
    if (start != std::string::npos && end != std::string::npos) {
      auto inner = str.substr(start + 1, end - start - 1);
      auto components = ParseFloatList(inner);
      if (components.size() >= 3) {
        Color color = {};
        color.red = components[0];
        color.green = components[1];
        color.blue = components[2];
        color.alpha = components.size() >= 4 ? components[3] : 1.0f;
        color.colorSpace = ColorSpace::SRGB;
        return color;
      }
    }
  }
  // Display P3 format: p3(r, g, b) or p3(r, g, b, a)
  if (str.size() > 3 && str.substr(0, 3) == "p3(") {
    auto start = str.find('(');
    auto end = str.find(')');
    if (start != std::string::npos && end != std::string::npos) {
      auto inner = str.substr(start + 1, end - start - 1);
      auto components = ParseFloatList(inner);
      if (components.size() >= 3) {
        Color color = {};
        color.red = components[0];
        color.green = components[1];
        color.blue = components[2];
        color.alpha = components.size() >= 4 ? components[3] : 1.0f;
        color.colorSpace = ColorSpace::DisplayP3;
        return color;
      }
    }
  }
  return {};
}

// Type converter from pagx::Data to tgfx::Data
static std::shared_ptr<tgfx::Data> ToTGFX(const std::shared_ptr<pagx::Data>& data) {
  if (!data) {
    return nullptr;
  }
  return tgfx::Data::MakeWithCopy(data->data(), data->size());
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

  return tgfx::Image::MakeFromEncoded(ToTGFX(data));
}

// Type converters from pagx to tgfx
static tgfx::Point ToTGFX(const Point& p) {
  return tgfx::Point::Make(p.x, p.y);
}

static tgfx::Color ToTGFX(const Color& c) {
  // tgfx::Color is always in sRGB color space. If source color is in Display P3,
  // we need to convert it to sRGB for correct rendering.
  if (c.colorSpace == ColorSpace::DisplayP3) {
    // Convert Display P3 to sRGB using tgfx::ColorSpace
    tgfx::ColorMatrix33 p3ToSRGB = {};
    tgfx::ColorSpace::DisplayP3()->gamutTransformTo(tgfx::ColorSpace::SRGB().get(), &p3ToSRGB);

    float r = c.red * p3ToSRGB.values[0][0] + c.green * p3ToSRGB.values[0][1] +
              c.blue * p3ToSRGB.values[0][2];
    float g = c.red * p3ToSRGB.values[1][0] + c.green * p3ToSRGB.values[1][1] +
              c.blue * p3ToSRGB.values[1][2];
    float b = c.red * p3ToSRGB.values[2][0] + c.green * p3ToSRGB.values[2][1] +
              c.blue * p3ToSRGB.values[2][2];
    return {r, g, b, c.alpha};
  }
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
  }

  PAGXContent build(const PAGXDocument& document) {
    _document = &document;
    // Clear mappings from previous builds.
    _tgfxLayerByPagxLayer.clear();
    _pendingMasks.clear();
    _fontCache.clear();

    PAGXContent content;
    content.width = document.width;
    content.height = document.height;

    // Build layer tree.
    auto rootLayer = tgfx::Layer::Make();
    for (const auto& layer : document.layers) {
      auto childLayer = convertLayer(layer);
      if (childLayer) {
        rootLayer->addChild(childLayer);
      }
    }

    // Apply masks after all layers are built (second pass).
    for (const auto& [layer, maskPagx, maskType] : _pendingMasks) {
      auto it = _tgfxLayerByPagxLayer.find(maskPagx);
      if (it != _tgfxLayerByPagxLayer.end()) {
        auto maskLayer = it->second;
        // tgfx requires mask layer to be visible for hasValidMask() check.
        // The mask layer won't be drawn because maskOwner is set.
        maskLayer->setVisible(true);
        layer->setMask(maskLayer);
        layer->setMaskType(maskType);
      }
    }

    content.root = rootLayer;
    _document = nullptr;
    _tgfxLayerByPagxLayer.clear();
    _pendingMasks.clear();
    _fontCache.clear();
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
      // Register layer for mask lookups.
      _tgfxLayerByPagxLayer[node] = layer;

      applyLayerAttributes(node, layer.get());

      // Queue mask to be applied in second pass.
      if (node->mask != nullptr) {
        _pendingMasks.emplace_back(layer, node->mask, ToTGFXMaskType(node->maskType));
      }

      for (const auto& child : node->children) {
        auto childLayer = convertLayer(child);
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
      auto tgfxElement = convertVectorElement(element);
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

    switch (node->nodeType()) {
      case NodeType::Rectangle:
        return convertRectangle(static_cast<const Rectangle*>(node));
      case NodeType::Ellipse:
        return convertEllipse(static_cast<const Ellipse*>(node));
      case NodeType::Polystar:
        return convertPolystar(static_cast<const Polystar*>(node));
      case NodeType::Path:
        return convertPath(static_cast<const Path*>(node));
      case NodeType::Text:
        return convertText(static_cast<const Text*>(node));
      case NodeType::Fill:
        return convertFill(static_cast<const Fill*>(node));
      case NodeType::Stroke:
        return convertStroke(static_cast<const Stroke*>(node));
      case NodeType::TrimPath:
        return convertTrimPath(static_cast<const TrimPath*>(node));
      case NodeType::RoundCorner:
        return convertRoundCorner(static_cast<const RoundCorner*>(node));
      case NodeType::MergePath:
        return convertMergePath(static_cast<const MergePath*>(node));
      case NodeType::Repeater:
        return convertRepeater(static_cast<const Repeater*>(node));
      case NodeType::Group:
        return convertGroup(static_cast<const Group*>(node));
      case NodeType::TextLayout:
        // TextLayout is handled in convertGroup, not converted directly.
        return nullptr;
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
    if (node->type == PolystarType::Polygon) {
      polystar->setPolystarType(tgfx::PolystarType::Polygon);
    } else {
      polystar->setPolystarType(tgfx::PolystarType::Star);
    }
    return polystar;
  }

  std::shared_ptr<tgfx::ShapePath> convertPath(const Path* node) {
    auto shapePath = std::make_shared<tgfx::ShapePath>();
    if (node->data) {
      shapePath->setPath(ToTGFX(*node->data));
    }
    return shapePath;
  }

  std::shared_ptr<tgfx::Text> convertText(const Text* node) {
    auto tgfxText = std::make_shared<tgfx::Text>();

    // Text must be pre-typeset with GlyphRuns. Use Typesetter before calling LayerBuilder.
    if (node->glyphRuns.empty()) {
      DEBUG_ASSERT(false && "Text element has no GlyphRun data. Use Typesetter to typeset first.");
      return tgfxText;
    }

    auto textBlob = buildTextBlob(node);
    if (textBlob) {
      tgfxText->setTextBlob(textBlob);
    }
    tgfxText->setPosition(tgfx::Point::Make(node->position.x, node->position.y));
    return tgfxText;
  }

  std::shared_ptr<tgfx::TextBlob> buildTextBlob(const Text* node) {
    tgfx::TextBlobBuilder builder;

    for (const auto& run : node->glyphRuns) {
      if (run->glyphs.empty()) {
        continue;
      }

      // Resolve font reference
      auto typeface = buildTypefaceFromFont(run->font);
      if (!typeface) {
        continue;
      }

      // For embedded fonts (CustomTypeface from TextPrecomposer), the glyph paths and positions
      // are already scaled to the target fontSize. Use fontSize=1.0 since paths are absolute.
      tgfx::Font font(typeface, 1);
      size_t count = run->glyphs.size();

      // Determine positioning mode (priority: matrices > xforms > positions > xPositions)
      if (!run->matrices.empty() && run->matrices.size() >= count) {
        // Matrix mode
        auto& buffer = builder.allocRunMatrix(font, count);
        memcpy(buffer.glyphs, run->glyphs.data(), count * sizeof(tgfx::GlyphID));
        auto* matrices = reinterpret_cast<float*>(buffer.positions);
        for (size_t i = 0; i < count; i++) {
          const auto& m = run->matrices[i];
          matrices[i * 6 + 0] = m.a;
          matrices[i * 6 + 1] = m.b;
          matrices[i * 6 + 2] = m.c;
          matrices[i * 6 + 3] = m.d;
          matrices[i * 6 + 4] = m.tx;
          matrices[i * 6 + 5] = m.ty;
        }
      } else if (!run->xforms.empty() && run->xforms.size() >= count) {
        // RSXform mode
        auto& buffer = builder.allocRunRSXform(font, count);
        memcpy(buffer.glyphs, run->glyphs.data(), count * sizeof(tgfx::GlyphID));
        auto* xforms = reinterpret_cast<tgfx::RSXform*>(buffer.positions);
        for (size_t i = 0; i < count; i++) {
          const auto& x = run->xforms[i];
          xforms[i] = tgfx::RSXform::Make(x.scos, x.ssin, x.tx, x.ty);
        }
      } else if (!run->positions.empty() && run->positions.size() >= count) {
        // Point mode
        auto& buffer = builder.allocRunPos(font, count);
        memcpy(buffer.glyphs, run->glyphs.data(), count * sizeof(tgfx::GlyphID));
        auto* positions = reinterpret_cast<tgfx::Point*>(buffer.positions);
        for (size_t i = 0; i < count; i++) {
          positions[i] = tgfx::Point::Make(run->positions[i].x, run->positions[i].y);
        }
      } else if (!run->xPositions.empty() && run->xPositions.size() >= count) {
        // Horizontal mode
        auto& buffer = builder.allocRunPosH(font, count, run->y);
        memcpy(buffer.glyphs, run->glyphs.data(), count * sizeof(tgfx::GlyphID));
        memcpy(buffer.positions, run->xPositions.data(), count * sizeof(float));
      }
    }

    return builder.build();
  }

  std::shared_ptr<tgfx::Typeface> buildTypefaceFromFont(const Font* fontNode) {
    if (!fontNode || fontNode->glyphs.empty()) {
      return nullptr;
    }

    auto it = _fontCache.find(fontNode);
    if (it != _fontCache.end()) {
      return it->second;
    }

    // Determine if font is path-based or image-based
    bool hasPath = false;
    bool hasImage = false;
    for (const auto& glyph : fontNode->glyphs) {
      if (glyph->path != nullptr) {
        hasPath = true;
      }
      if (glyph->image != nullptr) {
        hasImage = true;
      }
    }

    std::shared_ptr<tgfx::Typeface> typeface = nullptr;
    if (hasPath && !hasImage) {
      // Build path-based typeface
      tgfx::PathTypefaceBuilder builder;
      for (const auto& glyph : fontNode->glyphs) {
        if (glyph->path != nullptr) {
          builder.addGlyph(ToTGFX(*glyph->path));
        }
      }
      typeface = builder.detach();
    } else if (hasImage && !hasPath) {
      // Build image-based typeface
      tgfx::ImageTypefaceBuilder builder;
      for (const auto& glyph : fontNode->glyphs) {
        if (glyph->image != nullptr) {
          std::shared_ptr<tgfx::ImageCodec> codec = nullptr;
          auto imageNode = glyph->image;
          if (imageNode->data != nullptr) {
            codec = tgfx::ImageCodec::MakeFrom(ToTGFX(imageNode->data));
          } else if (imageNode->filePath.find("data:") == 0) {
            // Data URI - decode base64 image data
            auto commaPos = imageNode->filePath.find(',');
            if (commaPos != std::string::npos) {
              auto header = imageNode->filePath.substr(0, commaPos);
              if (header.find(";base64") != std::string::npos) {
                auto base64Data = imageNode->filePath.substr(commaPos + 1);
                auto data = Base64Decode(base64Data);
                if (data) {
                  codec = tgfx::ImageCodec::MakeFrom(ToTGFX(data));
                }
              }
            }
          } else if (!imageNode->filePath.empty()) {
            // External file path
            std::string fullPath = imageNode->filePath;
            if (_document && imageNode->filePath[0] != '/' && !_document->basePath.empty()) {
              fullPath = _document->basePath + imageNode->filePath;
            }
            codec = tgfx::ImageCodec::MakeFrom(fullPath);
          }

          if (codec) {
            builder.addGlyph(codec, ToTGFX(glyph->offset));
          }
        }
      }
      typeface = builder.detach();
    }

    if (typeface) {
      _fontCache[fontNode] = typeface;
    }
    return typeface;
  }

  std::shared_ptr<tgfx::FillStyle> convertFill(const Fill* node) {
    auto fill = std::make_shared<tgfx::FillStyle>();

    std::shared_ptr<tgfx::ColorSource> colorSource = nullptr;
    if (node->color) {
      colorSource = convertColorSource(node->color);
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
    if (node->color) {
      colorSource = convertColorSource(node->color);
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

    switch (node->nodeType()) {
      case NodeType::SolidColor: {
        auto solid = static_cast<const SolidColor*>(node);
        return tgfx::SolidColor::Make(ToTGFX(solid->color));
      }
      case NodeType::LinearGradient: {
        auto grad = static_cast<const LinearGradient*>(node);
        return convertLinearGradient(grad);
      }
      case NodeType::RadialGradient: {
        auto grad = static_cast<const RadialGradient*>(node);
        return convertRadialGradient(grad);
      }
      case NodeType::ImagePattern: {
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
    if (!node || node->image == nullptr) {
      return nullptr;
    }

    auto imageNode = node->image;
    std::shared_ptr<tgfx::Image> image = nullptr;
    if (imageNode->data) {
      image = tgfx::Image::MakeFromEncoded(ToTGFX(imageNode->data));
    } else if (imageNode->filePath.find("data:") == 0) {
      image = ImageFromDataURI(imageNode->filePath);
    } else if (!imageNode->filePath.empty()) {
      std::string imagePath = imageNode->filePath;
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
      // Skip TextLayout modifier - layout has been baked into GlyphRun positions by Typesetter
      if (element->nodeType() == NodeType::TextLayout) {
        continue;
      }

      auto tgfxElement = convertVectorElement(element);
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
      auto tgfxStyle = convertLayerStyle(style);
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
      auto tgfxFilter = convertLayerFilter(filter);
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

    switch (node->nodeType()) {
      case NodeType::DropShadowStyle: {
        auto style = static_cast<const DropShadowStyle*>(node);
        return tgfx::DropShadowStyle::Make(style->offsetX, style->offsetY, style->blurX,
                                           style->blurY, ToTGFX(style->color));
      }
      case NodeType::InnerShadowStyle: {
        auto style = static_cast<const InnerShadowStyle*>(node);
        return tgfx::InnerShadowStyle::Make(style->offsetX, style->offsetY, style->blurX,
                                            style->blurY, ToTGFX(style->color));
      }
      case NodeType::BackgroundBlurStyle: {
        auto style = static_cast<const pagx::BackgroundBlurStyle*>(node);
        return tgfx::BackgroundBlurStyle::Make(style->blurX, style->blurY);
      }
      default:
        return nullptr;
    }
  }

  std::shared_ptr<tgfx::LayerFilter> convertLayerFilter(const LayerFilter* node) {
    if (!node) {
      return nullptr;
    }

    switch (node->nodeType()) {
      case NodeType::BlurFilter: {
        auto filter = static_cast<const pagx::BlurFilter*>(node);
        return tgfx::BlurFilter::Make(filter->blurX, filter->blurY);
      }
      case NodeType::DropShadowFilter: {
        auto filter = static_cast<const DropShadowFilter*>(node);
        return tgfx::DropShadowFilter::Make(filter->offsetX, filter->offsetY, filter->blurX,
                                            filter->blurY, ToTGFX(filter->color),
                                            filter->shadowOnly);
      }
      default:
        return nullptr;
    }
  }

  LayerBuilder::Options _options = {};
  const PAGXDocument* _document = nullptr;
  std::unordered_map<const Layer*, std::shared_ptr<tgfx::Layer>> _tgfxLayerByPagxLayer = {};
  std::vector<std::tuple<std::shared_ptr<tgfx::Layer>, const Layer*, tgfx::LayerMaskType>>
      _pendingMasks = {};
  std::unordered_map<const Font*, std::shared_ptr<tgfx::Typeface>> _fontCache = {};
};

// Public API implementation

PAGXContent LayerBuilder::Build(const PAGXDocument& document, const Options& options) {
  LayerBuilderImpl builder(options);
  return builder.build(document);
}

PAGXContent LayerBuilder::FromFile(const std::string& filePath, const Options& options) {
  auto document = PAGXImporter::FromFile(filePath);
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
  auto document = PAGXImporter::FromXML(data, length);
  if (!document) {
    return {};
  }
  return Build(*document, options);
}

}  // namespace pagx
