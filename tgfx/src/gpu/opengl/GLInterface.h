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

#include "GLCaps.h"
#include "GLDefines.h"
#include "GLFunctions.h"
#include "GLProcGetter.h"

namespace tgfx {
class GLState;

/**
 * NOTE:
 * If added API can change OpenGL state, you should add code to recovery it.
 * 1. GLInterface.cpp - add HOOK function at HookWithState
 * 2. GLState.h/cpp - add hook function and GLAttribute
 */
class GLInterface {
 public:
  static const GLInterface* GetNative();

  GLFunction<GLActiveTexture> activeTexture;
  GLFunction<GLAttachShader> attachShader;
  GLFunction<GLBindBuffer> bindBuffer;
  GLFunction<GLBindFramebuffer> bindFramebuffer;
  GLFunction<GLBindRenderbuffer> bindRenderbuffer;
  GLFunction<GLBindTexture> bindTexture;
  GLFunction<GLBindVertexArray> bindVertexArray;
  GLFunction<GLBlendEquation> blendEquation;
  GLFunction<GLBlendEquationSeparate> blendEquationSeparate;
  GLFunction<GLBlendFunc> blendFunc;
  GLFunction<GLBlendFuncSeparate> blendFuncSeparate;
  GLFunction<GLBufferData> bufferData;
  GLFunction<GLCheckFramebufferStatus> checkFramebufferStatus;
  GLFunction<GLClear> clear;
  GLFunction<GLClearColor> clearColor;
  GLFunction<GLCompileShader> compileShader;
  GLFunction<GLCopyTexSubImage2D> copyTexSubImage2D;
  GLFunction<GLCreateProgram> createProgram;
  GLFunction<GLCreateShader> createShader;
  GLFunction<GLDeleteBuffers> deleteBuffers;
  GLFunction<GLDeleteFramebuffers> deleteFramebuffers;
  GLFunction<GLDeleteProgram> deleteProgram;
  GLFunction<GLDeleteRenderbuffers> deleteRenderbuffers;
  GLFunction<GLDeleteShader> deleteShader;
  GLFunction<GLDeleteTextures> deleteTextures;
  GLFunction<GLDeleteVertexArrays> deleteVertexArrays;
  GLFunction<GLDepthMask> depthMask;
  GLFunction<GLDisable> disable;
  GLFunction<GLDisableVertexAttribArray> disableVertexAttribArray;
  GLFunction<GLDrawArrays> drawArrays;
  GLFunction<GLDrawElements> drawElements;
  GLFunction<GLEnable> enable;
  GLFunction<GLIsEnabled> isEnabled;
  GLFunction<GLEnableVertexAttribArray> enableVertexAttribArray;
  GLFunction<GLFinish> finish;
  GLFunction<GLFlush> flush;
  GLFunction<GLFramebufferRenderbuffer> framebufferRenderbuffer;
  GLFunction<GLFramebufferTexture2D> framebufferTexture2D;
  GLFunction<GLFramebufferTexture2DMultisample> framebufferTexture2DMultisample;
  GLFunction<GLGenBuffers> genBuffers;
  GLFunction<GLGenFramebuffers> genFramebuffers;
  GLFunction<GLGenRenderbuffers> genRenderbuffers;
  GLFunction<GLGenTextures> genTextures;
  GLFunction<GLGenVertexArrays> genVertexArrays;
  GLFunction<GLGetError> getError;
  GLFunction<GLGetIntegerv> getIntegerv;
  GLFunction<GLGetBooleanv> getBooleanv;
  GLFunction<GLGetProgramInfoLog> getProgramInfoLog;
  GLFunction<GLGetProgramiv> getProgramiv;
  GLFunction<GLGetRenderbufferParameteriv> getRenderbufferParameteriv;
  GLFunction<GLGetShaderInfoLog> getShaderInfoLog;
  GLFunction<GLGetShaderiv> getShaderiv;
  GLFunction<GLGetString> getString;
  GLFunction<GLGetVertexAttribiv> getVertexAttribiv;
  GLFunction<GLGetVertexAttribPointerv> getVertexAttribPointerv;
  GLFunction<GLGetAttribLocation> getAttribLocation;
  GLFunction<GLGetUniformLocation> getUniformLocation;
  GLFunction<GLLinkProgram> linkProgram;
  GLFunction<GLPixelStorei> pixelStorei;
  GLFunction<GLReadPixels> readPixels;
  GLFunction<GLRenderbufferStorage> renderbufferStorage;
  GLFunction<GLRenderbufferStorageMultisample> renderbufferStorageMultisample;
  GLFunction<GLRenderbufferStorageMultisampleAPPLE> renderbufferStorageMultisampleAPPLE;
  GLFunction<GLRenderbufferStorageMultisampleEXT> renderbufferStorageMultisampleEXT;
  GLFunction<GLResolveMultisampleFramebuffer> resolveMultisampleFramebuffer;
  GLFunction<GLBlitFramebuffer> blitFramebuffer;
  GLFunction<GLScissor> scissor;
  GLFunction<GLShaderSource> shaderSource;
  GLFunction<GLTexImage2D> texImage2D;
  GLFunction<GLTexParameteri> texParameteri;
  GLFunction<GLTexParameteriv> texParameteriv;
  GLFunction<GLTexSubImage2D> texSubImage2D;
  GLFunction<GLUniform1f> uniform1f;
  GLFunction<GLUniform1i> uniform1i;
  GLFunction<GLUniform2f> uniform2f;
  GLFunction<GLUniform3f> uniform3f;
  GLFunction<GLUniform4fv> uniform4fv;
  GLFunction<GLUniformMatrix3fv> uniformMatrix3fv;
  GLFunction<GLUseProgram> useProgram;
  GLFunction<GLVertexAttribPointer> vertexAttribPointer;
  GLFunction<GLViewport> viewport;
  GLFunction<GLTextureBarrier> textureBarrier;
  GLFunction<GLFenceSync> fenceSync;
  GLFunction<GLWaitSync> waitSync;
  GLFunction<GLDeleteSync> deleteSync;

  std::shared_ptr<const GLCaps> caps = nullptr;

 private:
  static std::unique_ptr<const GLInterface> MakeNativeInterface(const GLProcGetter* getter);
  static std::unique_ptr<const GLInterface> HookWithState(const GLInterface* gl, GLState* state);

  friend class GLContext;
};
}  // namespace tgfx
