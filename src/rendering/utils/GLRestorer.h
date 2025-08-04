/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.˙
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "tgfx/gpu/opengl/GLFunctions.h"

namespace pag {
class GLRestorer {
 public:
  explicit GLRestorer(const tgfx::GLFunctions* gl);

  ~GLRestorer();

 private:
  const tgfx::GLFunctions* gl = nullptr;
  int viewport[4] = {};
  unsigned scissorEnabled = GL_FALSE;
  int scissorBox[4] = {};
  int frameBuffer = 0;
  int program = 0;
  int activeTexture = 0;
  int textureID = 0;
  int arrayBuffer = 0;
  int elementArrayBuffer = 0;
  int vertexArray = 0;

  unsigned blendEnabled = GL_FALSE;
  int blendEquation = 0;
  int equationRGB = 0;
  int equationAlpha = 0;
  int blendSrcRGB = 0;
  int blendDstRGB = 0;
  int blendSrcAlpha = 0;
  int blendDstAlpha = 0;
};
}  // namespace pag
