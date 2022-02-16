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
#include "rendering/filters/LayerFilter.h"

namespace pag {
class RenderCache;

struct FilterList;

class LayerStylesFilter : public Filter {
 public:
  static void TransformBounds(tgfx::Rect* bounds, const FilterList* filterList);

  explicit LayerStylesFilter(RenderCache* renderCache);

  ~LayerStylesFilter() override;

  bool initialize(tgfx::Context* context) override;

  void update(const FilterList* filterList, const tgfx::Rect& contentBounds,
              const tgfx::Rect& transformedBounds, const tgfx::Point& filterScale);

  void draw(tgfx::Context* context, const FilterSource* source,
            const FilterTarget* target) override;

 private:
  const FilterList* filterList = nullptr;
  RenderCache* renderCache = nullptr;
  LayerFilter* drawFilter = nullptr;
  tgfx::Rect contentBounds = {};
  tgfx::Rect transformedBounds = {};
  tgfx::Point filterScale = {};
};
}  // namespace pag
