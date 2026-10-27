/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include <cstdlib>
#include "pagx/html/importer/HTMLBackgroundLayout.h"
#include "pagx/html/importer/HTMLParserContext.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/PathData.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/utils/StringParser.h"
#include "pagx/xml/XMLDOM.h"

namespace pagx {
using namespace html;

HTMLImageIntrinsicSize HTMLParserContext::imageIntrinsicSize(
    const std::shared_ptr<DOMNode>& element, Image* image, const std::string& svgSource,
    const std::string& svgContent) {
  std::string key = image ? "raster:" + image->id : "svg:" + svgSource + svgContent;
  auto found = _imageIntrinsicSizes.find(key);
  HTMLImageIntrinsicSize intrinsic;
  if (found != _imageIntrinsicSizes.end()) intrinsic = found->second;
  else {
    if (image) {
      auto size = decodeImageNativeSize(image);
      if (size.first > 0 && size.second > 0) {
        intrinsic.width = static_cast<float>(size.first);
        intrinsic.height = static_cast<float>(size.second);
        intrinsic.ratio = intrinsic.width / intrinsic.height;
      }
    } else {
      auto dom = svgContent.empty()
                     ? XMLDOM::MakeFromFile(svgSource)
                     : XMLDOM::Make(reinterpret_cast<const uint8_t*>(svgContent.data()),
                                    svgContent.size());
      auto root = dom ? dom->getRootNode() : nullptr;
      if (root && root->name == "svg") {
        std::unordered_map<std::string, std::string> style;
        if (auto* attr = root->findAttribute("style")) ParseStyleString(*attr, style);
        auto dimension = [&](const char* name) {
          auto* attr = root->findAttribute(name);
          auto value = style.count(name) ? style[name] : (attr ? *attr : "");
          HTMLLengthPercentage length;
          if (value.empty() || value.find('%') != std::string::npos ||
              !ParseHTMLLengthPercentage(value, *_valueParser, length) || length.pixels < 0)
            return NAN;
          return length.pixels;
        };
        // SVG presentation dimensions also allow unitless user units.
        auto svgDimension = [&](const char* name) {
          auto* attr = root->findAttribute(name);
          auto value = style.count(name) ? style[name] : (attr ? *attr : "");
          char* end = nullptr;
          float number = std::strtof(value.c_str(), &end);
          if (end != value.c_str() && Trim(end).empty() && std::isfinite(number) && number >= 0)
            return number;
          return dimension(name);
        };
        intrinsic.width = svgDimension("width");
        intrinsic.height = svgDimension("height");
        if (intrinsic.width > 0 && intrinsic.height > 0)
          intrinsic.ratio = intrinsic.width / intrinsic.height;
        else if (auto* attr = root->findAttribute("viewBox")) {
          auto viewBox = ParseFloatList(*attr);
          if (viewBox.size() == 4 && viewBox[2] > 0 && viewBox[3] > 0)
            intrinsic.ratio = viewBox[2] / viewBox[3];
        }
      }
    }
    _imageIntrinsicSizes[key] = intrinsic;
  }
  // Browser-supplied natural sizes are per instance (density, orientation, and the browser's
  // fallback viewport for dimensionless SVG). They must never replace the cached resource
  // dimensions: a bare SVG viewBox supplies an aspect ratio, not a natural width/height.
  {
    auto read = [&](const char* name, float& value) {
      if (auto* attr = element->findAttribute(name)) {
        char* end = nullptr;
        float number = std::strtof(attr->c_str(), &end);
        if (end != attr->c_str() && Trim(end).empty() && std::isfinite(number) && number > 0)
          value = number;
      }
    };
    read("data-pagx-image-width", intrinsic.width);
    read("data-pagx-image-height", intrinsic.height);
    if (intrinsic.width > 0 && intrinsic.height > 0)
      intrinsic.ratio = intrinsic.width / intrinsic.height;
  }
  return intrinsic;
}

void HTMLParserContext::emitImageContent(Layer* layer, const HTMLBoxAttributes& box,
                                         const HTMLImageIntrinsicSize& intrinsic, Image* image,
                                         const std::string& svgSource,
                                         const std::string& svgContent) {
  auto content = BackgroundBoxRect(box, "content-box");
  Rect object;
  if (!ResolveHTMLObjectRect(intrinsic, box.objectFit, box.objectPosition,
                             {content.x, content.y, content.width, content.height}, object)) {
    warn("html: could not resolve replaced image geometry");
    return;
  }
  if (object.width <= 0 || object.height <= 0 || content.width <= 0 || content.height <= 0) return;
  auto* clip = box.borderRadiusSet ? _layerBuilder->makeBackgroundClip(box, content)
                                   : _document->makeNode<Layer>();
  clip->left = content.x;
  clip->top = content.y;
  clip->width = content.width;
  clip->height = content.height;
  clip->includeInLayout = false;
  clip->clipToBounds = true;
  layer->children.push_back(clip);
  if (clip->mask) {
    clip->mask->left = clip->mask->top = 0;
  }
  auto* objectLayer = _document->makeNode<Layer>();
  objectLayer->left = object.x - content.x;
  objectLayer->top = object.y - content.y;
  objectLayer->width = object.width;
  objectLayer->height = object.height;
  objectLayer->includeInLayout = false;
  clip->children.push_back(objectLayer);
  if (image) {
    auto* rect = _document->makeNode<Rectangle>();
    rect->percentWidth = rect->percentHeight = 100;
    objectLayer->contents.push_back(rect);
    auto* pattern = _document->makeNode<ImagePattern>();
    pattern->image = image;
    pattern->scaleMode = ScaleMode::Stretch;
    auto* fill = _document->makeNode<Fill>();
    fill->color = pattern;
    objectLayer->contents.push_back(fill);
  } else {
    // This is an SVG viewport, not a transform of already-imported vector coordinates.
    objectLayer->importDirective.format = "svg";
    objectLayer->importDirective.source = svgSource;
    objectLayer->importDirective.content = svgContent;
  }
}

void HTMLParserContext::finishImageLayouts() {
  if (_pendingImageLayouts.empty()) return;
  std::unordered_map<Layer*, Layer*> parents;
  auto visit = [&](auto&& self, Layer* parent) -> void {
    for (auto* child : parent->children) {
      parents[child] = parent;
      self(self, child);
    }
  };
  for (auto* layer : _document->layers) visit(visit, layer);
  for (auto& pending : _pendingImageLayouts) {
    auto found = parents.find(pending.layer);
    if (pending.layer->flex <= 0 || found == parents.end()) continue;
    // PAGX flex assigns its main-axis size only when that axis is not explicitly fixed.
    if (found->second->layout == LayoutMode::Horizontal) pending.layer->width = NAN;
    if (found->second->layout == LayoutMode::Vertical) pending.layer->height = NAN;
  }
  _document->applyLayout();
  for (auto& pending : _pendingImageLayouts) {
    auto bounds = pending.layer->layoutBounds();
    auto box = pending.box;
    _styleCascade->resolveUsedBoxSize(box, pending.element, bounds.width, bounds.height);
    // Keep the parent's layout slot separate from the fixed captured content and decorations.
    auto* paint = _document->makeNode<Layer>();
    paint->width = bounds.width;
    paint->height = bounds.height;
    paint->includeInLayout = false;
    _layerBuilder->applyBackgroundVisuals(paint, box);
    emitImageContent(paint, box, pending.intrinsic, pending.image, pending.svgSource,
                     pending.svgContent);
    pending.layer->children.push_back(_layerBuilder->maybeSplitBoxShadowFromClip(paint));
    _layerBuilder->applyBoxTransform(pending.layer, box, pending.element);
    applyMaskOrClip(pending.layer, box);
  }
  _pendingImageLayouts.clear();
  _document->applyLayout();
}
}  // namespace pagx
