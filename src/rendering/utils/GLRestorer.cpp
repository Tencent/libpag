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

#if defined(__APPLE__)
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE
#include <OpenGLES/ES3/gl.h>
#else
#include <OpenGL/gl3.h>
#endif
#elif defined(__ANDROID__) || defined(ANDROID)
#include <GLES3/gl3.h>
#elif defined(_WIN32)
#include <windows.h>
#include <GL/gl.h>
#include <GL/glext.h>
#elif defined(__OHOS__)
#include <GLES3/gl3.h>
#else
#include <GL/gl.h>
#endif

// Some platforms may not define these constants
#ifndef GL_VERTEX_ARRAY_BINDING
#define GL_VERTEX_ARRAY_BINDING 0x85B5
#endif

#ifndef GL_BLEND_EQUATION
#define GL_BLEND_EQUATION 0x8009
#endif

#ifndef GL_BLEND_EQUATION_RGB
#define GL_BLEND_EQUATION_RGB 0x8009
#endif

#ifndef GL_BLEND_EQUATION_ALPHA
#define GL_BLEND_EQUATION_ALPHA 0x883D
#endif

#ifndef GL_BLEND_SRC_RGB
#define GL_BLEND_SRC_RGB 0x80C9
#endif

#ifndef GL_BLEND_DST_RGB
#define GL_BLEND_DST_RGB 0x80C8
#endif

#ifndef GL_BLEND_SRC_ALPHA
#define GL_BLEND_SRC_ALPHA 0x80CB
#endif

#ifndef GL_BLEND_DST_ALPHA
#define GL_BLEND_DST_ALPHA 0x80CA
#endif

namespace pag {

static void ClearGLError() {
  while (glGetError() != GL_NO_ERROR) {
    // Silently clear any existing GL errors
  }
}

// Platform-specific VAO function declarations
#if defined(__APPLE__)
#if TARGET_OS_IPHONE
#define HAS_VAO 1
#else
#define HAS_VAO 1
#endif
#elif defined(__ANDROID__) || defined(ANDROID) || defined(__OHOS__)
#define HAS_VAO 1
#elif defined(_WIN32)
// Windows needs function pointer loading for VAO
typedef void(APIENTRY* PFNGLBINDVERTEXARRAYPROC)(GLuint array);
static PFNGLBINDVERTEXARRAYPROC glBindVertexArrayFunc = nullptr;
static bool vaoFunctionLoaded = false;

static void LoadVAOFunction() {
  if (!vaoFunctionLoaded) {
    glBindVertexArrayFunc =
        (PFNGLBINDVERTEXARRAYPROC)wglGetProcAddress("glBindVertexArray");
    vaoFunctionLoaded = true;
  }
}
#define HAS_VAO 0
#else
#define HAS_VAO 1
#endif

void GLRestorer::save() {
  // Clear any existing GL errors to prevent external errors from affecting our logic
  ClearGLError();

  glGetIntegerv(GL_VIEWPORT, viewport);
  scissorEnabled = glIsEnabled(GL_SCISSOR_TEST);
  if (scissorEnabled) {
    glGetIntegerv(GL_SCISSOR_BOX, scissorBox);
  }
  glGetIntegerv(GL_CURRENT_PROGRAM, &program);
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &frameBuffer);
  glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &textureID);
  glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &arrayBuffer);
  glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &elementArrayBuffer);

  // VAO support check and handling
#if HAS_VAO
  hasVertexArray = true;
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vertexArray);
#elif defined(_WIN32)
  LoadVAOFunction();
  if (glBindVertexArrayFunc != nullptr) {
    hasVertexArray = true;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vertexArray);
  }
#endif

  // Unbind buffers before PAG rendering
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

  blendEnabled = glIsEnabled(GL_BLEND);
  if (blendEnabled) {
    glGetIntegerv(GL_BLEND_EQUATION, &blendEquation);
    glGetIntegerv(GL_BLEND_EQUATION_RGB, &equationRGB);
    glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &equationAlpha);
    glGetIntegerv(GL_BLEND_SRC_RGB, &blendSrcRGB);
    glGetIntegerv(GL_BLEND_DST_RGB, &blendDstRGB);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSrcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDstAlpha);
  }

  // Unbind VAO before PAG rendering
  if (hasVertexArray && vertexArray > 0) {
#if HAS_VAO
    glBindVertexArray(0);
#elif defined(_WIN32)
    if (glBindVertexArrayFunc != nullptr) {
      glBindVertexArrayFunc(0);
    }
#endif
  }
}

void GLRestorer::restore() {
  // Restore viewport
  glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);

  // Restore scissor state
  if (scissorEnabled) {
    glEnable(GL_SCISSOR_TEST);
    glScissor(scissorBox[0], scissorBox[1], scissorBox[2], scissorBox[3]);
  } else {
    glDisable(GL_SCISSOR_TEST);
  }

  // Restore program
  glUseProgram(static_cast<unsigned>(program));

  // Restore framebuffer
  glBindFramebuffer(GL_FRAMEBUFFER, static_cast<unsigned>(frameBuffer));

  // Restore texture
  glActiveTexture(static_cast<unsigned>(activeTexture));
  glBindTexture(GL_TEXTURE_2D, static_cast<unsigned>(textureID));

  // Restore VAO
  if (hasVertexArray && vertexArray > 0) {
#if HAS_VAO
    glBindVertexArray(static_cast<unsigned>(vertexArray));
#elif defined(_WIN32)
    if (glBindVertexArrayFunc != nullptr) {
      glBindVertexArrayFunc(static_cast<unsigned>(vertexArray));
    }
#endif
  }

  // Restore buffers
  glBindBuffer(GL_ARRAY_BUFFER, static_cast<unsigned>(arrayBuffer));
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, static_cast<unsigned>(elementArrayBuffer));

  // Restore blend state
  if (blendEnabled) {
    glEnable(GL_BLEND);
    glBlendEquation(static_cast<unsigned>(blendEquation));
    glBlendEquationSeparate(static_cast<unsigned>(equationRGB), static_cast<unsigned>(equationAlpha));
    glBlendFuncSeparate(static_cast<unsigned>(blendSrcRGB), static_cast<unsigned>(blendDstRGB),
                        static_cast<unsigned>(blendSrcAlpha), static_cast<unsigned>(blendDstAlpha));
  } else {
    glDisable(GL_BLEND);
  }

  // Clear any GL errors produced by PAG to prevent affecting external rendering
  ClearGLError();
}

}  // namespace pag
