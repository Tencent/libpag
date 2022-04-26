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

#include "tgfx/core/Path.h"
#include "tgfx/core/Stroke.h"

namespace tgfx {
/**
 * PathEffect applies transformation to a Path.
 */
class PathEffect {
 public:
  /**
   * Creates a stroke path effect with the specified stroke options.
   */
  static std::unique_ptr<PathEffect> MakeStroke(const Stroke& stroke);

  /**
   * Creates a dash path effect.
   * @param intervals array containing an even number of entries (>=2), with the even indices
   * specifying the length of "on" intervals, and the odd indices specifying the length of "off"
   * intervals.
   * @param count number of elements in the intervals array
   * @param phase  offset into the intervals array (mod the sum of all of the intervals).
   */
  static std::unique_ptr<PathEffect> MakeDash(const float intervals[], int count, float phase);

  /**
   * Create a corner path effect.
   * @param radius  must be > 0 to have an effect. It specifies the distance from each corner that
   * should be "rounded".
   */
  static std::unique_ptr<PathEffect> MakeCorner(float radius);

  virtual ~PathEffect() = default;

  /**
   * Applies this effect to specified path. Returns false if this effect cannot be applied, and
   * leaves this path unchanged.
   */
  virtual bool applyTo(Path* path) const = 0;
};
}  // namespace tgfx