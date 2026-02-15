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
 * Text vertical alignment within the layout area.
 */
enum class VerticalAlign {
  /**
   * Align text to the top of the layout area.
   */
  Top,
  /**
   * Align text to the vertical center of the layout area.
   */
  Center,
  /**
   * Align text to the bottom of the layout area.
   */
  Bottom
};

}  // namespace pagx
