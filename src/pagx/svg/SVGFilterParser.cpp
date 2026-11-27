///////////////////////////////////////////////////////////////////////////////////////////////
//
// Tencent is pleased to support the open source community by making libpag available.
//
// Copyright (C) 2026 Tencent. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
// except in compliance with the License. You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software distributed under the
// License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
// either express or implied. See the License for the specific language governing permissions
// and limitations under the License.
//
///////////////////////////////////////////////////////////////////////////////////////////////

#include "pagx/svg/SVGFilterParser.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include "pagx/nodes/BlurFilter.h"
#include "pagx/nodes/DropShadowStyle.h"
#include "pagx/nodes/InnerShadowFilter.h"
#include "pagx/nodes/NoiseFilter.h"
#include "pagx/svg/SVGNoiseUtils.h"
#include "pagx/utils/StringParser.h"

namespace pagx {
namespace {

float ParseFloatOr(const std::string& value, float fallback) {
  if (value.empty()) {
    return fallback;
  }
  char* end = nullptr;
  const float parsed = std::strtof(value.c_str(), &end);
  if (end == value.c_str() || !std::isfinite(parsed)) {
    return fallback;
  }
  return parsed;
}

bool IsFinitePositive(float value) {
  return std::isfinite(value) && value > 0.0f;
}

}  // namespace

SVGFilterParser::SVGFilterParser(SVGParserContext* context) : context_(context) {
}

bool SVGFilterParser::parse(const std::shared_ptr<DOMNode>& filterElement,
                            std::vector<LayerFilter*>& filters, std::vector<LayerStyle*>& styles,
                            ShadowOnlyType* outShadowOnlyType) {
  const size_t initialFilterCount = filters.size();
  const size_t initialStyleCount = styles.size();

  std::vector<std::shared_ptr<DOMNode>> primitives;
  bool hasMerge = false;
  auto child = filterElement->getFirstChild();
  while (child) {
    if (!child->name.empty() && child->name.compare(0, 2, "fe") == 0) {
      primitives.push_back(child);
      if (child->name == "feMerge" || child->name == "feBlend") {
        hasMerge = true;
      }
    }
    child = child->getNextSibling();
  }

  const bool shadowOnly = !hasMerge;

  // A blur merged back with the sharp source is a glow composite; blurring the layer itself
  // would soften the artwork instead of adding a halo, so keep the graphic sharp.
  bool blurMergesWithSource = false;
  for (const auto& primitive : primitives) {
    if (primitive->name != "feMerge") {
      continue;
    }
    auto mergeChild = primitive->getFirstChild();
    while (mergeChild) {
      if (mergeChild->name == "feMergeNode" &&
          context_->getAttribute(mergeChild, "in") == "SourceGraphic") {
        blurMergesWithSource = true;
        break;
      }
      mergeChild = mergeChild->getNextSibling();
    }
    if (blurMergesWithSource) {
      break;
    }
  }

  if (parseNoise(primitives, filters) > 0) {
    return true;
  }

  size_t index = 0;
  while (index < primitives.size()) {
    const auto& node = primitives[index];
    if (node->name == "feFlood") {
      ++index;
      continue;
    }

    size_t consumed = 0;
    if (node->name == "feColorMatrix" && context_->getAttribute(node, "in") == "SourceAlpha") {
      consumed = parseColorMatrixShadow(primitives, index, shadowOnly, filters);
    } else if (node->name == "feGaussianBlur" &&
               context_->getAttribute(node, "in") == "SourceAlpha") {
      consumed = parseBlurFirstShadow(primitives, index, shadowOnly, filters);
    } else if (node->name == "feOffset") {
      consumed = parseOffsetShadow(primitives, index, shadowOnly, filters, styles);
    } else if (node->name == "feGaussianBlur") {
      consumed = parseForegroundBlur(primitives, index, blurMergesWithSource, filters);
    }

    if (consumed > 0) {
      index += consumed;
    } else {
      ++index;
    }
  }

  if (outShadowOnlyType != nullptr && shadowOnly && filters.size() > initialFilterCount) {
    for (size_t i = initialFilterCount; i < filters.size(); ++i) {
      auto* filter = filters[i];
      if (filter->nodeType() == NodeType::InnerShadowFilter) {
        *outShadowOnlyType = ShadowOnlyType::InnerShadow;
        break;
      }
      if (filter->nodeType() == NodeType::DropShadowFilter) {
        *outShadowOnlyType = ShadowOnlyType::DropShadow;
        break;
      }
    }
  }

  return filters.size() > initialFilterCount || styles.size() > initialStyleCount;
}

InnerShadowFilter* SVGFilterParser::createInnerShadow(float offsetX, float offsetY, float blurX,
                                                      float blurY, const Color& color,
                                                      bool shadowOnly) {
  auto* innerShadow = context_->_document->makeNode<InnerShadowFilter>();
  innerShadow->offsetX = offsetX;
  innerShadow->offsetY = offsetY;
  innerShadow->blurX = blurX;
  innerShadow->blurY = blurY;
  innerShadow->color = color;
  innerShadow->shadowOnly = shadowOnly;
  return innerShadow;
}

bool SVGFilterParser::isInnerShadowComposite(const std::shared_ptr<DOMNode>& node) const {
  if (!node || node->name != "feComposite" ||
      context_->getAttribute(node, "operator") != "arithmetic") {
    return false;
  }
  const float k1 = ParseFloatOr(context_->getAttribute(node, "k1"), 0.0f);
  const float k2 = ParseFloatOr(context_->getAttribute(node, "k2"), 0.0f);
  const float k3 = ParseFloatOr(context_->getAttribute(node, "k3"), 0.0f);
  // Figma writes k2=-1,k3=1; the SVG exporter writes k1=-1,k2=1. Both subtract the second
  // input, which distinguishes an inner shadow from a positive-coefficient glow.
  return (k2 < 0.0f && k3 > 0.0f) || (k1 < 0.0f && k2 > 0.0f);
}

bool SVGFilterParser::isMorphologyInput(const std::shared_ptr<DOMNode>& node, float* spread) const {
  if (!node || node->name != "feMorphology") {
    return false;
  }
  const std::string in = context_->getAttribute(node, "in");
  if (!in.empty() && in != "SourceAlpha" && in != "SourceGraphic") {
    return false;
  }
  const auto radiusValues = ParseFloatList(context_->getAttribute(node, "radius", "0"));
  if (radiusValues.empty() || !IsFinitePositive(radiusValues[0])) {
    return false;
  }
  const float radius = radiusValues[0];
  const bool erode = context_->getAttribute(node, "operator") == "erode";
  *spread = erode ? -radius : radius;
  return true;
}

bool SVGFilterParser::isBlendOfSourceGraphic(
    const std::vector<std::shared_ptr<DOMNode>>& primitives, size_t blendIndex) const {
  if (blendIndex >= primitives.size() || primitives[blendIndex]->name != "feBlend") {
    return false;
  }
  const auto& node = primitives[blendIndex];
  const std::string in = context_->getAttribute(node, "in");
  if (in == "SourceGraphic") {
    return true;
  }
  if (!in.empty()) {
    return false;
  }
  // A missing "in" resolves to the previous primitive; only accept it when in2 names a result
  // produced earlier in this filter, which keeps unrelated blends from becoming the source.
  const std::string in2 = context_->getAttribute(node, "in2");
  if (in2.empty()) {
    return false;
  }
  if (in2 == "SourceGraphic") {
    return true;
  }
  for (size_t i = 0; i < blendIndex; ++i) {
    if (context_->getAttribute(primitives[i], "result") == in2) {
      return true;
    }
  }
  return false;
}

bool SVGFilterParser::isOffsetSourceInput(const std::vector<std::shared_ptr<DOMNode>>& primitives,
                                          size_t offsetIndex) const {
  if (offsetIndex == 0) {
    return true;
  }
  const auto& previous = primitives[offsetIndex - 1];
  if (previous->name == "feFlood") {
    return ParseFloatOr(context_->getAttribute(previous, "flood-opacity"), 1.0f) == 0.0f;
  }
  if (previous->name == "feBlend") {
    return isBlendOfSourceGraphic(primitives, offsetIndex - 1);
  }
  return false;
}

size_t SVGFilterParser::parseColorMatrixShadow(
    const std::vector<std::shared_ptr<DOMNode>>& primitives, size_t index, bool shadowOnly,
    std::vector<LayerFilter*>& filters) {
  const size_t size = primitives.size();

  // feColorMatrix(SourceAlpha) -> feOffset -> feGaussianBlur -> feComposite -> feColorMatrix
  if (index + 4 < size && primitives[index + 1]->name == "feOffset" &&
      primitives[index + 2]->name == "feGaussianBlur" &&
      primitives[index + 3]->name == "feComposite" &&
      primitives[index + 4]->name == "feColorMatrix") {
    auto [offsetX, offsetY] = context_->parseFilterOffset(primitives[index + 1]);
    auto [blurX, blurY] = context_->parseFilterBlur(primitives[index + 2]);
    auto color = context_->parseFilterColorMatrix(primitives[index + 4]);
    if (isInnerShadowComposite(primitives[index + 3])) {
      auto* innerShadow = createInnerShadow(offsetX, offsetY, blurX, blurY, color, shadowOnly);
      context_->registerFilterAnimationTarget(innerShadow, primitives[index + 2],
                                              primitives[index + 1]);
      filters.push_back(innerShadow);
    } else {
      auto* filter = context_->createDropShadow(offsetX, offsetY, blurX, blurY, color, shadowOnly);
      context_->registerFilterAnimationTarget(filter, primitives[index + 2], primitives[index + 1]);
      filters.push_back(filter);
    }
    return 5;
  }

  // feColorMatrix(SourceAlpha) -> feOffset -> feComposite -> feColorMatrix (inner shadow, no blur)
  if (index + 3 < size && primitives[index + 1]->name == "feOffset" &&
      primitives[index + 2]->name == "feComposite" &&
      primitives[index + 3]->name == "feColorMatrix" &&
      isInnerShadowComposite(primitives[index + 2])) {
    auto [offsetX, offsetY] = context_->parseFilterOffset(primitives[index + 1]);
    auto color = context_->parseFilterColorMatrix(primitives[index + 3]);
    auto* innerShadow = createInnerShadow(offsetX, offsetY, 0.0f, 0.0f, color, shadowOnly);
    context_->registerFilterAnimationTarget(innerShadow, nullptr, primitives[index + 1]);
    filters.push_back(innerShadow);
    return 4;
  }

  // feColorMatrix(SourceAlpha) -> feOffset -> feGaussianBlur -> feColorMatrix
  if (index + 3 < size && primitives[index + 1]->name == "feOffset" &&
      primitives[index + 2]->name == "feGaussianBlur" &&
      primitives[index + 3]->name == "feColorMatrix") {
    auto [offsetX, offsetY] = context_->parseFilterOffset(primitives[index + 1]);
    auto [blurX, blurY] = context_->parseFilterBlur(primitives[index + 2]);
    auto color = context_->parseFilterColorMatrix(primitives[index + 3]);
    auto* filter = context_->createDropShadow(offsetX, offsetY, blurX, blurY, color, shadowOnly);
    context_->registerFilterAnimationTarget(filter, primitives[index + 2], primitives[index + 1]);
    filters.push_back(filter);
    return 4;
  }

  // feColorMatrix(SourceAlpha) -> feOffset -> feColorMatrix (solid shadow without blur)
  if (index + 2 < size && primitives[index + 1]->name == "feOffset" &&
      primitives[index + 2]->name == "feColorMatrix") {
    auto [offsetX, offsetY] = context_->parseFilterOffset(primitives[index + 1]);
    auto color = context_->parseFilterColorMatrix(primitives[index + 2]);
    auto* filter = context_->createDropShadow(offsetX, offsetY, 0.0f, 0.0f, color, shadowOnly);
    context_->registerFilterAnimationTarget(filter, nullptr, primitives[index + 1]);
    filters.push_back(filter);
    return 3;
  }

  return 0;
}

size_t SVGFilterParser::parseBlurFirstShadow(
    const std::vector<std::shared_ptr<DOMNode>>& primitives, size_t index, bool shadowOnly,
    std::vector<LayerFilter*>& filters) {
  const size_t size = primitives.size();
  if (index + 1 >= size || primitives[index + 1]->name != "feOffset") {
    return 0;
  }

  const bool hasComposite = index + 2 < size && primitives[index + 2]->name == "feComposite";
  if (hasComposite && !isInnerShadowComposite(primitives[index + 2])) {
    return 0;
  }

  auto [blurX, blurY] = context_->parseFilterBlur(primitives[index]);
  auto [offsetX, offsetY] = context_->parseFilterOffset(primitives[index + 1]);
  size_t colorIndex = index + 2 + (hasComposite ? 1 : 0);
  Color color = {0.0f, 0.0f, 0.0f, 1.0f};
  const bool hasColor = colorIndex < size && primitives[colorIndex]->name == "feColorMatrix";
  if (hasColor) {
    color = context_->parseFilterColorMatrix(primitives[colorIndex]);
  }

  LayerFilter* filter = nullptr;
  if (hasComposite) {
    filter = createInnerShadow(offsetX, offsetY, blurX, blurY, color, shadowOnly);
  } else {
    filter = context_->createDropShadow(offsetX, offsetY, blurX, blurY, color, shadowOnly);
  }
  context_->registerFilterAnimationTarget(filter, primitives[index], primitives[index + 1]);
  filters.push_back(filter);
  return 2 + (hasComposite ? 1u : 0u) + (hasColor ? 1u : 0u);
}

size_t SVGFilterParser::parseOffsetShadow(const std::vector<std::shared_ptr<DOMNode>>& primitives,
                                          size_t index, bool shadowOnly,
                                          std::vector<LayerFilter*>& filters,
                                          std::vector<LayerStyle*>& styles) {
  const size_t size = primitives.size();
  const auto& node = primitives[index];
  const std::string in = context_->getAttribute(node, "in");
  const bool alphaInput = in == "SourceAlpha";
  const bool sourceInput = in.empty() || in == "SourceGraphic";
  if (!alphaInput && !sourceInput) {
    return 0;
  }

  float spread = 0.0f;
  bool morphologyInput = false;
  if (sourceInput && index > 0 && primitives[index - 1]->name == "feMorphology") {
    morphologyInput = isMorphologyInput(primitives[index - 1], &spread);
  }
  if (sourceInput && !morphologyInput && !isOffsetSourceInput(primitives, index)) {
    return 0;
  }

  if (index + 1 >= size || primitives[index + 1]->name != "feGaussianBlur") {
    return 0;
  }

  auto [offsetX, offsetY] = context_->parseFilterOffset(node);
  auto [blurX, blurY] = context_->parseFilterBlur(primitives[index + 1]);
  Color color = {0.0f, 0.0f, 0.0f, 1.0f};
  const size_t colorIndex = index + 2;
  const bool hasColor = colorIndex < size && primitives[colorIndex]->name == "feColorMatrix";
  if (hasColor) {
    color = context_->parseFilterColorMatrix(primitives[colorIndex]);
  }

  if (morphologyInput) {
    // DropShadowFilter has no spread; represent the feMorphology dilation as a layer style so
    // the spread survives instead of being dropped silently.
    auto* style = context_->_document->makeNode<DropShadowStyle>();
    style->offsetX = offsetX;
    style->offsetY = offsetY;
    style->blurX = blurX;
    style->blurY = blurY;
    style->spread = spread;
    style->color = color;
    styles.push_back(style);
  } else {
    auto* filter = context_->createDropShadow(offsetX, offsetY, blurX, blurY, color, shadowOnly);
    context_->registerFilterAnimationTarget(filter, primitives[index + 1], node);
    filters.push_back(filter);
  }
  return 2 + (hasColor ? 1u : 0u);
}

size_t SVGFilterParser::parseForegroundBlur(const std::vector<std::shared_ptr<DOMNode>>& primitives,
                                            size_t index, bool blurMergesWithSource,
                                            std::vector<LayerFilter*>& filters) {
  const auto& node = primitives[index];
  const std::string in = context_->getAttribute(node, "in");
  bool sourceGraphic = in == "SourceGraphic" || (in.empty() && index == 0);
  if (!sourceGraphic && index > 0 && primitives[index - 1]->name == "feBlend") {
    sourceGraphic = isBlendOfSourceGraphic(primitives, index - 1);
  }
  if (!sourceGraphic || blurMergesWithSource) {
    return 0;
  }

  auto [blurX, blurY] = context_->parseFilterBlur(node);
  auto* blurFilter = context_->_document->makeNode<BlurFilter>();
  blurFilter->blurX = blurX;
  blurFilter->blurY = blurY;
  context_->registerFilterAnimationTarget(blurFilter, node, nullptr);
  filters.push_back(blurFilter);
  return 1;
}

size_t SVGFilterParser::parseNoise(const std::vector<std::shared_ptr<DOMNode>>& primitives,
                                   std::vector<LayerFilter*>& filters) {
  std::shared_ptr<DOMNode> turbulence;
  bool hasShadowPrimitive = false;
  bool hasRgbTransfer = false;
  bool hasDensity = false;
  bool hasOpacity = false;
  float density = 0.5f;
  float opacity = 0.15f;
  std::vector<Color> floodColors;

  for (const auto& primitive : primitives) {
    if (!primitive) {
      continue;
    }
    if (primitive->name == "feTurbulence") {
      turbulence = primitive;
    } else if (primitive->name == "feOffset" || primitive->name == "feGaussianBlur" ||
               primitive->name == "feMorphology") {
      hasShadowPrimitive = true;
    } else if (primitive->name == "feFlood") {
      const float floodOpacity =
          ParseFloatOr(context_->getAttribute(primitive, "flood-opacity"), 1.0f);
      if (floodOpacity <= 0.0f) {
        continue;
      }
      auto color = context_->parseColor(context_->getAttribute(primitive, "flood-color", "black"));
      color.alpha = std::clamp(color.alpha * floodOpacity, 0.0f, 1.0f);
      floodColors.push_back(color);
    } else if (primitive->name == "feComponentTransfer") {
      auto child = primitive->getFirstChild();
      while (child) {
        if (child->name == "feFuncR" || child->name == "feFuncG" || child->name == "feFuncB") {
          hasRgbTransfer = true;
        } else if (child->name == "feFuncA") {
          const auto type = context_->getAttribute(child, "type");
          auto values = ParseFloatList(context_->getAttribute(child, "tableValues"));
          if (type == "discrete" && !values.empty() && !hasDensity) {
            size_t visible = 0;
            for (float value : values) {
              if (value > 0.5f) {
                ++visible;
              }
            }
            const float coverage = static_cast<float>(visible) / static_cast<float>(values.size());
            density = NoiseCoverageToDensity(coverage);
            hasDensity = true;
          } else if (type == "table" && values.size() == 2 && !hasOpacity) {
            opacity = std::clamp(values[1], 0.0f, 1.0f);
            hasOpacity = true;
          }
        }
        child = child->getNextSibling();
      }
    } else if (primitive->name == "feColorMatrix" && !hasOpacity) {
      auto values = ParseFloatList(context_->getAttribute(primitive, "values"));
      // Only the exporter's identity-RGB/alpha-opacity matrix carries Multi noise opacity;
      // luminanceToAlpha matrices also have 20 entries but must not zero the opacity.
      if (values.size() >= 19 && values[0] == 1.0f && values[6] == 1.0f && values[12] == 1.0f) {
        opacity = std::clamp(values[18], 0.0f, 1.0f);
        hasOpacity = true;
      }
    }
  }

  if (!turbulence || hasShadowPrimitive) {
    return 0;
  }
  // PAGX noise is always fractal Perlin noise with three octaves and stitched tiling. Other
  // turbulence types, octave counts and anisotropic base frequencies are approximated.
  const std::string type = context_->getAttribute(turbulence, "type", "fractalNoise");
  if (type != "fractalNoise") {
    return 0;
  }
  if (floodColors.empty() && !hasRgbTransfer) {
    return 0;
  }

  auto baseFrequency = ParseFloatList(context_->getAttribute(turbulence, "baseFrequency", "0"));
  if (baseFrequency.empty()) {
    return 0;
  }
  const float frequency = baseFrequency[0];
  if (!IsFinitePositive(frequency)) {
    return 0;
  }

  auto* noise = context_->_document->makeNode<NoiseFilter>();
  noise->size = std::clamp(1.0f / frequency, 0.01f, 1000.0f);
  const float seed = ParseFloatOr(context_->getAttribute(turbulence, "seed"), 0.0f);
  noise->seed = std::isfinite(seed) ? seed : 0.0f;
  if (hasDensity) {
    noise->density = density;
  }

  if (floodColors.size() >= 2) {
    noise->mode = NoiseMode::Duo;
    noise->firstColor = floodColors[0];
    noise->secondColor = floodColors[1];
  } else if (hasRgbTransfer) {
    noise->mode = NoiseMode::Multi;
    if (hasOpacity) {
      noise->opacity = opacity;
    }
  } else {
    noise->mode = NoiseMode::Mono;
    noise->color = floodColors[0];
  }

  filters.push_back(noise);
  return primitives.size();
}

}  // namespace pagx
