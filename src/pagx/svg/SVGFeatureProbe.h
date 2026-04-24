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

#pragma once

#include <vector>
#include "pagx/nodes/Element.h"
#include "pagx/nodes/Layer.h"

namespace pagx {

/**
 * Result of probing a Layer (or sub-tree of Elements) for features that the SVG emitter cannot
 * losslessly represent. The SVG exporter consults this struct to decide whether a layer needs
 * to be baked to an embedded PNG instead of being walked element-by-element.
 *
 * SVG already has native analogues for most PPT-unsupported features: every blend mode maps to
 * `mix-blend-mode`, `feColorMatrix` handles ColorMatrixFilter, display-p3 colors travel in CSS
 * `color()`, and arbitrary affine (including shear) rides on `matrix()`. So this probe is much
 * narrower than PPTFeatureProbe: it only flags features for which SVG has no faithful primitive.
 */
struct SVGFeatureFlags {
  bool hasTextPath = false;         // TextPath element (PAGX's path-following text).
  bool hasTextModifier = false;     // TextModifier / RangeSelector (per-glyph animation styling).
  bool hasConicGradient = false;    // No SVG conic/sweep gradient primitive.
  bool hasDiamondGradient = false;  // No SVG diamond gradient primitive.
  bool hasBackgroundBlurStyle =
      false;  // Needs backdrop pixels; SVG's feGaussianBlur on BackgroundImage requires the
              // deprecated enable-background attribute that modern renderers ignore.

  /**
   * Returns true when at least one feature is present.
   */
  bool needsRasterization() const {
    return hasTextPath || hasTextModifier || hasConicGradient || hasDiamondGradient ||
           hasBackgroundBlurStyle;
  }

  /**
   * BackgroundBlurStyle depends on the backdrop pixels below the layer, so the renderer must
   * composite the whole scene clipped to the layer's bounds. Every other flagged feature is
   * self-contained and renders correctly against an empty canvas.
   */
  bool requiresBackdrop() const {
    return hasBackgroundBlurStyle;
  }
};

/**
 * Examines a single layer in isolation: its own styles (BackgroundBlurStyle) plus the elements
 * it directly owns (recursing through nested Group / TextBox). Composition layers and child
 * layers are NOT traversed — they are visited independently by the writer.
 */
SVGFeatureFlags ProbeLayerFeaturesForSVG(const Layer* layer);

/**
 * Walks an element list (and any nested Group / TextBox elements) and returns the union of
 * feature flags found within.
 */
SVGFeatureFlags ProbeElementsFeaturesForSVG(const std::vector<Element*>& elements);

}  // namespace pagx
