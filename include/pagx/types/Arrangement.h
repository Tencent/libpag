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
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

namespace pagx {

/**
 * Main-axis arrangement for child elements in a layout container.
 */
enum class Arrangement {
  /**
   * Pack children to the start of the main axis.
   */
  Start,
  /**
   * Pack children to the center of the main axis.
   */
  Center,
  /**
   * Pack children to the end of the main axis.
   */
  End,
  /**
   * Distribute children evenly with equal space between them.
   */
  SpaceBetween,
  /**
   * Distribute children with equal space around them, including the edges.
   */
  SpaceEvenly,
  /**
   * Distribute children with equal space on each side, half-size space at the edges.
   */
  SpaceAround
};

}  // namespace pagx
