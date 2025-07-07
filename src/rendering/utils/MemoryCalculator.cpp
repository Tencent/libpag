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

#include "MemoryCalculator.h"
#include "base/utils/Log.h"
#include "rendering/caches/LayerCache.h"

namespace pag {
static void* GetCacheResourcesForLayer(Layer* layer) {
  switch (layer->type()) {
    case LayerType::PreCompose:
      return static_cast<PreComposeLayer*>(layer)->composition;
    case LayerType::Image:
      return static_cast<ImageLayer*>(layer)->imageBytes;
    default:
      return layer;
  }
}

void MemoryCalculator::FillLayerGraphicsMemoriesPreFrame(
    Layer* layer, std::unordered_map<void*, tgfx::Point>& resourcesScaleMap,
    std::unordered_map<void*, std::vector<TimeRange>*>& resourcesTimeRangesMap,
    std::vector<int64_t>& memoriesPreFrame, std::unordered_set<void*>& cachedResources) {
  auto resources = GetCacheResourcesForLayer(layer);
  if (cachedResources.find(resources) != cachedResources.end()) {
    return;
  }
  if (!layer->isActive) {
    return;
  }
  cachedResources.insert(resources);

  auto layerType = layer->type();
  switch (layerType) {
    case LayerType::PreCompose:
      FillCompositionGraphicsMemories(static_cast<PreComposeLayer*>(layer)->composition,
                                      resourcesScaleMap, resourcesTimeRangesMap, memoriesPreFrame,
                                      cachedResources);
      break;
    default: {
      auto layerCache = LayerCache::Get(layer);
      if (!layerCache->cacheEnabled()) {
        break;
      }
      auto scaleIter = resourcesScaleMap.find(resources);
      if (scaleIter == resourcesScaleMap.end()) {
        LOGE("layer's scale has not calculated");
        break;
      }
      auto layerContent = layerCache->getContent(0);
      tgfx::Rect bounds = {};
      layerContent->measureBounds(&bounds);
      auto scale = std::max(scaleIter->second.x, scaleIter->second.y);
      if (layerType == LayerType::Image) {
        scale = std::min(scale, 1.0f);
      }
      bounds.setWH(ceil(bounds.width() * scale), ceil(bounds.height() * scale));
      int64_t graphicsMemory = ceil(bounds.width()) * ceil(bounds.height()) * 4;  // w*h*scale*rgba
      auto timeRanges = resourcesTimeRangesMap.find(resources)->second;
      for (auto& timeRange : *timeRanges) {
        for (Frame frame = timeRange.start; frame <= timeRange.end; frame++) {
          if (static_cast<size_t>(frame) >= memoriesPreFrame.size()) {
            break;
          }
          memoriesPreFrame[frame] += graphicsMemory;
        }
      }
    } break;
  }
}

void FillGraphicsMemories(
    Composition* composition,
    std::unordered_map<void*, std::vector<TimeRange>*>& resourcesTimeRangesMap,
    std::vector<int64_t>& memoriesPreFrame, int64_t& graphicsMemory) {
  // Just use the last one for best rendering quality, ignore all others.
  auto timeRanges = resourcesTimeRangesMap.find(composition)->second;
  for (auto timeRangeIter = timeRanges->begin(); timeRangeIter != timeRanges->end();
       timeRangeIter++) {
    auto timeRange = *timeRangeIter;
    for (auto frame = timeRange.start; frame <= timeRange.end; frame++) {
      if (static_cast<size_t>(frame) < memoriesPreFrame.size()) {
        std::vector<int64_t>::size_type currentFrame = (std::vector<int64_t>::size_type)frame;
        memoriesPreFrame[currentFrame] += graphicsMemory;
      } else {
        break;
      }
    }
  }
}

void MemoryCalculator::FillBitmapGraphicsMemories(
    Composition* composition, std::unordered_map<void*, tgfx::Point>&,
    std::unordered_map<void*, std::vector<TimeRange>*>& resourcesTimeRangesMap,
    std::vector<int64_t>& memoriesPreFrame, int64_t& graphicsMemory) {
  // Just use the last one for best rendering quality, ignore all others.
  auto sequence = static_cast<BitmapComposition*>(composition)->sequences.back();
  graphicsMemory += sequence->width * sequence->height * 4;
  FillGraphicsMemories(composition, resourcesTimeRangesMap, memoriesPreFrame, graphicsMemory);
}

void MemoryCalculator::FillVideoGraphicsMemories(
    Composition* composition,
    std::unordered_map<void*, std::vector<TimeRange>*>& resourcesTimeRangesMap,
    std::vector<int64_t>& memoriesPreFrame, int64_t& graphicsMemory) {
  // Just use the last one for best rendering quality, ignore all others.
  auto sequence = static_cast<VideoComposition*>(composition)->sequences.back();
  auto factor = sequence->alphaStartX > 0 || sequence->alphaStartY > 0 ? 3 : 2;
  graphicsMemory += sequence->width * sequence->height * 4 * factor;
  FillGraphicsMemories(composition, resourcesTimeRangesMap, memoriesPreFrame, graphicsMemory);
}

void MemoryCalculator::FillVectorGraphicsMemories(
    Composition* composition, std::unordered_map<void*, tgfx::Point>& resourcesScaleMap,
    std::unordered_map<void*, std::vector<TimeRange>*>& resourcesTimeRangesMap,
    std::vector<int64_t>& memoriesPreFrame, std::unordered_set<void*>& cachedResources) {
  VectorComposition* vectorComposition = static_cast<VectorComposition*>(composition);
  for (auto iter = vectorComposition->layers.begin(); iter != vectorComposition->layers.end();
       iter++) {
    auto layer = *iter;
    FillLayerGraphicsMemoriesPreFrame(layer, resourcesScaleMap, resourcesTimeRangesMap,
                                      memoriesPreFrame, cachedResources);
  }
}

int64_t MemoryCalculator::FillCompositionGraphicsMemories(
    Composition* composition, std::unordered_map<void*, tgfx::Point>& resourcesScaleMap,
    std::unordered_map<void*, std::vector<TimeRange>*>& resourcesTimeRangesMap,
    std::vector<int64_t>& memoriesPreFrame, std::unordered_set<void*>& cachedResources) {
  int64_t graphicsMemory = 0;
  switch (composition->type()) {
    case CompositionType::Vector: {
      FillVectorGraphicsMemories(composition, resourcesScaleMap, resourcesTimeRangesMap,
                                 memoriesPreFrame, cachedResources);
      break;
    }
    case CompositionType::Bitmap: {
      FillBitmapGraphicsMemories(composition, resourcesScaleMap, resourcesTimeRangesMap,
                                 memoriesPreFrame, graphicsMemory);
      break;
    }
    case CompositionType::Video: {
      FillVideoGraphicsMemories(composition, resourcesTimeRangesMap, memoriesPreFrame,
                                graphicsMemory);
      break;
    }
    default:
      break;
  }
  return graphicsMemory;
}

std::vector<int64_t> MemoryCalculator::GetRootLayerGraphicsMemoriesPreFrame(
    PreComposeLayer* rootLayer, std::unordered_map<void*, tgfx::Point>& resourcesScaleMap,
    std::unordered_map<void*, std::vector<TimeRange>*>& resourcesTimeRangesMap) {
  std::unordered_set<void*> cachedResources;
  if (static_cast<size_t>(rootLayer->composition->duration) > std::vector<int64_t>().max_size()) {
    return std::vector<int64_t>();
  }
  auto duration = (std::vector<int64_t>::size_type)rootLayer->composition->duration;
  std::vector<int64_t> memoriesPreFrame(duration, 0);
  FillLayerGraphicsMemoriesPreFrame(rootLayer, resourcesScaleMap, resourcesTimeRangesMap,
                                    memoriesPreFrame, cachedResources);
  return memoriesPreFrame;
}

bool MemoryCalculator::UpdateMaxScaleMapIfNeed(
    void* resource, tgfx::Point currentScale,
    std::unordered_map<void*, tgfx::Point>& resourcesMaxScaleMap) {
  auto maxScale = currentScale;
  auto iter = resourcesMaxScaleMap.find(resource);
  bool hasChange = true;
  if (iter != resourcesMaxScaleMap.end()) {
    auto scale = iter->second;
    hasChange = maxScale.x > scale.x || maxScale.y > scale.y;
    maxScale.x = maxScale.x > scale.x ? maxScale.x : scale.x;
    maxScale.y = maxScale.y > scale.y ? maxScale.y : scale.y;
    if (hasChange) {
      resourcesMaxScaleMap.erase(iter);
    }
  }
  if (hasChange) {
    resourcesMaxScaleMap.insert(std::make_pair(resource, maxScale));
  }
  return hasChange;
}

static bool TimeRangesHaveIntersection(const TimeRange& range, const TimeRange& otherRange) {
  return (range.start >= otherRange.start && range.start <= otherRange.end) ||
         (otherRange.start >= range.start && otherRange.start <= range.end);
}

// Method will make mistake when ranges don't have intersection
static TimeRange UnionTimeRanges(const TimeRange& range, const TimeRange& otherRange) {
  TimeRange unionRange;
  unionRange.start = range.start < otherRange.start ? range.start : otherRange.start;
  unionRange.end = range.end > otherRange.end ? range.end : otherRange.end;
  return unionRange;
}

void MemoryCalculator::UpdateTimeRangesMapIfNeed(
    void* resource, TimeRange timeRange,
    std::unordered_map<void*, std::vector<TimeRange>*>& resourcesTimeRangesMap, bool needUnion) {
  auto iter = resourcesTimeRangesMap.find(resource);
  std::vector<TimeRange>* resourceTimeRanges;
  if (iter != resourcesTimeRangesMap.end()) {
    resourceTimeRanges = iter->second;
  } else {
    resourceTimeRanges = new std::vector<TimeRange>();
    resourcesTimeRangesMap.insert(std::make_pair(resource, resourceTimeRanges));
  }
  auto timeRangeNeedAdd = timeRange;
  if (needUnion) {
    for (auto timeRangeIter = resourceTimeRanges->end() - 1;
         timeRangeIter != resourceTimeRanges->begin() - 1; timeRangeIter--) {
      if (TimeRangesHaveIntersection(*timeRangeIter, timeRangeNeedAdd)) {
        timeRangeNeedAdd = UnionTimeRanges(*timeRangeIter, timeRangeNeedAdd);
        resourceTimeRanges->erase(timeRangeIter);
      }
    }
  }
  resourceTimeRanges->push_back(timeRangeNeedAdd);
}

void MemoryCalculator::UpdateTimeRange(
    Layer* layer, Frame referenceStartTime,
    std::unordered_map<void*, std::vector<TimeRange>*>& resourcesTimeRangesMap) {
  if (!layer->isActive) {
    return;
  }
  TimeRange timeRange;
  timeRange.start = referenceStartTime + layer->startTime;
  timeRange.end = timeRange.start + layer->duration - 1;
  bool needUnionTimeRange = true;
  if (layer->type() == LayerType::PreCompose) {
    auto composition = static_cast<PreComposeLayer*>(layer)->composition;
    if (composition->type() == CompositionType::Vector) {
      auto sublayers = static_cast<VectorComposition*>(composition)->layers;
      for (auto iter = sublayers.begin(); iter != sublayers.end(); iter++) {
        UpdateTimeRange(*iter, timeRange.start, resourcesTimeRangesMap);
      }
    }
  }
  UpdateTimeRangesMapIfNeed(GetCacheResourcesForLayer(layer), timeRange, resourcesTimeRangesMap,
                            needUnionTimeRange);
}

void MemoryCalculator::UpdateMaxScaleAndTimeRange(
    Layer* layer, tgfx::Point referenceScale, Frame referenceStartTime,
    std::unordered_map<void*, tgfx::Point>& resourcesMaxScaleMap,
    std::unordered_map<void*, std::vector<TimeRange>*>& resourcesTimeRangesMap) {
  if (!layer->isActive) {
    return;
  }
  TimeRange timeRange;
  timeRange.start = referenceStartTime + layer->startTime;
  timeRange.end = timeRange.start + layer->duration - 1;
  bool needUnionTimeRange = true;
  if (layer->type() == LayerType::PreCompose) {
    auto composition = static_cast<PreComposeLayer*>(layer)->composition;
    auto layerScale = layer->getMaxScaleFactor();
    bool maxScaleHasChage = UpdateMaxScaleMapIfNeed(
        composition, {referenceScale.x * layerScale.x, referenceScale.y * layerScale.y},
        resourcesMaxScaleMap);
    if (composition->type() == CompositionType::Vector) {
      if (maxScaleHasChage) {
        auto maxScale = resourcesMaxScaleMap.find(composition)->second;
        auto sublayers = static_cast<VectorComposition*>(composition)->layers;
        for (auto iter = sublayers.begin(); iter != sublayers.end(); iter++) {
          UpdateMaxScaleAndTimeRange(*iter, maxScale, timeRange.start, resourcesMaxScaleMap,
                                     resourcesTimeRangesMap);
        }
      } else {
        auto sublayers = static_cast<VectorComposition*>(composition)->layers;
        for (auto iter = sublayers.begin(); iter != sublayers.end(); iter++) {
          UpdateTimeRange(*iter, timeRange.start, resourcesTimeRangesMap);
        }
      }
    }
  } else {
    auto cacheResources = GetCacheResourcesForLayer(layer);
    auto layerScale = layer->getMaxScaleFactor();
    UpdateMaxScaleMapIfNeed(cacheResources,
                            {referenceScale.x * layerScale.x, referenceScale.y * layerScale.y},
                            resourcesMaxScaleMap);
  }
  UpdateTimeRangesMapIfNeed(GetCacheResourcesForLayer(layer), timeRange, resourcesTimeRangesMap,
                            needUnionTimeRange);
}

void MemoryCalculator::CaculateResourcesMaxScaleAndTimeRanges(
    Layer* rootLayer, std::unordered_map<void*, tgfx::Point>& resourcesMaxScaleMap,
    std::unordered_map<void*, std::vector<TimeRange>*>& resourcesTimeRangesMap) {
  UpdateMaxScaleAndTimeRange(rootLayer, {1, 1}, rootLayer->startTime, resourcesMaxScaleMap,
                             resourcesTimeRangesMap);
}

int64_t CalculateGraphicsMemory(std::shared_ptr<File> file) {
  if (file == nullptr) {
    return 0;
  }
  auto rootLayer = file->getRootLayer();
  std::unordered_map<void*, tgfx::Point> resourcesMaxScaleMap;
  std::unordered_map<void*, std::vector<TimeRange>*> resourcesTimeRangesMap;
  MemoryCalculator::CaculateResourcesMaxScaleAndTimeRanges(rootLayer, resourcesMaxScaleMap,
                                                           resourcesTimeRangesMap);
  std::vector<int64_t> memoriesPreFrame = MemoryCalculator::GetRootLayerGraphicsMemoriesPreFrame(
      rootLayer, resourcesMaxScaleMap, resourcesTimeRangesMap);
  int64_t maxGraphicsMemory = 0;
  for (std::vector<int64_t>::size_type i = 0; i < memoriesPreFrame.size(); i++) {
    maxGraphicsMemory =
        maxGraphicsMemory > memoriesPreFrame[i] ? maxGraphicsMemory : memoriesPreFrame[i];
  }
  for (auto it = resourcesTimeRangesMap.begin(); it != resourcesTimeRangesMap.end(); it++) {
    delete it->second;
  }
  return maxGraphicsMemory;
}
}  // namespace pag
