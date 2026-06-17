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

// HTMLParserContext methods that convert specific HTML elements (`<img>`, inline `<svg>`, the
// rounded-image-wrapper fold) into PAGX `Layer` instances. Pure layer-side helpers live in
// `HTMLLayerBuilder` and are reached through `_layerBuilder` here. Text-fragment building is
// handled by `HTMLTextFragmentBuilder` and reached through `_textFragmentBuilder`.

#include <cmath>
#include "base/utils/MathUtil.h"
#include "pagx/html/importer/HTMLDetail.h"
#include "pagx/html/importer/HTMLParserContext.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/Layer.h"

namespace pagx {

using namespace pagx::html;

namespace {

// External `.svg` URL — i.e. ends with `.svg` and is not an inline `data:` URI. Such
// sources route through an SVG import directive rather than a raster image fill.
bool IsExternalSvgSrc(const std::string& src) {
  if (src.size() <= 4) return false;
  if (src.compare(0, 5, "data:") == 0) return false;
  return ToLower(src.substr(src.size() - 4)) == ".svg";
}

// Maps a CSS `object-fit` keyword onto a PAGX `ScaleMode`. Empty input means
// "unset" and yields the CSS default `fill` -> `Stretch` so a plain `<img>` with
// explicit width/height matches the browser's stretch behaviour rather than the
// `ImagePattern` default `LetterBox` (which preserves aspect ratio and would
// shrink images whose intrinsic aspect differs from the box's).
ScaleMode ResolveImageScaleMode(const std::string& objectFit) {
  if (objectFit.empty() || objectFit == "fill") return ScaleMode::Stretch;
  if (objectFit == "contain") return ScaleMode::LetterBox;
  if (objectFit == "cover") return ScaleMode::Zoom;
  // The SubsetTransformer normalises any other keyword to one of the above before
  // emitting the subset HTML; reaching this branch means a hand-written subset
  // file slipped through. Fall back to the CSS default.
  return ScaleMode::Stretch;
}

}  // namespace

bool HTMLParserContext::foldRoundedImageWrapper(const std::shared_ptr<DOMNode>& element,
                                                const HTMLBoxAttributes& box, Layer* layer) {
  if (!box.borderRadiusSet || !box.clipOverflow) return false;
  if (HTMLLayerBuilder::requiresInnerHost(box)) return false;

  std::shared_ptr<DOMNode> img = nullptr;
  for (auto c = element->getFirstChild(); c; c = c->getNextSibling()) {
    if (c->type == DOMNodeType::Element) {
      if (img) return false;
      if (c->name != "img") return false;
      img = c;
    } else if (c->type == DOMNodeType::Text) {
      if (!IsBlankText(c->name)) return false;
    }
  }
  if (!img) return false;

  // Reject SVG sources up front: they ride an import directive, not a raster fill.
  auto* srcAttr = img->findAttribute("src");
  if (!srcAttr || srcAttr->empty()) return false;
  const std::string& src = *srcAttr;
  if (IsExternalSvgSrc(src)) return false;

  // The image must exactly cover the wrapper's content box, anchored at top-left —
  // otherwise the rounded clip would shape only part of the visible image and folding
  // would stretch it across the wrapper.
  HTMLBoxAttributes imgBox = _styleCascade->computeBoxAttributes(img);
  if (std::isnan(imgBox.widthPx) || std::isnan(box.widthPx) ||
      !pag::FloatNearlyEqual(imgBox.widthPx, box.widthPx, HTML_IMAGE_WRAPPER_TOLERANCE_PX)) {
    return false;
  }
  if (std::isnan(imgBox.heightPx) || std::isnan(box.heightPx) ||
      !pag::FloatNearlyEqual(imgBox.heightPx, box.heightPx, HTML_IMAGE_WRAPPER_TOLERANCE_PX)) {
    return false;
  }
  float imgLeft = std::isnan(imgBox.leftPx) ? 0.0f : imgBox.leftPx;
  float imgTop = std::isnan(imgBox.topPx) ? 0.0f : imgBox.topPx;
  if (!pag::FloatNearlyEqual(imgLeft, 0.0f, HTML_IMAGE_WRAPPER_TOLERANCE_PX) ||
      !pag::FloatNearlyEqual(imgTop, 0.0f, HTML_IMAGE_WRAPPER_TOLERANCE_PX)) {
    return false;
  }

  auto* imageNode = registerImageResource(resolveImageSource(src));
  if (!imageNode) return false;

  // The rounded Rectangle emitted by applyBackgroundVisuals() above is now the actual
  // fill geometry, so the rectangular clip the importer would have written from
  // `overflow: hidden` is redundant.
  layer->clipToBounds = false;

  auto fill = _document->makeNode<Fill>();
  auto pattern = _document->makeNode<ImagePattern>();
  pattern->image = imageNode;
  pattern->scaleMode = ResolveImageScaleMode(imgBox.objectFit);
  fill->color = pattern;
  layer->contents.push_back(fill);
  _idAllocator->assign(layer, element);
  return true;
}

Layer* HTMLParserContext::convertImage(const std::shared_ptr<DOMNode>& element,
                                       const HTMLBoxAttributes& box) {
  auto* srcAttr = element->findAttribute("src");
  if (!srcAttr || srcAttr->empty()) {
    warn("html: <img> missing src; skipped");
    return nullptr;
  }
  const std::string& src = *srcAttr;
  if (IsExternalSvgSrc(src)) {
    auto layer = _document->makeNode<Layer>();
    _layerBuilder->applySizeAndPosition(layer, box);
    _layerBuilder->applyLayerAttributes(layer, element, box);
    layer->importDirective.source = resolveImageSource(src);
    layer->importDirective.format = "svg";
    _idAllocator->assign(layer, element);
    return layer;
  }

  auto* imageNode = registerImageResource(resolveImageSource(src));
  if (!imageNode) return nullptr;

  auto layer = _document->makeNode<Layer>();
  _layerBuilder->applySizeAndPosition(layer, box);
  _layerBuilder->applyLayerAttributes(layer, element, box);

  // Honour `border-radius` directly on `<img>`: a CSS `<img style="border-radius: 50%">` is the
  // canonical way to draw a circular avatar, and per-corner radii (e.g. only the top corners
  // rounded for a "card cover" image) follow the same Path-emission path the container code
  // uses. When the image has no border-radius, `buildBackgroundGeometry` falls back to a plain
  // `Rectangle width=100% height=100%`, preserving the legacy emission verbatim.
  layer->contents.push_back(_layerBuilder->buildBackgroundGeometry(box));

  auto fill = _document->makeNode<Fill>();
  auto pattern = _document->makeNode<ImagePattern>();
  pattern->image = imageNode;
  pattern->scaleMode = ResolveImageScaleMode(box.objectFit);
  fill->color = pattern;
  layer->contents.push_back(fill);
  _idAllocator->assign(layer, element);
  return layer;
}

Layer* HTMLParserContext::convertInlineSvg(const std::shared_ptr<DOMNode>& element,
                                           const HTMLBoxAttributes& box,
                                           const HTMLInheritedStyle& inherited) {
  auto layer = _document->makeNode<Layer>();
  _layerBuilder->applySizeAndPosition(layer, box);
  _layerBuilder->applyLayerAttributes(layer, element, box);

  // CSS `color` cascades into the SVG and is what `currentColor` resolves to.
  // `resolveInheritedStyle` returns the style descendants see, but `resolvedTextColor`
  // is the *element's own* colour after applying any `style="color: …"` on the
  // SVG itself — exactly what `currentColor` should resolve to at the SVG root.
  HTMLInheritedStyle svgStyle = _styleCascade->resolveInheritedStyle(element, inherited);
  std::string rootColor = HTMLInlineSvgEmitter::formatColorForAttribute(svgStyle.resolvedTextColor);
  _svgEmitter->resolveCurrentColor(element, rootColor);

  layer->importDirective.content = _svgEmitter->serialize(element);
  layer->importDirective.format = "svg";
  _idAllocator->assign(layer, element);
  return layer;
}

std::string HTMLParserContext::serializeSvg(const std::shared_ptr<DOMNode>& svgNode) {
  return _svgEmitter->serialize(svgNode);
}

Image* HTMLParserContext::registerImageResource(const std::string& imageSource) {
  return _imageResources->registerResource(imageSource);
}

std::string HTMLParserContext::resolveImageSource(const std::string& src) const {
  return _imageResources->resolveSource(src);
}

}  // namespace pagx
