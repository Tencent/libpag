/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#include "MaskCache.h"
#include "rendering/graphics/FeatherMask.h"
#include "rendering/renderers/MaskRenderer.h"

namespace pag {
MaskCache::MaskCache(Layer* layer)
    : FrameCache<tgfx::Path>(layer->startTime, layer->duration), layer(layer) {
  std::vector<TimeRange> timeRanges = {layer->visibleRange()};
  for (auto& mask : layer->masks) {
    mask->excludeVaryingRanges(&timeRanges);
  }
  staticTimeRanges = OffsetTimeRanges(timeRanges, -layer->startTime);
}

tgfx::Path* MaskCache::createCache(Frame layerFrame) {
  auto maskContent = new tgfx::Path();
  RenderMasks(maskContent, layer->masks, layerFrame);
  return maskContent;
}

FeatherMaskCache::FeatherMaskCache(Layer* layer)
    : FrameCache<GraphicContent>(layer->startTime, layer->duration), layer(layer) {
  std::vector<TimeRange> timeRanges = {layer->visibleRange()};
  for (auto& mask : layer->masks) {
    mask->excludeVaryingRanges(&timeRanges);
  }
  staticTimeRanges = OffsetTimeRanges(timeRanges, -layer->startTime);
}

GraphicContent* FeatherMaskCache::createCache(Frame layerFrame) {
  auto featherMask = FeatherMask::MakeFrom(layer->masks, layerFrame);
  return new GraphicContent(featherMask);
}
}  // namespace pag
