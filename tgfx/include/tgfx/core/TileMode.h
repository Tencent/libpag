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

namespace tgfx {
enum class TileMode {
  /**
   * Replicate the edge color if the shader draws outside of its original bounds.
   */
  Clamp,

  /**
   * Repeat the shader's image horizontally and vertically.
   */
  Repeat,

  /**
   * Repeat the shader's image horizontally and vertically, alternating mirror images so that
   * adjacent images always seam.
   */
  Mirror,

  /**
   * Only draw within the original domain, return transparent-black everywhere else.
   */
  Decal
};
}  // namespace tgfx
