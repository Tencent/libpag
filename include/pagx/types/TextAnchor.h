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
 * Text anchor alignment. Controls how text is positioned relative to its origin point.
 */
enum class TextAnchor {
  /**
   * The text position represents the start of the text. No offset is applied.
   */
  Start,
  /**
   * The text position represents the center of the text. The text is offset by half its width to
   * center it on the position.
   */
  Center,
  /**
   * The text position represents the end of the text. The text is offset by its full width so that
   * the text ends at the position.
   */
  End
};

}  // namespace pagx
