/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
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

#include "LayerStyleFilter.h"
#include "rendering/filters/layerstyle/DropShadowFilter.h"
#include "rendering/filters/layerstyle/GradientOverlayFilter.h"
#include "rendering/filters/layerstyle/OuterGlowFilter.h"
#include "rendering/filters/layerstyle/StrokeFilter.h"

namespace pag {

std::unique_ptr<LayerStyleFilter> LayerStyleFilter::Make(LayerStyle* layerStyle) {
  LayerStyleFilter* filter = nullptr;
  switch (layerStyle->type()) {
    case LayerStyleType::DropShadow:
      filter = new DropShadowFilter(reinterpret_cast<DropShadowStyle*>(layerStyle));
      break;
    case LayerStyleType::OuterGlow:
      filter = new OuterGlowFilter(reinterpret_cast<OuterGlowStyle*>(layerStyle));
      break;
    case LayerStyleType::Stroke:
      filter = new StrokeFilter(reinterpret_cast<StrokeStyle*>(layerStyle));
      break;
    case LayerStyleType::GradientOverlay:
      filter = new GradientOverlayFilter(reinterpret_cast<GradientOverlayStyle*>(layerStyle));
      break;
    default:
      break;
  }
  if (!filter) {
    return nullptr;
  }
  return std::unique_ptr<LayerStyleFilter>(filter);
}

}  // namespace pag
