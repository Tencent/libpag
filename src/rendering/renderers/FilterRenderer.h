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

#pragma once

#include "pag/pag.h"
#include "rendering/filters/FilterModifier.h"
#include "rendering/filters/LayerStylesFilter.h"
#include "rendering/graphics/Recorder.h"

namespace pag {
struct FilterNode {
  FilterNode(Filter* filter, const tgfx::Rect& bounds) : filter(filter), bounds(bounds) {
  }

  Filter* filter;
  tgfx::Rect bounds;
};

struct FilterList {
  Layer* layer = nullptr;
  Frame layerFrame = 0;
  tgfx::Matrix layerMatrix = tgfx::Matrix::I();
  float scaleFactorLimit = FLT_MAX;
  bool processVisibleAreaOnly = true;
  // 是否使用父级Composition容器的尺寸作为滤镜输入源。
  bool useParentSizeInput = false;
  tgfx::Point effectScale = {1.0f, 1.0f};
  tgfx::Point layerStyleScale = {1.0f, 1.0f};
  std::vector<Effect*> effects = {};
  std::vector<LayerStyle*> layerStyles = {};
};

class FilterRenderer {
 public:
  static void MeasureFilterBounds(tgfx::Rect* bounds, const FilterModifier* modifier);

  static void DrawWithFilter(tgfx::Canvas* parentCanvas, RenderCache* cache,
                             const FilterModifier* modifier, std::shared_ptr<Graphic> content);

 private:
  static std::unique_ptr<FilterList> MakeFilterList(const FilterModifier* modifier);

  static tgfx::Rect GetParentBounds(const FilterList* filterList);

  static tgfx::Rect GetContentBounds(const FilterList* filterList,
                                     std::shared_ptr<Graphic> content);

  static bool MakeEffectNode(std::vector<FilterNode>& filterNodes, tgfx::Rect& clipBounds,
                             const FilterList* filterList, RenderCache* renderCache,
                             tgfx::Rect& filterBounds, tgfx::Point& effectScale, int clipIndex);

  static std::vector<FilterNode> MakeFilterNodes(const FilterList* filterList,
                                                 RenderCache* renderCache,
                                                 tgfx::Rect* contentBounds,
                                                 const tgfx::Rect& clipRect);
};
}  // namespace pag
