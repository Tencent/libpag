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

#include <unordered_map>
#include "pag/file.h"

namespace pag {
template <typename T>
class FrameCache : public Cache {
 public:
  explicit FrameCache(Frame startTime, Frame duration) : startTime(startTime), duration(duration) {
    if (duration <= 0) {
      this->duration = duration = 1;
    }

    TimeRange range = {0, duration - 1};
    staticTimeRanges.push_back(range);
  }

  ~FrameCache() override {
    for (auto& item : frames) {
      delete item.second;
    }
  }

  virtual T* getCache(Frame contentFrame) {
    contentFrame = ConvertFrameByStaticTimeRanges(staticTimeRanges, contentFrame);
    if (contentFrame >= duration) {
      contentFrame = duration - 1;
    }
    if (contentFrame < 0) {
      contentFrame = 0;
    }
    std::lock_guard<std::mutex> autoLock(locker);
    auto cache = frames[contentFrame];
    if (cache == nullptr) {
      cache = createCache(contentFrame + startTime);
      frames[contentFrame] = cache;
    }
    return cache;
  }

  const std::vector<TimeRange>* getStaticTimeRanges() const {
    return &staticTimeRanges;
  }

 protected:
  Frame startTime = 0;
  Frame duration = 1;
  std::vector<TimeRange> staticTimeRanges;

  virtual T* createCache(Frame layerFrame) = 0;

 private:
  std::mutex locker = {};
  std::unordered_map<Frame, T*> frames;
};
}  // namespace pag
