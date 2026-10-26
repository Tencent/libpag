/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions and
//  limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "pagx/nodes/Node.h"
#include "pagx/types/Point.h"

namespace pagx {

class PathData;

/**
 * The combination mode of a mask path (spec 7.6), matching the legacy MaskMode.
 */
enum class MaskMode : uint8_t {
  None = 0,
  Add = 1,
  Subtract = 2,
  Intersect = 3,
  Lighten = 4,
  Darken = 5,
  Difference = 6,
  Accum = 7,
};

/**
 * A mask path entry in a layer's mask list (spec 7.6): a path shape combined into the layer's
 * mask with a mode, optional inversion, feathering, opacity and expansion, migrated from the
 * legacy MaskData. The path itself is a PathData node referenced by the entry, and the path
 * channel (Path value type, spec 6.1) animates it.
 */
class MaskPath : public Node {
 public:
  /**
   * The mask shape. Null disables the entry.
   */
  PathData* path = nullptr;

  /**
   * How this shape combines with the mask built so far. The default value is Add.
   */
  MaskMode maskMode = MaskMode::Add;

  /**
   * Whether the shape contribution is inverted. The default value is false.
   */
  bool inverted = false;

  /**
   * The horizontal and vertical feather radius in pixels. The default value is (0, 0).
   */
  Point feather = {};

  /**
   * The mask opacity in 0-1 space. The default value is 1.
   */
  float opacity = 1.0f;

  /**
   * The shape expansion in pixels; positive grows, negative shrinks. The default value is 0.
   */
  float expansion = 0.0f;

  NodeType nodeType() const override {
    return NodeType::MaskPath;
  }

 private:
  MaskPath() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
