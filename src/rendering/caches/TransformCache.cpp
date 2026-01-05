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

#include "TransformCache.h"
#include "rendering/renderers/TransformRenderer.h"

namespace pag {
TransformCache::TransformCache(Layer* layer)
    : FrameCache<Transform>(layer->startTime, layer->duration), layer(layer) {
  std::vector<TimeRange> timeRanges = {layer->visibleRange()};
  if (layer->transform != nullptr) {
    layer->transform->excludeVaryingRanges(&timeRanges);
  }
  if (layer->transform3D != nullptr) {
    layer->transform3D->excludeVaryingRanges(&timeRanges);
  }
  auto parent = layer->parent;
  while (parent != nullptr) {
    if (parent->transform != nullptr) {
      parent->transform->excludeVaryingRanges(&timeRanges);
    }
    if (parent->transform3D != nullptr) {
      parent->transform3D->excludeVaryingRanges(&timeRanges);
    }
    SplitTimeRangesAt(&timeRanges, parent->startTime);
    SplitTimeRangesAt(&timeRanges, parent->startTime + parent->duration);
    parent = parent->parent;
  }
  staticTimeRanges = OffsetTimeRanges(timeRanges, -layer->startTime);
}

Transform* TransformCache::createCache(Frame layerFrame) {
  auto transform = new Transform();
  if (layer->transform == nullptr) {
    return transform;
  }
  RenderTransform(transform, layer->transform, layerFrame);
  auto parent = layer->parent;
  while (parent != nullptr && parent->transform != nullptr) {
    Transform parentTransform = {};
    RenderTransform(&parentTransform, parent->transform, layerFrame);
    transform->matrix.postConcat(parentTransform.matrix);
    parent = parent->parent;
  }
  return transform;
}
}  // namespace pag
