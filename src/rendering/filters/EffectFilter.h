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

#include "RuntimeFilter.h"
#include "pag/file.h"
#include "rendering/filters/utils/FilterHelper.h"

namespace pag {
std::vector<tgfx::Point> ComputeVerticesForMotionBlurAndBulge(const tgfx::Rect& inputBounds,
                                                              const tgfx::Rect& outputBounds);

class Filter {
 public:
  virtual ~Filter() = default;

  void setFilterScale(const tgfx::Point& filterScale) {
    _filterScale = filterScale;
  }

  virtual void update(Frame layerFrame, const tgfx::Point& sourceScale) = 0;

  virtual bool shouldSkipFilter() {
    return false;
  }

  virtual void applyFilter(tgfx::Canvas* canvas, std::shared_ptr<tgfx::Image> source) = 0;

 protected:
  tgfx::Point _filterScale = tgfx::Point::Make(1, 1);
};

class EffectFilter : public Filter {
 public:
  static std::shared_ptr<EffectFilter> Make(Effect* effect);

  void setContentBounds(const tgfx::Rect& contentBounds) {
    _contentBounds = contentBounds;
  }

  void applyFilter(tgfx::Canvas* canvas, std::shared_ptr<tgfx::Image> source) override;

 protected:
  virtual std::shared_ptr<tgfx::RuntimeEffect> createRuntimeEffect();
  tgfx::Rect _contentBounds = tgfx::Rect::MakeEmpty();
};
}  // namespace pag
