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

#include "pagx/svg/SVGFeatureProbe.h"
#include "pagx/nodes/ColorSource.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/nodes/TextBox.h"

namespace pagx {

static void Merge(SVGFeatureFlags* dst, const SVGFeatureFlags& src) {
  dst->hasTextPath |= src.hasTextPath;
  dst->hasTextModifier |= src.hasTextModifier;
  dst->hasConicGradient |= src.hasConicGradient;
  dst->hasDiamondGradient |= src.hasDiamondGradient;
}

static void ProbeColorSource(const ColorSource* source, SVGFeatureFlags* out) {
  if (source == nullptr) {
    return;
  }
  switch (source->nodeType()) {
    case NodeType::ConicGradient:
      out->hasConicGradient = true;
      break;
    case NodeType::DiamondGradient:
      out->hasDiamondGradient = true;
      break;
    default:
      break;
  }
}

SVGFeatureFlags ProbeElementsFeaturesForSVG(const std::vector<Element*>& elements) {
  SVGFeatureFlags out;
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
      case NodeType::Fill:
        ProbeColorSource(static_cast<const Fill*>(el)->color, &out);
        break;
      case NodeType::Stroke:
        ProbeColorSource(static_cast<const Stroke*>(el)->color, &out);
        break;
      case NodeType::Group:
        Merge(&out, ProbeElementsFeaturesForSVG(static_cast<const Group*>(el)->elements));
        break;
      case NodeType::TextBox:
        Merge(&out, ProbeElementsFeaturesForSVG(static_cast<const TextBox*>(el)->elements));
        break;
      default:
        break;
    }
  }
  return out;
}

SVGFeatureFlags ProbeLayerFeaturesForSVG(const Layer* layer) {
  SVGFeatureFlags out;
  if (layer == nullptr || !layer->visible) {
    return out;
  }
  // Aggregating descendant flags here would force the smallest enclosing layer to bake the
  // entire sub-tree into one PNG when any grandchild trips the probe, which both blows up the
  // output size and turns surrounding native content into non-editable raster. Composition
  // layers and child layers are visited separately by writeLayer, each running its own probe.
  Merge(&out, ProbeElementsFeaturesForSVG(layer->contents));
  return out;
}

}  // namespace pagx
