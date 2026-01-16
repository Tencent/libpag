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

#include "layerstyle/LayerStyleFilter.h"
#include "pag/pag.h"
#include "rendering/filters/FilterModifier.h"

namespace pag {
struct FilterList;
class RenderCache;

class LayerStylesFilter {
 public:
  static void TransformBounds(tgfx::Rect* bounds, const FilterList* filterList);

  static std::shared_ptr<LayerStylesFilter> Make(RenderCache* cache,
                                                 const std::vector<LayerStyle*>& layerStyles,
                                                 Frame layerFrame, float sourceScale,
                                                 const tgfx::Point& filterScale);

  void applyFilter(Canvas* canvas, std::shared_ptr<tgfx::Image> image);

 private:
  std::vector<std::unique_ptr<LayerStyleFilter>> layerStyleFilters = {};
  bool drawSource = false;
};
}  // namespace pag
