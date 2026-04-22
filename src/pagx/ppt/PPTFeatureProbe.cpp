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

#include "pagx/ppt/PPTFeatureProbe.h"
#include <cmath>
#include "base/utils/MathUtil.h"
#include "pagx/nodes/BlendFilter.h"
#include "pagx/nodes/ColorMatrixFilter.h"
#include "pagx/nodes/ColorSource.h"
#include "pagx/nodes/ColorStop.h"
#include "pagx/nodes/ConicGradient.h"
#include "pagx/nodes/DiamondGradient.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/LayerFilter.h"
#include "pagx/nodes/LinearGradient.h"
#include "pagx/nodes/RadialGradient.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/nodes/TextBox.h"
#include "pagx/types/BlendMode.h"
#include "pagx/types/Color.h"
#include "pagx/types/Matrix.h"

namespace pagx {

using pag::FloatNearlyZero;

// A 2D affine matrix is representable by OOXML's <a:xfrm> (translation +
// rotation + axis-aligned scale/flip) iff its column basis vectors are
// orthogonal -- i.e. (a, b) . (c, d) == 0 after normalization. Any non-zero
// shear breaks that, and the matrix decomposition in PPTExporter silently
// drops the shear component, producing visibly wrong geometry.
static bool MatrixHasShear(const Matrix& m) {
  float lenA = std::sqrt(m.a * m.a + m.b * m.b);
  float lenC = std::sqrt(m.c * m.c + m.d * m.d);
  if (lenA <= 0 || lenC <= 0) {
    return false;
  }
  float dot = (m.a * m.c + m.b * m.d) / (lenA * lenC);
  return std::fabs(dot) > 1e-3f;
}

// BlendFilter emits via <a:fillOverlay>, which OOXML natively supports for a
// small set of modes. Any other mode on a BlendFilter requires rasterization.
static bool IsSupportedBlendFilterMode(BlendMode mode) {
  switch (mode) {
    case BlendMode::Normal:
    case BlendMode::Multiply:
    case BlendMode::Screen:
    case BlendMode::Darken:
    case BlendMode::Lighten:
      return true;
    default:
      return false;
  }
}

// Layer.blendMode, Fill.blendMode and Stroke.blendMode have no editable OOXML
// encoding at all -- the writer emits plain fills/strokes and silently drops
// the blend mode. Only Normal can survive the vector path; anything else must
// be rasterized so the composite against the backdrop is preserved.
static bool IsSupportedPaintBlendMode(BlendMode mode) {
  return mode == BlendMode::Normal;
}

static bool ColorIsWideGamut(const Color& c) {
  return c.colorSpace != ColorSpace::SRGB;
}

static void Merge(PPTFeatureFlags* dst, const PPTFeatureFlags& src) {
  dst->hasTextPath |= src.hasTextPath;
  dst->hasTextModifier |= src.hasTextModifier;
  dst->hasUnsupportedBlend |= src.hasUnsupportedBlend;
  dst->hasColorMatrix |= src.hasColorMatrix;
  dst->hasWideGamutColor |= src.hasWideGamutColor;
  dst->hasDiamondGradient |= src.hasDiamondGradient;
  dst->hasConicGradient |= src.hasConicGradient;
  dst->hasShearTransform |= src.hasShearTransform;
}

static bool GradientHasWideGamutStop(const std::vector<ColorStop*>& stops) {
  for (const auto* stop : stops) {
    if (stop && ColorIsWideGamut(stop->color)) {
      return true;
    }
  }
  return false;
}

static void ProbeColorSource(const ColorSource* source, PPTFeatureFlags* out) {
  if (source == nullptr) {
    return;
  }
  switch (source->nodeType()) {
    case NodeType::SolidColor:
      if (ColorIsWideGamut(static_cast<const SolidColor*>(source)->color)) {
        out->hasWideGamutColor = true;
      }
      break;
    case NodeType::LinearGradient:
      if (GradientHasWideGamutStop(static_cast<const LinearGradient*>(source)->colorStops)) {
        out->hasWideGamutColor = true;
      }
      break;
    case NodeType::RadialGradient:
      if (GradientHasWideGamutStop(static_cast<const RadialGradient*>(source)->colorStops)) {
        out->hasWideGamutColor = true;
      }
      break;
    case NodeType::ConicGradient:
      out->hasConicGradient = true;
      if (GradientHasWideGamutStop(static_cast<const ConicGradient*>(source)->colorStops)) {
        out->hasWideGamutColor = true;
      }
      break;
    case NodeType::DiamondGradient:
      out->hasDiamondGradient = true;
      if (GradientHasWideGamutStop(static_cast<const DiamondGradient*>(source)->colorStops)) {
        out->hasWideGamutColor = true;
      }
      break;
    default:
      break;
  }
}

PPTFeatureFlags ProbeElementsFeatures(const std::vector<Element*>& elements) {
  PPTFeatureFlags out;
  for (const auto* el : elements) {
    if (el == nullptr) {
      continue;
    }
    switch (el->nodeType()) {
      case NodeType::TextPath:
        out.hasTextPath = true;
        break;
      case NodeType::TextModifier:
      case NodeType::RangeSelector:
        out.hasTextModifier = true;
        break;
      case NodeType::Fill: {
        auto* fill = static_cast<const Fill*>(el);
        if (!IsSupportedPaintBlendMode(fill->blendMode)) {
          out.hasUnsupportedBlend = true;
        }
        ProbeColorSource(fill->color, &out);
        break;
      }
      case NodeType::Stroke: {
        auto* stroke = static_cast<const Stroke*>(el);
        if (!IsSupportedPaintBlendMode(stroke->blendMode)) {
          out.hasUnsupportedBlend = true;
        }
        ProbeColorSource(stroke->color, &out);
        break;
      }
      case NodeType::Group: {
        auto* g = static_cast<const Group*>(el);
        if (!FloatNearlyZero(g->skew)) {
          out.hasShearTransform = true;
        }
        Merge(&out, ProbeElementsFeatures(g->elements));
        break;
      }
      case NodeType::TextBox: {
        auto* tb = static_cast<const TextBox*>(el);
        Merge(&out, ProbeElementsFeatures(tb->elements));
        break;
      }
      default:
        break;
    }
  }
  return out;
}

PPTFeatureFlags ProbeLayerFeatures(const Layer* layer) {
  PPTFeatureFlags out;
  if (layer == nullptr || !layer->visible) {
    return out;
  }
  if (!IsSupportedPaintBlendMode(layer->blendMode)) {
    out.hasUnsupportedBlend = true;
  }
  if (!layer->matrix.isIdentity() && MatrixHasShear(layer->matrix)) {
    out.hasShearTransform = true;
  }
  for (const auto* filter : layer->filters) {
    if (filter == nullptr) {
      continue;
    }
    auto type = filter->nodeType();
    if (type == NodeType::ColorMatrixFilter) {
      out.hasColorMatrix = true;
    } else if (type == NodeType::BlendFilter) {
      auto* blend = static_cast<const BlendFilter*>(filter);
      if (!IsSupportedBlendFilterMode(blend->blendMode)) {
        out.hasUnsupportedBlend = true;
      }
    }
  }

  // Only probe the layer's own contents (groups / text boxes are emitted as
  // part of this layer in writeElements). Composition layers and child layers
  // are visited separately by writeLayer, so each one gets its own probe and
  // can be rasterized at its own scope. Aggregating descendant flags here
  // would force the smallest enclosing layer that has any unsupported feature
  // anywhere in its sub-tree to bake the entire sub-tree into one PNG, which
  // both blows up the output size and turns surrounding native content (e.g.
  // an underlying gradient) into a non-editable raster.
  Merge(&out, ProbeElementsFeatures(layer->contents));
  return out;
}

}  // namespace pagx
