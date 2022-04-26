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

#include "tgfx/core/Matrix.h"
#include "tgfx/core/Rect.h"

namespace tgfx {
class Quad {
 public:
  static Quad MakeFromRect(const Rect& rect, const Matrix& matrix);

  const Point& point(size_t i) const {
    return points[i];
  }

  Rect bounds() const;

 private:
  explicit Quad(std::vector<Point> points) : points(std::move(points)) {
  }

  std::vector<Point> points;
};
}  // namespace tgfx
