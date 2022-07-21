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

namespace tgfx {
/**
 * PathMeasure calculates the length of a Path and cuts child segments from it.
 */
class PathMeasure {
 public:
  /**
   * Initialize the PathMeasure with the specified path.
   */
  static std::unique_ptr<PathMeasure> MakeFrom(const Path& path);

  virtual ~PathMeasure() = default;

  /**
   * Return the total length of the the path.
   */
  virtual float getLength() = 0;

  /**
   * Given a start and stop distance, return in result the intervening segment(s). If the segment is
   * zero-length, return false, else return true. startD and stopD are pinned to legal values
   * (0..getLength()). If startD > stopD then return false (and leave dst untouched).
   */
  virtual bool getSegment(float startD, float stopD, Path* result) = 0;

  /**
   * Pins distance to 0 <= distance <= getLength(), and then computes
   * the corresponding position and tangent.
   * Returns false if there is no path, or a zero-length path was specified, in which case
   * position and tangent are unchanged.
   */
  virtual bool getPosTan(float distance, Point* position, Point* tangent) = 0;

  /**
   * Returns true if the current contour is closed().
   */
  virtual bool isClosed() = 0;
};
}  // namespace tgfx