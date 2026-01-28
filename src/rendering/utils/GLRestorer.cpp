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

#include "GLRestorer.h"

#ifndef PAG_BUILD_FOR_WEB

#if defined(__APPLE__)
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE
#include <OpenGLES/ES3/gl.h>
#else
#ifndef GL_SILENCE_DEPRECATION
#define GL_SILENCE_DEPRECATION
#endif
#include <OpenGL/gl3.h>
#endif
#elif defined(__ANDROID__) || defined(ANDROID) || defined(__OHOS__) || defined(_WIN32)
#include <GLES3/gl3.h>
#else
#include <GLES3/gl3.h>
#endif

namespace pag {

static void ClearGLError() {
  while (glGetError() != GL_NO_ERROR) {
  }
}

void GLRestorer::save() {
  ClearGLError();

  glGetIntegerv(GL_VIEWPORT, viewport);
  glGetIntegerv(GL_SCISSOR_BOX, scissorBox);
  scissorEnabled = glIsEnabled(GL_SCISSOR_TEST);
  glGetIntegerv(GL_CURRENT_PROGRAM, &program);
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &frameBuffer);
  glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &textureID);
  glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &arrayBuffer);
  glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &elementArrayBuffer);

  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vertexArray);
  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

  blendEnabled = glIsEnabled(GL_BLEND);
  if (blendEnabled) {
    glGetIntegerv(GL_BLEND_EQUATION_RGB, &blendEquationRGB);
    glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &blendEquationAlpha);
    glGetIntegerv(GL_BLEND_SRC_RGB, &blendSrcRGB);
    glGetIntegerv(GL_BLEND_DST_RGB, &blendDstRGB);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSrcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDstAlpha);
  }
}

void GLRestorer::restore() {
  glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);

  if (scissorEnabled) {
    glEnable(GL_SCISSOR_TEST);
    glScissor(scissorBox[0], scissorBox[1], scissorBox[2], scissorBox[3]);
  } else {
    glDisable(GL_SCISSOR_TEST);
  }

  glUseProgram(program);
  glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);
  glActiveTexture(activeTexture);
  glBindTexture(GL_TEXTURE_2D, textureID);
  if (vertexArray > 0) {
    glBindVertexArray(vertexArray);
  }
  glBindBuffer(GL_ARRAY_BUFFER, arrayBuffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementArrayBuffer);

  if (blendEnabled) {
    glEnable(GL_BLEND);
    glBlendEquationSeparate(blendEquationRGB, blendEquationAlpha);
    glBlendFuncSeparate(blendSrcRGB, blendDstRGB, blendSrcAlpha, blendDstAlpha);
  } else {
    glDisable(GL_BLEND);
  }

  ClearGLError();
}

}  // namespace pag

#endif  // PAG_BUILD_FOR_WEB
