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

namespace tgfx {
/**
 * These values match the orientation www.exif.org/Exif2-2.PDF.
 */
enum class Orientation {
  /**
   * Default
   */
  TopLeft = 1,
  /**
   * Reflected across y-axis
   */
  TopRight = 2,
  /**
   * Rotated 180
   */
  BottomRight = 3,
  /**
   * Reflected across x-axis
   */
  BottomLeft = 4,
  /**
   * Reflected across x-axis, Rotated 90 CCW
   */
  LeftTop = 5,
  /**
   * Rotated 90 CW
   */
  RightTop = 6,
  /**
   * Reflected across x-axis, Rotated 90 CW
   */
  RightBottom = 7,
  /**
   * Rotated 90 CCW
   */
  LeftBottom = 8
};

/**
 * Given an orientation and the width and height of the source data, returns a matrix that
 * transforms the source rectangle [0, 0, w, h] to a correctly oriented destination rectangle, with
 * the upper left corner still at [0, 0].
 */
Matrix OrientationToMatrix(Orientation orientation, int width, int height);

/**
 * Transforms the source size (width, height) to a correctly oriented destination size by the given
 * an orientation.
 */
void ApplyOrientation(Orientation orientation, int* width, int* height);

}  // namespace tgfx
