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
 * Result of probing a Layer (or a sub-tree of Elements) for features that the OOXML emitter
 * cannot losslessly represent. The PPT exporter consults this struct to decide whether a layer
 * needs to be rasterized as a fallback PNG instead of being walked element-by-element.
 *
 * The probe is intentionally conservative: it only flags features that are strictly impossible
 * to express in the slide XML. Features that have an approximate native mapping (e.g. radial
 * gradients with non-square focal radii) are not flagged.
 */
struct PPTFeatureFlags {
  bool hasTextPath = false;          // TextPath inside an element list.
  bool hasTextModifier = false;      // TextModifier (with its RangeSelector children).
  bool hasUnsupportedBlend = false;  // Blend mode outside of {Normal, Multiply, Screen, Darken,
                                     // Lighten} on a layer or BlendFilter in the layer chain.
  bool hasColorMatrix = false;       // ColorMatrixFilter on the layer.
  bool hasWideGamutColor = false;    // Any ColorSource references a non-sRGB color space.
  bool hasDiamondGradient = false;   // OOXML has no diamond-gradient primitive; the closest preset
                                     // (rectangular path gradient) does not produce a diamond
                                     // pattern and must be rasterized for a faithful result.
  bool hasConicGradient = false;     // OOXML has no sweep/angular gradient primitive; any path
                                     // gradient approximation collapses the angular distribution
                                     // and must be rasterized for a faithful result.
  bool hasShearTransform = false;    // A Group (or Layer matrix) carries a shear/skew that the
                                     // OOXML <a:xfrm> primitive cannot represent (it only supports
                                     // translation, rotation, and axis-aligned scale/flip). The
                                     // exporter's matrix decomposition silently drops the shear,
                                     // producing a visibly wrong shape unless the layer is baked.

  /**
   * Returns true when at least one unsupported feature is present.
   */
  bool needsRasterization(bool rasterizeUnsupportedBlend, bool rasterizeWideGamut) const {
    if (hasTextPath || hasTextModifier || hasColorMatrix || hasConicGradient ||
        hasDiamondGradient || hasShearTransform) {
      return true;
    }
    if (rasterizeUnsupportedBlend && hasUnsupportedBlend) {
      return true;
    }
    if (rasterizeWideGamut && hasWideGamutColor) {
      return true;
    }
    return false;
  }
};

/**
 * Examines a single layer in isolation: its own blend mode and filters, plus the elements it
 * directly owns (recursing through nested Group / TextBox elements). Composition layers and
 * child layers are NOT traversed -- they are visited independently by the writer and probed at
 * their own scope, which keeps any required rasterization fallback localized to the smallest
 * layer that actually contains the unsupported feature.
 */
PPTFeatureFlags ProbeLayerFeatures(const Layer* layer);

/**
 * Walks an element list (and any nested Group elements) and returns the union of feature flags
 * found within. Useful when probing inside a TextBox or Group without re-traversing its parent.
 */
PPTFeatureFlags ProbeElementsFeatures(const std::vector<Element*>& elements);

}  // namespace pagx
