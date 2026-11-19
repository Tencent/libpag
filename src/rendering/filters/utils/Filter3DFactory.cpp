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

#include "Filter3DFactory.h"
#include "rendering/filters/Transform3DFilter.h"

namespace pag {

bool Make3DLayerNode(std::vector<FilterNode>& filterNodes, tgfx::Rect& clipBounds,
                     const FilterList* filterList, RenderCache* renderCache,
                     tgfx::Rect& filterBounds, tgfx::Point& effectScale, bool* skipLayer) {
  if (filterList->layer->transform3D == nullptr) {
    return true;
  }
  auto filter = static_cast<Transform3DFilter*>(renderCache->getTransform3DFilter());
  if (filter == nullptr ||
      !filter->updateLayer(filterList->layer, filterList->layerFrame, renderCache)) {
    // A 3D layer cannot fall back to its untransformed content, so it is skipped instead.
    *skipLayer = true;
    return false;
  }
  auto oldBounds = filterBounds;
  auto containingComposition = filterList->layer->containingComposition;
  filterBounds = tgfx::Rect::MakeWH(containingComposition->width, containingComposition->height);
  filter->update(filterList->layerFrame, oldBounds, filterBounds, effectScale);
  if (!filterBounds.intersect(clipBounds)) {
    *skipLayer = true;
    return false;
  }
  filterNodes.emplace_back(filter, filterBounds);
  return true;
}

Filter* Make3DFilter(tgfx::Context* context) {
  auto filter = new Transform3DFilter();
  if (!filter->initialize(context)) {
    delete filter;
    filter = nullptr;
  }
  return filter;
}

bool Has3DSupport() {
  return true;
}

}  // namespace pag
