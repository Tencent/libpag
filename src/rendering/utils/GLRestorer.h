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
//  and limitations under the license.˙
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(PAG_BUILD_FOR_WEB) && !defined(_WIN32)

namespace pag {

/**
 * Saves and restores OpenGL state when rendering to a shared texture (external context).
 * This prevents PAG's rendering from interfering with the external OpenGL rendering.
 */
class GLRestorer {
 public:
  /**
   * Saves the current OpenGL state.
   */
  void save();

  /**
   * Restores the previously saved OpenGL state.
   */
  void restore();

 private:
  int viewport[4] = {};
  int scissorBox[4] = {};
  unsigned char scissorEnabled = 0;
  unsigned char blendEnabled = 0;
  int program = 0;
  int frameBuffer = 0;
  int activeTexture = 0;
  int textureID = 0;
  int arrayBuffer = 0;
  int elementArrayBuffer = 0;
  int vertexArray = 0;
  int blendEquationRGB = 0;
  int blendEquationAlpha = 0;
  int blendSrcRGB = 0;
  int blendDstRGB = 0;
  int blendSrcAlpha = 0;
  int blendDstAlpha = 0;
};

}  // namespace pag

#endif  // PAG_BUILD_FOR_WEB
