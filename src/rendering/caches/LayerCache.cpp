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

#include "LayerCache.h"
#include "rendering/caches/ImageContentCache.h"
#include "rendering/caches/PreComposeContentCache.h"
#include "rendering/caches/ShapeContentCache.h"
#include "rendering/caches/SolidContentCache.h"
#include "rendering/caches/TextContentCache.h"
#include "rendering/filters/utils/Filter3DFactory.h"

namespace pag {

LayerCache* LayerCache::Get(Layer* layer) {
  std::lock_guard<std::mutex> autoLock(layer->locker);
  if (layer->cache == nullptr) {
    layer->cache = new LayerCache(layer);
  }
  return static_cast<LayerCache*>(layer->cache);
}

LayerCache::LayerCache(Layer* layer) : layer(layer) {
  switch (layer->type()) {
    case LayerType::Shape:
      contentCache = new ShapeContentCache(static_cast<ShapeLayer*>(layer));
      break;
    case LayerType::Text:
      contentCache = new TextContentCache(static_cast<TextLayer*>(layer));
      break;
    case LayerType::Solid:
      contentCache = new SolidContentCache(static_cast<SolidLayer*>(layer));
      break;
    case LayerType::Image:
      contentCache = new ImageContentCache(static_cast<ImageLayer*>(layer));
      break;
    case LayerType::PreCompose:
      contentCache = new PreComposeContentCache(static_cast<PreComposeLayer*>(layer));
      break;
    default:
      contentCache = new EmptyContentCache(layer);
      break;
  }
  contentCache->update();
  transformCache = new TransformCache(layer);
  bool hasFeatherMask = false;
  for (auto mask : layer->masks) {
    if (mask->maskFeather != nullptr ||
        (mask->maskOpacity->animatable() || mask->maskOpacity->value != 255)) {
      hasFeatherMask = true;
      break;
    }
  }
  if (hasFeatherMask) {
    featherMaskCache = new FeatherMaskCache(layer);
  } else if (!layer->masks.empty()) {
    maskCache = new MaskCache(layer);
  }
  updateStaticTimeRanges();
  auto temp = layer->getScaleFactor();
  scaleFactor = {ToTGFX(temp.first), ToTGFX(temp.second)};
}

LayerCache::~LayerCache() {
  delete transformCache;
  delete maskCache;
  delete contentCache;
  delete featherMaskCache;
}

Transform* LayerCache::getTransform(Frame contentFrame) {
  return transformCache->getCache(contentFrame);
}

tgfx::Path* LayerCache::getMasks(Frame contentFrame) {
  auto mask = maskCache ? maskCache->getCache(contentFrame) : nullptr;
  if (mask && mask->isEmpty()) {
    return nullptr;
  }
  return mask;
}

std::shared_ptr<Modifier> LayerCache::getFeatherMask(Frame contentFrame) {
  if (featherMaskCache == nullptr) {
    return nullptr;
  }
  auto featherMaskContent = featherMaskCache->getCache(contentFrame);
  return Modifier::MakeMask(featherMaskContent->graphic, false, false);
}

Content* LayerCache::getContent(Frame contentFrame) {
  return contentCache->getCache(contentFrame);
}

Layer* LayerCache::getLayer() const {
  return layer;
}

std::pair<tgfx::Point, tgfx::Point> LayerCache::getScaleFactor() const {
  return scaleFactor;
}

bool LayerCache::checkFrameChanged(Frame contentFrame, Frame lastContentFrame) {
  if (contentFrame == lastContentFrame) {
    return false;
  }
  if ((contentFrame < 0 || contentFrame >= layer->duration) &&
      (lastContentFrame < 0 || lastContentFrame >= layer->duration)) {
    return false;
  }
  contentFrame = ConvertFrameByStaticTimeRanges(staticTimeRanges, contentFrame);
  lastContentFrame = ConvertFrameByStaticTimeRanges(staticTimeRanges, lastContentFrame);

  return contentFrame != lastContentFrame;
}

bool LayerCache::contentVisible(Frame contentFrame) {
  if (contentFrame < 0 || contentFrame >= layer->duration) {
    return false;
  }
  if (layer->transform3D && !Has3DSupport()) {
    return false;
  }
  auto layerTransform = getTransform(contentFrame);
  return layerTransform->visible();
}

void LayerCache::updateStaticTimeRanges() {
  // layer->startTime is excluded from all time ranges.
  if (layer->type() == LayerType::PreCompose &&
      static_cast<PreComposeLayer*>(layer)->composition->type() == CompositionType::Vector) {
    // 矢量预合成的内容静态区间包含了子项，
    // 这里的 staticTimeRanges 只记录 Layer 自身的静态区间，
    // 避免在 Layer->gotoFrame() 里重复判断非真实子项的帧号变化。
    TimeRange range = {0, layer->duration - 1};
    staticTimeRanges.push_back(range);
  } else {
    staticTimeRanges = *contentCache->getStaticTimeRanges();
  }
  MergeTimeRanges(&staticTimeRanges, transformCache->getStaticTimeRanges());
  if (maskCache) {
    MergeTimeRanges(&staticTimeRanges, maskCache->getStaticTimeRanges());
  }
  if (featherMaskCache) {
    MergeTimeRanges(&staticTimeRanges, featherMaskCache->getStaticTimeRanges());
  }
  if (layer->trackMatteLayer) {
    auto timeRanges = getTrackMatteStaticTimeRanges();
    MergeTimeRanges(&staticTimeRanges, &timeRanges);
  }
  if (!layer->layerStyles.empty() || !layer->effects.empty() || layer->transform3D) {
    auto timeRanges = getFilterStaticTimeRanges();
    MergeTimeRanges(&staticTimeRanges, &timeRanges);
  }
  if (layer->motionBlur) {
    // MotionBlur是根据Transform来处理图像，
    // 如果staticTransformRanges中的start帧被加上MotionBlur的效果
    // （start - 1帧到start帧之间有Transform),但后续因为静态空间的计算，
    // 不重新绘制该图层，导致后续静态区间里一直显示带有MotionBlur的效果，
    // 但实际是不需要带有MotionBlur,因此需要在LayerCache的staticTimeRanges过滤掉静态区间的第一帧。
    for (auto& timeRange : *transformCache->getStaticTimeRanges()) {
      SplitTimeRangesAt(&staticTimeRanges, timeRange.start + 1);
    }
  }
}

std::vector<TimeRange> LayerCache::getTrackMatteStaticTimeRanges() {
  auto trackMatteLayer = layer->trackMatteLayer;
  std::vector<TimeRange> timeRanges = {trackMatteLayer->visibleRange()};
  trackMatteLayer->excludeVaryingRanges(&timeRanges);
  SplitTimeRangesAt(&timeRanges, trackMatteLayer->startTime);
  SplitTimeRangesAt(&timeRanges, trackMatteLayer->startTime + trackMatteLayer->duration);
  auto parent = trackMatteLayer->parent;
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
  return OffsetTimeRanges(timeRanges, -layer->startTime);
}

std::vector<TimeRange> LayerCache::getFilterStaticTimeRanges() {
  std::vector<TimeRange> timeRanges = {layer->visibleRange()};
  for (auto& layerStyle : layer->layerStyles) {
    layerStyle->excludeVaryingRanges(&timeRanges);
  }
  for (auto& effect : layer->effects) {
    effect->excludeVaryingRanges(&timeRanges);
  }
  return OffsetTimeRanges(timeRanges, -layer->startTime);
}
}  // namespace pag
