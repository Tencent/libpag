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
 * The mode of auto layout for arranging child layers.
 */
enum class LayoutMode {
  /**
   * No auto layout. Child layers use absolute positioning.
   */
  Absolute,
  /**
   * Arrange child layers horizontally from left to right.
   */
  Horizontal,
  /**
   * Arrange child layers vertically from top to bottom.
   */
  Vertical
};

}  // namespace pagx
