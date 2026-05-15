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

#include <string>
#include "pagx/html/HTMLParserContext.h"
#include "pagx/utils/StringParser.h"

namespace pagx {

using namespace pagx::detail;

namespace {

void SerializeNode(std::string& out, const std::shared_ptr<DOMNode>& node) {
  if (!node) return;
  if (node->type == DOMNodeType::Text) {
    out += EscapeXml(node->name, /*isAttribute=*/false);
    return;
  }
  out.push_back('<');
  out += node->name;
  for (const auto& attr : node->attributes) {
    out.push_back(' ');
    out += attr.name;
    out += "=\"";
    out += EscapeXml(attr.value, /*isAttribute=*/true);
    out.push_back('"');
  }
  if (!node->firstChild) {
    out += "/>";
    return;
  }
  out.push_back('>');
  auto child = node->firstChild;
  while (child) {
    SerializeNode(out, child);
    child = child->nextSibling;
  }
  out += "</";
  out += node->name;
  out.push_back('>');
}

}  // namespace

Text* HTMLParserContext::buildTextElement(const TextFragment& fragment) {
  auto t = _document->makeNode<Text>();
  t->text = fragment.text;
  t->fontFamily = fragment.fontFamily;
  t->fontStyle = fragment.fontStyleName;
  t->fontSize = fragment.fontSize;
  t->letterSpacing = fragment.letterSpacing;
  return t;
}

Fill* HTMLParserContext::buildSolidFill(const Color& color) {
  auto fill = _document->makeNode<Fill>();
  auto solid = _document->makeNode<SolidColor>();
  solid->color = color;
  fill->color = solid;
  return fill;
}

void HTMLParserContext::applySizeAndPosition(Layer* layer, const HTMLBoxAttributes& box) {
  if (!std::isnan(box.widthPx)) layer->width = box.widthPx;
  if (!std::isnan(box.heightPx)) layer->height = box.heightPx;
  if (!std::isnan(box.widthPct)) layer->percentWidth = box.widthPct;
  if (!std::isnan(box.heightPct)) layer->percentHeight = box.heightPct;
  if (box.absolute) {
    layer->includeInLayout = false;
    if (!std::isnan(box.leftPx)) layer->left = box.leftPx;
    if (!std::isnan(box.rightPx)) layer->right = box.rightPx;
    if (!std::isnan(box.topPx)) layer->top = box.topPx;
    if (!std::isnan(box.bottomPx)) layer->bottom = box.bottomPx;
  }
  // `flex` is an "as-a-child" property; apply it on the outermost Layer regardless of
  // whether this Layer is a container, leaf, or image.
  if (box.flexGrowSet) {
    layer->flex = box.flexGrow;
  }
}

void HTMLParserContext::applyLayoutAttributes(Layer* layer, const HTMLBoxAttributes& box) {
  if (box.displayFlex) {
    layer->layout = box.flexRow ? LayoutMode::Horizontal : LayoutMode::Vertical;
  }
  if (box.gapSet) layer->gap = box.gapPx;
  if (box.paddingSet) layer->padding = box.padding;
  if (!box.alignItems.empty()) {
    const std::string& a = box.alignItems;
    if (a == "stretch") layer->alignment = Alignment::Stretch;
    else if (a == "center")
      layer->alignment = Alignment::Center;
    else if (a == "flex-start" || a == "start")
      layer->alignment = Alignment::Start;
    else if (a == "flex-end" || a == "end")
      layer->alignment = Alignment::End;
    else
      warn("html: unsupported align-items '" + a + "'");
  }
  if (!box.justifyContent.empty()) {
    const std::string& j = box.justifyContent;
    if (j == "flex-start" || j == "start") layer->arrangement = Arrangement::Start;
    else if (j == "center")
      layer->arrangement = Arrangement::Center;
    else if (j == "flex-end" || j == "end")
      layer->arrangement = Arrangement::End;
    else if (j == "space-between")
      layer->arrangement = Arrangement::SpaceBetween;
    else if (j == "space-evenly")
      layer->arrangement = Arrangement::SpaceEvenly;
    else if (j == "space-around")
      layer->arrangement = Arrangement::SpaceAround;
    else
      warn("html: unsupported justify-content '" + j + "'");
  }
}

bool HTMLParserContext::applyBackgroundVisuals(Layer* layer, const HTMLBoxAttributes& box,
                                               bool addRectangle) {
  bool emitted = false;
  Rectangle* rect = nullptr;
  if (addRectangle && (box.backgroundColorSet || !box.backgroundImage.empty() ||
                       box.borderRadiusSet || box.borderSet)) {
    rect = _document->makeNode<Rectangle>();
    rect->percentWidth = 100.0f;
    rect->percentHeight = 100.0f;
    rect->roundness = box.borderRadiusSet ? box.borderRadiusPx : 0.0f;
    layer->contents.push_back(rect);
    emitted = true;
  }

  // Background colour / gradient.
  if (rect && box.backgroundColorSet && box.backgroundImage.empty()) {
    layer->contents.push_back(buildSolidFill(box.backgroundColor));
    emitted = true;
  } else if (rect && !box.backgroundImage.empty()) {
    auto fill = _document->makeNode<Fill>();
    std::string bg = Trim(box.backgroundImage);
    std::string lower = ToLower(bg);
    if (lower.compare(0, 16, "linear-gradient(") == 0) {
      fill->color = parseLinearGradient(bg);
    } else if (lower.compare(0, 16, "radial-gradient(") == 0) {
      fill->color = parseRadialGradient(bg);
    } else if (lower.compare(0, 15, "conic-gradient(") == 0) {
      fill->color = parseConicGradient(bg);
    } else if (lower.compare(0, 4, "url(") == 0) {
      warn("html: background-image: url(...) not supported; use <img>");
    } else {
      warn("html: background-image '" + bg + "' not supported");
    }
    if (!fill->color && box.backgroundColorSet) {
      auto solid = _document->makeNode<SolidColor>();
      solid->color = box.backgroundColor;
      fill->color = solid;
    }
    if (fill->color) {
      layer->contents.push_back(fill);
      emitted = true;
    }
  }

  // Border.
  if (rect && box.borderSet) {
    auto stroke = _document->makeNode<Stroke>();
    auto solid = _document->makeNode<SolidColor>();
    solid->color = box.borderColor;
    stroke->color = solid;
    stroke->width = box.borderWidthPx;
    stroke->align = StrokeAlign::Inside;
    layer->contents.push_back(stroke);
    emitted = true;
  }

  // box-shadow -> DropShadowStyle / InnerShadowStyle list.
  if (!box.boxShadow.empty()) {
    auto shadows = parseShadowList(box.boxShadow);
    for (auto& s : shadows) {
      if (s.inset) {
        auto inner = _document->makeNode<InnerShadowStyle>();
        inner->offsetX = s.offsetX;
        inner->offsetY = s.offsetY;
        inner->blurX = s.blur;
        inner->blurY = s.blur;
        inner->color = s.color;
        layer->styles.push_back(inner);
      } else {
        auto drop = _document->makeNode<DropShadowStyle>();
        drop->offsetX = s.offsetX;
        drop->offsetY = s.offsetY;
        drop->blurX = s.blur;
        drop->blurY = s.blur;
        drop->color = s.color;
        layer->styles.push_back(drop);
      }
    }
    if (!shadows.empty()) emitted = true;
  }

  // backdrop-filter: blur(...) -> BackgroundBlurStyle.
  if (!box.backdropFilter.empty()) {
    auto steps = parseFilterChain(box.backdropFilter);
    for (auto& step : steps) {
      if (step.kind == FilterStep::Kind::Blur) {
        auto bg = _document->makeNode<BackgroundBlurStyle>();
        bg->blurX = step.blurX;
        bg->blurY = step.blurY;
        layer->styles.push_back(bg);
        emitted = true;
      } else {
        warn("html: backdrop-filter '" + step.raw + "' not supported");
      }
    }
  }

  return emitted;
}

void HTMLParserContext::applyLayerAttributes(Layer* layer, const std::shared_ptr<DOMNode>& element,
                                             const HTMLBoxAttributes& box) {
  if (box.opacitySet) layer->alpha = box.opacity;
  if (!box.mixBlendMode.empty()) {
    BlendMode mode = BlendModeFromString(box.mixBlendMode);
    layer->blendMode = mode;
  }
  if (box.clipOverflow) layer->clipToBounds = true;

  // filter chain (excluding backdrop-filter, which is handled as a Layer style).
  if (!box.filter.empty()) {
    auto steps = parseFilterChain(box.filter);
    for (auto& step : steps) {
      if (step.kind == FilterStep::Kind::Blur) {
        auto blur = _document->makeNode<BlurFilter>();
        blur->blurX = step.blurX;
        blur->blurY = step.blurY;
        layer->filters.push_back(blur);
      } else if (step.kind == FilterStep::Kind::DropShadow) {
        auto drop = _document->makeNode<DropShadowFilter>();
        drop->offsetX = step.shadow.offsetX;
        drop->offsetY = step.shadow.offsetY;
        drop->blurX = step.shadow.blur;
        drop->blurY = step.shadow.blur;
        drop->color = step.shadow.color;
        layer->filters.push_back(drop);
      } else {
        warn("html: filter '" + step.raw + "' not supported");
      }
    }
  }

  // data-* attributes -> customData
  for (const auto& attr : element->attributes) {
    if (attr.name.compare(0, 5, "data-") == 0) {
      std::string key = attr.name.substr(5);
      if (IsValidCustomDataKey(key)) {
        layer->customData[key] = attr.value;
      } else {
        warn("html: invalid data-* attribute name '" + attr.name + "'");
      }
    }
  }
  // href on <a>
  if (element->name == "a") {
    auto* href = element->findAttribute("href");
    if (href && !href->empty() && IsValidCustomDataKey("href")) {
      layer->customData["href"] = *href;
    }
  }
}

bool HTMLParserContext::foldRoundedImageWrapper(const std::shared_ptr<DOMNode>& element,
                                                const HTMLBoxAttributes& box, Layer* layer) {
  // Pattern requirements on the wrapper. We need a rounded clip and nothing on the
  // wrapper that would conflict with the folded image (no padding/flex/gap host split,
  // no SVG-style import directive). The wrapper may still carry a background colour /
  // gradient / border / shadow — those have already been emitted into `layer->contents`
  // by applyBackgroundVisuals() and will render underneath the folded image fill.
  if (!box.borderRadiusSet || !box.clipOverflow) return false;
  if (requiresInnerHost(box)) return false;

  // Locate the sole <img> child (no other elements, no significant text).
  std::shared_ptr<DOMNode> img = nullptr;
  for (auto c = element->getFirstChild(); c; c = c->getNextSibling()) {
    if (c->type == DOMNodeType::Element) {
      if (img) return false;
      if (c->name != "img") return false;
      img = c;
    } else if (c->type == DOMNodeType::Text) {
      if (!Trim(c->name).empty()) return false;
    }
  }
  if (!img) return false;

  // The image must exactly cover the wrapper's content box, anchored at top-left. If
  // it's smaller / offset, the wrapper's rounded clip would shape only part of the
  // visible image — folding then would stretch the image to fill the whole wrapper.
  HTMLBoxAttributes imgBox = resolveBox(img);
  auto closeTo = [](float a, float b) { return std::fabs(a - b) < 0.5f; };
  if (std::isnan(imgBox.widthPx) || std::isnan(box.widthPx) ||
      !closeTo(imgBox.widthPx, box.widthPx)) {
    return false;
  }
  if (std::isnan(imgBox.heightPx) || std::isnan(box.heightPx) ||
      !closeTo(imgBox.heightPx, box.heightPx)) {
    return false;
  }
  float imgLeft = std::isnan(imgBox.leftPx) ? 0.0f : imgBox.leftPx;
  float imgTop = std::isnan(imgBox.topPx) ? 0.0f : imgBox.topPx;
  if (!closeTo(imgLeft, 0.0f) || !closeTo(imgTop, 0.0f)) return false;

  // Resolve the image source. SVG sources go through an import directive in the regular
  // <img> path and can't be expressed as a fill pattern, so we leave them to the normal
  // container/child route.
  auto* srcAttr = img->findAttribute("src");
  if (!srcAttr || srcAttr->empty()) return false;
  const std::string& src = *srcAttr;
  bool isSvg = src.size() > 4 && ToLower(src.substr(src.size() - 4)) == ".svg" &&
               src.compare(0, 5, "data:") != 0;
  if (isSvg) return false;

  std::string resolved = LooksAbsolutePath(src) ? src : (_basePath + src);
  auto* imageNode = registerImageResource(resolved);
  if (!imageNode) return false;

  // Drop clipToBounds: the rounded Rectangle is now the actual fill geometry, so the
  // additional rectangular scroll-rect clip the importer would have written from
  // `overflow: hidden` is both redundant and (for the avatar case) misleading when
  // diffing the output against the spec's expected shape pattern.
  layer->clipToBounds = false;

  auto fill = _document->makeNode<Fill>();
  auto pattern = _document->makeNode<ImagePattern>();
  pattern->image = imageNode;
  fill->color = pattern;
  layer->contents.push_back(fill);
  assignElementId(layer, element);
  return true;
}

Layer* HTMLParserContext::convertImage(const std::shared_ptr<DOMNode>& element,
                                       const HTMLBoxAttributes& box) {
  auto* srcAttr = element->findAttribute("src");
  if (!srcAttr || srcAttr->empty()) {
    warn("html: <img> missing src; skipped");
    return nullptr;
  }
  std::string src = *srcAttr;
  if (src.size() > 4 && ToLower(src.substr(src.size() - 4)) == ".svg" &&
      src.compare(0, 5, "data:") != 0) {
    auto layer = _document->makeNode<Layer>();
    applySizeAndPosition(layer, box);
    applyLayerAttributes(layer, element, box);
    layer->importDirective.source = LooksAbsolutePath(src) ? src : (_basePath + src);
    layer->importDirective.format = "svg";
    assignElementId(layer, element);
    return layer;
  }

  std::string resolved = LooksAbsolutePath(src) ? src : (_basePath + src);
  auto* imageNode = registerImageResource(resolved);
  if (!imageNode) return nullptr;

  auto layer = _document->makeNode<Layer>();
  applySizeAndPosition(layer, box);
  applyLayerAttributes(layer, element, box);

  auto rect = _document->makeNode<Rectangle>();
  rect->percentWidth = 100.0f;
  rect->percentHeight = 100.0f;
  layer->contents.push_back(rect);

  auto fill = _document->makeNode<Fill>();
  auto pattern = _document->makeNode<ImagePattern>();
  pattern->image = imageNode;
  fill->color = pattern;
  layer->contents.push_back(fill);
  assignElementId(layer, element);
  return layer;
}

Layer* HTMLParserContext::convertInlineSvg(const std::shared_ptr<DOMNode>& element,
                                           const HTMLBoxAttributes& box) {
  auto layer = _document->makeNode<Layer>();
  applySizeAndPosition(layer, box);
  applyLayerAttributes(layer, element, box);
  layer->importDirective.content = serializeSvg(element);
  layer->importDirective.format = "svg";
  assignElementId(layer, element);
  return layer;
}

std::string HTMLParserContext::serializeSvg(const std::shared_ptr<DOMNode>& svgNode) {
  std::string out;
  SerializeNode(out, svgNode);
  return out;
}

Image* HTMLParserContext::registerImageResource(const std::string& imageSource) {
  if (imageSource.empty()) return nullptr;
  auto it = _imageSourceToId.find(imageSource);
  if (it != _imageSourceToId.end()) return it->second;
  auto imageNode = _document->makeNode<Image>();
  imageNode->id = generateUniqueId("image");
  imageNode->filePath = imageSource;
  _imageSourceToId[imageSource] = imageNode;
  return imageNode;
}

}  // namespace pagx
