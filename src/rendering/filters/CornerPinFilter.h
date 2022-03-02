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

#include "LayerFilter.h"

namespace pag {
class CornerPinFilter : public LayerFilter {
 public:
  explicit CornerPinFilter(Effect* effect);
  ~CornerPinFilter() override = default;

 protected:
  std::string onBuildVertexShader() override;

  std::string onBuildFragmentShader() override;

  std::vector<tgfx::Point> computeVertices(const tgfx::Rect& contentBounds,
                                           const tgfx::Rect& transformedBounds,
                                           const tgfx::Point& filterScale) override;

  void bindVertices(tgfx::Context* context, const FilterSource* source, const FilterTarget* target,
                    const std::vector<tgfx::Point>& points) override;

  bool needsMSAA() const override {
    return true;
  }

 private:
  void calculateVertexQs();

  Effect* effect = nullptr;
  float vertexQs[4] = {1.0f};
};
}  // namespace pag
