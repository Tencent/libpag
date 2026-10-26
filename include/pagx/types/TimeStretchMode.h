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
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions and
//  limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>

namespace pagx {

/**
 * Defines how the default timeline stretches to fit a target duration (spec 3.4, document-level
 * runtime interface — not serialized into the binary).
 */
enum class TimeStretchMode : uint8_t {
  /**
   * Keeps the original speed and holds the last frame when the content is shorter than the
   * target duration.
   */
  None = 0,
  /**
   * Changes the playback speed to fit the target duration.
   */
  Scale = 1,
  /**
   * Keeps the original speed and repeats the content when it is shorter than the target
   * duration. This is the default mode.
   */
  Repeat = 2,
  /**
   * Keeps the original speed and repeats the content in reverse when it is shorter than the
   * target duration.
   */
  RepeatInverted = 3,
};

}  // namespace pagx
