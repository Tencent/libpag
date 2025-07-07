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

#include "CompositionCache.h"
#include "ContentCache.h"

namespace pag {
class PreComposeContentCache : public ContentCache {
 public:
  explicit PreComposeContentCache(PreComposeLayer* layer);

 protected:
  void excludeVaryingRanges(std::vector<TimeRange>*) const override;
  GraphicContent* createContent(Frame layerFrame) const override;

 private:
  CompositionCache* compositionCache = nullptr;
};
}  // namespace pag
