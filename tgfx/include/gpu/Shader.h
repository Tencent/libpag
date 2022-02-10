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

#include <memory>
#include "pag/types.h"

namespace pag {
/**
 * Shaders specify the source color(s) for what is being drawn. If a paint has no shader, then the
 * paint's color is used. If the paint has a shader, then the shader's color(s) are use instead, but
 * they are modulated by the paint's alpha.
 */
class Shader {
 public:
  /**
   * Create a shader that draws the specified color.
   */
  static std::shared_ptr<Shader> MakeColorShader(Color color, Opacity opacity = Opaque);

  virtual ~Shader() = default;

  /**
   * Returns true if the shader is guaranteed to produce only opaque colors, subject to the Paint
   * using the shader to apply an opaque alpha value. Subclasses should override this to allow some
   * optimizations.
   */
  virtual bool isOpaque() const {
    return false;
  }
};
}  // namespace pag
