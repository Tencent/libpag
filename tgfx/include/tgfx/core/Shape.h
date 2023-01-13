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

#include "tgfx/core/Cacheable.h"
#include "tgfx/core/Color.h"
#include "tgfx/core/Path.h"
#include "tgfx/core/Stroke.h"

namespace tgfx {
class DrawOp;

class GpuPaint;

/**
 * Represents a geometric shape that can be used as a drawing mask. Shape is usually used to cache a
 * complex Path for frequent drawing. Unlike Path, Shape's resolution is fixed after it is created
 * unless it represents a simple rect or rrect. Therefore, drawing a Shape with scale factors great
 * than 1.0 may result in blurred output. Shape is thread safe.
 */
class Shape : public Cacheable {
 public:
  /**
   * Creates a Shape from the fills of the given path after being scaled by the resolutionScale.
   * Returns nullptr if path is empty or resolutionScale is less than 0.
   */
  static std::shared_ptr<Shape> MakeFromFill(const Path& path, float resolutionScale = 1.0f);

  /**
   * Creates a Shape from the strokes of the given path after being scaled by the resolutionScale.
   * Returns nullptr if path is empty or resolutionScale is less than 0.
   */
  static std::shared_ptr<Shape> MakeFromStroke(const Path& path, const Stroke& stroke,
                                               float resolutionScale = 1.0f);

  /**
   * Returns the bounds of the Shape.
   */
  virtual Rect getBounds() const = 0;

 private:
  virtual std::unique_ptr<DrawOp> makeOp(GpuPaint* paint, const Matrix& viewMatrix) const = 0;

  friend class Canvas;
};
}  // namespace tgfx
