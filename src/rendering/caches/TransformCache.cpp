/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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
#include "base/utils/Log.h"
#include "rendering/renderers/TransformRenderer.h"

namespace pag {
static void PrintStaticTimeRanges(int count, const std::string& key,
                                  const std::vector<TimeRange>& timeRanges) {
  if (count != 18) {
    return;
  }
  std::string text = "";
  for (auto& timeRange : timeRanges) {
    if (!text.empty()) {
      text += ",";
    }
    text += "[" + std::to_string(timeRange.start) + "," + std::to_string(timeRange.end) + "]";
  }
  LOGI("count:%d, key: %s, staticTimeRange: %s", count, key.c_str(), text.c_str());
}

TransformCache::TransformCache(Layer* layer)
    : FrameCache<Transform>(layer->startTime, layer->duration), layer(layer) {
  static int count = 0;
  count++;
  std::vector<TimeRange> timeRanges = {layer->visibleRange()};
  PrintStaticTimeRanges(count, "visibleRange", timeRanges);
  layer->transform->excludeVaryingRanges(&timeRanges);
  PrintStaticTimeRanges(count, "tansform", timeRanges);
  auto parent = layer->parent;
  while (parent != nullptr) {
    parent->transform->excludeVaryingRanges(&timeRanges);
    PrintStaticTimeRanges(count, "parent->transform", timeRanges);
    SplitTimeRangesAt(&timeRanges, parent->startTime);
    PrintStaticTimeRanges(count, "parent->startTime", timeRanges);
    SplitTimeRangesAt(&timeRanges, parent->startTime + parent->duration);
    PrintStaticTimeRanges(count, "parent->endTime", timeRanges);
    parent = parent->parent;
  }
  staticTimeRanges = OffsetTimeRanges(timeRanges, -layer->startTime);
  PrintStaticTimeRanges(count, "OffsetTimeRanges", staticTimeRanges);
}

Transform* TransformCache::createCache(Frame layerFrame) {
  auto transform = new Transform();
  RenderTransform(transform, layer->transform, layerFrame);
  auto parent = layer->parent;
  while (parent != nullptr) {
    Transform parentTransform = {};
    RenderTransform(&parentTransform, parent->transform, layerFrame);
    transform->matrix.postConcat(parentTransform.matrix);
    parent = parent->parent;
  }
  return transform;
}
}  // namespace pag