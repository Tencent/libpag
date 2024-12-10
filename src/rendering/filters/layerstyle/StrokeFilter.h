/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "AlphaEdgeDetectFilter.h"
#include "SolidStrokeFilter.h"

namespace pag {
class StrokeFilter : public LayerStyleFilter {
 public:
  explicit StrokeFilter(StrokeStyle* layerStyle);

  StrokeFilter(const StrokeFilter&) = delete;

  StrokeFilter(StrokeFilter&&) = delete;

  bool draw(Frame layerFrame, std::shared_ptr<tgfx::Image> source, const tgfx::Point& filterScale,
            const tgfx::Matrix& matrix, tgfx::Canvas* target) override;

 private:
  StrokeStyle* layerStyle = nullptr;
};
}  // namespace pag
