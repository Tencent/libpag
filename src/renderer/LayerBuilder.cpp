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

#include "LayerBuilder.h"
#include <chrono>
#include <tuple>
#include <unordered_map>
#include "ToTGFX.h"
#include "base/utils/Log.h"
#include "pagx/PAGXDocument.h"
#include "pagx/TextLayout.h"
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
#include "pagx/types/ColorSpace.h"
#include "pagx/types/Data.h"
#include "pagx/types/FillRule.h"
#include "pagx/types/LayerPlacement.h"
#include "pagx/types/MergePathMode.h"
#include "pagx/types/RepeaterOrder.h"
#include "pagx/types/SelectorTypes.h"
#include "pagx/types/TileMode.h"
#include "pagx/utils/Base64.h"
#include "renderer/GlyphRunRenderer.h"
#include "tgfx/core/ColorSpace.h"
#include "tgfx/core/CustomTypeface.h"
#include "tgfx/core/Data.h"
#include "tgfx/core/Font.h"
#include "tgfx/core/Image.h"
#include "tgfx/core/ImageCodec.h"
#include "tgfx/core/Path.h"
#include "tgfx/core/TextBlob.h"
#include "tgfx/core/TextBlobBuilder.h"
#include "tgfx/layers/Layer.h"
#include "tgfx/layers/LayerMaskType.h"
#include "tgfx/layers/LayerPaint.h"
#include "tgfx/layers/StrokeAlign.h"
#include "tgfx/layers/VectorLayer.h"
#include "tgfx/layers/filters/BlendFilter.h"
#include "tgfx/layers/filters/BlurFilter.h"
#include "tgfx/layers/filters/ColorMatrixFilter.h"
#include "tgfx/layers/filters/DropShadowFilter.h"
#include "tgfx/layers/filters/InnerShadowFilter.h"
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
#include "tgfx/layers/vectors/TextModifier.h"
#include "tgfx/layers/vectors/TextPath.h"
#include "tgfx/layers/vectors/TextSelector.h"
#include "tgfx/layers/vectors/TrimPath.h"
#include "tgfx/layers/vectors/VectorGroup.h"
#include "tgfx/platform/Print.h"

namespace pagx {

// Decode a data URI (e.g., "data:image/png;base64,...") to an Image.
static std::shared_ptr<tgfx::Image> ImageFromDataURI(const std::string& dataURI) {
  auto data = DecodeBase64DataURI(dataURI);
  if (!data) {
    return nullptr;
  }
  return tgfx::Image::MakeFromEncoded(ToTGFXData(data));
}

// Build context that maintains state during layer tree construction.
class LayerBuilderContext {
 public:
  explicit LayerBuilderContext(int maxImageDimension = 0) : _maxImageDimension(maxImageDimension) {
  }

  LayerBuildResult buildWithMap(const PAGXDocument& document) {
    auto root = build(document);
    LayerBuildResult result = {};
    result.root = root;
    result.layerMap = std::move(_tgfxLayerByPagxLayer);
    return result;
  }

  std::shared_ptr<tgfx::Layer> build(const PAGXDocument& document) {
    // Build layer tree.
    auto rootLayer = tgfx::Layer::Make();
    // [TEMP] Completely disable scrollRect to verify if the tgfx clip processing causes binding
    // error. If error disappears, the root cause is in tgfx ClipStack or applyClip.
    // TODO: Remove this line after root cause is confirmed and fixed.
    // rootLayer->setScrollRect(tgfx::Rect::MakeWH(document.width, document.height));
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

    return rootLayer;
  }

 private:
  std::shared_ptr<tgfx::Layer> convertLayer(const Layer* node) {
    if (!node) {
      return nullptr;
    }

    std::shared_ptr<tgfx::Layer> layer = nullptr;

    if (node->composition != nullptr) {
      // Layer references a composition - render composition content
      layer = convertComposition(node->composition);
    } else if (!node->contents.empty()) {
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

  std::shared_ptr<tgfx::Layer> convertComposition(const Composition* comp) {
    if (!comp) {
      return nullptr;
    }

    auto containerLayer = tgfx::Layer::Make();
    for (const auto& compLayer : comp->layers) {
      auto childLayer = convertLayer(compLayer);
      if (childLayer) {
        containerLayer->addChild(childLayer);
      }
    }
    return containerLayer;
  }

  std::shared_ptr<tgfx::Layer> convertVectorLayer(const Layer* node) {
    auto layer = tgfx::VectorLayer::Make();
    std::vector<std::shared_ptr<tgfx::VectorElement>> contents;
    contents.reserve(node->contents.size());
    for (const auto& element : node->contents) {
      auto tgfxElement = convertVectorElement(element);
      if (tgfxElement) {
        contents.push_back(tgfxElement);
      }
    }
    layer->setContents(contents);
    return layer;
  }

  // Generate TextBlob for a Text node using GlyphRunRenderer with the given inverse matrix.
  // For standalone Text (not inside TextBox), inverseMatrix is Identity.
  // For TextBox children, inverseMatrix cancels the cumulative Group transforms.
  static void PrepareTextBlob(Text* text, const tgfx::Matrix& inverseMatrix) {
    if (!text->glyphData->layoutRuns.empty()) {
      text->glyphData->textBlob =
          GlyphRunRenderer::BuildTextBlobFromLayoutRuns(text->glyphData->layoutRuns, inverseMatrix);
    } else if (!text->glyphRuns.empty()) {
      GlyphRunRenderer::BuildTextBlob(text, inverseMatrix);
    }
  }

  // Prepare TextBlobs for all Text children of a TextBox by applying inverse matrices.
  // This must happen at render time (not layout time) so that tgfx's StrokePainter can
  // detect the Group transform via geometry->matrix changes between apply() and draw().
  void prepareTextBoxTextBlobs(const TextBox* textBox) {
    std::vector<Text*> childText;
    std::vector<tgfx::Matrix> matrices;
    TextLayout::CollectTextElements(textBox->elements, childText, matrices);

    for (size_t i = 0; i < childText.size(); i++) {
      tgfx::Matrix inverse = {};
      if (!matrices[i].invert(&inverse)) {
        continue;
      }
      PrepareTextBlob(childText[i], inverse);
    }
  }

  std::shared_ptr<tgfx::VectorElement> convertVectorElement(Element* node) {
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
        return convertText(static_cast<Text*>(node));
      case NodeType::Fill:
        return convertFill(static_cast<const Fill*>(node));
      case NodeType::Stroke:
        return convertStroke(static_cast<const Stroke*>(node));
      case NodeType::TrimPath:
        return convertTrimPath(static_cast<const TrimPath*>(node));
      case NodeType::TextPath:
        return convertTextPath(static_cast<const TextPath*>(node));
      case NodeType::RoundCorner:
        return convertRoundCorner(static_cast<const RoundCorner*>(node));
      case NodeType::MergePath:
        return convertMergePath(static_cast<const MergePath*>(node));
      case NodeType::Repeater:
        return convertRepeater(static_cast<const Repeater*>(node));
      case NodeType::TextModifier:
        return convertTextModifier(static_cast<const TextModifier*>(node));
      case NodeType::Group:
        return convertGroup(static_cast<const Group*>(node));
      case NodeType::TextBox: {
        auto* textBox = static_cast<const TextBox*>(node);
        if (textBox->elements.empty()) {
          return nullptr;
        }
        prepareTextBoxTextBlobs(textBox);
        return convertGroup(textBox);
      }
      default:
        return nullptr;
    }
  }

  std::shared_ptr<tgfx::Rectangle> convertRectangle(const Rectangle* node) {
    auto rect = tgfx::Rectangle::Make();
    rect->setPosition(ToTGFX(node->position));
    rect->setSize({node->size.width, node->size.height});
    rect->setRoundness(node->roundness);
    rect->setReversed(node->reversed);
    return rect;
  }

  std::shared_ptr<tgfx::Ellipse> convertEllipse(const Ellipse* node) {
    auto ellipse = tgfx::Ellipse::Make();
    ellipse->setPosition(ToTGFX(node->position));
    ellipse->setSize({node->size.width, node->size.height});
    ellipse->setReversed(node->reversed);
    return ellipse;
  }

  std::shared_ptr<tgfx::Polystar> convertPolystar(const Polystar* node) {
    auto polystar = tgfx::Polystar::Make();
    polystar->setPosition(ToTGFX(node->position));
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
    auto shapePath = tgfx::ShapePath::Make();
    shapePath->setPosition(ToTGFX(node->position));
    if (node->data) {
      shapePath->setPath(ToTGFX(*node->data));
    }
    shapePath->setReversed(node->reversed);
    return shapePath;
  }

  std::shared_ptr<tgfx::Text> convertText(Text* node) {
    auto textBlob = node->glyphData->textBlob;
    if (textBlob == nullptr) {
      PrepareTextBlob(node, tgfx::Matrix::I());
      textBlob = node->glyphData->textBlob;
    }
    if (textBlob == nullptr) {
      return nullptr;
    }
    auto tgfxText = tgfx::Text::Make(textBlob, node->glyphData->anchors);
    if (tgfxText) {
      tgfxText->setPosition(tgfx::Point::Make(node->position.x, node->position.y));
    }
    return tgfxText;
  }

  std::shared_ptr<tgfx::FillStyle> convertFill(const Fill* node) {
    std::shared_ptr<tgfx::ColorSource> colorSource = nullptr;
    if (node->color) {
      colorSource = convertColorSource(node->color);
    }
    if (colorSource == nullptr) {
      // Image-backed fills drop out entirely until the underlying image arrives. Falling back to
      // an opaque SolidColor here would paint a black placeholder over every shape that uses an
      // ImagePattern before its decoded image is supplied via loadDecodedImage(); leaving the
      // fill empty keeps those shapes transparent until the image is ready.
      if (node->color && node->color->nodeType() == NodeType::ImagePattern) {
        return nullptr;
      }
      colorSource = tgfx::SolidColor::Make();
    }

    auto fill = tgfx::FillStyle::Make(colorSource);
    if (fill) {
      fill->setAlpha(node->alpha);
      if (node->blendMode != BlendMode::Normal) {
        fill->setBlendMode(ToTGFX(node->blendMode));
      }
      if (node->fillRule != FillRule::Winding) {
        fill->setFillRule(ToTGFX(node->fillRule));
      }
      if (node->placement != LayerPlacement::Background) {
        fill->setPlacement(ToTGFX(node->placement));
      }
    }
    return fill;
  }

  std::shared_ptr<tgfx::StrokeStyle> convertStroke(const Stroke* node) {
    std::shared_ptr<tgfx::ColorSource> colorSource = nullptr;
    if (node->color) {
      colorSource = convertColorSource(node->color);
    }
    if (colorSource == nullptr) {
      colorSource = tgfx::SolidColor::Make();
    }

    auto stroke = tgfx::StrokeStyle::Make(colorSource);
    if (stroke == nullptr) {
      return nullptr;
    }
    stroke->setStrokeWidth(node->width);
    stroke->setAlpha(node->alpha);
    stroke->setLineCap(ToTGFX(node->cap));
    stroke->setLineJoin(ToTGFX(node->join));
    stroke->setMiterLimit(node->miterLimit);
    if (!node->dashes.empty()) {
      stroke->setDashes(node->dashes);
      stroke->setDashOffset(node->dashOffset);
      stroke->setDashAdaptive(node->dashAdaptive);
    }
    if (node->blendMode != BlendMode::Normal) {
      stroke->setBlendMode(ToTGFX(node->blendMode));
    }
    if (node->align != StrokeAlign::Center) {
      stroke->setStrokeAlign(ToTGFX(node->align));
    }
    if (node->placement != LayerPlacement::Background) {
      stroke->setPlacement(ToTGFX(node->placement));
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
      case NodeType::ConicGradient: {
        auto grad = static_cast<const ConicGradient*>(node);
        return convertConicGradient(grad);
      }
      case NodeType::DiamondGradient: {
        auto grad = static_cast<const DiamondGradient*>(node);
        return convertDiamondGradient(grad);
      }
      case NodeType::ImagePattern: {
        auto pattern = static_cast<const ImagePattern*>(node);
        return convertImagePattern(pattern);
      }
      default:
        return nullptr;
    }
  }

  static void ExtractGradientStops(const std::vector<ColorStop*>& colorStops,
                                   std::vector<tgfx::Color>* colors,
                                   std::vector<float>* positions) {
    colors->reserve(colorStops.size());
    positions->reserve(colorStops.size());
    for (const auto* stop : colorStops) {
      colors->push_back(ToTGFX(stop->color));
      positions->push_back(stop->offset);
    }
    if (colors->empty()) {
      *colors = {tgfx::Color::Black(), tgfx::Color::White()};
      *positions = {0.0f, 1.0f};
    }
  }

  static std::shared_ptr<tgfx::ColorSource> ApplyGradientMatrix(
      std::shared_ptr<tgfx::Gradient> gradient, const Matrix& matrix) {
    if (gradient && !matrix.isIdentity()) {
      gradient->setMatrix(ToTGFX(matrix));
    }
    return gradient;
  }

  std::shared_ptr<tgfx::ColorSource> convertLinearGradient(const LinearGradient* node) {
    std::vector<tgfx::Color> colors;
    std::vector<float> positions;
    ExtractGradientStops(node->colorStops, &colors, &positions);
    return ApplyGradientMatrix(
        tgfx::Gradient::MakeLinear(ToTGFX(node->startPoint), ToTGFX(node->endPoint), colors,
                                   positions),
        node->matrix);
  }

  std::shared_ptr<tgfx::ColorSource> convertRadialGradient(const RadialGradient* node) {
    std::vector<tgfx::Color> colors;
    std::vector<float> positions;
    ExtractGradientStops(node->colorStops, &colors, &positions);
    return ApplyGradientMatrix(
        tgfx::Gradient::MakeRadial(ToTGFX(node->center), node->radius, colors, positions),
        node->matrix);
  }

  std::shared_ptr<tgfx::ColorSource> convertConicGradient(const ConicGradient* node) {
    std::vector<tgfx::Color> colors;
    std::vector<float> positions;
    ExtractGradientStops(node->colorStops, &colors, &positions);
    return ApplyGradientMatrix(tgfx::Gradient::MakeConic(ToTGFX(node->center), node->startAngle,
                                                         node->endAngle, colors, positions),
                               node->matrix);
  }

  std::shared_ptr<tgfx::ColorSource> convertDiamondGradient(const DiamondGradient* node) {
    std::vector<tgfx::Color> colors;
    std::vector<float> positions;
    ExtractGradientStops(node->colorStops, &colors, &positions);
    return ApplyGradientMatrix(
        tgfx::Gradient::MakeDiamond(ToTGFX(node->center), node->radius, colors, positions),
        node->matrix);
  }

  std::shared_ptr<tgfx::ColorSource> convertImagePattern(const ImagePattern* node) {
    if (!node || node->image == nullptr) {
      return nullptr;
    }

    auto image = getOrCreateImage(node->image);
    if (!image) {
      // Image data is missing for this node (never loaded or decode failed); paint nothing
      // rather than falling back to an opaque color that would be visibly wrong.
      return nullptr;
    }

    auto patternMatrix = ToTGFX(node->matrix);
    if (_maxImageDimension > 0) {
      auto originalWidth = image->width();
      auto originalHeight = image->height();
      image = getOrCreateScaledImage(node->image, _maxImageDimension);
      if (image->width() != originalWidth || image->height() != originalHeight) {
        auto scaleX = static_cast<float>(originalWidth) / static_cast<float>(image->width());
        auto scaleY = static_cast<float>(originalHeight) / static_cast<float>(image->height());
        patternMatrix.preScale(scaleX, scaleY);
      }
    }

    auto sampling = tgfx::SamplingOptions(ToTGFX(node->filterMode), ToTGFX(node->mipmapMode));
    auto pattern =
        tgfx::ImagePattern::Make(image, ToTGFX(node->tileModeX), ToTGFX(node->tileModeY), sampling);
    if (pattern && !patternMatrix.isIdentity()) {
      pattern->setMatrix(patternMatrix);
    }

    return pattern;
  }

  std::shared_ptr<tgfx::Image> getOrCreateImage(const Image* imageNode) {
    auto it = _imageCache.find(imageNode);
    if (it != _imageCache.end()) {
      return it->second;
    }
    std::shared_ptr<tgfx::Image> image = nullptr;
    if (imageNode->decodedImage) {
      // Host-decoded image supplied via PAGXDocument::loadDecodedImage(). Skip all codec paths
      // and reuse the tgfx::Image directly; mipmap state is assumed to already be applied by the
      // caller so we do not re-wrap here to avoid invalidating any shared cache.
      image = imageNode->decodedImage;
    } else if (imageNode->data) {
      image = tgfx::Image::MakeFromEncoded(ToTGFXData(imageNode->data));
    } else if (imageNode->filePath.find("data:") == 0) {
      image = ImageFromDataURI(imageNode->filePath);
    } else if (!imageNode->filePath.empty()) {
      image = tgfx::Image::MakeFromFile(imageNode->filePath);
    }
    // Enable mipmaps on every decoded image. ImagePattern already defaults to MipmapMode::Linear
    // on the sampling side, so the render pipeline is ready to consume mipmap levels; before this
    // call the image itself was not flagged as mipmapped, which silently disabled the benefit.
    // With mipmaps the codec decodes at a single resolution once and hardware-generated levels
    // cover every zoom ratio, eliminating the per-scale-level re-decode observed during zoom.
    if (image) {
      image = image->makeMipmapped(true);
    }
    // Only memoize successful results. A null entry would cache the absence of a decoded image
    // forever, so a later loadDecodedImage() on the same node would never take effect.
    if (image) {
      _imageCache[imageNode] = image;
    }
    return image;
  }

  std::shared_ptr<tgfx::Image> getOrCreateScaledImage(const Image* imageNode, int maxDimension) {
    auto it = _scaledImageCache.find(imageNode);
    if (it != _scaledImageCache.end()) {
      return it->second;
    }
    auto image = getOrCreateImage(imageNode);
    if (image) {
      image = constrainImageSize(image, maxDimension);
    }
    _scaledImageCache[imageNode] = image;
    return image;
  }

  static std::shared_ptr<tgfx::Image> constrainImageSize(std::shared_ptr<tgfx::Image> image,
                                                         int maxDimension) {
    if (image->width() <= maxDimension && image->height() <= maxDimension) {
      return image;
    }
    float scale = static_cast<float>(maxDimension) /
                  static_cast<float>(std::max(image->width(), image->height()));
    int newWidth = std::max(1, static_cast<int>(static_cast<float>(image->width()) * scale));
    int newHeight = std::max(1, static_cast<int>(static_cast<float>(image->height()) * scale));
    auto scaled = image->makeScaled(newWidth, newHeight);
    return scaled ? scaled : image;
  }

  std::shared_ptr<tgfx::TrimPath> convertTrimPath(const TrimPath* node) {
    auto trim = tgfx::TrimPath::Make();
    trim->setStart(node->start);
    trim->setEnd(node->end);
    trim->setOffset(node->offset);
    if (node->type == TrimType::Continuous) {
      trim->setTrimType(tgfx::TrimPathType::Continuous);
    }
    return trim;
  }

  std::shared_ptr<tgfx::TextPath> convertTextPath(const TextPath* node) {
    auto textPath = tgfx::TextPath::Make();
    if (node->path != nullptr) {
      textPath->setPath(ToTGFX(*node->path));
    }
    textPath->setBaselineOrigin(ToTGFX(node->baselineOrigin));
    textPath->setBaselineAngle(node->baselineAngle);
    textPath->setFirstMargin(node->firstMargin);
    textPath->setLastMargin(node->lastMargin);
    textPath->setPerpendicular(node->perpendicular);
    textPath->setReversed(node->reversed);
    textPath->setForceAlignment(node->forceAlignment);
    return textPath;
  }

  std::shared_ptr<tgfx::RoundCorner> convertRoundCorner(const RoundCorner* node) {
    auto round = tgfx::RoundCorner::Make();
    round->setRadius(node->radius);
    return round;
  }

  std::shared_ptr<tgfx::MergePath> convertMergePath(const MergePath* node) {
    auto merge = tgfx::MergePath::Make();
    if (node->mode != MergePathMode::Append) {
      merge->setMode(ToTGFX(node->mode));
    }
    return merge;
  }

  std::shared_ptr<tgfx::Repeater> convertRepeater(const Repeater* node) {
    auto repeater = tgfx::Repeater::Make();
    repeater->setCopies(node->copies);
    repeater->setOffset(node->offset);
    repeater->setOrder(ToTGFX(node->order));
    repeater->setAnchor(ToTGFX(node->anchor));
    repeater->setPosition(ToTGFX(node->position));
    repeater->setRotation(node->rotation);
    repeater->setScale(ToTGFX(node->scale));
    repeater->setStartAlpha(node->startAlpha);
    repeater->setEndAlpha(node->endAlpha);
    return repeater;
  }

  std::shared_ptr<tgfx::TextModifier> convertTextModifier(const TextModifier* node) {
    auto modifier = tgfx::TextModifier::Make();

    // Convert transform properties
    modifier->setAnchor(ToTGFX(node->anchor));
    modifier->setPosition(ToTGFX(node->position));
    modifier->setRotation(node->rotation);
    modifier->setScale(ToTGFX(node->scale));
    modifier->setSkew(node->skew);
    modifier->setSkewAxis(node->skewAxis);
    modifier->setAlpha(node->alpha);

    // Convert paint properties
    if (node->fillColor.has_value()) {
      modifier->setFillColor(ToTGFX(node->fillColor.value()));
    }
    if (node->strokeColor.has_value()) {
      modifier->setStrokeColor(ToTGFX(node->strokeColor.value()));
    }
    if (node->strokeWidth.has_value()) {
      modifier->setStrokeWidth(node->strokeWidth.value());
    }

    // Convert selectors
    std::vector<std::shared_ptr<tgfx::TextSelector>> tgfxSelectors;
    tgfxSelectors.reserve(node->selectors.size());
    for (const auto* selector : node->selectors) {
      if (selector->nodeType() == NodeType::RangeSelector) {
        auto rangeSelector = static_cast<const RangeSelector*>(selector);
        auto tgfxSelector = std::make_shared<tgfx::RangeSelector>();
        tgfxSelector->setStart(rangeSelector->start);
        tgfxSelector->setEnd(rangeSelector->end);
        tgfxSelector->setOffset(rangeSelector->offset);
        tgfxSelector->setUnit(ToTGFX(rangeSelector->unit));
        tgfxSelector->setShape(ToTGFX(rangeSelector->shape));
        tgfxSelector->setEaseIn(rangeSelector->easeIn);
        tgfxSelector->setEaseOut(rangeSelector->easeOut);
        tgfxSelector->setMode(ToTGFX(rangeSelector->mode));
        tgfxSelector->setWeight(rangeSelector->weight);
        tgfxSelector->setRandomOrder(rangeSelector->randomOrder);
        tgfxSelector->setRandomSeed(static_cast<uint16_t>(rangeSelector->randomSeed));
        tgfxSelectors.push_back(tgfxSelector);
      }
    }
    modifier->setSelectors(std::move(tgfxSelectors));

    return modifier;
  }

  std::shared_ptr<tgfx::VectorGroup> convertGroup(const Group* node) {
    auto group = tgfx::VectorGroup::Make();
    std::vector<std::shared_ptr<tgfx::VectorElement>> elements;
    elements.reserve(node->elements.size());

    for (const auto& element : node->elements) {
      // Skip empty TextBox modifier (no children) - layout has been baked into GlyphRun positions
      // by TextLayout. TextBox with children is rendered as a Group.
      if (element->nodeType() == NodeType::TextBox) {
        auto* textBox = static_cast<const TextBox*>(element);
        if (textBox->elements.empty()) {
          continue;
        }
      }

      auto tgfxElement = convertVectorElement(element);
      if (tgfxElement) {
        elements.push_back(tgfxElement);
      }
    }

    group->setElements(std::move(elements));

    // Apply transform properties
    if (node->anchor.x != 0 || node->anchor.y != 0) {
      group->setAnchor(ToTGFX(node->anchor));
    }
    if (node->position.x != 0 || node->position.y != 0) {
      group->setPosition(ToTGFX(node->position));
    }
    if (node->scale.x != 1 || node->scale.y != 1) {
      group->setScale(ToTGFX(node->scale));
    }
    if (node->rotation != 0) {
      group->setRotation(node->rotation);
    }
    if (node->alpha != 1) {
      group->setAlpha(node->alpha);
    }
    if (node->skew != 0) {
      group->setSkew(node->skew);
    }
    if (node->skewAxis != 0) {
      group->setSkewAxis(node->skewAxis);
    }

    return group;
  }

  void applyLayerAttributes(const Layer* node, tgfx::Layer* layer) {
    layer->setVisible(node->visible);
    layer->setAlpha(node->alpha);
    if (node->blendMode != BlendMode::Normal) {
      layer->setBlendMode(ToTGFX(node->blendMode));
    }

    // Apply transformation: combine x/y translation with matrix
    auto matrix = ToTGFX(node->matrix);
    if (node->x != 0 || node->y != 0) {
      matrix = tgfx::Matrix::MakeTrans(node->x, node->y) * matrix;
    }
    if (!matrix.isIdentity()) {
      layer->setMatrix(matrix);
    }

    // Apply 3D matrix if present (overrides 2D matrix)
    if (!node->matrix3D.isIdentity()) {
      layer->setMatrix3D(ToTGFX3D(node->matrix3D));
    }
    if (node->preserve3D) {
      layer->setPreserve3D(true);
    }

    // Apply layer rendering attributes
    if (!node->antiAlias) {
      layer->setAllowsEdgeAntialiasing(false);
    }
    layer->setAllowsGroupOpacity(node->groupOpacity);
    if (!node->passThroughBackground) {
      layer->setPassThroughBackground(false);
    }
    if (node->hasScrollRect) {
      layer->setScrollRect(ToTGFX(node->scrollRect));
    }

    // Layer styles
    std::vector<std::shared_ptr<tgfx::LayerStyle>> styles;
    styles.reserve(node->styles.size());
    for (const auto& style : node->styles) {
      auto tgfxStyle = convertLayerStyle(style);
      if (tgfxStyle) {
        if (style->excludeChildEffects) {
          tgfxStyle->setExcludeChildEffects(true);
        }
        styles.push_back(tgfxStyle);
      }
    }
    if (!styles.empty()) {
      layer->setLayerStyles(styles);
    }

    // Layer filters
    std::vector<std::shared_ptr<tgfx::LayerFilter>> filters;
    filters.reserve(node->filters.size());
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
        auto tgfxStyle =
            tgfx::DropShadowStyle::Make(style->offsetX, style->offsetY, style->blurX, style->blurY,
                                        ToTGFX(style->color), style->showBehindLayer);
        if (node->blendMode != BlendMode::Normal) {
          tgfxStyle->setBlendMode(ToTGFX(node->blendMode));
        }
        return tgfxStyle;
      }
      case NodeType::InnerShadowStyle: {
        auto style = static_cast<const InnerShadowStyle*>(node);
        auto tgfxStyle = tgfx::InnerShadowStyle::Make(style->offsetX, style->offsetY, style->blurX,
                                                      style->blurY, ToTGFX(style->color));
        if (node->blendMode != BlendMode::Normal) {
          tgfxStyle->setBlendMode(ToTGFX(node->blendMode));
        }
        return tgfxStyle;
      }
      case NodeType::BackgroundBlurStyle: {
        auto style = static_cast<const pagx::BackgroundBlurStyle*>(node);
        auto tgfxStyle =
            tgfx::BackgroundBlurStyle::Make(style->blurX, style->blurY, ToTGFX(style->tileMode));
        if (node->blendMode != BlendMode::Normal) {
          tgfxStyle->setBlendMode(ToTGFX(node->blendMode));
        }
        return tgfxStyle;
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
        return tgfx::BlurFilter::Make(filter->blurX, filter->blurY, ToTGFX(filter->tileMode));
      }
      case NodeType::DropShadowFilter: {
        auto filter = static_cast<const DropShadowFilter*>(node);
        return tgfx::DropShadowFilter::Make(filter->offsetX, filter->offsetY, filter->blurX,
                                            filter->blurY, ToTGFX(filter->color),
                                            filter->shadowOnly);
      }
      case NodeType::InnerShadowFilter: {
        auto filter = static_cast<const pagx::InnerShadowFilter*>(node);
        return tgfx::InnerShadowFilter::Make(filter->offsetX, filter->offsetY, filter->blurX,
                                             filter->blurY, ToTGFX(filter->color),
                                             filter->shadowOnly);
      }
      case NodeType::BlendFilter: {
        auto filter = static_cast<const pagx::BlendFilter*>(node);
        return tgfx::BlendFilter::Make(ToTGFX(filter->color), ToTGFX(filter->blendMode));
      }
      case NodeType::ColorMatrixFilter: {
        auto filter = static_cast<const pagx::ColorMatrixFilter*>(node);
        return tgfx::ColorMatrixFilter::Make(filter->matrix);
      }
      default:
        return nullptr;
    }
  }

  std::unordered_map<const Layer*, std::shared_ptr<tgfx::Layer>> _tgfxLayerByPagxLayer = {};
  std::vector<std::tuple<std::shared_ptr<tgfx::Layer>, const Layer*, tgfx::LayerMaskType>>
      _pendingMasks = {};
  int _maxImageDimension = 0;
  std::unordered_map<const Image*, std::shared_ptr<tgfx::Image>> _imageCache = {};
  std::unordered_map<const Image*, std::shared_ptr<tgfx::Image>> _scaledImageCache = {};
};

// Public API implementation

std::shared_ptr<tgfx::Layer> LayerBuilder::Build(PAGXDocument* document, int maxImageDimension) {
  if (document == nullptr) {
    return nullptr;
  }
  if (!document->isLayoutApplied()) {
    LOGE("LayerBuilder::Build() called before applyLayout(). Call document->applyLayout() first.");
    DEBUG_ASSERT(false);
    return nullptr;
  }

  LayerBuilderContext context(maxImageDimension);
  return context.build(*document);
}

LayerBuildResult LayerBuilder::BuildWithMap(PAGXDocument* document, int maxImageDimension) {
  if (document == nullptr) {
    return {};
  }
  if (!document->isLayoutApplied()) {
    LOGE(
        "LayerBuilder::BuildWithMap() called before applyLayout(). Call document->applyLayout() "
        "first.");
    DEBUG_ASSERT(false);
    return {};
  }

  LayerBuilderContext context(maxImageDimension);
  return context.buildWithMap(*document);
}

}  // namespace pagx
