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

#include "ContentCache.h"
#include "rendering/graphics/Picture.h"

namespace pag {
ContentCache::ContentCache(Layer* layer)
    : FrameCache<Content>(layer->startTime, layer->duration), layer(layer) {
}

void ContentCache::update() {
  staticTimeRanges = {layer->visibleRange()};
  excludeVaryingRanges(&staticTimeRanges);
  staticTimeRanges = OffsetTimeRanges(staticTimeRanges, -layer->startTime);
  _contentStatic = !HasVaryingTimeRange(&staticTimeRanges, 0, layer->duration);
  _hasFilters = (!layer->effects.empty() || !layer->layerStyles.empty() || layer->motionBlur ||
                 layer->transform3D);
  // 理论上当图层的matrix带了缩放时也不能缓存，会导致图层样式也会跟着缩放。但目前投影等滤镜的效果看起来区别不明显，性能优化考虑暂时忽略。
  _cacheFilters = _hasFilters && checkCacheFilters();

  if (_cacheFilters) {
    _cacheEnabled = true;
  } else {
    _cacheEnabled = checkCacheEnabled();
  }
}

bool ContentCache::checkCacheFilters() {
  bool varyingLayerStyle = false;
  bool varyingEffect = false;
  bool processVisibleAreaOnly = true;
  if (!layer->layerStyles.empty()) {
    std::vector<TimeRange> timeRanges = {layer->visibleRange()};
    for (auto& layerStyle : layer->layerStyles) {
      layerStyle->excludeVaryingRanges(&timeRanges);
    }
    timeRanges = OffsetTimeRanges(timeRanges, -layer->startTime);
    varyingLayerStyle = HasVaryingTimeRange(&timeRanges, 0, layer->duration);
  }
  if (!layer->effects.empty()) {
    std::vector<TimeRange> timeRanges = {layer->visibleRange()};
    for (auto& effect : layer->effects) {
      effect->excludeVaryingRanges(&timeRanges);
      if (!effect->processVisibleAreaOnly()) {
        processVisibleAreaOnly = false;
      }
    }
    timeRanges = OffsetTimeRanges(timeRanges, -layer->startTime);
    varyingEffect = HasVaryingTimeRange(&timeRanges, 0, layer->duration);
  }

  // 理论上当图层的matrix带了缩放时也不能缓存，会导致图层样式也会跟着缩放。但目前投影等滤镜的效果看起来区别不明显，性能优化考虑暂时忽略。
  return layer->masks.empty() && !layer->motionBlur && !layer->transform3D &&
         processVisibleAreaOnly && !varyingLayerStyle && !varyingEffect;
}

bool ContentCache::checkCacheEnabled() {
  auto cacheEnabled = false;
  if (layer->cachePolicy != CachePolicy::Auto) {
    cacheEnabled = layer->cachePolicy == CachePolicy::Enable;
  } else if (!layer->effects.empty() || !layer->layerStyles.empty() || layer->motionBlur ||
             layer->transform3D) {
    // 滤镜不缓存到纹理内，但含有滤镜每帧的输入都要求是纹理，开启纹理缓存的性能会更高。
    cacheEnabled = true;
  } else {
    // PreComposeLayer 和 ImageLayer 在 createContent() 时会创建
    // Image，不需要上层额外判断是否需要缓存。
    if (layer->type() == LayerType::Text || layer->type() == LayerType::Shape) {
      auto staticContent = !HasVaryingTimeRange(getStaticTimeRanges(), 0, layer->duration);
      cacheEnabled = staticContent && layer->duration > 1;
    }
  }
  return cacheEnabled;
}

Content* ContentCache::createCache(Frame layerFrame) {
  auto content = createContent(layerFrame);
  if (_cacheFilters) {
    auto filterModifier = FilterModifier::Make(layer, layerFrame);
    content->graphic = Graphic::MakeCompose(content->graphic, filterModifier);
  }
  if (_cacheEnabled) {
    content->graphic = Picture::MakeFrom(getCacheID(), content->graphic);
  }
  return content;
}
}  // namespace pag
