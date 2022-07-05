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

#include "Glyph.h"
#include "Modifier.h"
#include "TextureProxy.h"
#include "tgfx/core/Bitmap.h"
#include "tgfx/gpu/Paint.h"

namespace pag {
enum class GraphicType {
  Unknown,
  Picture,
  Shape,
  Text,
  Compose,
};

class Modifier;

/**
 * Graphic combines multiple drawings (shapes, texts, images) into an immutable container.
 * Graphic is thread safe.
 */
class Graphic {
 public:
  /**
   * Creates a compose Graphic with specified graphic and matrix. Returns nullptr if graphic is
   * nullptr or matrix is invisible.
   */
  static std::shared_ptr<Graphic> MakeCompose(std::shared_ptr<Graphic> graphic,
                                              const tgfx::Matrix& matrix);

  /**
   * Creates a compose Graphic with graphic contents. Returns nullptr if contents are emtpy.
   */
  static std::shared_ptr<Graphic> MakeCompose(std::vector<std::shared_ptr<Graphic>> contents);

  /**
   * Creates a compose Graphic with specified graphic and modifier. Returns nullptr if graphic is
   * nullptr.
   */
  static std::shared_ptr<Graphic> MakeCompose(std::shared_ptr<Graphic> graphic,
                                              std::shared_ptr<Modifier> modifier);

  virtual ~Graphic() = default;

  /**
   * Measures the bounds of this Graphic.
   */
  virtual void measureBounds(tgfx::Rect* bounds) const = 0;

  /**
   * Evaluates the Graphic to see if it overlaps or intersects with the specified point. The point
   * is in the coordinate space of the Graphic. This method always checks against the actual pixels
   * of the Graphic.
   */
  virtual bool hitTest(RenderCache* cache, float x, float y) = 0;

  /**
   * Returns the type of this graphic.
   */
  virtual GraphicType type() const = 0;

  /**
   * Gets a Path which is the filled equivalent of the Graphic contents. Returns false and
   * leaves the path unchanged if the Graphic contents are not opaque or can not be converted to
   * a path.
   */
  virtual bool getPath(tgfx::Path* path) const = 0;

  /**
   * Prepares this graphic for next draw() call. It collects all CPU tasks in this Graphic and run
   * them in parallel immediately.
   */
  virtual void prepare(RenderCache* cache) const = 0;

  /**
   * Draw this Graphic into specified Canvas.
   */
  virtual void draw(tgfx::Canvas* canvas, RenderCache* cache) const = 0;
};
}  // namespace pag
