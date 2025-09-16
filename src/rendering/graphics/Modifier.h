/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#include "rendering/graphics/Canvas.h"
#include "tgfx/core/BlendMode.h"
#include "tgfx/core/Canvas.h"
#include "tgfx/core/Path.h"

namespace pag {
class Graphic;

class RenderCache;

class Modifier {
 public:
  static std::shared_ptr<Modifier> MakeBlend(float alpha, tgfx::BlendMode blendMode);
  static std::shared_ptr<Modifier> MakeClip(const tgfx::Path& clip);
  static std::shared_ptr<Modifier> MakeMask(std::shared_ptr<Graphic> graphic, bool inverted,
                                            bool useLuma);

  virtual ~Modifier() = default;

  /**
   * Returns true if the result of applying this modifier is empty.
   */
  virtual bool isEmpty() const = 0;

  /**
   * Returns the type ID of this modifier.
   */
  virtual uint32_t type() const = 0;

  /**
   * Return false if target content with modification applied does not overlap or intersect with
   * specified point.
   */
  virtual bool hitTest(RenderCache* cache, float x, float y) const = 0;

  /**
   * Prepares this modifier for next applyToGraphic() call. It collects all CPU tasks in this
   * modifier and run them in parallel immediately.
   */
  virtual void prepare(RenderCache* cache) const = 0;

  /**
   * Applies the modification to content bounds.
   */
  virtual void applyToBounds(tgfx::Rect* bounds) const = 0;

  /**
   * Returns false if this modifier can not process path.
   */
  virtual bool applyToPath(tgfx::Path* path) const = 0;

  /**
   * Draws the graphic to specified canvas with custom modification.
   */
  virtual void applyToGraphic(Canvas* canvas, std::shared_ptr<Graphic> graphic) const = 0;

  /**
   * Returns a new modifier which is the combination of this modifier and specified modifier if this
   * modifier can be merged with specified modifier.
   */
  virtual std::shared_ptr<Modifier> mergeWith(const Modifier* modifier) const = 0;
};
}  // namespace pag
