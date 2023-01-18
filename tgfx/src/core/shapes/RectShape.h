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

#include "tgfx/core/Shape.h"

namespace tgfx {
class RectShape : public Shape {
 public:
  explicit RectShape(const Rect& rect, float resolutionScale = 1.0f);

  Rect getBounds() const override {
    return rect;
  }

 private:
  Rect rect = {};

  std::unique_ptr<DrawOp> makeOp(GpuPaint* paint, const Matrix& viewMatrix) const override;
};
}  // namespace tgfx
