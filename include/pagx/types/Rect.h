/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 THL A29 Limited, a Tencent company. All rights reserved.
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

namespace pagx {

/**
 * A rectangle defined by position and size.
 */
struct Rect {
  /**
   * The x coordinate of the rectangle origin.
   */
  float x = 0;

  /**
   * The y coordinate of the rectangle origin.
   */
  float y = 0;

  /**
   * The width of the rectangle.
   */
  float width = 0;

  /**
   * The height of the rectangle.
   */
  float height = 0;

  /**
   * Returns a Rect from left, top, right, bottom coordinates.
   */
  static Rect MakeLTRB(float l, float t, float r, float b) {
    return {l, t, r - l, b - t};
  }

  /**
   * Returns a Rect from position and size.
   */
  static Rect MakeXYWH(float x, float y, float w, float h) {
    return {x, y, w, h};
  }

  bool isEmpty() const {
    return width <= 0 || height <= 0;
  }

  bool operator==(const Rect& other) const {
    return x == other.x && y == other.y && width == other.width && height == other.height;
  }

  bool operator!=(const Rect& other) const {
    return !(*this == other);
  }
};

}  // namespace pagx
