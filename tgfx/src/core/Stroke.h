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

#include "pag/file.h"

namespace pag {
/**
 * Stroke controls options applied when stroking geometries (paths, glyphs).
 */
class Stroke {
 public:
  Stroke() = default;

  /**
   * Creates a new Stroke width specified options.
   */
  explicit Stroke(float width, Enum cap = LineCap::Butt, Enum join = LineJoin::Miter,
                  float miterLimit = 4.0f)
      : width(width), cap(cap), join(join), miterLimit(miterLimit) {
  }

  /**
   * The thickness of the pen used to outline the paths or glyphs.
   */
  float width = 1.0;

  /**
   *  The geometry drawn at the beginning and end of strokes.
   */
  Enum cap = LineCap::Butt;

  /**
   * The geometry drawn at the corners of strokes.
   */
  Enum join = LineJoin::Miter;

  /**
   * The limit at which a sharp corner is drawn beveled.
   */
  float miterLimit = 4.0f;
};
}  // namespace pag
