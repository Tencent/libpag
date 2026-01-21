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

namespace pagx {

/**
 * Tile modes for patterns, gradients, and filters.
 */
enum class TileMode {
  /**
   * Clamp to the edge color, extending the edge pixels outward.
   */
  Clamp,
  /**
   * Repeat the pattern by tiling it.
   */
  Repeat,
  /**
   * Repeat the pattern by mirroring it at the edges.
   */
  Mirror,
  /**
   * Render transparent pixels outside the bounds.
   */
  Decal
};

}  // namespace pagx
