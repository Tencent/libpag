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

#include "LayerStylesFilter.h"
#include "base/utils/TGFXCast.h"
#include "rendering/caches/RenderCache.h"
#include "rendering/renderers/FilterRenderer.h"
#include "tgfx/core/Surface.h"

namespace pag {
void LayerStylesFilter::TransformBounds(tgfx::Rect* bounds, const FilterList* filterList) {
  for (auto& layerStyle : filterList->layerStyles) {
    auto rect = *bounds;
    layerStyle->transformBounds(ToPAG(&rect), ToPAG(filterList->layerStyleScale),
                                filterList->layerFrame);
    rect.roundOut();
    bounds->join(rect);
  }
}

std::shared_ptr<LayerStylesFilter> LayerStylesFilter::Make(
    RenderCache* cache, const std::vector<LayerStyle*>& layerStyles, Frame layerFrame,
    float sourceScale, const tgfx::Point& filterScale) {
  std::vector<std::unique_ptr<LayerStyleFilter>> blowFilters;
  std::vector<std::unique_ptr<LayerStyleFilter>> aboveFilters;
  for (auto& layerStyle : layerStyles) {
    auto filter = LayerStyleFilter::Make(layerStyle, cache);
    if (!filter) {
      continue;
    }
    filter->update(layerFrame, filterScale, {sourceScale, sourceScale});
    if (layerStyle->drawPosition() == LayerStylePosition::Blow) {
      blowFilters.push_back(std::move(filter));
    } else {
      aboveFilters.push_back(std::move(filter));
    }
  }
  if (blowFilters.empty() && aboveFilters.empty()) {
    return nullptr;
  }
  auto filter = std::make_shared<LayerStylesFilter>();
  filter->layerStyleFilters = std::move(blowFilters);
  filter->layerStyleFilters.insert(filter->layerStyleFilters.end(),
                                   std::make_move_iterator(aboveFilters.begin()),
                                   std::make_move_iterator(aboveFilters.end()));
  // Above filters will draw the source image.
  filter->drawSource = aboveFilters.empty();
  return filter;
}

void LayerStylesFilter::applyFilter(Canvas* canvas, std::shared_ptr<tgfx::Image> image) {
  if (image == nullptr) {
    return;
  }
  for (const auto& layerStyleFilter : layerStyleFilters) {
    layerStyleFilter->draw(canvas, image);
  }
  if (drawSource) {
    canvas->drawImage(std::move(image));
  }
}

}  // namespace pag
