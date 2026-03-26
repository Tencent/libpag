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
 * Specifies how a Text element's position.y is interpreted relative to the text content.
 */
enum class TextBaseline {
  /**
   * Default mode. position.y represents the top of the visual pixel bounds.
   */
  VisualTop,
  /**
   * position.y represents the alphabetic baseline. Used by SVG text elements.
   */
  Alphabetic
};

}  // namespace pagx
