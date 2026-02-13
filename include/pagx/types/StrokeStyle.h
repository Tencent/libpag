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
 * Line cap styles that define the shape at the endpoints of open paths.
 */
enum class LineCap {
  /**
   * A flat cap that ends exactly at the path endpoint.
   */
  Butt,
  /**
   * A rounded cap that extends beyond the endpoint by half the stroke width.
   */
  Round,
  /**
   * A square cap that extends beyond the endpoint by half the stroke width.
   */
  Square
};

/**
 * Line join styles that define the shape at the corners of paths.
 */
enum class LineJoin {
  /**
   * A sharp join that extends to a point.
   */
  Miter,
  /**
   * A rounded join with a circular arc.
   */
  Round,
  /**
   * A beveled join that cuts off the corner.
   */
  Bevel
};

/**
 * Stroke alignment relative to the path.
 */
enum class StrokeAlign {
  /**
   * Stroke is centered on the path.
   */
  Center,
  /**
   * Stroke is drawn inside the path boundary.
   */
  Inside,
  /**
   * Stroke is drawn outside the path boundary.
   */
  Outside
};

}  // namespace pagx
