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
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "pagx/utils/TextPaintUtils.h"
#include <algorithm>
#include <cmath>
#include "pagx/nodes/TextModifier.h"
#include "pagx/utils/TextSelectorUtils.h"

namespace pagx {

namespace {

// Mirrors tgfx::TextModifier's BlendColor: the overlay's alpha is its blend weight, and the
// result alpha becomes the accumulated override weight consumed by the base/overlay branch.
Color BlendOverrideColor(const Color& base, const Color& overlay, float factor) {
  if (factor <= 0.0f) {
    return base;
  }
  float blendFactor = overlay.alpha * factor;
  if (blendFactor <= 0.0f) {
    return base;
  }
  float newAlpha = base.alpha + (1.0f - base.alpha) * blendFactor;
  if (blendFactor >= 1.0f) {
    return {overlay.red, overlay.green, overlay.blue, newAlpha, overlay.colorSpace};
  }
  float baseWeight = base.alpha * (1.0f - blendFactor);
  return {(base.red * baseWeight + overlay.red * blendFactor) / newAlpha,
          (base.green * baseWeight + overlay.green * blendFactor) / newAlpha,
          (base.blue * baseWeight + overlay.blue * blendFactor) / newAlpha, newAlpha,
          overlay.colorSpace};
}

}  // namespace

void ApplyTextModifierPaint(const TextModifier& modifier, size_t baseIndex, size_t totalCount,
                            std::vector<GlyphPaintStyle>* styles) {
  if (styles == nullptr || styles->empty() || totalCount == 0) {
    return;
  }
  TextSelectorEvaluator evaluator(modifier.selectors, totalCount);
  for (size_t i = 0; i < styles->size(); i++) {
    float factor = evaluator.weight(baseIndex + i);
    if (factor == 0.0f) {
      continue;
    }
    float absFactor = std::abs(factor);
    auto& style = (*styles)[i];
    style.alpha *= std::max(0.0f, (modifier.alpha - 1.0f) * absFactor + 1.0f);
    if (modifier.fillColor.has_value()) {
      style.fillColor = BlendOverrideColor(style.fillColor, *modifier.fillColor, absFactor);
    }
    if (modifier.strokeColor.has_value()) {
      style.strokeColor = BlendOverrideColor(style.strokeColor, *modifier.strokeColor, absFactor);
    }
    if (modifier.strokeWidth.has_value()) {
      float newFactor = style.strokeWidthFactor + (1.0f - style.strokeWidthFactor) * absFactor;
      if (newFactor > 0.0f) {
        style.strokeWidth =
            style.strokeWidth + (*modifier.strokeWidth - style.strokeWidth) * absFactor / newFactor;
        style.strokeWidthFactor = newFactor;
      }
    }
  }
}

float BlendGlyphStrokeWidth(float baseWidth, const GlyphPaintStyle& style) {
  if (style.strokeWidthFactor <= 0.0f) {
    return baseWidth;
  }
  if (style.strokeWidthFactor >= 1.0f) {
    return style.strokeWidth;
  }
  return baseWidth + (style.strokeWidth - baseWidth) * style.strokeWidthFactor;
}

bool GlyphPaintStylesEqual(const GlyphPaintStyle& a, const GlyphPaintStyle& b) {
  return a.fillColor == b.fillColor && a.strokeColor == b.strokeColor &&
         a.strokeWidth == b.strokeWidth && a.strokeWidthFactor == b.strokeWidthFactor &&
         a.alpha == b.alpha;
}

}  // namespace pagx
