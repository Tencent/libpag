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

#include "FrameCache.h"
#include "GraphicContent.h"
#include "rendering/filters/FilterModifier.h"

namespace pag {
class ContentCache : public FrameCache<Content> {
 public:
  explicit ContentCache(Layer* layer);

  bool cacheEnabled() const {
    return _cacheEnabled;
  }

  bool hasFilters() const {
    return _hasFilters;
  }

  bool cacheFilters() const {
    return _cacheFilters;
  }

  bool contentStatic() const {
    return _contentStatic;
  }

  void update();

 protected:
  Layer* layer = nullptr;
  bool _cacheEnabled = false;
  bool _hasFilters = false;
  bool _cacheFilters = false;
  bool _contentStatic = false;

  Content* createCache(Frame layerFrame) override;

  virtual ID getCacheID() const {
    return layer->uniqueID;
  }

  virtual void excludeVaryingRanges(std::vector<TimeRange>*) const {
  }
  virtual GraphicContent* createContent(Frame layerFrame) const = 0;

 private:
  bool checkCacheFilters();
  bool checkCacheEnabled();
};

class EmptyContentCache : public ContentCache {
 public:
  explicit EmptyContentCache(Layer* layer) : ContentCache(layer) {
  }

 protected:
  GraphicContent* createContent(Frame) const override {
    return new GraphicContent(nullptr);
  }
};

}  // namespace pag
