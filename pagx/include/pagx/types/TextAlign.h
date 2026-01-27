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
 * Text horizontal alignment.
 */
enum class TextAlign {
  /**
   * Align text to the start (left for LTR, right for RTL).
   * For TextPath: start from the beginning of the path.
   */
  Start,
  /**
   * Align text to the center.
   * For TextPath: center text on the path.
   */
  Center,
  /**
   * Align text to the end (right for LTR, left for RTL).
   * For TextPath: end text at the end of the path.
   */
  End,
  /**
   * Justify text (stretch to fill the available width).
   * The last line uses start alignment by default.
   * For TextPath: stretch text to fill the available path length.
   */
  Justify
};

}  // namespace pagx
