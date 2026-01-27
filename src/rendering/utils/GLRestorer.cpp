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
#include <OpenGL/gl3.h>
#endif
#elif defined(_WIN32)
#include <windows.h>
#include <GL/gl.h>
#elif defined(__ANDROID__) || defined(ANDROID) || defined(__OHOS__)
#include <GLES3/gl3.h>
#else
#include <GLES3/gl3.h>
#endif

#ifdef _WIN32
// Windows GL/gl.h only provides OpenGL 1.1, need to define missing constants
#define GL_VERTEX_ARRAY_BINDING 0x85B5
#define GL_BLEND_EQUATION_RGB 0x8009
#define GL_BLEND_EQUATION_ALPHA 0x883D
#define GL_BLEND_SRC_RGB 0x80C9
#define GL_BLEND_DST_RGB 0x80C8
#define GL_BLEND_SRC_ALPHA 0x80CB
#define GL_BLEND_DST_ALPHA 0x80CA
#define GL_FRAMEBUFFER 0x8D40
#define GL_FRAMEBUFFER_BINDING 0x8CA6
#define GL_CURRENT_PROGRAM 0x8B8D
#define GL_ACTIVE_TEXTURE 0x84E0
#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_ARRAY_BUFFER_BINDING 0x8894
#define GL_ELEMENT_ARRAY_BUFFER_BINDING 0x8895

typedef void(APIENTRY* BINDVERTEXARRAY)(GLuint array);
typedef void(APIENTRY* BINDFRAMEBUFFER)(GLenum target, GLuint framebuffer);
typedef void(APIENTRY* BINDBUFFER)(GLenum target, GLuint buffer);
typedef void(APIENTRY* USEPROGRAM)(GLuint program);
typedef void(APIENTRY* ACTIVETEXTURE)(GLenum texture);
typedef void(APIENTRY* BLENDEQUATIONSEPARATE)(GLenum modeRGB, GLenum modeAlpha);
typedef void(APIENTRY* BLENDFUNCSEPARATE)(GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha,
                                          GLenum dstAlpha);

static BINDVERTEXARRAY bindVertexArray = nullptr;
static BINDFRAMEBUFFER bindFramebuffer = nullptr;
static BINDBUFFER bindBuffer = nullptr;
static USEPROGRAM useProgram = nullptr;
static ACTIVETEXTURE activeTextureFunc = nullptr;
static BLENDEQUATIONSEPARATE blendEquationSeparate = nullptr;
static BLENDFUNCSEPARATE blendFuncSeparate = nullptr;

static void LoadGLFunctions() {
  static bool loaded = false;
  if (loaded) {
    return;
  }
  loaded = true;
  bindVertexArray = (BINDVERTEXARRAY)wglGetProcAddress("glBindVertexArray");
  bindFramebuffer = (BINDFRAMEBUFFER)wglGetProcAddress("glBindFramebuffer");
  bindBuffer = (BINDBUFFER)wglGetProcAddress("glBindBuffer");
  useProgram = (USEPROGRAM)wglGetProcAddress("glUseProgram");
  activeTextureFunc = (ACTIVETEXTURE)wglGetProcAddress("glActiveTexture");
  blendEquationSeparate = (BLENDEQUATIONSEPARATE)wglGetProcAddress("glBlendEquationSeparate");
  blendFuncSeparate = (BLENDFUNCSEPARATE)wglGetProcAddress("glBlendFuncSeparate");
}
#endif

namespace pag {

static void ClearGLError() {
  while (glGetError() != GL_NO_ERROR) {
  }
}

void GLRestorer::save() {
  ClearGLError();

#ifdef _WIN32
  LoadGLFunctions();
#endif

  glGetIntegerv(GL_VIEWPORT, viewport);
  glGetIntegerv(GL_SCISSOR_BOX, scissorBox);
  scissorEnabled = glIsEnabled(GL_SCISSOR_TEST);
  glGetIntegerv(GL_CURRENT_PROGRAM, &program);
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &frameBuffer);
  glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &textureID);
  glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &arrayBuffer);
  glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &elementArrayBuffer);

#ifdef _WIN32
  if (bindVertexArray != nullptr) {
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vertexArray);
    bindVertexArray(0);
  }
  if (bindBuffer != nullptr) {
    bindBuffer(GL_ARRAY_BUFFER, 0);
    bindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  }
#else
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vertexArray);
  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
#endif

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

#ifdef _WIN32
  if (useProgram != nullptr) {
    useProgram(program);
  }
  if (bindFramebuffer != nullptr) {
    bindFramebuffer(GL_FRAMEBUFFER, frameBuffer);
  }
  if (activeTextureFunc != nullptr) {
    activeTextureFunc(activeTexture);
  }
  glBindTexture(GL_TEXTURE_2D, textureID);
  if (bindVertexArray != nullptr && vertexArray > 0) {
    bindVertexArray(vertexArray);
  }
  if (bindBuffer != nullptr) {
    bindBuffer(GL_ARRAY_BUFFER, arrayBuffer);
    bindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementArrayBuffer);
  }
  if (blendEnabled) {
    glEnable(GL_BLEND);
    if (blendEquationSeparate != nullptr) {
      blendEquationSeparate(blendEquationRGB, blendEquationAlpha);
    }
    if (blendFuncSeparate != nullptr) {
      blendFuncSeparate(blendSrcRGB, blendDstRGB, blendSrcAlpha, blendDstAlpha);
    }
  } else {
    glDisable(GL_BLEND);
  }
#else
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
#endif

  ClearGLError();
}

}  // namespace pag

#endif  // PAG_BUILD_FOR_WEB
