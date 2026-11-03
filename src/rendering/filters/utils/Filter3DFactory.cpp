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

#include "Filter3DFactory.h"
#include "rendering/filters/Transform3DFilter.h"

namespace pag {

std::shared_ptr<tgfx::Image> Apply3DEffects(std::shared_ptr<tgfx::Image> input, RenderCache* cache,
                                            const FilterList* filterList,
                                            const tgfx::Rect& clipBounds,
                                            const tgfx::Point& sourceScale,
                                            tgfx::Rect* filterBounds, tgfx::Point* offset) {
  if (filterList->layer->transform3D) {
    auto oldBounds = *filterBounds;
    auto containingComposition = filterList->layer->containingComposition;
    *filterBounds = tgfx::Rect::MakeWH(containingComposition->width, containingComposition->height);
    if (!filterBounds->intersect(clipBounds)) {
      return nullptr;
    }
    return Transform3DFilter::Apply(input, cache, filterList->layer, filterList->layerFrame,
                                    oldBounds, sourceScale, offset);
  }
  offset->set(0, 0);
  return input;
}

bool Has3DSupport() {
  return true;
}

}  // namespace pag
