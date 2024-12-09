/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2024 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "LayerStyleFilter.h"

namespace pag {

class CanvasFilter : public LayerStyleFilter {
 public:
  bool draw(Frame layerFrame, std::shared_ptr<tgfx::Image> source, const tgfx::Point& filterScale,
            const tgfx::Matrix& matrix, tgfx::Canvas* target) override;

  std::shared_ptr<tgfx::Image> applyFilterEffect(Frame layerFrame,
                                                 std::shared_ptr<tgfx::Image> source,
                                                 const tgfx::Point& filterScale,
                                                 tgfx::Point* offset) override;

 protected:
  virtual bool onDraw(Frame layerFrame, std::shared_ptr<tgfx::Image> source,
                      const tgfx::Point& filterScale, const tgfx::Matrix& matrix,
                      tgfx::Canvas* target) = 0;
};

}  // namespace pag