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

#pragma once

#include <unordered_set>
#include "pag/file.h"
#include "pag/pag.h"
#include "tgfx/core/Point.h"

namespace pag {
class MemoryCalculator {
 public:
  static void CaculateResourcesMaxScaleAndTimeRanges(
      Layer* rootLayer, std::unordered_map<void*, tgfx::Point>& resourcesMaxScaleMap,
      std::unordered_map<void*, std::vector<TimeRange>*>& resourcesTimeRangesMap);

  static std::vector<int64_t> GetRootLayerGraphicsMemoriesPreFrame(
      PreComposeLayer* rootLayer, std::unordered_map<void*, tgfx::Point>& resourcesScaleMap,
      std::unordered_map<void*, std::vector<TimeRange>*>& resourcesTimeRangesMap);

 private:
  static void FillBitmapGraphicsMemories(
      Composition* composition, std::unordered_map<void*, tgfx::Point>& resourcesScaleMap,
      std::unordered_map<void*, std::vector<TimeRange>*>& resourcesTimeRangesMap,
      std::vector<int64_t>& memoriesPreFrame, int64_t& graphicsMemory);

  static void FillVideoGraphicsMemories(
      Composition* composition,
      std::unordered_map<void*, std::vector<TimeRange>*>& resourcesTimeRangesMap,
      std::vector<int64_t>& memoriesPreFrame, int64_t& graphicsMemory);

  static void FillVectorGraphicsMemories(
      Composition* composition, std::unordered_map<void*, tgfx::Point>& resourcesScaleMap,
      std::unordered_map<void*, std::vector<TimeRange>*>& resourcesTimeRangesMap,
      std::vector<int64_t>& memoriesPreFrame, std::unordered_set<void*>& cachedResources);

  static int64_t FillCompositionGraphicsMemories(
      Composition* composition, std::unordered_map<void*, tgfx::Point>& resourcesScaleMap,
      std::unordered_map<void*, std::vector<TimeRange>*>& resourcesTimeRangesMap,
      std::vector<int64_t>& memoriesPreFrame, std::unordered_set<void*>& cachedResources);

  static void FillLayerGraphicsMemoriesPreFrame(
      Layer* layer, std::unordered_map<void*, tgfx::Point>& resourcesScaleMap,
      std::unordered_map<void*, std::vector<TimeRange>*>& resourcesTimeRangesMap,
      std::vector<int64_t>& memoriesPreFrame, std::unordered_set<void*>& cachedResources);

  static bool UpdateMaxScaleMapIfNeed(void* resource, tgfx::Point currentScale,
                                      std::unordered_map<void*, tgfx::Point>& resourcesMaxScaleMap);
  static void UpdateTimeRangesMapIfNeed(
      void* resource, TimeRange timeRange,
      std::unordered_map<void*, std::vector<TimeRange>*>& resourcesTimeRangesMap,
      bool needUnion = true);

  static void UpdateTimeRange(
      Layer* layer, Frame referenceStartTime,
      std::unordered_map<void*, std::vector<TimeRange>*>& resourcesTimeRangesMap);

  static void UpdateMaxScaleAndTimeRange(
      Layer* layer, tgfx::Point referenceScale, Frame referenceStartTime,
      std::unordered_map<void*, tgfx::Point>& resourcesMaxScaleMap,
      std::unordered_map<void*, std::vector<TimeRange>*>& resourcesTimeRangesMap);
};
}  // namespace pag
