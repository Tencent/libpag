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

#include "base/utils/TGFXCast.h"
#include "rendering/caches/ContentCache.h"
#include "rendering/caches/MaskCache.h"
#include "rendering/caches/TransformCache.h"
#include "rendering/graphics/Modifier.h"

namespace pag {
class LayerCache : public Cache {
 public:
  static LayerCache* Get(Layer* layer);

  ~LayerCache() override;

  Transform* getTransform(Frame contentFrame);

  tgfx::Path* getMasks(Frame contentFrame);

  std::shared_ptr<Modifier> getFeatherMask(Frame contentFrame);

  Content* getContent(Frame contentFrame);

  Layer* getLayer() const;

  std::pair<tgfx::Point, tgfx::Point> getScaleFactor() const;

  bool checkFrameChanged(Frame contentFrame, Frame lastContentFrame);

  bool contentVisible(Frame contentFrame);

  bool contentStatic() const {
    return contentCache->contentStatic();
  }

  bool cacheEnabled() const {
    return contentCache->cacheEnabled();
  }

  bool hasFilters() const {
    return contentCache->hasFilters();
  }

  bool cacheFilters() const {
    return contentCache->cacheFilters();
  }

 private:
  Layer* layer = nullptr;
  TransformCache* transformCache = nullptr;
  MaskCache* maskCache = nullptr;
  FeatherMaskCache* featherMaskCache = nullptr;
  ContentCache* contentCache = nullptr;
  std::pair<tgfx::Point, tgfx::Point> scaleFactor = {};
  std::vector<TimeRange> staticTimeRanges;
  explicit LayerCache(Layer* layer);
  void updateStaticTimeRanges();
  std::vector<TimeRange> getTrackMatteStaticTimeRanges();
  std::vector<TimeRange> getFilterStaticTimeRanges();
};
}  // namespace pag
